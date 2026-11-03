// RITE — ceremonial electroacoustic / death-industrial / mental tekno piece, 3:00 at 112 BPM.
//
//   bars  0-8   0:00  INVOCATION    gong swell, bell tolls, the chant begins
//   bars  8-24  0:17  PROCESSION    frame drums + ritual bells in polyrhythm, half-time kick,
//                                   dub bass teaser, first braam
//   bars 24-48  0:51  THE RITE      kick + bass talking (with tape echo), metal & drum polymeters,
//                                   braams, tom roll
//   bars 48-52  1:43  THE VOID      cut: gong, bells, full choir
//   bars 52-76  1:51  CONJURATION   the peak: industrial noise bed, tortured metal, clank backbeat
//   bars 76-84  2:43  CLOSING       beat leaves, echo tails, tolls, the tape degrades and fades
//
// MIX. Everything plays into ONE shared space: a short room, then a shared
// console saturation, then the tape (pre/de-emphasis, asymmetric drive,
// wow & flutter, hiss) and a long hall. Same room, same air, same tape: the
// sounds glue into one world instead of sitting side by side.
//
// RHYTHM. One 16th grid, patterns of different lengths over the 16-step bar:
//   frame drums 5-in-12 (3 against 4) · ritual bells every 6 sixteenths
//   anvil 3-in-5 · ritual tom 3-in-8 (tresillo) · shaker accents every 3
//   bass: answers the kick on the 2nd and 3rd 16th of every beat, its pitch
//   cycles over 5 notes while the bar holds 8, so the line keeps rotating.
//
// KICK = clean sub + FM layer; the FM layer (not the sub) feeds the Sherman
// filterbank and a tempo delay. BASS = comb-filter bass: every note rings a
// comb tuned to its pitch, driven until it screeches, with a clean sub under
// it. SHERMAN = driven dual resonant filter (LP into BP) whose cutoff follows
// an envelope follower, a 7-sixteenth LFO and a random step every 16th.
// It gets: kick FM, comb-bass mids, sparse percussion, screeches, some hats.
// It never gets the subs, the chant or the soundscape.
//
// MIX. Drums (kick, hats, percussion) go through a drum bus (glue
// compression + saturation). Bass & screeches duck under the kick. Lows
// below 120 Hz are mono. Everything then shares the same room, console
// saturation, tape and hall.
//
// Render:  SEED=1 OUT=rite.wav sclang rite.scd      (SOLO=<layer> for stems:
//          kick bass sherman hats sparse screech braam metal chant perc noise sub detritus grains)

