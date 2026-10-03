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

    Arguments: #1 buffer~ name, #2 channel (1-based), #3 voice (0-based).
    Inlet: softkut~'s report outlet. Outlet: back to softkut~ (sends
    `report 30` on load, then `poll` every 100 ms). The visible area holds the
    displays; the logic sits below it.
    """
    v = Sheet(Patcher("patchers/" + VIEW_FILE))
    v.p.rect = [100.0, 100.0, VIEW_W + 40, 560.0]
    v.column(0.0, 0.0)
    label = v.msg("voice / channel", w=150.0)
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
    v.newline()
    rt = v.obj("route phase info", 3, ["", "", ""])
    v.wire(inl, rt)
    v.newline()

    # phase <voice> <sec>  ->  a 15 ms selection at the play head
    rv = v.obj("route #3", 2, ["", ""])
    ri = v.obj("route #3", 2, ["", ""])
    v.wire(rt, rv, outlet=0)
    v.wire(rt, ri, outlet=1)
    v.newline()
    ms = v.obj("* 1000.", 2, ["float"])
    up = v.obj("unpack 0. 0 0 0. 0. 0. 0", 1,
               ["float", "int", "int", "float", "float", "float", "int"])
    v.wire(rv, ms)
    v.wire(ri, up)
    v.newline()
    tf = v.obj("t f f", 1, ["float", "float"])
    pl = v.obj("+ 15.", 2, ["float"])
    lp = v.obj("pack 0. 0.", 2, [""])
    sel = v.obj("sel 0 1 2 3", 1, ["bang", "bang", "bang", "bang", ""])
    v.wire(ms, tf)
    v.wire(tf, pl, outlet=1)
    # info: <pos play rec startMs endMs windowMs state> -> loop window + state
    v.wire(up, lp, outlet=4, inlet=1)
    v.wire(up, lp, outlet=3, inlet=0)
    v.wire(lp, loop, inlet=2)
    v.wire(up, sel, outlet=6)
    v.newline()
    pk = v.obj("pack 0. 0.", 2, [""])
    v.wire(pl, pk, inlet=1)
    v.wire(tf, pk, outlet=0, inlet=0)
    v.wire(pk, wave, inlet=2)
    for i, word in enumerate(["stopped", "playing", "recording", "overdub"]):
        m = v.msg("set " + word)
        v.wire(sel, m, outlet=i)
        v.wire(m, state)
    v.newline()

    # on load: point both displays at the buffer~ channel, label, start
    # reporting. #n is substituted in message text, not in attribute values,
    # so the buffer~ is set by message; it is re-sent after 500 ms in case the
    # buffer~ loads after the view.
    lb = v.obj("loadbang", 1, ["bang"])
    dl = v.obj("del 500", 2, ["bang"])
    met = v.obj("metro 100", 2, ["bang"])
    v.newline()
    setb = v.msg("set #1 #2")
    lab = v.msg("set voice #3 / channel #2")
    rep = v.msg("report 30")
    poll = v.msg("poll")
    v.wire(lb, setb)
    v.wire(lb, dl)
    v.wire(dl, setb)
    v.wire(lb, lab)
    v.wire(lb, rep)
    v.wire(lb, met)
    v.wire(met, poll)
    v.wire(setb, wave)
    v.wire(setb, loop)
    v.wire(lab, label)
    v.newline()
    out = v.obj("outlet", 1, [])
    v.wire(rep, out)
    v.wire(poll, out)
    relayout(v.p, v.p._boxes[n_display:], 0.0, VIEW_H + 40.0)
    return v.p


def view(s, sk, buf, chan, voice, nout):
    """A softkut.view bpatcher: softkut~'s report outlet in, report/poll back out."""
    box = Box(id=s.p.get_id("bpatcher"), maxclass="bpatcher", name=VIEW_FILE,
              args=[buf, chan, voice], numinlets=1, numoutlets=1, outlettype=[""],
              patching_rect=s._rect(VIEW_W, VIEW_H), offset=[0.0, 0.0], viewvisibility=1,
              bgmode=0, border=0, clickthrough=0, enablehscroll=0, enablevscroll=0,
              lockeddragscroll=0)
    s.p.add_box(box)
    s.wire(sk, box, outlet=nout - 1)
    s.wire(box, sk)
    return box


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


def reset_row(s, sk, note):
    r = s.msg("reset")
    s.comment(note, 330.0)
    s.wire(r, sk)


def softkut_row(s, text, nvoices, y, stereo=False):
    """softkut~ with its gain stage and ezdac~, returns (softkut~, outlet count)."""
    outs = ["signal"] * nvoices + [""]
    s.column(15.0, y)
    sk = s.obj(text, nvoices, outs)
    s.x = 480.0
    g0 = s.obj("*~ 0.5", 2, ["signal"])
    g1 = s.obj("*~ 0.5", 2, ["signal"]) if stereo else None
    dac = s.ui("ezdac~", 45.0, 45.0, 2, [])
    s.comment("audio on/off", 90.0)
    s.wire(sk, g0)
    s.wire(g0, dac)
    if stereo:
        s.wire(sk, g1, outlet=1)
        s.wire(g1, dac, inlet=1)
    else:
        s.wire(g0, dac, inlet=1)
    s.newline()
    return sk, len(outs)


