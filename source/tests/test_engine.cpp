// Offline test harness for softkut::Engine.
//
// Exercises the host-agnostic engine without Max: the SPSC command queue, the
// power-of-two buffer framing, command routing through the queue into softcut,
// a record-then-playback round trip, idle-voice silence, and phase advance.
//
// Self-contained: no external test framework. Exit code 0 = all pass.

#include <atomic>
#include <cmath>
#include <cstdio>
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
    size_t fr[NV];
    for (int v = 0; v < NV; ++v) fr[v] = frames;

    int done = 0;
    while (done < total) {
        int n = (total - done < B) ? (total - done) : B;
        for (int i = 0; i < n; ++i) in[0][i] = inval;
        e.process(inp.data(), outp.data(), n, bufs, fr, mL.data(), mR.data());
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
static void test_usable_frames()
{
    std::printf("test_usable_frames\n");
    CHECK(Eng::usableFrames(1024) == 1024);
    CHECK(Eng::usableFrames(1000) == 512);
    CHECK(Eng::usableFrames(5000) == 4096);
    CHECK(Eng::usableFrames(3)    == 2);
    CHECK(Eng::usableFrames(1)    == 1);
    CHECK(Eng::usableFrames(0)    == 0);
}

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
    size_t fr[NV];
    for (int v = 0; v < NV; ++v) { bufs[v] = buf.data(); fr[v] = F; }

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
        e.process(inp.data(), outp.data(), B, bufs, fr, NULL, NULL);
        if (e.getRecFlag(0))     ++recSeen;
        if (e.getWroteBlock(0))  ++wroteSeen;
    }
    int nonzero = 0;
    for (size_t i = 0; i < F; ++i) if (buf[i] != 0.f) ++nonzero;

    CHECK(nonzero > 0);        // the pass did write to the buffer
    CHECK(recSeen == 0);       // ... while the record flag was never seen set
    CHECK(wroteSeen == 1);     // ... and exactly the writing block reports it

    // a block with no recording reports no write
    e.process(inp.data(), outp.data(), B, bufs, fr, NULL, NULL);
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
    size_t fr[NV];
    for (int v = 0; v < NV; ++v) { bufs[v] = buf.data(); fr[v] = F; }

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
    a.process(inp.data(), outpA.data(), N, bufs, fr, mixLA.data(), mixRA.data());

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
        b.process(inpB.data(), outpB.data(), n, bufs, fr,
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
    size_t fr[NV];
    for (int v = 0; v < NV; ++v) { bufs[v] = buf.data(); fr[v] = F; }

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
        e.process(inp.data(), outp.data(), B, bufs, fr, NULL, NULL);
        if (!e.getRecFlag(0)) finished = true;
    }
    done.store(true, std::memory_order_relaxed);
    poller.join();

    CHECK(finished);              // the record-once pass ended on its own
    CHECK(polls.load() > 0);
    CHECK(bad.load() == 0);
}

// ---------------------------------------------------------------------------
int main()
{
    test_usable_frames();
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

    std::printf("\n%d checks, %d failures\n", g_total, g_fail);
    return g_fail == 0 ? 0 : 1;
}
