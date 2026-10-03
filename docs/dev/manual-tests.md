# Manual test checklist (Max side)

`make test` drives `softkut::Engine` directly and links no Max libraries. The
plumbing below is therefore unverified by CI and has to be checked by hand in
Max after changes to either external. Run the list before tagging a release.

Setup: `make` then `make link`, restart Max, open `help/softkut~.maxhelp` and
`help/mc.softkut~.maxhelp`.

## 1. MC channel negotiation (`mc.softkut~`)

- Create `mc.softkut~ mybuf 4`. Confirm the voice outlet reports 4 channels
  (`mc.dac~` channel display, or an `mc.print`).
- Feed the input inlet from `mc.sig~` at 1, 4, then 8 channels. The output
  channel count must stay 4 in every case. Voices past the connected input
  channel count must record silence, not garbage.
- Change the creation argument and recreate. The outlet count must follow.

## 2. `buffer~` notifications

With DSP on and a voice playing:

- Resize the `buffer~` (`sizeinsamples`), shorter and longer, while a voice
  records. Playback must continue without a crash, over the whole new length.
- Load a stereo file into a 2-channel `buffer~` and play it with
  `softkut~ <name> 2`: voice 0 must play the left channel, voice 1 the right.
  `voicebuf 0 <name> 2` must switch voice 0 to the right channel; channel 3
  must silence the voice with one console warning.
- Load a 44.1 kHz file with DSP at 48 kHz. Rate 1 must play at the original
  pitch, and `loopend <v> 1` must loop exactly one second of the file.
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
- With DSP on and `@report 0`, send `report 20`. `phase` reports must start
  at once, without restarting DSP. Send `report 0`; they must stop.
- Put the object in a subpatcher and disable its audio with `pcontrol`
  (`enable 0`) while global audio stays on. Reports must stop.

## 5. Oversized signal vectors (optional)

The engine splits host vectors longer than 8192 samples into 8192-sample
chunks. Max's own vector size stops at 4096, so this needs a `poly~` with
upsampling (4096 x `up 4` = 16384). Confirm the output is continuous and
contains no silent tail.
