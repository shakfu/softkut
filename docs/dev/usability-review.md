# softkut~ usability and UI review

Date: 2026-11-17. Commit: `418429c`. Scope: `softkut~` and `mc.softkut~` as Max objects: creation args, message API, outlets, reports, help, docs and packaging.

Method: code and doc reading only; nothing was run in Max. `make test` passes (1/1). `REVIEW.md` (2026-10-03) covered correctness. This review assumes those fixes and looks only at the user-facing layer.

## Summary

- Signal plumbing now follows the Max contract: any `buffer~`, ms units, voices from 1, sync outlets.

- The control layer still follows norns. There are 35 table messages of the form `<cmd> <voice> <value>`, plus routing matrices. The only attribute is `@report`. Nothing saves with the patch, appears in the inspector, or works with `pattr`. This is the biggest usability cost, and M3 is still open.

- The `mc.softkut~` help patch is broken against the current API, so its first-run experience fails.

## 1. API shape

**A1. Every message needs a voice number, even on the default 1-voice object (high).**

- `[softkut~ buf]` has 1 voice, but you must still send `rate 1 0.5`.

- The leading `1` reads like a value. `rate 0.5` fails with "expected <voice> <value>" (`softkut_control.h:83`).

- `rate 1.5 2` truncates the voice with `atom_getlong` and plays voice 1 at rate 2. There is no warning.

- Fix: M3 option 1 (one voice per object, plain `rate 0.5`). Cheaper interim: if there is one voice and one argument, treat the argument as the value.

**A2. No parameter attributes (high).** All per-voice state is write-only. The only read-back is the 9-field `info` list. Users cannot:

- save settings with the patch, or recall them with `pattr` or snapshots;

- read values back with `getattr`;

- see ranges and defaults in the inspector.

With one voice per object, every table entry becomes a `CLASS_ATTR_FLOAT` with a `FILTER_CLIP` range. The clamp table in `sanitize()` already holds those ranges.

**A3. The default loop is 0-1000 ms, whatever the buffer length (high).**

- `setDefaults()` sets `loopEnd_ = 1.0 s` (`softkut_engine.h`).

- If you load a 5 s file and send `play 1 1`, you hear only its first second. The help's first message sets loop points explicitly, which hides this.

- As far as I recall from the Max ref (not checked here), `groove~` treats a loop end of 0 as the buffer end. Recommend the same: default loop end = 0 = whole buffer.

**A4. Three names for one concept (medium).**

- `position` is a message (jump the head).

- `phase` is a clock report.

- The sync outlet is a signal.

- `info`'s field 9 is also called `position`.

`quant` and `phaseoffset` only change the `phase` report, not playback, but their names suggest quantized playback. Once the sync outlet exists, `phase`, `@report`, `quant` and `phaseoffset` add no capability: `snapshot~` on the sync outlet does the same. Recommend dropping them, or moving them to a "norns compat" section.

**A5. `info` is a fragile positional list (medium).**

- Its 9 fields need a 9-wide `unpack`, as in the view (`unpack 0. 0 0 0. 0. 0. 0 0.`).

- The 9th field was added in this cycle, which already broke existing unpacks once.

- It reports the loop position twice (`pos` 0-1, and `position` in ms).

- Alternatives: separate `getattr`-style replies, or a `dict` output. With attributes (A2), `info` shrinks to transport state and the play head.

**A6. Messages that do something surprising or nothing (medium).**

- `pan` and `panslew` are accepted and do nothing, silently (`softkut_control.h:60-62`). They should raise "doesn't understand" until removed (D1).

- `stop` parks the heads but leaves the play/rec flags set. In `play~`, `stop` means "stop transport". It is unclear what `play 1 1` does after `stop`. Rename it (e.g. `park`) or make it clear the flags.

- `reconce` needs `rec <v> 1` as well (see `docs/dev/manual-tests.md` section 3). The README does not say so.

- `loopstart` greater than `loopend` is not checked (`sanitize()`). What softcut does in that case is undocumented.

- A loop point past the buffer's end wraps with no warning.

**A7. Creation arguments fail silently (medium).** In `softkut~.cpp:183-186`:

- `[softkut~ 4]` gives 1 voice with no buffer, because the first argument is not a symbol.

- `[softkut~ buf 4.]` gives 1 voice, because the voice count must be `A_LONG`.

- Neither posts a message.

- `mc.softkut~` takes its voice count as positional argument 2. Max's MC convention is `@chans N`, which also allows changing it at runtime.

