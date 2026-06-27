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
    t_bool         monoWarned;

    void          *reportout;
    void          *tclock;
    long           report;

    long           inputChans;                       // channels on the MC input
    double        *zeroIn;                            // silent input for unconnected voices
    long           zeroLen;
} t_mcsoftkut;

static t_class  *mcsoftkut_class = NULL;
static t_symbol *ps_phase, *ps_position;

// ---------------------------------------------------------------------------
static void ensure_vbuf(t_mcsoftkut *x, int v)
{
    if (!x->vbufname[v]) return;
    if (!x->vbuf[v]) x->vbuf[v] = buffer_ref_new((t_object *)x, x->vbufname[v]);
    else             buffer_ref_set(x->vbuf[v], x->vbufname[v]);
}

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

void mcsoftkut_poll(t_mcsoftkut *x)
{
    t_atom a[MC_MAX_VOICES];
    for (int v = 0; v < x->nvoices; ++v)
        atom_setfloat(a + v, x->engine->getSavedPosition(v));
    outlet_anything(x->reportout, ps_position, (short)x->nvoices, a);
}

// ---------------------------------------------------------------------------
// buffer~ association (same semantics as softkut~)
// ---------------------------------------------------------------------------
void mcsoftkut_set(t_mcsoftkut *x, t_symbol *s, long argc, t_atom *argv)
{
    if (argc < 1 || atom_gettype(argv) != A_SYM) {
        object_error((t_object *)x, "set: requires a buffer~ name");
        return;
    }
    t_symbol *name = atom_getsym(argv);
    for (int v = 0; v < x->nvoices; ++v) { x->vbufname[v] = name; ensure_vbuf(x, v); }
    x->monoWarned = false;
    if (!buffer_ref_getobject(x->vbuf[0]))
        object_warn((t_object *)x, "set: no buffer~ named %s", name->s_name);
}

void mcsoftkut_voicebuf(t_mcsoftkut *x, t_symbol *s, long argc, t_atom *argv)
{
    if (argc < 2 || atom_gettype(argv + 1) != A_SYM) {
        object_error((t_object *)x, "voicebuf: expected <voice> <buffer~ name>");
        return;
    }
    long v = atom_getlong(argv);
    if (v < 0 || v >= x->nvoices) {
        object_error((t_object *)x, "voicebuf: voice %ld out of range [0..%ld]", v, x->nvoices - 1);
        return;
    }
    x->vbufname[v] = atom_getsym(argv + 1);
    ensure_vbuf(x, (int)v);
    x->monoWarned = false;
}

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
    x->inputChans = count;
    return false;
}

