"""Write media/softkut-stereo.wav, the stereo demo file for the help patch.

Left: a bell arpeggio (C6 E6 G6 C7, eighth notes). Right: a bass pluck on each
beat (C3 G2 A2 F2). 2 s at 120 bpm, 44.1 kHz, 16-bit. The channels differ in
pitch and rhythm so a listener can tell which one a voice reads.
"""
import math
import os
import struct
import sys
import wave

SR = 44100
DUR = 2.0
N = int(SR * DUR)


def bell(f, t):
    return (math.sin(2 * math.pi * f * t) + 0.3 * math.sin(2 * math.pi * 2.76 * f * t)) \
        * math.exp(-t * 6.0)


def pluck(f, t):
    # a few odd harmonics with faster decay on the upper ones
    return sum(math.sin(2 * math.pi * k * f * t) / k * math.exp(-t * (3.0 + 2.0 * k))
               for k in (1, 3, 5))


def render(notes, step, voice, gain):
    out = [0.0] * N
    for i, f in enumerate(notes):
        start = int(i * step * SR)
        for n in range(start, N):
            out[n] += gain * voice(f, (n - start) / SR)
    return out


left = render([1046.5, 1318.5, 1568.0, 2093.0] * 2, 0.25, bell, 0.3)
right = render([130.8, 98.0, 110.0, 87.3], 0.5, pluck, 0.45)

path = sys.argv[1] if len(sys.argv) > 1 else os.path.join("media", "softkut-stereo.wav")
os.makedirs(os.path.dirname(path), exist_ok=True)
with wave.open(path, "wb") as w:
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(SR)
    clip = lambda x: max(-32767, min(32767, int(x * 32767)))
    w.writeframes(b"".join(struct.pack("<hh", clip(l), clip(r)) for l, r in zip(left, right)))
peak = max(max(map(abs, left)), max(map(abs, right)))
print("wrote %s: %d frames, 2 ch, peak %.2f" % (path, N, peak))
