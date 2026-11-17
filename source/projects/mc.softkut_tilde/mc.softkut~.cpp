// mc.softkut~ : N softcut voices on multichannel signals.
//
// The multichannel counterpart of softkut~, over softkut::Engine<16>. Voice v
// is channel v of every signal port. Parameters are array attributes, one
// value per voice: one value sets every voice, a list sets voices in order,
// and "setvalue <voice> <attr|message> ..." addresses one voice. @feedback
// routes voice outputs into record inputs, one chunk delayed. Design:
// docs/dev/m3-design.md (mc.softkut~ section).
//
// Ports: inlet 1 is the record input (MC) and takes every message, inlet 2 is
// the rate (MC signal, or a float that sets @rate). Outlet 1 is the audio
// (MC), outlet 2 the play heads (MC, @syncmode), outlet 3 the dumpout.

#include "ext.h"
#include "ext_obex.h"
#include "ext_buffer.h"
#include "z_dsp.h"

#include "softkut_control.h"

static const int kMaxVoices = 16;
typedef softkut::Engine<kMaxVoices> t_engine;
using softkut::CmdId;
using softkut::Param;
using softkut::kNumParams;
using softkut::kFloat;
static const Param *g_params = softkut::params(NULL);

typedef struct _mcsoftkut {
    t_pxobject     ob;
    t_engine      *engine;
    long           nvoices;               // @chans, fixed at creation
    t_bool         ready;                 // construction finished

    t_buffer_ref  *vbuf[kMaxVoices];      // one reference per voice, same buffer~
    t_symbol      *vbufname[kMaxVoices];
    long           vchan[kMaxVoices];     // 0-based channel, -1 = voice mod channels
    t_bool         chanWarned;
    t_symbol      *buffer;                // @buffer
    long           channel[kMaxVoices];   // @channel, from 1; 0 = voice number, wrapping
    long           channelCount;
    long           syncmode;              // @syncmode: 0 = ms, 1 = phase

    double         fval[kNumParams][kMaxVoices];
    long           lval[kNumParams][kMaxVoices];
    long           count[kNumParams];     // values per array attribute (= nvoices)

    float          fb[kMaxVoices][kMaxVoices];   // @feedback gains [src][dst]

    long           recChans;              // connected channels per inlet
    long           rateChans;
    long           rateOffset;            // index of inlet 2's first channel in ins[]
    void          *dumpout;
} t_mcsoftkut;

static t_class *mcsoftkut_class = NULL;

// ---------------------------------------------------------------------------
// parameters
// ---------------------------------------------------------------------------
// Check, store and send one voice's value (attribute units).
static void applyParam(t_mcsoftkut *x, int i, int v, double user)
{
    const Param &p = g_params[i];
    user = softkut::paramInput(p, user);
    float eng;
    if (!softkut::checkValue((t_object *)x, p.name, p.id, user, x->engine->getSampleRate(), &eng))
        return;
    const double stored = softkut::toUser(p.id, eng);
    if (p.kind == kFloat) x->fval[i][v] = stored;
    else                  x->lval[i][v] = (long)std::lround(stored);
    if (!x->engine->push(softkut::Command{p.id, (int16_t)v, 0, eng}))
        object_warn((t_object *)x, "%s: command queue full, dropped", p.name);
}

// one value: every voice; a list: voices in order
t_max_err mcsoftkut_param_set(t_mcsoftkut *x, t_object *attr, long argc, t_atom *argv)
{
    const int i = softkut::paramIndex((t_symbol *)object_method(attr, gensym("getname")));
    if (i < 0 || argc < 1 || !argv) return MAX_ERR_GENERIC;
    if (argc == 1) {
        for (int v = 0; v < x->nvoices; ++v) applyParam(x, i, v, atom_getfloat(argv));
    } else {
        if (argc > x->nvoices)
            object_warn((t_object *)x, "%s: %ld values for %ld voices, extra ignored",
                        g_params[i].name, argc, x->nvoices);
        for (int v = 0; v < argc && v < x->nvoices; ++v) applyParam(x, i, v, atom_getfloat(argv + v));
    }
    return MAX_ERR_NONE;
}

