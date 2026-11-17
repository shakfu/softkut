"""Generate docs/softkut~.maxref and docs/mc.softkut~.maxref.

Run from the repository root:  uv run python scripts/make_maxref.py  (make maxref)

Both objects share the voice attributes; mc.softkut~ holds one value per voice.
"""
from xml.sax.saxutils import escape as e

ATTRS = [
    # name, type, digest, description
    ("buffer", "symbol", "The buffer~", "The buffer~ the voice reads and writes. Also set by the first argument or the set message."),
    ("channel", "int", "buffer~ channel", "The channel of the buffer~ the voice reads and writes, counted from 1. Default 1. A channel the buffer~ lacks silences the voice, with one console warning."),
    ("syncmode", "int", "Play head output", "What the middle outlet carries: ms (0, the default), the play head in ms of the buffer~, wrapped to its length; or phase (1), 0-1 across the loop."),
    ("rate", "float64", "Playback and record rate", "1 is the buffer~'s recorded speed, 0.5 an octave down, 2 an octave up, negative reverse. Range -64 to 64. Ignored while the right inlet has a signal. Default 1."),
    ("rateslew", "float64", "Rate smoothing (ms)", "Smoothing time for rate changes, including the rate signal. Set 0 for audio-rate modulation. Default 1."),
    ("loopstart", "float64", "Loop start (ms)", "Loop start in ms of buffer material. Default 0."),
    ("loopend", "float64", "Loop end (ms)", "Loop end in ms of buffer material. 0 (the default) is the buffer~'s end, and follows the buffer~ when it is resized. A loop end past the buffer~'s end wraps to its start."),
    ("loop", "int", "Loop on/off", "1 (the default) loops; 0 plays one-shot and stops at the loop end."),
    ("fade", "float64", "Crossfade time (ms)", "Crossfade time for the loop wrap and for position jumps. Default 10."),
    ("reclevel", "float64", "Record level", "How much of the input is written, 0-1. Default 1."),
    ("prelevel", "float64", "Pre level", "How much existing content is kept on each pass, 0-1: 0 (the default) overwrites, 1 sums on top, between decays."),
    ("recpreslew", "float64", "Record and pre level smoothing (ms)", "Smoothing time for reclevel and prelevel. Default 1."),
    ("recoffset", "int", "Record head offset (samples)", "Offset of the record head from the play head, in samples of buffer material. Default -8."),
    ("recshape", "int", "Record fade shape", "Curve of the record level during crossfades: linear (0, the default), sine (1) or raised (2). raised is softcut's default and stores the input inverted."),
    ("preshape", "int", "Pre fade shape", "Curve of the pre level during crossfades: linear (0, the default), sine (1) or raised (2)."),
    ("softclip", "int", "Soft clip on/off", "1 applies softcut's soft clip to the record input: 1.2x gain below its knee, saturating above. 0 (the default) records the input unchanged. Stored samples are bounded to +/-32 either way."),
    ("prefc", "float64", "Input filter cutoff (Hz)", "Cutoff of the multimode filter on the record input. At least 10 Hz; softcut caps it at 0.4 times the sample rate. Default 16000."),
    ("prerq", "float64", "Input filter reciprocal Q", "Reciprocal Q of the input filter, at least 0.01; smaller is more resonant. Default 4."),
    ("prelp", "float64", "Input filter low-pass mix", "Low-pass output of the input filter, 0-1. Default 0."),
    ("prehp", "float64", "Input filter high-pass mix", "High-pass output of the input filter, 0-1. Default 0."),
    ("prebp", "float64", "Input filter band-pass mix", "Band-pass output of the input filter, 0-1. Default 0."),
    ("prebr", "float64", "Input filter band-reject mix", "Band-reject output of the input filter, 0-1. Default 0."),
    ("predry", "float64", "Input filter dry mix", "Unfiltered input, 0-1. Default 1: the input is recorded unfiltered."),
    ("prefcmod", "float64", "Input filter rate tracking", "How far the input filter cutoff follows the rate, 0-1. Default 1."),
    ("postfc", "float64", "Output filter cutoff (Hz)", "Cutoff of the multimode filter on playback. At least 10 Hz. Default 12000."),
    ("postrq", "float64", "Output filter reciprocal Q", "Reciprocal Q of the output filter, at least 0.01. Default 4."),
    ("postlp", "float64", "Output filter low-pass mix", "Low-pass output of the output filter, 0-1. Default 0."),
    ("posthp", "float64", "Output filter high-pass mix", "High-pass output of the output filter, 0-1. Default 0."),
    ("postbp", "float64", "Output filter band-pass mix", "Band-pass output of the output filter, 0-1. Default 0."),
    ("postbr", "float64", "Output filter band-reject mix", "Band-reject output of the output filter, 0-1. Default 0."),
    ("postdry", "float64", "Output filter dry mix", "Unfiltered playback, 0-1. Default 1."),
]


