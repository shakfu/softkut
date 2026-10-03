# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Any `buffer~` now works, as with `groove~`. Buffers of any length are used whole (softcut needed a power of two, so only the largest power-of-two prefix was used, e.g. 131072 of 196608 frames). Multichannel buffers are accepted: each voice reads and writes one channel, voice *v* defaulting to channel *v* mod the channel count, and `set`/`voicebuf` take an optional 1-based channel. The buffer's own sample rate is respected: rate 1 plays at recorded speed and times are seconds of buffer material. softcut is patched (`// softkut patch` in `SubHead`) to wrap indices by modulo, with a fast path for in-range indices, and to read one channel of an interleaved store. No measurable cost: 95 ns per voice-sample before and after on the engine benchmark. The engine's `process()` now takes one `BufferView` per voice.

- karma~-style per-voice metadata report: sending `poll` now emits one `info <voice> <pos> <play> <rec> <startMs> <endMs> <windowMs> <state>` list per voice on the message outlet (both `softkut~` and `mc.softkut~`). `pos` is normalized to the loop window (0..1); `state` is synthesized from the play/rec
  flags as `(rec<<1)|play` (0=stop, 1=play, 2=record, 3=overdub), since softcut
  has no state machine. Backed by the engine's `getVoiceInfo`/`VoiceInfo`, which caches the (otherwise write-only) loop bounds. Covered by `test_voice_info`.

### Fixed

- A `buffer~` that shrinks while a voice records is no longer written past its end. softcut indexes the buffer with its write index before wrapping it, so a write index beyond the new length wrote out of bounds; it is now re-wrapped when the length changes.

- `phase` reports no longer read an uninitialized phase quantum. softcut never initializes `Voice::phaseQuant`; the engine now sets it to 0 (unquantized) on creation and `reset`.

- The `mc.softkut~` help patch now opens from the object. It was named `mc.softcut~.maxhelp`, so Max never found it.

- Out-of-range parameter values no longer corrupt the engine or the `buffer~`. softcut validates nothing: `|rate| > 64` overran the resampler's 64-frame output buffer and crashed, `prerq`/`postrq <= 0` and negative slew times diverged to NaN that recording wrote into the buffer, and `prelevel`/`reclevel` above 1 grew the buffer on every pass toward inf. Values are now clamped (`rate` to +/-64, `rq` >= 0.01, `prelevel`/`reclevel` to 0..1, times >= 0, positions to 0..2^30 frames) and non-finite values are rejected, with a warning in the Max console. The engine re-checks on the audio thread, so direct engine callers get the same guarantee.

- A sample-rate change no longer rescales loops. softcut converts loop points, fade time, rec offset and phase offset to frames at the rate current when they are set, so a loop set at 48 kHz played 0.5 s per second at 96 kHz. The engine caches these in seconds and re-applies them on every `setSampleRate`. This also fixes the default 1 s loop, which was sized before the first sample rate was known. `reset` now also clears the phase offset, which softcut's own reset leaves in place.

- Commands are no longer lost when two threads send messages at once. Max delivers messages on the main and scheduler threads concurrently under Overdrive, and the command queue is single-producer. `Engine::push` now serializes producers with a mutex; the audio thread never takes it.

- `play` (or `rec`) without a prior `position` now respects the loop. softcut's `ReadWriteHead::run()` started the head without setting its `active_` flag, which only a position cut set, and the loop-bound check runs only on active heads. The head therefore ran through the whole buffer and wrapped at its end. Patched in the vendored softcut (`// softkut patch` in `ReadWriteHead.cpp`) rather than issuing an implicit cut from the engine, so the fix holds for every caller of softcut.

- Setting `@report` while audio is running now starts or stops reports at once. The report clock was armed only on DSP start, so turning reports on later did nothing until the next restart. The clock also now checks whether this object's patcher has audio on (`sys_getdspobjdspstate`), not whether global audio is on.

- `dsp64` no longer modifies state that the audio thread is reading. Max can recompile a running chain, and `dsp64` reset every voice's sample rate, cleared the 384 KiB-1 MiB feedback store, and (in `mc.softkut~`) freed the silent-input buffer the previous perform routine still used. `setSampleRate` now records the rate and `process()` applies it on the audio thread before draining commands. A null input pointer now means silence, so `mc.softkut~` no longer needs that buffer.

- `poll` no longer races with the audio thread. `Voice::playFlag`/`recFlag` are plain bools that the DSP thread clears when a record-once pass finishes, so reading them from the control thread was undefined behavior. The engine now publishes one 64-bit snapshot per voice from the audio thread at the end of
  every processed chunk -- transport code (`rec<<1|play`) in the high half,
  loop-normalized position in the low half -- and `getVoiceInfo` unpacks both from a single load, so an `info` report cannot pair one block's state with another block's position. `test_poll_during_record_once` exercises the path; ThreadSanitizer reports the old read path and is clean on the new one.

- The report clock no longer outlives the engine. Both externals freed the engine before the `@report` clock, and the clock callback reads the engine from the scheduler thread, so deleting an object with reports armed could dereference freed memory under Overdrive. The clock is now freed first.

- A record-once pass now marks its `buffer~` dirty. The wrappers keyed `buffer_setdirty` on the record flag still being set after `process()`, but a record-once pass clears that flag inside the same call that performs its last writes. When the loop window is shorter than one signal vector the whole pass fits in one block and the buffer was never marked dirty at all. The engine now reports `getWroteBlock(voice)` and the wrappers key on that.

- Signal vectors larger than 8192 samples are split into 8192-sample chunks instead of clamped. The engine's scratch buffers and feedback store hold one chunk, so a clamped pass left every output sample past 8192 unwritten (stale audio). Splitting keeps the feedback bus one chunk delayed rather than one vector delayed; no other host-visible behavior changes.

### Changed

- `help/softkut~.maxhelp` is rewritten as six tabs: basic looping, multichannel buffers, buffer sample rate, record/overdub, position reports, and value limits. Each tab shows the voice with the new `patchers/softkut.view.maxpat` bpatcher (arguments: buffer~, channel, voice): the `buffer~` waveform with a moving play head, the loop window, and the voice state, driven by `phase` reports and `poll`. The channels tab plays `media/softkut-stereo.wav` (bells left, bass right). The help and the view are generated with py2max (`make maxhelp`, `scripts/make_help.py`); the generator fails on overlapping boxes or out-of-range cords. The view's logic is laid out with OGDF's Sugiyama layout (`py2max[graph]`), one seeded run so the file is identical on every build. `scripts/make_media.py` writes the stereo file.

- `reset` also clears the one-block feedback history, so no audio from before the reset re-enters a voice.

- `make build` is incremental; it no longer runs `make clean` first.

- CI runs `make test` on macOS for every push to `main` and every pull request (`.github/workflows/test.yml`).

- The `poll` message now emits the richer `info` list in place of the former softkut-only `position` message (which reported only raw saved positions in seconds). The `@report <ms>` clock and its `phase` events are unchanged -- `phase` remains the faithful softcut/norns idiom for driving a waveform playhead cursor (absolute buffer position, change-throttled).

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

[Unreleased]: https://github.com/shakfu/softkut/compare/v0.1.0...HEAD [0.1.0]: https://github.com/shakfu/softkut
