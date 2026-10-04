# softkut~

A Max/MSP external wrapping monome's [softcut-lib](https://github.com/monome/softcut-lib), the real-time buffer looping and resampling engine from the [norns](https://monome.org/norns/) sound computer. Each softcut voice reads and writes one channel of a Max `buffer~`, with sub-sample looping, crossfaded overdub, resampling read/write heads, input and output multimode filters, and per-voice output level.

The DSP lives in a host-agnostic engine (`source/include/softkut_engine.h`); the Max externals are thin shells over it. Control messages reach the audio thread through a lock-free command queue.

The package builds two externals over the same engine and message API: **`softkut~`** (discrete per-voice inlets and outlets, 1-6 voices) and **`mc.softkut~`** (multichannel inlets and outlets, 1-16 voices; see [Multichannel variant](#multichannel-variant-mcsoftkut)).

## Building

```
git submodule update --init --recursive   # max-sdk-base (softcut-lib is vendored)
make build                                 # configure + build (universal mac binary)
make test                                  # build + run the offline engine test harness
```

`make build` produces `externals/softkut~.mxo` and `externals/mc.softkut~.mxo`. `make link` symlinks the repo into your Max `Packages` folder so Max can find the externals and the help patch. The build is also a standard CMake project (`cmake .. && cmake --build .`).

## The object

```
[softkut~ <buffer~ name> <voices> @report <ms>]
```

`<voices>` is the voice count, default **1**, maximum **6**. `[softkut~ skbuf]` is a one-voice looper; `[softkut~ skbuf 4]` has four voices.

- **Inlets** (one signal per voice): voice *v*'s record input. Control messages go to the **left inlet**.

- **Outlets**, left to right: one audio outlet per voice, then one **sync** outlet per voice, then the **report** outlet.

The sync outlet is the voice's play head as a signal, in ms of buffer material, wrapped to the buffer's length, as with `groove~`'s sync outlet. `[snapshot~ 30]` turns it into numbers for `waveform~`.

All voices share the `buffer~` named in the first argument (or via `set`). Each voice can instead read its own `buffer~` with `voicebuf`.

### Buffer requirements

Any `buffer~` works, as with `groove~`:

- **Any length.** The whole buffer is used. Loop points past its end wrap to its start.

- **Any channel count.** Each voice reads and writes one channel. By default voice 1 uses channel 1, voice 2 channel 2, and so on, wrapping when there are more voices than channels. `set` and `voicebuf` take an optional channel to override this.

- **Its own sample rate.** Rate `1` plays the buffer at its recorded speed, and times are ms of buffer material, so a 44.1 kHz file loops correctly in a 48 kHz patch.

File loading, saving and clearing are `buffer~`'s job (`read`, `replace`, `write`, `crop`, `clear`).

## Message API

Most messages take `<message> <voice> <value>`. Voices and channels count from **1**. Times are in **ms**, gains and levels are linear amplitudes, flags are `0`/`1`.

Values that would destabilize softcut are clamped with a console warning: `rate` to -64..64, `prerq`/`postrq` to >= 0.01, `reclevel`/`prelevel` to 0..1, times and slews to >= 0, positions to >= 0. Non-finite values are rejected.

### Transport

| Message | Args | Description |
|---|---|---|
| `play` | `<voice> <0/1>` | Start or stop playback (reading). Playback starts inside the loop. |
| `rec` | `<voice> <0/1>` | Start or stop recording (writing). |
| `reconce` | `<voice> <0/1>` | Record one loop pass, then clear the record flag. |
| `stop` | `<voice>` | Park the voice's heads at once (silent); unlike `play 0`, leaves the flags set. |
| `position` | `<voice> <ms>` | Jump the play head to a position in the buffer. |
| `enable` | `<voice> <0/1>` | Gate a voice. Off skips it entirely: no audio, no recording, no feedback. Default on. |

### Loop and rate

| Message | Args | Description |
|---|---|---|
| `rate` | `<voice> <ratio>` | `1` = recorded speed, `0.5` = octave down, `2` = octave up, negative = reverse. |
| `loopstart` | `<voice> <ms>` | Loop start. |
| `loopend` | `<voice> <ms>` | Loop end. |
| `loop` | `<voice> <0/1>` | `0` = one-shot (stops at the loop end), `1` = loop. |
| `fade` | `<voice> <ms>` | Loop and record crossfade time. |

### Recording level and overdub

| Message | Args | Description |
|---|---|---|
| `reclevel` | `<voice> <amp>` | How much of the input is written. |
| `prelevel` | `<voice> <amp>` | How much existing content is kept: `0` overwrites, `1` sums on top, between decays. |
| `recoffset` | `<voice> <ms>` | Record head offset from the play head (default -8 samples). |

### Input filter (multimode SVF on the record input)

| Message | Args | Description |
|---|---|---|
| `prefc` | `<voice> <Hz>` | Cutoff frequency. |
| `prerq` | `<voice> <rq>` | Reciprocal Q (smaller = more resonant). |
| `prelp` `prehp` `prebp` `prebr` | `<voice> <mix>` | Low-pass, high-pass, band-pass, band-reject mix. |
| `predry` | `<voice> <amp>` | Dry (unfiltered) input mix. |
| `prefcmod` | `<voice> <amt>` | How much the cutoff tracks the rate. |

### Output filter (multimode SVF on playback)

| Message | Args | Description |
|---|---|---|
| `postfc` | `<voice> <Hz>` | Cutoff frequency. |
| `postrq` | `<voice> <rq>` | Reciprocal Q. |
| `postlp` `posthp` `postbp` `postbr` | `<voice> <mix>` | Low-pass, high-pass, band-pass, band-reject mix. |
| `postdry` | `<voice> <amp>` | Dry (unfiltered) playback mix. |

### Output level

| Message | Args | Description |
|---|---|---|
| `level` | `<voice> <amp>` | Voice output gain, smoothed. |
| `levelslew` | `<voice> <ms>` | Smoothing time for `level`. |

Neither object has a stereo mix outlet. `pan` and `panslew` are accepted but do nothing; place voices in stereo downstream.

### Slew (smoothing inside softcut)

| Message | Args | Description |
|---|---|---|
| `recpreslew` | `<voice> <ms>` | Smoothing time for record and pre levels. |
| `rateslew` | `<voice> <ms>` | Smoothing time for rate changes. |

### Phase, sync and reports

| Message | Args | Description |
|---|---|---|
| `quant` | `<voice> <ms>` | Report `phase` in steps of this size (`0` = unquantized). |
| `phaseoffset` | `<voice> <ms>` | Offset added to the reported `phase`. softcut applies it only when `quant` > 0. |
| `sync` | `<follow> <lead> <offset ms>` | Jump voice `follow` to voice `lead`'s position plus `offset`. |
| `poll` | none | Send one `info` list per voice out the report outlet. |

The report outlet sends:

- `phase <voice> <ms>`: every `@report` ms while the patcher's audio is on, for each voice whose position changed. The position in the buffer, wrapped to its length.

- `info <voice> <pos> <play> <rec> <start> <end> <window> <state> <position>`: on `poll`. `pos` is 0-1 within the loop window; `start`, `end`, `window` and `position` are ms; `state` is 0 stopped, 1 playing, 2 recording, 3 overdub.

### Buffer association

| Message | Args | Description |
|---|---|---|
| `set` | `<buffer~ name> [<channel>]` | All voices read the named `buffer~`. With a channel, every voice uses it; without, voice *v* uses channel *v*, wrapping. |
| `voicebuf` | `<voice> <buffer~ name> [<channel>]` | One voice reads its own `buffer~` and channel (overrides `set`). |

### Routing matrices

| Message | Args | Description |
|---|---|---|
| `inlevel` | `<inlet> <voice> <gain>` | Route signal `inlet` into `voice`'s record input. Default identity (inlet *v* -> voice *v* at unity). |
| `feedback` | `<src> <dst> <gain>` | Route voice `src`'s output (before `level`) into voice `dst`'s record input, one block delayed. Self-feedback is allowed. |

### Global

| Message | Args | Description |
|---|---|---|
| `reset` | none | Stop every voice and restore every default (rate 1, 0-1000 ms loop, looping on, level 1, no feedback, identity input routing). The next `play` starts from the loop start. The `buffer~` assignment and `@report` are kept. |

### Attributes

| Attribute | Description |
|---|---|
| `@report <ms>` | `phase` report interval (`0` = off, the default). Takes effect at once. |

## Examples

**Basic loop**: record 2 s into a buffer, then loop it.

```
[buffer~ skbuf 2000]
[softkut~ skbuf]

loopend 1 2000, rec 1 1, play 1 1     (record while monitoring)
... after 2 s ...
rec 1 0                               (stop recording, keep looping)
```

**Stereo**: two voices on one stereo `buffer~`. Voice 1 reads the left channel, voice 2 the right; outlets 1 and 2 are their audio.

```
[buffer~ skbuf 2000 2]
[softkut~ skbuf 2]
```

**Feedback overdub**: feed voice 1 into voice 2 to build layers.

```
feedback 1 2 0.8
```

## Viewing a voice

`patchers/softkut.view.maxpat` shows one voice: the `buffer~` channel it reads, a moving play head, its loop window and its state. Use it in a `bpatcher` (patcher file `softkut.view.maxpat`, 520 x 180) with three arguments: the `buffer~` name, the channel and the voice, e.g. `skbuf 1 1`. Connect `softkut~`'s report outlet to its left inlet, the voice's sync outlet to its right inlet, and its outlet back to `softkut~`'s left inlet. It polls `info` every 100 ms.

## Multichannel variant: `mc.softkut~`

`mc.softkut~` presents the same engine through Max's multichannel signals:

```
[mc.softkut~ <buffer~ name> <voices>]    e.g. [mc.softkut~ skbuf 8]
```

- **Inlet**: a multichannel record input; channel *v* feeds voice *v*. A mono cord or an N-channel cord both work. Control messages go here.

- **Outlet 1**: the voices' audio, one channel per voice (`mc.unpack~` to split).

- **Outlet 2**: the voices' sync signals, one channel per voice.

- **Outlet 3**: reports.

The voice count (default 6, maximum 16) is the channel count of both signal outlets. The message API is identical; `inlevel`'s inlet index is the input channel.

## Project layout

- `source/include/softkut_engine.h` — host-agnostic engine (voices, command queue, level/pan, routing matrices, buffer framing, runtime voice count). No Max dependency.

- `source/include/softkut_control.h` — shared control-message table, dispatch and reports (used by both shells).

- `source/include/softkut_units.h` — message units: ms and voices from 1 in messages, seconds and 0-based in the engine.

- `source/projects/softkut_tilde/softkut~.cpp` — discrete Max shell (1-6 voices).

- `source/projects/mc.softkut_tilde/mc.softkut~.cpp` — multichannel shell (variable voices).

- `source/tests/test_engine.cpp` — offline test harness (`make test`).

- `source/thirdparty/softcut-lib` — the upstream softcut library (built statically).

- `help/softkut~.maxhelp` — help patch.

- `docs/softkut~.maxref` — structured object reference (Max ref panel).

- `CHANGELOG.md` — release notes. `TODO.md` — deferred work (fade-curve shaping).

## Credits

`softkut~` wraps [softcut-lib](https://github.com/monome/softcut-lib) by monome (ezra buchla et al.); see `source/thirdparty/softcut-lib` for its license. The Max object scaffolding follows common buffer~/MSP idioms (originally modeled on the karma~ external).
