// mc.softkut~ : multichannel variant of softkut~.
//
// Same softcut engine and control surface as softkut~, but presented through
// Max's MC (multichannel) system: a single multichannel record-input inlet, a
// multichannel voice-output outlet, a 2-channel stereo-mix outlet, and a message
// outlet for reports. The voice count is set by the second creation argument
// (default 6, capped at MC_MAX_VOICES) and becomes the channel count of the
// voice-output outlet.
//
// All DSP, the command queue, and the routing matrices live in the shared
// softkut::Engine; the control messages are dispatched through softkut_control.h
// (shared with softkut~). This file owns only the MC-specific plumbing.

#include "ext.h"
#include "ext_obex.h"
#include "ext_buffer.h"
#include "z_dsp.h"

#include "softkut_control.h"   // shared command dispatch (pulls in the engine)

// ---------------------------------------------------------------------------
static const int MC_MAX_VOICES = 16;                 // compile-time voice cap
typedef softkut::Engine<MC_MAX_VOICES> t_engine;

typedef struct _mcsoftkut {
    t_pxobject     ob;
    t_engine      *engine;
    long           nvoices;                          // active voices = output chans

    t_buffer_ref  *vbuf[MC_MAX_VOICES];
    t_symbol      *vbufname[MC_MAX_VOICES];
    long           vchan[MC_MAX_VOICES];   // channel per voice, -1 = voice mod channels
    t_bool         chanWarned;        // throttle the missing-channel warning

    void          *reportout;
    void          *tclock;
    long           report;
} t_mcsoftkut;

static t_class  *mcsoftkut_class = NULL;
static t_symbol *ps_phase, *ps_info;

// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// control messages -> shared dispatch
// ---------------------------------------------------------------------------
void mcsoftkut_cmd(t_mcsoftkut *x, t_symbol *s, long argc, t_atom *argv)
{ softkut::dispatchCmd(x->engine, (t_object *)x, s, argc, argv); }

void mcsoftkut_sync(t_mcsoftkut *x, t_symbol *s, long argc, t_atom *argv)
{ softkut::dispatchSync(x->engine, (t_object *)x, argc, argv); }

void mcsoftkut_stop(t_mcsoftkut *x, t_symbol *s, long argc, t_atom *argv)
{ softkut::dispatchStop(x->engine, (t_object *)x, argc, argv); }

void mcsoftkut_enable(t_mcsoftkut *x, t_symbol *s, long argc, t_atom *argv)
{ softkut::dispatchEnable(x->engine, (t_object *)x, s, argc, argv); }

void mcsoftkut_feedback(t_mcsoftkut *x, t_symbol *s, long argc, t_atom *argv)
{ softkut::dispatchFeedback(x->engine, (t_object *)x, argc, argv); }

void mcsoftkut_inlevel(t_mcsoftkut *x, t_symbol *s, long argc, t_atom *argv)
{ softkut::dispatchInlevel(x->engine, (t_object *)x, argc, argv); }

void mcsoftkut_reset(t_mcsoftkut *x)
{
    if (!x->engine->reset())
        object_warn((t_object *)x, "reset: command queue full, dropped");
}

// one `info <voice> <pos> <play> <rec> <startMs> <endMs> <windowMs> <state>`
// list per voice; positions converted from seconds to ms here.
void mcsoftkut_poll(t_mcsoftkut *x)
{
    for (int v = 0; v < x->nvoices; ++v) {
        softkut::VoiceInfo vi = x->engine->getVoiceInfo(v);
        t_atom a[8];
        atom_setlong (a + 0, v);
        atom_setfloat(a + 1, vi.position);
        atom_setlong (a + 2, vi.play);
        atom_setlong (a + 3, vi.rec);
        atom_setfloat(a + 4, vi.startSec * 1000.0);
        atom_setfloat(a + 5, vi.endSec * 1000.0);
        atom_setfloat(a + 6, vi.windowSec * 1000.0);
        atom_setlong (a + 7, vi.state);
        outlet_anything(x->reportout, ps_info, 8, a);
    }
}

// ---------------------------------------------------------------------------
// buffer~ association (same semantics as softkut~)
// ---------------------------------------------------------------------------
void mcsoftkut_set(t_mcsoftkut *x, t_symbol *s, long argc, t_atom *argv)
{ softkut::setBuffer(x, argc, argv); }

void mcsoftkut_voicebuf(t_mcsoftkut *x, t_symbol *s, long argc, t_atom *argv)
{ softkut::setVoiceBuffer(x, argc, argv); }

