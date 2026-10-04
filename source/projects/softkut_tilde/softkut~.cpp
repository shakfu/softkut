// softkut~ : a Max/MSP external wrapping monome's softcut-lib.
//
// Thin Max shell over softkut::Engine (../../include/softkut_engine.h), which
// owns the softcut voices, the lock-free command queue, the double<->float
// conversion, and the per-voice output level. This file owns only Max plumbing: inlets/outlets, the per-voice
// buffer~ references, message parsing, the phase-report clock, and the perform
// call.
//
// Topology: a runtime voice count set by the second creation argument
// (channels, default 1, max NumVoices=6). One signal inlet (record input) and
// one signal outlet (playback) per voice, then one sync outlet per voice (the
// play head in ms of buffer material), then a message outlet for reports.
// (No stereo mix outlet -- pan downstream if needed.)
//
// Buffer model (zero-copy): each voice reads and writes one channel of the
// locked samples of a buffer~ of any length, channel count and sample rate
// (shared name by default; "voicebuf <v> <name> [<chan>]" overrides one voice).
// See softkut_control.h for channel selection.

#include "ext.h"          // standard Max include, always required
#include "ext_obex.h"     // required for new style Max object
#include "ext_buffer.h"   // buffer~ reference + lock/unlock
#include "z_dsp.h"        // MSP

#include "softkut_control.h"   // shared command table + dispatch (pulls in engine)

// ---------------------------------------------------------------------------
static const int NumVoices = 6;             // compile-time maximum (array sizing)
typedef softkut::Engine<NumVoices> t_engine;

// Outlet order (Max creates outlets right-to-left, so first created = rightmost):
// the message/report outlet is created FIRST (rightmost, the conventional data-
// outlet position), then the 2 * nvoices signal outlets. The message outlet is
// not part of perform's outs[] array: outs[v] is voice v's audio and
// outs[nvoices + v] its sync signal.

typedef struct _softkut {
    t_pxobject     ob;          // MSP object header (must be first)
    t_engine      *engine;      // host-agnostic DSP engine (heap: needs C++ ctor)
    long           nvoices;     // active voice count (1..NumVoices, creation arg)

    t_buffer_ref  *vbuf[NumVoices];     // per-voice buffer~ reference
    t_symbol      *vbufname[NumVoices]; // per-voice buffer~ name
    long           vchan[NumVoices];   // channel per voice, -1 = voice mod channels
    t_bool         chanWarned;        // throttle the missing-channel warning

    void          *reportout;   // message outlet for phase / position reports
    void          *tclock;      // phase-report clock
    long           report;      // report interval in ms (0 = off)
} t_softkut;

static t_class  *softkut_class = NULL;

// The control surface (command table + dispatch) lives in softkut_control.h,
// shared with mc.softkut~. The thunks below forward to it.

// ---------------------------------------------------------------------------
// control messages -> shared dispatch (softkut_control.h)
// ---------------------------------------------------------------------------
void softkut_cmd(t_softkut *x, t_symbol *s, long argc, t_atom *argv)
{ softkut::dispatchCmd(x->engine, (t_object *)x, s, argc, argv); }

void softkut_sync(t_softkut *x, t_symbol *s, long argc, t_atom *argv)
{ softkut::dispatchSync(x->engine, (t_object *)x, argc, argv); }

void softkut_stop(t_softkut *x, t_symbol *s, long argc, t_atom *argv)
{ softkut::dispatchStop(x->engine, (t_object *)x, argc, argv); }

void softkut_enable(t_softkut *x, t_symbol *s, long argc, t_atom *argv)
{ softkut::dispatchEnable(x->engine, (t_object *)x, s, argc, argv); }

void softkut_feedback(t_softkut *x, t_symbol *s, long argc, t_atom *argv)
{ softkut::dispatchFeedback(x->engine, (t_object *)x, argc, argv); }

void softkut_inlevel(t_softkut *x, t_symbol *s, long argc, t_atom *argv)
{ softkut::dispatchInlevel(x->engine, (t_object *)x, argc, argv); }

void softkut_reset(t_softkut *x)
{
    if (!x->engine->reset())
        object_warn((t_object *)x, "reset: command queue full, dropped");
}

