"""Generate help/softkut~.maxhelp and patchers/softkut.view.maxpat with py2max.

Run from the repository root:  uv run python scripts/make_help.py

Boxes are placed in rows with widths estimated from their text. The script
fails if any two boxes overlap, if a cord names a port its box does not have,
or if py2max's lint reports an error.

py2max takes port counts from a per-class table that ignores arguments
(`route a b` gets 2 outlets, `t f f` gets 1), so objects whose ports depend on
their arguments pass explicit counts here.
"""
import sys

from py2max import Box, Patcher, lint
from py2max.core.common import Rect

TAB_W = 720.0
VIEW_W, VIEW_H = 520.0, 180.0
GAP = 8.0
CHAR_W = 7.0          # conservative for Arial 12: overestimates rather than overlaps
ROW_H = 22.0


def text_w(text, minimum=34.0):
    return max(minimum, CHAR_W * len(text) + 16.0)


def comment_h(text, w):
    per_line = max(1, int((w - 10) / 6.4))
    lines = 0
    for para in text.split("\n"):
        lines += max(1, -(-len(para) // per_line))
    return 6.0 + 15.0 * lines


class Sheet:
    """A py2max patcher with row-based placement and port bookkeeping."""

    def __init__(self, patcher):
        self.p = patcher
        self.x = 15.0
        self.y = 12.0
        self.row_h = 0.0

    # ---- placement --------------------------------------------------------
    def at(self, x, y):
        self.x, self.y, self.row_h = x, y, 0.0
        return self

    def newline(self, extra=0.0):
        self.y += self.row_h + GAP + extra
        self.x = self.left
        self.row_h = 0.0
        return self

    def column(self, x, y):
        self.left = x
        return self.at(x, y)

    def _rect(self, w, h):
        r = [self.x, self.y, w, h]
        self.x += w + GAP
        self.row_h = max(self.row_h, h)
        return r

    # ---- boxes ------------------------------------------------------------
    def obj(self, text, ins, outs, w=None):
        """newobj with explicit ports: `outs` is the outlet type list."""
        return self.p.add_textbox(text, numinlets=ins, numoutlets=len(outs),
                                  outlettype=outs, patching_rect=self._rect(w or text_w(text), ROW_H))

    def msg(self, text, w=None):
        return self.p.add_message(text, patching_rect=self._rect(w or text_w(text), ROW_H))

    def comment(self, text, w, size=None):
        h = comment_h(text, w) if not size else size + 12.0
        kw = {"fontsize": size} if size else {}
        return self.p.add_comment(text, patching_rect=self._rect(w, h), **kw)

    def ui(self, maxclass, w, h, ins, outs, **attrs):
        box = Box(id=self.p.get_id(maxclass), maxclass=maxclass, numinlets=ins,
                  numoutlets=len(outs), outlettype=outs or None,
                  patching_rect=self._rect(w, h), parameter_enable=0, **attrs)
        return self.p.add_box(box)

    def flonum(self, w=60.0):
        return self.ui("flonum", w, ROW_H, 1, ["", "bang"])

    def waveform(self, w, h):
        return self.ui("waveform~", w, h, 5, ["float", "float", "float", "float", "list", ""],
                       setmode=1, allowdrag=0)

    def wire(self, src, dst, outlet=0, inlet=0):
        return self.p.add_line(src, dst, inlet=inlet, outlet=outlet)

    def to(self, dst, *srcs):
        for s in srcs:
            self.wire(s, dst)


# ---- layout engine ------------------------------------------------------------------
LEVEL_GAP = 26.0      # vertical space between OGDF levels, for the cords
NODE_GAP = 16.0       # minimum horizontal gap between boxes on one level


def relayout(patcher, boxes, x0, y0):
    """Lay out `boxes` (and the cords among them) with OGDF's Sugiyama layout.

    Sugiyama assigns each box to a level along the cords and orders each level
    to minimize crossings. OGDF is called directly rather than through py2max's
    `graph:ogdf-sugiyama` manager, which rescales the result to a fixed span and
    leaves the crossing minimization unseeded and multi-run (a different layout
    on every run). Levels become rows from (x0, y0) down; within a row the OGDF
    order is kept and boxes are pushed apart to NODE_GAP. Other boxes and
    cords to them are untouched.
    """
    import ogdf

    ids = {b.id for b in boxes}
    graph = ogdf.Graph()
    attrs = ogdf.GraphAttributes(graph, ogdf.NODE_GRAPHICS | ogdf.EDGE_GRAPHICS)
    node = {}
    for b in boxes:
        node[b.id] = graph.new_node()
        attrs.set_width(node[b.id], b.patching_rect[2])
        attrs.set_height(node[b.id], b.patching_rect[3])
    for ln in patcher._lines:
        (src, _), (dst, _) = ln.source, ln.destination
        if src in ids and dst in ids:
            graph.new_edge(node[src], node[dst])
    ogdf.set_seed(1)
    sugiyama = ogdf.SugiyamaLayout()
    sugiyama.set_runs(1)          # one seeded run: the same layout every time
    sugiyama.call(attrs)

    levels = {}
    for b in boxes:
        levels.setdefault(round(attrs.y(node[b.id]), 3), []).append(b)
    y = y0
    for key in sorted(levels):
        row = sorted(levels[key], key=lambda b: attrs.x(node[b.id]))
        x = x0
        height = 0.0
        for b in row:
            w, h = b.patching_rect[2], b.patching_rect[3]
            left = max(x, x0 + attrs.x(node[b.id]) - w / 2.0)
            b.patching_rect = Rect(left, y, w, h)
            x = left + w + NODE_GAP
            height = max(height, h)
        y += height + LEVEL_GAP


# ---- the voice view: waveform, play head, loop window, state ------------------
VIEW_FILE = "softkut.view.maxpat"


def make_view():
    """patchers/softkut.view.maxpat, a bpatcher abstraction.

    Arguments: #1 buffer~ name, #2 channel (from 1). Inlet 1: softkut~'s
    dumpout. Inlet 2: its sync outlet (play head in ms), sampled every 30 ms
    for the cursor. Outlet: back to softkut~, sending `poll` every 100 ms for
    the loop window and state. The visible area holds the displays; the logic
    sits below it.
    """
    v = Sheet(Patcher("patchers/" + VIEW_FILE))
    v.p.rect = [100.0, 100.0, VIEW_W + 40, 560.0]
    v.column(0.0, 0.0)
    label = v.msg("channel", w=150.0)
    state = v.msg("stopped", w=90.0)
    v.comment("play head = moving cursor; loop window = strip below", 260.0)
    v.newline(-4.0)
    wave = v.waveform(VIEW_W, 112.0)
    v.newline(-6.0)
    loop = v.waveform(VIEW_W, 26.0)

    # logic, outside the visible area; placed by relayout() below
    n_display = len(v.p._boxes)
    v.column(0.0, VIEW_H + 40.0)
    inl = v.obj("inlet", 0, [""])
    sig = v.obj("inlet", 0, [""])
    v.newline()

    # sync signal (ms) -> a 15 ms selection at the play head
    snap = v.obj("snapshot~ 30", 2, ["float"])
    tf = v.obj("t f f", 1, ["float", "float"])
    pl = v.obj("+ 15.", 2, ["float"])
    pk = v.obj("pack 0. 0.", 2, [""])
    v.wire(sig, snap)
    v.wire(snap, tf)
    v.wire(tf, pl, outlet=1)
    v.wire(pl, pk, inlet=1)
    v.wire(tf, pk, outlet=0, inlet=0)
    v.wire(pk, wave, inlet=2)

    # info <play> <rec> <state> <position> <start> <end> -> loop window + state
    ri = v.obj("route info", 2, ["", ""])
    up = v.obj("unpack 0 0 0 0. 0. 0.", 1, ["int", "int", "int", "float", "float", "float"])
    lp = v.obj("pack 0. 0.", 2, [""])
    sel = v.obj("sel 0 1 2 3", 1, ["bang", "bang", "bang", "bang", ""])
    v.wire(inl, ri)
    v.wire(ri, up)
    v.wire(up, lp, outlet=5, inlet=1)
    v.wire(up, lp, outlet=4, inlet=0)
    v.wire(lp, loop, inlet=2)
    v.wire(up, sel, outlet=2)
    for i, word in enumerate(["stopped", "playing", "recording", "overdub"]):
        m = v.msg("set " + word)
        v.wire(sel, m, outlet=i)
        v.wire(m, state)

    # on load: point both displays at the buffer~ channel, label, start polling.
    # #n is substituted in message text, not in attribute values, so the
    # buffer~ is set by message; it is re-sent after 500 ms in case the buffer~
    # loads after the view.
    lb = v.obj("loadbang", 1, ["bang"])
    dl = v.obj("del 500", 2, ["bang"])
    met = v.obj("metro 100", 2, ["bang"])
    setb = v.msg("set #1 #2")
    lab = v.msg("set channel #2")
    poll = v.msg("poll")
    v.wire(lb, setb)
    v.wire(lb, dl)
    v.wire(dl, setb)
    v.wire(lb, lab)
    v.wire(lb, met)
    v.wire(met, poll)
    v.wire(setb, wave)
    v.wire(setb, loop)
    v.wire(lab, label)
    out = v.obj("outlet", 1, [])
    v.wire(poll, out)
    # Max numbers inlets by x position, not creation order: pin the two inlets
    # in order on a row of their own and lay out the rest below them
    y0 = VIEW_H + 40.0
    inl.patching_rect = Rect(0.0, y0, inl.patching_rect[2], ROW_H)
    sig.patching_rect = Rect(inl.patching_rect[2] + 40.0, y0, sig.patching_rect[2], ROW_H)
    logic = [b for b in v.p._boxes[n_display:] if b not in (inl, sig)]
    relayout(v.p, logic, 0.0, y0 + ROW_H + LEVEL_GAP)
    return v.p


# softkut~ ports: inlets record input, rate; outlets audio, sync, dumpout
SK_INS = 2
SK_OUTS = ["signal", "signal", ""]
AUDIO, SYNC, DUMP = 0, 1, 2


def view(s, sk, buf, chan):
    """A softkut.view bpatcher on one softkut~: dumpout -> inlet 1, sync
    outlet -> inlet 2, poll back out."""
    box = Box(id=s.p.get_id("bpatcher"), maxclass="bpatcher", name=VIEW_FILE,
              args=[buf, chan], numinlets=2, numoutlets=1, outlettype=[""],
              patching_rect=s._rect(VIEW_W, VIEW_H), offset=[0.0, 0.0], viewvisibility=1,
              bgmode=0, border=0, clickthrough=0, enablehscroll=0, enablevscroll=0,
              lockeddragscroll=0)
    s.p.add_box(box)
    s.wire(sk, box, outlet=DUMP)
    s.wire(sk, box, outlet=SYNC, inlet=1)
    s.wire(box, sk)
    return box


def attrui(s, attr, w=150.0):
    """An attrui showing and setting one softkut~ attribute."""
    return s.ui("attrui", w, ROW_H, 1, [""], attr=attr)


# ---- tab scaffolding ---------------------------------------------------------------
def header(s, title, sub):
    s.p.rect = [0.0, 26.0, TAB_W, 680.0]
    s.p.showontab = 1
    s.column(15.0, 12.0)
    s.comment(title, 640.0, size=16.0)
    s.newline()
    s.comment(sub, 660.0)
    s.newline(6.0)


def loader(s, file, buf_text):
    lb = s.obj("loadbang", 1, ["bang"])
    s.newline()
    rp = s.msg("replace " + file)
    s.newline()
    buf = s.obj(buf_text, 1, ["float", "bang"])
    s.wire(lb, rp)
    s.wire(rp, buf)
    return buf


def softkut_row(s, texts, y):
    """One softkut~ per text, side by side, each into its own *~ 0.5 and one
    ezdac~: the first to the left channel, the second to the right. A single
    voice goes to both."""
    s.column(15.0, y)
    sks = [s.obj(t, SK_INS, SK_OUTS) for t in texts]
    s.x = max(s.x, 480.0)
    gains = [s.obj("*~ 0.5", 2, ["signal"]) for _ in sks]
    dac = s.ui("ezdac~", 45.0, 45.0, 2, [])
    s.comment("on/off", 50.0)
    for sk, g in zip(sks, gains):
        s.wire(sk, g, outlet=AUDIO)
    s.wire(gains[0], dac)
    s.wire(gains[-1], dac, inlet=1)
    s.newline()
    return sks


def bottom(s, *ys):
    return max(ys) + GAP


# ---- tabs ----------------------------------------------------------------------------
def tab_basic(s):
    header(s, "Loop any buffer~",
           "softkut~ is one softcut voice. Its parameters are attributes: set them in the "
           "object box (@fade 20), by message (rate 0.5), in the inspector, or with attrui. "
           "Times are ms of buffer material. The view at the bottom shows the buffer~, the "
           "moving play head and the loop window.")
    top = s.y
    s.column(15.0, top)
    play = s.msg("play 1")
    stop = s.msg("play 0")
    s.comment("the loop is the whole buffer~ until loopend is set", 300.0)
    s.newline()
    win = s.msg("loopstart 200, loopend 1200")
    s.newline()
    full = s.msg("loopstart 0, loopend 0")
    s.comment("loopend 0 = the buffer~'s end", 190.0)
    s.newline()
    rates = [s.msg("rate 1"), s.msg("rate 0.5"), s.msg("rate -1")]
    s.comment("0.5 = octave down, -1 = reverse", 200.0)
    s.newline()
    ui_rate = attrui(s, "rate", 130.0)
    ui_fade = attrui(s, "fade", 130.0)
    s.comment("attrui sets and shows an attribute", 170.0)
    s.newline()
    pos = s.msg("position 600")
    s.comment("jump the play head (ms), with a crossfade", 260.0)
    s.newline()
    rs = s.msg("reset")
    s.comment("reset: stop, and restore every attribute default. @buffer and @channel are "
              "kept.", 360.0)
    s.newline()
    left_bottom = s.y

    s.column(480.0, top)
    loader(s, "cello-f2.aif", "buffer~ skh_basic")
    s.newline()
    s.comment("replace sizes the buffer~ to the file. Every frame is used; the length need "
              "not be a power of two.", 220.0)
    s.newline()

    sk, = softkut_row(s, ["softkut~ skh_basic @fade 20"], bottom(s, left_bottom, s.y))
    s.to(sk, play, stop, win, full, *rates, ui_rate, ui_fade, pos, rs)
    view(s, sk, "skh_basic", 1)


def tab_channels(s):
    header(s, "Multichannel buffer~ (stereo)",
           "Each softkut~ reads and writes one channel, @channel (from 1). A stereo loop is "
           "two objects on one buffer~: started by the same message, they stay together. "
           "Here the left channel is bells and the right is bass.")
    top = s.y
    s.column(15.0, top)
    play = s.msg("loopend 2000, play 1")
    stop = s.msg("play 0")
    s.comment("to both objects", 120.0)
    s.newline()
    one = s.msg("channel 1")
    s.comment("the right object reads channel 1 too", 240.0)
    s.newline()
    two = s.msg("channel 2")
    s.comment("back to channel 2", 150.0)
    s.newline()
    bad = s.msg("channel 3")
    s.comment("no channel 3: the voice falls silent, with one console warning", 330.0)
    s.newline()
    det = s.msg("rate 0.5")
    s.comment("bass an octave down", 150.0)
    s.newline()
    rs = s.msg("reset")
    s.comment("start over", 100.0)
    s.newline()
    left_bottom = s.y

    s.column(480.0, top)
    loader(s, "softkut-stereo.wav", "buffer~ skh_stereo")
    s.newline()
    s.comment("softkut-stereo.wav is in the package's media folder. If Max reports it cannot "
              "open the file, restart Max so it finds the folder.", 220.0)
    s.newline()

    left, right = softkut_row(s, ["softkut~ skh_stereo @channel 1",
                                  "softkut~ skh_stereo @channel 2"],
                              bottom(s, left_bottom, s.y))
    for sk in (left, right):
        s.to(sk, play, stop, rs)
    s.to(right, one, two, bad, det)
    view(s, left, "skh_stereo", 1)
    s.newline()
    view(s, right, "skh_stereo", 2)
    s.newline()
    s.comment("The views draw channels 1 and 2. After channel 1, the right view's play head "
              "still moves over the channel 2 waveform, but the object reads channel 1.", 520.0)
    s.p.rect = [0.0, 26.0, TAB_W, s.y + 60.0]


def tab_samplerate(s):
    header(s, "The buffer~ sample rate",
           "Like groove~, softkut~ plays a buffer~ at its own sample rate: rate 1 is the "
           "recorded speed, whatever the DSP rate.")
    top = s.y
    s.column(15.0, top)
    play = s.msg("loopstart 0, loopend 1000, play 1")
    s.newline()
    stop = s.msg("play 0")
    s.newline()
    s.comment("Loop times are ms at the buffer~'s rate. After sr 22050, loopend 1000 covers "
              "22050 frames, so the loop window in the view doubles.", 380.0)
    s.newline()
    rs = s.msg("reset")
    s.comment("start over", 100.0)
    s.newline()
    left_bottom = s.y

    s.column(480.0, top)
    lb = s.obj("loadbang", 1, ["bang"])
    s.newline()
    rp = s.msg("replace cello-f2.aif")
    s.newline()
    rates = [s.msg("sr 44100"), s.msg("sr 22050"), s.msg("sr 88200")]
    s.newline()
    buf = s.obj("buffer~ skh_sr", 1, ["float", "bang"])
    s.newline()
    s.comment("sr relabels the buffer~'s rate: 22050 plays an octave down, 88200 an octave up.",
              220.0)
    s.newline()
    s.wire(lb, rp)
    s.to(buf, rp, *rates)

    sk, = softkut_row(s, ["softkut~ skh_sr"], bottom(s, left_bottom, s.y))
    s.to(sk, play, stop, rs)
    view(s, sk, "skh_sr", 1)


def tab_record(s):
    header(s, "Record and overdub",
           "The left inlet is the record input. prelevel sets how much of the old content "
           "survives each pass. Watch the waveform fill in.")
    top = s.y
    s.column(15.0, top)
    go = s.msg("loopend 2000, rec 1, play 1")
    s.newline()
    off = s.msg("rec 0")
    s.comment("stop recording, keep looping", 200.0)
    s.newline()
    once = s.msg("reconce")
    s.comment("record one pass, from the next loop start", 260.0)
    s.newline()
    pre = [s.msg("prelevel 0"), s.msg("prelevel 0.7"), s.msg("prelevel 1")]
    s.newline()
    s.comment("prelevel: 0 overwrites, 1 sums on top, between decays", 380.0)
    s.newline()
    color = s.msg("recshape raised, softclip 1")
    clean = s.msg("recshape linear, softclip 0")
    s.newline()
    s.comment("softcut's colored record path (inverted, 1.2x soft clip), or the "
              "transparent default", 380.0)
    s.newline()
    stp = s.msg("stop")
    s.comment("stop = play 0 and rec 0", 200.0)
    s.newline()
    rs = s.msg("reset")
    s.comment("start over (the recording stays in the buffer~)", 300.0)
    s.newline()
    left_bottom = s.y

    s.column(480.0, top)
    freq = s.flonum()
    s.comment("pitch", 50.0)
    s.newline()
    osc = s.obj("cycle~ 220", 2, ["signal"])
    lfo = s.obj("cycle~ 3", 2, ["signal"])
    s.newline()
    amp = s.obj("*~", 2, ["signal"])
    s.comment("input: a pulsing tone", 150.0)
    s.newline()
    clr = s.msg("clear")
    s.newline()
    buf = s.obj("buffer~ skh_rec 3000", 1, ["float", "bang"])
    s.newline()
    s.wire(freq, osc)
    s.wire(osc, amp)
    s.wire(lfo, amp, inlet=1)
    s.wire(clr, buf)

    sk, = softkut_row(s, ["softkut~ skh_rec"], bottom(s, left_bottom, s.y))
    s.to(sk, go, off, once, *pre, color, clean, stp, rs)
    s.wire(amp, sk)
    view(s, sk, "skh_rec", 1)


def tab_rate(s):
    header(s, "Rate as a signal",
           "The right inlet takes the rate as a signal, per sample, in place of @rate. Here "
           "an LFO bends the pitch around rate 1. A number in the right inlet sets @rate.")
    top = s.y
    s.column(15.0, top)
    play = s.msg("loopend 2000, play 1")
    stop = s.msg("play 0")
    s.newline()
    s.comment("Delete the cord into the right inlet to return to @rate. rateslew smooths the "
              "signal too: set it to 0 for audio-rate modulation.", 380.0)
    s.newline()
    sl0 = s.msg("rateslew 0")
    sl1 = s.msg("rateslew 50")
    s.newline()
    rs = s.msg("reset")
    s.comment("start over", 100.0)
    s.newline()
    left_bottom = s.y

    s.column(480.0, top)
    loader(s, "cello-f2.aif", "buffer~ skh_rate")
    s.newline()
    lf = s.flonum()
    s.comment("LFO Hz", 60.0)
    s.newline()
    dp = s.flonum()
    s.comment("depth", 50.0)
    s.newline()
    lfo = s.obj("cycle~ 0.5", 2, ["signal"])
    s.newline()
    dep = s.obj("*~ 0.1", 2, ["signal"])
    s.newline()
    off = s.obj("+~ 1.", 2, ["signal"])
    s.comment("rate = 1 + depth x LFO", 160.0)
    s.newline()
    s.wire(lf, lfo)
    s.wire(dp, dep, inlet=1)
    s.wire(lfo, dep)
    s.wire(dep, off)

    sk, = softkut_row(s, ["softkut~ skh_rate"], bottom(s, left_bottom, s.y))
    s.to(sk, play, stop, sl0, sl1, rs)
    s.wire(off, sk, inlet=1)
    view(s, sk, "skh_rate", 1)


def tab_playhead(s):
    header(s, "Play head, info and attributes",
           "The middle outlet is the play head as a signal: ms in the buffer~, or 0-1 across "
           "the loop with @syncmode phase. The right outlet answers poll with an info list, "
           "and getattr messages with the attribute's value.")
    top = s.y
    s.column(15.0, top)
    play = s.msg("loopstart 500, loopend 2500, play 1")
    s.newline()
    ms = s.msg("syncmode ms")
    ph = s.msg("syncmode phase")
    s.newline()
    poll = s.msg("poll")
    s.comment("info: play, rec, state (0 stopped, 1 playing, 2 recording, 3 overdub), play "
              "head, loop start, loop end; times in ms", 380.0)
    s.newline()
    gets = [s.msg("getrate"), s.msg("getloopend"), s.msg("getbuffer")]
    s.comment("getattr replies", 120.0)
    s.newline()
    rs = s.msg("reset")
    s.comment("start over", 100.0)
    s.newline()
    left_bottom = s.y

    s.column(480.0, top)
    loader(s, "cello-f2.aif", "buffer~ skh_ph")
    s.newline()

    sk, = softkut_row(s, ["softkut~ skh_ph"], bottom(s, left_bottom, s.y))
    s.to(sk, play, ms, ph, poll, *gets, rs)
    s.column(15.0, s.y)
    snap = s.obj("snapshot~ 50", 2, ["float"])
    sy = s.flonum(80.0)
    s.comment("play head", 70.0)
    s.wire(sk, snap, outlet=SYNC)
    s.wire(snap, sy)
    s.newline()
    pre = s.obj("prepend set", 2, [""])
    last = s.msg("", w=400.0)
    s.comment("latest reply", 90.0)
    s.wire(sk, pre, outlet=DUMP)
    s.wire(pre, last)
    s.newline()
    view(s, sk, "skh_ph", 1)


def tab_limits(s):
    header(s, "Safe values",
           "softcut checks no values. softkut~ clamps those that would crash it or write NaN "
           "into the buffer~, and posts a warning to the Max console. getattr then reports "
           "the clamped value.")
    rows = [("rate 100", "rate is limited to -64..64"),
            ("postrq -1", "filter rq is at least 0.01"),
            ("prelevel 1.5", "rec and pre levels are 0..1"),
            ("loopstart -1", "positions are 0 ms or more"),
            ("recpreslew -1", "times and slews are 0 ms or more"),
            ("reset", "restore the defaults")]
    msgs = []
    for m, c in rows:
        msgs.append(s.msg(m, w=130.0))
        s.comment(c, 260.0)
        s.newline()
    sk = s.obj("softkut~ skh_limits", SK_INS, SK_OUTS)
    s.obj("buffer~ skh_limits 1000", 1, ["float", "bang"])
    s.to(sk, *msgs)


# ---- mc.softkut~ ------------------------------------------------------------------
MC_OUTS = ["multichannelsignal", "multichannelsignal", ""]


def mc_row(s, text, nvoices, y):
    """mc.softkut~ into mc.mixdown~ 2 and mc.dac~, with an ezdac~ for audio
    on/off. Returns the object and an mc.unpack~ of its play heads."""
    s.column(15.0, y)
    sk = s.obj(text, 2, MC_OUTS)
    s.newline()
    mix = s.obj("mc.mixdown~ 2", 1, ["multichannelsignal"])
    g = s.obj("mc.*~ 0.3", 2, ["multichannelsignal"])
    dac = s.obj("mc.dac~", 1, [])
    s.ui("ezdac~", 45.0, 45.0, 2, [])
    s.comment("on/off", 50.0)
    s.wire(sk, mix, outlet=AUDIO)
    s.wire(mix, g)
    s.wire(g, dac)
    s.newline()
    heads = s.obj("mc.unpack~ %d" % nvoices, 1, ["signal"] * nvoices)
    route = s.obj("route info", 2, ["", ""])
    voices = s.obj("route " + " ".join(str(v + 1) for v in range(nvoices)), 1,
                   [""] * (nvoices + 1))
    s.wire(sk, heads, outlet=SYNC)
    s.wire(sk, route, outlet=DUMP)
    s.wire(route, voices)
    s.newline()
    return sk, heads, voices


def mc_view(s, sk, heads, voices, voice, buf, chan):
    """A softkut.view for one voice of an mc.softkut~: its info lines (voice
    number stripped) and its play head."""
    pre = s.obj("prepend info", 2, [""])
    s.newline()
    box = Box(id=s.p.get_id("bpatcher"), maxclass="bpatcher", name=VIEW_FILE,
              args=[buf, chan], numinlets=2, numoutlets=1, outlettype=[""],
              patching_rect=s._rect(VIEW_W, VIEW_H), offset=[0.0, 0.0], viewvisibility=1,
              bgmode=0, border=0, clickthrough=0, enablehscroll=0, enablevscroll=0,
              lockeddragscroll=0)
    s.p.add_box(box)
    s.wire(voices, pre, outlet=voice - 1)
    s.wire(pre, box)
    s.wire(heads, box, outlet=voice - 1, inlet=1)
    s.wire(box, sk)
    s.newline()
    return box


def mc_tab_basic(s):
    header(s, "Voices as channels",
           "mc.softkut~ is @chans softkut~ voices: voice v is channel v of every signal port. "
           "Attributes take one value per voice: one value sets every voice, a list sets them "
           "in order. setvalue addresses one voice.")
    top = s.y
    s.column(15.0, top)
    play = s.msg("play 1")
    stop = s.msg("play 0")
    some = s.msg("play 1 0 1")
    s.comment("voices 1 and 3", 110.0)
    s.newline()
    rl = s.msg("rate 1 0.5 -1")
    r1 = s.msg("rate 1")
    s.comment("a list per voice, or one value for all", 240.0)
    s.newline()
    sv = s.msg("setvalue 2 rate 2")
    sp = s.msg("setvalue 3 position 500")
    s.newline()
    s.comment("setvalue <voice> <attribute or message> <value>", 380.0)
    s.newline()
    gr = s.msg("getrate")
    s.comment("replies rate <one value per voice> out the right outlet", 330.0)
    s.newline()
    rs = s.msg("reset")
    s.comment("stop, restore every default", 200.0)
    s.newline()
    left_bottom = s.y

    s.column(480.0, top)
    loader(s, "cello-f2.aif", "buffer~ skh_mc")
    s.newline()

    sk, heads, voices = mc_row(s, "mc.softkut~ skh_mc @chans 3 @loopend 2000 @rate 1 0.5 -1",
                               3, bottom(s, left_bottom, s.y))
    s.to(sk, play, stop, some, rl, r1, sv, sp, gr, rs)
    s.column(15.0, s.y)
    for v in (1, 2, 3):
        mc_view(s, sk, heads, voices, v, "skh_mc", 1)
    s.p.rect = [0.0, 26.0, TAB_W, s.y + 60.0]


def mc_tab_stereo(s):
    header(s, "Stereo, and sync",
           "@channel 0 (the default) gives voice v channel v, wrapping: on a stereo buffer~, "
           "voice 1 reads the left and voice 2 the right. sync cuts one voice to another's "
           "position, sample-exact, inside one object.")
    top = s.y
    s.column(15.0, top)
    play = s.msg("loopend 2000, play 1")
    stop = s.msg("play 0")
    s.newline()
    det = s.msg("setvalue 2 rate 0.5")
    s.comment("the right channel drifts", 160.0)
    s.newline()
    same = s.msg("rate 1, sync 2 1")
    s.comment("voice 2 jumps to voice 1: locked again", 260.0)
    s.newline()
    ch = s.msg("channel 1 1")
    ch0 = s.msg("channel 0")
    s.comment("both read channel 1; back to the default", 270.0)
    s.newline()
    rs = s.msg("reset")
    s.comment("start over", 100.0)
    s.newline()
    left_bottom = s.y

    s.column(480.0, top)
    loader(s, "softkut-stereo.wav", "buffer~ skh_mcst")
    s.newline()

    sk, heads, voices = mc_row(s, "mc.softkut~ skh_mcst @chans 2", 2, bottom(s, left_bottom, s.y))
    s.to(sk, play, stop, det, same, ch, ch0, rs)
    s.column(15.0, s.y)
    for v in (1, 2):
        mc_view(s, sk, heads, voices, v, "skh_mcst", v)
    s.p.rect = [0.0, 26.0, TAB_W, s.y + 60.0]


def mc_tab_rate(s):
    header(s, "Rates as a signal",
           "The right inlet takes one rate channel per voice. Voices past its channel count "
           "use @rate. Here three LFOs bend three voices by different amounts.")
    top = s.y
    s.column(15.0, top)
    play = s.msg("loopend 2000, play 1")
    stop = s.msg("play 0")
    s.newline()
    sl = s.msg("rateslew 0")
    s.comment("rateslew smooths the signal too", 220.0)
    s.newline()
    rs = s.msg("reset")
    s.comment("start over", 100.0)
    s.newline()
    left_bottom = s.y

    s.column(480.0, top)
    loader(s, "cello-f2.aif", "buffer~ skh_mcr")
    s.newline()
    s.column(15.0, bottom(s, left_bottom, s.y))
    rates = []
    for hz, depth in ((0.3, 0.02), (0.5, 0.1), (2.0, 0.3)):
        osc = s.obj("cycle~ %g" % hz, 2, ["signal"])
        dep = s.obj("*~ %g" % depth, 2, ["signal"])
        off = s.obj("+~ 1.", 2, ["signal"])
        s.wire(osc, dep)
        s.wire(dep, off)
        rates.append(off)
        s.newline()
    pack = s.obj("mc.pack~ 3", 3, ["multichannelsignal"])
    s.comment("rate = 1 + depth x LFO, per voice", 240.0)
    for k, r in enumerate(rates):
        s.wire(r, pack, inlet=k)
    s.newline()

    sk, heads, voices = mc_row(s, "mc.softkut~ skh_mcr @chans 3", 3, s.y)
    s.to(sk, play, stop, sl, rs)
    s.wire(pack, sk, inlet=1)
    s.column(15.0, s.y)
    mc_view(s, sk, heads, voices, 3, "skh_mcr", 1)
    s.p.rect = [0.0, 26.0, TAB_W, s.y + 60.0]


def mc_tab_record(s):
    header(s, "Record one voice, play three",
           "The left inlet's channel v is voice v's record input. A mono cord feeds voice 1 "
           "only; the other voices record silence, so record with setvalue 1 rec 1.")
    top = s.y
    s.column(15.0, top)
    go = s.msg("play 1, setvalue 1 rec 1")
    s.newline()
    off = s.msg("setvalue 1 rec 0")
    s.comment("stop recording, keep looping", 200.0)
    s.newline()
    pre = s.msg("prelevel 0.7")
    s.comment("overdub, decaying", 150.0)
    s.newline()
    fb = s.msg("feedback 1 2 0.7, setvalue 2 rec 1")
    s.newline()
    s.comment("voice 2 records voice 1's output, at its own rate (feedback src dst gain)",
              400.0)
    s.newline()
    fb0 = s.msg("feedback 1 2 0, setvalue 2 rec 0")
    s.newline()
    stp = s.msg("stop")
    s.comment("every voice", 100.0)
    s.newline()
    rs = s.msg("reset")
    s.comment("start over (the recording stays in the buffer~)", 300.0)
    s.newline()
    left_bottom = s.y

    s.column(480.0, top)
    osc = s.obj("cycle~ 220", 2, ["signal"])
    lfo = s.obj("cycle~ 3", 2, ["signal"])
    s.newline()
    amp = s.obj("*~", 2, ["signal"])
    s.comment("input", 50.0)
    s.newline()
    clr = s.msg("clear")
    s.newline()
    buf = s.obj("buffer~ skh_mcrec 2000", 1, ["float", "bang"])
    s.newline()
    s.wire(osc, amp)
    s.wire(lfo, amp, inlet=1)
    s.wire(clr, buf)

    sk, heads, voices = mc_row(s, "mc.softkut~ skh_mcrec @chans 3 @rate 1 2 -0.5",
                               3, bottom(s, left_bottom, s.y))
    s.to(sk, go, off, pre, fb, fb0, stp, rs)
    s.wire(amp, sk)
    s.column(15.0, s.y)
    mc_view(s, sk, heads, voices, 1, "skh_mcrec", 1)
    s.p.rect = [0.0, 26.0, TAB_W, s.y + 60.0]


def help_root(path, tabs):
    root = Patcher(path)
    root.rect = [100.0, 100.0, TAB_W + 20, 760.0]
    root.showrootpatcherontab = 0
    root.showontab = 0
    for i, (name, build) in enumerate(tabs):
        sub = Patcher(parent=root)
        root.add_subpatcher("p " + name, patching_rect=[15.0 + 110 * i, 60.0, 100.0, 22.0],
                            patcher=sub, numinlets=0, numoutlets=0)
        build(Sheet(sub))
    return root


# ---- checks ----------------------------------------------------------------------------
def overlaps(a, b):
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    return ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah


def check(patcher, path="root"):
    errors = []
    boxes = list(patcher._boxes)
    rects = {b.id: list(b.patching_rect) for b in boxes}
    ports = {b.id: (b.numinlets, b.numoutlets) for b in boxes}
    for i, a in enumerate(boxes):
        for b in boxes[i + 1:]:
            if overlaps(rects[a.id], rects[b.id]):
                errors.append("%s: %s overlaps %s" % (path, a.id, b.id))
    for ln in patcher._lines:
        (s, o), (d, i) = ln.source, ln.destination
        if o >= ports[s][1] or i >= ports[d][0]:
            errors.append("%s: cord %s:%d -> %s:%d out of range" % (path, s, o, d, i))
    for b in boxes:
        if b.subpatcher is not None:
            errors += check(b.subpatcher, path + "/" + b.id)
    return errors


def main():
    root = help_root("help/softkut~.maxhelp",
                     [("basic", tab_basic), ("channels", tab_channels),
                      ("samplerate", tab_samplerate), ("record", tab_record), ("rate", tab_rate),
                      ("playhead", tab_playhead), ("limits", tab_limits)])
    mc_root = help_root("help/mc.softkut~.maxhelp",
                        [("basic", mc_tab_basic), ("stereo", mc_tab_stereo),
                         ("rate", mc_tab_rate), ("record", mc_tab_record)])
    view_patch = make_view()
    errors = check(root) + check(mc_root, "mc") + check(view_patch, "view")
    # the help's bpatchers declare 2 inlets and 1 outlet: the view must have
    # them, and since Max numbers them by x position, in creation order left
    # to right (the first inlet takes the dumpout, the second the sync signal)
    for cls, want in (("inlet", 2), ("outlet", 1)):
        ports = [b for b in view_patch._boxes if b.text == cls]
        if len(ports) != want:
            errors.append("view: %d %s objects, expected %d" % (len(ports), cls, want))
        xs = [b.patching_rect[0] for b in ports]
        if xs != sorted(xs) or len(set(xs)) != len(xs):
            errors.append("view: %s objects are not left to right in creation order: %s"
                          % (cls, xs))
    # py2max's lint gives a bpatcher that loads a file 0 ports, and mc.pack~ and
    # mc.unpack~ 2, whatever they declare; their cords are checked above
    # against the declared ports instead
    exempt = ("'bpatcher' has 0", "'mc.pack~' has", "'mc.unpack~' has")
    findings = [f for f in lint(root) + lint(mc_root) + lint(view_patch) if f.severity == "error"
                and not any(e in str(f) for e in exempt)]
    if errors or findings:
        sys.exit("\n".join(errors + [str(f) for f in findings]))
    view_patch.save()
    root.save()
    mc_root.save()
    print("wrote help/softkut~.maxhelp, help/mc.softkut~.maxhelp and patchers/%s: no "
          "overlaps, cords and lint clean" % VIEW_FILE)


if __name__ == "__main__":
    main()
