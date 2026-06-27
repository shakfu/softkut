# TODO

## Expose fade-curve shaping (deferred)

**Status:** investigated, deferred to a future session. This is the one remaining
softcut-lib capability not reachable from Max (see "API coverage" in the README).
Implementing it also gives a clean fix for the inverted-record-polarity quirk.

### Why

softcut's crossfade behavior (used for loop wraps and record in/out) is shaped by a
per-voice `FadeCurves` object with configurable curve *shapes* and *window ratios*.
These are currently baked to fixed defaults and not exposed by the `Softcut<>`
template, by norns, or by softkut~. Exposing them lets users:

- Choose the **record fade shape**. The default `Raised` rec curve is
  polarity-inverting (`y = sinf(x) * -1.f` in `FadeCurves::calcRecFade`), which is
  why a recorded signal comes back inverted (documented in `test_record_playback`).
  Selecting `Linear` or `Sine` gives a non-inverting record.
- Tune loop/record crossfade character (`Linear` / `Sine` / `Raised`) and the
  pre-window / rec-delay proportions.

### How fade curves work (data flow)

- `softcut::FadeCurves` (`include/softcut/FadeCurves.h`) holds two precomputed
  lookup buffers (`recFadeBuf`, `preFadeBuf`, size 1001) plus shape enums and ratio
  fields. `Shape { Linear=0, Sine=1, Raised=2 }`.
  - Public setters: `setRecShape`, `setPreShape`, `setRecDelayRatio`,
    `setPreWindowRatio`, `setMinRecDelayFrames`, `setMinPreWindowFrames`. Each setter
    immediately recomputes its buffer (`calcRecFade` / `calcPreFade`).
  - `init()` resets to defaults: rec shape `Raised`, pre shape `Linear`,
    `preWindowRatio = 1/8`, `recDelayRatio = 1/128`, min frames 0.
- Each `Voice` owns a **private** `FadeCurves fadeCurves;` member.
- `Voice::reset()` calls `fadeCurves.init()` then `sch.init(&fadeCurves)`
  (`ReadWriteHead::init` -> `SubHead::init`), so each of the voice's two `SubHead`s
  stores a **pointer** to the voice's `fadeCurves` member.
- `SubHead::poke()` reads `fadeCurves->getPreFadeValue(fade_)` and
  `getRecFadeValue(fade_)` per sample while recording.

Key consequence: because the heads hold a pointer to the member, **calling a setter on
the voice's `fadeCurves` takes effect immediately** (next `poke` reads the recomputed
buffer). No head re-init needed.

Note: `Voice::init(FadeCurves*)` is declared in `Voice.h` but has **no definition** — a
dangling declaration. Do not rely on it; the live object is the private member.

### Proposed patch (vendored softcut-lib is untracked -> free to patch)

Mark all edits with a clear comment (e.g. `// softkut~ patch: expose fade curves`) so
the local divergence from upstream is obvious and greppable.

1. **`softcut-lib/include/softcut/Voice.h`** — add public forwarders (the methods are
   Voice members so they reach the private `fadeCurves`; `FadeCurves.h` is already
   included):
   ```cpp
   void setRecFadeShape(FadeCurves::Shape s) { fadeCurves.setRecShape(s); }
   void setPreFadeShape(FadeCurves::Shape s) { fadeCurves.setPreShape(s); }
   void setRecFadeDelay(float ratio)         { fadeCurves.setRecDelayRatio(ratio); }
   void setPreFadeWindow(float ratio)        { fadeCurves.setPreWindowRatio(ratio); }
   ```
   (min-frames setters optional; default 0 is fine for a first cut.)

2. **`softcut-lib/include/softcut/Softcut.h`** — per-voice pass-throughs:
   ```cpp
   void setRecFadeShape(int v, FadeCurves::Shape s) { scv[v].setRecFadeShape(s); }
   void setPreFadeShape(int v, FadeCurves::Shape s) { scv[v].setPreFadeShape(s); }
   void setRecFadeDelay(int v, float r)             { scv[v].setRecFadeDelay(r); }
   void setPreFadeWindow(int v, float r)            { scv[v].setPreFadeWindow(r); }
   ```

3. **`source/include/softkut_engine.h`** — new `CmdId`s (`RecFadeShape`,
   `PreFadeShape`, `RecFadeDelay`, `PreFadeWindow`), `handle()` cases, and `set*`
   helpers. The shape value travels in `Command::value` as a float cast of the enum
   (0/1/2). **Persistence:** store the chosen shapes/ratios in engine state and
   re-apply them after `cut_.reset()` in the `Reset` handler (mirror how
   `setDefaults()` re-applies engine params), so the `reset` message and any softcut
   reset don't silently revert fade settings.

4. **`source/projects/softkut_tilde/softkut~.cpp`** — Max messages (symbol-based shape
   is most Max-natural):
   - `recfadeshape <voice> <linear|sine|raised>`
   - `prefadeshape <voice> <linear|sine|raised>`
   - `recfadedelay <voice> <ratio>`
   - `prefadewindow <voice> <ratio>`

   Map the symbol to the enum (`linear`->0, `sine`->1, `raised`->2). Reject unknown
   symbols. These are not the `<voice> <float>` table form, so add dedicated handlers
   (like `sync`/`feedback`).

### Caveats / bugs to handle in the patch

- **`calcPreFade()` has a latent bug**: its `Raised` branch tests `else if (recShape
  == Raised)` instead of `preShape == Raised` (with `assert(preShape == Raised)`). So
  `preShape = Raised` while `recShape != Raised` falls through to the final `else` and
  leaves `preFadeBuf` stale. Either fix that line as part of the patch, or restrict
  the exposed pre-shape to `Linear`/`Sine` and document it. Rec shape (the one that
  matters for polarity) is unaffected.
- **Stack-buffer overrun on large ratios**: `calc*Fade()` size the active window as
  `max(minFrames, ratio * fadeBufSize)` and index a `fadeBufSize`-sized stack buffer
  up to that count. A ratio > 1 (or min frames > 1001) overruns it. **Clamp**
  `ratio` to `[0, 1]` and any min-frames to `< 1001` in the Max handlers.
- **Polarity / default choice**: keep softcut's `Raised` rec default for fidelity, but
  document that `recfadeshape <v> linear` (or `sine`) yields non-inverting record.
  Consider whether softkut~ should default to `Linear` for least surprise — a
  behavior change worth calling out, not making silently.

### Testing plan (offline harness)

- Record a DC input with `recfadeshape 0 raised` -> stored interior is negative
  (matches current `test_record_playback`). With `recfadeshape 0 linear` -> stored
  interior is **positive** (non-inverting). This proves the shape change is wired and
  pins the polarity behavior.
- Ratio clamp: setting a ratio > 1 must not crash (clamped) — assert the engine still
  produces finite output.
- Reset persistence: set a non-default shape, send `reset`, confirm the shape is
  re-applied (record polarity stays as chosen).

### Docs to update when implemented

- `README.md` — new "Fade shaping" message section + note the polarity implication.
- `docs/softkut~.maxref` — add the 4 methods.
- `help/softkut~.maxhelp` — optional demo of `recfadeshape`.
- Memory: update `softcut-lib-quirks` (record polarity now user-controllable) and
  `softkut-architecture` (fade-curve coverage).

### Rough scope

Small-to-medium: ~4 forwarders in softcut, ~4 pass-throughs, ~4 engine commands +
persistence, ~4 Max handlers with symbol parsing + clamping, ~3 tests, doc updates.
The investigation (this note) is the hard part; the implementation is mechanical and
follows the existing `feedback`/`inlevel` patterns.
