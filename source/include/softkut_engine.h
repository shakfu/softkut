// softkut_engine.h : host-agnostic softcut engine for softkut~.
//
// This header owns everything that does NOT depend on Max: the softcut voice
// array, a lock-free single-producer/single-consumer command queue, the
// double<->float block conversion, buffer-rate scaling, the rate and sync
// signals, and the transport snapshot read by `poll`. The Max externals
// (softkut~, mc.softkut~) are thin shells over it; the offline test harness
// drives it directly.
//
// Threading model (mirrors softcut's own Commands design):
//   - Control threads enqueue commands via the set*/push helpers. Max delivers
//     messages on both the main and the scheduler thread (Overdrive), so
//     push() serializes producers with a mutex. The audio thread never takes it.
//   - ONE audio/consumer thread drains them at the top of process().
//   - setSampleRate() may run on the host's DSP-setup thread while the audio
//     thread is still processing (Max can recompile a running chain). It only
//     records the request; process() applies it before draining commands.
//   - Phase getters read std::atomic state inside softcut and are safe from any
//     thread without going through the queue.
//   - Transport state (play/rec) is NOT safe to read from softcut directly:
//     Voice::playFlag/recFlag are plain bools and the audio thread clears
//     recFlag when a record-once pass finishes. The audio thread publishes a
//     packed state+position snapshot instead, and the poll getters read that.

#ifndef SOFTKUT_ENGINE_H
#define SOFTKUT_ENGINE_H

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>

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
    RecPreSlewTime, RateSlewTime,
    VoiceSync, Stop, FbLevel,
    RecShape, PreShape, SoftClip,
    Reset
};

struct Command {
    CmdId   id;
    int16_t idx0;
    int16_t idx1;
    float   value;
};

// ---------------------------------------------------------------------------
// Parameter validation. softcut checks no values, and some corrupt its state:
//   - |rate| > 64 overruns the resampler's 64-frame output buffer (crash).
//   - rq <= 0 or a negative slew time makes a filter or ramp diverge to
//     inf/NaN, which recording then writes into the buffer.
//   - prelevel > 1 multiplies the buffer on every pass until it overflows;
//     a large reclevel reaches inf the same way.
//   - huge positions overflow softcut's int frame indices.
// sanitize() clamps a value into its safe range and rejects non-finite ones.
// `sr` bounds the seconds-valued parameters.
// ---------------------------------------------------------------------------
enum class Check { Ok, Clamped, Rejected };

static const float  kMaxRate   = 64.f;          // Resampler::OUT_BUF_FRAMES
static const float  kMinRq     = 0.01f;         // SVF peak gain ~1.41/rq
static const double kMaxFrames = 1073741824.0;  // 2^30: int index headroom

inline Check sanitize(CmdId id, float &v, double sr)
{
    if (!std::isfinite(v)) return Check::Rejected;
    const float inf    = std::numeric_limits<float>::infinity();
    const float maxSec = static_cast<float>(kMaxFrames / sr);
    float lo = -inf, hi = inf;
    switch (id) {
        case CmdId::Rate:           lo = -kMaxRate; hi = kMaxRate; break;
        case CmdId::LoopStart:
        case CmdId::LoopEnd:
        case CmdId::Position:       lo = 0.f; hi = maxSec; break;
        case CmdId::VoiceSync:      lo = -maxSec; hi = maxSec; break;
        case CmdId::RecOffset:      lo = static_cast<float>(-kMaxFrames);   // frames
                                    hi = static_cast<float>(kMaxFrames); break;
        case CmdId::RecLevel:
        case CmdId::PreLevel:       lo = 0.f; hi = 1.f; break;
        case CmdId::RecShape:
        case CmdId::PreShape:       lo = 0.f; hi = 2.f; break;   // FadeCurves::Shape
        case CmdId::PreFilterRq:
        case CmdId::PostFilterRq:   lo = kMinRq; break;
        case CmdId::FadeTime:
        case CmdId::RecPreSlewTime:
        case CmdId::RateSlewTime:   lo = 0.f; break;
        default: break;
    }
    const float c = v < lo ? lo : (v > hi ? hi : v);
    if (c == v) return Check::Ok;
    v = c;
    return Check::Clamped;
}

