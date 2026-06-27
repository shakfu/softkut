// softkut~ : a Max/MSP external wrapping monome's softcut-lib.
//
// Thin Max shell over softkut::Engine (../../include/softkut_engine.h), which
// owns the softcut voices, the lock-free command queue, the double<->float
// conversion, the power-of-two buffer framing, and the per-voice output
// level/pan ramps + stereo mix. This file owns only Max plumbing: inlets/
// outlets, the per-voice buffer~ references, message parsing, the phase-report
// clock, and the perform call.
//
// Topology: a runtime voice count set by the second creation argument
// (channels, default 1, max NumVoices=6). One signal inlet (record input) and
// one signal outlet (playback) per voice, plus a trailing message outlet for
// phase/position reports. (No stereo mix outlet -- pan downstream if needed.)
//
// Buffer model (zero-copy): each voice points at the locked
// samples of its own mono buffer~ (defaulting to the shared name; override per
// voice with "voicebuf <v> <name>" -- e.g. route voice 0 -> left buffer~,
// voice 1 -> right buffer~ for stereo). softcut needs contiguous mono and wraps
// indices with a power-of-two bitmask, so the engine uses the largest power-of-
// two prefix of each buffer~'s frame count. Multichannel buffer~s are refused.

#include "ext.h"          // standard Max include, always required
#include "ext_obex.h"     // required for new style Max object
#include "ext_buffer.h"   // buffer~ reference + lock/unlock
#include "z_dsp.h"        // MSP

#include "softkut_control.h"   // shared command table + dispatch (pulls in engine)

// ---------------------------------------------------------------------------
static const int NumVoices = 6;             // compile-time maximum (array sizing)
typedef softkut::Engine<NumVoices> t_engine;

// Signal outlets are laid out: nvoices voice outputs, then mix L, then mix R
// (mix outlet indices are runtime = nvoices, nvoices+1); the message outlet is
// created last.

typedef struct _softkut {
    t_pxobject     ob;          // MSP object header (must be first)
    t_engine      *engine;      // host-agnostic DSP engine (heap: needs C++ ctor)
    long           nvoices;     // active voice count (1..NumVoices, creation arg)

    t_buffer_ref  *vbuf[NumVoices];     // per-voice buffer~ reference
    t_symbol      *vbufname[NumVoices]; // per-voice buffer~ name
    t_bool         monoWarned;          // throttle the multichannel warning

    void          *reportout;   // message outlet for phase / position reports
    void          *tclock;      // phase-report clock
    long           report;      // report interval in ms (0 = off)
} t_softkut;

static t_class  *softkut_class = NULL;
static t_symbol *ps_phase, *ps_position;

// The control surface (command table + dispatch) lives in softkut_control.h,
// shared with mc.softkut~. The thunks below forward to it.

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
// create or repoint the buffer~ reference for one voice
static void ensure_vbuf(t_softkut *x, int v)
{
    if (!x->vbufname[v]) return;
    if (!x->vbuf[v]) x->vbuf[v] = buffer_ref_new((t_object *)x, x->vbufname[v]);
    else             buffer_ref_set(x->vbuf[v], x->vbufname[v]);
}

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

// report each voice's saved playback position out the message outlet.
void softkut_poll(t_softkut *x)
{
    t_atom a[NumVoices];
    for (int v = 0; v < x->nvoices; ++v)
        atom_setfloat(a + v, x->engine->getSavedPosition(v));
    outlet_anything(x->reportout, ps_position, (short)x->nvoices, a);
}

// ---------------------------------------------------------------------------
// buffer~ association
// ---------------------------------------------------------------------------
// "set <name>": point every voice at the named (shared) buffer~.
void softkut_set(t_softkut *x, t_symbol *s, long argc, t_atom *argv)
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

