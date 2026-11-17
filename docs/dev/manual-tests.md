# Manual test checklist (Max side)

`make test` drives `softkut::Engine` directly and links no Max libraries. The plumbing below is therefore unverified by CI and has to be checked by hand in Max after changes to either external. Run the list before tagging a release.

Setup: `make` then `make link`, restart Max, open `help/softkut~.maxhelp` and `help/mc.softkut~.maxhelp`.

## 1. MC channel negotiation (`mc.softkut~`)

- Create `mc.softkut~ mybuf @chans 4`. Confirm the voice outlet reports 4 channels (`mc.dac~` channel display, or an `mc.print`).

- Feed the input inlet from `mc.sig~` at 1, 4, then 8 channels. The output channel count must stay 4 in every case. Voices past the connected input channel count must record silence, not garbage.

- Change `@chans` in the box. The outlet count must follow. Sending `chans 2` must post an error.

## 2. `buffer~` notifications

With DSP on and a voice playing:

- Resize the `buffer~` (`sizeinsamples`), shorter and longer, while a voice records. Playback must continue without a crash, over the whole new length.

- Load a stereo file into a 2-channel `buffer~` and play it with `softkut~ <name> @channel 1` and `softkut~ <name> @channel 2`: they must play the left and right channels. `channel 3` must silence the voice with one console warning.

- Load a 44.1 kHz file with DSP at 48 kHz. Rate 1 must play at the original pitch, and `loopend 1000` must loop exactly one second of the file.

- Send `set <name>` to repoint voices at another `buffer~`. It must take effect on the next perform block.

- Delete the `buffer~` object. The voices must fall silent, not crash. Recreate it and confirm playback resumes.

## 3. Dirty state after recording

- Record normally (`rec 1`, then `rec 0`). Closing the patch must prompt to save the modified `buffer~`.

- Record once with a loop window **shorter than one signal vector**: set the signal vector to 1024, `loopstart 0`, `loopend 2`, then `reconce`. The whole pass completes inside one perform block. The `buffer~` must still be marked dirty. This is the case that `getWroteBlock()` fixes; keying dirty state on the record flag misses it.

## 4. Teardown

- Turn Overdrive on (Options -> Overdrive), then delete a playing object while a `metro` sends it `poll`. Repeat about 20 times, for both objects. Neither has a clock any more; any crash points at the engine or `buffer~` references.

## 5. Oversized signal vectors (optional)

The engine splits host vectors longer than 8192 samples into 8192-sample chunks. Max's own vector size stops at 4096, so this needs a `poly~` with upsampling (4096 x `up 4` = 16384). Confirm the output is continuous and contains no silent tail.

## 6. Ports and units

- `softkut~ <buf>`: the inlets must be record input and rate; the outlets audio, play head and dumpout, left to right (check the assist strings).

- Play and watch outlet 2 through `snapshot~ 30` into a number box: it must rise in ms and wrap at the loop end. With `loopend` past the buffer's end it must wrap at the buffer's length instead. With `syncmode phase` it must run 0-1 across the loop.

- `mc.softkut~ <buf> @chans 4`: outlet 2 must carry 4 sync channels and outlet 1 the 4 audio channels (`mc.snapshot~` or `mc.unpack~` into `snapshot~`). perform assumes the output array lists outlet 1's channels before outlet 2's; swapped outlets mean that assumption is wrong.

- `loopend 500` must loop 500 ms; `rate 100` must warn and clamp to 64.

## 7. Attributes and the rate inlet (`softkut~`)

- Inspector: the attributes must appear in the categories Buffer, Loop, Record, Input filter and Output filter, with their labels. `loop` must show as a toggle and `syncmode` as a menu.

- Save and reopen a patch with changed attributes: the values must come back, and the voice must not start playing.

- `getrate`, `getloopend` and `getbuffer` must answer out outlet 3. After `rate 100`, `getrate` must answer 64.

- A float into inlet 2 must set `@rate` (an `attrui rate` follows). A float into inlet 1 must post an error.

- A `cycle~` into inlet 2 must bend the pitch; deleting the cord must return to `@rate`.

- `[softkut~ 4]` must post "first argument must be a buffer~ name"; `[softkut~ buf 4]` must warn that extra arguments are ignored.

- Stereo lock: two objects on one stereo `buffer~`, sent `play 1` from a `metro`. Invert one output and sum: it must null, with Overdrive on and off. An offset of one vector means the `@chans` design is needed (`docs/dev/m3-design.md`, decision 5).

## 8. `mc.softkut~` channel layout and per-voice control

These checks test SDK assumptions the shell rests on (`docs/dev/m3-design.md`, mc.softkut~ section).

- Rate inlet layout: `mc.softkut~ <buf> @chans 3`, a 2-channel cord into inlet 1 and a 3-channel `mc.pack~` of `sig~ 0.5`, `sig~ 1`, `sig~ 2` into inlet 2. The voices must play at 0.5, 1 and 2. Swapped or wrong rates mean `ins[]` is not inlet 1's channels followed by inlet 2's, or `getnuminputchannels` does not count per inlet.

- Unconnected rate inlet: with no cord into inlet 2, every voice must follow `@rate`. Voice 1 silent or stuck means `count[1]` does not report the connection.

- `rate 1 0.5 -1` must set three voices; `rate 2` all of them; `getrate` must reply with three values. `rate 1 2 3 4` on 3 voices must warn.

- `setvalue 2 rate 0.5` must change voice 2 only, and an `attrui rate` must update. If Max handles `setvalue` itself before the object sees it, voice 2 will not change.

- `play 1 0 1` must play voices 1 and 3; `setvalue 2 play 1` must add voice 2.

- `sync 2 1` with both voices at rate 1: invert voice 2 and sum with voice 1 through `mc.unpack~`; it must null.

- A mono cord into inlet 1 with `rec 1`: voice 1 must record the input and voices 2 and 3 silence.

- Save and reopen with `@rate 1 0.5 -1`: the three values must come back.

- `feedback 1 2 0.7, setvalue 2 rec 1` with voice 1 playing: voice 2 must record voice 1's output. `getfeedback` must reply `feedback 1 2 0.7`. Save and reopen: the route must come back. `reset` must clear it.