// ---------------------------------------------------------------------------
// One voice's view of a host sample store. Any length; an interleaved
// multichannel store is read one channel at a time: `samples` points at that
// channel's first sample and `stride` is the channel count. `sampleRate` is the
// store's own rate (0 = the DSP rate).
// ---------------------------------------------------------------------------
struct BufferView {
    float       *samples;
    size_t       frames;
    unsigned int stride;
    double       sampleRate;
};

// ---------------------------------------------------------------------------
// Per-voice metadata snapshot (karma~-style). Filled by Engine::getVoiceInfo
// and reported on demand by the shells' `poll` handler. Times are seconds of
// buffer material (the shell converts to ms); `position` is normalized to [0,1]
// within the voice's loop window; `state` is a synthesized play/rec code.
// ---------------------------------------------------------------------------
struct VoiceInfo {
    float position;    // 0..1 within the loop window (start..end)
    float bufferSec;   // play head in the buffer, wrapped to its length
    int   play;        // play flag (0/1)
    int   rec;         // record flag (0/1)
    float startSec;    // loop start (seconds)
    float endSec;      // loop end (seconds)
    float windowSec;   // endSec - startSec
    int   state;       // 0=stop, 1=play, 2=record, 3=overdub (rec<<1 | play)
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

    Engine() : numVoices_(NumVoices) {
        for (int v = 0; v < NumVoices; ++v) {
            pubSnap_[v].store(0, std::memory_order_relaxed);
            wrote_[v].store(false, std::memory_order_relaxed);
            bufRate_[v] = 0.0;
            lastSync_[v] = 0.0;
            bufLen_[v].store(0.0, std::memory_order_relaxed);
            scale_[v].store(1.0f, std::memory_order_relaxed);
            syncPhase_[v].store(false, std::memory_order_relaxed);
            rateSig_[v] = false;
            lastRate_[v] = 1.0f;
            boundFrames_[v] = 0;
        }
        setDefaults();
        clearFeedbackStore();
    }

    // NumVoices is the compile-time maximum (array sizing); the active voice
    // count is a runtime value in [1, NumVoices]. Only active voices are
    // processed.
    int  numVoices() const { return numVoices_; }
    int  maxVoices() const { return NumVoices; }
    void setNumVoices(int n) {
        numVoices_ = n < 1 ? 1 : (n > NumVoices ? NumVoices : n);
    }

    // Records the request only; process() applies it (see threading model).
    void setSampleRate(double sr) {
        requestedSr_.store(sr, std::memory_order_relaxed);
        srPending_.store(true, std::memory_order_release);
    }
    // the most recently requested rate (control-side view)
    double getSampleRate() const { return requestedSr_.load(std::memory_order_relaxed); }

    // ---- producer side: enqueue commands -------------------------------
    bool push(const Command &c) {
        std::lock_guard<std::mutex> lock(pushMutex_);   // serialize producers
        return queue_.push(c);
    }
    size_t pending() const { return queue_.size(); }
    // commands applied by the audio thread so far (for tests/diagnostics)
    uint64_t handled() const { return handled_.load(std::memory_order_relaxed); }

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
    // record head offset from the play head, in frames of buffer material
    bool setRecOffset(int v, float frames) { return cmd(CmdId::RecOffset, v, frames); }

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
    // fade shapes: 0 linear, 1 sine, 2 raised (softcut's record default, which
    // inverts polarity); soft clip: softcut's 1.2x clipper on the write path
    bool setRecShape(int v, int shape)     { return cmd(CmdId::RecShape, v, (float)shape); }
    bool setPreShape(int v, int shape)     { return cmd(CmdId::PreShape, v, (float)shape); }
    bool setSoftClip(int v, bool on)       { return cmd(CmdId::SoftClip, v, on ? 1.f : 0.f); }
    bool setRateSlewTime(int v, float s)   { return cmd(CmdId::RateSlewTime, v, s); }

