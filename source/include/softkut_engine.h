// softkut_engine.h : host-agnostic softcut engine for softkut~.
//
// This header owns everything that does NOT depend on Max: the softcut voice
// array, a lock-free single-producer/single-consumer command queue, the
// double<->float block conversion, the power-of-two buffer framing softcut
// requires, and the phase-poll bookkeeping. The Max external (softkut~.cpp) is a
// thin shell over this; the offline test harness drives this directly.
//
// Threading model (mirrors softcut's own Commands design):
//   - ONE control/producer thread enqueues commands via the set*/push helpers.
//   - ONE audio/consumer thread drains them at the top of process().
//   - Phase getters read std::atomic state inside softcut and are safe from any
//     thread without going through the queue.
// A Max object's messages are assumed to arrive serialized on the control side
// (the standard assumption; defer to the main thread if that is ever violated).

#ifndef SOFTKUT_ENGINE_H
#define SOFTKUT_ENGINE_H

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "softcut/Softcut.h"
#include "softcut/Types.h"
#include "softcut/Utilities.h"   // softcut::LogRamp

namespace softkut {

// ---------------------------------------------------------------------------
// command set: every per-voice control parameter routed through the queue.
// All commands carry <idx0, idx1, value>; most use only idx0 (voice) + value.
// ---------------------------------------------------------------------------
enum class CmdId : uint16_t {
    Rate, LoopStart, LoopEnd, LoopFlag, FadeTime,
    RecLevel, PreLevel, RecFlag, PlayFlag, RecOnceFlag, Position, RecOffset,
    PreFilterFc, PreFilterFcMod, PreFilterRq, PreFilterLp, PreFilterHp,
    PreFilterBp, PreFilterBr, PreFilterDry,
    PostFilterFc, PostFilterRq, PostFilterLp, PostFilterHp,
    PostFilterBp, PostFilterBr, PostFilterDry,
    Level, Pan, LevelSlewTime, PanSlewTime,
    RecPreSlewTime, RateSlewTime,
    PhaseQuant, PhaseOffset, VoiceSync,
    Stop, FbLevel, Enable, InLevel,
    Reset
};

struct Command {
    CmdId   id;
    int16_t idx0;
    int16_t idx1;
    float   value;
};

// ---------------------------------------------------------------------------
// Lock-free single-producer / single-consumer ring buffer.
// Capacity must be a power of two. push() is called only from the producer
// thread, pop() only from the consumer thread.
// ---------------------------------------------------------------------------
template <typename T, size_t Capacity>
class SpscQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
public:
    SpscQueue() : head_(0), tail_(0) {}

    bool push(const T &v) {
        const size_t h = head_.load(std::memory_order_relaxed);
        const size_t n = (h + 1) & (Capacity - 1);
        if (n == tail_.load(std::memory_order_acquire))
            return false;                      // full
        buf_[h] = v;
        head_.store(n, std::memory_order_release);
        return true;
    }

    bool pop(T &out) {
        const size_t t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire))
            return false;                      // empty
        out = buf_[t];
        tail_.store((t + 1) & (Capacity - 1), std::memory_order_release);
        return true;
    }

    // approximate count (for tests/diagnostics; not for control flow)
    size_t size() const {
        const size_t h = head_.load(std::memory_order_acquire);
        const size_t t = tail_.load(std::memory_order_acquire);
        return (h - t) & (Capacity - 1);
    }

private:
    T                   buf_[Capacity];
    std::atomic<size_t> head_;   // next write slot (producer)
    std::atomic<size_t> tail_;   // next read slot  (consumer)
};

// ---------------------------------------------------------------------------
// Engine
// ---------------------------------------------------------------------------
template <int NumVoices>
class Engine {
public:
    static const int   kMaxBlock = 8192;     // upper bound on host vector size
    static const size_t kQueueCap = 1024;
    static constexpr float kHalfPi = 1.57079632679489662f;

    Engine() {
        for (int v = 0; v < NumVoices; ++v)
            lastQuant_[v] = -1.0;             // force first report
        setDefaults();
        zeroOutStore();
    }

    int numVoices() const { return NumVoices; }

    // softcut requires a power-of-two buffer length (it wraps with a bitmask).
    // Largest power of two <= n (0 if n < 1).
    static size_t usableFrames(size_t n) {
        if (n < 1) return 0;
        size_t p = 1;
        while ((p << 1) != 0 && (p << 1) <= n) p <<= 1;
        return p;
    }

