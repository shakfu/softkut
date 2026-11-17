// softkut~ : one softcut voice as a Max/MSP object.
//
// Thin Max shell over softkut::Engine<1> (../../include/softkut_engine.h).
// Voice parameters are attributes; transport stays as messages, since a saved
// play or rec flag would start the voice when the patcher opens. Design:
// docs/dev/m3-design.md.
//
// Ports: inlet 1 is the record input and takes every message, inlet 2 is the
// rate (signal, or a float that sets @rate). Outlet 1 is the audio, outlet 2
// the play head (@syncmode), outlet 3 the dumpout (info, getattr replies).

#include "ext.h"
#include "ext_obex.h"
#include "ext_buffer.h"
#include "z_dsp.h"

#include "softkut_control.h"   // buffer~ helpers, value checks (pulls in engine)

typedef softkut::Engine<1> t_engine;
using softkut::CmdId;

using softkut::Param;
using softkut::kNumParams;
using softkut::kFloat;
static const Param *g_params = softkut::params(NULL);

// ---------------------------------------------------------------------------
typedef struct _softkut {
    t_pxobject     ob;
    t_engine      *engine;
    long           nvoices;           // always 1 (the shared buffer~ helpers read it)

    t_buffer_ref  *vbuf[1];
    t_symbol      *vbufname[1];       // @buffer
    long           vchan[1];          // 0-based channel (from @channel)
    t_bool         chanWarned;
    long           channel;           // @channel, from 1
    long           syncmode;          // @syncmode: 0 = ms, 1 = phase

    double         fval[kNumParams];  // float parameter storage
    long           lval[kNumParams];  // long and flag parameter storage

    t_bool         rateSignal;        // inlet 2 has a signal cord
    void          *dumpout;
} t_softkut;

static t_class *softkut_class = NULL;

// ---------------------------------------------------------------------------
// parameters
// ---------------------------------------------------------------------------
// Check, store and send one parameter value (attribute units). A clamped value
// is stored clamped, so getattr reports what the engine uses.
static void applyParam(t_softkut *x, int i, double user)
{
    const Param &p = g_params[i];
    user = softkut::paramInput(p, user);
    float eng;
    if (!softkut::checkValue((t_object *)x, p.name, p.id, user, x->engine->getSampleRate(), &eng))
        return;
    const double stored = softkut::toUser(p.id, eng);
    if (p.kind == kFloat) x->fval[i] = stored;
    else                  x->lval[i] = (long)std::lround(stored);
    if (!x->engine->push(softkut::Command{p.id, 0, 0, eng}))
        object_warn((t_object *)x, "%s: command queue full, dropped", p.name);
}

t_max_err softkut_param_set(t_softkut *x, t_object *attr, long argc, t_atom *argv)
{
    const int i = softkut::paramIndex((t_symbol *)object_method(attr, gensym("getname")));
    if (i < 0 || argc < 1 || !argv) return MAX_ERR_GENERIC;
    applyParam(x, i, atom_getfloat(argv));
    return MAX_ERR_NONE;
}

// every parameter to its default, through the attribute so the inspector follows
static void applyDefaults(t_softkut *x)
{
    for (int i = 0; i < kNumParams; ++i) {
        applyParam(x, i, g_params[i].def);
        object_attr_touch((t_object *)x, softkut::paramSym(i));
    }
}

// ---------------------------------------------------------------------------
// buffer~ and channel attributes
// ---------------------------------------------------------------------------
t_max_err softkut_buffer_set(t_softkut *x, t_object *attr, long argc, t_atom *argv)
{
    if (argc < 1 || atom_gettype(argv) != A_SYM) return MAX_ERR_GENERIC;
    x->vbufname[0] = atom_getsym(argv);
    x->chanWarned  = false;
    softkut::ensureVbuf(x, 0);
    return MAX_ERR_NONE;
}