static void applyDefaults(t_mcsoftkut *x)
{
    for (int i = 0; i < kNumParams; ++i) {
        for (int v = 0; v < x->nvoices; ++v) applyParam(x, i, v, g_params[i].def);
        object_attr_touch((t_object *)x, softkut::paramSym(i));
    }
}

// ---------------------------------------------------------------------------
// buffer~, channel, sync mode, voice count
// ---------------------------------------------------------------------------
t_max_err mcsoftkut_buffer_set(t_mcsoftkut *x, t_object *attr, long argc, t_atom *argv)
{
    if (argc < 1 || atom_gettype(argv) != A_SYM) return MAX_ERR_GENERIC;
    x->buffer = atom_getsym(argv);
    for (int v = 0; v < kMaxVoices; ++v) {
        x->vbufname[v] = x->buffer;
        if (v < x->nvoices) softkut::ensureVbuf(x, v);
    }
    x->chanWarned = false;
    return MAX_ERR_NONE;
}

static void setChannel(t_mcsoftkut *x, int v, long c)
{
    if (c < 0) {
        object_error((t_object *)x, "channel: %ld must be 0 (the voice number) or more", c);
        return;
    }
    x->channel[v] = c;
    x->vchan[v]   = c - 1;          // 0 -> -1: voice mod channels
}

// one value: every voice; a list: voices in order
t_max_err mcsoftkut_channel_set(t_mcsoftkut *x, t_object *attr, long argc, t_atom *argv)
{
    if (argc < 1 || !argv) return MAX_ERR_GENERIC;
    for (int v = 0; v < x->nvoices; ++v)
        if (argc == 1 || v < argc) setChannel(x, v, atom_getlong(argv + (argc == 1 ? 0 : v)));
    x->chanWarned = false;
    return MAX_ERR_NONE;
}

t_max_err mcsoftkut_syncmode_set(t_mcsoftkut *x, t_object *attr, long argc, t_atom *argv)
{
    if (argc < 1) return MAX_ERR_GENERIC;
    x->syncmode = atom_getlong(argv) ? 1 : 0;
    for (int v = 0; v < kMaxVoices; ++v) x->engine->setSyncPhase(v, x->syncmode == 1);
    return MAX_ERR_NONE;
}

// @chans sizes the signal outlets, so it is read only at creation
t_max_err mcsoftkut_chans_set(t_mcsoftkut *x, t_object *attr, long argc, t_atom *argv)
{
    if (argc < 1) return MAX_ERR_GENERIC;
    if (x->ready) {
        object_error((t_object *)x, "chans: set it in the object box");
        return MAX_ERR_GENERIC;
    }
    long n = atom_getlong(argv);
    if (n < 1 || n > kMaxVoices) {
        object_warn((t_object *)x, "chans: %ld out of range, clamped to [1..%d]", n, kMaxVoices);
        n = n < 1 ? 1 : kMaxVoices;
    }
    x->nvoices = n;
    return MAX_ERR_NONE;
}

// "set <name> [<channel>]": @buffer, and @channel for every voice if given
void mcsoftkut_set(t_mcsoftkut *x, t_symbol *s, long argc, t_atom *argv)
{
    if (argc < 1 || atom_gettype(argv) != A_SYM) {
        object_error((t_object *)x, "set: expected <buffer~ name> [<channel>]");
        return;
    }
    if (argc > 1) object_attr_setlong(x, gensym("channel"), atom_getlong(argv + 1));
    object_attr_setsym(x, gensym("buffer"), atom_getsym(argv));
    if (!buffer_ref_getobject(x->vbuf[0]))
        object_warn((t_object *)x, "set: no buffer~ named %s", atom_getsym(argv)->s_name);
}

// ---------------------------------------------------------------------------
// transport
// ---------------------------------------------------------------------------
static void push(t_mcsoftkut *x, const char *msg, CmdId id, int v, float value)
{
    if (!x->engine->push(softkut::Command{id, (int16_t)v, 0, value}))
        object_warn((t_object *)x, "%s: command queue full, dropped", msg);
}

