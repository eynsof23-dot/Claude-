# soundscape

A generative soundscape engine in plain C++17. It has no dependencies and no
samples: every sound is synthesised from physical models, oscillators, noise
and filters. Each run composes a new piece, which can be written to a WAV file
or streamed live, endlessly.

It has three styles:

- **electroacoustic** (default). Acousmatic sound worlds made of struck,
  scraped and bowed metal, glass and wood; prepared strings; granular clouds
  that re-process what just happened; frozen instants; sounds that fly
  through space; hard cuts into silence. There are no chords and no beat.
- **cinematic.** Dark tonal film music: drone, pads, choir, braams, risers,
  taiko, a pulsing ostinato.
- **ritual.** A composed 4:30 track: ambient and IDM crossed with tekno drive
  and a cinematic industrial atmosphere. Bowed tortured cymbals, whales,
  field recordings, primal percussion in polymeters, a distorted rolling
  bass with tape echo, a tuned kick, neurobass, glitches and a
  Sherman-style filterbank.

```
  seed 1234  ·  electroacoustic  ·  03:00

  [00:00]  ── DUST    [###.......] arch  ·  harmonic series on 74 Hz, stretched x1.02
  [00:00]  dust · glass
  [00:00]  dust · wood
  [00:04]  accelerando · glass
  [00:05]  sweep up · approaching
  [00:11]  bounce · glass
  [00:34]  CUT ── METAL   [#####.....] falling  ·  odd partials of 79 Hz
  [00:35]  rattling string
  [00:35]  attack · string
  [00:35]  bow · plate
  [00:49]  scrape · plate
  ...
  [01:17]  ── SWARM   [########..] falling  ·  odd partials of 66 Hz
  ...
```

## Build

```sh
cmake -B build && cmake --build build          # or simply:
g++ -std=c++17 -O2 -o soundscape src/main.cpp
```

This works with GCC, Clang and MSVC on Linux, macOS and Windows. A 3-minute
piece renders in about 5 seconds.

## Use

```sh
./soundscape                          # electroacoustic, 3 minutes -> soundscape.wav
./soundscape -s 1234 -d 300           # seed 1234, 5 minutes (same seed = same piece)
./soundscape --style cinematic        # the film-music style
./soundscape --style ritual -s 7      # the 4:30 ritual track (136 BPM)
./soundscape --style ritual --bpm 128 # slower; the length follows the tempo
```

**Listen live, forever:**

```sh
./soundscape --raw | ffplay -f f32le -ar 48000 -ch_layout stereo -nodisp -   # ffmpeg (any OS)
./soundscape --raw | aplay -f FLOAT_LE -c 2 -r 48000                       # Linux
./soundscape --raw | play -t raw -e floating-point -b 32 -c 2 -r 48000 -   # macOS (sox)
```

(Older ffmpeg versions use `-ac 2` instead of `-ch_layout stereo`.)

**Hear one layer at a time.** This is the best way to learn what each
layer does:

```sh
./soundscape -s 1234 --solo resonators
./soundscape -s 1234 --solo grains,glitter
./soundscape -s 1234 --mute sub,air
```

| Style | Layers |
|---|---|
| electroacoustic | `resonators strings grains glitter sweeps air sub` |
| cinematic | `drone pad choir braam booms drums heartbeat risers bells wind ostinato` |
| ritual | `cymbals whales field atmosphere kick perc bass neuro filterbank` |

**Export stems for your DAW.** The same seed always gives the same piece, so
the solos line up sample for sample:

```sh
mkdir -p stems
for l in resonators strings grains glitter sweeps air sub; do
  ./soundscape -q -s 1234 --solo $l -o stems/$l.wav
done
```

Each stem carries its own reverb. The master compressor reacts a little
differently to each stem than to the full mix, so the summed stems won't
match the mix exactly.

## How the electroacoustic style works

| File | What's in it |
|---|---|
| `src/acoustic.hpp` | Physical models: materials, modal bodies, strings, exciters, spatial movement |
| `src/textures.hpp` | Granulator (with its tape loop), noise sweeps, air, sub pressure |
| `src/electroacoustic.hpp` | The composer: scenes, spectra, gestures, cuts |

**Sound objects = exciter × body.** A body is a set of resonant modes
measured from real objects: bar, plate, bowl, bell, glass, wood, skin.
Their non-integer ratios are why they don't sound like "notes". An exciter
is how energy goes in: *strike*, *bounce* (hits accelerating and fading
like a dropped ball), *slowing taps*, *accelerando*, *scrape* (jagged
friction with catches), *bow*, or *dust* (a fine rain of impulses). The
same plate struck, scraped or bowed gives three very different sounds that
still clearly belong to one object.

**Prepared strings.** A Karplus-Strong waveguide that can slide in pitch
mid-note, with a "rattle" that buzzes like a bolt on a piano string.

**The tape.** Everything the objects play is recorded onto a 12-second
loop. The granulator scatters 10–400 ms fragments of it, transposed,
reversed, sliding an octave or thrown across the stereo field, so the
piece keeps transforming its own material. *Freeze* stops the tape and the
grains hover over one instant. *Glitter* grains are pure sines from the
scene's spectrum.

**Space.** Every object has a path: still, drifting, fly-by, approaching or
receding. Distance makes a sound quieter, duller and wetter all at once,
the way a real room does.