    void setSampleRate(double sr) {
        sampleRate_ = sr;
        cut_.setSampleRate(static_cast<unsigned int>(sr));
        for (int v = 0; v < NumVoices; ++v) {
            outLevel_[v].setSampleRate(static_cast<float>(sr));
            outPan_[v].setSampleRate(static_cast<float>(sr));
            for (int w = 0; w < NumVoices; ++w) {
                fbLevel_[v][w].setSampleRate(static_cast<float>(sr));
                inLevel_[v][w].setSampleRate(static_cast<float>(sr));
            }
        }
        zeroOutStore();   // fresh DSP start: clear the feedback history
    }
    double getSampleRate() const { return sampleRate_; }

    // ---- producer side: enqueue commands -------------------------------
    bool push(const Command &c) { return queue_.push(c); }
    size_t pending() const { return queue_.size(); }

    bool setRate(int v, float x)         { return cmd(CmdId::Rate, v, x); }
    bool setLoopStart(int v, float s)    { return cmd(CmdId::LoopStart, v, s); }
    bool setLoopEnd(int v, float s)      { return cmd(CmdId::LoopEnd, v, s); }
    bool setLoopFlag(int v, bool b)      { return cmd(CmdId::LoopFlag, v, b ? 1.f : 0.f); }
    bool setFadeTime(int v, float s)     { return cmd(CmdId::FadeTime, v, s); }
    bool setRecLevel(int v, float a)     { return cmd(CmdId::RecLevel, v, a); }
    bool setPreLevel(int v, float a)     { return cmd(CmdId::PreLevel, v, a); }
    bool setRecFlag(int v, bool b)       { return cmd(CmdId::RecFlag, v, b ? 1.f : 0.f); }
    bool setPlayFlag(int v, bool b)      { return cmd(CmdId::PlayFlag, v, b ? 1.f : 0.f); }
    bool setRecOnceFlag(int v, bool b)   { return cmd(CmdId::RecOnceFlag, v, b ? 1.f : 0.f); }
    bool cutToPos(int v, float s)        { return cmd(CmdId::Position, v, s); }
    bool setRecOffset(int v, float d)    { return cmd(CmdId::RecOffset, v, d); }

    bool setPreFilterFc(int v, float x)    { return cmd(CmdId::PreFilterFc, v, x); }
    bool setPreFilterFcMod(int v, float x) { return cmd(CmdId::PreFilterFcMod, v, x); }
    bool setPreFilterRq(int v, float x)    { return cmd(CmdId::PreFilterRq, v, x); }
    bool setPreFilterLp(int v, float x)    { return cmd(CmdId::PreFilterLp, v, x); }
    bool setPreFilterHp(int v, float x)    { return cmd(CmdId::PreFilterHp, v, x); }
    bool setPreFilterBp(int v, float x)    { return cmd(CmdId::PreFilterBp, v, x); }
    bool setPreFilterBr(int v, float x)    { return cmd(CmdId::PreFilterBr, v, x); }
    bool setPreFilterDry(int v, float x)   { return cmd(CmdId::PreFilterDry, v, x); }

    bool setPostFilterFc(int v, float x)   { return cmd(CmdId::PostFilterFc, v, x); }
    bool setPostFilterRq(int v, float x)   { return cmd(CmdId::PostFilterRq, v, x); }
    bool setPostFilterLp(int v, float x)   { return cmd(CmdId::PostFilterLp, v, x); }
    bool setPostFilterHp(int v, float x)   { return cmd(CmdId::PostFilterHp, v, x); }
    bool setPostFilterBp(int v, float x)   { return cmd(CmdId::PostFilterBp, v, x); }
    bool setPostFilterBr(int v, float x)   { return cmd(CmdId::PostFilterBr, v, x); }
    bool setPostFilterDry(int v, float x)  { return cmd(CmdId::PostFilterDry, v, x); }

    bool setRecPreSlewTime(int v, float s) { return cmd(CmdId::RecPreSlewTime, v, s); }
    bool setRateSlewTime(int v, float s)   { return cmd(CmdId::RateSlewTime, v, s); }

