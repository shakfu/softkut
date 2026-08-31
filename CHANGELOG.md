# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- karma~-style per-voice metadata report: sending `poll` now emits one
  `info <voice> <pos> <play> <rec> <startMs> <endMs> <windowMs> <state>` list
  per voice on the message outlet (both `softkut~` and `mc.softkut~`). `pos` is
  normalized to the loop window (0..1); `state` is synthesized from the play/rec
  flags as `(rec<<1)|play` (0=stop, 1=play, 2=record, 3=overdub), since softcut
  has no state machine. Backed by the engine's `getVoiceInfo`/`VoiceInfo`, which
  caches the (otherwise write-only) loop bounds. Covered by `test_voice_info`.

### Fixed

- `poll` no longer races with the audio thread. `Voice::playFlag`/`recFlag` are
  plain bools that the DSP thread clears when a record-once pass finishes, so
  reading them from the control thread was undefined behavior. The engine now
  publishes one 64-bit snapshot per voice from the audio thread at the end of
  every processed chunk -- transport code (`rec<<1|play`) in the high half,
  loop-normalized position in the low half -- and `getVoiceInfo` unpacks both
  from a single load, so an `info` report cannot pair one block's state with
  another block's position. `test_poll_during_record_once` exercises the path;
  ThreadSanitizer reports the old read path and is clean on the new one.

- The report clock no longer outlives the engine. Both externals freed the
  engine before the `@report` clock, and the clock callback reads the engine
  from the scheduler thread, so deleting an object with reports armed could
  dereference freed memory under Overdrive. The clock is now freed first.

- A record-once pass now marks its `buffer~` dirty. The wrappers keyed
  `buffer_setdirty` on the record flag still being set after `process()`, but a
  record-once pass clears that flag inside the same call that performs its last
  writes. When the loop window is shorter than one signal vector the whole pass
  fits in one block and the buffer was never marked dirty at all. The engine now
  reports `getWroteBlock(voice)` and the wrappers key on that.

- Signal vectors larger than 8192 samples are split into 8192-sample chunks
  instead of clamped. The engine's scratch buffers and feedback store hold one
  chunk, so a clamped pass left every output sample past 8192 unwritten (stale
  audio). Splitting keeps the feedback bus one chunk delayed rather than one
  vector delayed; no other host-visible behavior changes.

### Changed

- The `poll` message now emits the richer `info` list in place of the former
  softkut-only `position` message (which reported only raw saved positions in
  seconds). The `@report <ms>` clock and its `phase` events are unchanged --
  `phase` remains the faithful softcut/norns idiom for driving a waveform
  playhead cursor (absolute buffer position, change-throttled).

## [0.1.0] - 2026-06-27

Initial implementation: a Max/MSP wrapper around monome's softcut-lib, plus a
multichannel variant, built on a shared host-agnostic engine.

### Added

#### `softkut~` external

- A variable number of softcut voices (the optional `<channels>` creation
  argument, default 1, max 6) reading/writing a shared (or per-voice) mono
  `buffer~`, zero-copy: voices point directly at the locked `buffer~` samples
  each perform block. One signal inlet (record input) and one signal outlet per
  voice, plus the stereo-mix pair and a report outlet.
- Full per-voice control surface: `rate`, `loopstart`, `loopend`, `loop`,
  `fade`, `reclevel`, `prelevel`, `rec`, `play`, `reconce`, `position`,
  `recoffset`, `stop`.
- Input (pre) and output (post) multimode SVF filters: `prefc`/`prerq`/`prelp`/
  `prehp`/`prebp`/`prebr`/`predry`/`prefcmod` and the matching `post*`.
- Per-voice output `level` and equal-power `pan` (smoothed) feeding a stereo mix
  outlet pair, with `levelslew`/`panslew`; plus `recpreslew`/`rateslew`.
- Phase/sync: `quant`, `phaseoffset`, `sync`, and a `poll` / `@report <ms>`
  reporting outlet (race-free position + quantized-phase events).
- Per-voice `buffer~` assignment via `voicebuf` (stereo = two mono buffers);
  perform dedup-locks distinct buffers.
- Voice-to-voice `feedback` matrix (one-block-delayed) for overdub/looping
  networks.
- Inlet-to-voice `inlevel` input routing matrix (default identity).
- Per-voice `enable` gate (skips processing entirely).
- `reset` to restore per-voice and routing defaults.

#### `mc.softkut~` external

- Multichannel variant sharing the same engine and control surface, presented
  through Max's MC system: one `Z_MC_INLETS` record-input inlet, one
  multichannel voice-output outlet, and a message (report) outlet.
- Variable voice count via the second creation argument (default 6, cap 16),
  which sets the output channel count.

#### Engine and infrastructure

- `softkut_engine.h`: host-agnostic `softcut::Engine` owning the softcut voices,
  a lock-free single-producer/single-consumer command queue, double<->float
  block conversion, power-of-two buffer framing, level/pan ramps, the feedback
  and input matrices, and phase-poll bookkeeping. Runtime-settable active voice
  count over a compile-time maximum.
- `softkut_control.h`: shared command table + dispatch used by both shells so
  their control surfaces cannot drift.
- Offline test harness (`make test`, via ctest) covering the queue, buffer
  framing, command routing, record/playback, level, pan, per-voice buffers,
  stop, feedback, enable, input matrix, and runtime voice count.

#### Documentation

- `README.md` full message-API guide and examples.
- `docs/softkut~.maxref` structured object reference.
- `help/softkut~.maxhelp` help patch.
- `TODO.md` investigation/spec for the deferred fade-curve shaping work.

### Changed

- Build system: softcut-lib is built once as a static (PIC) library shared by
  both externals and the test harness; top-level CMake wires the shared include
  paths; `Makefile` gains a `test` target and fixes the `clean` typo.
- `package-info.json` updated from the template placeholder to softkut metadata.

### Removed

- Vendored `karma~` sources (used only as a scaffolding reference; no karma code
  was ever compiled or linked).
- Template `example` object, help, and maxref stubs.

### Notes

- softcut's record path is intentionally colored (a ~1.2x soft-clip and a
  polarity-inverting default `Raised` rec-fade curve), so recorded audio returns
  gained and inverted; this is upstream behavior, documented in the tests.
- `buffer~` must be mono; only its largest power-of-two prefix is used.
- File I/O (load/save/clear) is delegated to Max's `buffer~`, not reimplemented.
- Fade-curve shaping is the one softcut-lib capability not yet exposed (locked
  behind `Voice`'s private state); investigated and deferred — see `TODO.md`.

[Unreleased]: https://github.com/shakfu/softkut/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/shakfu/softkut
