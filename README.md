# softkut~

A Max/MSP external wrapping monome's [softcut-lib](https://github.com/monome/softcut-lib) — the
real-time audio buffer looping/resampling engine from the [norns](https://monome.org/norns/) sound
computer. `softkut~` exposes 6 independent softcut voices that read and write a shared (or
per-voice) Max `buffer~`, with sub-sample looping, crossfaded overdub, resampling read/write heads,
per-voice input and output multimode filters, a per-voice level/pan stereo mix, and voice-to-voice
plus inlet-to-voice routing matrices.

The DSP lives in a host-agnostic engine (`source/include/softkut_engine.h`); this Max external is a
thin shell over it. Control messages are marshalled to the audio thread through a lock-free
command queue, so parameter changes are click-free and thread-safe.

## Building

```
git submodule update --init --recursive   # max-sdk-base (softcut-lib is vendored)
make build                                 # configure + build (universal mac binary)
make test                                  # build + run the offline engine test harness
```

`make build` produces `externals/softkut~.mxo`. `make link` symlinks the repo into your Max
`Packages` folder so Max can find the external and its help patch. The build is also a standard
CMake project (`cmake .. && cmake --build .`).

## The object

```
[softkut~ <buffer~ name> @report <ms>]
```

- **Inlets** (6 signals): voice 0..5 record inputs. Control messages go to the **left inlet**.
- **Outlets**: `0..5` = per-voice signal outputs · `6` = stereo mix L · `7` = stereo mix R ·
  `8` = message outlet (phase/position reports).

All voices share the `buffer~` named in the first argument (or via `set`). Each voice can instead
be pointed at its own `buffer~` with `voicebuf` (e.g. for stereo: voice 0 -> left, voice 1 -> right).

### Buffer requirements

softcut needs **contiguous mono** sample memory and wraps buffer indices with a power-of-two
bitmask. Therefore:

- The `buffer~` must be **mono** (a multichannel `buffer~` is refused with a warning).
- Only the **largest power-of-two prefix** of the buffer's length is used; the remainder is left
  untouched. softkut~ posts a note on DSP start when it trims (e.g. "using 32768 of 48000 frames").
  For full use of a buffer, size it to a power of two (e.g. `buffer~ skbuf 1000` ≈ 48000 samples ->
  32768 used; size it exactly if you need every sample).

File loading/saving/clearing is handled by Max's `buffer~` itself (`read`, `replace`, `write`,
`crop`, `clear`), not by softkut~.

## Message API

Most messages address a single voice and take the form `<message> <voice> <value>` where `voice`
is `0..5`. Times are in **seconds**, gains/levels are linear amplitudes, flags are `0`/`1`.

### Transport

| Message | Args | Description |
|---|---|---|
| `play` | `<voice> <0/1>` | Enable/disable playback (reading) for a voice. |
| `rec` | `<voice> <0/1>` | Enable/disable recording (writing) for a voice. |
| `reconce` | `<voice> <0/1>` | Record-once: records a single loop pass then auto-clears the record flag. |
| `stop` | `<voice>` | Immediately park the voice's heads (silent), distinct from `play 0`. |
| `position` | `<voice> <sec>` | Jump the play head to a position (seconds within the buffer). |
| `enable` | `<voice> <0/1>` | Master on/off gate for a voice. When off, the voice is skipped entirely (no audio, no recording, no feedback contribution). Default on. |

### Loop and rate

| Message | Args | Description |
|---|---|---|
| `rate` | `<voice> <ratio>` | Playback/record rate. `1` = normal, `0.5` = half speed/octave down, `2` = double, negative = reverse. |
| `loopstart` | `<voice> <sec>` | Loop start point (seconds). |
| `loopend` | `<voice> <sec>` | Loop end point (seconds). |
| `loop` | `<voice> <0/1>` | Loop flag. `0` = one-shot (stops at loop end), `1` = loop. |
| `fade` | `<voice> <sec>` | Loop/record crossfade time (seconds). |

### Recording level and overdub

| Message | Args | Description |
|---|---|---|
| `reclevel` | `<voice> <amp>` | Record amplitude — how much of the input is written. |
| `prelevel` | `<voice> <amp>` | Preserve level of existing buffer content. `0` = overwrite, `1` = full overdub (sum on top), in between = decaying overdub. |
| `recoffset` | `<voice> <sec>` | Offset of the record head relative to the play head (seconds; default ≈ -8 samples). |

### Input filter (pre-filter, multimode SVF applied to the record input)

| Message | Args | Description |
|---|---|---|
| `prefc` | `<voice> <Hz>` | Cutoff frequency. |
| `prerq` | `<voice> <rq>` | Reciprocal Q (resonance; smaller = more resonant). |
| `prelp` `prehp` `prebp` `prebr` | `<voice> <mix>` | Low-pass / high-pass / band-pass / band-reject mix amounts. |
| `predry` | `<voice> <amp>` | Dry (unfiltered) input mix. |
| `prefcmod` | `<voice> <amt>` | How much the cutoff tracks playback rate. |