    // output mix: per-voice level (voice gain) and pan (-1 left .. +1 right)
    bool setLevel(int v, float a)          { return cmd(CmdId::Level, v, a); }
    bool setPan(int v, float p)            { return cmd(CmdId::Pan, v, p); }
    bool setLevelSlewTime(int v, float s)  { return cmd(CmdId::LevelSlewTime, v, s); }
    bool setPanSlewTime(int v, float s)    { return cmd(CmdId::PanSlewTime, v, s); }

    bool setPhaseQuant(int v, float x)     { return cmd(CmdId::PhaseQuant, v, x); }
    bool setPhaseOffset(int v, float s)    { return cmd(CmdId::PhaseOffset, v, s); }

    bool syncVoice(int follow, int lead, float offset) {
        Command c{CmdId::VoiceSync, (int16_t)follow, (int16_t)lead, offset};
        return queue_.push(c);
    }
    bool stopVoice(int v) { return cmd(CmdId::Stop, v, 0.f); }
    bool setEnabled(int v, bool b) { return cmd(CmdId::Enable, v, b ? 1.f : 0.f); }
    // route voice `src`'s output into voice `dst`'s record input at gain `g`
    bool setFeedback(int src, int dst, float g) {
        Command c{CmdId::FbLevel, (int16_t)src, (int16_t)dst, g};
        return queue_.push(c);
    }
    // route input inlet `inl` into voice `dst`'s record input at gain `g`
    bool setInLevel(int inl, int dst, float g) {
        Command c{CmdId::InLevel, (int16_t)inl, (int16_t)dst, g};
        return queue_.push(c);
    }
    bool reset() { Command c{CmdId::Reset, 0, 0, 0.f}; return queue_.push(c); }

    // ---- consumer side: drain queue + process one block ----------------
    // ins/voiceOuts: NumVoices arrays of `nframes` host samples (double). Each
    //   voiceOuts[v] receives that voice's post-level (pre-pan) signal.
    // bufs/rawFrames: per-voice (mono) sample store + its raw frame count. A
    //   voice with a null buf outputs silence. rawFrames is reduced internally
    //   to a usable power-of-two prefix.
    // mixL/mixR: the equal-power panned sum of all voices (may be null to skip).
    void process(const double *const *ins, double *const *voiceOuts, int nframes,
                 float *const *bufs, const size_t *rawFrames,
                 double *mixL, double *mixR) {
        drain();

        if (nframes > kMaxBlock) nframes = kMaxBlock;   // guard scratch bounds

        for (int i = 0; i < nframes; ++i) { mixLf_[i] = 0.f; mixRf_[i] = 0.f; }

        // prevOut holds last block's raw voice outputs (feedback source, one
        // block delayed); curOut receives this block's raw outputs. We read
        // prevOut for ALL src while writing curOut, so the within-block voice
        // order doesn't affect the feedback (matches SoftcutClient::mixInput).
        float (*prevOut)[kMaxBlock] = outStore_[outCur_ ^ 1];
        float (*curOut)[kMaxBlock]  = outStore_[outCur_];

        for (int dst = 0; dst < NumVoices; ++dst) {
            // record input = input matrix (inlet -> voice) + voice->voice feedback
            for (int i = 0; i < nframes; ++i) recIn_[i] = 0.f;
            for (int inl = 0; inl < NumVoices; ++inl) {
                const double *in = ins[inl];
                for (int i = 0; i < nframes; ++i)
                    recIn_[i] += static_cast<float>(in[i]) * inLevel_[inl][dst].update();
            }
            for (int src = 0; src < NumVoices; ++src)
                for (int i = 0; i < nframes; ++i)
                    recIn_[i] += prevOut[src][i] * fbLevel_[src][dst].update();

            float       *buf    = bufs ? bufs[dst] : nullptr;
            const size_t frames = (buf && rawFrames) ? usableFrames(rawFrames[dst]) : 0;
            if (enabled_[dst] && buf && frames >= 1) {
                cut_.setVoiceBuffer(dst, buf, frames);
                cut_.processBlock(dst, recIn_, outScratch_, nframes);
            } else {
                for (int i = 0; i < nframes; ++i) outScratch_[i] = 0.f;
            }

            // retain raw output for next block's feedback
            for (int i = 0; i < nframes; ++i) curOut[dst][i] = outScratch_[i];

            // per-voice level (gain) + equal-power pan into the stereo mix
            double *out = voiceOuts[dst];
            for (int i = 0; i < nframes; ++i) {
                const float lv = outLevel_[dst].update();
                const float pv = outPan_[dst].update();          // [0,1]
                const float s  = outScratch_[i] * lv;
                out[i] = static_cast<double>(s);
                mixLf_[i] += s * std::cos(pv * kHalfPi);
                mixRf_[i] += s * std::sin(pv * kHalfPi);
            }
        }
        outCur_ ^= 1;   // this block's curOut becomes next block's prevOut

        if (mixL) for (int i = 0; i < nframes; ++i) mixL[i] = static_cast<double>(mixLf_[i]);
        if (mixR) for (int i = 0; i < nframes; ++i) mixR[i] = static_cast<double>(mixRf_[i]);
        updatePhases();
    }

