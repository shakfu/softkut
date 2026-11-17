# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Signal rate: `softkut~`'s second inlet takes the rate as a signal, per sample, in place of `@rate`; a float there sets `@rate`. softcut is patched (`// softkut patch` in `Voice::processBlockMono`) to take a per-sample rate target, so no sub-block chunking is needed. Samples are clamped to +/-64, and a non-finite sample repeats the previous one.

- `@syncmode phase` makes `softkut~`'s sync outlet a 0-1 ramp across the loop. The default, `ms`, keeps the head in ms of buffer material.

- Sync outlets: each voice's play head as a signal, for every sample, in ms of buffer material and wrapped to the buffer's length. It is `softkut~`'s outlet 2 and `mc.softkut~`'s multichannel outlet 2. Use `snapshot~` to drive a `waveform~` cursor. softcut is patched (`// softkut patch` in `Voice::processBlockMono`) to write the head position for every sample; it published one value per block.

- Any `buffer~` now works, as with `groove~`. Buffers of any length are used whole (softcut needed a power of two, so only the largest power-of-two prefix was used, e.g. 131072 of 196608 frames). Multichannel buffers are accepted: each voice reads and writes one channel, `@channel`. The buffer's own sample rate is respected: rate 1 plays at recorded speed and times are ms of buffer material. softcut is patched (`// softkut patch` in `SubHead`) to wrap indices by modulo, with a fast path for in-range indices, and to read one channel of an interleaved store. No measurable cost: 95 ns per voice-sample before and after on the engine benchmark.

- `poll` sends `info` out the rightmost outlet: play, record, state (0 stopped, 1 playing, 2 recording, 3 overdub), play head, loop start and loop end, times in ms; `mc.softkut~` sends one list per voice, prefixed with the voice. It replaces 0.1.0's `position` message, which reported raw head positions in seconds. softcut has no state machine, so the state is derived from the play and record flags, and the loop bounds, which softcut does not expose, are cached by the engine.

### Fixed

- A voice that records without playing now outputs silence. softcut's record-only path never wrote the output sample, which `Voice::processBlockMono` left uninitialized, so the voice output whatever the stack held. Patched in the vendored `Voice.cpp`.

- `preshape raised` had no effect unless `recshape` was also `raised`. softcut's pre-fade calculation tested the record shape in the `Raised` branch, so with any other record shape it returned early and kept the previous pre curve. Patched in the vendored `FadeCurves.cpp`.

- A cut sent before the first audio block (`position`, or `play` after `reset`) now records where it plays. The engine applied commands before binding the voice's buffer, so softcut computed the write index against a length of 0: the write head started at frame 0 while the read head jumped to the target. Times were also scaled by the previous buffer's sample rate. Buffers are now bound before commands are applied.

- The play head reported by `info` and the sync outlets stays inside the buffer. With a loop point past the buffer's end, softcut wraps its read index but its head position runs on, so a 0.5 s buffer with a 2 s loop reported positions up to 2 s and a `waveform~` cursor left the display.

- A `buffer~` that shrinks while a voice records is no longer written past its end. softcut indexes the buffer with its write index before wrapping it, so a write index beyond the new length wrote out of bounds; it is now re-wrapped when the length changes.

- softcut no longer reads an uninitialized phase quantum. It updates a quantized phase every sample, but never initializes `Voice::phaseQuant`; the engine sets it to 0 on creation and `reset`.

- The `mc.softkut~` help patch now opens from the object. It was named `mc.softcut~.maxhelp`, so Max never found it.

- Out-of-range parameter values no longer corrupt the engine or the `buffer~`. softcut validates nothing: `|rate| > 64` overran the resampler's 64-frame output buffer and crashed, `prerq`/`postrq <= 0` and negative slew times diverged to NaN that recording wrote into the buffer, and `prelevel`/`reclevel` above 1 grew the buffer on every pass toward inf. Values are now clamped (`rate` to +/-64, `rq` >= 0.01, `prelevel`/`reclevel` to 0..1, times >= 0, positions to 0..2^30 frames) and non-finite values are rejected, with a warning in the Max console. The engine re-checks on the audio thread, so direct engine callers get the same guarantee.