t_max_err softkut_channel_set(t_softkut *x, t_object *attr, long argc, t_atom *argv)
{
    if (argc < 1) return MAX_ERR_GENERIC;
    const long c = atom_getlong(argv);
    if (c < 1) {
        object_error((t_object *)x, "channel: %ld must be 1 or more", c);
        return MAX_ERR_GENERIC;
    }
    x->channel    = c;
    x->vchan[0]   = c - 1;
    x->chanWarned = false;
    return MAX_ERR_NONE;
}

t_max_err softkut_syncmode_set(t_softkut *x, t_object *attr, long argc, t_atom *argv)
{
    if (argc < 1) return MAX_ERR_GENERIC;
    x->syncmode = atom_getlong(argv) ? 1 : 0;
    x->engine->setSyncPhase(0, x->syncmode == 1);
    return MAX_ERR_NONE;
}

// "set <name> [<channel>]", as groove~'s set
void softkut_set(t_softkut *x, t_symbol *s, long argc, t_atom *argv)
{
    if (argc < 1 || atom_gettype(argv) != A_SYM) {
        object_error((t_object *)x, "set: expected <buffer~ name> [<channel>]");
        return;
    }
    if (argc > 1 && object_attr_setlong(x, gensym("channel"), atom_getlong(argv + 1)) != MAX_ERR_NONE)
        return;
    object_attr_setsym(x, gensym("buffer"), atom_getsym(argv));
    if (!buffer_ref_getobject(x->vbuf[0]))
        object_warn((t_object *)x, "set: no buffer~ named %s", atom_getsym(argv)->s_name);
}

// ---------------------------------------------------------------------------
// transport messages
// ---------------------------------------------------------------------------
static void push(t_softkut *x, const char *msg, CmdId id, float value)
{
    if (!x->engine->push(softkut::Command{id, 0, 0, value}))
        object_warn((t_object *)x, "%s: command queue full, dropped", msg);
}

// "play 0/1", "rec 0/1"
static void flagMessage(t_softkut *x, t_symbol *s, long argc, t_atom *argv, CmdId id)
{
    if (argc < 1) { object_error((t_object *)x, "%s: expected 0 or 1", s->s_name); return; }
    push(x, s->s_name, id, atom_getfloat(argv) != 0.0 ? 1.f : 0.f);
}

void softkut_play(t_softkut *x, t_symbol *s, long argc, t_atom *argv)
{ flagMessage(x, s, argc, argv, CmdId::PlayFlag); }

void softkut_rec(t_softkut *x, t_symbol *s, long argc, t_atom *argv)
{ flagMessage(x, s, argc, argv, CmdId::RecFlag); }

// record one loop pass from the next cut or loop wrap (sets rec itself)
void softkut_reconce(t_softkut *x)
{ push(x, "reconce", CmdId::RecOnceFlag, 1.f); }

// play 0 and rec 0 in one command
void softkut_stop(t_softkut *x)
{ push(x, "stop", CmdId::Stop, 0.f); }

void softkut_position(t_softkut *x, double ms)
{
    float sec;
    if (!softkut::checkValue((t_object *)x, "position", CmdId::Position, ms,
                             x->engine->getSampleRate(), &sec)) return;
    push(x, "position", CmdId::Position, sec);
}

// stop and restore every parameter default; @buffer and @channel are kept
void softkut_reset(t_softkut *x)
{
    if (!x->engine->reset()) object_warn((t_object *)x, "reset: command queue full, dropped");
    applyDefaults(x);
    object_attr_setlong(x, gensym("syncmode"), 0);
}

// "info <play> <rec> <state> <position> <start> <end>" out the dumpout, times
// in ms. start and end are the loop the engine plays: @loopend 0 reports the
// buffer's end, which getattr cannot.
void softkut_poll(t_softkut *x)
{
    const softkut::VoiceInfo vi = x->engine->getVoiceInfo(0);
    t_atom a[6];
    atom_setlong (a + 0, vi.play);
    atom_setlong (a + 1, vi.rec);
    atom_setlong (a + 2, vi.state);
    atom_setfloat(a + 3, vi.bufferSec * 1000.0);
    atom_setfloat(a + 4, vi.startSec * 1000.0);
    atom_setfloat(a + 5, vi.endSec * 1000.0);
    outlet_anything(x->dumpout, gensym("info"), 6, a);
}