    bool syncVoice(int follow, int lead, float offset) {
        return push(Command{CmdId::VoiceSync, (int16_t)follow, (int16_t)lead, offset});
    }
    // play 0 and rec 0 as one command, so both apply in the same block
    bool stopVoice(int v) { return cmd(CmdId::Stop, v, 0.f); }
    // Sync output: false = head in seconds of buffer material, true = 0-1
    // across the loop. Read by the audio thread each block; no queue needed.
    void setSyncPhase(int v, bool b) { syncPhase_[v].store(b, std::memory_order_relaxed); }
    // route voice `src`'s output into voice `dst`'s record input at gain `g`,
    // one chunk later (the latency of a send~/receive~ loop in Max)
    bool setFeedback(int src, int dst, float g) {
        return push(Command{CmdId::FbLevel, (int16_t)src, (int16_t)dst, g});
    }
    bool reset() { return push(Command{CmdId::Reset, 0, 0, 0.f}); }

    // ---- consumer side: drain queue + process one block ----------------
    // ins/voiceOuts: one array of `nframes` host samples (double) per voice:
    //   voice v records ins[v], plus any feedback routes into it, and plays
    //   into voiceOuts[v]. A null ins[v] is silence.
    // bufs: one BufferView per voice. A null `bufs`, or a view with null
    //   samples or zero frames, makes that voice silent.
    // syncOuts: optional, one array per voice (entries may be null): the play
    //   head for every sample, in seconds of buffer material, wrapped to the
    //   buffer's length (or 0-1 across the loop, see setSyncPhase). A voice with
    //   no buffer holds its last value.
    // rateIns: optional, one array per voice (entries may be null): the rate
    //   for every sample, in place of setRate(). Clamped to +/-kMaxRate; a
    //   non-finite sample repeats the previous one.
    // A host vector longer than kMaxBlock is split into kMaxBlock chunks rather
    // than clamped: the scratch buffers and the feedback store hold one chunk,
    // so a clamped pass left every output sample past kMaxBlock unwritten.
    void process(const double *const *ins, double *const *voiceOuts, int nframes,
                 const BufferView *bufs, double *const *syncOuts = nullptr,
                 const double *const *rateIns = nullptr) {
        if (srPending_.exchange(false, std::memory_order_acquire))
            applySampleRate(requestedSr_.load(std::memory_order_relaxed));
        // bind the buffers before the drain: a cut computes the write index
        // from the buffer length, and times scale by the buffer's rate
        bindBuffers(bufs);
        drain();

        for (int v = 0; v < numVoices_; ++v)
            wrote_[v].store(false, std::memory_order_relaxed);

        const double *inSlice[NumVoices];
        double       *outSlice[NumVoices];
        double       *syncSlice[NumVoices];
        const double *rateSlice[NumVoices];
        for (int off = 0; off < nframes; off += kMaxBlock) {
            const int n = (nframes - off < kMaxBlock) ? (nframes - off) : kMaxBlock;
            for (int v = 0; v < numVoices_; ++v) {
                inSlice[v]  = ins[v] ? ins[v] + off : nullptr;
                outSlice[v] = voiceOuts[v] + off;
                syncSlice[v] = (syncOuts && syncOuts[v]) ? syncOuts[v] + off : nullptr;
                rateSlice[v] = (rateIns && rateIns[v]) ? rateIns[v] + off : nullptr;
            }
            processChunk(inSlice, outSlice, syncSlice, rateSlice, n);
        }
    }