// a transport command for one voice; `value` in message units
static void transport(t_mcsoftkut *x, const char *msg, CmdId id, int v, double value)
{
    float eng = 0.f;
    if (id == CmdId::Position) {
        if (!softkut::checkValue((t_object *)x, msg, id, value, x->engine->getSampleRate(), &eng))
            return;
    } else {
        eng = value != 0.0 ? 1.f : 0.f;
    }
    push(x, msg, id, v, eng);
}

// "play 1": every voice; "play 1 0 1": voices in order. Same for rec, position.
static void listMessage(t_mcsoftkut *x, t_symbol *s, long argc, t_atom *argv, CmdId id)
{
    if (argc < 1) { object_error((t_object *)x, "%s: expected a value", s->s_name); return; }
    for (int v = 0; v < x->nvoices; ++v)
        if (argc == 1 || v < argc)
            transport(x, s->s_name, id, v, atom_getfloat(argv + (argc == 1 ? 0 : v)));
}

void mcsoftkut_play(t_mcsoftkut *x, t_symbol *s, long argc, t_atom *argv)
{ listMessage(x, s, argc, argv, CmdId::PlayFlag); }

void mcsoftkut_rec(t_mcsoftkut *x, t_symbol *s, long argc, t_atom *argv)
{ listMessage(x, s, argc, argv, CmdId::RecFlag); }

void mcsoftkut_position(t_mcsoftkut *x, t_symbol *s, long argc, t_atom *argv)
{ listMessage(x, s, argc, argv, CmdId::Position); }

void mcsoftkut_reconce(t_mcsoftkut *x)
{ for (int v = 0; v < x->nvoices; ++v) push(x, "reconce", CmdId::RecOnceFlag, v, 1.f); }

void mcsoftkut_stop(t_mcsoftkut *x)
{ for (int v = 0; v < x->nvoices; ++v) push(x, "stop", CmdId::Stop, v, 0.f); }

// "sync <follow> <lead> <offset ms>": cut one voice to another's position
void mcsoftkut_sync(t_mcsoftkut *x, t_symbol *s, long argc, t_atom *argv)
{
    if (argc < 2) { object_error((t_object *)x, "sync: expected <follow> <lead> [<offset ms>]"); return; }
    const long f = atom_getlong(argv), l = atom_getlong(argv + 1);
    const int  follow = softkut::toIndex(f, (int)x->nvoices);
    const int  lead   = softkut::toIndex(l, (int)x->nvoices);
    if (follow < 0 || lead < 0) {
        object_error((t_object *)x, "sync: voice out of range [1..%ld]", x->nvoices);
        return;
    }
    float off = 0.f;
    if (argc > 2 && !softkut::checkValue((t_object *)x, "sync", CmdId::VoiceSync, atom_getfloat(argv + 2),
                                         x->engine->getSampleRate(), &off)) return;
    if (!x->engine->syncVoice(follow, lead, off)) object_warn((t_object *)x, "sync: command queue full, dropped");
}

// "setvalue <voice> <attribute|message> <args...>": one voice
void mcsoftkut_setvalue(t_mcsoftkut *x, t_symbol *s, long argc, t_atom *argv)
{
    if (argc < 2 || atom_gettype(argv + 1) != A_SYM) {
        object_error((t_object *)x, "setvalue: expected <voice> <attribute or message> <value>");
        return;
    }
    const long user = atom_getlong(argv);
    const int  v    = softkut::toIndex(user, (int)x->nvoices);
    if (v < 0) { object_error((t_object *)x, "setvalue: voice %ld out of range [1..%ld]", user, x->nvoices); return; }
    t_symbol  *name = atom_getsym(argv + 1);
    const long n    = argc - 2;
    const double val = n > 0 ? atom_getfloat(argv + 2) : 0.0;

    const int i = softkut::paramIndex(name);
    if (i >= 0) {
        if (n < 1) { object_error((t_object *)x, "setvalue: %s needs a value", name->s_name); return; }
        applyParam(x, i, v, val);
        object_attr_touch((t_object *)x, name);
    } else if (name == gensym("channel")) {
        if (n < 1) { object_error((t_object *)x, "setvalue: channel needs a value"); return; }
        setChannel(x, v, atom_getlong(argv + 2));
        x->chanWarned = false;
        object_attr_touch((t_object *)x, name);
    } else if (name == gensym("play") || name == gensym("rec") || name == gensym("position")) {
        if (n < 1) { object_error((t_object *)x, "setvalue: %s needs a value", name->s_name); return; }
        const CmdId id = name == gensym("play") ? CmdId::PlayFlag
                       : name == gensym("rec")  ? CmdId::RecFlag : CmdId::Position;
        transport(x, name->s_name, id, v, val);
    } else if (name == gensym("reconce")) {
        push(x, "reconce", CmdId::RecOnceFlag, v, 1.f);
    } else if (name == gensym("stop")) {
        push(x, "stop", CmdId::Stop, v, 0.f);
    } else {
        object_error((t_object *)x, "setvalue: %s is not a voice attribute or message", name->s_name);
    }
}

