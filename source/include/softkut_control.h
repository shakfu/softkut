// softkut_control.h : shared control-message dispatch for the softkut~ shells.
//
// Both the discrete softkut~ and the multichannel mc.softkut~ present the same
// control surface (the engine commands). This header holds the single copy of
// the command table and the message handlers so the two shells can't drift. The
// handlers are templated on the engine type (softkut~ uses Engine<6>,
// mc.softkut~ uses Engine<kMaxVoices>) and operate only on the engine + the
// owning t_object (for error/warning posts); buffer~ handling, outlets and
// perform stay shell-specific.

#ifndef SOFTKUT_CONTROL_H
#define SOFTKUT_CONTROL_H

#include "ext.h"
#include "ext_obex.h"

#include "softkut_engine.h"

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

inline int parseVoiceVal(t_object *owner, t_symbol *s, long argc, t_atom *argv,
                         int numVoices, long *v, double *val) {
    if (argc < 2) {
        object_error(owner, "%s: expected <voice> <value>", s->s_name);
        return 0;
    }
    long voice = atom_getlong(argv);
    if (voice < 0 || voice >= numVoices) {
        object_error(owner, "%s: voice %ld out of range [0..%d]", s->s_name, voice, numVoices - 1);
        return 0;
    }
    *v = voice; *val = atom_getfloat(argv + 1);
    return 1;
}

// ---- templated message handlers (engine-only) --------------------------
template <class Eng>
void dispatchCmd(Eng *engine, t_object *owner, t_symbol *s, long argc, t_atom *argv) {
    long v; double val;
    if (!parseVoiceVal(owner, s, argc, argv, engine->numVoices(), &v, &val)) return;
    int n; CmdEntry *t = commandTable(&n);
    for (int i = 0; i < n; ++i) {
        if (s == t[i].sym) {
            Command c{t[i].id, (int16_t)v, 0, (float)val};
            if (!engine->push(c)) object_warn(owner, "%s: command queue full, dropped", s->s_name);
            return;
        }
    }
    object_error(owner, "%s: unknown command", s->s_name);
}

template <class Eng>
void dispatchSync(Eng *engine, t_object *owner, long argc, t_atom *argv) {
    if (argc < 3) { object_error(owner, "sync: expected <follow> <lead> <offset>"); return; }
    long follow = atom_getlong(argv), lead = atom_getlong(argv + 1);
    int nv = engine->numVoices();
    if (follow < 0 || follow >= nv || lead < 0 || lead >= nv) {
        object_error(owner, "sync: voice index out of range [0..%d]", nv - 1); return;
    }
    if (!engine->syncVoice((int)follow, (int)lead, (float)atom_getfloat(argv + 2)))
        object_warn(owner, "sync: command queue full, dropped");
}

template <class Eng>
void dispatchStop(Eng *engine, t_object *owner, long argc, t_atom *argv) {
    if (argc < 1) { object_error(owner, "stop: expected <voice>"); return; }
    long v = atom_getlong(argv); int nv = engine->numVoices();
    if (v < 0 || v >= nv) { object_error(owner, "stop: voice %ld out of range [0..%d]", v, nv - 1); return; }
    if (!engine->stopVoice((int)v)) object_warn(owner, "stop: command queue full, dropped");
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
    long src = atom_getlong(argv), dst = atom_getlong(argv + 1); int nv = engine->numVoices();
    if (src < 0 || src >= nv || dst < 0 || dst >= nv) {
        object_error(owner, "feedback: voice index out of range [0..%d]", nv - 1); return;
    }
    if (!engine->setFeedback((int)src, (int)dst, (float)atom_getfloat(argv + 2)))
        object_warn(owner, "feedback: command queue full, dropped");
}

template <class Eng>
void dispatchInlevel(Eng *engine, t_object *owner, long argc, t_atom *argv) {
    if (argc < 3) { object_error(owner, "inlevel: expected <inlet> <voice> <gain>"); return; }
    long inl = atom_getlong(argv), dst = atom_getlong(argv + 1); int nv = engine->numVoices();
    if (inl < 0 || inl >= nv || dst < 0 || dst >= nv) {
        object_error(owner, "inlevel: index out of range [0..%d]", nv - 1); return;
    }
    if (!engine->setInLevel((int)inl, (int)dst, (float)atom_getfloat(argv + 2)))
        object_warn(owner, "inlevel: command queue full, dropped");
}

} // namespace softkut

#endif // SOFTKUT_CONTROL_H