- A sample-rate change no longer rescales loops. softcut converts loop points, fade time and the record offset to frames at the rate current when they are set, so a loop set at 48 kHz played 0.5 s per second at 96 kHz. The engine caches these and re-applies them on every sample-rate change.

- Commands are no longer lost when two threads send messages at once. Max delivers messages on the main and scheduler threads concurrently under Overdrive, and the command queue is single-producer. `Engine::push` now serializes producers with a mutex; the audio thread never takes it.

- `play` (or `rec`) without a prior `position` now respects the loop. softcut's `ReadWriteHead::run()` started the head without setting its `active_` flag, which only a position cut set, and the loop-bound check runs only on active heads. The head therefore ran through the whole buffer and wrapped at its end. Patched in the vendored softcut (`// softkut patch` in `ReadWriteHead.cpp`) rather than issuing an implicit cut from the engine, so the fix holds for every caller of softcut.

- `dsp64` no longer modifies state that the audio thread is reading. Max can recompile a running chain, and `dsp64` reset every voice's sample rate, cleared the feedback store (1 MiB at 16 voices), and (in `mc.softkut~`) freed the silent-input buffer the previous perform routine still used. `setSampleRate` now records the rate and `process()` applies it on the audio thread before draining commands. A null input pointer now means silence, so `mc.softkut~` no longer needs that buffer.

- `poll` no longer races with the audio thread. `Voice::playFlag`/`recFlag` are plain bools that the DSP thread clears when a record-once pass finishes, so reading them from the control thread was undefined behavior. The engine now publishes one 64-bit snapshot per voice from the audio thread at the end of every processed chunk -- transport code (`rec<<1|play`) in the high half, loop-normalized position in the low half -- and `getVoiceInfo` unpacks both from a single load, so an `info` report cannot pair one block's state with another block's position. `test_poll_during_record_once` exercises the path; ThreadSanitizer reports the old read path and is clean on the new one.

- A record-once pass now marks its `buffer~` dirty. The wrappers keyed `buffer_setdirty` on the record flag still being set after `process()`, but a record-once pass clears that flag inside the same call that performs its last writes. When the loop window is shorter than one signal vector the whole pass fits in one block and the buffer was never marked dirty at all. The engine now reports `getWroteBlock(voice)` and the wrappers key on that.

- Signal vectors larger than 8192 samples are split into 8192-sample chunks instead of clamped. The engine's scratch buffers and feedback store hold one chunk, so a clamped pass left every output sample past 8192 unwritten (stale audio). Splitting keeps feedback one chunk delayed rather than one vector delayed; no other host-visible behavior changes.

### Changed

- **Breaking:** `softkut~` is one voice, and its parameters are attributes: saved with the patcher, shown in the inspector, readable with `getattr`, usable with `pattr`. Messages lose the voice argument. Transport (`play`, `rec`, `reconce`, `position`, `stop`) stays as messages, since a saved play flag would start the voice on load. The inlets are the record input and the rate; the outlets are audio, sync, and a dumpout for `info` and attribute values. `info` is now `info <play> <rec> <state> <position> <start> <end>`, times in ms; `start` and `end` are the loop played, so `@loopend 0` reports the buffer's end. Use one object per voice. Design: `docs/dev/m3-design.md`.

  ```
  [softkut~ loop @loopend 2000 @rate 0.5 @prelevel 0.6]
  ```

  Removed from `softkut~`: the voice-count argument, `voicebuf` (use `@buffer` and `@channel`), `level`, `levelslew`, `pan`, `panslew`, `inlevel`, `feedback`, `sync`, `enable`, `@report`, `phase`, `quant` and `phaseoffset`. Each has a Max replacement listed in the design document.