// ---------------------------------------------------------------------------
// feedback routes
// ---------------------------------------------------------------------------
// "feedback <src> <dst> <gain> ...": set routes (voices from 1), one chunk
// delayed. Other routes keep their gains. Saved as the list of nonzero routes.
t_max_err mcsoftkut_feedback_set(t_mcsoftkut *x, t_object *attr, long argc, t_atom *argv)
{
    if (argc < 3 || argc % 3 || !argv) {
        object_error((t_object *)x, "feedback: expected <src> <dst> <gain>, repeated");
        return MAX_ERR_GENERIC;
    }
    for (long k = 0; k < argc; k += 3) {
        const long us = atom_getlong(argv + k), ud = atom_getlong(argv + k + 1);
        const int  src = softkut::toIndex(us, (int)x->nvoices);
        const int  dst = softkut::toIndex(ud, (int)x->nvoices);
        if (src < 0 || dst < 0) {
            object_error((t_object *)x, "feedback: voice out of range [1..%ld]", x->nvoices);
            continue;
        }
        float g;
        if (!softkut::checkValue((t_object *)x, "feedback", CmdId::FbLevel, atom_getfloat(argv + k + 2),
                                 x->engine->getSampleRate(), &g)) continue;
        x->fb[src][dst] = g;
        if (!x->engine->setFeedback(src, dst, g))
            object_warn((t_object *)x, "feedback: command queue full, dropped");
    }
    return MAX_ERR_NONE;
}

t_max_err mcsoftkut_feedback_get(t_mcsoftkut *x, t_object *attr, long *argc, t_atom **argv)
{
    long n = 0;
    for (int s = 0; s < x->nvoices; ++s)
        for (int d = 0; d < x->nvoices; ++d) if (x->fb[s][d] != 0.f) n += 3;
    char alloc;
    if (atom_alloc_array(n, argc, argv, &alloc) != MAX_ERR_NONE) return MAX_ERR_OUT_OF_MEM;
    long k = 0;
    for (int s = 0; s < x->nvoices; ++s)
        for (int d = 0; d < x->nvoices; ++d) {
            if (x->fb[s][d] == 0.f) continue;
            atom_setlong (*argv + k++, s + 1);
            atom_setlong (*argv + k++, d + 1);
            atom_setfloat(*argv + k++, x->fb[s][d]);
        }
    *argc = n;
    return MAX_ERR_NONE;
}

// stop and restore every default, including no feedback; @buffer and
// @channel are kept
void mcsoftkut_reset(t_mcsoftkut *x)
{
    if (!x->engine->reset()) object_warn((t_object *)x, "reset: command queue full, dropped");
    for (int s = 0; s < kMaxVoices; ++s)
        for (int d = 0; d < kMaxVoices; ++d) x->fb[s][d] = 0.f;
    object_attr_touch((t_object *)x, gensym("feedback"));
    applyDefaults(x);
    object_attr_setlong(x, gensym("syncmode"), 0);
}

// one "info <voice> <play> <rec> <state> <position> <start> <end>" per voice
void mcsoftkut_poll(t_mcsoftkut *x)
{
    for (int v = 0; v < x->nvoices; ++v) {
        const softkut::VoiceInfo vi = x->engine->getVoiceInfo(v);
        t_atom a[7];
        atom_setlong (a + 0, v + 1);
        atom_setlong (a + 1, vi.play);
        atom_setlong (a + 2, vi.rec);
        atom_setlong (a + 3, vi.state);
        atom_setfloat(a + 4, vi.bufferSec * 1000.0);
        atom_setfloat(a + 5, vi.startSec * 1000.0);
        atom_setfloat(a + 6, vi.endSec * 1000.0);
        outlet_anything(x->dumpout, gensym("info"), 7, a);
    }
}