    // ---- phase poll (safe from any thread) -----------------------------
    double getSavedPosition(int v) { return cut_.getSavedPosition(v); }
    double getQuantPhase(int v)    { return cut_.getQuantPhase(v); }
    bool   getPlayFlag(int v)      { return cut_.getPlayFlag(v); }
    bool   getRecFlag(int v)       { return cut_.getRecFlag(v); }
    bool   getEnabled(int v)       { return enabled_[v]; }

    // returns true (once) when a voice's quantized phase has changed since the
    // last call; used to throttle phase reporting to the host.
    bool checkQuantPhaseChanged(int v) {
        const softcut::phase_t q = cut_.getQuantPhase(v);
        if (q != lastQuant_[v]) { lastQuant_[v] = q; return true; }
        return false;
    }

private:
    bool cmd(CmdId id, int v, float val) {
        Command c{id, (int16_t)v, 0, val};
        return queue_.push(c);
    }

    void drain() {
        Command c;
        while (queue_.pop(c)) handle(c);
    }

    void handle(const Command &c) {
        const int v = c.idx0;
        switch (c.id) {
            case CmdId::Rate:           cut_.setRate(v, c.value); break;
            case CmdId::LoopStart:      cut_.setLoopStart(v, c.value); break;
            case CmdId::LoopEnd:        cut_.setLoopEnd(v, c.value); break;
            case CmdId::LoopFlag:       cut_.setLoopFlag(v, c.value > 0.f); break;
            case CmdId::FadeTime:       cut_.setFadeTime(v, c.value); break;
            case CmdId::RecLevel:       cut_.setRecLevel(v, c.value); break;
            case CmdId::PreLevel:       cut_.setPreLevel(v, c.value); break;
            case CmdId::RecFlag:        cut_.setRecFlag(v, c.value > 0.f); break;
            case CmdId::PlayFlag:       cut_.setPlayFlag(v, c.value > 0.f); break;
            case CmdId::RecOnceFlag:    cut_.setRecOnceFlag(v, c.value > 0.f); break;
            case CmdId::Position:       cut_.cutToPos(v, c.value); break;
            case CmdId::RecOffset:      cut_.setRecOffset(v, c.value); break;
            case CmdId::PreFilterFc:    cut_.setPreFilterFc(v, c.value); break;
            case CmdId::PreFilterFcMod: cut_.setPreFilterFcMod(v, c.value); break;
            case CmdId::PreFilterRq:    cut_.setPreFilterRq(v, c.value); break;
            case CmdId::PreFilterLp:    cut_.setPreFilterLp(v, c.value); break;
            case CmdId::PreFilterHp:    cut_.setPreFilterHp(v, c.value); break;
            case CmdId::PreFilterBp:    cut_.setPreFilterBp(v, c.value); break;
            case CmdId::PreFilterBr:    cut_.setPreFilterBr(v, c.value); break;
            case CmdId::PreFilterDry:   cut_.setPreFilterDry(v, c.value); break;
            case CmdId::PostFilterFc:   cut_.setPostFilterFc(v, c.value); break;
            case CmdId::PostFilterRq:   cut_.setPostFilterRq(v, c.value); break;
            case CmdId::PostFilterLp:   cut_.setPostFilterLp(v, c.value); break;
            case CmdId::PostFilterHp:   cut_.setPostFilterHp(v, c.value); break;
            case CmdId::PostFilterBp:   cut_.setPostFilterBp(v, c.value); break;
            case CmdId::PostFilterBr:   cut_.setPostFilterBr(v, c.value); break;
            case CmdId::PostFilterDry:  cut_.setPostFilterDry(v, c.value); break;
            case CmdId::RecPreSlewTime: cut_.setRecPreSlewTime(v, c.value); break;
            case CmdId::RateSlewTime:   cut_.setRateSlewTime(v, c.value); break;
            case CmdId::Level:          outLevel_[v].setTarget(c.value); break;
            // pan arrives as [-1,1]; the ramp/mix work in [0,1]
            case CmdId::Pan:            outPan_[v].setTarget((c.value + 1.f) * 0.5f); break;
            case CmdId::LevelSlewTime:  outLevel_[v].setTime(c.value); break;
            case CmdId::PanSlewTime:    outPan_[v].setTime(c.value); break;
            case CmdId::PhaseQuant:     cut_.setPhaseQuant(v, c.value); break;
            case CmdId::PhaseOffset:    cut_.setPhaseOffset(v, c.value); break;
            case CmdId::VoiceSync:      cut_.syncVoice(c.idx0, c.idx1, c.value); break;
            case CmdId::Stop:           cut_.stopVoice(v); break;
            case CmdId::Enable:         enabled_[v] = c.value > 0.f; break;
            case CmdId::FbLevel:        fbLevel_[c.idx0][c.idx1].setTarget(c.value); break;
            case CmdId::InLevel:        inLevel_[c.idx0][c.idx1].setTarget(c.value); break;
            case CmdId::Reset:          cut_.reset(); setDefaults(); break;
        }
    }

