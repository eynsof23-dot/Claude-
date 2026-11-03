# SuperCollider pieces

Rendered offline (no audio device needed). Requires SuperCollider and
sc3-plugins (for the JPverb reverb). `./render.sh [seed] [piece]`, or open a
`.scd` file in the SuperCollider IDE and run it. A different seed gives a
different take on the same form. `SOLO=<layer>` renders a single layer, so
you can export stems.

## RITE (`rite.scd`): 3:00, 112 BPM

Ceremonial electroacoustic and death-industrial. Every sound plays into one
shared space (a short room, then console saturation, tape and a long hall),
on a single 16th-note grid with polyrhythms.

| Time | Section |
|---|---|
| 0:00 | **Invocation.** Gong swell, bell tolls, low chant with throat-singing overtones |
| 0:17 | **Procession.** Frame drums 5-in-12, ritual bells every 6 sixteenths, half-time kick, dub bass teaser, braam |
| 0:51 | **The rite.** Kick and bass talking ("boom, da-da"), bass tape echo, tresillo tom, anvil 3-in-5, braams |
| 1:43 | **The void.** Cut: gong, bells, full choir |
| 1:51 | **Conjuration.** The peak: industrial noise bed, tortured bowed metal, clank backbeat |
| 2:43 | **Closing.** The beat leaves, echo tails, tolls; the tape wobbles and fades |

**Kick:** a clean sub, an FM "tok" that feeds the Sherman and a tempo
echo, and a distortion layer for the mental-tekno grit. Drive and distortion
change on every step and are automated per section. The pattern is a
polymeter: half-time in the procession, 3-3-2-2 (10 steps) in the rite,
4-4-3-3 (14 steps) in the first half of the peak, straight four only at the
climax. The bar downbeats are always anchored.

**Bass (sparse, never rolling):** distorted sub booms that glide, bend and
ring a short comb (20-step cycle); FM growls with tempo-locked wah,
pulse-width wobble, pitch bends and auto-pan (2-in-11); flanged, wobbling
drone swells. They all run through a bass bus: compression, parallel mid
grit (so they're heard on phones), distortion, mono lows, a kick duck.

**Bass screeches:** resonant filter swells on the root and sub-octave. The
cutoff climbs from about 50 Hz to about 1 kHz as the resonance tightens and
the volume grows, then it cuts on the downbeat. The real Sherman's cutoff
and resonance swell in sync.

**Hats:** sparse Euclidean cycles (closed 5-in-13, open 3-in-11, ghost
3-in-7), with colour and length changing per step.

**Space:** auto-pan on the hats, percussion, swells and growls; a flanger on
the Sherman return and the bass drones; an unsynced delay (311 / 467 ms) on
the percussion and metal.

**Sherman-style filterbank:** a driven resonant low-pass into a band-pass.
The cutoff follows an envelope follower, a 7-sixteenth LFO and a random step
on every 16th. It's fed the kick's FM layer, the sparse percussion, some
hats and the echo repeats. It never gets the subs, the chant or the
soundscape.

**Rhythm:** polymeters on the 16th grid (5-in-12, 3-in-16, 2-in-7, 1-in-12,
4-in-9, 3-in-5, 3-in-8, accents every 3, 5 and 7) plus true polyrhythms:
hand drums on triplet 8ths (3 against 4) and a metal pulse of 5 per bar
(5 against 4).

**Mix:**
- Drums go through a drum bus (glue compression and saturation) and a
  short, tight plate. The plate is high-passed, so the kick's sub stays dry.
- Atmospheres go through an atmosphere bus (EQ clean-up, slow compression,
  a slight duck under the kick) and the long hall.
- Then everything shares the same short room and console saturation.
- On the master: tape, de-harsh EQ, 3-band multiband compression, mono
  lows, limiter.

Layers: `kick bass sherman hats sparse screech braam metal chant perc noise sub detritus grains`

## ASH (`ash.scd`): 3:00

The first sketch: Rust, Machine, Furnace, Ash. Free-time, no grid.