// a number in inlet 2 sets @rate for every voice; inlet 1 takes no numbers
void mcsoftkut_float(t_mcsoftkut *x, double f)
{
    if (proxy_getinlet((t_object *)x) == 1)
        object_attr_setfloat(x, gensym("rate"), f);
    else
        object_error((t_object *)x, "inlet 1 takes a signal or messages; send the rate to inlet 2");
}

void mcsoftkut_int(t_mcsoftkut *x, long n) { mcsoftkut_float(x, (double)n); }

// ---------------------------------------------------------------------------
// MC negotiation, perform, dsp
// ---------------------------------------------------------------------------
long mcsoftkut_multichanneloutputs(t_mcsoftkut *x, long index)
{
    return (index == 0 || index == 1) ? x->nvoices : 0;
}

long mcsoftkut_inputchanged(t_mcsoftkut *x, long index, long count)
{
    return false;   // output channel counts follow @chans only
}

void mcsoftkut_perform64(t_mcsoftkut *x, t_object *dsp64, double **ins, long numins,
                         double **outs, long numouts, long vec, long flags, void *usr)
{
    const int nv = (int)x->nvoices;
    // ins[]: inlet 1's channels, then inlet 2's. Voices past a connected
    // channel count record silence, or use @rate.
    const double *recIn[kMaxVoices], *rateIn[kMaxVoices];
    double       *audio[kMaxVoices], *sync[kMaxVoices];
    for (int v = 0; v < nv; ++v) {
        recIn[v]  = v < x->recChans ? ins[v] : NULL;
        rateIn[v] = v < x->rateChans ? ins[x->rateOffset + v] : NULL;
        audio[v]  = outs[v];
        sync[v]   = outs[nv + v];
    }
    softkut::BufferView               views[kMaxVoices];
    softkut::BufferLocks<kMaxVoices>  locks;
    softkut::lockBuffers(x, nv, views, locks);
    x->engine->process(recIn, audio, (int)vec, views, sync, rateIn);
    softkut::releaseBuffers(x, nv, views, locks);
    if (x->syncmode == 0) softkut::syncToMs(sync, nv, vec);
}

void mcsoftkut_dsp64(t_mcsoftkut *x, t_object *dsp64, short *count, double srate,
                     long maxvectorsize, long flags)
{
    x->engine->setSampleRate(srate);
    // channels per inlet. ins[] holds inlet 1's channels, then inlet 2's; an
    // unconnected inlet still has its channels there, carrying nothing.
    const long recN  = (long)object_method(dsp64, gensym("getnuminputchannels"), x, 0);
    const long rateN = (long)object_method(dsp64, gensym("getnuminputchannels"), x, 1);
    x->recChans   = count[0] ? recN : 0;
    x->rateChans  = count[1] ? rateN : 0;
    x->rateOffset = recN;
    for (int v = 0; v < x->nvoices; ++v) softkut::ensureVbuf(x, v);
    object_method(dsp64, gensym("dsp_add64"), x, (method)mcsoftkut_perform64, 0, NULL);
}

// ---------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------
void mcsoftkut_dblclick(t_mcsoftkut *x)
{
    t_buffer_obj *b = x->vbuf[0] ? buffer_ref_getobject(x->vbuf[0]) : NULL;
    if (b) buffer_view(b);
}

t_max_err mcsoftkut_notify(t_mcsoftkut *x, t_symbol *s, t_symbol *msg, void *sender, void *data)
{
    for (int v = 0; v < kMaxVoices; ++v)
        if (x->vbuf[v]) buffer_ref_notify(x->vbuf[v], s, msg, sender, data);
    return MAX_ERR_NONE;
}