// ---------------------------------------------------------------------------
// perform
// ---------------------------------------------------------------------------
void mcsoftkut_perform64(t_mcsoftkut *x, t_object *dsp64, double **ins, long numins,
                         double **outs, long numouts, long vec, long flags, void *usr)
{
    const int nv = (int)x->nvoices;

    // map MC input channels to voice record inputs (silence past the connected
    // channel count), and voice outlet channels to the engine's voice outputs.
    double *voiceIns[MC_MAX_VOICES];
    double *voiceOuts[MC_MAX_VOICES];
    for (int v = 0; v < nv; ++v) {
        voiceIns[v]  = (v < numins) ? ins[v] : x->zeroIn;
        voiceOuts[v] = outs[v];
    }
    // no stereo-mix outlet on mc.softkut~ (pan downstream with mc.* objects)
    double *mixL = NULL, *mixR = NULL;

    // resolve + dedup-lock the distinct buffer~s
    t_buffer_obj *lockedObj[MC_MAX_VOICES];
    float        *lockedSamps[MC_MAX_VOICES];
    int           nlocked = 0;
    float        *samps[MC_MAX_VOICES];
    size_t        frames[MC_MAX_VOICES];

    for (int v = 0; v < nv; ++v) {
        samps[v] = NULL; frames[v] = 0;
        t_buffer_obj *b = x->vbuf[v] ? buffer_ref_getobject(x->vbuf[v]) : NULL;
        if (!b) continue;
        if ((long)buffer_getchannelcount(b) != 1) {
            if (!x->monoWarned) {
                object_warn((t_object *)x, "voice %d: multichannel buffer~ not supported (mono only)", v);
                x->monoWarned = true;
            }
            continue;
        }
        int li = -1;
        for (int k = 0; k < nlocked; ++k) if (lockedObj[k] == b) { li = k; break; }
        if (li < 0) {
            float *sp = buffer_locksamples(b);
            if (!sp) continue;
            lockedObj[nlocked]   = b;
            lockedSamps[nlocked] = sp;
            li = nlocked++;
        }
        samps[v]  = lockedSamps[li];
        frames[v] = (size_t)buffer_getframecount(b);
    }

    x->engine->process(voiceIns, voiceOuts, (int)vec, samps, frames, mixL, mixR);

    for (int v = 0; v < nv; ++v)
        if (samps[v] && x->engine->getEnabled(v) && x->engine->getRecFlag(v))
            buffer_setdirty(buffer_ref_getobject(x->vbuf[v]));
    for (int k = 0; k < nlocked; ++k)
        buffer_unlocksamples(lockedObj[k]);
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
    if (sys_getdspstate())
        clock_delay(x->tclock, x->report);
}

// ---------------------------------------------------------------------------
// dsp / lifecycle
// ---------------------------------------------------------------------------
void mcsoftkut_dsp64(t_mcsoftkut *x, t_object *dsp64, short *count, double srate,
                     long maxvectorsize, long flags)
{
    x->engine->setSampleRate(srate);

    for (int v = 0; v < x->nvoices; ++v) ensure_vbuf(x, v);

    // silent input buffer for voices past the connected input channel count
    if (x->zeroLen != maxvectorsize) {
        if (x->zeroIn) sysmem_freeptr(x->zeroIn);
        x->zeroIn  = (double *)sysmem_newptrclear(sizeof(double) * maxvectorsize);
        x->zeroLen = maxvectorsize;
    }

    x->inputChans = (long)object_method(dsp64, gensym("getnuminputchannels"), x, 0);

    t_buffer_obj *b0 = x->vbuf[0] ? buffer_ref_getobject(x->vbuf[0]) : NULL;
    if (b0) {
        long frames = (long)buffer_getframecount(b0);
        long usable = (long)t_engine::usableFrames(frames);
        if (usable && usable != frames)
            object_post((t_object *)x,
                        "buffer~ \"%s\": using %ld of %ld frames (softcut requires power-of-two length)",
                        x->vbufname[0]->s_name, usable, frames);
    }

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
        snprintf_zero(s, 256, "(list) phase / position reports");
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

    x->monoWarned = false;
    x->report     = 0;
    x->tclock     = clock_new((t_object *)x, (method)mcsoftkut_clock);
    x->inputChans = 0;
    x->zeroIn     = NULL;
    x->zeroLen    = 0;

    for (int v = 0; v < MC_MAX_VOICES; ++v) { x->vbuf[v] = NULL; x->vbufname[v] = name; }

    attr_args_process(x, (short)argc, argv);

    x->ob.z_misc |= Z_NO_INPLACE | Z_MC_INLETS;
    return x;
}

void mcsoftkut_free(t_mcsoftkut *x)
{
    dsp_free((t_pxobject *)x);
    if (x->engine) delete x->engine;
    for (int v = 0; v < MC_MAX_VOICES; ++v)
        if (x->vbuf[v]) object_free(x->vbuf[v]);
    if (x->tclock) object_free(x->tclock);
    if (x->zeroIn) sysmem_freeptr(x->zeroIn);
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
    CLASS_ATTR_LABEL(c, "report", 0, "Phase report interval (ms, 0 = off)");

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    mcsoftkut_class = c;

    ps_phase    = gensym("phase");
    ps_position = gensym("position");
}