def bottom(s, *ys):
    return max(ys) + GAP


# ---- tabs ----------------------------------------------------------------------------
def tab_basic(s):
    header(s, "Loop any buffer~",
                "Messages take <voice> <value>; voices count from 0. Times are seconds of buffer "
                "material. The view at the bottom shows the buffer~, the moving play head and the "
                "loop window.")
    top = s.y
    s.column(15.0, top)
    play = s.msg("loopstart 0 0.2, loopend 0 1.2, play 0 1")
    s.newline()
    stop = s.msg("play 0 0")
    s.comment("play starts inside the loop: no position message needed", 330.0)
    s.newline()
    rates = [s.msg("rate 0 1"), s.msg("rate 0 0.5"), s.msg("rate 0 -1")]
    s.comment("0.5 = octave down, -1 = reverse", 200.0)
    s.newline()
    pos = s.msg("position 0 0.6")
    s.comment("jump the play head (seconds)", 220.0)
    s.newline()
    rs = s.msg("reset")
    s.comment("reset: stop every voice and restore every default (loop 0-1 s, rate 1, "
              "level 1, no feedback). The buffer~ and @report are kept.", 360.0)
    s.newline()
    left_bottom = s.y

    s.column(480.0, top)
    loader(s, "cello-f2.aif", "buffer~ skh_basic")
    s.newline()
    s.comment("replace sizes the buffer~ to the file. Every frame is used; the length need "
              "not be a power of two.", 220.0)
    s.newline()

    sk, nout = softkut_row(s, "softkut~ skh_basic", 1, bottom(s, left_bottom, s.y))
    s.to(sk, play, stop, *rates, pos, rs)
    view(s, sk, "skh_basic", 1, 0, nout)


def tab_channels(s):
    header(s, "Multichannel buffer~",
                "Each voice reads and writes one channel. By default voice v uses channel v mod "
                "the channel count: on this stereo buffer~, voice 0 plays the left channel (bells) "
                "and voice 1 the right (bass).")
    top = s.y
    s.column(15.0, top)
    play = s.msg("loopend 0 2, loopend 1 2, play 0 1, play 1 1")
    s.newline()
    stop = s.msg("play 0 0, play 1 0")
    s.newline()
    d = s.msg("set skh_stereo")
    s.comment("default: voice 0 -> channel 1, voice 1 -> channel 2", 300.0)
    s.newline()
    both = s.msg("set skh_stereo 1")
    s.comment("both voices read channel 1 (channels count from 1)", 300.0)
    s.newline()
    v1 = s.msg("voicebuf 1 skh_stereo 2")
    s.comment("voice 1 alone reads channel 2", 220.0)
    s.newline()
    bad = s.msg("voicebuf 1 skh_stereo 3")
    s.comment("no channel 3: voice 1 falls silent, with one console warning", 230.0)
    s.newline()
    det = s.msg("rate 1 0.5")
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

    sk, nout = softkut_row(s, "softkut~ skh_stereo 2", 2, bottom(s, left_bottom, s.y), stereo=True)
    s.to(sk, play, stop, d, both, v1, bad, det, rs)
    view(s, sk, "skh_stereo", 1, 0, nout)
    s.newline()
    view(s, sk, "skh_stereo", 2, 1, nout)
    s.newline()
    s.comment("The views show channels 1 and 2. After set skh_stereo 1, voice 1's play head "
              "still moves over the channel 2 view, but it reads channel 1.", 520.0)
    s.p.rect = [0.0, 26.0, TAB_W, s.y + 60.0]


