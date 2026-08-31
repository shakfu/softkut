# Manual test checklist (Max side)

`make test` drives `softkut::Engine` directly and links no Max libraries. The
plumbing below is therefore unverified by CI and has to be checked by hand in
Max after changes to either external. Run the list before tagging a release.

Setup: `make` then `make link`, restart Max, open `help/softkut~.maxhelp` and
`help/mc.softcut~.maxhelp`.

## 1. MC channel negotiation (`mc.softkut~`)

- Create `mc.softkut~ mybuf 4`. Confirm the voice outlet reports 4 channels
  (`mc.dac~` channel display, or an `mc.print`).
- Feed the input inlet from `mc.sig~` at 1, 4, then 8 channels. The output
  channel count must stay 4 in every case. Voices past the connected input
  channel count must record silence, not garbage.
- Change the creation argument and recreate. The outlet count must follow.

## 2. `buffer~` notifications

With DSP on and a voice playing:

- Resize the `buffer~` (`sizeinsamples`). Playback must continue without a
  crash; the usable length is the largest power of two that fits.
- Send `set <name>` and `voicebuf <v> <name>` to repoint voices at another
  `buffer~`. Both must take effect on the next perform block.
- Delete the `buffer~` object. The voices must fall silent, not crash.
  Recreate it and confirm playback resumes.

## 3. Dirty state after recording

- Record normally (`rec <v> 1`, then `rec <v> 0`). Closing the patch must
  prompt to save the modified `buffer~`.
- Record once with a loop window **shorter than one signal vector**: set the
  signal vector to 1024, `loopstart <v> 0`, `loopend <v> 0.002`, then
  `reconce <v> 1` with `rec <v> 1`. The whole pass completes inside one perform
  block. The `buffer~` must still be marked dirty. This is the case that
  `getWroteBlock()` fixes; keying dirty state on the record flag misses it.

## 4. Clock teardown

- Set `@report 20` and start DSP so `phase` reports stream out.
- Turn Overdrive on (Options -> Overdrive), then delete the object while the
  reports are running. Repeat about 20 times. Any crash means the report clock
  outlived the engine.
- Repeat with DSP toggled off immediately before the delete.

## 5. Oversized signal vectors (optional)

The engine splits host vectors longer than 8192 samples into 8192-sample
chunks. Max's own vector size stops at 4096, so this needs a `poly~` with
upsampling (4096 x `up 4` = 16384). Confirm the output is continuous and
contains no silent tail.