void mcsoftkut_assist(t_mcsoftkut *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) {
        snprintf_zero(s, 256, a == 0 ? "(multichannel signal) Record inputs, messages"
                                     : "(multichannel signal/float) Rates");
    } else if (a == 0) {
        snprintf_zero(s, 256, "(multichannel signal) %ld voice outputs", x->nvoices);
    } else if (a == 1) {
        snprintf_zero(s, 256, x->syncmode ? "(multichannel signal) %ld play heads, 0-1 across the loop"
                                          : "(multichannel signal) %ld play heads, ms in the buffer~",
                      x->nvoices);
    } else {
        snprintf_zero(s, 256, "(list) info, attribute values");
    }
}

void *mcsoftkut_new(t_symbol *s, long argc, t_atom *argv)
{
    t_mcsoftkut *x = (t_mcsoftkut *)object_alloc(mcsoftkut_class);
    if (!x) return NULL;

    dsp_setup((t_pxobject *)x, 2);                    // record inputs, rates
    x->dumpout = outlet_new(x, NULL);                 // created first: rightmost
    object_obex_store(x, gensym("dumpout"), (t_object *)x->dumpout);
    outlet_new(x, "multichannelsignal");              // play heads
    outlet_new(x, "multichannelsignal");              // audio

    x->engine     = new t_engine();
    x->nvoices    = 1;
    x->ready      = false;
    x->chanWarned = false;
    x->buffer     = NULL;
    x->syncmode   = 0;
    x->recChans   = x->rateChans = x->rateOffset = 0;
    x->channelCount = kMaxVoices;
    for (int v = 0; v < kMaxVoices; ++v) {
        x->vbuf[v] = NULL; x->vbufname[v] = NULL; x->vchan[v] = -1; x->channel[v] = 0;
        for (int w = 0; w < kMaxVoices; ++w) x->fb[v][w] = 0.f;
    }

    // @chans first: it sizes every array attribute
    const long nargs = attr_args_offset((short)argc, argv);
    for (long k = nargs; k + 1 < argc; ++k)
        if (atom_gettype(argv + k) == A_SYM && atom_getsym(argv + k) == gensym("@chans"))
            mcsoftkut_chans_set(x, NULL, 1, argv + k + 1);
    x->engine->setNumVoices((int)x->nvoices);
    x->channelCount = x->nvoices;
    for (int i = 0; i < kNumParams; ++i) x->count[i] = x->nvoices;
    applyDefaults(x);

    if (nargs > 0) {
        if (atom_gettype(argv) == A_SYM) object_attr_setsym(x, gensym("buffer"), atom_getsym(argv));
        else object_error((t_object *)x, "first argument must be a buffer~ name");
    }
    if (nargs > 1)
        object_warn((t_object *)x, "extra arguments ignored: set the voice count with @chans");
    attr_args_process(x, (short)argc, argv);
    x->ready = true;

    x->ob.z_misc |= Z_NO_INPLACE | Z_MC_INLETS;
    return x;
}

void mcsoftkut_free(t_mcsoftkut *x)
{
    dsp_free((t_pxobject *)x);
    if (x->engine) delete x->engine;
    for (int v = 0; v < kMaxVoices; ++v)
        if (x->vbuf[v]) object_free(x->vbuf[v]);
}