    // ---- phase / transport poll (safe from any thread) -----------------
    // play/rec come from the snapshot the audio thread publishes at the end of
    // each chunk, never from softcut's plain-bool flags.
    // positions in seconds of buffer material (see BufferView::sampleRate)
    // getSavedPosition is the head position softcut steps, which can pass the
    // buffer's end when a loop point does (reads wrap there).
    double getSavedPosition(int v) { return cut_.getSavedPosition(v) / scale(v); }
    // buffer length (seconds) as of the voice's last processed block; 0 = none
    double getBufferSeconds(int v) const { return bufLen_[v].load(std::memory_order_relaxed); }
    bool   getPlayFlag(int v)      { return (snapState(snap(v)) & 1) != 0; }
    bool   getRecFlag(int v)       { return (snapState(snap(v)) & 2) != 0; }
    // True if voice v wrote to its buffer during the last process() call. A
    // record-once pass clears the record flag inside that same call, so the
    // record flag alone misses the block holding its final writes.
    bool   getWroteBlock(int v)    { return wrote_[v].load(std::memory_order_relaxed); }

    // Snapshot the karma~-style metadata for one voice. Safe from the control
    // thread: position and play/rec come from one published word, so a report
    // never pairs one block's transport state with another block's position.
    // The loop bounds are cached in std::atomic<float> updated wherever loop
    // start/end change.
    VoiceInfo getVoiceInfo(int v) {
        const uint64_t sn    = snap(v);
        const int      st    = snapState(sn);
        const float    pos   = snapPos(sn);
        const float    start = loopStart_[v].load(std::memory_order_relaxed);
        const float    end   = static_cast<float>(loopEndSec(v));
        const float    win   = end - start;
        const float    norm  = win > 1e-9f ? (pos - start) / win : 0.f;
        VoiceInfo info;
        info.position  = norm < 0.f ? 0.f : (norm > 1.f ? 1.f : norm);
        info.bufferSec = static_cast<float>(wrapToBuffer(v, pos));
        info.play      = (st & 1) ? 1 : 0;
        info.rec       = (st & 2) ? 1 : 0;
        info.startSec  = start;
        info.endSec    = end;
        info.windowSec = end - start;
        info.state     = st;
        return info;
    }

private:
    // One word carries the whole poll snapshot: the transport code (rec<<1 |
    // play) in the high half, the head position in seconds (float bits) in the
    // low half. Written only by the audio thread, read by anyone.
    static_assert(ATOMIC_LLONG_LOCK_FREE == 2,
                  "the poll snapshot must be lock-free: the audio thread writes it");

    static uint64_t packSnap(int st, float pos) {
        uint32_t bits;
        std::memcpy(&bits, &pos, sizeof bits);
        return (static_cast<uint64_t>(static_cast<uint32_t>(st)) << 32) | bits;
    }
    static int   snapState(uint64_t sn) { return static_cast<int>(sn >> 32); }
    static float snapPos(uint64_t sn) {
        const uint32_t bits = static_cast<uint32_t>(sn);
        float pos;
        std::memcpy(&pos, &bits, sizeof pos);
        return pos;
    }
    uint64_t snap(int v) const { return pubSnap_[v].load(std::memory_order_acquire); }

    void publishSnap(int v) {
        const float pos = static_cast<float>(getSavedPosition(v));
        const int   st  = (cut_.getRecFlag(v) ? 2 : 0) | (cut_.getPlayFlag(v) ? 1 : 0);
        pubSnap_[v].store(packSnap(st, pos), std::memory_order_release);
    }

    // a position in seconds, wrapped into the voice's buffer as softcut wraps
    // its read index
    double wrapToBuffer(int v, double sec) const {
        const double len = bufLen_[v].load(std::memory_order_relaxed);
        if (!(len > 0.0)) return sec;
        const double m = std::fmod(sec, len);
        return m < 0.0 ? m + len : m;
    }

