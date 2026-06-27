// softkut~ : a Max/MSP external wrapping monome's softcut-lib.
//
// Thin Max shell over softkut::Engine (../../include/softkut_engine.h), which
// owns the softcut voices, the lock-free command queue, the double<->float
// conversion, the power-of-two buffer framing, and the per-voice output
// level/pan ramps + stereo mix. This file owns only Max plumbing: inlets/
// outlets, the per-voice buffer~ references, message parsing, the phase-report
// clock, and the perform call.
//
// Topology: NumVoices voices, one signal inlet (record input) and one signal
// outlet (playback) per voice, plus a stereo mix pair (L/R) carrying the
// equal-power panned sum of all voices, plus a trailing message outlet for
// phase/position reports.
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

#include "softkut_engine.h"

// ---------------------------------------------------------------------------
static const int NumVoices = 6;
typedef softkut::Engine<NumVoices> t_engine;

// outlet index layout (signal outlets first, message outlet last)
enum {
    OUT_MIX_L = NumVoices,        // 6
    OUT_MIX_R = NumVoices + 1,    // 7
    NUM_SIGNAL_OUTLETS = NumVoices + 2
};

typedef struct _softkut {
    t_pxobject     ob;          // MSP object header (must be first)
    t_engine      *engine;      // host-agnostic DSP engine (heap: needs C++ ctor)

    t_buffer_ref  *vbuf[NumVoices];     // per-voice buffer~ reference
    t_symbol      *vbufname[NumVoices]; // per-voice buffer~ name
    t_bool         monoWarned;          // throttle the multichannel warning

    void          *reportout;   // message outlet for phase / position reports
    void          *tclock;      // phase-report clock
    long           report;      // report interval in ms (0 = off)
} t_softkut;

static t_class  *softkut_class = NULL;
static t_symbol *ps_phase, *ps_position;

// ---------------------------------------------------------------------------
// table-driven control surface: every message below is "<voice> <value>" and
// maps onto one engine command id.
// ---------------------------------------------------------------------------
struct CmdEntry { const char *name; softkut::CmdId id; t_symbol *sym; };

static CmdEntry g_cmds[] = {
    {"rate",        softkut::CmdId::Rate,           NULL},
    {"loopstart",   softkut::CmdId::LoopStart,      NULL},
    {"loopend",     softkut::CmdId::LoopEnd,        NULL},
    {"loop",        softkut::CmdId::LoopFlag,       NULL},
    {"fade",        softkut::CmdId::FadeTime,       NULL},
    {"reclevel",    softkut::CmdId::RecLevel,       NULL},
    {"prelevel",    softkut::CmdId::PreLevel,       NULL},
    {"rec",         softkut::CmdId::RecFlag,        NULL},
    {"play",        softkut::CmdId::PlayFlag,       NULL},
    {"reconce",     softkut::CmdId::RecOnceFlag,    NULL},
    {"position",    softkut::CmdId::Position,       NULL},
    {"recoffset",   softkut::CmdId::RecOffset,      NULL},
    {"prefc",       softkut::CmdId::PreFilterFc,    NULL},
    {"prefcmod",    softkut::CmdId::PreFilterFcMod, NULL},
    {"prerq",       softkut::CmdId::PreFilterRq,    NULL},
    {"prelp",       softkut::CmdId::PreFilterLp,    NULL},
    {"prehp",       softkut::CmdId::PreFilterHp,    NULL},
    {"prebp",       softkut::CmdId::PreFilterBp,    NULL},
    {"prebr",       softkut::CmdId::PreFilterBr,    NULL},
    {"predry",      softkut::CmdId::PreFilterDry,   NULL},
    {"postfc",      softkut::CmdId::PostFilterFc,   NULL},
    {"postrq",      softkut::CmdId::PostFilterRq,   NULL},
    {"postlp",      softkut::CmdId::PostFilterLp,   NULL},
    {"posthp",      softkut::CmdId::PostFilterHp,   NULL},
    {"postbp",      softkut::CmdId::PostFilterBp,   NULL},
    {"postbr",      softkut::CmdId::PostFilterBr,   NULL},
    {"postdry",     softkut::CmdId::PostFilterDry,  NULL},
    {"level",       softkut::CmdId::Level,          NULL},
    {"pan",         softkut::CmdId::Pan,            NULL},
    {"levelslew",   softkut::CmdId::LevelSlewTime,  NULL},
    {"panslew",     softkut::CmdId::PanSlewTime,    NULL},
    {"recpreslew",  softkut::CmdId::RecPreSlewTime, NULL},
    {"rateslew",    softkut::CmdId::RateSlewTime,   NULL},
    {"quant",       softkut::CmdId::PhaseQuant,     NULL},
    {"phaseoffset", softkut::CmdId::PhaseOffset,    NULL},
};
static const int g_ncmds = (int)(sizeof(g_cmds) / sizeof(g_cmds[0]));

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
static int parse_voice_val(t_softkut *x, t_symbol *s, long argc, t_atom *argv,
                           long *v, double *val)
{
    if (argc < 2) {
        object_error((t_object *)x, "%s: expected <voice> <value>", s->s_name);
        return 0;
    }
    long voice = atom_getlong(argv);
    if (voice < 0 || voice >= NumVoices) {
        object_error((t_object *)x, "%s: voice %ld out of range [0..%d]",
                     s->s_name, voice, NumVoices - 1);
        return 0;
    }
    *v   = voice;
    *val = atom_getfloat(argv + 1);
    return 1;
}

