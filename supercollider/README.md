# SuperCollider pieces

Rendered offline (no audio device needed). Requires SuperCollider and
sc3-plugins (for the JPverb reverb). `./render.sh [seed] [piece]`, or open a
`.scd` file in the SuperCollider IDE and run it. A different seed gives a
different take on the same form. `SOLO=<layer>` renders a single layer, so
you can export stems.

## RITE (`rite.scd`): 4:30, 110 BPM

Ceremonial electroacoustic and death-industrial. Every sound plays into one
shared space (a short room, then console saturation, tape and a long hall),
on a single 16th-note grid with polyrhythms. The form follows the map of
nine Falsa Ilusión references: a slow intro, flat grooves where elements
rotate every 8 bars, two 16 s breaks that drop the kick and sub but keep the
textures, and the peak in the last third.

| Time | Bars | Section |
|---|---|---|
| 0:00 | 0-8 | **Invocation.** Gong swell, bell tolls, low chant with throat-singing overtones |
| 0:17 | 8-20 | **Procession.** Frame drums, ritual bells, half-time kick from 0:26, dub bass teaser, first bleeps |
| 0:44 | 20-44 | **The rite.** Kick 3-3-2-2 and bass talking, tresillo tom, hand percussion, grit, growls; modular sequence A from 1:01 |
| 1:36 | 44-52 | **The void** (break). Gong, bells, full choir, the ancient pad, sequence A alone in its echo, temple and machine room |
| 1:53 | 52-76 | **Conjuration.** Kick 4-4-3-3, clank backbeat, metal ticks, noise bed, sequence B with the tempo echo |
| 2:46 | 76-84 | **The descent** (break). The low growl alone with the drone, the long screech, the ancient pad, tolls, a tom roll |
| 3:03 | 84-112 | **The peak.** Kick 3-3-2-2-4-4-3-3 (24 steps), never a straight four; sequence C, then A; the pad underneath from 3:29 |
| 4:04 | 112-124 | **Closing.** The kick walks out, the pad, tolls, the tape wobbles and fades |

**Kick:** a clean sub, an FM "tok" (loud only in the half-time procession,
where it feeds the Sherman and a tempo echo) and a low, warm distortion
layer (40-900 Hz). Drive and distortion change on every step and are
automated per section. The pattern is a polymeter: 3-3-2-2 (10 steps) in
the rite, 4-4-3-3 (14 steps) in the first half of the peak, straight four
only at the climax (v14: no more straight four anywhere; the peak uses
3-3-2-2-4-4-3-3, 24 steps). The bar downbeats are always anchored. The kick owns
everything below 55 Hz.

**Bass (sparse, root only, no sweeps):** distorted sub booms with a short
comb ring (20-step cycle), FM growls (2-in-11) whose colour changes on 3-,
5- and 7-hit cycles and occasionally doubles, low growls an octave under
(3-in-13), and drone swells. They all
run through a bass bus: high-pass at 55 Hz, compression, parallel mid grit
(heard on phones), distortion, mono lows, and a heavy duck under the kick.

**Industrial bass screeches:** a grinding bass tone whose fixed resonances
(the root's 3rd, 5th, 7th, 9th and 13th harmonics, plus a comb on its
octave) ring louder and tighter while noise, crackle and distortion grow.
Nothing sweeps, so nothing shifts in pitch, and it cuts on the downbeat.
The low body stays dry and centred. The resonant part is thrown wide, far
back into the hall and a long tempo echo (half note / dotted half).

**Hats:** a hypnotic pulse on every 3rd sixteenth, with accents cycling in
4s (it loops every 12 steps against the bar). Plus an open hat on a fixed
off-step and a ghost tick before the downbeat. Every 8th bar they breathe
out for half a bar. Every pattern is fixed: there are no random hits.

**Parallel saturation send (the Decapitator / Hamburger / Sherman logic):**
the kick (amount varying per step and building per section) and a little of
the bass go into a parallel bus: warm low-shelf-pushed tube saturation with a
dark tone filter, plus diode-style mid clipping. It's mixed under the clean
signal, and part of it continues into the Sherman.

**Tortured cymbals** at the doors of the piece, a different one every time:
bowed (surging pressure, stick-slip chatter, a slight bend), struck crash,
scraped or reversed swell. Each uses a different body (cymbal, gong, plate,
sheet), pitch, tone and side.
**Whales**, sparse and far away: slow gliding calls through a flanger, the
hall and their own long echo (0.83 / 1.27 s).