    // one <= kMaxBlock chunk; the queue is already drained by the caller.
    void processChunk(const double *const *ins, double *const *voiceOuts,
                      double *const *syncOuts, const double *const *rateIns, int nframes) {
        for (int v = 0; v < numVoices_; ++v) {
            const double *in = ins[v];
            for (int i = 0; i < nframes; ++i) recIn_[i] = in ? static_cast<float>(in[i]) : 0.f;
            addFeedback(v, nframes);

            const size_t frames = boundFrames_[v];
            const bool   active = frames >= 1;
            // the record flag only changes at the drain above or at the end of
            // processBlock (record-once completion), so its value here says
            // whether this chunk writes to the buffer.
            const bool   writing = active && cut_.getRecFlag(v);
            if (active) {
                const double rate = bufRate_[v] > 0.0 ? bufRate_[v] : sampleRate_;
                const float *rateBuf = fillRate(v, rateIns[v], nframes);
                cut_.processBlock(v, recIn_, outScratch_, nframes,
                                  syncOuts[v] ? phaseScratch_ : nullptr, rateBuf);
                if (syncOuts[v]) writeSync(v, syncOuts[v], nframes, frames, rate);
            } else {
                for (int i = 0; i < nframes; ++i) outScratch_[i] = 0.f;
                if (syncOuts[v])
                    for (int i = 0; i < nframes; ++i) syncOuts[v][i] = lastSync_[v];
            }
            if (writing) wrote_[v].store(true, std::memory_order_relaxed);
            publishSnap(v);

            double *out = voiceOuts[v];
            float  *keep = outStore_[outCur_][v];   // next chunk's feedback source
            for (int i = 0; i < nframes; ++i) {
                out[i]  = static_cast<double>(outScratch_[i]);
                keep[i] = outScratch_[i];
            }
        }
        outCur_ ^= 1;
    }

    // Mix the previous chunk's outputs into voice dst's record input. Every
    // voice reads the previous chunk, so the voice order does not matter. A
    // route whose gain is 0 and has slewed to 0 is skipped: idle routes cost
    // nothing, where updating all N^2 ramps cost 512 per sample at 16 voices.
    void addFeedback(int dst, int nframes) {
        const float (*prev)[kMaxBlock] = outStore_[outCur_ ^ 1];
        for (int src = 0; src < numVoices_; ++src) {
            if (fbTarget_[src][dst] == 0.f && std::fabs(fbLast_[src][dst]) < 1e-6f) continue;
            softcut::LogRamp &g = fbLevel_[src][dst];
            float last = 0.f;
            for (int i = 0; i < nframes; ++i) {
                last = g.update();
                recIn_[i] += prev[src][i] * last;
            }
            fbLast_[src][dst] = last;
        }
    }

    // fresh DSP start or reset: no audio from before re-enters a voice
    void clearFeedbackStore() {
        std::memset(outStore_, 0, sizeof outStore_);
        outCur_ = 0;
    }

    // Point each voice at its buffer for this process() call. A null `bufs`,
    // or a view with no samples or frames, leaves the voice unbound (silent).
    void bindBuffers(const BufferView *bufs) {
        for (int v = 0; v < numVoices_; ++v) {
            const BufferView *bv = bufs ? &bufs[v] : nullptr;
            float *buf = bv ? bv->samples : nullptr;
            // softcut indexes frames with int: cap far beyond any real buffer~
            const size_t frames = !buf ? 0 : (bv->frames < (size_t)kMaxFrames
                                              ? bv->frames : (size_t)kMaxFrames);
            boundFrames_[v] = frames;
            if (frames < 1) continue;
            const double rate = bv->sampleRate > 0.0 ? bv->sampleRate : sampleRate_;
            const double len  = static_cast<double>(frames) / rate;
            const bool   resized = len != bufLen_[v].load(std::memory_order_relaxed);
            bufLen_[v].store(len, std::memory_order_relaxed);
            // reapplyVoice() reads bufLen_, so store the length first
            if (bv->sampleRate != bufRate_[v]) setBufferRate(v, bv->sampleRate);
            else if (resized) applyLoopEnd(v);   // loop end 0 follows the buffer
            cut_.setVoiceBuffer(v, buf, frames, bv->stride);
        }
    }

