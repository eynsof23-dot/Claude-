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

**Kick:** a clean sub tuned to the key, plus an FM layer. Only the FM layer
goes through the Sherman filterbank and a tempo delay.

**Bass:** each note rings a comb filter tuned to its pitch, driven until it
screeches, with a clean sub underneath. Separate comb screeches sweep at
phrase ends.

**Sherman-style filterbank:** a driven resonant low-pass into a band-pass.
The cutoff follows an envelope follower, a 7-sixteenth LFO and a random step
on every 16th. It's fed the kick's FM layer, the comb-bass mids, the sparse
percussion, the screeches and some hats. It never gets the subs, the chant
or the soundscape.

**Hats and sparse percussion:** offbeat and ghost hats, 16ths accented
every 3, offbeat open hats; rims 3-in-16, wood blocks 2-in-7, zaps 1-in-12,
glitch rolls at the ends of phrases.

**Mix:** a drum bus with glue compression and saturation, sidechain ducking
on the bass and screeches, lows below 120 Hz in mono, then the shared room,
console saturation, tape and hall.

Layers: `kick bass sherman hats sparse screech braam metal chant perc noise sub detritus grains`

## ASH (`ash.scd`): 3:00

The first sketch: Rust, Machine, Furnace, Ash. Free-time, no grid.