MC_ATTRS = [
    ("feedback", "atom", "Feedback routes", "The nonzero routes as src dst gain triples, voices from 1; saved with the patcher. Setting it changes only the routes named (see the feedback message)."),
    ("chans", "int", "Voice count", "Number of voices, 1-16, default 1: the channel count of both signal outlets. Set it in the object box; it cannot change later."),
    ("channel", "int", "buffer~ channel per voice", "One value per voice, counted from 1. 0 (the default) gives voice v channel v, wrapping, so a stereo buffer~ plays as voices 1 and 2."),
]

METHODS = [
    # name, args [(name, type, optional)], digest, description
    ("play", [("flag", "int", 0)], "Start or stop playback", "1 starts playback, 0 stops it. Playback starts inside the loop."),
    ("rec", [("flag", "int", 0)], "Start or stop recording", "1 starts writing the record input into the buffer~, 0 stops it."),
    ("reconce", [], "Record one loop pass", "Records one pass, from the next loop start or position jump, then stops recording."),
    ("stop", [], "Stop playback and recording", "Equivalent to play 0 and rec 0, applied in the same signal vector."),
    ("position", [("ms", "float", 0)], "Jump the play head", "Jumps the play head to a position in ms of buffer material, with a crossfade (@fade)."),
    ("set", [("buffer-name", "symbol", 0), ("channel", "int", 1)], "Set the buffer~", "Sets @buffer, and @channel if given."),
    ("poll", [], "Report the voice's state", "Sends info out the right outlet (see the outlet)."),
    ("reset", [], "Restore the defaults", "Stops the voice and restores every attribute default. @buffer and @channel are kept."),
    ("signal", [], "Record input, or rate", "Left inlet: the record input. Right inlet: the rate, per sample, in place of @rate."),
    ("float", [("rate", "float", 0)], "Set the rate", "In the right inlet, sets @rate. The left inlet takes no numbers."),
    ("int", [("rate", "int", 0)], "Set the rate", "In the right inlet, sets @rate."),
]