    // head phase (buffer frames, per sample) -> seconds of buffer material,
    // wrapped as softcut wraps its read index
    void writeSync(int v, double *sync, int nframes, size_t frames, double rate) {
        const double len = static_cast<double>(frames);
        if (syncPhase_[v].load(std::memory_order_relaxed)) {
            // 0-1 across the loop, from the unwrapped head
            const double start = loopStart_[v].load(std::memory_order_relaxed);
            const double win   = loopEndSec(v) - start;
            for (int i = 0; i < nframes; ++i) {
                const double p = win > 1e-9 ? (phaseScratch_[i] / rate - start) / win : 0.0;
                sync[i] = p < 0.0 ? 0.0 : (p > 1.0 ? 1.0 : p);
            }
            lastSync_[v] = sync[nframes - 1];
            return;
        }
        for (int i = 0; i < nframes; ++i) {
            double f = phaseScratch_[i];
            if (f < 0.0 || f >= len) {
                f = std::fmod(f, len);
                if (f < 0.0) f += len;
            }
            sync[i] = f / rate;
        }
        lastSync_[v] = sync[nframes - 1];
    }

    // The rate signal for one chunk, scaled to the buffer rate and clamped
    // (|rate| > 64 overruns softcut's resampler). Null without a signal. When a
    // signal is removed, the voice returns to the setRate() value.
    const float *fillRate(int v, const double *in, int nframes) {
        if (!in) {
            if (rateSig_[v]) { rateSig_[v] = false; applyRate(v); }
            return nullptr;
        }
        rateSig_[v] = true;
        const float k = scale(v);
        float r = lastRate_[v];
        for (int i = 0; i < nframes; ++i) {
            const float x = static_cast<float>(in[i]);
            if (std::isfinite(x)) r = x;
            float y = r * k;
            y = y < -kMaxRate ? -kMaxRate : (y > kMaxRate ? kMaxRate : y);
            rateScratch_[i] = y;
        }
        lastRate_[v] = r;
        return rateScratch_;
    }

    // loop end in seconds of buffer material; 0 means the buffer's end (once
    // a buffer has been seen)
    double loopEndSec(int v) const {
        const double e = loopEnd_[v].load(std::memory_order_relaxed);
        if (e > 0.0) return e;
        const double len = bufLen_[v].load(std::memory_order_relaxed);
        return len > 0.0 ? len : e;
    }
    void applyLoopEnd(int v) {
        cut_.setLoopEnd(v, scaled(v, CmdId::LoopEnd, static_cast<float>(loopEndSec(v))));
    }

    bool cmd(CmdId id, int v, float val) {
        return push(Command{id, (int16_t)v, 0, val});
    }