**A8. No float input on signal inlets (low).** `groove~` takes a float in its rate inlet. Here a number box into an inlet errors. The bigger gap is M5: `rate` is not signal-rate, so modulation is at scheduler rate plus `rateslew`.

## 2. Outlets and patching

**O1. Multi-voice outlet layout breaks patches when the voice count changes (medium).**

- `[softkut~ buf 6]` has 13 outlets: 6 audio, then 6 sync, then reports.

- Voice 2's sync is outlet 8. Changing the argument from 2 to 3 renumbers every sync outlet, and Max drops or misroutes cords.

- One voice per object (M3) fixes this. MC fixes it for multi-voice use.

**O2. The README's `groove~` comparison is inaccurate (medium, doc).** `README.md:42` says the sync outlet works "as with `groove~`'s sync outlet". `groove~`'s sync outlet is a 0-1 ramp over the loop (from memory of the Max ref; not checked here). softkut's outputs ms of buffer, wrapped. Either:

- reword the claim, or

- add a `@syncmode` (ms in buffer / 0-1 in loop).

The 0-1 form is what most `groove~` patches expect.

**O3. The view bpatcher needs 3 cords plus a feedback loop (low).**

- `softkut.view.maxpat` polls `info` every 100 ms back into the object.

- `poll` emits all N voices, so N views cause N^2 lists every 100 ms. All of them go to everything on the report outlet, including user `print`s.

- Alternative: the view could poll with its own voice argument (`poll <v>`), or read attributes once A2 exists.

## 3. Defaults (M6, still open)

Recording is colored out of the box. From `Voice::reset` and `SubHead.cpp:119`:

- the input is low-passed at 16 kHz with rq 4, and `predry` is 0;

- a 1.2x soft clip is applied;

- the default Raised rec fade inverts polarity.

A Max user treating this as `record~` with loops gets altered audio, and the help never says so. This is the second-largest surprise after A3.

## 4. Help, docs, packaging

| # | Finding | Severity |
|-|-|-|
| H1 | `help/mc.softkut~.maxhelp` is pre-M4. Voices are 0-based (`play 0 1`, which now errors), times are in seconds (`recpreslew 0 0.05` now means 50 us), and it has `pan` buttons and text about "outlets 6/7 stereo mix". | high |
| H2 | There is no `mc.softkut~.maxref`. | medium |
| H3 | `package-info.json` description says "6-voice buffer looper". | low |
| H4 | `patchers/` tracks `softkut.view-fixed.maxpat`, `softkut.view-nw.maxpat` and `softkut.view-old copy.maxpat`. All are on Max's search path, and one name has a space. | low |
| H5 | The help covers about 20 of 44 messages. Filters, `feedback`, `inlevel`, `sync`, `reconce`, `enable` and `stop` have no tab. Defer this until after M3. | low |
| H6 | `init/object-mappings.txt` is empty. Delete it, or map `softcut~` to `softkut~`. | low |

What works:

- The `softkut~` help's six tabs and the view.

- Clamp warnings in message units.

- Assist strings that follow the voice count.

- `dblclick` opens the buffer.

- `reset` keeps the buffer and `@report`.

## 5. Priority

1. M3: one voice per object plus attributes. This fixes A1, A2, O1, and most of A5 and A7.

2. A3: whole-buffer default loop.

3. H1: fix or remove the `mc` help.

4. M6: transparent record defaults.

5. A6: reject `pan`; rename `stop`; document `reconce`.

6. A4: drop `phase`/`@report`/`quant`/`phaseoffset`. O2: sync mode.

## Alternative framing

Rather than reshaping one external, ship two:

- `softkut~`: a single voice with attributes, idiomatic and `groove~`-like.

- `mc.softkut~ @chans N`: control via `mc.target`/`setvalue`.

Then move the norns layer (matrices, `phase`, `sync`) into an abstraction (`softkut.norns`). That keeps norns-script portability without putting it in the core API. Whether native MC objects get `setvalue` routing for free is unverified (`REVIEW.md` M3). Check that before committing to this.

## Open questions

1. With one voice per object, `sync` needs a second object's position. Is `position` driven from another object's sync outlet (via `snapshot~`) good enough, given the scheduler jitter?

2. Should the default loop be the whole buffer, even though norns scripts assume 0-1 s?

3. Is `info`'s list shape a compatibility commitment yet, or free to change?
