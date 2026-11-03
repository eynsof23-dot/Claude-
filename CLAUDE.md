# Working with this producer

The owner of this repo is a music producer making **dark, ceremonial mental
tekno crossed with electroacoustic, ambient, IDM, cinematic and death-industrial
music**. Claude writes and mixes the tracks in SuperCollider and renders them
in the cloud. The producer listens on their **phone** (MP3s sent with
SendUserFile) and gives feedback by ear. They are not a coder, so keep
replies about the music: what changed and what it sounds like.

The current track is **RITE**: `supercollider/rite.scd`, 110 BPM, about 3:03.
Its version history and every design decision are in
`supercollider/README.md`. Read that before changing anything. Work on
branch `claude/music-recommendation-tool-237qop` unless told otherwise.

## The sound they want

**References.** C.Ysme, Dina Rocaille, Yakh, Emiliano Pennisi (Avenir), and
the Falsa Ilusión label (Savage Nihilism, Primitif, Shivir, Ciklo). Not
generic mental tekno: it is connected to ambient and IDM, keeps the
distortion, and has a cinematic, industrial atmosphere. Ceremonial: "like
ceremonial magick is happening".

**Rules (all from their feedback; don't break them):**
- **Tempo:** max 140 BPM.
- **Hypnotic:** little variations, not big changes.
- **Rhythm:** polymeters and polyrhythms, but every rhythm is connected (no
  random hits). The kick pattern can be polymetric too.
- **No pitch changes** in the bass layers or screeches: no sweeps, no pitch
  shift, no slides. Movement comes from filters, resonance, distortion and
  level, never pitch.
- **No acid line.** No "rolling tourouroun" bass under the kick.
- **No percussion that goes "tzioum"**: pitchy zaps, chirpy thumps, or a
  Sherman envelope sweeping on percussion.
- **Nothing commercial-sounding.** Hats are sparse and rhythmically
  interesting, not a 16th-note hat.
- **Kick:** heavy and weighted. Big through LOW saturation (think
  Decapitator, Sherman, Hamburger on sends), with weight and harmonics so the
  mix isn't thin. An FM layer with Sherman and delay. Per-step distortion
  for movement. The other elements sit around the kick, with normal
  sidechain from the kick.
- **Screeches:** BASS / industrial. Resonant, swelling "wooom - woom",
  distant and cinematic, delayed, comb-filtered, distorted, like turning a
  resonant filter up in a Sherman. Never high or harsh. The one in RITE at
  about 2:14 (2:16 since v13 at 110 BPM) is the model.
- **Shermanized sounds:** more of them. Resonant, wet sound design,
  organic and synthetic combined.
- **Distorted bass layers:** they love them (the low growl at 2:02 in an
  earlier version). Modulated, interesting, low-saturated, never fizzy or
  harsh.
- **Tortured bowed cymbals** at specific spots: different every time, never
  harsh.
- **Whales:** sparse, in the background, with clearly audible delay and
  flanger.
- **Atmosphere:** moves slowly, never static. Field recordings and textures.
- **Space:** everything in a shared room, as sends, so each element gets its
  own amount. A separate reverb for drums and for atmospheres. Delays and
  reverbs controlled, not chaotic. The sub never gets reverb (the kick's
  distortion and FM layers can). Everything below 160 Hz is mono. Use auto
  pan, flanger and unsynced delays for width.
- **Tape:** glue with asymmetric tape distortion where it's needed.

**Mix rules:**
- **Mental tekno lives in the mids.** Saturate the mids, avoid mud, never
  harsh.
- Not muddy, not squashed. They noticed when the mix was pushed to -5 LUFS.
- No phase problems. Careful with low cuts and stereo tricks: check mono.
- Always compare with the references.
- Sound-design changes are fine if they improve the track, but when they
  ask to "fix the mix", don't change the sounds or rhythms.

## Reference targets (loud sections of nine Falsa Ilusión tracks)

Band level in dB of the total, then crest and stereo correlation. Measure
with `supercollider/tools/mixcheck.py bands`. The nine (v13): Primitif & Shivir
- Ilusión Oculta (100 BPM), Bandikoot - Reverse Polarity (120), Nosh -
Rhebulp (120) and Vector (140), Ciklo - Clavos y Piedras (110), Andiela - Aöm
(102), Andiela vs SexWords - Cautela, Absorto, Envanecimiento (110).

| band | ref median | ref range | RITE v13 (peak) |
|---|---|---|---|
| sub 20-60 | -1.9 | -5.1 … -0.7 | -1.2 |
| low 60-120 | -11.6 | -14.5 … -6.8 | -12.8 |
| lowmid 120-300 | -10.8 | -17.6 … -7.8 | -11.4 |
| mid-lo 300-800 | -11.6 | -19.9 … -6.0 | -11.4 |
| mid 800-2.5k | -12.6 | -17.1 … -7.3 | -14.2 / -13.0 |
| presence 2.5-6k | -23.4 | -25.3 … -17.7 | -25.1 / -23.2 |
| air 6-15k | -31.2 | -35.8 … -20.1 | -32.1 |

- **References:**
  - Loud parts at -9.5 to -5.8 LUFS (median -7.9).
  - Full-band crest 6.1-8.6 dB (median 6.8).
  - Mid correlation 0.1-0.9.
  - Peakiness 1.4-4.5 dB.
  - The kick's harmonics (150 Hz-2 kHz) sit at -2 to -10 dB vs its sub
    (median -7.6). The kick is tuned to 40-51 Hz.
- **RITE v13:**
  - Loud parts at -7.9 LUFS, peak -0.9 dBFS, crest 6.4-6.7 dB.
  - Mono-sum loss 0.18 dB, peakiness 2.2 dB, kick harmonics -8.8 dB.
- **The old two-clip targets (v10-v12)** matched only the darker of the two
  clips. The nine-track set wants 3-4 dB more at 800 Hz-6 kHz and a little less sub.

When the producer sends more references (files, or SoundCloud/Bandcamp
links via `yt-dlp` when the environment allows those hosts), measure their
loudest 16 s and update this table.

## Workflow

```sh
# setup, if SuperCollider is missing
apt-get update && apt-get install -y --no-install-recommends supercollider-server \
  supercollider-language sc3-plugins-server sc3-plugins-language ffmpeg && pip install yt-dlp

cd supercollider
./render.sh                               # rite.wav, seed 1 (about 3 min)
SOLO=bass ./render.sh                     # one layer
MUTE=kick,hats ./render.sh                # without some layers
ffmpeg -i rite.wav -b:a 320k RITE.mp3     # for the phone
python3 tools/mixcheck.py bands ref.wav@60 rite.wav@121   # compare with a reference
python3 tools/mixcheck.py reso|mono|loud ...               # resonances, phase, loudness
```

- **Parallel renders:** they're safe, because each output gets its own
  `.osc` score file. Pass `OUT=/abs/path.wav`.
- **Where renders go:** keep them and any reference audio out of git (use
  the scratchpad). Commit `.scd` changes and update the README's version
  notes for each version.
- **Before sending a version:** check band balance against the references,
  crest, mono loss, peaks and resonances. When a mix complaint is vague,
  measure first. For example, the "is the kick squashing?" question was
  answered with a pilot-tone test (`mixcheck.py pilot`).