// ---------------------------------------------------------------------------
extern "C" void ext_main(void *r)
{
    t_class *c = class_new("mc.softkut~", (method)mcsoftkut_new, (method)mcsoftkut_free,
                           (long)sizeof(t_mcsoftkut), 0L, A_GIMME, 0);

    class_addmethod(c, (method)mcsoftkut_play,     "play",     A_GIMME, 0);
    class_addmethod(c, (method)mcsoftkut_rec,      "rec",      A_GIMME, 0);
    class_addmethod(c, (method)mcsoftkut_position, "position", A_GIMME, 0);
    class_addmethod(c, (method)mcsoftkut_reconce,  "reconce",  0);
    class_addmethod(c, (method)mcsoftkut_stop,     "stop",     0);
    class_addmethod(c, (method)mcsoftkut_sync,     "sync",     A_GIMME, 0);
    class_addmethod(c, (method)mcsoftkut_setvalue, "setvalue", A_GIMME, 0);
    class_addmethod(c, (method)mcsoftkut_reset,    "reset",    0);
    class_addmethod(c, (method)mcsoftkut_poll,     "poll",     0);
    class_addmethod(c, (method)mcsoftkut_set,      "set",      A_GIMME, 0);
    class_addmethod(c, (method)mcsoftkut_float,    "float",    A_FLOAT, 0);
    class_addmethod(c, (method)mcsoftkut_int,      "int",      A_LONG, 0);

    class_addmethod(c, (method)mcsoftkut_dsp64,               "dsp64",               A_CANT, 0);
    class_addmethod(c, (method)mcsoftkut_multichanneloutputs, "multichanneloutputs", A_CANT, 0);
    class_addmethod(c, (method)mcsoftkut_inputchanged,        "inputchanged",        A_CANT, 0);
    class_addmethod(c, (method)mcsoftkut_assist,              "assist",              A_CANT, 0);
    class_addmethod(c, (method)mcsoftkut_dblclick,            "dblclick",            A_CANT, 0);
    class_addmethod(c, (method)mcsoftkut_notify,              "notify",              A_CANT, 0);
    class_addmethod(c, (method)object_obex_dumpout,           "dumpout",             A_CANT, 0);

    CLASS_ATTR_LONG(c, "chans", 0, t_mcsoftkut, nvoices);
    CLASS_ATTR_ACCESSORS(c, "chans", NULL, (method)mcsoftkut_chans_set);
    CLASS_ATTR_LABEL(c, "chans", 0, "Voices (set in the object box)");
    CLASS_ATTR_CATEGORY(c, "chans", 0, "Buffer");

    CLASS_ATTR_SYM(c, "buffer", 0, t_mcsoftkut, buffer);
    CLASS_ATTR_ACCESSORS(c, "buffer", NULL, (method)mcsoftkut_buffer_set);
    CLASS_ATTR_LABEL(c, "buffer", 0, "buffer~ name");
    CLASS_ATTR_CATEGORY(c, "buffer", 0, "Buffer");
    CLASS_ATTR_SAVE(c, "buffer", 0);

    CLASS_ATTR_LONG_VARSIZE(c, "channel", 0, t_mcsoftkut, channel, channelCount, kMaxVoices);
    CLASS_ATTR_ACCESSORS(c, "channel", NULL, (method)mcsoftkut_channel_set);
    CLASS_ATTR_LABEL(c, "channel", 0, "buffer~ channel per voice (0 = voice number, wrapping)");
    CLASS_ATTR_CATEGORY(c, "channel", 0, "Buffer");
    CLASS_ATTR_SAVE(c, "channel", 0);

    class_addattr(c, attribute_new("feedback", gensym("atom"), 0,
                                   (method)mcsoftkut_feedback_get, (method)mcsoftkut_feedback_set));
    CLASS_ATTR_LABEL(c, "feedback", 0, "Feedback routes (src dst gain ...)");
    CLASS_ATTR_CATEGORY(c, "feedback", 0, "Record");
    CLASS_ATTR_SAVE(c, "feedback", 0);

    CLASS_ATTR_LONG(c, "syncmode", 0, t_mcsoftkut, syncmode);
    CLASS_ATTR_ACCESSORS(c, "syncmode", NULL, (method)mcsoftkut_syncmode_set);
    CLASS_ATTR_ENUMINDEX2(c, "syncmode", 0, "ms", "phase");
    CLASS_ATTR_LABEL(c, "syncmode", 0, "Play head output");
    CLASS_ATTR_CATEGORY(c, "syncmode", 0, "Buffer");
    CLASS_ATTR_SAVE(c, "syncmode", 0);

    for (int i = 0; i < kNumParams; ++i) {
        const Param &p = g_params[i];
        const bool f = p.kind == kFloat;
        const long off = f ? (long)(calcoffset(t_mcsoftkut, fval) + i * kMaxVoices * sizeof(double))
                           : (long)(calcoffset(t_mcsoftkut, lval) + i * kMaxVoices * sizeof(long));
        const long cnt = (long)(calcoffset(t_mcsoftkut, count) + i * sizeof(long));
        class_addattr(c, attr_offset_array_new(p.name, f ? gensym("float64") : gensym("long"),
                                               kMaxVoices, 0, NULL, (method)mcsoftkut_param_set,
                                               cnt, off));
        softkut::decorateParam(c, p);
    }

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    mcsoftkut_class = c;
}