// ---------------------------------------------------------------------------
// MC negotiation
// ---------------------------------------------------------------------------
// channels produced on the (single) multichannel voice-output outlet.
long mcsoftkut_multichanneloutputs(t_mcsoftkut *x, long index)
{
    return (index == 0) ? x->nvoices : 0;
}

// input channel count changed; our output count is fixed by nvoices, so the
// output channel count never changes -> return false.
long mcsoftkut_inputchanged(t_mcsoftkut *x, long index, long count)
{
    return false;
}

// ---------------------------------------------------------------------------
// perform
// ---------------------------------------------------------------------------
void mcsoftkut_perform64(t_mcsoftkut *x, t_object *dsp64, double **ins, long numins,
                         double **outs, long numouts, long vec, long flags, void *usr)
{
    const int nv = (int)x->nvoices;

    // map MC input channels to voice record inputs (null = silence past the
    // connected channel count), and voice outlet channels to the voice outputs.
    double *voiceIns[MC_MAX_VOICES];
    double *voiceOuts[MC_MAX_VOICES];
    for (int v = 0; v < nv; ++v) {
        voiceIns[v]  = (v < numins) ? ins[v] : NULL;
        voiceOuts[v] = outs[v];
    }
    // no stereo-mix outlet on mc.softkut~ (pan downstream with mc.* objects)
    double *mixL = NULL, *mixR = NULL;

    softkut::BufferView            views[MC_MAX_VOICES];
    softkut::BufferLocks<MC_MAX_VOICES> locks;
    softkut::lockBuffers(x, nv, views, locks);
    x->engine->process(voiceIns, voiceOuts, (int)vec, views, mixL, mixR);
    softkut::releaseBuffers(x, nv, views, locks);
}

// ---------------------------------------------------------------------------
// phase-report clock
// ---------------------------------------------------------------------------
void mcsoftkut_clock(t_mcsoftkut *x)
{
    if (x->report <= 0) return;
    for (int v = 0; v < x->nvoices; ++v) {
        if (x->engine->checkQuantPhaseChanged(v)) {
            t_atom a[2];
            atom_setlong (a + 0, v);
            atom_setfloat(a + 1, x->engine->getQuantPhase(v));
            outlet_anything(x->reportout, ps_phase, 2, a);
        }
    }
    if (sys_getdspobjdspstate((t_object *)x))   // this patcher's audio, not global
        clock_delay(x->tclock, x->report);
}

// ---------------------------------------------------------------------------
// dsp / lifecycle
// ---------------------------------------------------------------------------
void mcsoftkut_dsp64(t_mcsoftkut *x, t_object *dsp64, short *count, double srate,
                     long maxvectorsize, long flags)
{
    x->engine->setSampleRate(srate);

    for (int v = 0; v < x->nvoices; ++v) softkut::ensureVbuf(x, v);


    object_method(dsp64, gensym("dsp_add64"), x, (method)mcsoftkut_perform64, 0, NULL);

    if (x->report > 0)
        clock_delay(x->tclock, x->report);
}

void mcsoftkut_buf_dblclick(t_mcsoftkut *x)
{
    for (int v = 0; v < x->nvoices; ++v) {
        t_buffer_obj *b = x->vbuf[v] ? buffer_ref_getobject(x->vbuf[v]) : NULL;
        if (b) { buffer_view(b); return; }
    }
}

t_max_err mcsoftkut_notify(t_mcsoftkut *x, t_symbol *s, t_symbol *msg, void *sender, void *data)
{
    for (int v = 0; v < MC_MAX_VOICES; ++v)
        if (x->vbuf[v]) buffer_ref_notify(x->vbuf[v], s, msg, sender, data);
    return MAX_ERR_NONE;
}

void mcsoftkut_assist(t_mcsoftkut *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) {
        snprintf_zero(s, 256, "(multichannel signal) per-voice record inputs / messages");
    } else if (a == 0) {
        snprintf_zero(s, 256, "(multichannel signal) %ld voice outputs", x->nvoices);
    } else {
        snprintf_zero(s, 256, "(list) phase / info reports");
    }
}