// `poll`: one info list per voice (see softkut::reportInfo)
void softkut_poll(t_softkut *x)
{ softkut::reportInfo(x); }

// ---------------------------------------------------------------------------
// buffer~ association
// ---------------------------------------------------------------------------
// "set <name>": point every voice at the named (shared) buffer~.
void softkut_set(t_softkut *x, t_symbol *s, long argc, t_atom *argv)
{ softkut::setBuffer(x, argc, argv); }

// "voicebuf <v> <name>": override one voice's buffer~ (e.g. for stereo).
void softkut_voicebuf(t_softkut *x, t_symbol *s, long argc, t_atom *argv)
{ softkut::setVoiceBuffer(x, argc, argv); }

// ---------------------------------------------------------------------------
// perform
// ---------------------------------------------------------------------------
void softkut_perform64(t_softkut *x, t_object *dsp64, double **ins, long nins,
                       double **outs, long nouts, long vec, long flags, void *usr)
{
    const int nv = (int)x->nvoices;
    double *voiceOuts[NumVoices], *syncOuts[NumVoices];
    for (int v = 0; v < nv; ++v) { voiceOuts[v] = outs[v]; syncOuts[v] = outs[nv + v]; }
    double *mixL = NULL, *mixR = NULL;   // no stereo-mix outlets (pan downstream)

    softkut::BufferView            views[NumVoices];
    softkut::BufferLocks<NumVoices> locks;
    softkut::lockBuffers(x, nv, views, locks);
    x->engine->process(ins, voiceOuts, (int)vec, views, mixL, mixR, syncOuts);
    softkut::releaseBuffers(x, nv, views, locks);
    softkut::syncToMs(syncOuts, nv, vec);
}

// ---------------------------------------------------------------------------
// phase-report clock
// ---------------------------------------------------------------------------
void softkut_clock(t_softkut *x)
{
    if (x->report <= 0) return;
    softkut::reportPhase(x);
    if (sys_getdspobjdspstate((t_object *)x))   // this patcher's audio, not global
        clock_delay(x->tclock, x->report);
}

// ---------------------------------------------------------------------------
// dsp / lifecycle
// ---------------------------------------------------------------------------
void softkut_dsp64(t_softkut *x, t_object *dsp64, short *count, double srate,
                   long maxvectorsize, long flags)
{
    x->engine->setSampleRate(srate);

    for (int v = 0; v < x->nvoices; ++v) softkut::ensureVbuf(x, v);


    object_method(dsp64, gensym("dsp_add64"), x, (method)softkut_perform64, 0, NULL);

    if (x->report > 0)
        clock_delay(x->tclock, x->report);
}

void softkut_buf_dblclick(t_softkut *x)
{
    for (int v = 0; v < NumVoices; ++v) {
        t_buffer_obj *b = x->vbuf[v] ? buffer_ref_getobject(x->vbuf[v]) : NULL;
        if (b) { buffer_view(b); return; }
    }
}

t_max_err softkut_notify(t_softkut *x, t_symbol *s, t_symbol *msg, void *sender, void *data)
{
    for (int v = 0; v < NumVoices; ++v)
        if (x->vbuf[v]) buffer_ref_notify(x->vbuf[v], s, msg, sender, data);
    return MAX_ERR_NONE;
}

void softkut_assist(t_softkut *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) {
        snprintf_zero(s, 256, (a == 0) ? "(signal) Voice 1 record input / messages"
                                       : "(signal) Voice %ld record input", a + 1);
    } else if (a < x->nvoices) {
        snprintf_zero(s, 256, "(signal) Voice %ld output", a + 1);
    } else if (a < 2 * x->nvoices) {
        snprintf_zero(s, 256, "(signal) Voice %ld play head (ms in the buffer~)",
                      a - x->nvoices + 1);
    } else {
        snprintf_zero(s, 256, "(list) phase / info reports");
    }
}