// create or repoint the buffer~ reference for one voice
static void ensure_vbuf(t_softkut *x, int v)
{
    if (!x->vbufname[v]) return;
    if (!x->vbuf[v]) x->vbuf[v] = buffer_ref_new((t_object *)x, x->vbufname[v]);
    else             buffer_ref_set(x->vbuf[v], x->vbufname[v]);
}

// ---------------------------------------------------------------------------
// control messages -> engine commands
// ---------------------------------------------------------------------------
void softkut_cmd(t_softkut *x, t_symbol *s, long argc, t_atom *argv)
{
    long v; double val;
    if (!parse_voice_val(x, s, argc, argv, &v, &val)) return;

    for (int i = 0; i < g_ncmds; ++i) {
        if (s == g_cmds[i].sym) {
            softkut::Command c{g_cmds[i].id, (int16_t)v, 0, (float)val};
            if (!x->engine->push(c))
                object_warn((t_object *)x, "%s: command queue full, dropped", s->s_name);
            return;
        }
    }
    object_error((t_object *)x, "%s: unknown command", s->s_name);
}

void softkut_sync(t_softkut *x, t_symbol *s, long argc, t_atom *argv)
{
    if (argc < 3) {
        object_error((t_object *)x, "sync: expected <follow> <lead> <offset>");
        return;
    }
    long follow = atom_getlong(argv);
    long lead   = atom_getlong(argv + 1);
    if (follow < 0 || follow >= NumVoices || lead < 0 || lead >= NumVoices) {
        object_error((t_object *)x, "sync: voice index out of range [0..%d]", NumVoices - 1);
        return;
    }
    if (!x->engine->syncVoice((int)follow, (int)lead, (float)atom_getfloat(argv + 2)))
        object_warn((t_object *)x, "sync: command queue full, dropped");
}

void softkut_reset(t_softkut *x)
{
    if (!x->engine->reset())
        object_warn((t_object *)x, "reset: command queue full, dropped");
}

// "stop <voice>": immediately park the voice's heads (distinct from "play 0").
void softkut_stop(t_softkut *x, t_symbol *s, long argc, t_atom *argv)
{
    if (argc < 1) { object_error((t_object *)x, "stop: expected <voice>"); return; }
    long v = atom_getlong(argv);
    if (v < 0 || v >= NumVoices) {
        object_error((t_object *)x, "stop: voice %ld out of range [0..%d]", v, NumVoices - 1);
        return;
    }
    if (!x->engine->stopVoice((int)v))
        object_warn((t_object *)x, "stop: command queue full, dropped");
}

// "enable <voice> <0/1>": master on/off gate for a voice (skips its processing).
void softkut_enable(t_softkut *x, t_symbol *s, long argc, t_atom *argv)
{
    long v; double val;
    if (!parse_voice_val(x, s, argc, argv, &v, &val)) return;
    if (!x->engine->setEnabled((int)v, val > 0.0))
        object_warn((t_object *)x, "enable: command queue full, dropped");
}

// "feedback <src> <dst> <gain>": route voice src's output into voice dst's
// record input (one block delayed).
void softkut_feedback(t_softkut *x, t_symbol *s, long argc, t_atom *argv)
{
    if (argc < 3) {
        object_error((t_object *)x, "feedback: expected <src> <dst> <gain>");
        return;
    }
    long src = atom_getlong(argv);
    long dst = atom_getlong(argv + 1);
    if (src < 0 || src >= NumVoices || dst < 0 || dst >= NumVoices) {
        object_error((t_object *)x, "feedback: voice index out of range [0..%d]", NumVoices - 1);
        return;
    }
    if (!x->engine->setFeedback((int)src, (int)dst, (float)atom_getfloat(argv + 2)))
        object_warn((t_object *)x, "feedback: command queue full, dropped");
}

