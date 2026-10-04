// softkut_control.h : shared control-message dispatch for the softkut~ shells.
//
// Both the discrete softkut~ and the multichannel mc.softkut~ present the same
// control surface (the engine commands). This header holds the single copy of
// the command table and the message handlers so the two shells can't drift. The
// handlers are templated on the engine type (softkut~ uses Engine<6>,
// mc.softkut~ uses Engine<kMaxVoices>) and operate only on the engine + the
// owning t_object (for error/warning posts). The buffer~ helpers are templated
// on the shell struct, which must provide vbuf[], vbufname[], vchan[],
// chanWarned, nvoices and engine. Outlets and perform stay shell-specific.

#ifndef SOFTKUT_CONTROL_H
#define SOFTKUT_CONTROL_H

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include "ext_buffer.h"

#include "softkut_engine.h"
#include "softkut_units.h"

namespace softkut {

// "<voice> <value>" control messages -> engine command id.
struct CmdEntry { const char *name; CmdId id; t_symbol *sym; };

// Single shared table (one instance across all translation units, since this is
// an inline function with a static local).
inline CmdEntry *commandTable(int *count) {
    static CmdEntry cmds[] = {
        {"rate",        CmdId::Rate,           nullptr},
        {"loopstart",   CmdId::LoopStart,      nullptr},
        {"loopend",     CmdId::LoopEnd,        nullptr},
        {"loop",        CmdId::LoopFlag,       nullptr},
        {"fade",        CmdId::FadeTime,       nullptr},
        {"reclevel",    CmdId::RecLevel,       nullptr},
        {"prelevel",    CmdId::PreLevel,       nullptr},
        {"rec",         CmdId::RecFlag,        nullptr},
        {"play",        CmdId::PlayFlag,       nullptr},
        {"reconce",     CmdId::RecOnceFlag,    nullptr},
        {"position",    CmdId::Position,       nullptr},
        {"recoffset",   CmdId::RecOffset,      nullptr},
        {"prefc",       CmdId::PreFilterFc,    nullptr},
        {"prefcmod",    CmdId::PreFilterFcMod, nullptr},
        {"prerq",       CmdId::PreFilterRq,    nullptr},
        {"prelp",       CmdId::PreFilterLp,    nullptr},
        {"prehp",       CmdId::PreFilterHp,    nullptr},
        {"prebp",       CmdId::PreFilterBp,    nullptr},
        {"prebr",       CmdId::PreFilterBr,    nullptr},
        {"predry",      CmdId::PreFilterDry,   nullptr},
        {"postfc",      CmdId::PostFilterFc,   nullptr},
        {"postrq",      CmdId::PostFilterRq,   nullptr},
        {"postlp",      CmdId::PostFilterLp,   nullptr},
        {"posthp",      CmdId::PostFilterHp,   nullptr},
        {"postbp",      CmdId::PostFilterBp,   nullptr},
        {"postbr",      CmdId::PostFilterBr,   nullptr},
        {"postdry",     CmdId::PostFilterDry,  nullptr},
        {"level",       CmdId::Level,          nullptr},
        {"pan",         CmdId::Pan,            nullptr},
        {"levelslew",   CmdId::LevelSlewTime,  nullptr},
        {"panslew",     CmdId::PanSlewTime,    nullptr},
        {"recpreslew",  CmdId::RecPreSlewTime, nullptr},
        {"rateslew",    CmdId::RateSlewTime,   nullptr},
        {"quant",       CmdId::PhaseQuant,     nullptr},
        {"phaseoffset", CmdId::PhaseOffset,    nullptr},
    };
    static const int n = (int)(sizeof(cmds) / sizeof(cmds[0]));
    if (count) *count = n;
    return cmds;
}

// Cache the table's symbols. Idempotent; call once from each ext_main.
inline void initCommandSymbols() {
    int n; CmdEntry *t = commandTable(&n);
    for (int i = 0; i < n; ++i)
        if (!t[i].sym) t[i].sym = gensym(t[i].name);
}

// "<voice> <value>": voice counted from 1 -> engine index; value as given
inline int parseVoiceVal(t_object *owner, t_symbol *s, long argc, t_atom *argv,
                         int numVoices, long *v, double *val) {
    if (argc < 2) {
        object_error(owner, "%s: expected <voice> <value>", s->s_name);
        return 0;
    }
    const long user = atom_getlong(argv);
    const int  idx  = toIndex(user, numVoices);
    if (idx < 0) {
        object_error(owner, "%s: voice %ld out of range [1..%d]", s->s_name, user, numVoices);
        return 0;
    }
    *v = idx; *val = atom_getfloat(argv + 1);
    return 1;
}

// Convert a message value to engine units and report what the engine will do
// with it: reject it, or clamp it (the warning shows message units). The engine
// re-applies the same check on the audio thread; this only informs.
inline bool checkValue(t_object *owner, const char *name, CmdId id, double user,
                       double sr, float *engine) {
    float v = toEngine(id, user);
    switch (sanitize(id, v, sr)) {
        case Check::Rejected:
            object_error(owner, "%s: value is not a finite number", name);
            return false;
        case Check::Clamped:
            object_warn(owner, "%s: %g out of range, clamped to %g", name, user, toUser(id, v));
            break;
        default:
            break;
    }
    *engine = v;
    return true;
}

// ---- templated message handlers (engine-only) --------------------------
template <class Eng>
void dispatchCmd(Eng *engine, t_object *owner, t_symbol *s, long argc, t_atom *argv) {
    long v; double val;
    if (!parseVoiceVal(owner, s, argc, argv, engine->numVoices(), &v, &val)) return;
    int n; CmdEntry *t = commandTable(&n);
    for (int i = 0; i < n; ++i) {
        if (s == t[i].sym) {
            float f;
            if (!checkValue(owner, s->s_name, t[i].id, val, engine->getSampleRate(), &f)) return;
            Command c{t[i].id, (int16_t)v, 0, f};
            if (!engine->push(c)) object_warn(owner, "%s: command queue full, dropped", s->s_name);
            return;
        }
    }
    object_error(owner, "%s: unknown command", s->s_name);
}

// two indices counted from 1 -> engine indices; errors name `what`
inline int parsePair(t_object *owner, const char *msg, const char *what, t_atom *argv,
                     int count, int *a, int *b) {
    *a = toIndex(atom_getlong(argv), count);
    *b = toIndex(atom_getlong(argv + 1), count);
    if (*a < 0 || *b < 0) {
        object_error(owner, "%s: %s out of range [1..%d]", msg, what, count);
        return 0;
    }
    return 1;
}

template <class Eng>
void dispatchSync(Eng *engine, t_object *owner, long argc, t_atom *argv) {
    if (argc < 3) { object_error(owner, "sync: expected <follow> <lead> <offset ms>"); return; }
    int follow, lead;
    if (!parsePair(owner, "sync", "voice", argv, engine->numVoices(), &follow, &lead)) return;
    float off;
    if (!checkValue(owner, "sync", CmdId::VoiceSync, atom_getfloat(argv + 2),
                    engine->getSampleRate(), &off)) return;
    if (!engine->syncVoice(follow, lead, off))
        object_warn(owner, "sync: command queue full, dropped");
}

template <class Eng>
void dispatchStop(Eng *engine, t_object *owner, long argc, t_atom *argv) {
    if (argc < 1) { object_error(owner, "stop: expected <voice>"); return; }
    const long user = atom_getlong(argv);
    const int  v    = toIndex(user, engine->numVoices());
    if (v < 0) {
        object_error(owner, "stop: voice %ld out of range [1..%d]", user, engine->numVoices());
        return;
    }
    if (!engine->stopVoice(v)) object_warn(owner, "stop: command queue full, dropped");
}

template <class Eng>
void dispatchEnable(Eng *engine, t_object *owner, t_symbol *s, long argc, t_atom *argv) {
    long v; double val;
    if (!parseVoiceVal(owner, s, argc, argv, engine->numVoices(), &v, &val)) return;
    if (!engine->setEnabled((int)v, val > 0.0)) object_warn(owner, "enable: command queue full, dropped");
}

template <class Eng>
void dispatchFeedback(Eng *engine, t_object *owner, long argc, t_atom *argv) {
    if (argc < 3) { object_error(owner, "feedback: expected <src> <dst> <gain>"); return; }
    int src, dst;
    if (!parsePair(owner, "feedback", "voice", argv, engine->numVoices(), &src, &dst)) return;
    float g;
    if (!checkValue(owner, "feedback", CmdId::FbLevel, atom_getfloat(argv + 2),
                    engine->getSampleRate(), &g)) return;
    if (!engine->setFeedback(src, dst, g))
        object_warn(owner, "feedback: command queue full, dropped");
}

template <class Eng>
void dispatchInlevel(Eng *engine, t_object *owner, long argc, t_atom *argv) {
    if (argc < 3) { object_error(owner, "inlevel: expected <inlet> <voice> <gain>"); return; }
    int inl, dst;
    if (!parsePair(owner, "inlevel", "index", argv, engine->numVoices(), &inl, &dst)) return;
    float g;
    if (!checkValue(owner, "inlevel", CmdId::InLevel, atom_getfloat(argv + 2),
                    engine->getSampleRate(), &g)) return;
    if (!engine->setInLevel(inl, dst, g))
        object_warn(owner, "inlevel: command queue full, dropped");
}

// ---- reports (report outlet), shared by both shells ---------------------
// T provides engine, nvoices and reportout. Voices are reported from 1, times
// in ms of buffer material.

// `poll`: one `info <voice> <pos 0-1> <play> <rec> <startMs> <endMs> <windowMs>
// <state> <positionMs>` per voice. positionMs is the play head in the buffer.
template <class T>
void reportInfo(T *x) {
    static t_symbol *ps_info = gensym("info");
    for (int v = 0; v < x->nvoices; ++v) {
        const VoiceInfo vi = x->engine->getVoiceInfo(v);
        t_atom a[9];
        atom_setlong (a + 0, v + 1);
        atom_setfloat(a + 1, vi.position);
        atom_setlong (a + 2, vi.play);
        atom_setlong (a + 3, vi.rec);
        atom_setfloat(a + 4, vi.startSec * 1000.0);
        atom_setfloat(a + 5, vi.endSec * 1000.0);
        atom_setfloat(a + 6, vi.windowSec * 1000.0);
        atom_setlong (a + 7, vi.state);
        atom_setfloat(a + 8, vi.bufferSec * 1000.0);
        outlet_anything(x->reportout, ps_info, 9, a);
    }
}

// report clock: `phase <voice> <ms>` for each voice whose phase changed
template <class T>
void reportPhase(T *x) {
    static t_symbol *ps_phase = gensym("phase");
    for (int v = 0; v < x->nvoices; ++v) {
        if (x->engine->checkQuantPhaseChanged(v)) {
            t_atom a[2];
            atom_setlong (a + 0, v + 1);
            atom_setfloat(a + 1, x->engine->getQuantPhase(v) * 1000.0);
            outlet_anything(x->reportout, ps_phase, 2, a);
        }
    }
}

// @report setter, shared by both shells (each has `report` and `tclock`).
// Arms or stops the report clock at once; it was otherwise only armed on the
// next DSP start, so turning reports on while audio ran did nothing.
template <class T>
t_max_err reportSet(T *x, void *attr, long argc, t_atom *argv) {
    if (argc < 1 || !argv) return MAX_ERR_NONE;
    const long ms = atom_getlong(argv);
    x->report = ms < 0 ? 0 : ms;
    if (x->report > 0 && sys_getdspobjdspstate((t_object *)x))
        clock_delay(x->tclock, x->report);
    else
        clock_unset(x->tclock);
    return MAX_ERR_NONE;
}

// ---- sync outlets ------------------------------------------------------
// The engine reports the play head in seconds of buffer material; the sync
// outlets carry milliseconds, the unit of groove~, play~ and waveform~.
inline void syncToMs(double *const *sync, int nv, long n) {
    for (int v = 0; v < nv; ++v)
        for (long i = 0; i < n; ++i) sync[v][i] *= 1000.0;
}

// ---- buffer~ association ----------------------------------------------
// vchan[v] is the 0-based channel voice v reads, or -1 for the default:
// voice v reads channel (v mod channel count), as Max's MC players do.

template <class T>
void ensureVbuf(T *x, int v) {
    if (!x->vbufname[v]) return;
    if (!x->vbuf[v]) x->vbuf[v] = buffer_ref_new((t_object *)x, x->vbufname[v]);
    else             buffer_ref_set(x->vbuf[v], x->vbufname[v]);
}

// Optional 1-based channel argument -> vchan value. Returns 0 on bad input.
inline int parseChannel(t_object *owner, const char *msg, long argc, t_atom *argv, long *chan) {
    *chan = -1;
    if (argc < 1) return 1;
    const long c = atom_getlong(argv);
    if (c < 1) { object_error(owner, "%s: channel %ld must be 1 or more", msg, c); return 0; }
    *chan = c - 1;
    return 1;
}

// "set <name> [<chan>]": every voice reads the named buffer~.
template <class T>
void setBuffer(T *x, long argc, t_atom *argv) {
    if (argc < 1 || atom_gettype(argv) != A_SYM) {
        object_error((t_object *)x, "set: expected <buffer~ name> [<channel>]");
        return;
    }
    long chan;
    if (!parseChannel((t_object *)x, "set", argc - 1, argv + 1, &chan)) return;
    t_symbol *name = atom_getsym(argv);
    for (int v = 0; v < x->nvoices; ++v) {
        x->vbufname[v] = name;
        x->vchan[v] = chan;
        ensureVbuf(x, v);
    }
    x->chanWarned = false;
    if (!buffer_ref_getobject(x->vbuf[0]))
        object_warn((t_object *)x, "set: no buffer~ named %s", name->s_name);
}

// "voicebuf <voice> <name> [<chan>]": one voice reads its own buffer~.
template <class T>
void setVoiceBuffer(T *x, long argc, t_atom *argv) {
    if (argc < 2 || atom_gettype(argv + 1) != A_SYM) {
        object_error((t_object *)x, "voicebuf: expected <voice> <buffer~ name> [<channel>]");
        return;
    }
    const long user = atom_getlong(argv);
    const int  v    = toIndex(user, (int)x->nvoices);
    if (v < 0) {
        object_error((t_object *)x, "voicebuf: voice %ld out of range [1..%ld]", user, x->nvoices);
        return;
    }
    long chan;
    if (!parseChannel((t_object *)x, "voicebuf", argc - 2, argv + 2, &chan)) return;
    x->vbufname[v] = atom_getsym(argv + 1);
    x->vchan[v] = chan;
    ensureVbuf(x, v);
    x->chanWarned = false;
}

// The distinct buffer~s one perform call has locked.
template <int N>
struct BufferLocks {
    t_buffer_obj *obj[N];
    float        *samps[N];
    int           n = 0;
};

// Resolve each voice's buffer~, lock each distinct one once, and fill views.
// A voice with no buffer~, or naming a channel the buffer~ lacks, is silent.
template <class T, int N>
void lockBuffers(T *x, int nv, BufferView (&views)[N], BufferLocks<N> &locks) {
    for (int v = 0; v < nv; ++v) {
        views[v] = BufferView{nullptr, 0, 1, 0.0};
        t_buffer_obj *b = x->vbuf[v] ? buffer_ref_getobject(x->vbuf[v]) : NULL;
        if (!b) continue;
        const long nch = (long)buffer_getchannelcount(b);
        if (nch < 1) continue;
        const long chan = x->vchan[v] < 0 ? v % nch : x->vchan[v];
        if (chan >= nch) {
            if (!x->chanWarned) {
                object_warn((t_object *)x, "voice %d: buffer~ %s has no channel %ld",
                            v + 1, x->vbufname[v]->s_name, chan + 1);
                x->chanWarned = true;
            }
            continue;
        }
        int li = -1;
        for (int k = 0; k < locks.n; ++k) if (locks.obj[k] == b) { li = k; break; }
        if (li < 0) {
            float *sp = buffer_locksamples(b);
            if (!sp) continue;
            locks.obj[locks.n] = b;
            locks.samps[locks.n] = sp;
            li = locks.n++;
        }
        views[v] = BufferView{locks.samps[li] + chan, (size_t)buffer_getframecount(b),
                              (unsigned int)nch, (double)buffer_getsamplerate(b)};
    }
}

// Mark buffer~s that were written dirty, then unlock. getWroteBlock() covers a
// record-once pass, which clears the record flag inside the same process() call
// that performs its last writes.
template <class T, int N>
void releaseBuffers(T *x, int nv, const BufferView (&views)[N], BufferLocks<N> &locks) {
    for (int v = 0; v < nv; ++v)
        if (views[v].samples && x->engine->getWroteBlock(v))
            buffer_setdirty(buffer_ref_getobject(x->vbuf[v]));
    for (int k = 0; k < locks.n; ++k)
        buffer_unlocksamples(locks.obj[k]);
}

} // namespace softkut

#endif // SOFTKUT_CONTROL_H
