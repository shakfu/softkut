# softkut~

A Max/MSP external wrapping monome's [softcut-lib](https://github.com/monome/softcut-lib), the real-time buffer looping and resampling engine from the [norns](https://monome.org/norns/) sound computer. Each softcut voice reads and writes one channel of a Max `buffer~`, with sub-sample looping, crossfaded overdub, resampling read/write heads, and input and output multimode filters.

The DSP lives in a host-agnostic engine (`source/include/softkut_engine.h`); the Max externals are thin shells over it. Control messages reach the audio thread through a lock-free command queue.

The package builds two externals over the same engine: **`softkut~`** (one voice, parameters as attributes) and **`mc.softkut~`** (1-16 voices as multichannel signals, array attributes; see [Multichannel variant](#multichannel-variant-mcsoftkut)).

## Building

```text
git submodule update --init --recursive   # max-sdk-base (softcut-lib is vendored)
make build                                 # configure + build (universal mac binary)
make test                                  # build + run the offline engine test harness
```

`make build` produces `externals/softkut~.mxo` and `externals/mc.softkut~.mxo`. `make link` symlinks the repo into your Max `Packages` folder so Max can find the externals and the help patch. The build is also a standard CMake project (`cmake .. && cmake --build .`).

## The object

```
[softkut~ <buffer~ name> @attribute value ...]
```

`softkut~` is one softcut voice. Use one object per voice: three play heads on one buffer are three objects naming the same `buffer~`.

| Port | Content |
|-|-|
| Inlet 1 | Record input (signal). Takes every message. |
| Inlet 2 | Rate. A signal sets the rate per sample, in place of `@rate`. A number sets `@rate`. |
| Outlet 1 | Audio. |
| Outlet 2 | Play head (signal): ms in the buffer, or 0-1 across the loop with `@syncmode phase`. |
| Outlet 3 | `info` lists and attribute values (`getrate` replies `rate 1.`). |

Parameters are attributes. They save with the patcher, appear in the inspector, answer `getattr`, and work with `pattr` and `attrui`. Set one in the box (`@fade 20`) or by message (`fade 20`). Transport stays as messages, since a saved play flag would start the voice when the patcher opens.

Values that would destabilize softcut are clamped with a console warning, and the attribute keeps the clamped value. Non-finite values are rejected.

### Buffer requirements

Any `buffer~` works, as with `groove~`:

- **Any length.** The whole buffer is used. Loop points past its end wrap to its start.

- **Any channel count.** The voice reads and writes channel `@channel` (default 1). A stereo loop is two objects, `@channel 1` and `@channel 2`, started by the same message.

- **Its own sample rate.** Rate `1` plays the buffer at its recorded speed, and times are ms of buffer material, so a 44.1 kHz file loops correctly in a 48 kHz patch.

File loading, saving and clearing are `buffer~`'s job (`read`, `replace`, `write`, `crop`, `clear`).

### Attributes

Times are ms of buffer material; levels are linear amplitudes.

| Attribute | Range | Default | Description |
|-|-|-|-|
| `buffer` | | argument 1 | The `buffer~`. |
| `channel` | >= 1 | 1 | The `buffer~` channel read and written. |
| `syncmode` | `ms`, `phase` | `ms` | Outlet 2: ms in the buffer, or 0-1 across the loop. |
| `rate` | -64..64 | 1 | `0.5` = octave down, negative = reverse. Ignored while inlet 2 has a signal. |
| `rateslew` | >= 0 ms | 1 | Rate smoothing; also smooths the rate signal. |
| `loopstart` | >= 0 ms | 0 | Loop start. |
| `loopend` | >= 0 ms | 0 | Loop end; `0` = the buffer's end. |
| `loop` | 0/1 | 1 | `0` = one-shot. |
| `fade` | >= 0 ms | 10 | Loop and cut crossfade. |
| `reclevel` | 0..1 | 1 | How much input is written. |
| `prelevel` | 0..1 | 0 | How much existing content is kept: `0` overwrites, `1` sums. |
| `recpreslew` | >= 0 ms | 1 | Smoothing of `reclevel` and `prelevel`. |
| `recoffset` | samples | -8 | Record head offset from the play head. |
| `recshape` | `linear`, `sine`, `raised` | `linear` | Record fade curve. `raised` is softcut's default and stores the input inverted. |
| `preshape` | `linear`, `sine`, `raised` | `linear` | Pre fade curve. |
| `softclip` | 0/1 | 0 | softcut's soft clip on the record input (1.2x gain below its knee). |
| `prefc` | >= 10 Hz | 16000 | Input filter cutoff. softcut caps it at 0.4 x the sample rate. |
| `prerq` | >= 0.01 | 4 | Input filter reciprocal Q. |
| `prelp` `prehp` `prebp` `prebr` | 0..1 | 0 | Input filter low-pass, high-pass, band-pass, band-reject mix. |
| `predry` | 0..1 | 1 | Unfiltered input mix. |
| `prefcmod` | 0..1 | 1 | How far the input cutoff follows the rate. |
| `postfc` | >= 10 Hz | 12000 | Output filter cutoff. |
| `postrq` | >= 0.01 | 4 | Output filter reciprocal Q. |
| `postlp` `posthp` `postbp` `postbr` | 0..1 | 0 | Output filter mixes. |
| `postdry` | 0..1 | 1 | Unfiltered output mix. |

Recording is transparent by default: no input filter, no soft clip, no inversion. `recshape raised` with `softclip 1` (and `prelp 1`, `predry 0`) gives softcut's colored record path. Stored samples are bounded to +/-32, so a feedback loop saturates instead of filling the `buffer~` with inf.

### Messages

| Message | Args | Description |
|-|-|-|
| `play` | `0/1` | Start or stop playback. Playback starts inside the loop. |
| `rec` | `0/1` | Start or stop recording. |
| `reconce` | none | Record one loop pass, from the next loop start or `position`, then stop recording. |
| `stop` | none | `play 0` and `rec 0` in one command. |
| `position` | `<ms>` | Jump the play head, with a crossfade. |
| `set` | `<buffer~> [<channel>]` | Set `@buffer`, and `@channel` if given. |
| `poll` | none | Send `info <play> <rec> <state> <position> <start> <end>` out outlet 3. Times are ms; `state` is 0 stopped, 1 playing, 2 recording, 3 overdub; `start` and `end` are the loop played. |
| `reset` | none | Stop, and restore every attribute default. `@buffer` and `@channel` are kept. |

### Patching what the object no longer does

| Task | Max patching |
|-|-|
| Output level, pan | `*~`, `gain~`, `pan~`-style patching |
| Input routing | cords and `*~` into inlet 1 |
| Feedback between voices | `mc.softkut~`'s `feedback`, or `send~`/`receive~` into inlet 1 (one vector delayed) |
| Align one voice to another | `snapshot~` on the other's outlet 2, into `position` |
| Quantized position reports | `snapshot~` on outlet 2, then `round` |

## Examples

**Basic loop**: record 2 s into a buffer, then loop it.

```
[buffer~ skbuf 2000]
[softkut~ skbuf]

loopend 2000, rec 1, play 1     (record while monitoring)
... after 2 s ...
rec 0                           (stop recording, keep looping)
```

**Three play heads** on one mono buffer, one recording:

```
[softkut~ loop @loopend 2000 @prelevel 0.6]                  records and plays
[softkut~ loop @loopend 2000 @rate 0.5]                      octave down
[softkut~ loop @loopstart 500 @loopend 1500 @rate -1]        reverse sub-loop
```

**Stereo**: two objects on one stereo `buffer~`, both sent `play 1`.

```
[buffer~ skbuf 2000 2]
[softkut~ skbuf @channel 1]   [softkut~ skbuf @channel 2]
```

## Viewing a voice

`patchers/softkut.view.maxpat` shows one voice: the `buffer~` channel it reads, a moving play head, its loop window and its state. Use it in a `bpatcher` (patcher file `softkut.view.maxpat`, 520 x 180) with two arguments: the `buffer~` name and the channel, e.g. `skbuf 1`. Connect `softkut~`'s outlet 3 to its left inlet, outlet 2 to its right inlet, and its outlet back to `softkut~`'s left inlet. It polls `info` every 100 ms.

## Multichannel variant: `mc.softkut~`

```
[mc.softkut~ <buffer~ name> @chans <voices> @attribute values ...]
```

`mc.softkut~` is `@chans` voices (1-16, default 1) in one object. Voice *v* is channel *v* of every signal port.

| Port | Content |
|-|-|
| Inlet 1 | Record inputs (MC): channel *v* feeds voice *v*. A voice past the cord's channel count records silence. Takes every message. |
| Inlet 2 | Rates (MC): channel *v* is voice *v*'s rate. A voice past the channel count uses `@rate`. A number sets `@rate` for every voice. |
| Outlet 1 | Audio (MC), one channel per voice. |
| Outlet 2 | Play heads (MC), as `softkut~`'s outlet 2. |
| Outlet 3 | `info` lists and attribute values. |

Every `softkut~` attribute is an array here, one value per voice. One value sets every voice; a list sets the voices in order. `getrate` replies with one value per voice.

```
[mc.softkut~ loop @chans 3 @loopend 2000 @rate 1 0.5 -1]

rate 1                    every voice
rate 1 0.5 -1             one value per voice
setvalue 2 rate 2         voice 2 only
```

`@channel` is per voice too. `0`, the default, gives voice *v* channel *v*, wrapping: a stereo `buffer~` plays as voices 1 and 2 with no setup. `@chans` is fixed at creation, since it sets the outlets' channel counts.

| Message | Args | Description |
|-|-|-|
| `play`, `rec` | `<0/1>` or one per voice | Transport for every voice, or per voice. |
| `position` | `<ms>` or one per voice | Jump the play heads. |
| `reconce`, `stop`, `reset` | none | As `softkut~`, for every voice. |
| `setvalue` | `<voice> <name> <args>` | One voice: any attribute, `channel`, `play`, `rec`, `position`, `reconce` or `stop`. |
| `sync` | `<follow> <lead> [<offset ms>]` | Jump voice `follow` to voice `lead`'s position plus an offset, sample-exact. |
| `feedback` | `<src> <dst> <gain> ...` | Route voice `src`'s output into voice `dst`'s record input, one signal vector later. Other routes keep their gains; `0` removes a route. Also an attribute, saved with the patch; `getfeedback` lists the routes. `reset` clears them. |
| `set` | `<buffer~> [<channel>]` | Set `@buffer`, and `@channel` for every voice if given. |
| `poll` | none | One `info <voice> <play> <rec> <state> <position> <start> <end>` per voice. |

A mono cord into inlet 1 feeds voice 1 only, so `rec 1` makes the other voices record silence. Record one voice with `setvalue 1 rec 1`, or spread the input with `mc.pack~` or `mc.dup~`.

## Project layout

- `source/include/softkut_engine.h` — host-agnostic engine (voices, command queue, buffer binding and rate scaling, rate and sync signals, runtime voice count). No Max dependency.

- `source/include/softkut_control.h` — the parameter table, value checks and `buffer~` helpers, shared by both shells.

- `source/include/softkut_units.h` — message units: ms and voices from 1 in messages, seconds and 0-based in the engine.

- `source/projects/softkut_tilde/softkut~.cpp` — one-voice Max shell (attributes).

- `source/projects/mc.softkut_tilde/mc.softkut~.cpp` — multichannel shell (1-16 voices, array attributes).

- `source/tests/test_engine.cpp` — offline test harness (`make test`).

- `source/thirdparty/softcut-lib` — the upstream softcut library (built statically).

- `help/softkut~.maxhelp`, `help/mc.softkut~.maxhelp` — help patches, generated by `scripts/make_help.py` (`make maxhelp`).

- `docs/softkut~.maxref`, `docs/mc.softkut~.maxref` — object references (Max ref panel), generated by `scripts/make_maxref.py` (`make maxref`).

- `CHANGELOG.md` — release notes. `TODO.md` — deferred work (fade-curve shaping).

## Credits

`softkut~` wraps [softcut-lib](https://github.com/monome/softcut-lib) by monome (ezra buchla et al.); see `source/thirdparty/softcut-lib` for its license. The Max object scaffolding follows common buffer~/MSP idioms (originally modeled on the karma~ external).