def tab_samplerate(s):
    header(s, "The buffer~ sample rate",
                "Like groove~, softkut~ plays a buffer~ at its own sample rate: rate 1 is the "
                "recorded speed, whatever the DSP rate.")
    top = s.y
    s.column(15.0, top)
    play = s.msg("loopstart 0 0, loopend 0 1, play 0 1")
    s.newline()
    stop = s.msg("play 0 0")
    s.newline()
    s.comment("Loop times are seconds at the buffer~'s rate. After sr 22050, loopend 1 covers "
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

    sk, nout = softkut_row(s, "softkut~ skh_sr", 1, bottom(s, left_bottom, s.y))
    s.to(sk, play, stop, rs)
    view(s, sk, "skh_sr", 1, 0, nout)


def tab_record(s):
    header(s, "Record and overdub",
                "The signal inlet is the voice's record input. prelevel sets how much of the old "
                "content survives each pass. Watch the waveform fill in.")
    top = s.y
    s.column(15.0, top)
    go = s.msg("loopend 0 2, rec 0 1, play 0 1")
    s.newline()
    off = s.msg("rec 0 0")
    s.comment("stop recording, keep looping", 200.0)
    s.newline()
    pre = [s.msg("prelevel 0 0"), s.msg("prelevel 0 0.7"), s.msg("prelevel 0 1")]
    s.newline()
    s.comment("prelevel: 0 overwrites, 1 sums on top, between decays", 380.0)
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

    sk, nout = softkut_row(s, "softkut~ skh_rec", 1, bottom(s, left_bottom, s.y))
    s.to(sk, go, off, *pre, rs)
    s.wire(amp, sk)
    view(s, sk, "skh_rec", 1, 0, nout)


def tab_reports(s):
    header(s, "Position reports",
                "The right outlet reports the play head. report <ms> starts or stops phase "
                "messages at once, also while audio runs. The view at the bottom is driven by "
                "these reports.")
    top = s.y
    s.column(15.0, top)
    play = s.msg("loopend 0 2, play 0 1")
    s.newline()
    on, off = s.msg("report 30"), s.msg("report 0")
    q, q0 = s.msg("quant 0 0.25"), s.msg("quant 0 0")
    s.newline()
    s.comment("report 0 freezes the play head; quant moves it in 0.25 s steps", 400.0)
    s.newline()
    poll = s.msg("poll")
    s.comment("poll: one info list per voice: voice, position 0-1 in the loop, play, rec, "
              "start ms, end ms, window ms, state", 380.0)
    s.newline()
    rs = s.msg("reset")
    s.comment("start over", 100.0)
    s.newline()
    left_bottom = s.y

    s.column(480.0, top)
    buf = loader(s, "cello-f2.aif", "buffer~ skh_rep")
    s.newline()

    sk, nout = softkut_row(s, "softkut~ skh_rep", 1, bottom(s, left_bottom, s.y))
    s.to(sk, play, on, off, q, q0, poll, rs)
    s.column(15.0, s.y)
    rt = s.obj("route phase info", 3, ["", "", ""])
    up = s.obj("unpack 0 0.", 1, ["int", "float"])
    ph = s.flonum(70.0)
    s.comment("phase (s)", 70.0)
    s.wire(sk, rt, outlet=nout - 1)
    s.wire(rt, up)
    s.wire(up, ph, outlet=1)
    s.newline()
    pre = s.obj("prepend set", 2, [""])
    last = s.msg("", w=300.0)
    s.comment("latest info", 90.0)
    s.wire(rt, pre, outlet=1)
    s.wire(pre, last)
    s.newline()
    view(s, sk, "skh_rep", 1, 0, nout)


def tab_limits(s):
    header(s, "Safe values",
                "softcut checks no values. softkut~ clamps those that would crash it or write NaN "
                "into the buffer~, and posts a warning to the Max console.")
    rows = [("rate 0 100", "rate is limited to -64..64"),
            ("postrq 0 -1", "filter rq is at least 0.01"),
            ("prelevel 0 1.5", "rec and pre levels are 0..1"),
            ("loopstart 0 -1", "positions are 0 or more"),
            ("recpreslew 0 -1", "times and slews are 0 or more"),
            ("reset", "restore the defaults")]
    msgs = []
    for m, c in rows:
        msgs.append(s.msg(m, w=130.0))
        s.comment(c, 260.0)
        s.newline()
    sk = s.obj("softkut~ skh_limits", 1, ["signal", ""])
    s.obj("buffer~ skh_limits 1000", 1, ["float", "bang"])
    s.to(sk, *msgs)


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
    root = Patcher("help/softkut~.maxhelp")
    root.rect = [100.0, 100.0, TAB_W + 20, 760.0]
    root.showrootpatcherontab = 0
    root.showontab = 0
    tabs = [("basic", tab_basic), ("channels", tab_channels), ("samplerate", tab_samplerate),
            ("record", tab_record), ("reports", tab_reports), ("limits", tab_limits)]
    for i, (name, build) in enumerate(tabs):
        sub = Patcher(parent=root)
        root.add_subpatcher("p " + name, patching_rect=[15.0 + 110 * i, 60.0, 100.0, 22.0],
                            patcher=sub, numinlets=0, numoutlets=0)
        build(Sheet(sub))
    view_patch = make_view()
    errors = check(root) + check(view_patch, "view")
    # the help's bpatchers declare 1 inlet and 1 outlet: the view must have them
    for cls in ("inlet", "outlet"):
        n = sum(1 for b in view_patch._boxes if b.text == cls)
        if n != 1:
            errors.append("view: %d %s objects, expected 1" % (n, cls))
    # py2max's lint gives a bpatcher that loads a file 0 ports, whatever it
    # declares; its cords are checked above against the declared ports instead
    findings = [f for f in lint(root) + lint(view_patch) if f.severity == "error"
                and "'bpatcher' has 0" not in str(f)]
    if errors or findings:
        sys.exit("\n".join(errors + [str(f) for f in findings]))
    view_patch.save()
    root.save()
    print("wrote help/softkut~.maxhelp (%d tabs) and patchers/%s: no overlaps, cords and "
          "lint clean" % (len(tabs), VIEW_FILE))


if __name__ == "__main__":
    main()