void *softkut_new(t_symbol *s, long argc, t_atom *argv)
{
    t_softkut *x = (t_softkut *)object_alloc(softkut_class);
    if (!x) return NULL;

    // args: [buffer~ name] [channels]. channels = voice count, default 1 (mono),
    // clamped to [1, NumVoices].
    t_symbol *name    = (argc > 0 && atom_gettype(argv) == A_SYM)      ? atom_getsym(argv)      : NULL;
    long      nvoices = (argc > 1 && atom_gettype(argv + 1) == A_LONG) ? atom_getlong(argv + 1) : 1;
    if (nvoices < 1) nvoices = 1;
    if (nvoices > NumVoices) nvoices = NumVoices;
    x->nvoices = nvoices;

    dsp_setup((t_pxobject *)x, nvoices);            // one record-input inlet per voice

    // message/report outlet first (-> rightmost), then the voice and sync
    // signal outlets (perform's outs[]: voices, then syncs).
    x->reportout = outlet_new(x, NULL);
    for (int i = 0; i < 2 * nvoices; ++i)
        outlet_new(x, "signal");

    x->engine     = new t_engine();
    x->engine->setNumVoices((int)nvoices);
    x->chanWarned = false;
    x->report     = 0;
    x->tclock     = clock_new((t_object *)x, (method)softkut_clock);

    for (int v = 0; v < NumVoices; ++v) { x->vbuf[v] = NULL; x->vbufname[v] = name; x->vchan[v] = -1; }

    attr_args_process(x, (short)argc, argv);

    x->ob.z_misc |= Z_NO_INPLACE;
    return x;
}

void softkut_free(t_softkut *x)
{
    dsp_free((t_pxobject *)x);
    // the clock goes first: its callback runs on the scheduler thread and reads
    // the engine, so freeing the engine while a report is still armed is a
    // use-after-free. object_free() on a clock unsets it.
    if (x->tclock) object_free(x->tclock);
    if (x->engine) delete x->engine;
    for (int v = 0; v < NumVoices; ++v)
        if (x->vbuf[v]) object_free(x->vbuf[v]);
}

// ---------------------------------------------------------------------------
extern "C" void ext_main(void *r)
{
    t_class *c = class_new("softkut~", (method)softkut_new, (method)softkut_free,
                           (long)sizeof(t_softkut), 0L, A_GIMME, 0);

    // table-driven "<voice> <value>" control messages (shared table)
    softkut::initCommandSymbols();
    int ncmds; softkut::CmdEntry *cmds = softkut::commandTable(&ncmds);
    for (int i = 0; i < ncmds; ++i)
        class_addmethod(c, (method)softkut_cmd, cmds[i].name, A_GIMME, 0);

    class_addmethod(c, (method)softkut_set,      "set",      A_GIMME, 0);
    class_addmethod(c, (method)softkut_voicebuf, "voicebuf", A_GIMME, 0);
    class_addmethod(c, (method)softkut_sync,     "sync",     A_GIMME, 0);
    class_addmethod(c, (method)softkut_stop,     "stop",     A_GIMME, 0);
    class_addmethod(c, (method)softkut_enable,   "enable",   A_GIMME, 0);
    class_addmethod(c, (method)softkut_feedback, "feedback", A_GIMME, 0);
    class_addmethod(c, (method)softkut_inlevel,  "inlevel",  A_GIMME, 0);
    class_addmethod(c, (method)softkut_reset,    "reset",             0);
    class_addmethod(c, (method)softkut_poll,     "poll",              0);

    class_addmethod(c, (method)softkut_dsp64,        "dsp64",    A_CANT, 0);
    class_addmethod(c, (method)softkut_assist,       "assist",   A_CANT, 0);
    class_addmethod(c, (method)softkut_buf_dblclick, "dblclick", A_CANT, 0);
    class_addmethod(c, (method)softkut_notify,       "notify",   A_CANT, 0);

    CLASS_ATTR_LONG(c, "report", 0, t_softkut, report);
    CLASS_ATTR_FILTER_MIN(c, "report", 0);
    CLASS_ATTR_ACCESSORS(c, "report", NULL, (method)softkut::reportSet<t_softkut>);
    CLASS_ATTR_LABEL(c, "report", 0, "Phase report interval (ms, 0 = off)");

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    softkut_class = c;

}
