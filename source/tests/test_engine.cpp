// Offline test harness for softkut::Engine.
//
// Exercises the host-agnostic engine without Max: the SPSC command queue, the
// power-of-two buffer framing, command routing through the queue into softcut,
// a record-then-playback round trip, idle-voice silence, and phase advance.
//
// Self-contained: no external test framework. Exit code 0 = all pass.

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <limits>
#include <thread>
#include <vector>

#include "softkut_engine.h"

using softkut::Engine;
using softkut::SpscQueue;

// ---------------------------------------------------------------------------
static int g_total = 0;
static int g_fail  = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        ++g_total;                                                         \
        if (!(cond)) { ++g_fail; std::printf("  FAIL [line %d]: %s\n", __LINE__, #cond); } \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                              \
    do {                                                                   \
        ++g_total;                                                         \
        double da = (double)(a), db = (double)(b);                         \
        if (std::fabs(da - db) > (tol)) {                                  \
            ++g_fail;                                                      \
            std::printf("  FAIL [line %d]: |%g - %g| > %g\n", __LINE__, da, db, (double)(tol)); \
        }                                                                  \
    } while (0)

static const int    NV = 6;
static const double SR = 48000.0;
typedef Engine<NV>  Eng;

// A mono BufferView at the DSP rate; a null store makes the voice silent.
static softkut::BufferView mono(float *p, size_t frames)
{
    return softkut::BufferView{p, p ? frames : 0, 1, 0.0};
}

// Run `total` samples through the engine in B-sized blocks with per-voice
// buffers `bufs` (each `frames` long), feeding a constant `inval` to voice 0.
// Optionally capture voice-0, voice-1, mix-L and mix-R outputs.
static void runPV(Eng &e, float *const bufs[NV], size_t frames, int total, double inval,
                  std::vector<double> *cap0, std::vector<double> *cap1,
                  std::vector<double> *capL, std::vector<double> *capR)
{
    const int B = 64;
    std::vector<std::vector<double> > in(NV, std::vector<double>(B, 0.0));
    std::vector<std::vector<double> > out(NV, std::vector<double>(B, 0.0));
    std::vector<double *> inp(NV), outp(NV);
    for (int v = 0; v < NV; ++v) { inp[v] = in[v].data(); outp[v] = out[v].data(); }
    std::vector<double> mL(B, 0.0), mR(B, 0.0);
    softkut::BufferView fr[NV];
    for (int v = 0; v < NV; ++v) fr[v] = mono(bufs[v], frames);

    int done = 0;
    while (done < total) {
        int n = (total - done < B) ? (total - done) : B;
        for (int i = 0; i < n; ++i) in[0][i] = inval;
        e.process(inp.data(), outp.data(), n, fr, mL.data(), mR.data());
        if (cap0) for (int i = 0; i < n; ++i) cap0->push_back(out[0][i]);
        if (cap1) for (int i = 0; i < n; ++i) cap1->push_back(out[1][i]);
        if (capL) for (int i = 0; i < n; ++i) capL->push_back(mL[i]);
        if (capR) for (int i = 0; i < n; ++i) capR->push_back(mR[i]);
        done += n;
    }
}

// Convenience: all voices share one buffer; capture voice-0 only.
static void run(Eng &e, float *buf, size_t frames, int total, double inval,
                std::vector<double> *cap)
{
    float *bufs[NV];
    for (int v = 0; v < NV; ++v) bufs[v] = buf;
    runPV(e, bufs, frames, total, inval, cap, NULL, NULL, NULL);
}

// Steady-state mean of the interior quarter..three-quarter region.
static double midMean(const std::vector<double> &v)
{
    double sum = 0.0; size_t cnt = 0;
    for (size_t i = v.size() / 4; i < (v.size() * 3) / 4; ++i) { sum += v[i]; ++cnt; }
    return cnt ? sum / (double)cnt : 0.0;
}

// Configure a voice to loop-play the whole buffer at unity rate.
static void configPlay(Eng &e, int v, size_t F)
{
    const float endSec = (float)F / (float)SR;
    e.setLoopStart(v, 0.f);
    e.setLoopEnd(v, endSec);
    e.setRate(v, 1.0f);
    e.setFadeTime(v, 0.001f);
    e.cutToPos(v, 0.f);
    e.setPlayFlag(v, true);
    e.setRecFlag(v, false);
}

// ---------------------------------------------------------------------------
static void test_spsc_queue()
{
    std::printf("test_spsc_queue\n");
    SpscQueue<int, 4> q;            // capacity 4 -> 3 usable slots
    CHECK(q.size() == 0);
    CHECK(q.push(10));
    CHECK(q.push(20));
    CHECK(q.push(30));
    CHECK(!q.push(40));             // full
    CHECK(q.size() == 3);

    int v = 0;
    CHECK(q.pop(v) && v == 10);     // FIFO order
    CHECK(q.pop(v) && v == 20);
    CHECK(q.pop(v) && v == 30);
    CHECK(!q.pop(v));               // empty
    CHECK(q.size() == 0);

    // wrap-around still FIFO
    CHECK(q.push(1)); CHECK(q.push(2));
    CHECK(q.pop(v) && v == 1);
    CHECK(q.push(3));
    CHECK(q.pop(v) && v == 2);
    CHECK(q.pop(v) && v == 3);
}

static void test_command_drain()
{
    std::printf("test_command_drain\n");
    Eng e;
    e.setSampleRate(SR);
    const size_t F = 8192;
    std::vector<float> buf(F, 0.f);

    e.setRate(0, 2.0f);
    e.setPlayFlag(0, true);
    CHECK(e.pending() >= 2);        // queued, not yet applied

    run(e, buf.data(), F, 64, 0.0, NULL);
    CHECK(e.pending() == 0);        // drained by process()
}

static void test_record_playback()
{
    // Round-trip test. Note softcut's record path is intentionally colored: a
    // soft-clip stage with 1.2x default gain plus a polarity-inverting default
    // "Raised" rec-fade curve. So a DC input is NOT stored at unity; this test
    // therefore validates (a) that recording writes a non-trivial value derived
    // from the input, and (b) that playback faithfully reproduces whatever is in
    // the buffer -- which is what exercises the engine's read path, the
    // float<->double conversion, and command routing through the queue.
    std::printf("test_record_playback\n");
    Eng e;
    e.setSampleRate(SR);

    const size_t F      = 8192;            // power of two
    const float  endSec = (float)F / (float)SR;
    std::vector<float> buf(F, 0.f);

    e.setLoopStart(0, 0.f);
    e.setLoopEnd(0, endSec);
    e.setRate(0, 1.0f);
    e.setFadeTime(0, 0.001f);
    e.setRecLevel(0, 1.0f);
    e.setPreLevel(0, 0.0f);                // overwrite (no overdub)
    e.cutToPos(0, 0.f);
    e.setRecFlag(0, true);
    e.setPlayFlag(0, true);

    // record a DC level of 0.5 over two full loops
    run(e, buf.data(), F, (int)F * 2, 0.5, NULL);

    // recording wrote a non-trivial, consistent value into the buffer interior
    const double stored = buf[F / 2];
    CHECK(std::fabs(stored) > 0.3);
    CHECK_NEAR(buf[F / 4], stored, 0.05);   // interior is uniform for DC input

    // play back (record off) and capture one loop
    e.setRecFlag(0, false);
    e.cutToPos(0, 0.f);
    std::vector<double> cap;
    run(e, buf.data(), F, (int)F, 0.0, &cap);

    // playback reproduces the stored buffer value in the steady interior
    double sum = 0.0; int cnt = 0;
    for (size_t i = F / 4; i < (F * 3) / 4; ++i) { sum += cap[i]; ++cnt; }
    double mean = sum / cnt;
    CHECK_NEAR(mean, stored, 0.06);
    CHECK(std::fabs(mean) > 0.3);           // clearly non-zero playback
}

static void test_idle_voice_silent()
{
    std::printf("test_idle_voice_silent\n");
    Eng e;
    e.setSampleRate(SR);
    const size_t F = 8192;
    std::vector<float> buf(F, 0.3f);        // non-zero buffer content

    std::vector<double> cap;
    run(e, buf.data(), F, 256, 0.0, &cap);  // play/rec off by default

    double maxabs = 0.0;
    for (size_t i = 0; i < cap.size(); ++i)
        if (std::fabs(cap[i]) > maxabs) maxabs = std::fabs(cap[i]);
    CHECK(maxabs < 1e-6);                    // idle voice outputs silence
}

static void test_phase_advance()
{
    std::printf("test_phase_advance\n");
    Eng e;
    e.setSampleRate(SR);
    const size_t F      = 8192;
    const float  endSec = (float)F / (float)SR;
    std::vector<float> buf(F, 0.f);

    e.setLoopStart(0, 0.f);
    e.setLoopEnd(0, endSec);
    e.setRate(0, 1.0f);
    e.cutToPos(0, 0.f);
    e.setPlayFlag(0, true);

    run(e, buf.data(), F, (int)F / 4, 0.0, NULL);   // play a quarter loop

    double p = e.getSavedPosition(0);
    CHECK(p > 0.0);                          // head advanced
    CHECK(p < (double)F);                     // sane (sec or sample units)
}

static void test_output_level()
{
    // Playing a pre-filled DC buffer, the per-voice output scales with `level`.
    std::printf("test_output_level\n");
    Eng e;
    e.setSampleRate(SR);
    const size_t F = 8192;
    std::vector<float> buf(F, 0.5f);        // DC buffer content
    configPlay(e, 0, F);

    e.setLevel(0, 1.0f);
    std::vector<double> c1;
    run(e, buf.data(), F, 4000, 0.0, &c1);
    CHECK_NEAR(midMean(c1), 0.5, 0.06);     // unity gain passes the buffer

    e.setLevel(0, 0.0f);
    std::vector<double> c0;
    run(e, buf.data(), F, 4000, 0.0, &c0);
    CHECK(std::fabs(midMean(c0)) < 0.02);   // level 0 -> silence (after slew)
}

static void test_pan()
{
    // Equal-power pan steers a voice between the stereo-mix L/R outputs.
    std::printf("test_pan\n");
    Eng e;
    e.setSampleRate(SR);
    const size_t F = 8192;
    std::vector<float> buf(F, 0.5f);
    float *bufs[NV]; for (int v = 0; v < NV; ++v) bufs[v] = (v == 0) ? buf.data() : NULL;
    configPlay(e, 0, F);
    e.setLevel(0, 1.0f);

    e.setPan(0, -1.0f);                     // hard left
    std::vector<double> L1, R1;
    runPV(e, bufs, F, 4000, 0.0, NULL, NULL, &L1, &R1);
    CHECK_NEAR(midMean(L1), 0.5, 0.06);
    CHECK(std::fabs(midMean(R1)) < 0.03);

    e.setPan(0, 1.0f);                      // hard right
    std::vector<double> L2, R2;
    runPV(e, bufs, F, 4000, 0.0, NULL, NULL, &L2, &R2);
    CHECK(std::fabs(midMean(L2)) < 0.03);
    CHECK_NEAR(midMean(R2), 0.5, 0.06);
}

static void test_per_voice_buffers()
{
    // Each voice reads its own buffer~ independently.
    std::printf("test_per_voice_buffers\n");
    Eng e;
    e.setSampleRate(SR);
    const size_t F = 8192;
    std::vector<float> bufA(F,  0.5f);
    std::vector<float> bufB(F, -0.4f);
    float *bufs[NV];
    bufs[0] = bufA.data();
    bufs[1] = bufB.data();
    for (int v = 2; v < NV; ++v) bufs[v] = NULL;

    configPlay(e, 0, F);
    configPlay(e, 1, F);

    std::vector<double> c0, c1;
    runPV(e, bufs, F, 4000, 0.0, &c0, &c1, NULL, NULL);
    CHECK_NEAR(midMean(c0),  0.5, 0.06);    // voice 0 reads bufA
    CHECK_NEAR(midMean(c1), -0.4, 0.06);    // voice 1 reads bufB
}

static void test_stop()
{
    // "stop" parks a playing voice's heads -> output falls silent.
    std::printf("test_stop\n");
    Eng e;
    e.setSampleRate(SR);
    const size_t F = 8192;
    std::vector<float> buf(F, 0.5f);
    configPlay(e, 0, F);

    std::vector<double> before;
    run(e, buf.data(), F, 2000, 0.0, &before);
    CHECK(std::fabs(midMean(before)) > 0.3);   // playing

    e.stopVoice(0);
    std::vector<double> after;
    run(e, buf.data(), F, 2000, 0.0, &after);
    CHECK(std::fabs(midMean(after)) < 0.02);    // stopped -> silent
}

static void test_feedback()
{
    // voice 0 plays a DC buffer; its output is routed into voice 1's record
    // input via the feedback matrix, so voice 1 records a non-trivial signal.
    std::printf("test_feedback\n");
    const size_t F      = 8192;
    const float  endSec = (float)F / (float)SR;

    // helper: run the scenario with feedback gain g, return |bufB interior|
    struct Scen {
        static double recordedMag(float g, size_t F, float endSec) {
            Eng e;
            e.setSampleRate(SR);
            std::vector<float> bufA(F, 0.3f);   // voice 0 source (DC)
            std::vector<float> bufB(F, 0.0f);   // voice 1 record target
            float *bufs[NV];
            bufs[0] = bufA.data();
            bufs[1] = bufB.data();
            for (int v = 2; v < NV; ++v) bufs[v] = NULL;

            configPlay(e, 0, F);                // voice 0 plays bufA

            // voice 1 records (no playback), fed only by the feedback bus
            e.setLoopStart(1, 0.f);
            e.setLoopEnd(1, endSec);
            e.setRate(1, 1.0f);
            e.setFadeTime(1, 0.001f);
            e.setRecLevel(1, 1.0f);
            e.setPreLevel(1, 0.0f);
            e.cutToPos(1, 0.f);
            e.setRecFlag(1, true);
            e.setPlayFlag(1, false);

            e.setFeedback(0, 1, g);

            runPV(e, bufs, F, (int)F * 3, 0.0, NULL, NULL, NULL, NULL);
            return std::fabs((double)bufB[F / 2]);
        }
    };

    CHECK(Scen::recordedMag(1.0f, F, endSec) > 0.1);   // feedback records signal
    CHECK(Scen::recordedMag(0.0f, F, endSec) < 0.02);  // no feedback -> silent
}

static void test_enable()
{
    // "enable 0 0" gates a voice off (processing skipped -> silence); re-enabling
    // restores output.
    std::printf("test_enable\n");
    Eng e;
    e.setSampleRate(SR);
    const size_t F = 8192;
    std::vector<float> buf(F, 0.5f);
    configPlay(e, 0, F);

    std::vector<double> on1;
    run(e, buf.data(), F, 2000, 0.0, &on1);
    CHECK(std::fabs(midMean(on1)) > 0.3);

    e.setEnabled(0, false);
    std::vector<double> off;
    run(e, buf.data(), F, 2000, 0.0, &off);
    CHECK(std::fabs(midMean(off)) < 0.02);

    e.setEnabled(0, true);
    std::vector<double> on2;
    run(e, buf.data(), F, 2000, 0.0, &on2);
    CHECK(std::fabs(midMean(on2)) > 0.3);
}

static void test_input_matrix()
{
    // Route signal inlet 0 into voice 1's record input via the input matrix and
    // confirm voice 1 records it; with the route at 0, voice 1 stays silent
    // (its own inlet 1 is silent).
    std::printf("test_input_matrix\n");
    const size_t F      = 8192;
    const float  endSec = (float)F / (float)SR;

    struct Scen {
        static double recordedMag(float g, size_t F, float endSec) {
            Eng e;
            e.setSampleRate(SR);
            std::vector<float> bufB(F, 0.0f);
            float *bufs[NV];
            for (int v = 0; v < NV; ++v) bufs[v] = NULL;
            bufs[1] = bufB.data();

            e.setLoopStart(1, 0.f);
            e.setLoopEnd(1, endSec);
            e.setRate(1, 1.0f);
            e.setFadeTime(1, 0.001f);
            e.setRecLevel(1, 1.0f);
            e.setPreLevel(1, 0.0f);
            e.cutToPos(1, 0.f);
            e.setRecFlag(1, true);
            e.setPlayFlag(1, false);

            e.setInLevel(0, 1, g);   // inlet 0 -> voice 1 record

            // runPV feeds `inval` into inlet 0
            runPV(e, bufs, F, (int)F * 3, 0.5, NULL, NULL, NULL, NULL);
            return std::fabs((double)bufB[F / 2]);
        }
    };

    CHECK(Scen::recordedMag(1.0f, F, endSec) > 0.1);
    CHECK(Scen::recordedMag(0.0f, F, endSec) < 0.02);
}

static void test_num_voices()
{
    // Runtime voice count (used by mc.softkut~). setNumVoices clamps to
    // [1, NumVoices], and only active voices are processed.
    std::printf("test_num_voices\n");
    Eng e;
    e.setSampleRate(SR);
    CHECK(e.numVoices() == NV);             // defaults to the template max
    CHECK(e.maxVoices() == NV);

    e.setNumVoices(3);
    CHECK(e.numVoices() == 3);
    e.setNumVoices(0);                      // clamps up to 1
    CHECK(e.numVoices() == 1);
    e.setNumVoices(999);                    // clamps down to max
    CHECK(e.numVoices() == NV);

    // with 2 active voices, voice 0 plays a DC buffer and voice 5 (inactive when
    // count<6) is not processed
    e.setNumVoices(2);
    const size_t F = 8192;
    std::vector<float> buf(F, 0.5f);
    configPlay(e, 0, F);
    std::vector<double> c0;
    run(e, buf.data(), F, 2000, 0.0, &c0);
    CHECK(std::fabs(midMean(c0)) > 0.3);    // active voice still works
}

// karma~-style metadata snapshot: cached loop bounds (ms via *1000 in the
// shell), normalized position, and the synthesized play/rec state code.
static void test_voice_info()
{
    std::printf("test_voice_info\n");
    Eng e;
    e.setSampleRate(SR);
    const size_t F = 8192;
    std::vector<float> buf(F, 0.5f);

    // known loop window: 0.25s .. 0.75s
    e.setLoopStart(0, 0.25f);
    e.setLoopEnd(0, 0.75f);
    e.setRate(0, 1.0f);
    e.setFadeTime(0, 0.001f);
    e.cutToPos(0, 0.25f);
    run(e, buf.data(), F, 64, 0.0, NULL);   // drain commands so bounds settle

    softkut::VoiceInfo vi = e.getVoiceInfo(0);
    CHECK_NEAR(vi.startSec, 0.25, 1e-6);
    CHECK_NEAR(vi.endSec, 0.75, 1e-6);
    CHECK_NEAR(vi.windowSec, 0.5, 1e-6);
    CHECK(vi.play == 0 && vi.rec == 0 && vi.state == 0);    // stopped
    CHECK(vi.position >= 0.f && vi.position <= 1.f);

    // play -> state 1
    e.setPlayFlag(0, true);
    run(e, buf.data(), F, 1024, 0.0, NULL);
    vi = e.getVoiceInfo(0);
    CHECK(vi.play == 1 && vi.rec == 0 && vi.state == 1);
    CHECK(vi.position >= 0.f && vi.position <= 1.f);

    // play + rec -> overdub, state 3
    e.setRecFlag(0, true);
    run(e, buf.data(), F, 256, 0.0, NULL);
    vi = e.getVoiceInfo(0);
    CHECK(vi.play == 1 && vi.rec == 1 && vi.state == 3);

    // rec only -> state 2
    e.setPlayFlag(0, false);
    run(e, buf.data(), F, 256, 0.0, NULL);
    vi = e.getVoiceInfo(0);
    CHECK(vi.play == 0 && vi.rec == 1 && vi.state == 2);

    // reset restores default loop bounds (0..1) once the command drains
    e.reset();
    run(e, buf.data(), F, 64, 0.0, NULL);
    vi = e.getVoiceInfo(0);
    CHECK_NEAR(vi.startSec, 0.0, 1e-6);
    CHECK_NEAR(vi.endSec, 1.0, 1e-6);
    CHECK(vi.play == 0 && vi.rec == 0 && vi.state == 0);
}

// A record-once pass clears the record flag inside the same process() call that
// performs its last writes, so the record flag alone does not tell a host that
// the buffer changed. With a loop window shorter than one block the whole pass
// fits in a single call and the flag is never observed true from outside.
static void test_record_once_wrote_block()
{
    std::printf("test_record_once_wrote_block\n");
    Eng e;
    e.setSampleRate(SR);
    e.setNumVoices(1);
    const size_t F = 8192;
    std::vector<float> buf(F, 0.f);
    float *bufs[NV];
    softkut::BufferView fr[NV];
    for (int v = 0; v < NV; ++v) { bufs[v] = buf.data(); fr[v] = mono(bufs[v], F); }

    e.setLoopStart(0, 0.f);
    e.setLoopEnd(0, 0.002f);          // 96-sample loop, shorter than one block
    e.setRate(0, 1.0f);
    e.setFadeTime(0, 0.0002f);
    e.cutToPos(0, 0.f);
    e.setPlayFlag(0, true);
    e.setRecFlag(0, true);
    e.setRecOnceFlag(0, true);

    const int B = 1024;
    std::vector<std::vector<double> > in(NV, std::vector<double>(B, 0.0));
    std::vector<std::vector<double> > out(NV, std::vector<double>(B, 0.0));
    std::vector<const double *> inp(NV);
    std::vector<double *>       outp(NV);
    for (int v = 0; v < NV; ++v) { inp[v] = in[v].data(); outp[v] = out[v].data(); }
    for (int i = 0; i < B; ++i) in[0][i] = 1.0;

    int recSeen = 0, wroteSeen = 0;
    for (int blk = 0; blk < 8; ++blk) {
        e.process(inp.data(), outp.data(), B, fr, NULL, NULL);
        if (e.getRecFlag(0))     ++recSeen;
        if (e.getWroteBlock(0))  ++wroteSeen;
    }
    int nonzero = 0;
    for (size_t i = 0; i < F; ++i) if (buf[i] != 0.f) ++nonzero;

    CHECK(nonzero > 0);        // the pass did write to the buffer
    CHECK(recSeen == 0);       // ... while the record flag was never seen set
    CHECK(wroteSeen == 1);     // ... and exactly the writing block reports it

    // a block with no recording reports no write
    e.process(inp.data(), outp.data(), B, fr, NULL, NULL);
    CHECK(!e.getWroteBlock(0));
}

// A plain (non-record-once) recording block reports a write; stopping the
// record flag ends the reports.
static void test_wrote_block_plain_record()
{
    std::printf("test_wrote_block_plain_record\n");
    Eng e;
    e.setSampleRate(SR);
    const size_t F = 8192;
    std::vector<float> buf(F, 0.f);
    configPlay(e, 0, F);
    e.setRecFlag(0, true);
    run(e, buf.data(), F, 256, 1.0, NULL);
    CHECK(e.getWroteBlock(0));
    CHECK(e.getRecFlag(0));

    e.setRecFlag(0, false);
    run(e, buf.data(), F, 256, 1.0, NULL);
    CHECK(!e.getWroteBlock(0));

    // a disabled voice never writes, even with the record flag set
    e.setRecFlag(0, true);
    e.setEnabled(0, false);
    run(e, buf.data(), F, 256, 1.0, NULL);
    CHECK(!e.getWroteBlock(0));
}

// Host vectors longer than kMaxBlock are split, not clamped: every output
// sample is written, and the result matches processing the same split by hand.
static void test_block_split()
{
    std::printf("test_block_split\n");
    const int N = Eng::kMaxBlock + 100;
    const size_t F = 8192;
    std::vector<float> buf(F);
    for (size_t i = 0; i < F; ++i) buf[i] = std::sin((float)i * 0.01f);
    float *bufs[NV];
    softkut::BufferView fr[NV];
    for (int v = 0; v < NV; ++v) { bufs[v] = buf.data(); fr[v] = mono(bufs[v], F); }

    const double kSentinel = -12345.0;
    std::vector<std::vector<double> > in(NV, std::vector<double>(N, 0.0));
    std::vector<const double *> inp(NV);
    for (int v = 0; v < NV; ++v) inp[v] = in[v].data();

    // one oversized call
    Eng a;
    a.setSampleRate(SR);
    configPlay(a, 0, F);
    std::vector<std::vector<double> > outA(NV, std::vector<double>(N, kSentinel));
    std::vector<double *> outpA(NV);
    for (int v = 0; v < NV; ++v) outpA[v] = outA[v].data();
    std::vector<double> mixLA(N, kSentinel), mixRA(N, kSentinel);
    a.process(inp.data(), outpA.data(), N, fr, mixLA.data(), mixRA.data());

    int unwritten = 0;
    for (int v = 0; v < NV; ++v)
        for (int i = 0; i < N; ++i) if (outA[v][i] == kSentinel) ++unwritten;
    for (int i = 0; i < N; ++i)
        if (mixLA[i] == kSentinel || mixRA[i] == kSentinel) ++unwritten;
    CHECK(unwritten == 0);

    // the same span processed as kMaxBlock + remainder
    Eng b;
    b.setSampleRate(SR);
    configPlay(b, 0, F);
    std::vector<std::vector<double> > outB(NV, std::vector<double>(N, 0.0));
    std::vector<double *> outpB(NV);
    std::vector<const double *> inpB(NV);
    std::vector<double> mixLB(N, 0.0), mixRB(N, 0.0);
    int off = 0;
    while (off < N) {
        const int n = (N - off < Eng::kMaxBlock) ? (N - off) : Eng::kMaxBlock;
        for (int v = 0; v < NV; ++v) {
            inpB[v]  = in[v].data() + off;
            outpB[v] = outB[v].data() + off;
        }
        b.process(inpB.data(), outpB.data(), n, fr,
                  mixLB.data() + off, mixRB.data() + off);
        off += n;
    }
    int mismatch = 0;
    for (int i = 0; i < N; ++i) {
        if (outA[0][i] != outB[0][i]) ++mismatch;
        if (mixLA[i] != mixLB[i] || mixRA[i] != mixRB[i]) ++mismatch;
    }
    CHECK(mismatch == 0);
    // the tail past kMaxBlock carries real signal, not silence
    double tail = 0.0;
    for (int i = Eng::kMaxBlock; i < N; ++i) tail += std::fabs(outA[0][i]);
    CHECK(tail > 0.0);
}

// Transport state must be readable from a second thread while the audio thread
// completes a record-once pass (which clears softcut's plain-bool record flag).
// Meaningful under -fsanitize=thread; here it also asserts the reports stay in
// range while the DSP side mutates.
static void test_poll_during_record_once()
{
    std::printf("test_poll_during_record_once\n");
    Eng e;
    e.setSampleRate(SR);
    e.setNumVoices(1);
    const size_t F = 8192;
    std::vector<float> buf(F, 0.f);
    float *bufs[NV];
    softkut::BufferView fr[NV];
    for (int v = 0; v < NV; ++v) { bufs[v] = buf.data(); fr[v] = mono(bufs[v], F); }

    const float endSec = (float)F / (float)SR;
    e.setLoopStart(0, 0.f);
    e.setLoopEnd(0, endSec);
    e.setRate(0, 1.0f);
    e.setFadeTime(0, 0.001f);
    e.cutToPos(0, 0.f);
    e.setPlayFlag(0, true);
    e.setRecFlag(0, true);
    e.setRecOnceFlag(0, true);

    std::atomic<bool> done(false);
    std::atomic<int>  bad(0);
    std::atomic<int>  polls(0);
    std::thread poller([&]() {
        while (!done.load(std::memory_order_relaxed)) {
            softkut::VoiceInfo vi = e.getVoiceInfo(0);
            if (vi.state < 0 || vi.state > 3) bad.fetch_add(1);
            if (vi.position < 0.f || vi.position > 1.f) bad.fetch_add(1);
            if (vi.rec != ((vi.state >> 1) & 1) || vi.play != (vi.state & 1)) bad.fetch_add(1);
            polls.fetch_add(1);
        }
    });

    const int B = 64;
    std::vector<std::vector<double> > in(NV, std::vector<double>(B, 0.0));
    std::vector<std::vector<double> > out(NV, std::vector<double>(B, 0.0));
    std::vector<const double *> inp(NV);
    std::vector<double *>       outp(NV);
    for (int v = 0; v < NV; ++v) { inp[v] = in[v].data(); outp[v] = out[v].data(); }
    for (int i = 0; i < B; ++i) in[0][i] = 1.0;

    bool finished = false;
    for (int blk = 0; blk < 1000 && !finished; ++blk) {
        e.process(inp.data(), outp.data(), B, fr, NULL, NULL);
        if (!e.getRecFlag(0)) finished = true;
    }
    done.store(true, std::memory_order_relaxed);
    poller.join();

    CHECK(finished);              // the record-once pass ended on its own
    CHECK(polls.load() > 0);
    CHECK(bad.load() == 0);
}

// sanitize() clamps each unsafe parameter into its range and rejects non-finite
// values.
static void test_sanitize()
{
    std::printf("test_sanitize\n");
    using softkut::CmdId;
    using softkut::Check;
    using softkut::sanitize;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    float v;

    v = nan;   CHECK(sanitize(CmdId::Rate, v, SR) == Check::Rejected);
    v = inf;   CHECK(sanitize(CmdId::Level, v, SR) == Check::Rejected);
    v = -inf;  CHECK(sanitize(CmdId::FbLevel, v, SR) == Check::Rejected);

    v = 1.5f;  CHECK(sanitize(CmdId::Rate, v, SR) == Check::Ok);    CHECK(v == 1.5f);
    v = 100.f; CHECK(sanitize(CmdId::Rate, v, SR) == Check::Clamped); CHECK(v == 64.f);
    v = -1e9f; CHECK(sanitize(CmdId::Rate, v, SR) == Check::Clamped); CHECK(v == -64.f);
    v = -1.f;  sanitize(CmdId::PostFilterRq, v, SR); CHECK(v == softkut::kMinRq);
    v = 0.f;   sanitize(CmdId::PreFilterRq, v, SR);  CHECK(v == softkut::kMinRq);
    v = 1.5f;  sanitize(CmdId::PreLevel, v, SR);     CHECK(v == 1.f);
    v = -0.5f; sanitize(CmdId::PreLevel, v, SR);     CHECK(v == 0.f);
    v = 1e6f;  sanitize(CmdId::RecLevel, v, SR);     CHECK(v == 1.f);
    v = -1.f;  sanitize(CmdId::LoopStart, v, SR);    CHECK(v == 0.f);
    v = -1.f;  sanitize(CmdId::RecPreSlewTime, v, SR); CHECK(v == 0.f);
    v = -1.f;  sanitize(CmdId::LevelSlewTime, v, SR);  CHECK(v == 0.f);
    v = 1e30f; sanitize(CmdId::Position, v, SR);
    CHECK((double)v * SR <= softkut::kMaxFrames);
    v = -1e30f; sanitize(CmdId::RecOffset, v, SR);
    CHECK((double)v * SR >= -softkut::kMaxFrames);
    // gains are unbounded: only finiteness is required
    v = 5.f;   CHECK(sanitize(CmdId::FbLevel, v, SR) == Check::Ok);
    v = -0.1f; CHECK(sanitize(CmdId::RecOffset, v, SR) == Check::Ok);
}

// Every parameter, at extreme values, must leave the buffer and the output
// finite and bounded while the voice records over itself with feedback.
// Before validation, rq < 0 or a negative slew wrote NaN into the buffer and
// |rate| >= 100 crashed the resampler.
static void test_extreme_values_stay_finite()
{
    std::printf("test_extreme_values_stay_finite\n");
    const float vals[] = {-1e30f, -1e6f, -100.f, -1.f, 0.f, 1.5f, 100.f, 1e6f, 1e30f,
                          std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity()};
    const int nIds = (int)softkut::CmdId::PhaseOffset + 1;
    const size_t F = 4096;
    int bad = 0;
    for (int id = 0; id < nIds; ++id) {
        for (float val : vals) {
            Eng e;
            e.setSampleRate(SR);
            e.setNumVoices(1);
            std::vector<float> buf(F);
            for (size_t i = 0; i < F; ++i) buf[i] = 0.5f * std::sin((float)i * 0.05f);
            e.setLoopStart(0, 0.f);
            e.setLoopEnd(0, (float)F / (float)SR);
            e.setPreLevel(0, 0.5f);
            e.setRecFlag(0, true);
            e.setPlayFlag(0, true);
            e.setFeedback(0, 0, 1.f);
            e.cutToPos(0, 0.f);
            e.push(softkut::Command{(softkut::CmdId)id, 0, 0, val});

            std::vector<double> cap;
            run(e, buf.data(), F, (int)F * 8, 0.3, &cap);
            bool ok = true;
            for (float s : buf) if (!std::isfinite(s) || std::fabs(s) > 1e3f) ok = false;
            // output may carry a large but finite user gain (level, filter mix)
            for (double s : cap) if (!std::isfinite(s)) ok = false;
            if (!ok) {
                ++bad;
                std::printf("  cmd %d value %g: non-finite or runaway buffer\n", id, (double)val);
            }
        }
    }
    CHECK(bad == 0);
}

// Play a [0, loopSec] loop and return the furthest saved position (seconds)
// reached over `seconds` of audio at the engine's current rate.
static double maxLoopPosition(Eng &e, std::vector<float> &buf, double sr, double seconds)
{
    double maxPos = 0.0;
    const int total = (int)(sr * seconds), B = 64;
    for (int done = 0; done < total; done += B) {
        run(e, buf.data(), buf.size(), B, 0.0, NULL);
        const double p = e.getSavedPosition(0);
        if (p > maxPos) maxPos = p;
    }
    return maxPos;
}

// softcut stores loop points as frames at the rate current when they were set.
// A later sample-rate change must not rescale the loop in seconds.
static void test_samplerate_change_keeps_loop()
{
    std::printf("test_samplerate_change_keeps_loop\n");
    const double rates[] = {96000.0, 44100.0};
    for (double sr2 : rates) {
        Eng e;
        e.setSampleRate(SR);
        e.setNumVoices(1);
        std::vector<float> buf(1 << 18, 0.f);
        e.setLoopStart(0, 0.f);
        e.setLoopEnd(0, 0.25f);
        e.setFadeTime(0, 0.001f);
        e.cutToPos(0, 0.f);
        e.setPlayFlag(0, true);
        run(e, buf.data(), buf.size(), 64, 0.0, NULL);   // apply at 48 kHz

        e.setSampleRate(sr2);
        e.cutToPos(0, 0.f);
        CHECK_NEAR(maxLoopPosition(e, buf, sr2, 1.0), 0.25, 0.005);
    }

    // the constructor's default 1 s loop must also be 1 s at a non-default rate
    Eng d;
    d.setSampleRate(44100.0);
    d.setNumVoices(1);
    std::vector<float> buf(1 << 18, 0.f);
    d.setFadeTime(0, 0.001f);
    d.cutToPos(0, 0.f);
    d.setPlayFlag(0, true);
    CHECK_NEAR(maxLoopPosition(d, buf, 44100.0, 2.5), 1.0, 0.005);
}

// Max delivers messages on the main and the scheduler thread at once
// (Overdrive). Two producers pushing concurrently must lose no command.
static void test_concurrent_producers()
{
    std::printf("test_concurrent_producers\n");
    Eng e;
    e.setSampleRate(SR);
    e.setNumVoices(2);
    const int K = 200000;

    std::atomic<bool> stop(false);
    std::thread consumer([&]() {
        const int B = 64;
        std::vector<std::vector<double> > in(NV, std::vector<double>(B, 0.0));
        std::vector<std::vector<double> > out(NV, std::vector<double>(B, 0.0));
        std::vector<const double *> inp(NV);
        std::vector<double *>       outp(NV);
        for (int v = 0; v < NV; ++v) { inp[v] = in[v].data(); outp[v] = out[v].data(); }
        while (!stop.load(std::memory_order_relaxed))
            e.process(inp.data(), outp.data(), B, NULL, NULL, NULL);
    });
    auto producer = [&](int voice) {
        for (int i = 0; i < K; ++i)
            while (!e.setRate(voice, 1.f + (float)(i % 7) * 0.1f)) std::this_thread::yield();
    };
    std::thread p0(producer, 0), p1(producer, 1);
    p0.join();
    p1.join();

    // wait (bounded) for the consumer to drain what was queued
    for (int i = 0; i < 100000 && e.pending() > 0; ++i) std::this_thread::yield();
    stop.store(true, std::memory_order_relaxed);
    consumer.join();

    CHECK(e.pending() == 0);
    CHECK(e.handled() == (uint64_t)(2 * K));
}

// Furthest and nearest saved positions (seconds) over `seconds` of audio,
// ignoring the first `settle` seconds.
static void positionRange(Eng &e, std::vector<float> &buf, double seconds, double settle,
                          double *lo, double *hi)
{
    *lo = 1e9; *hi = -1e9;
    const int total = (int)(SR * seconds), skip = (int)(SR * settle), B = 64;
    for (int done = 0; done < total; done += B) {
        run(e, buf.data(), buf.size(), B, 0.0, NULL);
        if (done < skip) continue;
        const double p = e.getSavedPosition(0);
        if (p < *lo) *lo = p;
        if (p > *hi) *hi = p;
    }
}

// Starting a voice with the play or rec flag alone, without a `position` cut,
// must respect the loop. softcut's run() left the head without its active
// flag, so it ran through the whole buffer and wrapped only at its end.
static void test_play_without_position_loops()
{
    std::printf("test_play_without_position_loops\n");
    double lo, hi;
    {   // loop at the buffer start, play flag only
        Eng e; e.setSampleRate(SR); e.setNumVoices(1);
        std::vector<float> buf(1 << 18, 0.f);
        e.setLoopStart(0, 0.f); e.setLoopEnd(0, 0.25f); e.setFadeTime(0, 0.001f);
        e.setPlayFlag(0, true);
        positionRange(e, buf, 1.0, 0.0, &lo, &hi);
        CHECK_NEAR(hi, 0.25, 0.005);
    }
    {   // loop away from the head's initial phase: the head jumps into it
        Eng e; e.setSampleRate(SR); e.setNumVoices(1);
        std::vector<float> buf(1 << 18, 0.f);
        e.setLoopStart(0, 0.5f); e.setLoopEnd(0, 0.75f); e.setFadeTime(0, 0.001f);
        e.setPlayFlag(0, true);
        positionRange(e, buf, 1.0, 0.02, &lo, &hi);
        CHECK(lo >= 0.5 - 0.005);
        CHECK(hi <= 0.75 + 0.005);
    }
    {   // rec flag only
        Eng e; e.setSampleRate(SR); e.setNumVoices(1);
        std::vector<float> buf(1 << 18, 0.f);
        e.setLoopStart(0, 0.f); e.setLoopEnd(0, 0.25f); e.setFadeTime(0, 0.001f);
        e.setRecFlag(0, true);
        positionRange(e, buf, 1.0, 0.0, &lo, &hi);
        CHECK_NEAR(hi, 0.25, 0.005);
    }
}

// A null input pointer is silence: mc.softkut~ passes null for voices past
// the connected channel count instead of a buffer that dsp64 reallocates.
static void test_null_input_is_silence()
{
    std::printf("test_null_input_is_silence\n");
    Eng e;
    e.setSampleRate(SR);
    e.setNumVoices(2);
    const size_t F = 8192;
    std::vector<float> buf(F, 0.5f);
    configPlay(e, 1, F);
    e.setRecFlag(1, true);            // overwrite (pre 0) with the null input

    const int B = 64;
    std::vector<double> in0(B, 1.0), out0(B), out1(B);
    const double *ins[NV]  = {in0.data(), NULL};
    double       *outs[NV] = {out0.data(), out1.data()};
    float        *bufs[NV] = {NULL, buf.data()};   // voice 0 has no buffer
    softkut::BufferView fr[NV] = {mono(bufs[0], 0), mono(bufs[1], F)};
    for (int done = 0; done < (int)F * 2; done += B)
        e.process(ins, outs, B, fr, NULL, NULL);

    double maxAbs = 0.0;
    for (size_t i = F / 4; i < (F * 3) / 4; ++i)
        maxAbs = std::max(maxAbs, (double)std::fabs(buf[i]));
    CHECK(maxAbs < 1e-3);             // recorded silence over the old content
}

// The host may set the sample rate while the audio thread processes (Max
// recompiles a running chain). The request is applied by process(), so the
// engine stays consistent. Meaningful under -fsanitize=thread.
static void test_samplerate_set_during_process()
{
    std::printf("test_samplerate_set_during_process\n");
    Eng e;
    e.setSampleRate(SR);
    e.setNumVoices(1);
    const size_t F = 8192;
    std::vector<float> buf(F);
    for (size_t i = 0; i < F; ++i) buf[i] = std::sin((float)i * 0.01f);
    configPlay(e, 0, F);

    std::atomic<bool> done(false);
    std::thread host([&]() {
        for (int i = 0; !done.load(std::memory_order_relaxed); ++i)
            e.setSampleRate(i & 1 ? 96000.0 : 48000.0);
    });
    std::vector<double> cap;
    run(e, buf.data(), F, (int)F * 16, 0.0, &cap);
    done.store(true, std::memory_order_relaxed);
    host.join();

    int bad = 0;
    for (double s : cap) if (!std::isfinite(s) || std::fabs(s) > 2.0) ++bad;
    CHECK(bad == 0);
    e.setSampleRate(44100.0);
    CHECK(e.getSampleRate() == 44100.0);   // control side sees the request at once
}

// Process `total` samples on voices 0..nv-1 with explicit views, feeding
// `inval` to every voice. Captures voice 0 and voice 1 when asked.
static void runViews(Eng &e, const softkut::BufferView *views, int total, double inval,
                     std::vector<double> *cap0, std::vector<double> *cap1)
{
    const int B = 64;
    std::vector<std::vector<double> > in(NV, std::vector<double>(B, inval));
    std::vector<std::vector<double> > out(NV, std::vector<double>(B, 0.0));
    std::vector<const double *> inp(NV);
    std::vector<double *>       outp(NV);
    for (int v = 0; v < NV; ++v) { inp[v] = in[v].data(); outp[v] = out[v].data(); }
    for (int done = 0; done < total; done += B) {
        e.process(inp.data(), outp.data(), B, views, NULL, NULL);
        if (cap0) cap0->insert(cap0->end(), out[0].begin(), out[0].end());
        if (cap1) cap1->insert(cap1->end(), out[1].begin(), out[1].end());
    }
}

static const float kGuard = 12345.f;   // sentinel past a view's last frame

// A buffer of any length is used whole: recording over a full pass reaches its
// last frame, and nothing past it is touched.
static void test_any_length_buffer()
{
    std::printf("test_any_length_buffer\n");
    const size_t F = 10000, G = 256;          // not a power of two
    std::vector<float> store(F + G, 0.f);
    for (size_t i = F; i < F + G; ++i) store[i] = kGuard;

    Eng e; e.setSampleRate(SR); e.setNumVoices(1);
    softkut::BufferView views[NV] = {mono(store.data(), F)};
    e.setLoopStart(0, 0.f);
    e.setLoopEnd(0, (float)F / (float)SR);
    e.setFadeTime(0, 0.001f);
    e.cutToPos(0, 0.f);
    e.setRecFlag(0, true);
    e.setPlayFlag(0, true);
    runViews(e, views, (int)F * 3, 0.5, NULL, NULL);

    int written = 0;
    for (size_t i = 8192; i < F - 64; ++i) if (store[i] != 0.f) ++written;
    CHECK(written == (int)(F - 64 - 8192));   // frames past the old pow2 prefix
    int guard = 0;
    for (size_t i = F; i < F + G; ++i) if (store[i] != kGuard) ++guard;
    CHECK(guard == 0);
}

// Loop points past the end of the buffer wrap modulo its length rather than
// reading or writing out of bounds.
static void test_loop_past_buffer_end()
{
    std::printf("test_loop_past_buffer_end\n");
    const size_t F = 3000, G = 256;
    std::vector<float> store(F + G, 0.f);
    for (size_t i = F; i < F + G; ++i) store[i] = kGuard;

    Eng e; e.setSampleRate(SR); e.setNumVoices(1);
    softkut::BufferView views[NV] = {mono(store.data(), F)};
    e.setLoopStart(0, 0.f);
    e.setLoopEnd(0, 1.0f);                     // 48000 frames over a 3000-frame buffer
    e.setRecFlag(0, true);
    e.setPlayFlag(0, true);
    e.cutToPos(0, 0.5f);
    std::vector<double> cap;
    runViews(e, views, 48000, 0.25, &cap, NULL);

    int guard = 0;
    for (size_t i = F; i < F + G; ++i) if (store[i] != kGuard) ++guard;
    CHECK(guard == 0);
    int bad = 0;
    for (double s : cap) if (!std::isfinite(s) || std::fabs(s) > 2.0) ++bad;
    CHECK(bad == 0);
}

// An interleaved store is read and written one channel at a time.
static void test_interleaved_channels()
{
    std::printf("test_interleaved_channels\n");
    const size_t F = 4096;
    const unsigned C = 2;
    std::vector<float> store(F * C);
    for (size_t i = 0; i < F; ++i) { store[i * C] = 0.5f; store[i * C + 1] = -0.5f; }

    Eng e; e.setSampleRate(SR); e.setNumVoices(2);
    softkut::BufferView views[NV] = {
        softkut::BufferView{store.data(),     F, C, 0.0},
        softkut::BufferView{store.data() + 1, F, C, 0.0}};
    configPlay(e, 0, F);
    configPlay(e, 1, F);
    std::vector<double> cap0, cap1;
    runViews(e, views, (int)F, 0.0, &cap0, &cap1);
    CHECK(midMean(cap0) > 0.4);                 // voice 0 reads channel 1
    CHECK(midMean(cap1) < -0.4);                // voice 1 reads channel 2

    // recording on voice 0 writes channel 1 only
    e.setRecFlag(0, true);
    runViews(e, views, (int)F * 2, 0.0, NULL, NULL);
    int ch1Changed = 0, ch2Changed = 0;
    for (size_t i = F / 4; i < (F * 3) / 4; ++i) {
        if (store[i * C] != 0.5f) ++ch1Changed;
        if (store[i * C + 1] != -0.5f) ++ch2Changed;
    }
    CHECK(ch1Changed > 0);
    CHECK(ch2Changed == 0);
}

// A buffer that shrinks between blocks while recording must not be written
// past its new end (the write index is re-wrapped).
static void test_buffer_shrinks_while_recording()
{
    std::printf("test_buffer_shrinks_while_recording\n");
    const size_t F = 10000, S = 3000;
    std::vector<float> store(F, 0.f);

    Eng e; e.setSampleRate(SR); e.setNumVoices(1);
    softkut::BufferView views[NV] = {mono(store.data(), F)};
    e.setLoopStart(0, 0.f);
    e.setLoopEnd(0, (float)F / (float)SR);
    e.cutToPos(0, 0.f);
    e.setRecFlag(0, true);
    e.setPlayFlag(0, true);
    runViews(e, views, 8000, 0.5, NULL, NULL);   // write index near frame 8000

    for (size_t i = S; i < F; ++i) store[i] = kGuard;
    views[0].frames = S;
    runViews(e, views, 4000, 0.5, NULL, NULL);
    int guard = 0;
    for (size_t i = S; i < F; ++i) if (store[i] != kGuard) ++guard;
    CHECK(guard == 0);
}

// A store at another sample rate plays at its own speed (as groove~ does), and
// loop times are seconds of store material.
static void test_buffer_sample_rate()
{
    std::printf("test_buffer_sample_rate\n");
    const double BR = 24000.0;                 // store rate; DSP runs at SR
    const size_t F = 24000;                    // 1 s of store material
    const size_t edge = 6000 + 200;            // 0.25 s loop end + fade margin
    std::vector<float> store(F, 0.f);
    for (size_t i = edge; i < F; ++i) store[i] = 1.f;   // must never be heard

    const double dspRates[] = {SR, 96000.0};
    for (double dsp : dspRates) {
        Eng e; e.setSampleRate(dsp); e.setNumVoices(1);
        softkut::BufferView views[NV] = {softkut::BufferView{store.data(), F, 1, BR}};
        e.setLoopStart(0, 0.f);
        e.setLoopEnd(0, 0.25f);
        e.setFadeTime(0, 0.001f);
        e.cutToPos(0, 0.f);
        e.setPlayFlag(0, true);

        // natural speed: 0.125 s of real time advances 0.125 s of store
        const int half = (int)(dsp * 0.125);
        std::vector<double> cap;
        runViews(e, views, half - half % 64, 0.0, &cap, NULL);
        CHECK_NEAR(e.getSavedPosition(0), 0.125, 0.005);

        // one second: the loop wraps at 0.25 store seconds, never past it
        runViews(e, views, (int)dsp, 0.0, &cap, NULL);
        double peak = 0.0;
        for (double s : cap) peak = std::max(peak, std::fabs(s));
        CHECK(peak < 0.01);
        softkut::VoiceInfo vi = e.getVoiceInfo(0);
        CHECK(vi.position >= 0.f && vi.position <= 1.f);
    }

    // rate is clamped after scaling: 64 x (96k / 48k) must not overrun softcut
    Eng f; f.setSampleRate(SR); f.setNumVoices(1);
    std::vector<float> s96(96000, 0.25f);
    softkut::BufferView v96[NV] = {softkut::BufferView{s96.data(), s96.size(), 1, 96000.0}};
    f.setLoopStart(0, 0.f); f.setLoopEnd(0, 1.f);
    f.setRate(0, 64.f);
    f.cutToPos(0, 0.f);
    f.setPlayFlag(0, true);
    std::vector<double> out;
    runViews(f, v96, 4800, 0.0, &out, NULL);
    int bad = 0;
    for (double s : out) if (!std::isfinite(s)) ++bad;
    CHECK(bad == 0);
}

// sync lands the follower on the lead's position in store seconds, even when
// their stores run at different rates.
static void test_sync_across_store_rates()
{
    std::printf("test_sync_across_store_rates\n");
    std::vector<float> a(24000, 0.f), b(48000, 0.f);
    Eng e; e.setSampleRate(SR); e.setNumVoices(2);
    softkut::BufferView views[NV] = {
        softkut::BufferView{a.data(), a.size(), 1, 24000.0},
        softkut::BufferView{b.data(), b.size(), 1, 0.0}};
    for (int v = 0; v < 2; ++v) {
        e.setLoopStart(v, 0.f); e.setLoopEnd(v, 0.9f); e.setFadeTime(v, 0.001f);
        e.setPlayFlag(v, true);
    }
    e.cutToPos(0, 0.3f);
    e.cutToPos(1, 0.f);
    runViews(e, views, 64 * 10, 0.0, NULL, NULL);
    e.syncVoice(1, 0, 0.f);
    runViews(e, views, 64, 0.0, NULL, NULL);
    CHECK_NEAR(e.getSavedPosition(1), e.getSavedPosition(0), 0.005);
}

// reset stops every voice and restores every default, so the next play starts
// from the beginning of the default loop.
static void test_reset_restarts()
{
    std::printf("test_reset_restarts\n");
    Eng e; e.setSampleRate(SR); e.setNumVoices(2);
    const size_t F = 1 << 17;
    std::vector<float> buf(F);
    for (size_t i = 0; i < F; ++i) buf[i] = 0.5f * std::sin((float)i * 0.01f);

    // a playing, overdubbing, fed-back voice with non-default settings
    e.setLoopStart(0, 0.5f); e.setLoopEnd(0, 1.5f); e.setRate(0, 2.f);
    e.setLevel(0, 0.25f); e.setFeedback(1, 0, 1.f); e.setPreLevel(0, 1.f);
    e.cutToPos(0, 1.f); e.setPlayFlag(0, true); e.setRecFlag(0, true);
    e.setPlayFlag(1, true); e.cutToPos(1, 0.f);
    run(e, buf.data(), F, 4800, 0.0, NULL);
    CHECK(e.getSavedPosition(0) > 0.5);

    e.reset();
    std::vector<double> cap;
    run(e, buf.data(), F, 4800, 0.0, &cap);
    double peak = 0.0;
    for (size_t i = 64; i < cap.size(); ++i) peak = std::max(peak, std::fabs(cap[i]));
    CHECK(peak < 1e-6);                        // every voice stopped
    CHECK_NEAR(e.getSavedPosition(0), 0.0, 1e-9);
    softkut::VoiceInfo vi = e.getVoiceInfo(0);
    CHECK(vi.state == 0);
    CHECK_NEAR(vi.startSec, 0.0, 1e-6);
    CHECK_NEAR(vi.endSec, 1.0, 1e-6);

    // play alone restarts at the default loop start, at rate 1, unity level
    e.setPlayFlag(0, true);
    double lo, hi;
    positionRange(e, buf, 1.5, 0.0, &lo, &hi);
    CHECK(lo < 0.01);
    CHECK_NEAR(hi, 1.0, 0.005);
    cap.clear();
    run(e, buf.data(), F, 4800, 0.0, &cap);
    double outPeak = 0.0;
    for (double s : cap) outPeak = std::max(outPeak, std::fabs(s));
    CHECK(outPeak > 0.3);                      // level back to 1, not 0.25
}

// ---------------------------------------------------------------------------
int main()
{
    test_spsc_queue();
    test_command_drain();
    test_record_playback();
    test_idle_voice_silent();
    test_phase_advance();
    test_output_level();
    test_pan();
    test_per_voice_buffers();
    test_stop();
    test_feedback();
    test_enable();
    test_input_matrix();
    test_num_voices();
    test_voice_info();
    test_record_once_wrote_block();
    test_wrote_block_plain_record();
    test_block_split();
    test_poll_during_record_once();
    test_sanitize();
    test_extreme_values_stay_finite();
    test_samplerate_change_keeps_loop();
    test_concurrent_producers();
    test_play_without_position_loops();
    test_null_input_is_silence();
    test_samplerate_set_during_process();
    test_any_length_buffer();
    test_loop_past_buffer_end();
    test_interleaved_channels();
    test_buffer_shrinks_while_recording();
    test_buffer_sample_rate();
    test_sync_across_store_rates();
    test_reset_restarts();

    std::printf("\n%d checks, %d failures\n", g_total, g_fail);
    return g_fail == 0 ? 0 : 1;
}