### Output filter (post-filter, multimode SVF applied to playback)

| Message | Args | Description |
|---|---|---|
| `postfc` | `<voice> <Hz>` | Cutoff frequency. |
| `postrq` | `<voice> <rq>` | Reciprocal Q (resonance). |
| `postlp` `posthp` `postbp` `postbr` | `<voice> <mix>` | Low-pass / high-pass / band-pass / band-reject mix amounts. |
| `postdry` | `<voice> <amp>` | Dry (unfiltered) playback mix. |

### Output mix (per-voice level and pan into the stereo mix outlets 6/7)

| Message | Args | Description |
|---|---|---|
| `level` | `<voice> <amp>` | Voice output gain (scales both the per-voice outlet and the mix). Smoothed. |
| `pan` | `<voice> <-1..1>` | Equal-power pan into the stereo mix. `-1` = left, `0` = centre, `1` = right. Smoothed. |
| `levelslew` | `<voice> <sec>` | Smoothing time for `level`. |
| `panslew` | `<voice> <sec>` | Smoothing time for `pan`. |

### Slew (parameter smoothing inside softcut)

| Message | Args | Description |
|---|---|---|
| `recpreslew` | `<voice> <sec>` | Smoothing time for record/pre levels. |
| `rateslew` | `<voice> <sec>` | Smoothing time for rate changes. |

### Phase / sync

| Message | Args | Description |
|---|---|---|
| `quant` | `<voice> <unit>` | Quantization unit for the reported phase. |
| `phaseoffset` | `<voice> <sec>` | Offset applied to the reported phase (seconds). |
| `sync` | `<follow> <lead> <offset>` | Snap voice `follow` to voice `lead`'s position plus `offset` (seconds). |
| `poll` | — | Emit each voice's current play position out the report outlet as `position <p0> <p1> ... <p5>`. |

With `@report <ms>` set to a non-zero interval, the report outlet also automatically emits
`phase <voice> <quantphase>` whenever a voice's quantized phase changes.

### Buffer association

| Message | Args | Description |
|---|---|---|
| `set` | `<buffer~ name>` | Point **all** voices at the named (shared) mono `buffer~`. |
| `voicebuf` | `<voice> <buffer~ name>` | Point **one** voice at its own `buffer~` (overrides the shared default). |

### Routing matrices

| Message | Args | Description |
|---|---|---|
| `inlevel` | `<inlet> <voice> <gain>` | Route signal `inlet` (0..5) into `voice`'s record input at `gain`. Defaults to identity (inlet *v* -> voice *v* at unity); the off-diagonal lets one input feed several voices. |
| `feedback` | `<src> <dst> <gain>` | Route voice `src`'s output into voice `dst`'s record input at `gain` (one block delayed). Enables overdub/looping networks; self-feedback (`src == dst`) is allowed — mind stability. |

A voice's record input is therefore: `sum over inlets ( inlet * inlevel[inlet][voice] ) + sum over
src ( voiceOutput[src] * feedback[src][voice] )`.

### Global

| Message | Args | Description |
|---|---|---|
| `reset` | — | Reset all voices and routing to defaults (rate 1, 1 s loop, looping on, level 1, pan centre, no feedback, identity input routing). |

### Attributes

| Attribute | Description |
|---|---|
| `@report <ms>` | Phase-report interval in milliseconds (`0` = off, the default). |

## Examples

**Basic loop** — record 2 seconds into a mono buffer, then loop it:

```
[buffer~ skbuf 2000]                 (mono, ~96000 samples -> 65536 used at 48k)
[softkut~ skbuf]

set skbuf
loopstart 0 0   ,  loopend 0 2   ,  loop 0 1
rec 0 1  ,  play 0 1              (record while monitoring)
... after 2 s ...
rec 0 0                           (stop recording, keep looping)
```

**Stereo** — two voices, two mono buffers, panned hard L/R:

```
[buffer~ bufL 2000]   [buffer~ bufR 2000]
voicebuf 0 bufL  ,  voicebuf 1 bufR
pan 0 -1  ,  pan 1 1
```

**Feedback overdub** — feed voice 0 into voice 1 to build layers:

```
feedback 0 1 0.8     (voice 0 output -> voice 1 record at 0.8)
```

## Project layout

- `source/include/softkut_engine.h` — host-agnostic engine (voices, command queue, level/pan,
  routing matrices, buffer framing). No Max dependency.
- `source/projects/softkut_tilde/softkut~.cpp` — the Max shell.
- `source/tests/test_engine.cpp` — offline test harness (`make test`).
- `source/thirdparty/softcut-lib` — the upstream softcut library (built statically).
- `help/softkut~.maxhelp` — help patch.

## Credits

`softkut~` wraps [softcut-lib](https://github.com/monome/softcut-lib) by monome (ezra buchla et al.);
see `source/thirdparty/softcut-lib` for its license. The Max object scaffolding follows common
buffer~/MSP idioms (originally modeled on the karma~ external).