// a number in inlet 2 sets @rate; inlet 1 takes no numbers
void softkut_float(t_softkut *x, double f)
{
    if (proxy_getinlet((t_object *)x) == 1)
        object_attr_setfloat(x, gensym("rate"), f);
    else
        object_error((t_object *)x, "inlet 1 takes a signal or messages; send the rate to inlet 2");
}

void softkut_int(t_softkut *x, long n) { softkut_float(x, (double)n); }

// ---------------------------------------------------------------------------
// perform / dsp
// ---------------------------------------------------------------------------
void softkut_perform64(t_softkut *x, t_object *dsp64, double **ins, long nins,
                       double **outs, long nouts, long vec, long flags, void *usr)
{
    const double *recIn[1]  = {ins[0]};
    const double *rateIn[1] = {x->rateSignal ? ins[1] : NULL};
    double       *audio[1]  = {outs[0]};
    double       *sync[1]   = {outs[1]};

    softkut::BufferView    views[1];
    softkut::BufferLocks<1> locks;
    softkut::lockBuffers(x, 1, views, locks);
    x->engine->process(recIn, audio, (int)vec, views, sync, rateIn);
    softkut::releaseBuffers(x, 1, views, locks);
    if (x->syncmode == 0) softkut::syncToMs(sync, 1, vec);
}

void softkut_dsp64(t_softkut *x, t_object *dsp64, short *count, double srate,
                   long maxvectorsize, long flags)
{
    x->engine->setSampleRate(srate);
    x->rateSignal = count[1] != 0;
    softkut::ensureVbuf(x, 0);
    object_method(dsp64, gensym("dsp_add64"), x, (method)softkut_perform64, 0, NULL);
}

// ---------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------
void softkut_dblclick(t_softkut *x)
{
    t_buffer_obj *b = x->vbuf[0] ? buffer_ref_getobject(x->vbuf[0]) : NULL;
    if (b) buffer_view(b);
}

t_max_err softkut_notify(t_softkut *x, t_symbol *s, t_symbol *msg, void *sender, void *data)
{
    if (x->vbuf[0]) buffer_ref_notify(x->vbuf[0], s, msg, sender, data);
    return MAX_ERR_NONE;
}

void softkut_assist(t_softkut *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) {
        snprintf_zero(s, 256, a == 0 ? "(signal) Record input, messages"
                                     : "(signal/float) Rate");
    } else if (a == 0) {
        snprintf_zero(s, 256, "(signal) Audio");
    } else if (a == 1) {
        snprintf_zero(s, 256, x->syncmode ? "(signal) Play head, 0-1 across the loop"
                                          : "(signal) Play head, ms in the buffer~");
    } else {
        snprintf_zero(s, 256, "(list) info, attribute values");
    }
}

void *softkut_new(t_symbol *s, long argc, t_atom *argv)
{
    t_softkut *x = (t_softkut *)object_alloc(softkut_class);
    if (!x) return NULL;

    dsp_setup((t_pxobject *)x, 2);                 // record input, rate
    x->dumpout = outlet_new(x, NULL);              // created first: rightmost
    object_obex_store(x, gensym("dumpout"), (t_object *)x->dumpout);
    outlet_new(x, "signal");                       // sync
    outlet_new(x, "signal");                       // audio

    x->engine     = new t_engine();
    x->nvoices    = 1;
    x->vbuf[0]    = NULL;
    x->vbufname[0] = NULL;
    x->vchan[0]   = 0;
    x->channel    = 1;
    x->chanWarned = false;
    x->syncmode   = 0;
    x->rateSignal = false;
    applyDefaults(x);

    // [softkut~ <buffer~> @attr ...]: one plain argument
    const long nargs = attr_args_offset((short)argc, argv);
    if (nargs > 0) {
        if (atom_gettype(argv) == A_SYM) x->vbufname[0] = atom_getsym(argv);
        else object_error((t_object *)x, "first argument must be a buffer~ name");
    }
    if (nargs > 1)
        object_warn((t_object *)x, "extra arguments ignored: softkut~ is one voice, use one object per voice");
    attr_args_process(x, (short)argc, argv);

    x->ob.z_misc |= Z_NO_INPLACE;
    return x;
}