- **Breaking:** `mc.softkut~` follows the same model. `@chans` (1-16, default 1, fixed at creation) replaces the positional voice count, and every voice attribute is an array: one value sets every voice, a list sets them in order, and `setvalue <voice> <name> <value>` addresses one voice. `play`, `rec` and `position` take one value or one per voice. Inlet 2 takes one rate channel per voice. `@channel 0`, the default, gives voice *v* channel *v*, wrapping. `sync` stays, since voices in one object can be aligned sample-exactly, and so does `feedback <src> <dst> <gain>`, now a saved attribute. The same network in Max needs `mc.matrix~` and an `mc.send~`/`mc.receive~` loop. Routes at gain 0 are skipped; every route used to update its ramp every sample, 512 updates per sample at 16 voices. `info` is `info <voice> <play> <rec> <state> <position> <start> <end>`. The messages removed from `softkut~` are removed here too, except `sync` and `feedback`. Its help patch is rewritten (four tabs) and it gains `docs/mc.softkut~.maxref`; both maxrefs are generated by `scripts/make_maxref.py` (`make maxref`).

  ```
  [mc.softkut~ loop @chans 3 @rate 1 0.5 -1]
  ```

- The default loop is the whole buffer: loop end `0` means the buffer's end, and follows the buffer when it is resized. It was 0-1000 ms, so a longer file played only its first second.

- Recording is transparent by default: a DC input of 0.5 is stored as 0.5. softcut colored it three ways: a 16 kHz low-pass on the input, a 1.2x soft clip, and a record fade curve (`Raised`) whose negative sign stored the input inverted. The defaults are now `predry 1`, `prelp 0`, `recshape linear` and `softclip 0`; `recshape raised` and `softclip 1` restore softcut's sound. softcut is patched (`// softkut patch` in `SubHead`, `Voice`, `Softcut`) to switch the soft clip at run time and to set the fade shapes per voice.

- Stored samples are bounded to +/-32 (+30 dBFS), and a NaN is stored as 0. The soft clip had been the only limit on the write path: without it, feedback with a loop gain above 1 grew the buffer to inf, and a NaN input stayed in the buffer for good. The bound is far above any normal level, so it leaves recordings unchanged. Patched in `SubHead::poke`.

- `stop` clears the play and record flags in one command, as `play 0, rec 0` would. It parked the heads and left both flags set. `reconce` sets the record flag itself; it needed `rec 1` as well.