**Mix chain (v9):** the bass bus dips 280 Hz and 650 Hz and uses a
frequency-split sidechain (lows duck fully, mids less). Saturation stages are
lighter: console 1.15, drum bus 1.25, master tape at most 1.5-1.8. The
master's low band compresses gently (2:1, slower attack). The hall comes
down in the grooves. The atmosphere drifts with a balance control (not a
rotator, so its polarity never flips) and the hall width is mono-safe.
Checked per band: lows mono (corr 1.0), low-mids 0.91, mids 0.66,
presence 0.45; the whole track loses 0.15 dB when summed to mono.

**Reference-matched mix (v10):** balanced against two Falsa Ilusión
references (FAI AR 12). The boom at 60-120 Hz is reduced at the source: the
saturation send has a gentle low push and a 100 Hz dip, the kick has dips at
75/95 Hz with more clean sub, the bass bus dips at 90/180/280 Hz, the drum
bus at 100/200 Hz, and the Sherman return is high-passed at 150 Hz. The
mids are denser and wider: parallel saturation of 250 Hz-3.5 kHz on the bass
bus, driven differently left and right. Presence comes up 3.5 dB at 4 kHz,
air comes down from 6 kHz, and the tape hiss is halved. A soft clipper feeds
a -1 dBFS limiter: loud parts at about -9.8 LUFS with a crest of about 9.3 dB
(not squashed). Phase checked: mono-sum loss is 0.0-0.3 dB below 300 Hz and
1.2-1.8 dB in the mids.

**v11 (weight, resonances, harshness):**
- **Kick:** heavier, weighted with low saturation. A longer sub, symmetric
  saturation (odd harmonics give weight without the boomy 2nd), and more
  level.
- **Resonances:** where root-tuned layers stack on the same frequency
  (4x, 6x, 8x... the root), narrow notches follow the key. A dynamic
  de-harsher at 2.6 kHz acts only on spikes.
- **Cymbals:** excitation moved down to 1-3.5 kHz, less drive, darker tone
  (11 dB less energy above 2.5 kHz).
- **Bass and atmosphere:** weight restored. The bass's mid saturation is
  smoother, the atmosphere bus has a low shelf, and the chant and hall are
  high-passed lower.
- **Presence:** comes from a parallel 1.5-6 kHz exciter on the master
  (harmonics of the distorted mids), not from EQ hiss.
- **Result:** every band of the peak section sits within the range of the
  two references.

**v12 (the kick no longer squashes the mix):**
- **Measured, not guessed.** Test tones at 237 Hz, 1 kHz, 4 kHz and 9.9 kHz
  were mixed in, and the track was rendered with and without the kick. In
  v11, every kick turned everything else down by about 6 dB. The cause was
  not the limiter. The kick was driving the room's and the tape's
  saturators, which everything else also passes through.
- **The kick has its own wire to the master.** It goes through its own copy
  of the tape (same drive and bias) and its own parallel saturation. It gets
  the same character without dragging the rest down. What's left is about
  1 dB of glue.
- **Deliberate sidechain instead.** The bass sends, delays, Sherman,
  percussion and drum plate duck under the kick by set amounts. The
  atmosphere and bass already did.
- **The room is a send/return.** Each group sends its own amount: atmosphere
  0.25, drums 0.22, the kick's grit and FM 0.2, Sherman 0.18, delays 0.12,
  bass 0.1, and the sub nothing. The hall is high-passed at 160 Hz, and
  everything below 160 Hz is mono.
- **Heavier kick.** More low saturation on the sub, more of the distortion
  layer, and its own saturation 30% louder. A 600 Hz / 1.2 kHz lift brings
  the mids back to the references.
- **Result:** loud parts at -8.9 LUFS, peak -1 dBFS, crest 6.8-6.9 dB (the
  references are at 6.7-6.9 dB). Mono-sum loss is 0.1 dB.

**v13 (nine Falsa Ilusión references, new percussion, 110 BPM):**
- **References measured.** Nine full tracks from the label (Primitif & Shivir,
  Bandikoot, Nosh x2, Ciklo, Andiela, Andiela vs SexWords x3): tempo, the
  loudest 16 s of each, the arrangement over time and the rhythm grid.
- **Tempo 110 BPM** (was 112). The label sits at 100-140, and four of the
  nine are at exactly 110 (Ciklo and all of Andiela vs SexWords). Everything
  is on the step grid, so the form is the same, 3 s longer.