MC_METHODS = [
    ("play", [("flags", "list", 0)], "Start or stop playback", "One value for every voice, or one per voice: play 1 0 1 plays voices 1 and 3."),
    ("rec", [("flags", "list", 0)], "Start or stop recording", "One value for every voice, or one per voice. A voice past the record input's channel count records silence."),
    ("position", [("ms", "list", 0)], "Jump the play heads", "One position in ms for every voice, or one per voice, with a crossfade (@fade)."),
    ("reconce", [], "Record one loop pass", "Every voice records one pass, from its next loop start or position jump."),
    ("stop", [], "Stop playback and recording", "play 0 and rec 0 for every voice."),
    ("setvalue", [("voice", "int", 0), ("name", "symbol", 0), ("value", "list", 1)], "Address one voice", "Sets one voice's attribute (any voice attribute, or channel), or sends it play, rec, position, reconce or stop. Voices count from 1."),
    ("sync", [("follow", "int", 0), ("lead", "int", 0), ("offset-ms", "float", 1)], "Align one voice to another", "Jumps voice follow to voice lead's position plus offset ms, sample-exact."),
    ("feedback", [("src", "int", 0), ("dst", "int", 0), ("gain", "float", 0)], "Route a voice into another's record input", "Sets the gain from voice src's output to voice dst's record input, one signal vector later; repeat the triple to set several routes. Other routes keep their gains; 0 removes a route. Self-feedback is allowed. reset clears every route."),
    ("set", [("buffer-name", "symbol", 0), ("channel", "int", 1)], "Set the buffer~", "Sets @buffer, and @channel for every voice if given."),
    ("poll", [], "Report each voice's state", "Sends one info list per voice out the right outlet (see the outlet)."),
    ("reset", [], "Restore the defaults", "Stops every voice and restores every attribute default. @buffer and @channel are kept."),
    ("signal", [], "Record inputs, or rates", "Left inlet: channel v is voice v's record input. Right inlet: channel v is voice v's rate, in place of @rate."),
    ("float", [("rate", "float", 0)], "Set the rate", "In the right inlet, sets @rate for every voice. The left inlet takes no numbers."),
    ("int", [("rate", "int", 0)], "Set the rate", "In the right inlet, sets @rate for every voice."),
]


def ports(w, kind, items):
    w("\t<%slist>" % kind)
    for i, (typ, dig, desc) in enumerate(items):
        w('\t\t<%s id="%d" type="%s">' % (kind, i, typ))
        w("\t\t\t<digest>%s</digest>" % e(dig))
        w("\t\t\t<description>%s</description>" % e(desc))
        w("\t\t</%s>" % kind)
    w("\t</%slist>" % kind)


def write(path, name, digest, description, inlets, outlets, arg_desc, methods, attrs, seealso):
    L = []
    w = L.append
    w("<?xml version='1.0' encoding='utf-8' standalone='yes'?>")
    w('<?xml-stylesheet href="./_c74_ref.xsl" type="text/xsl"?>')
    w("")
    w("<c74object name='%s' module='msp' category='MSP Sampling'>" % name)
    w("")
    w("\t<digest>%s</digest>" % e(digest))
    w("")
    w("\t<description>%s</description>" % e(description))
    w("")
    w("\t<metadatalist>")
    w("\t\t<metadata name='author'>Shakeeb Alireza</metadata>")
    for tag in ("MSP", "MSP Sampling", "softcut"):
        w("\t\t<metadata name='tag'>%s</metadata>" % tag)
    w("\t</metadatalist>")
    w("")
    ports(w, "inlet", inlets)
    w("")
    ports(w, "outlet", outlets)
    w("")
    w("\t<objarglist>")
    w('\t\t<objarg name="buffer-name" optional="1" type="symbol">')
    w("\t\t\t<digest>The buffer~</digest>")
    w("\t\t\t<description>%s</description>" % e(arg_desc))
    w("\t\t</objarg>")
    w("\t</objarglist>")
    w("")
    w("\t<methodlist>")
    for mname, args, dig, desc in methods:
        w('\t\t<method name="%s">' % mname)
        if args:
            w("\t\t\t<arglist>")
            for an, at, opt in args:
                w('\t\t\t\t<arg name="%s" type="%s" optional="%d" />' % (an, at, opt))
            w("\t\t\t</arglist>")
        w("\t\t\t<digest>%s</digest>" % e(dig))
        w("\t\t\t<description>%s</description>" % e(desc))
        w("\t\t</method>")
    w("\t</methodlist>")
    w("")
    w("\t<attributelist>")
    for aname, typ, dig, desc, size in attrs:
        w("\t\t<attribute name='%s' get='1' set='1' type='%s' size='%s' >" % (aname, typ, size))
        w("\t\t\t<digest>%s</digest>" % e(dig))
        w("\t\t\t<description>%s</description>" % e(desc))
        w("\t\t</attribute>")
    w("\t</attributelist>")
    w("")
    w("\t<seealsolist>")
    for s in seealso:
        w('\t\t<seealso name="%s"/>' % s)
    w("\t</seealsolist>")
    w("")
    w("</c74object>")
    open(path, "w").write("\n".join(L) + "\n")