(
var seed = ("SEED".getenv ? "1").asInteger;
var solo = "SOLO".getenv;
var outPath = "OUT".getenv ? (thisProcess.nowExecutingPath.dirname +/+ "rite.wav");
var bpm = 112, stepDur = 15 / bpm, bars = 84, total = bars * 16 * stepDur;
var busSpace = 16, busHall = 18, busGrain = 20, busBass = 22, busMix = 24, ctlDuck = 10;
var busSherman = 26, busKdel = 28, busDrums = 30;
var opts, server, score, defs, add, set, on, pw, euclid, metalSpec, root, field, pick, at;
var echo, kdel, sherman, drumbus, grainer, space, master, frame, tom, anvil, accent, cycle, cyclePeak, bassN = 0;
var rimP, blockP, zapP;
var tingsha, bellF;

thisThread.randSeed = seed;

opts = ServerOptions.new
  .numOutputBusChannels_(2).numInputBusChannels_(0)
  .sampleRate_(48000).memSize_(65536).maxNodes_(8192);
server = Server(\riteNRT, NetAddr("127.0.0.1", 57191), opts);
score = Score.new;

// ---------------------------------------------------------------------------
// SYNTHDEFS
// ---------------------------------------------------------------------------
defs = [

  // Kick: a clean sub tuned to the root, plus an FM layer (inharmonic
  // sine-on-sine, index collapsing fast) that gives the punch and the
  // metallic "tok". Only the FM layer goes to the Sherman and the delay.
  // Writes a duck envelope to a control bus for the sidechain.
  SynthDef(\kick, { |freq = 49, amp = 0.5, fm = 0.5, sherman = 0.5, kdel = 0.3, mute = 0|
    var subEnv = EnvGen.ar(Env.perc(0.002, 0.5, 1, -4), doneAction: 2);
    var penv = EnvGen.ar(Env([freq * 3.5, freq * 1.15, freq], [0.02, 0.1], \exp));
    var sub = SinOsc.ar(penv) * subEnv;
    var fmEnv = EnvGen.ar(Env.perc(0.001, 0.16, 1, -6));
    var idx = EnvGen.ar(Env([7, 0.5], [0.09], -4));
    var mod = SinOsc.ar(freq * 2 * 1.41) * idx * freq * 2;
    var fmSig = (SinOsc.ar((freq * 2) + mod) * fmEnv * 2).tanh * fm;
    var click = LPF.ar(HPF.ar(WhiteNoise.ar, 2000), 6000) * EnvGen.ar(Env.perc(0.0003, 0.006)) * 0.2;
    var body = ((sub * 1.6).tanh * 0.9) + click + (fmSig * 0.5);
    Out.kr(ctlDuck, EnvGen.kr(Env.perc(0.001, 0.2, 1, -3)));
    Out.ar(busDrums, ((body * amp) ! 2) * (1 - mute));
    Out.ar(busSherman, (fmSig * amp * sherman) ! 2);
    Out.ar(busKdel, (fmSig * amp * kdel) ! 2);
  }),

  // Comb bass: a short burst (saw + noise) rings a comb filter tuned to the
  // note, so the note IS the comb's resonance: hollow, metallic, screechy.
  // "scream" adds a pitch slide into the note and jitter on the comb delay;
  // asymmetric drive; a clean sine sub underneath. Ducks under the kick.
  // Mids also go to the Sherman and the tape echo.
  SynthDef(\cbass, { |freq = 98, amp = 0.3, gate = 0.12, decay = 0.6, scream = 0.2, drive = 2,
      sherman = 0.4, echo = 0.4, mute = 0|
    var env = EnvGen.ar(Env([0, 1, 1, 0], [0.003, gate, 0.05]), doneAction: 2);
    var duck = 1 - (In.kr(ctlDuck).clip(0, 1) * 0.85);
    var exc = ((Saw.ar(freq * 0.5) * 0.4) + PinkNoise.ar(0.4)) * EnvGen.ar(Env.perc(0.001, 0.06));
    var f = freq * LFNoise2.kr(9).range(1, 1 + (scream * 0.03)) * EnvGen.kr(Env([1 + (scream * 0.25), 1], [gate * 0.6], -3));
    var comb = CombC.ar(exc + (Saw.ar(freq) * 0.05), 0.05, 1 / f, decay);
    var sub = SinOsc.ar(freq * 0.5) * 0.55;
    var mid;
    comb = ((comb * drive) + 0.2).tanh - 0.2.tanh;
    mid = LPF.ar(HPF.ar(comb, 110), 6000) * env * amp * duck;
    sub = sub * env * amp * duck;
    Out.ar(busSpace, (((mid * 0.6) + sub) ! 2) * (1 - mute));
    Out.ar(busSherman, (mid * sherman) ! 2);
    Out.ar(busBass, (mid * echo) ! 2);
  }),

  // Comb screech: noise ringing a comb whose pitch sweeps: a scream of
  // metal under stress. Ducks lightly under the kick.
  SynthDef(\screech, { |freq = 1600, freq2 = 250, dur = 1.5, amp = 0.15, decay = 3, pan = 0,
      sherman = 0.5, mute = 0|
    var env = EnvGen.kr(Env([0, 1, 1, 0], [0.05, dur, 0.3], [2, 0, -3]), doneAction: 2);
    var f = EnvGen.kr(Env([freq, freq2], [dur], \exp)) * LFNoise2.kr(12, 0.02, 1);
    var duck = 1 - (In.kr(ctlDuck).clip(0, 1) * 0.5);
    var src = BPF.ar(PinkNoise.ar, f * 2, 0.5) + (Dust.ar(200) * 0.3);
    var sig = CombC.ar(src, 0.05, 1 / f, decay);
    sig = LPF.ar(LeakDC.ar((sig * 2).tanh), 6500) * env * amp * duck;
    sig = Pan2.ar(sig, pan);
    Out.ar(busSpace, sig * (1 - mute));
    Out.ar(busHall, sig * 0.4 * (1 - mute));
    Out.ar(busSherman, sig * sherman);
  }),

  // Hats: six detuned square waves (the classic metallic hat cluster) +
  // noise, band-limited so they shimmer without getting harsh.
  SynthDef(\hat, { |amp = 0.1, decay = 0.04, pan = 0, tone = 0.5, sherman = 0.1, mute = 0|
    var env = EnvGen.ar(Env.perc(0.001, decay, 1, -6), doneAction: 2);
    var freqs = [205.3, 304.4, 369.6, 522.7, 540, 800] * tone.linexp(0, 1, 1.0, 1.5);
    var sig = Mix(Pulse.ar(freqs, 0.5)) * 0.2;
    sig = BPF.ar(sig, 8500, 0.9) + (HPF.ar(WhiteNoise.ar, 7000) * 0.2);
    sig = LPF.ar(HPF.ar(sig, 5500), 12000) * env * amp * 48;
    sig = Pan2.ar(sig, pan);
    Out.ar(busDrums, sig * (1 - mute));
    Out.ar(busSherman, sig * sherman);
  }),

  // Sparse percussion: rim (0), wood block (1), zap (2), glitch (3).
  SynthDef(\click, { |type = 0, freq = 1000, amp = 0.2, decay = 0.08, pan = 0, sherman = 0.5, kdel = 0.2, mute = 0|
    var env = EnvGen.ar(Env.perc(0.0005, decay, 1, -5), doneAction: 2);
    var rim = (BPF.ar(WhiteNoise.ar, freq * 1.8, 0.3) * 3) + (SinOsc.ar(freq) * 0.5);
    var block = Ringz.ar(Impulse.ar(0), [freq, freq * 1.47], [0.06, 0.04]).sum * 0.3;
    var zap = SinOsc.ar(EnvGen.ar(Env([freq * 4, freq * 0.2], [0.04], \exp)));
    var glitch = Latch.ar(WhiteNoise.ar, Impulse.ar(freq * 2)) * LFPulse.ar(freq * 0.25);
    var sig = LPF.ar(Select.ar(type, [rim, block, zap, glitch]) * env * amp, 9000);
    sig = Pan2.ar(sig, pan);
    Out.ar(busDrums, sig * (1 - mute));
    Out.ar(busSherman, sig * sherman);
    Out.ar(busKdel, sig * kdel);
  }),

  // Tape echo (used twice: bass, and the kick's FM layer): left/right
  // times differ, cross-fed, every repeat thinner, darker and a little more
  // saturated, with slight wobble. Can feed the Sherman too.
  SynthDef(\echo, { |in = 22, tL = 0.4, tR = 0.53, fb = 0.5, mix = 0.8, hp = 250, lp = 2800, toSherman = 0, mute = 0|
    var input = In.ar(in, 2);
    var local = LocalIn.ar(2);
    var dl = DelayC.ar(input + local, 2, [tL, tR] + (LFNoise2.kr(0.5 ! 2) * 0.002));
    LocalOut.ar((LPF.ar(HPF.ar(dl.reverse, hp), lp) * fb * 1.3).tanh / 1.3);
    Out.ar(busSpace, dl * mix * (1 - mute));
    Out.ar(busHall, dl * 0.25 * (1 - mute));
    Out.ar(busSherman, dl * toSherman);
  }),

  // SHERMAN-STYLE FILTERBANK: input overdriven, then a resonant low-pass
  // pushed near self-oscillation and saturated again, then a band-pass
  // above it in series. Cutoff = envelope follower (every hit squelches) +
  // LFO over 7 sixteenths (polymeter against the bar) + a random step on
  // every 16th (smoothed).
  SynthDef(\sherman, { |base = 250, reso = 0.75, envAmt = 0.8, lfoAmt = 0.5, drive = 4, mix = 1, step = 0.134, mute = 0|
    var in = In.ar(busSherman, 2);
    var env = Amplitude.kr(in.sum, 0.002, 0.12);
    var lfo = SinOsc.kr(1 / (step * 7));
    var sh = Lag.kr(TRand.kr(-1, 1, Impulse.kr(1 / step)), 0.01);
    var cut = (base.lag(2) * (2 ** ((envAmt * (env * 6).clip(0, 1) * 5) + (lfoAmt * lfo * 2) + (sh * 1.5)))).clip(40, 12000);
    var rq = reso.lag(2).linlin(0, 1, 0.7, 0.05);
    var x = (in * drive.lag(2)).tanh;
    var y1 = (RLPF.ar(x, cut, rq) * 1.6).tanh;
    var y2 = BPF.ar(y1, (cut * 2.4).clip(40, 14000), rq * 1.5) * 2;
    var sig = LPF.ar(LeakDC.ar(((y1 * 0.6) + (y2 * 0.9)).tanh), 9000) * 0.5 * mix;
    Out.ar(busSpace, sig * (1 - mute));
    Out.ar(busHall, sig * 0.25 * (1 - mute));
  }),

  // DRUM BUS: one compressor across kick, hats and percussion (glue), then
  // gentle asymmetric saturation, then into the shared space.
  SynthDef(\drumbus, { |thresh = 0.3, sat = 1.4|
    var in = In.ar(busDrums, 2);
    var sig = Compander.ar(in, in, thresh, 1, 1/3, 0.005, 0.12);
    sig = ((sig * sat) + 0.1).tanh - 0.1.tanh;
    sig = sig / (sat * (1 - 0.1.tanh.squared));
    Out.ar(busSpace, sig);
  }),

  // Braam: root + octave + twelfth, three detuned saws each, DRIVEN BEFORE
  // the filter, then a resonant low-pass that opens and slowly closes.
  SynthDef(\braam, { |freq = 55, amp = 0.3, atk = 0.05, dur = 4, bright = 0.7, mute = 0|
    var env = EnvGen.ar(Env([0, 1, 0.7, 0], [atk, dur * 0.5, dur * 0.5], [2, -2, -4]), doneAction: 2);
    var fenv = EnvGen.kr(Env([0, 1, 0.25], [atk + 0.12, dur * 0.8], [2, -4]));
    var l = Mix([1, 2, 3].collect { |r, n| Saw.ar(freq * r * [-0.11, 0, 0.12].midiratio, [1, 0.8, 0.5][n] / 3).sum });
    var r = Mix([1, 2, 3].collect { |r2, n| Saw.ar(freq * r2 * [-0.13, 0.02, 0.1].midiratio, [1, 0.8, 0.5][n] / 3).sum });
    var sig = ([l, r] * 2.5).tanh;
    sig = RLPF.ar(sig, 60 + (fenv * bright * 3000), 0.45);
    sig = HPF.ar(sig, 45) * env * amp;
    Out.ar(busSpace, sig * (1 - mute));
    Out.ar(busHall, sig * 0.3 * (1 - mute));
    Out.ar(busGrain, sig * 0.2);
  }),

  // Metal: 16 inharmonic modes (bowl, gong, bell, plate, bar, sheet), struck
  // (0), scraped (1) or bowed (2), optionally bent. "hard" = mallet: soft
  // felt to steel (tamed so it never gets harsh). "dist" = distance.
  SynthDef(\metal, { |freq = 200, amp = 0.2, dur = 4, mode = 0, bend = 1, drive = 1, pan = 0,
      dist = 0.3, hard = 0.5, hall = 0.4, gsend = 0.4, mute = 0|
    var ratios = \ratios.kr(1 ! 16), amps = \amps.kr(0.05 ! 16), rings = \rings.kr(1 ! 16);
    var bendEnv = Line.kr(1, bend, dur) * LFNoise2.kr(4, 0.004, 1);
    var strike = LPF.ar(Decay2.ar(Impulse.ar(0), 0.0005, 0.012) * (1 + PinkNoise.ar(0.5)),
      hard.linexp(0, 1, 900, 9000));
    var scrape = BPF.ar(WhiteNoise.ar, LFNoise1.kr(6).exprange(700, 3000), 0.6)
      * LFNoise0.ar(LFNoise1.kr(1).range(12, 50)).range(0, 1).squared * EnvGen.kr(Env.sine(dur));
    var bow = BPF.ar(PinkNoise.ar, freq * 4, 0.4)
      * EnvGen.kr(Env([0, 1, 0.8, 0], [0.35, 0.35, 0.3] * dur, \sin)) * LFNoise1.kr(7, 0.25, 1);
    var exc = Select.ar(mode, [strike, scrape * 0.4, bow * 0.3]);
    var sig = DynKlank.ar(`[ratios, amps, rings], exc, freq * bendEnv);
    sig = LeakDC.ar((sig * drive).tanh);
    sig = LPF.ar(sig, (1 - dist).linexp(0, 1, 2200, 11000));
    DetectSilence.ar(sig.abs + Line.ar(1, 0, dur), 0.0002, 0.3, doneAction: 2);
    sig = Pan2.ar(sig, pan) * amp * (1 - (dist * 0.5));
    Out.ar(busSpace, sig * (1 - mute));
    Out.ar(busHall, sig * (hall + (dist * 0.4)) * (1 - mute));
    Out.ar(busGrain, sig * gsend);
  }),

  // Chant: low monk voices. A buzzing source through three moving vowel
  // formants (u - o - a), plus a narrow resonance that steps along the
  // harmonics: overtone (throat) singing.
  SynthDef(\chant, { |freq = 98, amp = 0.2, atk = 6, sus = 20, rel = 8, overtone = 8, pan = 0,
      hall = 0.5, gsend = 0.3, mute = 0|
    var env = EnvGen.kr(Env.linen(atk, sus, rel, 1, \sin), doneAction: 2);
    var vib = LFNoise2.kr(0.2, 0.003, 1) * SinOsc.kr(Rand(4.5, 5.5), 0, 0.002, 1);
    var src = (Saw.ar(freq * vib * [1, Rand(1.002, 1.006), Rand(0.995, 0.998)]).sum * 0.33) + PinkNoise.ar(0.04);
    var v = LFNoise2.kr(0.07).range(0, 1);
    var f = [v.linlin(0, 1, 300, 700), v.linlin(0, 1, 700, 1150), v.linlin(0, 1, 2400, 2700)];
    var sig = Mix(BPF.ar(src, f, [0.12, 0.1, 0.08], [1, 0.6, 0.2])) * 3;
    var ot = (freq * LFNoise2.kr(0.1).range(overtone * 0.75, overtone * 1.25).round(1)).lag(0.5);
    sig = sig + (BPF.ar(src, ot, 0.012) * 5);
    sig = Pan2.ar(LPF.ar(HPF.ar(sig, 120), 5000), pan) * env * amp;
    Out.ar(busSpace, sig * (1 - mute));
    Out.ar(busHall, sig * hall * (1 - mute));
    Out.ar(busGrain, sig * gsend);
  }),

  // Frame drum / ritual tom: a skin (membrane modes) whose pitch settles
  // after the hit, plus the low thump of the palm.
  SynthDef(\frame, { |freq = 90, amp = 0.4, decay = 0.6, hard = 0.3, pan = 0, mute = 0|
    var exc = LPF.ar(Decay2.ar(Impulse.ar(0), 0.0008, 0.01) * (1 + PinkNoise.ar(0.4)), hard.linexp(0, 1, 800, 6000));
    var penv = EnvGen.kr(Env([1.15, 1], [0.05], -4));
    var sig = DynKlank.ar(`[[1, 1.593, 2.136, 2.296, 2.653, 2.918, 3.156],
      [1, 0.5, 0.4, 0.3, 0.25, 0.2, 0.15] * 0.1, [1, 0.6, 0.5, 0.4, 0.3, 0.25, 0.2] * decay], exc, freq * penv);
    var thump = SinOsc.ar(freq * EnvGen.ar(Env([1.6, 1], [0.04]))) * EnvGen.ar(Env.perc(0.002, decay * 0.5)) * 0.5;
    sig = ((sig + thump) * 1.5).tanh;
    DetectSilence.ar(sig, 0.0002, 0.2, doneAction: 2);
    sig = Pan2.ar(sig, pan) * amp;
    Out.ar(busDrums, sig * (1 - mute));
    Out.ar(busHall, sig * 0.1 * (1 - mute));
  }),

  // Shaker / seed rattle.
  SynthDef(\shaker, { |amp = 0.1, pan = 0, mute = 0|
    var env = EnvGen.ar(Env([0, 1, 0], [0.004, 0.07], [2, -5]), doneAction: 2);
    var sig = LPF.ar(BPF.ar(WhiteNoise.ar, Rand(3500, 5500), 1.0), 7000) * env * amp;
    Out.ar(busDrums, Pan2.ar(sig, pan) * (1 - mute));
  }),

  // Industrial noise bed: pink noise + crackle, resonant low-pass, metallic
  // comb tube, driven. Darker than ASH's noise wall.
  SynthDef(\noisewall, { |amp = 0.2, atk = 20, sus = 10, rel = 1.5, lo = 150, hi = 2200, mute = 0|
    var env = EnvGen.kr(Env([0, 1, 1, 0], [atk, sus, rel], [3, 0, -4]), doneAction: 2);
    var sig = PinkNoise.ar(1 ! 2) + (Crackle.ar(1.95 ! 2) * 0.25);
    var cut = EnvGen.kr(Env([lo, hi, hi], [atk, sus], \exp)) * LFNoise2.kr(0.4 ! 2).range(0.6, 1.4);
    sig = RLPF.ar(sig, cut, 0.3);
    sig = sig + (CombC.ar(sig, 0.05, 1 / LFNoise2.kr(0.1 ! 2).range(40, 70), 1.5) * 0.25);
    sig = LPF.ar((sig * 4).tanh, 4000) * env * amp;
    Out.ar(busSpace, sig * (1 - mute));
    Out.ar(busHall, sig * 0.2 * (1 - mute));
    Out.ar(busGrain, sig * 0.2);
  }),

  SynthDef(\sub, { |freq = 50, amp = 0.4, dur = 6, mute = 0|
    var sig = SinOsc.ar(EnvGen.kr(Env([freq * 2, freq * 0.6], [dur], \exp)));
    sig = (sig * EnvGen.kr(Env.perc(0.02, dur, 1, -3), doneAction: 2) * 1.6).tanh * amp;
    Out.ar(busSpace, (sig ! 2) * (1 - mute));
  }),

  // Concrete detritus: tiny resonant clicks and grit (kept below 4 kHz).
  SynthDef(\detritus, { |amp = 0.1, density = 6, dur = 20, mute = 0|
    var env = EnvGen.kr(Env.linen(2, (dur - 4).max(0.1), 2, 1, \sin), doneAction: 2);
    var trig = Dust.ar(density ! 2);
    var sig = Ringz.ar(trig * TRand.ar(0.1, 1, trig), TExpRand.ar(300, 4000, trig), TExpRand.ar(0.005, 0.06, trig));
    sig = sig + (LPF.ar(Dust2.ar((density * 3) ! 2), 3000) * 0.12);
    sig = HPF.ar(sig, 200) * env * amp;
    Out.ar(busSpace, sig * (1 - mute));
    Out.ar(busHall, sig * 0.3 * (1 - mute));
    Out.ar(busGrain, sig * 0.4);
  }),

  // Granulator: records what is sent to it, scatters grains of the recent past.
  SynthDef(\grainer, { |density = 4, gdur = 0.15, amp = 0.5, spread = 0.3, rev = 0.3, mute = 0|
    var frames = 48000 * 8;
    var buf = LocalBuf(frames, 1).clear;
    var input = In.ar(busGrain, 2).sum;
    var write = Phasor.ar(0, 1, 0, frames);
    var trig, pos, rate, sig;
    BufWr.ar(input, buf, write);
    trig = Dust.kr(density.lag(4));
    pos = ((write / frames) - TRand.kr(0.15, 0.95, trig)).wrap(0, 1);
    rate = TChoose.kr(trig, [0.5, 1, 1, 2, 0.75, 1.5]) * TRand.kr(1 - spread, 1 + spread, trig);
    rate = rate * (TRand.kr(0, 1, trig) > rev).linlin(0, 1, -1, 1);
    sig = GrainBuf.ar(2, trig, TRand.kr(gdur * 0.5, gdur * 1.6, trig), buf, rate, pos, 4,
      TRand.kr(-0.9, 0.9, trig), -1, 512);
    sig = LPF.ar(LeakDC.ar(sig), 7000) * amp.lag(3) * (1 - mute);
    Out.ar(busSpace, sig * 0.6);
    Out.ar(busHall, sig * 0.6);
  }),

  // THE SHARED SPACE: every sound passes through the same short room and the
  // same console saturation before the master.
  SynthDef(\space, { |roomMix = 0.35, sat = 1.3|
    var in = In.ar(busSpace, 2);
    var room = JPverb.ar(HPF.ar(in, 140), 1.3, 0.5, 0.8, 0.707, 0.2, 1.0, 1, 1, 0.6, 300, 6000);
    var sig = in + (room * roomMix);
    sig = ((sig * sat) + 0.08).tanh - 0.08.tanh;
    sig = sig / (sat * (1 - 0.08.tanh.squared));
    Out.ar(busMix, sig);
  }),

  // Master: long hall (pre-delayed, widened), then the tape, a de-harsh EQ
  // for the 3-4 kHz zone, glue and a limiter.
  SynthDef(\master, { |drive = 1.4, bias = 0.12, wow = 0.3, hiss = 0.004, hallMix = 0.8, gain = 1,
      fade = 1, fadeTime = 3|
    var sig = In.ar(busMix, 2);
    var hall = JPverb.ar(DelayN.ar(HPF.ar(In.ar(busHall, 2), 160), 0.05, 0.045),
      8, 0.4, 2.6, 0.707, 0.3, 0.5, 1.0, 0.8, 0.5, 250, 4500);
    var d = drive.lag(5), w = wow.lag(5), mid, side;
    mid = (hall[0] + hall[1]) * 0.5;
    side = (hall[0] - hall[1]) * 0.5 * 1.4;
    hall = [mid + side, mid - side];
    sig = sig + (hall * hallMix);
    sig = HPF.ar(sig, 24);
    // lows below 120 Hz in mono: the kick and sub hit the center as one
    mid = (sig[0] + sig[1]) * 0.5;
    side = HPF.ar((sig[0] - sig[1]) * 0.5, 120);
    sig = [mid + side, mid - side];
    sig = sig + (HPF.ar(sig, 3000) * 0.5);
    sig = ((sig * d) + bias).tanh - bias.tanh;
    sig = sig / (d * (1 - bias.tanh.squared));
    sig = BHiShelf.ar(sig, 3000, 1, -4);
    sig = BPeakEQ.ar(sig, 3400, 1.2, -3);
    sig = BPeakEQ.ar(sig, 320, 1.0, -2.5);   // make room in the low-mids
    sig = DelayC.ar(sig, 0.03, 0.006 + (LFNoise2.kr(0.7) * 0.0012 * w) + (SinOsc.kr(5.8) * 0.00015 * w));
    sig = sig + HPF.ar(PinkNoise.ar(hiss.lag(5) ! 2), 1500);
    sig = LPF.ar(sig, 12500);
    sig = LeakDC.ar(sig);
    sig = Compander.ar(sig, sig, 0.25, 1, 0.5, 0.01, 0.25);
    sig = Limiter.ar(sig * gain * Lag.kr(fade, fadeTime), 0.93, 0.01);
    Out.ar(0, sig);
  })
];

// ---------------------------------------------------------------------------
// HELPERS
// ---------------------------------------------------------------------------
score.add([0.0, ['/g_new', 1, 0, 0]]);
defs.do { |d| score.add([0.0, ['/d_recv', d.asBytes]]) };

on = { |layer| if(solo.isNil or: { solo == layer }) { 0 } { 1 } };
add = { |time, name, args, layer|
  var s = Synth.basicNew(name, server);
  score.add([time, s.newMsg(Group.basicNew(server, 1), args ++ [\mute, on.(layer)], \addToHead)]);
  s
};
set = { |time, node, args| score.add([time, node.setMsg(*args)]) };
at = { |bar, step = 0| ((bar * 16) + step) * stepDur };

// piecewise-linear automation over bars: pts = [[bar, value], ...]
pw = { |x, pts|
  var res = pts.last[1];
  block { |break|
    if(x <= pts[0][0]) { res = pts[0][1]; break.value };
    (pts.size - 1).do { |k|
      if(x < pts[k + 1][0]) {
        res = pts[k][1] + ((pts[k + 1][1] - pts[k][1]) * (x - pts[k][0]) / (pts[k + 1][0] - pts[k][0]));
        break.value;
      };
    };
  };
  res
};
euclid = { |k, n, rot = 0| Array.fill(n, { |i| (((i * k) % n) < k).binaryValue }).rotate(rot) };

root = [41.2, 43.65, 46.25, 49.0].choose;          // E1 F1 F#1 G1: kick and sub sit here
field = root * [3, 4, 5, 6, 7, 8, 9, 11, 13, 16];  // every metal is tuned to these
pick = { |lo, hi|
  var f = field.choose;
  while { f < lo } { f = f * 2 };
  while { f > hi } { f = f / 2 };
  f
};
metalSpec = { |kind, ring = 1|
  var base = switch(kind,
    \bowl, { [1, 1.003, 2.71, 2.716, 5.15, 5.16, 8.43, 8.45, 12.39, 12.42, 17.0, 17.05, 3.3, 4.1, 6.2, 7.0] },
    \gong, { Array.fill(16, { |k| ((k + 1) ** 1.15) * rrand(0.97, 1.03) }) },
    \bell, { [0.5, 1, 1.183, 1.506, 2, 2.514, 2.662, 3.011, 4.166, 5.433, 6.796, 8.215, 9.1, 10.3, 11.7, 13.2] },
    \plate, { [1, 1.35, 1.67, 2.13, 2.39, 2.84, 3.17, 3.52, 3.98, 4.41, 4.97, 5.43, 6.12, 6.7, 7.3, 8.1] },
    \bar, { [1, 2.756, 5.404, 8.933, 13.34, 1.01, 2.77, 5.43, 8.97, 3.1, 4.4, 6.9, 11.2, 1.52, 2.21, 7.7] },
    { Array.fill(16, { |k| (k + 1) ** 1.33 }) }  // sheet
  );
  var ratios = base.collect { |r| r * rrand(0.99, 1.01) };
  var amps = ratios.collect { |r, k| rrand(0.3, 1.0) * ((k + 1) ** -0.5) * 0.05 };
  var rings = ratios.collect { |r| rrand(0.6, 1.4) * (r ** -0.4) * ring };
  [\ratios, ratios, \amps, amps, \rings, rings]
};

// rhythmic material (seeded)
frame = euclid.(5, 12, 12.rand);
tom = euclid.(3, 8, 0);
anvil = euclid.(3, 5, 5.rand);
accent = euclid.(3, 8, 8.rand);
cycle = [[0, 0, 1, 0, -2], [0, 1, 0, 0, 3], [0, 0, -2, 0, 1]].choose;
cyclePeak = cycle.copy.put(3, 12);
tingsha = [pick.(1400, 2600), pick.(1400, 2600)];
bellF = pick.(110, 230);
rimP = euclid.(3, 16, 16.rand);     // rims: 3 in 16
blockP = euclid.(2, 7, 7.rand);     // wood blocks: 2 in 7 (polymeter)
zapP = euclid.(1, 12, 12.rand);     // zaps: 1 in 12

// ---------------------------------------------------------------------------
// FX CHAIN (created first, at the tail, so every source runs before it)
// ---------------------------------------------------------------------------
// order: bass echo -> kick echo -> Sherman -> drum bus -> grains -> space -> master
echo = Synth.basicNew(\echo, server);
score.add([0.01, echo.newMsg(Group.basicNew(server, 1),
  [\in, busBass, \tL, 3 * stepDur, \tR, 4 * stepDur, \fb, 0.5, \toSherman, 0.15, \mute, on.("bass")], \addToTail)]);
kdel = Synth.basicNew(\echo, server);
score.add([0.0102, kdel.newMsg(Group.basicNew(server, 1),
  [\in, busKdel, \tL, 3 * stepDur, \tR, 6 * stepDur, \fb, 0.45, \mix, 0.7, \hp, 400, \lp, 3500,
   \toSherman, 0.4, \mute, on.("kick")], \addToTail)]);
sherman = Synth.basicNew(\sherman, server);
score.add([0.0104, sherman.newMsg(Group.basicNew(server, 1),
  [\step, stepDur, \base, 250, \reso, 0.7, \drive, 4, \mix, 0.55, \mute, on.("sherman")], \addToTail)]);
drumbus = Synth.basicNew(\drumbus, server);
score.add([0.0106, drumbus.newMsg(Group.basicNew(server, 1), [], \addToTail)]);
grainer = Synth.basicNew(\grainer, server);
score.add([0.011, grainer.newMsg(Group.basicNew(server, 1),
  [\density, 2, \gdur, 0.3, \amp, 1.4, \spread, 0.05, \rev, 0.3, \mute, on.("grains")], \addToTail)]);
space = Synth.basicNew(\space, server);
score.add([0.012, space.newMsg(Group.basicNew(server, 1), [], \addToTail)]);
master = Synth.basicNew(\master, server);
score.add([0.013, master.newMsg(Group.basicNew(server, 1), [\gain, 1.35], \addToTail)]);

// ---------------------------------------------------------------------------
// LONG GESTURES
// ---------------------------------------------------------------------------
// gong: bowed swell to open, struck at the doors of each section
add.(0.05, \metal, [\freq, pick.(55, 110), \amp, 0.7, \dur, 14, \mode, 2, \drive, 1, \pan, 0, \dist, 0.4,
  \hall, 0.5] ++ metalSpec.(\gong, 4), "metal");
[[24, 0.45], [48, 0.3], [52, 0.45], [76, 0.4]].do { |ga|
  var b = ga[0];
  add.(at.(b), \metal, [\freq, pick.(55, 110), \amp, ga[1], \dur, 0.5, \mode, 0, \hard, 0.15, \pan, rrand(-0.2, 0.2),
    \dist, 0.35, \hall, 0.6] ++ metalSpec.(\gong, 4), "metal");
};
add.(at.(44), \metal, [\freq, pick.(55, 110), \amp, 0.6, \dur, 4 * 16 * stepDur, \mode, 2, \pan, 0, \dist, 0.3]
  ++ metalSpec.(\gong, 3), "metal");

// singing bowls through the opening and the void
[[1, 12], [5, 14], [48.2, 8]].do { |p|
  add.(at.(p[0]), \metal, [\freq, pick.(180, 520), \amp, 0.45, \dur, p[1], \mode, 2, \pan, rrand(-0.7, 0.7),
    \dist, 0.5] ++ metalSpec.(\bowl, 3), "metal");
};

// the chant: voices enter one by one; a full choir in the void
add.(at.(0, 4), \chant, [\freq, root * 2, \amp, 0.45, \atk, 10, \sus, 30, \rel, 12, \overtone, 8, \pan, -0.2], "chant");
add.(at.(4), \chant, [\freq, root * 3, \amp, 0.30, \atk, 10, \sus, 26, \rel, 12, \overtone, 6, \pan, 0.3], "chant");
add.(at.(20), \chant, [\freq, root * 2.004, \amp, 0.35, \atk, 8, \sus, 50, \rel, 10, \overtone, 9, \pan, 0.1], "chant");
[[2, 0.27, -0.4, 8], [3, 0.2, 0.4, 6], [4, 0.15, -0.1, 5], [2.997, 0.15, 0.2, 10]].do { |v|
  add.(at.(47, 12), \chant, [\freq, root * v[0], \amp, v[1], \atk, 2, \sus, 7, \rel, 8, \overtone, v[3],
    \pan, v[2], \hall, 0.8], "chant");
};
add.(at.(54), \chant, [\freq, root * 2, \amp, 0.35, \atk, 10, \sus, 40, \rel, 8, \overtone, 8, \pan, -0.2], "chant");
add.(at.(60), \chant, [\freq, root * 3, \amp, 0.25, \atk, 8, \sus, 30, \rel, 8, \overtone, 12, \pan, 0.3], "chant");
add.(at.(74), \chant, [\freq, root * 2, \amp, 0.45, \atk, 6, \sus, 14, \rel, 12, \overtone, 8, \pan, 0, \hall, 0.8], "chant");

// braams at the phrase doors (swells at 16 and 44; the 44 one on the b2: dread)
[[16, 1.5, 1], [32, 0.05, 1], [40, 0.05, 1], [44, 2.5, 1.0595], [52, 0.05, 1], [60, 0.05, 1], [68, 0.05, 1]].do { |b|
  add.(at.(b[0]), \braam, [\freq, root * b[2], \amp, 0.2, \atk, b[1], \dur, rrand(4.5, 6.5), \bright, rrand(0.5, 0.8)], "braam");
};

// a long comb scream across the void
add.(at.(48, 4), \screech, [\freq, pick.(600, 1200), \freq2, pick.(100, 250), \dur, 6, \amp, 0.05,
  \decay, 6, \pan, 0, \sherman, 0.3], "screech");

// subs under the two big entries
[24, 52].do { |b| add.(at.(b), \sub, [\freq, root, \amp, 0.5, \dur, 6], "sub") };

// tortured bowed sheet metal, bending down
[40, 60, 72].do { |b|
  add.(at.(b) + rrand(0, 1.0), \metal, [\freq, pick.(70, 220), \amp, 0.5, \dur, rrand(10.0, 14.0), \mode, 2,
    \bend, rrand(0.84, 0.92), \drive, 2, \pan, rrand(-0.6, 0.6), \dist, 0.3] ++ metalSpec.(\sheet, 2), "metal");
};

// industrial noise bed through the conjuration
add.(at.(56), \noisewall, [\amp, 0.16, \atk, 18, \sus, (18 * 16 * stepDur) - 18, \rel, 1.5], "noise");

// detritus
add.(at.(2), \detritus, [\amp, 0.45, \density, 3, \dur, 20 * 16 * stepDur], "detritus");
add.(at.(52), \detritus, [\amp, 0.45, \density, 6, \dur, 24 * 16 * stepDur], "detritus");

// ---------------------------------------------------------------------------
// THE GRID
// ---------------------------------------------------------------------------
(bars * 16).do { |s|
  var bar = s div: 16, st = s % 16, time = s * stepDur;
  var rite = (bar >= 24) and: { bar < 48 };
  var conj = (bar >= 52) and: { bar < 76 };
  var groove = rite or: { conj };
  var hush = (bar == 47) and: { st >= 8 };
  var x = bar + (st / 16);

  // kick: half-time in the procession, four on the floor in the rite.
  // The FM layer grows through the piece and feeds the Sherman + delay.
  if(hush.not) {
    var kfm = pw.(x, [[12, 0.3], [24, 0.45], [52, 0.7], [76, 0.8]]);
    var ksh = pw.(x, [[12, 0.3], [24, 0.45], [52, 0.65]]);
    if((bar >= 12) and: { bar < 24 } and: { (st == 0) or: { st == 8 } }) {
      add.(time, \kick, [\freq, root, \amp, 0.4, \fm, kfm, \sherman, ksh, \kdel, 0.2], "kick");
    };
    if(groove and: { st % 4 == 0 }) {
      add.(time, \kick, [\freq, root, \amp, 0.55, \fm, kfm, \sherman, ksh, \kdel, 0.3], "kick");
    };
    if(groove and: { st == 14 } and: { bar.odd } and: { 0.6.coin }) {
      add.(time, \kick, [\freq, root, \amp, 0.3, \fm, kfm, \sherman, ksh, \kdel, 0.4], "kick");
    };
  };

  // bass answers the kick: "boom, da-da"; a muffled dub teaser in the procession
  if(hush.not and: {
    ((bar >= 16) and: { bar < 24 } and: { st % 4 == 2 }) or: { groove and: { (st % 4 == 2) or: { st % 4 == 3 } } }
  }) {
    var pat = if(bar >= 60) { cyclePeak } { cycle };
    var iv = pat[bassN % pat.size];
    var vel = if(accent[bassN % 8] == 1) { 1 } { 0.72 };
    add.(time, \cbass, [\freq, root * 2 * iv.midiratio, \amp, 0.54 * vel, \gate, stepDur * 0.8,
      \decay, pw.(x, [[16, 0.3], [24, 0.4], [47, 0.8], [52, 0.6], [76, 1.2]]),
      \scream, pw.(x, [[16, 0.0], [24, 0.15], [47, 0.5], [52, 0.4], [76, 0.9]]) * vel,
      \drive, pw.(x, [[16, 1.5], [24, 2], [52, 2.5], [76, 3.5]]),
      \sherman, pw.(x, [[16, 0.2], [24, 0.35], [52, 0.5]]),
      \echo, pw.(x, [[16, 0.9], [24, 0.5], [52, 0.6]])], "bass");
    bassN = bassN + 1;
  };

  // hats: offbeat 8ths + ghost 16ths in the rite; 16ths with accents every 3
  // and offbeat open hats in the conjuration; a few in the procession
  if(hush.not) {
    if((bar >= 16) and: { bar < 24 } and: { st == 6 or: { st == 14 } }) {
      add.(time, \hat, [\amp, 0.05, \decay, 0.04, \pan, 0.3], "hats");
    };
    if(rite and: { st % 4 == 2 }) {
      add.(time, \hat, [\amp, 0.09, \decay, 0.05, \pan, 0.25, \sherman, 0.15], "hats");
    };
    if(rite and: { st.odd } and: { bar >= 28 } and: { 0.5.coin }) {
      add.(time, \hat, [\amp, 0.03, \decay, 0.025, \pan, rrand(-0.4, 0.4), \tone, 0.7], "hats");
    };
    if(conj and: { st % 4 != 2 }) {
      add.(time, \hat, [\amp, if(s % 3 == 0) { 0.08 } { 0.035 }, \decay, 0.035,
        \pan, (s * 0.07).sin * 0.4, \tone, 0.6, \sherman, 0.2], "hats");
    };
    if(conj and: { st % 4 == 2 }) {
      add.(time, \hat, [\amp, 0.07, \decay, if(bar >= 56) { 0.22 } { 0.06 }, \pan, 0.3, \tone, 0.4,
        \sherman, 0.25], "hats");
    };
  };

  // sparse percussion: rims 3-in-16, blocks 2-in-7, zaps 1-in-12, glitch rolls
  if(groove and: { hush.not } and: { bar >= 28 }) {
    if(rimP[st] == 1) {
      add.(time, \click, [\type, 0, \freq, 900, \amp, 0.336, \decay, 0.05, \pan, -0.45, \sherman, 0.5,
        \kdel, 0.25], "sparse");
    };
    if(blockP[s % 7] == 1) {
      add.(time, \click, [\type, 1, \freq, pick.(500, 1100), \amp, 0.84, \decay, 0.12, \pan, 0.5,
        \sherman, 0.3, \kdel, 0.15], "sparse");
    };
    if(conj and: { zapP[s % 12] == 1 } and: { 0.6.coin }) {
      add.(time, \click, [\type, 2, \freq, rrand(500, 1200), \amp, 0.336, \decay, 0.07,
        \pan, rrand(-0.7, 0.7), \sherman, 0.6, \kdel, 0.4], "sparse");
    };
    if((bar % 4 == 3) and: { st >= 12 } and: { 0.5.coin }) {
      add.(time, \click, [\type, 3, \freq, rrand(1500, 4000), \amp, 0.224, \decay, 0.06,
        \pan, rrand(-0.6, 0.6), \sherman, 0.4, \kdel, 0.3], "sparse");
    };
  };

  // comb screeches at the ends of phrases
  if(((rite and: { bar % 8 == 7 }) or: { conj and: { bar % 4 == 3 } }) and: { st == 8 } and: { 0.7.coin } and: { hush.not }) {
    var up = 0.4.coin;
    add.(time, \screech, [\freq, if(up) { rrand(200, 400) } { rrand(1200, 2400) },
      \freq2, if(up) { rrand(1200, 2400) } { rrand(150, 350) }, \dur, stepDur * rrand(6, 8),
      \amp, 0.12, \decay, rrand(1.5, 4.0), \pan, rrand(-0.5, 0.5), \sherman, 0.5], "screech");
  };

  // frame drums: 5 in 12 (3 against 4)
  if((((bar >= 8) and: { bar < 48 }) or: { (bar >= 52) and: { bar < 80 } }) and: { frame[s % 12] == 1 } and: { hush.not }) {
    var low = (s % 12) < 6;
    add.(time, \frame, [\freq, root * (if(low) { 2 } { 3 }), \amp, (if(low) { 0.1 } { 0.07 }) * rrand(0.85, 1.0),
      \decay, if(low) { 0.7 } { 0.45 }, \hard, rrand(0.2, 0.4), \pan, if(low) { -0.3 } { 0.3 }], "perc");
  };

  // ritual tom: tresillo in the grooves, a roll into the void
  if(groove and: { tom[s % 8] == 1 } and: { hush.not } and: { (bar < 46) or: { bar >= 48 } }) {
    add.(time, \frame, [\freq, root * 1.5, \amp, 0.105, \decay, 0.9, \hard, 0.45, \pan, -0.1], "perc");
  };
  if(((bar == 46) and: { st % 2 == 0 }) or: { (bar == 47) and: { st < 8 } }) {
    add.(time, \frame, [\freq, root * 1.5, \amp, (x - 46).linlin(0, 1.5, 0.08, 0.2), \decay, 0.6, \hard, 0.5,
      \pan, rrand(-0.2, 0.2)], "perc");
  };

  // shaker: every 16th, accents every 3
  if(groove and: { hush.not } and: { 0.85.coin }) {
    add.(time, \shaker, [\amp, (if(s % 3 == 0) { 0.032 } { 0.014 }) * rrand(0.8, 1.0),
      \pan, (s * 0.05).sin * 0.5], "perc");
  };

  // ritual bells (tingsha): every 6 sixteenths, two pitches alternating
  if((((bar >= 10) and: { bar < 47 }) or: { (bar >= 52) and: { bar < 76 } }) and: { s % 6 == 0 }) {
    add.(time, \metal, [\freq, tingsha[(s div: 6) % 2], \amp, if(rite or: { conj }) { 0.16 } { 0.12 }, \dur, 0.5,
      \mode, 0, \hard, 0.35, \pan, if((s div: 6).even) { -0.5 } { 0.5 }, \dist, 0.55, \hall, 0.5, \gsend, 0.5]
      ++ metalSpec.(\bell, 1.2), "metal");
  };

  // anvil: 3 in 5
  if(groove and: { bar >= 32 } and: { anvil[s % 5] == 1 } and: { hush.not }) {
    add.(time, \metal, [\freq, pick.(500, 1100), \amp, 0.22, \dur, 0.3, \mode, 0, \hard, 0.55, \pan, 0.4,
      \dist, 0.3, \hall, 0.2, \gsend, 0.3] ++ metalSpec.(\bar, 0.35), "metal");
  };

  // clank backbeat in the conjuration
  if(conj and: { st == 12 }) {
    add.(time, \metal, [\freq, pick.(250, 450), \amp, 0.35, \dur, 0.3, \mode, 0, \hard, 0.6, \drive, 2,
      \pan, -0.15, \dist, 0.2, \hall, 0.3, \gsend, 0.5] ++ metalSpec.(\plate, 0.4), "metal");
  };

  // bell tolls in the invocation, the void and the closing
  if((st == 0) and: {
    ((bar < 8) and: { bar.even }) or: { (bar >= 48) and: { bar < 52 } } or: { (bar >= 76) and: { bar.even } }
  }) {
    add.(time, \metal, [\freq, bellF, \amp, if((bar >= 48) and: { bar < 52 }) { 0.28 } { 0.45 }, \dur, 0.5,
      \mode, 0, \hard, 0.3, \pan, rrand(-0.3, 0.3),
      \dist, 0.6, \hall, 0.6] ++ metalSpec.(\bell, 3), "metal");
  };
};

// ---------------------------------------------------------------------------
// AUTOMATION
// ---------------------------------------------------------------------------
set.(at.(8), grainer, [\density, 5, \gdur, 0.2, \spread, 0.2]);
set.(at.(24), grainer, [\density, 8, \gdur, 0.12, \spread, 0.4, \rev, 0.35]);
set.(at.(48), grainer, [\density, 3, \gdur, 0.6, \spread, 0.02, \rev, 0.5]);
set.(at.(52), grainer, [\density, 14, \gdur, 0.08, \spread, 0.7, \rev, 0.45]);
set.(at.(76), grainer, [\density, 3, \gdur, 0.5, \spread, 0.05, \rev, 0.6, \amp, 0.9]);
set.(at.(52), master, [\drive, 1.8]);
set.(at.(24), sherman, [\base, 280, \reso, 0.75, \drive, 4.5]);
set.(at.(48), sherman, [\base, 180, \reso, 0.85, \drive, 3]);
set.(at.(52), sherman, [\base, 320, \reso, 0.85, \drive, 6, \lfoAmt, 0.7]);
set.(at.(76), sherman, [\base, 200, \reso, 0.7, \drive, 3]);
set.(at.(76), master, [\drive, 1.5, \wow, 1.2, \hiss, 0.006]);
set.(at.(80), master, [\wow, 2.2, \drive, 2.2, \hiss, 0.008]);
set.(at.(81), master, [\fadeTime, 5, \fade, 0]);

score.add([total, [\c_set, 0, 0]]);
score.sort;

("RITE  seed " ++ seed ++ "  root " ++ root.round(0.01) ++ " Hz  ->  " ++ outPath).postln;
score.recordNRT(outPath ++ ".osc", outPath, nil, 48000, "WAV", "int24", opts, "", total, {
  File.delete(outPath ++ ".osc");
  "done".postln;
  0.exit;
});
)