// "voicebuf <v> <name>": override one voice's buffer~ (e.g. for stereo).
void softkut_voicebuf(t_softkut *x, t_symbol *s, long argc, t_atom *argv)
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
// perform
// ---------------------------------------------------------------------------
void softkut_perform64(t_softkut *x, t_object *dsp64, double **ins, long nins,
                       double **outs, long nouts, long vec, long flags, void *usr)
{
    const int nv = (int)x->nvoices;
    double *voiceOuts[NumVoices];
    for (int v = 0; v < nv; ++v) voiceOuts[v] = outs[v];
    double *mixL = NULL, *mixR = NULL;   // no stereo-mix outlets (pan downstream)

    // Resolve each voice's buffer~ and lock the distinct ones exactly once
    // (several voices may share a buffer~).
    t_buffer_obj *lockedObj[NumVoices];
    float        *lockedSamps[NumVoices];
    int           nlocked = 0;
    float        *samps[NumVoices];
    size_t        frames[NumVoices];

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

    x->engine->process(ins, voiceOuts, (int)vec, samps, frames, mixL, mixR);

    // mark recorded-into buffers dirty, then release all locks
    for (int v = 0; v < nv; ++v)
        if (samps[v] && x->engine->getEnabled(v) && x->engine->getRecFlag(v))
            buffer_setdirty(buffer_ref_getobject(x->vbuf[v]));
    for (int k = 0; k < nlocked; ++k)
        buffer_unlocksamples(lockedObj[k]);
}

// ---------------------------------------------------------------------------
// phase-report clock
// ---------------------------------------------------------------------------
void softkut_clock(t_softkut *x)
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
void softkut_dsp64(t_softkut *x, t_object *dsp64, short *count, double srate,
                   long maxvectorsize, long flags)
{
    x->engine->setSampleRate(srate);

    for (int v = 0; v < x->nvoices; ++v) ensure_vbuf(x, v);

    // inform the user once if voice 0's buffer~ length is being reduced
    t_buffer_obj *b0 = x->vbuf[0] ? buffer_ref_getobject(x->vbuf[0]) : NULL;
    if (b0) {
        long frames = (long)buffer_getframecount(b0);
        long usable = (long)t_engine::usableFrames(frames);
        if (usable && usable != frames)
            object_post((t_object *)x,
                        "buffer~ \"%s\": using %ld of %ld frames (softcut requires power-of-two length)",
                        x->vbufname[0]->s_name, usable, frames);
    }

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
        snprintf_zero(s, 256, (a == 0) ? "(signal) Voice 0 record input / messages"
                                       : "(signal) Voice %ld record input", a);
    } else if (a < x->nvoices) {
        snprintf_zero(s, 256, "(signal) Voice %ld output", a);
    } else {
        snprintf_zero(s, 256, "(list) Phase / position reports");
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

    // one signal outlet per voice (line up with the perform outs[] array); the
    // message outlet is created last.
    x->reportout = outlet_new(x, NULL);
    for (int i = 0; i < nvoices; ++i)
        outlet_new(x, "signal");

    x->engine     = new t_engine();
    x->engine->setNumVoices((int)nvoices);
    x->monoWarned = false;
    x->report     = 0;
    x->tclock     = clock_new((t_object *)x, (method)softkut_clock);

    for (int v = 0; v < NumVoices; ++v) { x->vbuf[v] = NULL; x->vbufname[v] = name; }

    attr_args_process(x, (short)argc, argv);

    x->ob.z_misc |= Z_NO_INPLACE;
    return x;
}

void softkut_free(t_softkut *x)
{
    dsp_free((t_pxobject *)x);
    if (x->engine) delete x->engine;
    for (int v = 0; v < NumVoices; ++v)
        if (x->vbuf[v]) object_free(x->vbuf[v]);
    if (x->tclock) object_free(x->tclock);
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
    CLASS_ATTR_LABEL(c, "report", 0, "Phase report interval (ms, 0 = off)");

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    softkut_class = c;

    ps_phase    = gensym("phase");
    ps_position = gensym("position");
}