- **The clicky percussion is gone.** From 1:00 the rim (a snare-like noise
  click at 1.4 kHz, 0.9 ms attack) and the wood block (an impulse into a
  resonator, 0.46 ms attack, peaking 5 dB under the whole mix) cut through
  everything. On the same rhythms (3-in-16 and 2-in-7) there is now
  hand percussion: a damped skin (left) and a hollow log (right). A soft noise
  strike excites fixed modal resonators (2-6 ms rise, nothing above 4.5 kHz,
  no pitch envelope). Its peaks sit 10 dB under the mix and its centre is around
  530 Hz. The glitch roll before every 8th bar is now a gravel roll.
- **Noisy percussion here and there.** `grit`: gravel and chain noise
  with crackle, a comb tuned to the key, soft rise, dark (below 5 kHz). 3-in-11,
  only in the second half of every 8-bar phrase, so the top breathes over
  the phrase while the floor stays the same (that's how the references
  vary: constant low end, the hi/air bands moving by up to 10 dB every
  16-32 s). `exhale`: a resonant noise breath over the last beat of each
  phrase, cut dead on the downbeat.
- **Mix matched to the nine-reference median.** RITE was 4 dB short at
  800 Hz-2.5 kHz and 3.7 dB short at 2.5-6 kHz, and the sub was too dominant.
  - The kick's clean sub is down 1 dB, its grit layer is up, and its own
    saturation is 15% louder. Harmonics vs sub went from -9.9 to -8.8 dB
    (the refs' median is -7.6).
  - Less dip at 90-100 Hz on the bass and saturation buses.
  - The bass bus's wide mid saturation is up (0.85 to 1.2).
  - On the master: +5 dB at 1.2 kHz, +4.5 dB at 1.9 kHz, the exciter at
    0.45, the low-mid body at 0.16, 1.5 dB less air, and the mid band of the
    multiband compressor eased.
- **Result:**
  - Every band of the peak is inside the references' range.
  - Loud parts at -7.9 LUFS (the refs' median), peak -0.9 dBFS, crest
    6.4-6.7 dB (the refs are at 6.1-8.6).
  - Mono-sum loss 0.18 dB, peakiness 2.2 dB.
  - The mids are noisier, closer to the references (flatness 0.21; the refs'
    median is 0.23).
  - Pilot test: the kick still doesn't squash anything (the 1-10 kHz tones
    move less than 1 dB in the peak).
- **What the references do that RITE doesn't (yet).** They're 5-7:47 long
  and loudness stays flat for minutes once the groove starts. Breaks are about
  16 s, and they drop the kick and sub but keep the textures. Most peak in the
  last third. Kicks are four-on-the-floor (Bandikoot, Ciklo, Cautela) or a
  broken one-bar pattern (Nosh - Vector at 140). Hats and percussion run
  3-, 5-, 9- and 11-step cycles. Ciklo's bass answers the kick on the 3rd 16th.

**v14 (6 minutes, checked against the nine references):**
- **The form** is above. Elements rotate by bar range (`ranges` in the
  score): typically 4-5 layers are audible at once, 6-7 at the 90th percentile.
  That's the same density as the 3-minute v13, even with the new elements.
- **No straight four.** The climax's 4/4 kick is gone. The peak joins the rite's
  and the conjuration's patterns (3-3-2-2-4-4-3-3, 24 steps against the bar)
  and goes back to 4-4-3-3 for its last 8 bars.
- **New sounds:**
  - Three sparse modular sequences, `lpg`: low-pass-gate pings with phase
    modulation and a wavefolder. Fixed notes from the root's harmonic
    field, no glides, no resonance. The timbre moves on 2/3/5/7 cycles.
    A plays 5-in-13, B 4-in-11 (metallic, tempo echo), C 3-in-7 (woody).
  - `air`: continuous high air, partly correlated noise drifting between
    5.5 and 9.5 kHz with tape dust, its level set per 8-bar phrase.
  - `machine`: a distant room tone for the breaks.
- **Clashes, measured from the solo stems:**
  - The tom (3x the root, 147 Hz) and the low frame drum (4x) sat on the
    kick's body at 135 Hz in 77% of frames. They're now at 5x, 6x and 9x,
    high-passed at 170 Hz, with a softer strike. The clash score went from
    5.6 to 1.7.
  - The bass's 150-220 Hz now ducks fully under the kick, and the bass bus
    has a 135 Hz dip.
  - The Sherman is low-passed at 5.5 kHz and dipped at 850 Hz, where it
    stacked on the bass.
- **Harshness:** 2.5-6 kHz spikes went from 471 to 141 per minute in the rite,
  and from 366 to 147 in the peak.
  - The kick's click is softer (1.5 ms attack, darker) and its FM index is
    lower.
  - Its own tape pre-emphasis is gentler.
  - The hats have a 2 ms attack, are high-passed at 6 kHz and are a little
    quieter.
  - What's left is mostly the kick's own transient.
- **Top end and stereo:**
  - Air at -28 to -31 dB (the refs' median is -31.2), air crest 26-29 dB
    (refs about 28.6), air correlation 0.64-0.70 (refs median 0.71).
  - The air bed and the hats change level per phrase, so the top end moves
    while the floor holds.
  - The mids are narrower (side x0.85 on the master, a smaller L/R offset in
    the bass's mid saturation): correlation is about 0.5 and side/mid -5 dB,
    both the refs' median. Before, they were 0.3 and -2.8 dB.
- **Result:**
  - Every band inside the references' range.
  - Grooves at -7.9 to -8.4 LUFS, peak -0.9 dBFS, crest 6.4-7.0 dB.
  - Mono-sum loss 0.14 dB.
  - Pilot test: the kick moves the other tones by 1.4 dB at most, in every
    section.

**v15 (4:30, no air, audible modular, denser dark atmosphere, ancient pads):**
- **4:30** (124 bars). The same journey, tighter. Phrases are counted from bar
  4, so every section starts on a phrase.
- **The air layer is gone** (too high and brash on the phone). The top end
  sits at the dark end of the references again (air about -35 dB; their
  range is -35.8 to -20.1).
- **The modular sequences were inaudible.** They sat 28-32 dB under the mix in
  the grooves. They're now at 17-20 dB under, with a 2.8 kHz low-pass so the
  louder folds don't add presence spikes (their share of spikes went from 33%
  to 9%).
- **New `bleep`s:** tiny low-pass-gate blips on fixed notes, 3-in-19 and
  2-in-23 cycles drifting against the bar, into the unsynced echo.
- **Denser atmosphere, kept dark:**
  - `temple`: a distant rumble, a slow low-mid wind and far-off resonant
    creaks, 260 Hz-3 kHz only. It runs under every section, louder in the
    breaks.
  - The machine room in both breaks and the closing.
- **Ancient pads (`pad`):** a distant temple choir and harmonium. Open
  fifths and octaves on the root (4x, 6x, 8x, 12x), no thirds. Detuned
  pulses through vowel formants drifting u-o-a: the timbre moves, the notes
  never do. In the void, the descent, under the second half of the peak and
  in the closing.
- **Clashes and crowding, re-measured from the stems:**
  - The temple was first on the kick's body (214 Hz). It's high-passed at
    260 Hz now.
  - The frame drums moved to 8x and 12x the root and the tom to 7x. The
    kick-perc low-mid clash is gone.
  - Percussion ducks a little more under the kick (drum bus 0.3).
  - Stacked percussion is thinned in the peak.
  - Typically 4-5 layers are clearly audible at once, 6-8 at the 90th
    percentile.
- **Result:**
  - All bands inside the references' range, mids at -12.8 to -13.5 dB (the
    refs' median is -12.6).
  - Grooves at -7.8 to -8.8 LUFS, peak -0.9 dBFS, crest 6.4-6.9 dB.
  - Mono loss 0.16 dB, peakiness 2.6-2.8 dB.
  - Pilot test: the kick moves the other tones by 2 dB at most.

**Stereo map:** kick, bass, screech body and sub in the centre (mono below
160 Hz). Closed hats just right of centre, open hat left, ghost ticks
right. Hand skin left, hollow log right, grit alternating left/right, shaker right, anvil right, metal ticks
far right, clank left, frame drums split low-left / high-right, triplet
hand drums alternating. Cymbals, whales, chant, braams and the screech tails
wide. Percussion is tuned above the bass range, and there's a dip at
200-300 Hz on the bass bus and the master against mud.

**Space:** a short room for everything, a short plate for the drums, a long
hall only for the atmospheres. Tempo echoes are used sparingly, and an
unsynced delay (311 / 467 ms) goes on just a few hits. The atmosphere bus
moves slowly: rotating stereo image, drifting tone, breathing level.

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

Layers: `kick bass cymbals whales sherman hats sparse grit seq pad temple machine screech braam metal chant perc noise sub detritus grains`

## ASH (`ash.scd`): 3:00

The first sketch: Rust, Machine, Furnace, Ash. Free-time, no grid.