INFO = ("state is 0 stopped, 1 playing, 2 recording, 3 overdub; position is the play head; "
        "start and end are the loop played, so @loopend 0 reports the buffer~'s end. Times are "
        "ms. Also answers getattr messages, such as getrate, with the attribute name and value.")


def main():
    write("docs/softkut~.maxref", "softkut~", "One softcut looper voice on a buffer~",
          "softkut~ is one voice of monome's softcut-lib, the norns looping engine. It plays and "
          "records one channel of a buffer~ with crossfaded loop points and position jumps, "
          "overdub with a pre level, resampled reads and writes at any rate, and multimode "
          "filters on the record input and on playback. Parameters are attributes; play, rec "
          "and the other transport controls are messages. Use one object per voice, or "
          "mc.softkut~. Any buffer~ works: any length, any channel count, and its own sample "
          "rate. Times are ms of buffer material.",
          [("signal", "Record input, messages",
            "The signal recorded while rec is on. Takes every message and attribute."),
           ("signal/float", "Rate",
            "A signal sets the rate for every sample, in place of @rate; @rateslew still smooths "
            "it. A number sets @rate.")],
          [("signal", "Audio", "The voice's playback, after the output filter."),
           ("signal", "Play head",
            "The play head for every sample: ms in the buffer~, wrapped to its length, or 0-1 "
            "across the loop with @syncmode phase. Use snapshot~ to drive a waveform~ cursor."),
           ("list", "info and attribute values",
            'On poll, "info <play> <rec> <state> <position> <start> <end>": ' + INFO)],
          "Sets @buffer. Further plain arguments are ignored with a warning: use one object per "
          "voice.",
          METHODS, [a + ("1",) for a in ATTRS],
          ["buffer~", "groove~", "record~", "poke~", "mc.softkut~"])

    mc_attrs = [MC_ATTRS[0] + ("0",), MC_ATTRS[1] + ("1",)]
    for a in ATTRS:
        if a[0] == "channel":
            mc_attrs.append(MC_ATTRS[2] + ("16",))
        elif a[0] in ("buffer", "syncmode"):
            mc_attrs.append(a + ("1",))
        else:
            desc = a[3].replace("Ignored while the right inlet has a signal.",
                                "Ignored for a voice with a rate channel in the right inlet.")
            mc_attrs.append((a[0], a[1], a[2], desc + " One value per voice; one value sets "
                             "every voice.", "16"))
    write("docs/mc.softkut~.maxref", "mc.softkut~", "softcut looper voices as multichannel signals",
          "mc.softkut~ is @chans softkut~ voices (1-16) in one object: voice v is channel v of "
          "every signal port. Every voice attribute holds one value per voice: one value sets "
          "every voice, a list sets them in order, and setvalue addresses one voice. Voices in "
          "one object can be aligned sample-exactly with sync.",
          [("multichannel signal", "Record inputs, messages",
            "Channel v is voice v's record input; a voice past the channel count records "
            "silence. Takes every message and attribute."),
           ("multichannel signal/float", "Rates",
            "Channel v sets voice v's rate per sample, in place of @rate; a voice past the "
            "channel count uses @rate. A number sets @rate for every voice.")],
          [("multichannel signal", "Audio", "One channel per voice."),
           ("multichannel signal", "Play heads",
            "One channel per voice: ms in the buffer~, or 0-1 across the loop with @syncmode "
            "phase."),
           ("list", "info and attribute values",
            'On poll, one "info <voice> <play> <rec> <state> <position> <start> <end>" per '
            "voice: " + INFO)],
          "Sets @buffer. Set the voice count with @chans; further plain arguments are ignored "
          "with a warning.",
          MC_METHODS, mc_attrs,
          ["softkut~", "buffer~", "mc.pack~", "mc.unpack~", "mc.dup~"])
    print("wrote docs/softkut~.maxref and docs/mc.softkut~.maxref")


if __name__ == "__main__":
    main()