    // softcut updates per-voice saved/quant phase once per block internally;
    // nothing to push here, but kept as a hook for future block-rate polling.
    void updatePhases() {}

    // Apply the same per-voice defaults the shell expects after construction or
    // a reset(): rate 1, a 1s loop, looping on, short fade, clean record path.
    void setDefaults() {
        for (int v = 0; v < NumVoices; ++v) {
            cut_.setRate(v, 1.0f);
            cut_.setLoopStart(v, 0.0f);
            cut_.setLoopEnd(v, 1.0f);
            cut_.setLoopFlag(v, true);
            cut_.setFadeTime(v, 0.01f);
            cut_.setRecLevel(v, 1.0f);
            cut_.setPreLevel(v, 0.0f);
            cut_.setPlayFlag(v, false);
            cut_.setRecFlag(v, false);
            outLevel_[v].setTime(0.01f);
            outLevel_[v].setTarget(1.0f);    // unity voice gain
            outPan_[v].setTime(0.01f);
            outPan_[v].setTarget(0.5f);      // centre (0.5 in [0,1])
            enabled_[v] = true;                   // voices active by default
            for (int w = 0; w < NumVoices; ++w) {
                fbLevel_[v][w].setTime(0.01f);
                fbLevel_[v][w].setTarget(0.0f);   // no feedback by default
                inLevel_[v][w].setTime(0.01f);
                // identity input routing: inlet v -> voice v at unity
                inLevel_[v][w].setTarget(v == w ? 1.0f : 0.0f);
            }
        }
    }

    void zeroOutStore() {
        for (int p = 0; p < 2; ++p)
            for (int v = 0; v < NumVoices; ++v)
                for (int i = 0; i < kMaxBlock; ++i)
                    outStore_[p][v][i] = 0.f;
        outCur_ = 0;
    }

    softcut::Softcut<NumVoices>     cut_;
    SpscQueue<Command, kQueueCap>   queue_;
    double                          sampleRate_ = 48000.0;
    softcut::phase_t                lastQuant_[NumVoices];
    softcut::LogRamp                outLevel_[NumVoices];
    softcut::LogRamp                outPan_[NumVoices];
    softcut::LogRamp                fbLevel_[NumVoices][NumVoices];  // [src][dst]
    softcut::LogRamp                inLevel_[NumVoices][NumVoices];  // [inlet][voice]
    bool                            enabled_[NumVoices];
    float                           outScratch_[kMaxBlock];
    float                           recIn_[kMaxBlock];
    float                           mixLf_[kMaxBlock];
    float                           mixRf_[kMaxBlock];
    float                           outStore_[2][NumVoices][kMaxBlock]; // feedback history
    int                             outCur_ = 0;
};

} // namespace softkut

#endif // SOFTKUT_ENGINE_H
