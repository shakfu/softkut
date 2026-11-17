// softkut_control.h : Max-side helpers shared by softkut~ and mc.softkut~.
//
// The parameter table (one entry per voice attribute), value checks, the
// buffer~ helpers and the sync-outlet conversion. The buffer~ helpers are
// templated on the shell struct, which must provide vbuf[], vbufname[],
// vchan[], chanWarned, nvoices and engine.

#ifndef SOFTKUT_CONTROL_H
#define SOFTKUT_CONTROL_H

#include <cmath>

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include "ext_buffer.h"

#include "softkut_engine.h"
#include "softkut_units.h"

namespace softkut {

// ---- voice parameters ----------------------------------------------------
// Values are in attribute units (ms, Hz, frames, amplitudes); `def` is the
// default after creation and reset.
enum ParamKind { kFloat, kLong, kFlag, kShape };

struct Param {
    const char *name;
    CmdId       id;
    ParamKind   kind;
    double      def;
    const char *min;        // inspector range, null = none
    const char *max;
    const char *category;
    const char *label;
};

static const int kNumParams = 28;   // entries in params()

inline const Param *params(int *count) {
    static const Param p[] = {
        {"rate",       CmdId::Rate,           kFloat, 1,     "-64", "64", "Loop",   "Rate (ignored while inlet 2 has a signal)"},
        {"rateslew",   CmdId::RateSlewTime,   kFloat, 1,     "0",   NULL, "Loop",   "Rate slew (ms)"},
        {"loopstart",  CmdId::LoopStart,      kFloat, 0,     "0",   NULL, "Loop",   "Loop start (ms)"},
        {"loopend",    CmdId::LoopEnd,        kFloat, 0,     "0",   NULL, "Loop",   "Loop end (ms, 0 = buffer end)"},
        {"loop",       CmdId::LoopFlag,       kFlag,  1,     NULL,  NULL, "Loop",   "Loop (off = one-shot)"},
        {"fade",       CmdId::FadeTime,       kFloat, 10,    "0",   NULL, "Loop",   "Loop and cut crossfade (ms)"},
        {"reclevel",   CmdId::RecLevel,       kFloat, 1,     "0",   "1",  "Record", "Record level"},
        {"prelevel",   CmdId::PreLevel,       kFloat, 0,     "0",   "1",  "Record", "Pre level (old material kept)"},
        {"recpreslew", CmdId::RecPreSlewTime, kFloat, 1,     "0",   NULL, "Record", "Record and pre level slew (ms)"},
        {"recoffset",  CmdId::RecOffset,      kLong,  -8,    NULL,  NULL, "Record", "Record head offset (samples)"},
        {"recshape",   CmdId::RecShape,       kShape, 0,     NULL,  NULL, "Record", "Record fade shape (raised inverts polarity)"},
        {"preshape",   CmdId::PreShape,       kShape, 0,     NULL,  NULL, "Record", "Pre fade shape"},
        {"softclip",   CmdId::SoftClip,       kFlag,  0,     NULL,  NULL, "Record", "Soft clip the record input (1.2x gain)"},
        {"prefc",      CmdId::PreFilterFc,    kFloat, 16000, "10",  NULL, "Input filter", "Cutoff (Hz)"},
        {"prerq",      CmdId::PreFilterRq,    kFloat, 4,     "0.01", NULL, "Input filter", "Reciprocal Q"},
        {"prelp",      CmdId::PreFilterLp,    kFloat, 0,     "0",   "1",  "Input filter", "Low-pass mix"},
        {"prehp",      CmdId::PreFilterHp,    kFloat, 0,     "0",   "1",  "Input filter", "High-pass mix"},
        {"prebp",      CmdId::PreFilterBp,    kFloat, 0,     "0",   "1",  "Input filter", "Band-pass mix"},
        {"prebr",      CmdId::PreFilterBr,    kFloat, 0,     "0",   "1",  "Input filter", "Band-reject mix"},
        {"predry",     CmdId::PreFilterDry,   kFloat, 1,     "0",   "1",  "Input filter", "Dry mix"},
        {"prefcmod",   CmdId::PreFilterFcMod, kFloat, 1,     "0",   "1",  "Input filter", "Cutoff tracking of rate"},
        {"postfc",     CmdId::PostFilterFc,   kFloat, 12000, "10",  NULL, "Output filter", "Cutoff (Hz)"},
        {"postrq",     CmdId::PostFilterRq,   kFloat, 4,     "0.01", NULL, "Output filter", "Reciprocal Q"},
        {"postlp",     CmdId::PostFilterLp,   kFloat, 0,     "0",   "1",  "Output filter", "Low-pass mix"},
        {"posthp",     CmdId::PostFilterHp,   kFloat, 0,     "0",   "1",  "Output filter", "High-pass mix"},
        {"postbp",     CmdId::PostFilterBp,   kFloat, 0,     "0",   "1",  "Output filter", "Band-pass mix"},
        {"postbr",     CmdId::PostFilterBr,   kFloat, 0,     "0",   "1",  "Output filter", "Band-reject mix"},
        {"postdry",    CmdId::PostFilterDry,  kFloat, 1,     "0",   "1",  "Output filter", "Dry mix"},
    };
    static_assert(sizeof(p) / sizeof(p[0]) == kNumParams, "update kNumParams");
    if (count) *count = kNumParams;
    return p;
}


// the table's names as symbols, cached on first use
inline t_symbol *paramSym(int i) {
    static t_symbol *syms[kNumParams] = {};
    if (!syms[i]) syms[i] = gensym(params(NULL)[i].name);
    return syms[i];
}

inline int paramIndex(t_symbol *s) {
    for (int i = 0; i < kNumParams; ++i) if (paramSym(i) == s) return i;
    return -1;
}

// round integer kinds; flags are 0 or 1
inline double paramInput(const Param &p, double user) {
    if (p.kind == kFlag)  return user != 0.0 ? 1.0 : 0.0;
    if (p.kind != kFloat) return std::round(user);
    return user;
}

// inspector decoration and saving, after the attribute itself is added
inline void decorateParam(t_class *c, const Param &p) {
    if (p.kind == kFlag)  CLASS_ATTR_STYLE(c, p.name, 0, "onoff");
    if (p.kind == kShape) CLASS_ATTR_ENUMINDEX3(c, p.name, 0, "linear", "sine", "raised");
    if (p.min) CLASS_ATTR_MIN(c, p.name, 0, p.min);
    if (p.max) CLASS_ATTR_MAX(c, p.name, 0, p.max);
    CLASS_ATTR_LABEL(c, p.name, 0, p.label);
    CLASS_ATTR_CATEGORY(c, p.name, 0, p.category);
    CLASS_ATTR_SAVE(c, p.name, 0);
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

// ---- sync outlets ------------------------------------------------------
// The engine reports the play head in seconds of buffer material; the sync
// outlets carry milliseconds, the unit of groove~, play~ and waveform~.
inline void syncToMs(double *const *sync, int nv, long n) {
    for (int v = 0; v < nv; ++v)
        for (long i = 0; i < n; ++i) sync[v][i] *= 1000.0;
}


// ---- buffer~ ---------------------------------------------------------------
// vchan[v] is the 0-based channel voice v reads, or -1 for the default:
// voice v reads channel (v mod channel count), as Max's MC players do.

template <class T>
void ensureVbuf(T *x, int v) {
    if (!x->vbufname[v]) return;
    if (!x->vbuf[v]) x->vbuf[v] = buffer_ref_new((t_object *)x, x->vbufname[v]);
    else             buffer_ref_set(x->vbuf[v], x->vbufname[v]);
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