**Scenes.** `METAL`, `DUST`, `BOWED`, `SWARM`, `STRINGS`, `VOID`. Each
picks a *spectrum* (e.g. a stretched harmonic series on 58 Hz) and tunes
every object to it. That's how things belong together without a key, the
way spectralist composers work. Scenes follow an energy arc and have their
own contour (rising, falling, arch). They either crossfade, or **cut**:
everything chokes into silence, only the reverb tail survives, and the next
world opens with an attack.

## How the ritual style works

| File | What's in it |
|---|---|
| `src/ritual_instruments.hpp` | Bowed cymbals, whales, field recordings, kick, percussion, bass, neurobass, tape, echo, stutter, filterbank |
| `src/ritual.hpp` | The arrangement: sections, automation curves, the 16th-note sequencer |

| Time | Section |
|---|---|
| 0:00 | **Awakening.** Three bowed cymbals being bent and ground; wind, a creek, crickets, drips; whales |
| 0:35 | **Gathering.** Frame drums (5 in 12), shaker accents every 3, log drum (3 in 7); a muffled bass |
| 1:03 | **The call.** The kick arrives half-time, the bass filter opens, a riser, a beat of silence |
| 1:17 | **Ritual I.** Kick + rolling bass with echo, tresillo deep drum, industrial clank, anvil, bones |
| 2:21 | **Descent.** The beat drops out: whales, thunder, tortured cymbals, neurobass; a drum roll |
| 2:56 | **Ritual II.** The peak: more drive, neurobass, double clank, more glitches |
| 3:52 | **Dissolution.** Instruments leave one by one; the bowed cymbals and field recordings close it |

The seed picks the key (E to A Phrygian), the patterns and every event.
The arrangement is fixed. Techniques worth stealing:

- **Bowed cymbal, tortured.** 40 dense inharmonic modes driven by
  high-passed noise (the bow). Bend all the modes together by a few percent
  for the groan. Add stick-slip "chatter" impulses for the grind.
- **Kick and bass as one engine.** The kick is tuned to the key's root, the
  bass plays the three 16ths *between* kicks, and the bass ducks under
  every kick (sidechain). The sub stays clean, the mids get the tape drive
  and the echo.
- **Asymmetric tape.** A bias before `tanh()` makes even harmonics (warmth).
  Auto-gain keeps the level steady, so drive changes the tone, not the
  volume.
- **Polymeter.** Patterns of 12, 7, 5, 9 and 3 steps over a 16-step bar.
  The bass plays 3 notes per beat but cycles 7 pitches. Nothing lines up
  for long, which makes it hypnotic.
- **IDM stutter.** Grab the last 16th or 32nd and loop it while sliding the
  pitch and crushing the bits. Use it at phrase ends.
- **Filterbank.** Drive, then a resonant low-pass close to
  self-oscillation, then a band-pass in series. Modulate the cutoff with an
  envelope follower, a 7-step LFO and a random step on every 16th. Feed it
  drums *and* water *and* bass, and organic and synthetic melt together.
- **Field recordings from nothing.** A creek is hundreds of tiny sines that
  rise in pitch as they decay (bubbles). Crickets are a 4.5 kHz tone
  switched on and off in pulse trains.

## How the cinematic style works

| File | What's in it |
|---|---|
| `src/layers.hpp` | The 11 instruments, each a commented sound-design recipe |
| `src/harmony.hpp` | Chord progressions (a Markov chain over dark chords) |
| `src/soundscape.hpp` | The conductor: form, orchestration, transitions, timing |

One number drives everything: **intensity** (0 to 1). Sections follow an
arc, and every instrument reacts. Filters open, chords change faster, the
ostinato and drums come in, and risers land on booms tuned to the key.

## Sound-design ideas you can steal

**From the electroacoustic style:**

- **Separate the exciter from the body.** In Vital or Serum: a noise or
  impulse oscillator into a bank of resonant band-pass filters (or a
  resonator / comb device) tuned to non-integer ratios. Change only the
  noise envelope, and you go from a hit to a scrape to a bow.
- **Doublets.** Two copies of every mode, a few cents apart. This is the
  slow shimmer of real bells and bowls.
- **Granulate your own output.** Resample what you just played and run it
  through a granular device (Ableton Granulator, Portal, Quanta). Then
  freeze it.
- **Distance is three controls at once:** volume down, low-pass down,
  reverb send up. Automate them together to move a sound away from you.
- **Cut to silence.** A hard stop with the reverb tail left hanging hits
  harder than any build-up.
- **Tune everything to one spectrum.** Pick a low fundamental and use only
  its (slightly stretched) partials for every sound in a section.

**From the cinematic style:**

- A sub sine kept *out* of the reverb, slightly saturated so small speakers
  hear it.
- Pads: 3 detuned saws per note, low-passed hard. Dark sound design means
  *taking highs away*.
- Braams: distortion *before* the filter, then a resonant low-pass that
  snaps open and closes over ~3 s.
- Booms tuned to the key; risers that cut dead on the impact.

## Tweaking

- `grainParams()` in `electroacoustic.hpp`: grain density, length,
  transpositions and reverse probability per scene.
- The `gesture()` weights decide what happens in each scene. `kRate` sets
  how often.
- `material()` in `acoustic.hpp`: add your own material by listing mode
  ratios.
- Reverb: the `reverb_.init(...)` lines (length in seconds, size, darkness).