- `recoffset` is in samples of buffer material, default -8 (softcut's). It was in ms, which made the default depend on the sample rate.

- **Breaking:** voices and channels count from 1, and every time is in ms of buffer material: `loopstart`, `loopend`, `position`, `fade`, the slews and the `sync` offset. Clamp warnings show these units. The engine keeps seconds and 0-based voices; `source/include/softkut_units.h` is the one conversion, covered by the offline tests.

- Both help patches are rewritten: `softkut~.maxhelp` in seven tabs (basic looping, stereo buffers, buffer sample rate, record and overdub, rate signal, play head and attributes, value limits) and `mc.softkut~.maxhelp` in four (voices as channels, stereo and sync, rate signals, recording one voice). Each shows its voices with the `patchers/softkut.view.maxpat` bpatcher (arguments: buffer~, channel): the `buffer~` waveform, a moving play head from the sync outlet, the loop window and the state from `info`. The stereo tabs play `media/softkut-stereo.wav` (bells left, bass right), written by `scripts/make_media.py`. Both helps and the view are generated with py2max (`make maxhelp`, `scripts/make_help.py`), which fails on overlapping boxes or out-of-range cords; the view's logic is laid out with OGDF's Sugiyama layout, one seeded run, so the files are identical on every build.

- `reset` also clears the feedback history, so no audio from before the reset re-enters a voice.

- `make build` is incremental; it no longer runs `make clean` first.

- CI runs `make test` on macOS for every push to `main` and every pull request (`.github/workflows/test.yml`).

### Removed

- `@report` and the `phase` reports it clocked, with `quant` and `phaseoffset`: the sync outlets carry the play head for every sample, and `snapshot~` samples it. With the clock go two 0.1.0 defects: `@report` set while audio ran did nothing until DSP restarted, and deleting an object with reports armed under Overdrive could read the freed engine.

- The engine's norns client layer, except the feedback matrix: output `level` and `pan` with their slews, the stereo mix, the `inlevel` matrix and `enable`. No shell sends these since the attribute redesign; Max patching covers each (`docs/dev/m3-design.md`). `Engine::process()` no longer takes mix outputs.

## [0.1.0] - 2026-06-27

Initial implementation: a Max/MSP wrapper around monome's softcut-lib, plus a multichannel variant, built on a shared host-agnostic engine.

### Added

#### `softkut~` external

- A variable number of softcut voices (the optional `<channels>` creation argument, default 1, max 6) reading/writing a shared (or per-voice) mono `buffer~`, zero-copy: voices point directly at the locked `buffer~` samples each perform block. One signal inlet (record input) and one signal outlet per voice, plus the stereo-mix pair and a report outlet.

- Full per-voice control surface: `rate`, `loopstart`, `loopend`, `loop`, `fade`, `reclevel`, `prelevel`, `rec`, `play`, `reconce`, `position`, `recoffset`, `stop`.

- Input (pre) and output (post) multimode SVF filters: `prefc`/`prerq`/`prelp`/ `prehp`/`prebp`/`prebr`/`predry`/`prefcmod` and the matching `post*`.

- Per-voice output `level` and equal-power `pan` (smoothed) feeding a stereo mix outlet pair, with `levelslew`/`panslew`; plus `recpreslew`/`rateslew`.

- Phase/sync: `quant`, `phaseoffset`, `sync`, and a `poll` / `@report <ms>` reporting outlet (race-free position + quantized-phase events).

- Per-voice `buffer~` assignment via `voicebuf` (stereo = two mono buffers); perform dedup-locks distinct buffers.

- Voice-to-voice `feedback` matrix (one-block-delayed) for overdub/looping networks.

- Inlet-to-voice `inlevel` input routing matrix (default identity).

- Per-voice `enable` gate (skips processing entirely).

- `reset` to restore per-voice and routing defaults.

#### `mc.softkut~` external

- Multichannel variant sharing the same engine and control surface, presented through Max's MC system: one `Z_MC_INLETS` record-input inlet, one multichannel voice-output outlet, and a message (report) outlet.

- Variable voice count via the second creation argument (default 6, cap 16), which sets the output channel count.

#### Engine and infrastructure

- `softkut_engine.h`: host-agnostic `softcut::Engine` owning the softcut voices, a lock-free single-producer/single-consumer command queue, double<->float block conversion, power-of-two buffer framing, level/pan ramps, the feedback and input matrices, and phase-poll bookkeeping. Runtime-settable active voice count over a compile-time maximum.

- `softkut_control.h`: shared command table + dispatch used by both shells so their control surfaces cannot drift.

- Offline test harness (`make test`, via ctest) covering the queue, buffer framing, command routing, record/playback, level, pan, per-voice buffers, stop, feedback, enable, input matrix, and runtime voice count.

#### Documentation

- `README.md` full message-API guide and examples.

- `docs/softkut~.maxref` structured object reference.

- `help/softkut~.maxhelp` help patch.

- `TODO.md` investigation/spec for the deferred fade-curve shaping work.

### Changed

- Build system: softcut-lib is built once as a static (PIC) library shared by both externals and the test harness; top-level CMake wires the shared include paths; `Makefile` gains a `test` target and fixes the `clean` typo.

- `package-info.json` updated from the template placeholder to softkut metadata.

### Removed

- Vendored `karma~` sources (used only as a scaffolding reference; no karma code was ever compiled or linked).

- Template `example` object, help, and maxref stubs.

### Notes

- softcut's record path is intentionally colored (a ~1.2x soft-clip and a polarity-inverting default `Raised` rec-fade curve), so recorded audio returns gained and inverted; this is upstream behavior, documented in the tests.

- `buffer~` must be mono; only its largest power-of-two prefix is used.

- File I/O (load/save/clear) is delegated to Max's `buffer~`, not reimplemented.

- Fade-curve shaping is the one softcut-lib capability not yet exposed (locked behind `Voice`'s private state); investigated and deferred — see `TODO.md`.

[Unreleased]: <https://github.com/shakfu/softkut/compare/v0.1.0...HEAD> [0.1.0]: <https://github.com/shakfu/softkut>