    void drain() {
        Command c;
        while (queue_.pop(c)) {
            handle(c);
            handled_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // audio thread only (see setSampleRate)
    void applySampleRate(double sr) {
        sampleRate_ = sr;
        cut_.setSampleRate(static_cast<unsigned int>(sr));
        for (int v = 0; v < NumVoices; ++v) {
            updateScale(v);
            reapplyVoice(v);
            for (int w = 0; w < NumVoices; ++w) fbLevel_[v][w].setSampleRate(static_cast<float>(sr));
        }
        clearFeedbackStore();
    }

    // ---- buffer sample rate ------------------------------------------------
    // softcut converts seconds to frames, and steps its heads, at the DSP rate.
    // To address a store recorded at another rate, every time and the rate are
    // scaled by bufferRate/dspRate before they reach softcut, and positions read
    // back are divided by it. Like groove~, rate 1 then plays the store at its
    // own speed, and times are seconds of store material.
    float scale(int v) const { return scale_[v].load(std::memory_order_relaxed); }

    void updateScale(int v) {
        const double k = bufRate_[v] > 0.0 ? bufRate_[v] / sampleRate_ : 1.0;
        scale_[v].store(static_cast<float>(k), std::memory_order_relaxed);
    }

    void setBufferRate(int v, double rate) {
        bufRate_[v] = rate;
        updateScale(v);
        reapplyVoice(v);
    }

    // seconds of store material -> the seconds softcut expects, kept in range
    float scaled(int v, CmdId id, float sec) const {
        float x = sec * scale(v);
        sanitize(id, x, sampleRate_);
        return x;
    }

    void applyRate(int v) {
        float r = rate_[v] * scale(v);
        sanitize(CmdId::Rate, r, sampleRate_);   // |rate * scale| <= 64 too
        cut_.setRate(v, r);
    }

    // softcut stores times as frames at the rate current when they were set, so
    // a DSP-rate or store-rate change re-applies every cached time and the rate.
    void reapplyVoice(int v) {
        applyRate(v);
        cut_.setLoopStart(v, scaled(v, CmdId::LoopStart, loopStart_[v].load(std::memory_order_relaxed)));
        applyLoopEnd(v);
        cut_.setFadeTime(v, scaled(v, CmdId::FadeTime, fadeTime_[v].load(std::memory_order_relaxed)));
        applyRecOffset(v);
    }

    // softcut takes the offset in seconds at the DSP rate and stores frames;
    // frames of buffer material are the same frames
    void applyRecOffset(int v) {
        cut_.setRecOffset(v, recOffset_[v].load(std::memory_order_relaxed)
                             / static_cast<float>(sampleRate_));
    }

    void handle(Command c) {
        if (sanitize(c.id, c.value, sampleRate_) == Check::Rejected) return;
        const int v = c.idx0;
        switch (c.id) {
            case CmdId::Rate:           rate_[v] = c.value; applyRate(v); break;
            case CmdId::LoopStart:      cut_.setLoopStart(v, scaled(v, c.id, c.value));
                                        loopStart_[v].store(c.value, std::memory_order_relaxed); break;
            case CmdId::LoopEnd:        loopEnd_[v].store(c.value, std::memory_order_relaxed);
                                        applyLoopEnd(v); break;
            case CmdId::LoopFlag:       cut_.setLoopFlag(v, c.value > 0.f); break;
            case CmdId::FadeTime:       cut_.setFadeTime(v, scaled(v, c.id, c.value));
                                        fadeTime_[v].store(c.value, std::memory_order_relaxed); break;
            case CmdId::RecLevel:       cut_.setRecLevel(v, c.value); break;
            case CmdId::PreLevel:       cut_.setPreLevel(v, c.value); break;
            case CmdId::RecFlag:        cut_.setRecFlag(v, c.value > 0.f); break;
            case CmdId::PlayFlag:       cut_.setPlayFlag(v, c.value > 0.f); break;
            // record-once writes only while the record flag is set: arm both
            case CmdId::RecOnceFlag:    cut_.setRecOnceFlag(v, c.value > 0.f);
                                        if (c.value > 0.f) cut_.setRecFlag(v, true); break;
            case CmdId::Position:       cut_.cutToPos(v, scaled(v, c.id, c.value)); break;
            case CmdId::RecOffset:      recOffset_[v].store(c.value, std::memory_order_relaxed);
                                        applyRecOffset(v); break;
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
            // softcut's syncVoice adds offsets in DSP seconds; the two voices'
            // stores may run at different rates, so convert through each scale.
            // At drain time the saved position is the lead's current position.
            case CmdId::VoiceSync:
                cut_.cutToPos(c.idx0, scaled(c.idx0, CmdId::Position,
                              static_cast<float>(getSavedPosition(c.idx1)) + c.value));
                break;
            case CmdId::Stop:           cut_.setPlayFlag(v, false);
                                        cut_.setRecFlag(v, false); break;
            case CmdId::FbLevel:        fbLevel_[c.idx0][c.idx1].setTarget(c.value);
                                        fbTarget_[c.idx0][c.idx1] = c.value; break;
            case CmdId::RecShape:       cut_.setRecFadeShape(v, (int)std::lround(c.value)); break;
            case CmdId::PreShape:       cut_.setPreFadeShape(v, (int)std::lround(c.value)); break;
            case CmdId::SoftClip:       cut_.setSoftClip(v, c.value > 0.f); break;
            // stop every voice and restore all defaults
            case CmdId::Reset:          cut_.reset(); setDefaults(); clearFeedbackStore(); break;
        }
    }

    // Apply the same per-voice defaults the shell expects after construction or
    // a reset(): rate 1, the whole buffer as the loop, looping on, short fade,
    // and a transparent record path (no input filter, soft clip or inverting
    // record fade).
    void setDefaults() {
        for (int v = 0; v < NumVoices; ++v) {
            rate_[v] = 1.0f;
            loopStart_[v].store(0.0f, std::memory_order_relaxed);
            loopEnd_[v].store(0.0f, std::memory_order_relaxed);   // buffer end
            fadeTime_[v].store(0.01f, std::memory_order_relaxed);
            recOffset_[v].store(-8.0f, std::memory_order_relaxed);   // softcut's default
            reapplyVoice(v);
            // softcut updates a quantized phase every sample but never
            // initializes its quantum, and reset() leaves the offset: zero both
            cut_.setPhaseQuant(v, 0.0f);
            cut_.setPhaseOffset(v, 0.0f);
            cut_.setLoopFlag(v, true);
            cut_.setRecLevel(v, 1.0f);
            cut_.setPreLevel(v, 0.0f);
            cut_.setPlayFlag(v, false);
            cut_.setRecFlag(v, false);
            // softcut low-passes the record input at 16 kHz by default; a Max
            // looper should record its input unfiltered
            cut_.setPreFilterLp(v, 0.0f);
            cut_.setPreFilterDry(v, 1.0f);
            // softcut's Raised record fade inverts polarity, and its soft clip
            // gains the input 1.2x
            cut_.setRecFadeShape(v, 0);   // linear
            cut_.setPreFadeShape(v, 0);   // linear, softcut's default
            cut_.setSoftClip(v, false);
            for (int w = 0; w < NumVoices; ++w) {   // no feedback routes
                fbLevel_[v][w] = softcut::LogRamp(static_cast<float>(sampleRate_), 0.01f);
                fbTarget_[v][w] = 0.f;
                fbLast_[v][w]   = 0.f;
            }
        }
    }

    int                             numVoices_;     // active voice count <= NumVoices
    softcut::Softcut<NumVoices>     cut_;
    SpscQueue<Command, kQueueCap>   queue_;
    std::mutex                      pushMutex_;     // producers only
    double                          sampleRate_ = 48000.0;  // audio thread
    std::atomic<double>             requestedSr_{48000.0};
    std::atomic<bool>               srPending_{false};
    std::atomic<uint64_t>           pubSnap_[NumVoices];    // state<<32 | position
    std::atomic<bool>               wrote_[NumVoices];      // buffer written last block
    std::atomic<float>              loopStart_[NumVoices];  // cached loop bounds
    std::atomic<float>              loopEnd_[NumVoices];    // (seconds) for reports
    std::atomic<float>              fadeTime_[NumVoices];   // re-applied on a
    std::atomic<float>              recOffset_[NumVoices];  // sample-rate change (frames)
    std::atomic<uint64_t>           handled_{0};
    float                           rate_[NumVoices];       // user rate (audio thread)
    double                          bufRate_[NumVoices];    // store rate, 0 = DSP rate
    std::atomic<float>              scale_[NumVoices];      // bufRate / DSP rate
    std::atomic<double>             bufLen_[NumVoices];     // buffer length, seconds
    float                           outScratch_[kMaxBlock];
    softcut::phase_t                phaseScratch_[kMaxBlock];  // per-sample head phase
    double                          lastSync_[NumVoices];      // held while inactive
    std::atomic<bool>               syncPhase_[NumVoices];     // sync as 0-1 of the loop
    bool                            rateSig_[NumVoices];       // last chunk had a rate signal
    size_t                          boundFrames_[NumVoices];   // this call's buffer, 0 = none
    float                           lastRate_[NumVoices];      // held over non-finite samples
    float                           rateScratch_[kMaxBlock];
    float                           recIn_[kMaxBlock];
    softcut::LogRamp                fbLevel_[NumVoices][NumVoices];  // [src][dst]
    float                           fbTarget_[NumVoices][NumVoices];
    float                           fbLast_[NumVoices][NumVoices];   // last ramp output
    float                           outStore_[2][NumVoices][kMaxBlock]; // outputs, ping-ponged
    int                             outCur_ = 0;
};

} // namespace softkut

#endif // SOFTKUT_ENGINE_H
