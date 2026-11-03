# Working with this producer

The owner of this repo is a music producer making **dark, ceremonial mental
tekno crossed with electroacoustic, ambient, IDM, cinematic and death-industrial
music**. Claude writes and mixes the tracks in SuperCollider and renders them
in the cloud. The producer listens on their **phone** (MP3s sent with
SendUserFile) and gives feedback by ear. They are not a coder, so keep
replies about the music: what changed and what it sounds like.

The current track is **RITE**: `supercollider/rite.scd`, 112 BPM, about 3:00.
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
  about 2:14 is the model.
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

## Reference targets (loud sections, from their two reference clips)

Band level in dB of the total, then crest and stereo correlation. Measure
with `supercollider/tools/mixcheck.py bands`.

| band | ref 1 | ref 2 | RITE v12 (peak) |
|---|---|---|---|
| sub 20-60 | -1.6 | -1.0 | -0.9 |
| low 60-120 | -13.5 | -15.4 | -13.1 |
| lowmid 120-300 | -10.0 | -14.6 | -12.6 |
| mid-lo 300-800 | -9.7 | -11.8 | -11.9 |
| mid 800-2.5k | -16.4 | -11.7 | -16.8 |
| presence 2.5-6k | -26.2 | -23.5 | -27.1 |
| air 6-15k | -31.9 | -29.2 | -30.8 |

- **References:** full-band crest 6.7-6.9 dB, about -7.3 LUFS, mids wide
  (correlation 0.1-0.5), resonance peakiness 2.1-3.0 dB.
- **RITE v12:** crest 6.8-6.9 dB, loud parts -8.9 LUFS, peak -1 dBFS,
  mono-sum loss 0.1 dB, peakiness 2.1 dB.

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
