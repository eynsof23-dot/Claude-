# soundscape

A generative, dark, cinematic soundscape engine in plain C++17. It has no
dependencies and no samples: every sound is synthesised from oscillators,
noise and filters.

Each run composes a new piece. It has a key, a tempo and a full arc: a quiet
opening, a long build, risers into booms and braams, a drum-driven climax, then
the fall. Write it to a WAV file, or stream it live, endlessly.

```
  seed 42  ·  E Harmonic minor  ·  87 BPM  ·  03:00

  [00:00]  section 1  [#.........] 0.06
  [00:00]  chord Emadd9 (i)
  [00:19]  section 2  [##........] 0.19  choir
  [00:35]  riser (3s)
  [00:38]  BOOM
  [00:38]  section 3  [#####.....] 0.46  ostinato heartbeat choir
  ...
  [01:25]  BOOM
  [01:25]  BRAAM
  [01:25]  section 5  [##########] 0.91  drums
```

## Build

```sh
cmake -B build && cmake --build build          # or simply:
g++ -std=c++17 -O2 -o soundscape src/main.cpp
```

This works with GCC, Clang and MSVC on Linux, macOS and Windows. A 3-minute
piece renders in about 7 seconds.

## Use

```sh
./soundscape                       # random 3-minute piece -> soundscape.wav
./soundscape -s 42 -d 300          # seed 42, 5 minutes (same seed = same piece)
./soundscape -o night.wav          # choose the file name
```

**Listen live, forever** (the music changes key every few minutes):

```sh
./soundscape --raw | ffplay -f f32le -ar 48000 -ch_layout stereo -nodisp -   # ffmpeg (any OS)
./soundscape --raw | aplay -f FLOAT_LE -c 2 -r 48000                       # Linux
./soundscape --raw | play -t raw -e floating-point -b 32 -c 2 -r 48000 -   # macOS (sox)
```

(Older ffmpeg versions use `-ac 2` instead of `-ch_layout stereo`.)

**Hear one instrument at a time.** This is the best way to learn what each
layer contributes:

```sh
./soundscape -s 42 --solo pad
./soundscape -s 42 --solo braam,booms,risers
./soundscape -s 42 --mute wind,bells
```

Layers: `drone pad choir braam booms drums heartbeat risers bells wind ostinato`

**Export stems for your DAW.** The same seed always gives the same piece, so
the solos line up sample for sample:

```sh
mkdir -p stems
for l in drone pad choir braam booms drums heartbeat risers bells wind ostinato; do
  ./soundscape -q -s 42 --solo $l -o stems/$l.wav
done
```

Drag them in, then add your own melody, vocals or sound design on top. Each
stem carries its own reverb tail. The master compressor reacts a little
differently to each stem than to the full mix, so the summed stems won't
match the mix exactly.

## How it works

| File | What's in it |
|---|---|
| `src/dsp.hpp` | Oscillators, filters, envelopes, noise, delay lines |
| `src/effects.hpp` | The hall reverb, ping-pong echo, bus compressor |
| `src/harmony.hpp` | Chord progressions (a Markov chain over dark chords) |
| `src/layers.hpp` | The 11 instruments, each a commented sound-design recipe |
| `src/soundscape.hpp` | The conductor: form, orchestration, transitions, timing |
| `src/main.cpp` | Command line, WAV writer, live streaming |

One number drives everything: **intensity** (0 to 1). The conductor plans
sections with an intensity arc, and every instrument reacts to it. Filters
open, levels rise, chords change faster, the ostinato and drums come in.

## Sound-design recipes you can steal

Every recipe below is in `src/layers.hpp` with exact numbers, and can be
rebuilt in Vital, Serum or any stock synth.

- **Drone.** A sine one octave below the root, slightly saturated so it's
  audible on small speakers, and kept *out* of the reverb. On top, two saws
  detuned ±6 cents, one per ear, through a low-pass that slowly drifts.
- **Dark pad.** 3 saws per note (left, centre, right, ±8 cents), with a
  low-pass at 300 Hz when quiet and up to 2.5 kHz at the climax. Attack and
  release of 4–9 s, so chords crossfade. Dark means *taking highs away*.
- **Choir.** A saw (the vocal cords) through 3 band-pass filters at vowel
  formants (a: 800/1150/2800 Hz). Morph the vowel slowly, and fade the
  vibrato in after the note starts.
- **Braam.** Low root + octave + fifth, many detuned saws, **distortion
  before the filter**, then a resonant low-pass that snaps open and closes
  over ~3 s.
- **Boom.** A sine dropping 150 → 40 Hz in ~60 ms (an 808 is the same
  idea), **tuned to the key**, plus a short noise burst for the attack.
- **Riser.** Pink noise through a band-pass sweeping 180 Hz → 6 kHz with
  rising resonance and an exponential volume curve. It **cuts dead** on the
  impact.
- **Bells.** FM with non-integer ratios (3.5, 1.41, 2.76) for inharmonic
  metal. The modulation decays faster than the volume, so the tone starts
  bright and fades to pure. Mostly echo and reverb.
- **Ostinato.** 16th-note "filter pluck" bass: the cutoff jumps on every
  note and snaps shut. Accents on a 3-3-2 grid give that restless film pulse.
- **The room.** An 8-line feedback-delay reverb. High-pass its input
  (reverb on sub = mud), damp the highs inside the loop (that's what makes it
  "dark"), and modulate the delay times slightly.
- **Harmony.** A drone that never leaves the root, with chords moving above
  it: i → bVI, the Phrygian bII, chromatic mediants (Cm → Abm, Cm → E).
  The friction against the pedal tone is the dread.

## Tweaking

Good places to start:

- `Reverb::init` in `soundscape.hpp`: `7.5f` is the reverb length in
  seconds, `1.6f` the room size, `3800.0f` the darkness.
- `planMovement`: tempo range, section lengths, where the climax falls.
- Level constants (`lvl`, `amp`) at the top of each layer's `render`.
- `harmony.hpp`: the chord weights decide how strange the progressions get.