void *mcsoftkut_new(t_symbol *s, long argc, t_atom *argv)
{
    t_mcsoftkut *x = (t_mcsoftkut *)object_alloc(mcsoftkut_class);
    if (!x) return NULL;

    t_symbol *name    = (argc > 0 && atom_gettype(argv) == A_SYM)  ? atom_getsym(argv)  : NULL;
    long      nvoices = (argc > 1 && atom_gettype(argv + 1) == A_LONG) ? atom_getlong(argv + 1) : 6;
    if (nvoices < 1) nvoices = 1;
    if (nvoices > MC_MAX_VOICES) nvoices = MC_MAX_VOICES;
    x->nvoices = nvoices;

    dsp_setup((t_pxobject *)x, 1);                  // one MC record-input inlet

    // one multichannel voice-output outlet, then the message outlet. Stereo is
    // left to downstream mc.* objects (the per-voice level still applies here).
    x->reportout = outlet_new(x, NULL);             // outlet 1: reports
    outlet_new(x, "multichannelsignal");            // outlet 0: voice outputs

    x->engine = new t_engine();
    x->engine->setNumVoices((int)nvoices);

    x->chanWarned = false;
    x->report     = 0;
    x->tclock     = clock_new((t_object *)x, (method)mcsoftkut_clock);

    for (int v = 0; v < MC_MAX_VOICES; ++v) { x->vbuf[v] = NULL; x->vbufname[v] = name; x->vchan[v] = -1; }

    attr_args_process(x, (short)argc, argv);

    x->ob.z_misc |= Z_NO_INPLACE | Z_MC_INLETS;
    return x;
}

void mcsoftkut_free(t_mcsoftkut *x)
{
    dsp_free((t_pxobject *)x);
    // the clock goes first: its callback runs on the scheduler thread and reads
    // the engine, so freeing the engine while a report is still armed is a
    // use-after-free. object_free() on a clock unsets it.
    if (x->tclock) object_free(x->tclock);
    if (x->engine) delete x->engine;
    for (int v = 0; v < MC_MAX_VOICES; ++v)
        if (x->vbuf[v]) object_free(x->vbuf[v]);
}

// ---------------------------------------------------------------------------
extern "C" void ext_main(void *r)
{
    t_class *c = class_new("mc.softkut~", (method)mcsoftkut_new, (method)mcsoftkut_free,
                           (long)sizeof(t_mcsoftkut), 0L, A_GIMME, 0);

    softkut::initCommandSymbols();
    int ncmds; softkut::CmdEntry *cmds = softkut::commandTable(&ncmds);
    for (int i = 0; i < ncmds; ++i)
        class_addmethod(c, (method)mcsoftkut_cmd, cmds[i].name, A_GIMME, 0);

    class_addmethod(c, (method)mcsoftkut_set,      "set",      A_GIMME, 0);
    class_addmethod(c, (method)mcsoftkut_voicebuf, "voicebuf", A_GIMME, 0);
    class_addmethod(c, (method)mcsoftkut_sync,     "sync",     A_GIMME, 0);
    class_addmethod(c, (method)mcsoftkut_stop,     "stop",     A_GIMME, 0);
    class_addmethod(c, (method)mcsoftkut_enable,   "enable",   A_GIMME, 0);
    class_addmethod(c, (method)mcsoftkut_feedback, "feedback", A_GIMME, 0);
    class_addmethod(c, (method)mcsoftkut_inlevel,  "inlevel",  A_GIMME, 0);
    class_addmethod(c, (method)mcsoftkut_reset,    "reset",             0);
    class_addmethod(c, (method)mcsoftkut_poll,     "poll",              0);

    class_addmethod(c, (method)mcsoftkut_dsp64,               "dsp64",               A_CANT, 0);
    class_addmethod(c, (method)mcsoftkut_multichanneloutputs, "multichanneloutputs", A_CANT, 0);
    class_addmethod(c, (method)mcsoftkut_inputchanged,        "inputchanged",        A_CANT, 0);
    class_addmethod(c, (method)mcsoftkut_assist,              "assist",              A_CANT, 0);
    class_addmethod(c, (method)mcsoftkut_buf_dblclick,        "dblclick",            A_CANT, 0);
    class_addmethod(c, (method)mcsoftkut_notify,              "notify",              A_CANT, 0);

    CLASS_ATTR_LONG(c, "report", 0, t_mcsoftkut, report);
    CLASS_ATTR_FILTER_MIN(c, "report", 0);
    CLASS_ATTR_ACCESSORS(c, "report", NULL, (method)softkut::reportSet<t_mcsoftkut>);
    CLASS_ATTR_LABEL(c, "report", 0, "Phase report interval (ms, 0 = off)");

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    mcsoftkut_class = c;

    ps_phase    = gensym("phase");
    ps_info     = gensym("info");
}
