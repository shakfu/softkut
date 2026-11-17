# M3 design: one voice per object, parameters as attributes

Date: 2026-11-17. Branch: `dev`. Status: draft, not implemented.

Context: `docs/dev/usability-review.md` (A1, A2, O1) and `REVIEW.md` M3, M5, M6. Decided 2026-11-17: discrete one-voice `softkut~`; no sample-exact cross-voice alignment needed; `rate` becomes a signal inlet in the same change.

## Box

```
[softkut~ <buffer~> @attr value ...]
```

| Port | Kind | Content |
|-|-|-|
| Inlet 1 | signal, messages | Record input. All messages and attributes. |
| Inlet 2 | signal, float | Rate. A signal sets the rate per sample and overrides `@rate`. A float sets `@rate`. |
| Outlet 1 | signal | Audio. |
| Outlet 2 | signal | Sync: the play head (see `@syncmode`). |
| Outlet 3 | messages | Dumpout: `getattr` replies and `info`. |

The creation argument is the `buffer~` name only. The voice count argument goes away.

## Attributes (saved with the patcher)

Ranges are the clamps in `sanitize()` (`softkut_engine.h`) unless marked new. "maxSec" is 2^30 frames at the buffer's rate. "Changed" marks a default that differs from softcut's.

### Buffer and sync

| Attribute | Type | Unit | Range | Default | Changed | Notes |
|-|-|-|-|-|-|-|
| `buffer` | symbol | | | arg 1 | | `set <name>` stays as an alias, as in `groove~`. |
| `channel` | long | | >= 1 | 1 | new | Channel of `buffer` this voice reads and writes. Missing channel: silence, one warning. |
| `syncmode` | enum | | `ms`, `phase` | `ms` | new | `ms`: head in ms of buffer, wrapped. `phase`: 0-1 across the loop, as `groove~`. |

### Loop and rate

| Attribute | Type | Unit | Range | Default | Changed | Notes |
|-|-|-|-|-|-|-|
| `rate` | float | ratio | -64..64 | 1 | | Ignored while inlet 2 has a signal. |
| `rateslew` | float | ms | >= 0 | 1 | | Also smooths the rate signal. Set 0 for audio-rate modulation. |
| `loopstart` | float | ms | 0..maxSec | 0 | | |
| `loopend` | float | ms | 0..maxSec | 0 | yes (1000) | 0 = end of buffer (A3). |
| `loop` | long | 0/1 | | 1 | | `0` = one-shot. |
| `fade` | float | ms | >= 0 | 10 | | Loop and cut crossfade. |

### Record

| Attribute | Type | Unit | Range | Default | Changed | Notes |
|-|-|-|-|-|-|-|
| `reclevel` | float | amp | 0..1 | 1 | | |
| `prelevel` | float | amp | 0..1 | 0 | | |
| `recpreslew` | float | ms | >= 0 | 1 | | |
| `recoffset` | long | samples | +/-2^30 | -8 | unit (ms) | Samples of buffer material (decision 1). |
| `recshape` | enum | | `linear`, `sine`, `raised` | `linear` | yes (`raised`) | M6. `raised` inverts polarity. Needs the `FadeCurves` patch (`TODO.md`). |
| `preshape` | enum | | `linear`, `sine`, `raised` | `linear` | | M6. See the upstream bug below. |
| `softclip` | long | 0/1 | | 0 | yes (always on) | M6. Needs a runtime switch for `SubHead.cpp:119`. |

### Input filter (record path)

| Attribute | Type | Unit | Range | Default | Changed | Notes |
|-|-|-|-|-|-|-|
| `prefc` | float | Hz | >= 10 | 16000 | | `Svf` clamps to 0.4 x sr (decision 4). |
| `prerq` | float | | >= 0.01 | 4 | | |
| `prelp` | float | mix | 0..1 | 0 | yes (1) | M6: transparent input. |
| `prehp` `prebp` `prebr` | float | mix | 0..1 | 0 | | |
| `predry` | float | amp | 0..1 | 1 | yes (0) | M6. |
| `prefcmod` | float | | 0..1 | 1 | | Cutoff tracking of rate. |

### Output filter

| Attribute | Type | Unit | Range | Default | Changed | Notes |
|-|-|-|-|-|-|-|
| `postfc` | float | Hz | >= 10 | 12000 | | As `prefc`. |
| `postrq` | float | | >= 0.01 | 4 | | |
| `postlp` `posthp` `postbp` `postbr` | float | mix | 0..1 | 0 | | |
| `postdry` | float | amp | 0..1 | 1 | | |

## Messages (events, not saved)