void softkut_free(t_softkut *x)
{
    dsp_free((t_pxobject *)x);
    if (x->engine) delete x->engine;
    if (x->vbuf[0]) object_free(x->vbuf[0]);
}

// ---------------------------------------------------------------------------
extern "C" void ext_main(void *r)
{
    t_class *c = class_new("softkut~", (method)softkut_new, (method)softkut_free,
                           (long)sizeof(t_softkut), 0L, A_GIMME, 0);

    class_addmethod(c, (method)softkut_play,     "play",     A_GIMME, 0);
    class_addmethod(c, (method)softkut_rec,      "rec",      A_GIMME, 0);
    class_addmethod(c, (method)softkut_reconce,  "reconce",  0);
    class_addmethod(c, (method)softkut_stop,     "stop",     0);
    class_addmethod(c, (method)softkut_position, "position", A_FLOAT, 0);
    class_addmethod(c, (method)softkut_reset,    "reset",    0);
    class_addmethod(c, (method)softkut_poll,     "poll",     0);
    class_addmethod(c, (method)softkut_set,      "set",      A_GIMME, 0);
    class_addmethod(c, (method)softkut_float,    "float",    A_FLOAT, 0);
    class_addmethod(c, (method)softkut_int,      "int",      A_LONG, 0);

    class_addmethod(c, (method)softkut_dsp64,    "dsp64",    A_CANT, 0);
    class_addmethod(c, (method)softkut_assist,   "assist",   A_CANT, 0);
    class_addmethod(c, (method)softkut_dblclick, "dblclick", A_CANT, 0);
    class_addmethod(c, (method)softkut_notify,   "notify",   A_CANT, 0);
    class_addmethod(c, (method)object_obex_dumpout, "dumpout", A_CANT, 0);

    CLASS_ATTR_SYM(c, "buffer", 0, t_softkut, vbufname);
    CLASS_ATTR_ACCESSORS(c, "buffer", NULL, (method)softkut_buffer_set);
    CLASS_ATTR_LABEL(c, "buffer", 0, "buffer~ name");
    CLASS_ATTR_CATEGORY(c, "buffer", 0, "Buffer");
    CLASS_ATTR_SAVE(c, "buffer", 0);

    CLASS_ATTR_LONG(c, "channel", 0, t_softkut, channel);
    CLASS_ATTR_ACCESSORS(c, "channel", NULL, (method)softkut_channel_set);
    CLASS_ATTR_MIN(c, "channel", 0, "1");
    CLASS_ATTR_LABEL(c, "channel", 0, "buffer~ channel");
    CLASS_ATTR_CATEGORY(c, "channel", 0, "Buffer");
    CLASS_ATTR_SAVE(c, "channel", 0);

    CLASS_ATTR_LONG(c, "syncmode", 0, t_softkut, syncmode);
    CLASS_ATTR_ACCESSORS(c, "syncmode", NULL, (method)softkut_syncmode_set);
    CLASS_ATTR_ENUMINDEX2(c, "syncmode", 0, "ms", "phase");
    CLASS_ATTR_LABEL(c, "syncmode", 0, "Play head output");
    CLASS_ATTR_CATEGORY(c, "syncmode", 0, "Buffer");
    CLASS_ATTR_SAVE(c, "syncmode", 0);

    for (int i = 0; i < kNumParams; ++i) {
        const Param &p = g_params[i];
        const long off = p.kind == kFloat ? (long)(calcoffset(t_softkut, fval) + i * sizeof(double))
                                          : (long)(calcoffset(t_softkut, lval) + i * sizeof(long));
        class_addattr(c, attr_offset_new(p.name, p.kind == kFloat ? gensym("float64") : gensym("long"),
                                         0, NULL, (method)softkut_param_set, off));
        softkut::decorateParam(c, p);
    }

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    softkut_class = c;
}
