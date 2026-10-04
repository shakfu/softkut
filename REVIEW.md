# softkut~ review

Date: 2026-10-03. Commit: `5661e0b`. Scope: `softkut~`, `mc.softkut~`, the shared engine and control headers, help patches, docs, build and CI.

Method:

- Read all first-party sources, docs, help patches (parsed as JSON), and the relevant softcut-lib code.

- `make test`: builds cleanly, 1/1 ctest entry passes (19 test functions).

- Wrote two throwaway probes against `Engine` (not committed) to check suspected defects. Findings marked **Confirmed** were reproduced by a probe. **Plausible** means inferred from code reading and not reproduced. Max-side behavior was not run in Max.

## Summary

- The engine/shell split is sound. One header owns DSP and threading. One header owns the message table, so both externals share 44 messages without drift.

- Four defects need fixing first. Invalid filter values write NaN into the user's `buffer~`. Loop points do not follow sample-rate changes. `play` without `position` ignores the loop bounds. The SPSC queue can get two producers when Overdrive is on.

- The user-facing layer has drifted from the code. Help patches, README, maxref and `package-info.json` still describe 6 fixed voices, a stereo-mix outlet, and a `position` report. The `mc.softkut~` help file is misnamed, so Max never opens it.

- The largest usability cost is the power-of-two buffer rule. The help patch's own `buffer~` loses 33% of its frames. Loop points past the usable length wrap silently.