// "inlevel <inlet> <voice> <gain>": route signal inlet into a voice's record
// input (matrix; defaults to identity: inlet v -> voice v at unity).
void softkut_inlevel(t_softkut *x, t_symbol *s, long argc, t_atom *argv)
{
    if (argc < 3) {
        object_error((t_object *)x, "inlevel: expected <inlet> <voice> <gain>");
        return;
    }
    long inl = atom_getlong(argv);
    long dst = atom_getlong(argv + 1);
    if (inl < 0 || inl >= NumVoices || dst < 0 || dst >= NumVoices) {
        object_error((t_object *)x, "inlevel: index out of range [0..%d]", NumVoices - 1);
        return;
    }
    if (!x->engine->setInLevel((int)inl, (int)dst, (float)atom_getfloat(argv + 2)))
        object_warn((t_object *)x, "inlevel: command queue full, dropped");
}

// report each voice's saved playback position out the message outlet.
void softkut_poll(t_softkut *x)
{
    t_atom a[NumVoices];
    for (int v = 0; v < NumVoices; ++v)
        atom_setfloat(a + v, x->engine->getSavedPosition(v));
    outlet_anything(x->reportout, ps_position, NumVoices, a);
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
    for (int v = 0; v < NumVoices; ++v) { x->vbufname[v] = name; ensure_vbuf(x, v); }
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
    if (v < 0 || v >= NumVoices) {
        object_error((t_object *)x, "voicebuf: voice %ld out of range [0..%d]", v, NumVoices - 1);
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
    double *voiceOuts[NumVoices];
    for (int v = 0; v < NumVoices; ++v) voiceOuts[v] = outs[v];
    double *mixL = (nouts > OUT_MIX_L) ? outs[OUT_MIX_L] : NULL;
    double *mixR = (nouts > OUT_MIX_R) ? outs[OUT_MIX_R] : NULL;

    // Resolve each voice's buffer~ and lock the distinct ones exactly once
    // (several voices may share a buffer~).
    t_buffer_obj *lockedObj[NumVoices];
    float        *lockedSamps[NumVoices];
    int           nlocked = 0;
    float        *samps[NumVoices];
    size_t        frames[NumVoices];

    for (int v = 0; v < NumVoices; ++v) {
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
    for (int v = 0; v < NumVoices; ++v)
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
    for (int v = 0; v < NumVoices; ++v) {
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

    for (int v = 0; v < NumVoices; ++v) ensure_vbuf(x, v);

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
    } else if (a < NumVoices) {
        snprintf_zero(s, 256, "(signal) Voice %ld output", a);
    } else if (a == OUT_MIX_L) {
        snprintf_zero(s, 256, "(signal) Stereo mix L");
    } else if (a == OUT_MIX_R) {
        snprintf_zero(s, 256, "(signal) Stereo mix R");
    } else {
        snprintf_zero(s, 256, "(list) Phase / position reports");
    }
}

void *softkut_new(t_symbol *s, long argc, t_atom *argv)
{
    t_softkut *x = (t_softkut *)object_alloc(softkut_class);
    if (!x) return NULL;

    dsp_setup((t_pxobject *)x, NumVoices);          // NumVoices signal inlets

    // Signal outlets first (indices 0..NUM_SIGNAL_OUTLETS-1) so they line up
    // with the perform outs[] array; the message outlet is created last.
    for (int i = 0; i < NUM_SIGNAL_OUTLETS; ++i)
        outlet_new(x, "signal");
    x->reportout = outlet_new(x, NULL);

    x->engine     = new t_engine();
    x->monoWarned = false;
    x->report     = 0;
    x->tclock     = clock_new((t_object *)x, (method)softkut_clock);

    t_symbol *name = (argc > 0 && atom_gettype(argv) == A_SYM) ? atom_getsym(argv) : NULL;
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

    // table-driven "<voice> <value>" control messages
    for (int i = 0; i < g_ncmds; ++i) {
        g_cmds[i].sym = gensym(g_cmds[i].name);
        class_addmethod(c, (method)softkut_cmd, g_cmds[i].name, A_GIMME, 0);
    }

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