| Message | Args | Notes |
|-|-|-|
| `play` | `0/1` | Transport. Not an attribute: a saved `1` would start playback on load. |
| `rec` | `0/1` | Transport, same reason. |
| `reconce` | none | Records one loop pass. Should imply `rec 1`; today it needs both (A6). |
| `position` | `<ms>` | Crossfaded cut. |
| `stop` | none | `play 0, rec 0` as one command (decision 2). |
| `reset` | none | Restores every attribute default. Keeps `buffer`, `channel`. |
| `poll` | none | One `info <play> <rec> <state> <ms>` on outlet 3. The loop bounds move to `getattr`. |

## Removed

| Removed | Replacement |
|-|-|
| `<voice>` argument on every message | one object per voice |
| `voicebuf` | `@buffer`, `@channel` |
| `level`, `levelslew`, `pan`, `panslew` | `*~`, `line~`, `gain~` (decided 2026-10-03) |
| `inlevel` | cords and `*~` into inlet 1 |
| `feedback` | `send~`/`receive~` into inlet 1, one vector delayed |
| `sync` | `snapshot~` on another object's sync outlet into `position`; block-accurate |
| `enable` | `play 0, rec 0` (a stopped voice costs almost nothing); `mute~` for a subpatcher |
| `@report`, `phase`, `quant`, `phaseoffset` | sync outlet into `snapshot~`, then `round` or `+` (decision 3) |
| `stop`'s park behaviour (heads parked, flags kept) | none; no use case found (decision 2) |

`mc.softkut~` is out of scope here. Its shape (array attributes, `setvalue`) is a separate decision.

## Signal-rate `rate` (M5)

`Voice::processBlockMono` already applies rate per sample: `sch.setRate(rateRamp.update())` (`Voice.cpp:88`). A rate buffer therefore needs no sub-block chunking:

- Patch `processBlockMono` to take an optional `const float *rateIn` and set `rateRamp`'s target per sample (`// softkut patch`).
- The engine scales each sample by bufferRate/dspRate, and clamps it to -64..64 on the audio thread; a non-finite sample holds the last value. This is the `kMaxRate` crash bound, at audio rate.
- `dsp64` checks `count[1]`. A connected signal selects the rate buffer; an unconnected inlet uses `@rate`.
- `processBlockMono` builds a `std::function` per call (`Voice.cpp:60`). That is per block, unchanged by this patch.

## Upstream bug found while drafting

`FadeCurves::calcPreFade` (`FadeCurves.cpp:103`) tests `recShape == Raised` in the pre-curve branch, then asserts `preShape == Raised`. With `preshape raised` and `recshape linear`, no branch matches; the function returns early and the previous pre curve stays in `preFadeBuf`. Fix in the vendored copy before exposing `preshape`.

## Decisions (2026-11-17)

1. **`recoffset` in samples**, as a `long`. softcut sets it in frames. In ms the default depends on the rate: -0.167 ms at 48 kHz, -0.181 ms at 44.1 kHz.

2. **`stop` = `play 0, rec 0`** in one command. Users of `play~` read `stop` as "stop transport". The park behaviour is dropped: it had no use case, and it left the next `play 1` ambiguous.

3. **Remove `@report`, `phase`, `quant` and `phaseoffset`** (A4). Each duplicates the sync outlet with patching (`snapshot~`, `round`, `+`). Removing them also removes a clock and its teardown path. Only norns-script fidelity is lost, which is not a goal.

4. **Ranges.** fc >= 10 Hz with no static maximum: `Svf::setFc` clamps to 0.4 x sr (`Svf.cpp:30-41`), a limit that depends on the DSP rate. Document the effective ceiling (19.2 kHz at 48 kHz). rq >= 0.01, unchanged. Mixes, dry levels and `prefcmod` 0..1. The mixes are linear gains with no stability risk, so their clamp is for the inspector and `pattr`. `prefcmod` above 1 can drive fc negative (`Voice.cpp:220-221`), which `Svf` clamps to 10 Hz: safe, but meaningless.

5. **Stereo: two objects; measure before adding `@chans`.** Two objects with the same settings and one start message stay locked: the voices are deterministic, and one signal can feed both rate inlets. Unverified risk: with the scheduler on the main thread (Overdrive off, or Scheduler in Audio Interrupt off), a message can arrive between the two perform calls and leave one object a vector behind. Test: a `metro` sends `play 1` to two objects on one stereo buffer, with and without Overdrive; null-test the outputs with one channel inverted. If an offset appears, add `@chans N`: N voices locked in one engine, attributes shared, one channel each.

## Open

1. `loopend 0` = buffer end needs the buffer length in the engine. The shell passes it in `BufferView` each block, so the engine can resolve it there.