- Direction (decided 2026-10-03): a port of the softcut-lib engine into Max, built on Max idioms. It is not a norns port. The current code ports much of the norns client layer and few Max idioms. See [section 0](#0-direction-softcut-lib-port-with-max-native-infrastructure).

## 0. Direction: softcut-lib port with Max-native infrastructure

### What is being ported today

softcut-lib's `Softcut<N>` is an array of independent `Voice`s. The only cross-voice call is `syncVoice` (`Softcut.h:180-182`). Everything else in softkut~ comes from one of two layers:

| Feature in softkut~ | Origin | Max-native equivalent |
|-|-|-|
| `inlevel` matrix | norns client (`SoftcutClient::mixInput`) | `matrix~`, `mc.matrix~` |
| `feedback` matrix (one chunk delayed) | norns client | `send~`/`receive~`, `tapin~`/`tapout~` (one vector delayed, the same latency) |
| `level`, `levelslew` | norns client | `*~` with `line~`, `gain~`, `mc.gain~` |
| `pan`, `panslew` (already dead) | norns client | `mc.stereo~`, `pan~`-style patching |
| `@report` clock, `phase` messages | norns poll system | signal sync outlet (`groove~` idiom), `snapshot~` |
| `<cmd> <voice> <value>` messages | norns Lua/OSC API | attributes; `target` (`poly~`) or per-channel `setvalue` (MC) |
| 0-based voices, seconds | softcut OSC API | 1-based channels/voices, milliseconds |

Under this direction the norns rows are candidates for removal, not maintenance. `level` is the only one with a convenience argument (built-in slew). The two matrices add no capability over Max patching: their feedback latency equals Max's minimum signal feedback latency.

### Gaps against Max idioms

**M1. `buffer~` contract (highest priority).** **Status: done** (any length, channel select, buffer sample rate; see `CHANGELOG.md`). `groove~`, `play~` and `poke~` accept any length, any channel count with a channel selector, and the buffer's own sample rate. softkut~ requires power-of-two length (D2), mono only, and ignores `buffer_getsamplerate`. Meeting the Max contract needs patches to the vendored `SubHead`: an arbitrary-length wrap in `wrapBufIndex`, and a channel stride in `peek`/`poke`. These two patches turn softcut-lib into a Max-shaped engine. Everything else in this list is shell work.

**M2. Signal sync outlet.** **Status: done** (sync outlets on both externals; see `CHANGELOG.md`). Max loopers expose the playhead as a signal (`groove~` sync outlet, 0..1 over the loop). It is sample-accurate, drives `waveform~` through `snapshot~`, and costs no scheduler time. It replaces `@report`, `phase` messages and C5. softcut computes phase per sample but publishes it once per block (`Voice::updateQuantPhase`), so a per-sample outlet needs a small `Voice` patch that writes phase into a caller buffer.

**M3. Parameters as attributes.** Attributes save with the patcher, show in the inspector, work with `pattr`, and answer `getattr`. They also make D7 and most of C7 go away. Two shapes:

1. One voice per object. `softkut~` is one voice; `mc.softkut~ @chans N` is N voices. Messages lose the voice argument (`rate 1.5`, `@rate 1.5`). MC per-channel control uses `setvalue <chan> rate 1.5`. Unverified: whether Max provides `setvalue` for native MC objects or the object must implement it.

2. Keep N voices per object and add `target <n>` (`poly~` idiom: 1-based, 0 = all). Plain `rate 1.5` goes to the target voice.

Recommend option 1. Voices are independent in softcut-lib, so splitting costs only `syncVoice`. Keep `sync` inside `mc.softkut~`, or drive `position` from another object's sync outlet. The discrete 6-inlet `softkut~` disappears; multi-voice use goes through MC.

**M4. Units and indexing.** **Status: done** (ms and voices from 1 in messages; engine unchanged). Milliseconds for times (`groove~`, `play~`, `karma~`). 1-based voices and channels (`buffer~` channels, `poly~`, MC). The engine stays in seconds and 0-based; the shell converts. This resolves D5 and D8.

**M5. Signal-rate control.** `groove~` takes rate as a signal. softcut smooths rate per sample but its setters are per block. Options: sub-block chunking (16-32 samples), or a `Voice` patch that accepts a rate buffer. `processBlockMono` builds a `std::function` on every call (`Voice.cpp:60`), so very small chunks are costly. Start with rate only.

**M6. Transparent record by default.** `poke~` and `record~` write input verbatim. softcut writes it through a 16 kHz low-pass pre-filter, a 1.2x soft clip (compile-time `#if 1`, `SubHead.cpp:119`), and a polarity-inverting rec fade. Recommend transparent defaults (`predry 1`, `prelp 0`, linear rec fade shape) with the softcut behavior available by message. This makes the fade-curve work in `TODO.md` required, not deferred. The soft clip needs a runtime switch patched into `SubHead`.

**M7. Package conventions.** One `.maxref` per object, tabbed help patches, a vignette. `init/object-mappings.txt` is empty.

### Effect on existing findings

- C1, C2, C4, C6: unchanged.
- C3: fix in the engine. A Max user expects `groove~` behavior: playback starts inside the loop.
- C5: obsolete if M2 replaces the report clock.
- C7: mostly obsolete with M3 (attributes are last-value state).
- D1: extend to removing `feedback`, `inlevel`, and probably `level`. D4 then disappears.
- D2: becomes M1, required.
- D3: smaller after M3 option 1. Each shell becomes a thin wrapper over a one-voice engine.
- D5, D8: resolved by M4. D6: resolved by M6.

### Risks of this direction

- Vendored fork. M1, M2, M5 and M6 each patch softcut-lib. Upstream fixes then need manual merging. `Svf.cpp` is already patched. Tag every patch with a greppable comment and list them in `docs/dev/`. I have not checked upstream's recent activity.
- Breaking change. Message shapes, units and indexing all change. At version 0.1.0 with no known users, this is the cheapest time to do it.
- What remains of the port is the `Voice`: heads, crossfades, resampler, SVFs, rec/pre slews. That is the part of softcut worth porting; the rest becomes Max plumbing.

## 1. Correctness

### C1. Invalid parameters write NaN into the buffer~ (Confirmed, high)

**Status: fixed** (see `CHANGELOG.md`, Unreleased).

`softkut_control.h:76-89` checks the voice index only. Values pass to softcut unchecked. `Svf::setRq` (`softcut-lib/src/Svf.cpp:46`) does not clamp.

Probe: `postrq 0 -1` with `postlp 0 1`, `postdry 0 0` gives NaN output within 2000 blocks. With `rec 0 1`, `prelevel 0 1` and `feedback 0 0 1`, 12800 non-finite samples were written to the buffer. The `buffer~` is then marked dirty, so the corruption can be saved with the patch.

Fix: a per-command range table in `softkut_control.h` (clamp and warn), plus `std::isfinite` on every float. Minimum set: `prerq`/`postrq` > 0, `fade` >= 0, `rateslew`/`recpreslew`/`levelslew` >= 0, `rate` finite. This is a trust-boundary check, so it should not be skipped.

### C2. Loop points do not follow sample-rate changes (Confirmed, high)

**Status: fixed** (see `CHANGELOG.md`, Unreleased).

`ReadWriteHead::setLoopStartSeconds` stores `sec * sr` as frames (`softcut-lib/src/ReadWriteHead.cpp:100-110`). `dsp64` calls `Engine::setSampleRate` on every DSP start, but nothing re-applies loop points.

Probe: `loopend 0 1` applied at 48 kHz, then sample rate changed:

| new SR | loop length played |
|-|-|
| 48000 | 1.000 s |
| 96000 | 0.500 s |
| 44100 | 1.088 s |

`info` keeps reporting `endMs = 1000`, because the engine caches seconds (`softkut_engine.h:518-519`). Fade time, rec offset and phase offset are also stored in frames and have the same problem (`ReadWriteHead::calcFadeInc`, `Voice::setRecOffset`, `Voice::setPhaseOffset`).

Plausible corollary: `setDefaults()` runs in the constructor before any `setSampleRate`, so the default 1 s loop is sized at softcut's construction rate.

Fix: cache all seconds-valued parameters in the engine (loop bounds already are) and re-apply them after `cut_.setSampleRate`.

### C3. `play` without a prior `position` ignores the loop (Confirmed, high usability)

**Status: fixed** (see `CHANGELOG.md`, Unreleased).

Probe: buffer of 2^18 frames, `loopstart 0 0`, `loopend 0 1`, `play 0 1`, no `position`. Output is non-zero, but the playhead runs to 4.0 s (the probe length) and wraps by bitmask at the buffer end, with no crossfade. Adding `position 0 0` gives a correct 1.0 s loop.

This is upstream softcut behavior; norns scripts always call `softcut.position` first. But the help patch's main record/play message omits `position`, so the first thing a user tries does not loop as configured. Every engine test calls `cutToPos` first, so no test covers this path.

Fix options:

1. Engine: on `PlayFlag` 0 -> 1 with the active head stopped, cut to `loopStart_`. Needs a test.

2. Documentation and help patch only. Keeps norns fidelity.

### C4. Two producers can push into the SPSC queue (Plausible, high)

**Status: fixed** (see `CHANGELOG.md`, Unreleased).

`SpscQueue` is single-producer (`softkut_engine.h:78-118`). The header assumes Max messages arrive serialized (`softkut_engine.h:18-19`). With Overdrive on, Max delivers messages on both the main thread (UI clicks) and the scheduler thread (`metro`, `line`). Two concurrent `push` calls can write the same slot and lose or corrupt a command.

Fix: wrap `Engine::push` calls in the shells with `critical_enter(0)`/`critical_exit(0)`. Only producers contend; the audio thread stays lock-free. Rejected alternative: `defer_low` every control message, which moves scheduler-timed messages onto the main thread and destroys their timing.

### C5. `@report` changes at runtime do not start the clock (Confirmed by reading, medium)

**Status: fixed** (see `CHANGELOG.md`, Unreleased).

The clock is armed only in `dsp64` (`softkut~.cpp:247-248`) and re-armed only inside the callback, which returns early when `report <= 0` (`softkut~.cpp:211`). Setting `report 100` while DSP is running and `report` was 0 produces no reports until DSP restarts. Same code in `mc.softkut~.cpp:216, 260`.

Fix: an attribute setter that calls `clock_delay` or `clock_unset`. Also prefer `sys_getdspobjdspstate(x)` over `sys_getdspstate()`; the SDK recommends it for objects in a patcher whose audio may be off (`z_dsp.h:201-208`).

### C6. `dsp64` mutates state the audio thread reads (Plausible, medium)

**Status: fixed** (see `CHANGELOG.md`, Unreleased).

`dsp64` runs on the main thread and calls `setSampleRate` (rewrites every voice and ramp) and `zeroOutStore` (384 KiB for `Engine<6>`, 1 MiB for `Engine<16>`). `mc.softkut~` also frees and reallocates `zeroIn` (`mc.softkut~.cpp:240-244`). If Max recompiles the chain while the old chain is still running (patch edits with audio on), these race `perform64`. I have not verified Max's chain-swap ordering; treat this as a hypothesis to test.

Fix: send the sample rate through the queue and apply it at the top of `process()`. Size `zeroIn` once (or to the largest vector seen) and never free it while DSP is on.

### C7. Commands queue up while DSP is off (Confirmed by reading, medium)

The queue drains only in `process()`. With DSP off, every message waits. After 1023 commands the queue drops new ones with a warning. A `line` at 1 ms grain fills it in about 1 s. `poll` reports stale state until DSP starts.

Alternative design: last-value-wins parameter slots (atomic float per `(param, voice)` plus a dirty bitmask) for continuous parameters. The queue is then needed only for edge-triggered commands (`position`, `sync`, `stop`, `reset`). This also removes C7's overflow for slider-driven parameters.

### C8. Argument parsing accepts wrong types silently (Confirmed by reading, low)

- `parseVoiceVal` (`softkut_control.h:82`) calls `atom_getlong` on any atom. `rate foo 1` addresses voice 0.

- The voice-count argument must be `A_LONG` (`softkut~.cpp:286`). `[softkut~ buf 4.]` and `[softkut~ buf @report 100 4]` silently create 1 voice.

- The power-of-two trim notice covers voice 0's buffer only (`softkut~.cpp:234-243`).

## 2. Design

### D1. The pan path is dead (high, simplification)

Both shells pass `mixL = mixR = NULL`. The engine still carries `outPan_`, `mixLf_`, `mixRf_`, the equal-power mix in `processChunk`, the `Pan`/`PanSlewTime` commands and `test_pan`. `pan` and `panslew` are accepted and do nothing; the help patches have buttons for them.

Recommend deleting the mix path and both commands, so `pan` raises "unknown message". Downstream `mc.stereo~`/`mc.pan~` already cover stereo. Alternative: restore a stereo outlet on `softkut~` only.

### D2. Power-of-two buffers are the main usability cost (high)

**Status: fixed** by M1.

The help patch uses `buffer~ skbuf 4096` (4096 ms). At 48 kHz that is 196608 frames; 131072 are used (66.7%). A `loopend` beyond 2.73 s wraps by bitmask to the start of the buffer, with no warning.

Options, in order of preference:

1. Patch `SubHead::wrapBufIndex` (vendored, already locally patched in `Svf.cpp`) to wrap arbitrary lengths. Head increments are bounded by rate, so a compare-and-subtract wrap works outside cuts. Cost: one branch per index; needs measurement against the bitmask.

2. Keep the rule, and warn when `loopstart`/`loopend`/`position` exceed the usable length of that voice's buffer.

3. Add a message that resizes the `buffer~` to the next power of two.

### D3. The two shells duplicate about 200 lines (medium)

`poll`, `set`, `voicebuf`, the buffer resolve/lock/dirty/unlock loop, the clock, `notify`, and `free` are copied between `softkut~.cpp` and `mc.softkut~.cpp`. Drift has already happened:

- `softkut_buf_dblclick` loops to `NumVoices`; the mc version loops to `nvoices`.

- `mc.softkut~.cpp:5` still describes a 2-channel stereo-mix outlet.

`softkut_control.h` already templates the message handlers. Move the lock loop and `poll` there too.

### D4. Routing matrices cost O(N^2) per sample (Plausible, medium)

`processChunk` updates `numVoices` input ramps and `numVoices` feedback ramps per destination voice per sample, even when every gain is 0. For `mc.softkut~ buf 16` that is 512 `LogRamp::update` calls and multiply-adds per sample. I have not profiled it.

Fix: skip a ramp whose target and current value are both 0. Size `kMaxBlock` storage from `maxvectorsize` to cut the 1 MiB per-instance feedback store.

### D5. Units are inconsistent (medium)

**Status: fixed** by M4.

- Inputs: seconds (`loopstart`, `fade`, `position`).

- `phase` report: seconds.

- `info` report: milliseconds, plus a 0..1 normalized position.

Max loopers (`groove~`, `karma~`) use ms. Pick one unit for all three.

### D6. Defaults color the recording (medium)

A user expecting a transparent looper gets:

- Input low-passed at 16 kHz (`Voice::reset`: pre-filter `lpMix 1`, `dry 0`, `fc 16000`).

- A 1.2x soft clip and inverted polarity on record (noted in `CHANGELOG.md` and `TODO.md`).

These are norns defaults. The README states none of them in the message tables. Either document a defaults table, or set `predry 1`/`prelp 0` in `setDefaults()` and document the departure.

### D7. No state persistence or introspection (low)

All per-voice parameters are messages, so nothing is saved with the patcher, shown in the inspector, or readable back. Options: a `dump` message that emits current values, or per-voice list attributes. `dump` is the smaller change.

### D8. Indexing conventions (low)

**Status: fixed** by M4.

Voices and `inlevel` inlet indices are 0-based. Max's MC objects number channels from 1 (e.g. `mc.target`), as do softcut's Lua bindings. 0-based is defensible (it matches softcut's OSC API) but should be stated in the first line of the help patch.

### D9. Dead code

- `Engine::updatePhases()` (`softkut_engine.h:469-471`): empty.

- `Engine::getEnabled`, `getPlayFlag`: unused. `getEnabled` reads a non-atomic `bool`.

- `t_mcsoftkut::inputChans`: written in two places, never read.

- `init/object-mappings.txt`: empty tracked file.

## 3. User interface and documentation

### U1. `mc.softkut~` help never opens (Confirmed, high)

**Status: fixed** (file renamed to `help/mc.softkut~.maxhelp`).

The file is `help/mc.softcut~.maxhelp` (`softcut`, not `softkut`). Max looks up `mc.softkut~.maxhelp`. `docs/dev/manual-tests.md:8` repeats the typo. There is no `mc.softkut~.maxref`.

### U2. Help patches contradict the objects (Confirmed, high)

**Status: partly fixed.** `softkut~.maxhelp` rewritten for the current API (2026-10-03); `mc.softkut~.maxhelp` not yet. Both need redoing after M3.

- `softkut~.maxhelp`: comment says "This instance has 6 voices (the second argument)". The box is `softkut~ skbuf @report 100`: 1 voice, 2 outlets.

- `mc.softcut~.maxhelp`: comment says "outlets 6/7 are the panned stereo mix; outlet 8 reports". The object has 2 outlets.

- Both titles say "6 voices". Both have `pan` buttons that do nothing (D1).

- The record/play message omits `position` (C3).

### U3. Help coverage is thin (medium)

**Status: partly fixed.** `softkut~.maxhelp` rewritten for the current API (2026-10-03); `mc.softkut~.maxhelp` not yet. Both need redoing after M3.

The help patches demonstrate 12 of 44 messages. Missing: filters, `feedback`, `inlevel`, `sync`, `voicebuf`, `reconce`, `enable`, `stop`, and parsing of `info`. The `CHANGELOG` names a `waveform~` playhead driven by `phase` as the intended use of `@report`; the help patch prints to the console instead. Suggest tabbed subpatchers: basics, overdub, filters, routing, multi-voice stereo, playhead display.

### U4. README, maxref and package metadata are stale (Confirmed, medium)

| Location | Says | Code does |
|-|-|-|
| `README.md:5`, `:15`, `:231` | 6 voices | default 1, max 6 |
| `README.md:65` | voice is `0..5` | `0..N-1` |
| `README.md:140` | `poll` emits `position <p0> ... <p5>` | emits one `info` list per voice |
| `docs/softkut~.maxref:93`, `:473` | `position <p0> ... <p5>` | `info ...` |
| `docs/softkut~.maxref:32-97` | 6 inlets, 7 outlets | N inlets, N+1 outlets |
| `package-info.json` description | "6-voice buffer looper" | 1-6 / 1-16 voices |

### U5. What works well

- Every handler posts a usage string on bad arity or out-of-range voice.

- Outlet order follows Max convention: report outlet rightmost.

- Assist strings track the runtime voice count.

- `docs/dev/manual-tests.md` lists the Max-only behavior that CI cannot reach.

## 4. Build, tests, CI

- `make build` depends on `clean` (`Makefile:12`), so every `make test` rebuilds softcut and both externals from scratch. `$(call section,...)` references an undefined macro and prints nothing.

- Both workflows trigger on `workflow_dispatch` only. Tests never run on push or PR. `package.yml` does not run `ctest`.

- The Windows package step runs `cp -a` under `shell: cmd`. Whether `cp` resolves depends on the runner PATH (unverified). The step also omits `help`, `docs` and `package-info.json`.

- Test gaps matching the findings above: play without `position` (C3), sample-rate change (C2), invalid parameter values (C1), concurrent producers (C4).

- `test_phase_advance` asserts `p < 8192` on a value in seconds (`test_engine.cpp:242`). It cannot fail.

## 5. Priority

Revised for the Max-native direction. Docs and help (U1-U4) wait until the API settles; fix only the help file name now.

| # | Item | Effort |
|-|-|-|
| 1 | C1 parameter validation; C2 re-apply params on SR change; C4 producer lock | small |
| 2 | Decide M3 (voice-per-object vs `target`), M4 units/indexing, norns-layer removal | decision |
| 3 | M1 `buffer~` contract: arbitrary length, channel select, buffer SR | medium, needs profiling |
| 4 | M3 attributes and new message shape; D1 removals; C3 initial cut | medium |
| 5 | M2 signal sync outlet (replaces `@report`, C5) | small-medium |
| 6 | M6 transparent record defaults (`TODO.md` fade work, soft-clip switch) | medium |
| 7 | CI on push/PR | small |
| 8 | M5 signal-rate `rate` | medium |
| 9 | M7, U1-U4: maxrefs, tabbed help, README | medium |
| 10 | C6 `dsp64` race | small |

## Open questions

1. Answered: a softcut-lib port with Max idioms, not a norns port (section 0).

2. M3: one voice per object, or N voices with `target`? Option 1 is the more idiomatic choice, but it drops the discrete multi-inlet `softkut~`.

3. Keep `level` as a convenience, or remove it with the other norns-layer features?

4. M6: transparent by default, or softcut's colored record by default? The direction argues for transparent.

5. Is a growing patch set on the vendored softcut-lib acceptable (M1, M2, M5, M6)?

6. C4 and C6 rest on assumptions about Max threading. Can they be checked under ThreadSanitizer with Overdrive on, as was done for the `poll` race?
