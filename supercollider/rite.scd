// RITE — ceremonial electroacoustic / death-industrial / mental tekno piece, 4:30 at 110 BPM.
//
//   bars   0-8    0:00  INVOCATION    gong swell, bell tolls, the chant begins
//   bars   8-20   0:17  PROCESSION    frame drums + ritual bells, half-time kick, dub bass teaser
//   bars  20-44   0:44  THE RITE      kick 3-3-2-2 + bass, hand percussion, grit, modular sequence A
//   bars  44-52   1:36  THE VOID      break: gong, bells, full choir, ancient pad, sequence A alone
//   bars  52-76   1:53  CONJURATION   kick 4-4-3-3, clank backbeat, noise bed, sequence B
//   bars  76-84   2:46  THE DESCENT   break: the low growl alone, the long screech, ancient pad
//   bars  84-112  3:03  THE PEAK      kick 3-3-2-2-4-4-3-3 (never a straight four), sequence C
//   bars 112-124  4:04  CLOSING       the kick walks out, ancient pad, tolls, the tape fades
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
// filterbank and a tempo delay; a DIST layer gives the tekno grit; drive and
// distortion change per step. The kick plays POLYMETERS (10- and 14-step).
// SCREECHES = resonant filter SWELLS (cutoff up, resonance tight, volume up,
// cut on the downbeat) with the Sherman swelling in sync.
// SHERMAN = driven dual resonant filter (LP into BP) whose cutoff follows an
// envelope follower, a 7-sixteenth LFO and a random step every 16th. It gets
// kick FM, sparse percussion, hats, screech echoes; never subs or the chant.
//
// BASS (all sparse, never rolling) -> BASS BUS (comp, distortion, mono lows,
// heavy duck under the kick): a distorted sub BOOM gliding onto the root on
// a 20-step cycle, a driven FM GROWL through the Sherman on an 11-step cycle,
// and distorted DRONE swells under the phrases.
//
// POLYRHYTHM. Besides the 16th-grid polymeters there are true polyrhythms:
// hand drums on triplet 8ths (3 against 4) and a metal pulse of 5 per bar
// (5 against 4) in the peak.
//
// MIX (signal flow):
//   drums  (kick, hats, percussion, rhythmic metal) -> DRUM BUS: glue comp + sat
//          + short tight PLATE (high-passed: the kick sub stays dry)
//   atmos  (chant, gongs, bowls, bells, braams, screeches, noise, grains)
//          -> ATMOS BUS: EQ clean-up, slow comp, slight duck under the kick
//          + long HALL (atmospheres only)
//   delays: kick-FM echo and screech echo, tempo-synced
//   all -> the SAME ROOM (short early reflections) -> console saturation
//       -> MASTER: tape, de-harsh EQ, 3-band multiband compression, limiter
//
// Render:  SEED=1 OUT=rite.wav sclang rite.scd      (SOLO=<layer> for stems:
//          kick bass cymbals whales sherman hats sparse grit seq pad temple machine screech braam metal chant perc noise sub detritus grains)

(
var seed = ("SEED".getenv ? "1").asInteger;
var solo = "SOLO".getenv;
var outPath = "OUT".getenv ? (thisProcess.nowExecutingPath.dirname +/+ "rite.wav");
var bpm = 110, stepDur = 15 / bpm, bars = 124, total = bars * 16 * stepDur;
var busSpace = 16, busHall = 18, busGrain = 20, busBass = 22, busMix = 24, ctlDuck = 10;
var busSherman = 26, busKdel = 28, busDrums = 30, busDverb = 32, busAtmos = 34, busLow = 36, busUdel = 38, busSat = 40, busWdel = 42, busWhd = 44, busKick = 46, busKickM = 48, busKsat = 50, busRoom = 52;
var busSdel = busBass;  // the old bass echo bus now carries the screech echo
var opts, server, score, defs, add, set, on, pw, euclid, metalSpec, root, field, pick, at;
var wdel, whd, satbus, lowP, ksat, bassbus, boomP, growlP, kdrive, ksend, kN = 0, kpA, kpB, kdistC, hatA, hatO, hatG, hatTone, udel, dverb, atmosbus, echo, kdel, sherman, drumbus, grainer, space, master, frame, tom, anvil, accent, cycle, cyclePeak, bassN = 0;
var rimP, blockP, zapP, gritP;
var kpC, ranges, inR, seqAP, seqBP, seqCP, seqAN, seqBN, seqCN, bleepP, bleepQ, bleepN;
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

  // Kick: three layers.
  //  * SUB: a clean sine tuned to the root (the weight)
  //  * FM "tok": metallic mid-range punch; feeds the Sherman and the echo.
  //    Loud in the half-time procession, pulled back once the kick drives.
  //  * DIST: the body overdriven into an asymmetric clipper, kept LOW
  //    (40-900 Hz) with a short tail: warm, saturated tekno weight.
  // "drive" and "dist" change on every step and are automated per section.
  // Writes a duck envelope to a control bus for the sidechain.
  SynthDef(\kick, { |freq = 49, amp = 0.5, fm = 0.5, drive = 1.6, dist = 0.5, sherman = 0.5, kdel = 0.3, sat = 0.5, mute = 0|
    var subEnv = EnvGen.ar(Env.perc(0.002, 0.7, 1, -3.5), doneAction: 2);   // longer, heavier sub
    var penv = EnvGen.ar(Env([freq * 3.5, freq * 1.15, freq], [0.02, 0.1], \exp));
    var sine = SinOsc.ar(penv);
    var sub = sine * subEnv;
    var fmEnv = EnvGen.ar(Env.perc(0.001, 0.12, 1, -5));
    var idx = EnvGen.ar(Env([7, 0.5], [0.07], -4));   // v14: a little less FM bite (presence spikes)
    var fmSig = SinOsc.ar(freq * 4 * EnvGen.ar(Env([1.5, 1], [0.03])), SinOsc.ar(freq * 4 * 1.41) * idx) * fmEnv;
    var dl = ((sine * (4 + (drive * 3))) + 0.3).tanh - 0.3.tanh;
    var click = LPF.ar(HPF.ar(WhiteNoise.ar, 1500), 4500) * EnvGen.ar(Env.perc(0.0015, 0.008)) * 0.1;   // v14: softer, darker click
    var body;
    dl = (dl + (dl.squared * 0.25)).softclip;
    dl = LPF.ar(HPF.ar(dl, 40), 900) * EnvGen.ar(Env.perc(0.002, 0.3, 1, -3)) * dist;  // low, warm grit
    sub = (((sub * drive * 1.3) + 0.1).tanh - 0.1.tanh) * (1.6.tanh / (drive * 1.3).tanh);
    // LOW SATURATION for weight: symmetric drive on the sub adds odd harmonics
    // (3x, 5x the root), heard as weight on any speaker, without the boomy 2nd
    sub = (sub * 2.8).tanh / 2.8.tanh;
    fmSig = (fmSig * (1 + drive)).tanh * fm;
    body = (sub * 1.15) + click + (fmSig * 0.7) + (dl * 1.4);   // more grit: the kick's harmonics carry it on any speaker
    body = BPeakEQ.ar(body, 95, 1.0, -2.5);   // keep the weight in the sub, not the boom
    body = BPeakEQ.ar(body, 75, 0.9, -2);
    Out.kr(ctlDuck, EnvGen.kr(Env.perc(0.001, 0.28, 1, -3)));
    Out.ar(busKick, ((body * amp) ! 2) * (1 - mute));   // own path: never pumps the drums
    Out.ar(busSherman, (fmSig * amp * sherman) ! 2);
    Out.ar(busKdel, (fmSig * amp * kdel) ! 2);
    Out.ar(busDverb, (fmSig * amp * 0.1) ! 2);
    Out.ar(busKsat, ((sub + (dl * 0.5)) * amp * sat ! 2) * (1 - mute));   // parallel saturation send (own wire)
    Out.ar(busRoom, (((fmSig * 0.7) + (dl * 0.6)) * amp * 0.2 ! 2) * (1 - mute));   // room send: the grit and the tok, never the sub
  }),

  // SWELL (the industrial bass screech): a grinding bass tone (saws on the
  // root, a sub pulse) whose FIXED resonances (the root's 3rd, 5th, 7th, 9th
  // and 13th harmonics, plus a comb on its octave) ring louder and tighter
  // while noise, crackle and distortion grow: a machine straining. Nothing
  // sweeps, so there is no pitch movement. Cuts dead on the downbeat. Only the
  // low body stays dry in the bass bus; the resonant screech is thrown wide,
  // far back into the hall and a long tempo echo ("wooom ... woom").
  SynthDef(\swell, { |freq = 49, dur = 4, amp = 0.2, drive = 3, pan = 0, sherman = 0.2, mute = 0|
    var env = EnvGen.kr(Env([0, 1, 0], [dur, 0.05], [4, -4]), doneAction: 2);
    var p = EnvGen.kr(Env([0, 1], [dur], 3));
    var src = (Saw.ar(freq * [1, 1.003]) * 0.4) + (Pulse.ar(freq * 0.5, 0.35) * 0.4)
      + (PinkNoise.ar(0.4) * p) + (Crackle.ar(1.95) * 0.15 * p);
    var res = Mix(BPF.ar(src, freq * [3, 5, 7, 9, 13], (0.05 - (p * 0.042)).max(0.006), [1, 0.8, 0.6, 0.5, 0.35]))
      * (1 + (p * 9));
    var comb = CombC.ar(src * 0.2, 0.05, [1 / (freq * 2), 1 / (freq * 2.004)], 1.5 + (p * 2));
    var sig = (src * 0.25) + res + (comb * p);
    var low, wide;
    sig = ((sig * (1 + (drive * p * 4.5))) + 0.2).tanh - 0.2.tanh;
    sig = LPF.ar(LeakDC.ar(sig), 2200) * env * amp;
    low = LPF.ar(sig.sum * 0.5, 150);   // the low body stays dry and centred (bass bus)
    wide = HPF.ar(sig, 150);            // the resonant screech goes wide and far back
    Out.ar(busLow, (low ! 2) * (1 - mute));
    Out.ar(busAtmos, wide * 0.45 * (1 - mute));
    Out.ar(busHall, wide * 0.7 * (1 - mute));
    Out.ar(busWdel, wide * 0.6 * (1 - mute));
    Out.ar(busSherman, sig * sherman);
  }),

  // BOOM: a distorted sub hit at a fixed pitch (only a 12 ms attack
  // transient, no glide), a short static comb ring, and a saturated mid band
  // so it reads on phones.
  SynthDef(\boom, { |freq = 49, amp = 0.4, decay = 0.9, drive = 3, comb = 0.3, sherman = 0.3, sat = 0.2, mute = 0|
    var penv = EnvGen.ar(Env([freq * 1.5, freq], [0.012], -4));
    var env = EnvGen.ar(Env.perc(0.005, decay, 1, -3), doneAction: 2);
    var sig = SinOsc.ar(penv) + (SinOsc.ar(penv * 2) * 0.15);
    var mid;
    sig = ((sig * drive) + 0.25).tanh - 0.25.tanh;
    sig = sig + (CombC.ar(sig, 0.05, 1 / (freq * 4), 0.3) * comb);
    mid = (LPF.ar(HPF.ar(sig, 150), 1600) * 3).tanh * 0.35;
    sig = (LPF.ar(sig, 1800) + mid) * env * amp;
    Out.ar(busLow, (sig ! 2) * (1 - mute));
    Out.ar(busSherman, (HPF.ar(sig, 200) * sherman) ! 2);
    Out.ar(busSat, (sig * sat ! 2) * (1 - mute));
  }),

  // GROWL: FM + saw + pulse at a fixed pitch through a fixed resonant
  // low-pass and a "talking" band-pass, driven hard. No sweeps: the colour
  // comes from the FM index decaying and from the drive.
  SynthDef(\growl, { |freq = 98, amp = 0.3, dur = 0.3, fmAmt = 2, cut = 900, drive = 4,
      sherman = 0.6, dly = 0.1, sat = 0.2, pan = 0, mute = 0|
    var env = EnvGen.ar(Env([0, 1, 0.7, 0], [0.005, dur * 0.5, dur * 0.5], [-2, -2, -4]), doneAction: 2);
    var mod = SinOsc.ar(freq * 0.5) * fmAmt * EnvGen.ar(Env([1, 0.3], [dur]));
    var osc = SinOsc.ar(freq, mod) + (Saw.ar(freq * 1.005) * 0.4) + (Pulse.ar(freq * 0.5, 0.4) * 0.3);
    var sig = RLPF.ar(osc, cut, 0.2);
    sig = sig + (BPF.ar(sig, (cut * 1.8).clip(60, 9000), 0.15) * 0.7);
    sig = ((sig * drive) + 0.2).tanh - 0.2.tanh;
    sig = Pan2.ar(LPF.ar(HPF.ar(sig, 50), 2000) * env * amp, pan);   // dark after the drive: warm, not harsh
    Out.ar(busLow, sig * (1 - mute));
    Out.ar(busSherman, sig * sherman);
    Out.ar(busSdel, sig * dly * (1 - mute));
    Out.ar(busSat, sig * sat * (1 - mute));
  }),

  // DRONE: a distorted reese-like swell at a fixed pitch and fixed filter;
  // it grows by volume and drive only.
  SynthDef(\bdrone, { |freq = 49, amp = 0.25, atk = 2, sus = 2, rel = 2, cut = 400, drive = 3, mute = 0|
    var env = EnvGen.kr(Env.linen(atk, sus, rel, 1, [2, 0, -3]), doneAction: 2);
    var osc = (Saw.ar(freq * [0.997, 1.003]) * 0.5) + (Pulse.ar(freq * 0.5, 0.3) * 0.4);
    var sig = RLPF.ar(osc, cut, 0.3);
    sig = ((sig * drive * (0.6 + (env * 0.8))) + 0.2).tanh - 0.2.tanh;
    sig = LPF.ar(sig, 2500) * env * amp * 0.6;
    Out.ar(busLow, sig * (1 - mute));
  }),

  // TORTURED CYMBAL: dense inharmonic modes (cymbal, gong, plate or sheet),
  // played four ways: BOWED (surging pressure + stick-slip chatter + a slight
  // bend), a STRUCK crash with a long ring, SCRAPED (jagged friction), or a
  // REVERSED swell that cuts dead. Saturated; tone and placement vary.
  SynthDef(\cymbal, { |freq = 300, amp = 0.2, dur = 6, bend = 0.99, chatter = 0.5, mode = 0, tone = 8500,
      pan = 0, hall = 0.5, sherman = 0.15, mute = 0|
    // mode 0 bowed · 1 struck crash · 2 scraped · 3 reversed swell (cut dead)
    var ratios = \ratios.kr(1 ! 16), amps = \amps.kr(0.05 ! 16), rings = \rings.kr(1 ! 16);
    var env = EnvGen.kr(Env([0, 1, 0.8, 0], [dur * 0.4, dur * 0.4, dur * 0.2], [3, 0, -3]));
    var rev = EnvGen.kr(Env([0, 1, 0], [dur, 0.02], [6, -4]));
    var press = env * LFNoise2.kr(3).range(0.6, 1.2);
    var bow = (BPF.ar(PinkNoise.ar, LFNoise2.kr(0.5).exprange(1200, 3500), 0.5) * press * 0.3)
      + (Dust.ar(LFNoise2.kr(1).range(60, 220)) * press * chatter * 0.5);
    var crash = Decay2.ar(Impulse.ar(0), 0.001, 0.06) * (1 + PinkNoise.ar(0.8)) * 1.5;
    var scrape = BPF.ar(PinkNoise.ar, LFNoise1.kr(5).exprange(900, 3000), 0.6)
      * LFNoise0.ar(LFNoise1.kr(1).range(20, 60)).range(0, 1).squared * env * 0.5;
    var swell = BPF.ar(PinkNoise.ar, 2200, 0.8) * rev * 0.5;
    var exc = Select.ar(mode, [bow, crash, scrape, swell]);
    var sig = DynKlank.ar(`[ratios, amps, rings], exc, freq * Line.kr(1, bend, dur));
    sig = LPF.ar(LPF.ar(LeakDC.ar((sig * 1.8).tanh), tone), tone) * amp;   // darker, less fizz
    sig = sig * Select.kr(mode, [1, 1, 1, rev]);
    DetectSilence.ar(sig.abs + Line.ar(1, 0, dur), 0.0002, 0.5, doneAction: 2);
    sig = Pan2.ar(sig, (pan + (LFNoise2.kr(0.2) * 0.2)).clip(-1, 1));
    Out.ar(busAtmos, sig * (1 - mute));
    Out.ar(busHall, sig * hall * (1 - mute));
    Out.ar(busSherman, sig * sherman);
  }),

  // WHALE: a distant humpback-like call. A tone with a few harmonics that
  // glides slowly (shape 0: moan from freq to freq2; 1: cry, up and back),
  // soft vibrato, an occasional growl (fast amplitude flutter), filtered as
  // if under water, then a slow flanger; sent far into the hall and the
  // unsynced echo.
  SynthDef(\whale, { |freq = 200, freq2 = 260, dur = 3, amp = 0.1, shape = 0, growl = 0, pan = 0, mute = 0|
    var p = Line.kr(0, 1, dur);
    var glide = Select.kr(shape, [p, (p * pi).sin]);
    var f = freq * ((freq2 / freq) ** glide) * SinOsc.kr(Rand(3.0, 5.0), 0, 0.006, 1);
    var env = EnvGen.kr(Env([0, 1, 0.8, 0], [dur * 0.3, dur * 0.4, dur * 0.3], \sin));
    var sig = Mix(SinOsc.ar(f * [1, 2, 3, 4], 0, [1, 0.5, 0.25, 0.12]));
    Line.kr(0, 1, dur + 0.05, doneAction: 2);
    sig = sig * (1 - (growl * (0.5 + (0.5 * SinOsc.kr(Rand(18.0, 30.0))))));
    sig = LPF.ar(sig, 1600) * env * amp;
    sig = sig + DelayC.ar(sig, 0.012, SinOsc.kr(0.15).range(0.001, 0.008));
    sig = Pan2.ar(sig, (pan + (LFNoise2.kr(0.1) * 0.3)).clip(-1, 1));
    Out.ar(busAtmos, sig * 0.4 * (1 - mute));
    Out.ar(busHall, sig * 0.9 * (1 - mute));
    Out.ar(busWhd, sig * 0.8 * (1 - mute));   // their own long echo
  }),

  // Hats: six detuned square waves (the classic metallic hat cluster) +
  // noise, band-limited so they shimmer without getting harsh.
  SynthDef(\hat, { |amp = 0.1, decay = 0.04, pan = 0, tone = 0.5, sherman = 0.1, dverb = 0.12, udel = 0, mute = 0|
    var env = EnvGen.ar(Env.perc(0.002, decay, 1, -6), doneAction: 2);   // v14: 2 ms attack (no spike)
    var freqs = [205.3, 304.4, 369.6, 522.7, 540, 800] * tone.linexp(0, 1, 1.0, 1.5);
    var sig = Mix(Pulse.ar(freqs, 0.5)) * 0.2;
    sig = BPF.ar(sig, 8500, 0.9) + (HPF.ar(WhiteNoise.ar, 7000) * 0.2);
    sig = LPF.ar(HPF.ar(sig, 6000), 11000) * env * amp * 36;
    sig = Pan2.ar(sig, pan);
    Out.ar(busDrums, sig * (1 - mute));
    Out.ar(busDverb, sig * dverb * (1 - mute));
    Out.ar(busUdel, sig * udel * (1 - mute));
    Out.ar(busSherman, sig * sherman);
  }),

  // Sparse percussion: rim (0), wood block (1), zap (2), glitch (3).
  SynthDef(\click, { |type = 0, freq = 1000, amp = 0.2, decay = 0.08, pan = 0, sherman = 0.5, kdel = 0.2,
      dverb = 0.2, udel = 0, mute = 0|
    var env = EnvGen.ar(Env.perc(0.0005, decay, 1, -5), doneAction: 2);
    var rim = (BPF.ar(WhiteNoise.ar, freq * 1.8, 0.3) * 3) + (SinOsc.ar(freq) * 0.5);
    var block = Ringz.ar(Impulse.ar(0), [freq, freq * 1.47], [0.06, 0.04]).sum * 0.3;
    var zap = SinOsc.ar(EnvGen.ar(Env([freq * 4, freq * 0.2], [0.04], \exp)));
    var glitch = Latch.ar(WhiteNoise.ar, Impulse.ar(freq * 2)) * LFPulse.ar(freq * 0.25);
    var sig = LPF.ar(Select.ar(type, [rim, block, zap, glitch]) * env * amp, 9000);
    sig = Pan2.ar(sig, pan);
    Out.ar(busDrums, sig * (1 - mute));
    Out.ar(busDverb, sig * dverb * (1 - mute));
    Out.ar(busUdel, sig * udel * (1 - mute));
    Out.ar(busSherman, sig * sherman);
    Out.ar(busKdel, sig * kdel);
  }),

  // HAND PERCUSSION (v13, replaces the rim and the wood block, which clicked
  // and sat in the way): a hand on a damped skin, or a hollow log. A soft
  // noise strike (2-6 ms rise, never an impulse) excites four FIXED modal
  // resonators, then gentle saturation. No pitch envelope, no click, nothing
  // above 4.5 kHz: it sits in the low mids, under the other sounds.
  SynthDef(\hand, { |freq = 300, amp = 0.3, decay = 0.18, rise = 0.003, tone = 1600, slap = 0.25, drive = 2,
      pan = 0, sherman = 0.15, dverb = 0.2, udel = 0, kdel = 0, mute = 0|
    var ratios = \ratios.kr([1, 1.59, 2.14, 2.65]), gains = \gains.kr([1, 0.5, 0.3, 0.15]);
    var exc = LPF.ar(PinkNoise.ar, tone) * EnvGen.ar(Env([0, 1, 0], [rise, 0.02], [2, -4]));
    var modes = Mix.fill(4, { |k| Ringz.ar(exc, freq * ratios[k], decay * (1 - (k * 0.18))) * gains[k] }) * 0.02;
    var sig = modes + (LPF.ar(exc, tone * 0.8) * slap);
    sig = (sig * drive).tanh / drive.tanh;
    sig = HPF.ar(LPF.ar(sig, 4500), 150) * EnvGen.ar(Env.perc(0.001, decay * 1.6, 1, -4), doneAction: 2) * amp;
    sig = Pan2.ar(sig, pan);
    Out.ar(busDrums, sig * (1 - mute));
    Out.ar(busDverb, sig * dverb * (1 - mute));
    Out.ar(busUdel, sig * udel * (1 - mute));
    Out.ar(busSherman, sig * sherman);
    Out.ar(busKdel, sig * kdel);
  }),

  // GRIT (v13): noisy percussion, gravel and chains. White noise plus dense
  // crackle through a band-pass and a short comb tuned to the key, a soft
  // 4 ms rise, saturated, nothing above 5 kHz. Fixed tuning, no sweeps.
  SynthDef(\grit, { |freq = 200, amp = 0.2, decay = 0.15, lo = 500, hi = 3000, crackle = 0.5, comb = 0.5,
      pan = 0, sherman = 0.15, dverb = 0.25, udel = 0, mute = 0|
    var env = EnvGen.ar(Env([0, 1, 0], [0.004, decay], [2, -5]), doneAction: 2);
    var n = (WhiteNoise.ar * (1 - crackle)) + (Dust2.ar(2500) * 3 * crackle);
    var sig = LPF.ar(HPF.ar(n, lo), hi);
    sig = sig + (CombC.ar(sig, 0.05, 1 / freq, decay * 0.6) * comb * 0.5);
    sig = LPF.ar((sig * env * 2.5).tanh * 0.5, 5000) * amp;
    sig = Pan2.ar(sig, pan);
    Out.ar(busDrums, sig * (1 - mute));
    Out.ar(busDverb, sig * dverb * (1 - mute));
    Out.ar(busUdel, sig * udel * (1 - mute));
    Out.ar(busSherman, sig * sherman);
  }),

  // EXHALE (v13): a reversed noise breath into a phrase door. Noise through a
  // FIXED resonant band-pass (a harmonic of the root) rises slowly and is
  // cut dead on the beat. Wide, into the atmosphere bus and a little hall.
  SynthDef(\exhale, { |freq = 600, amp = 0.1, dur = 1, rq = 0.25, hall = 0.3, mute = 0|
    var env = EnvGen.ar(Env([0, 1, 0], [dur, 0.008], [5, -4]), doneAction: 2);
    var n = [PinkNoise.ar, PinkNoise.ar];
    var sig = (BPF.ar(n, freq, rq) * 2.5) + (LPF.ar(HPF.ar(n, 300), 2500) * 0.25);
    sig = HPF.ar((sig * 2).tanh * env * amp, 200);
    Out.ar(busAtmos, sig * (1 - mute));
    Out.ar(busHall, sig * hall * (1 - mute));
  }),

  // LPG (v14): a modular voice through a low-pass gate (a vactrol: quick
  // rise, slow natural fall). Phase modulation and a wavefolder give the
  // colour; the gate closes the filter and the level together, the soft
  // "bonk" of a modular ping. No resonance, no pitch envelope, so there is
  // no chirp or zap: only the timbre moves.
  SynthDef(\lpg, { |freq = 300, amp = 0.2, decay = 0.3, fm = 1.5, ratio = 2, fold = 0.3, bright = 0.6, pan = 0,
      sherman = 0.2, udel = 0.2, kdel = 0, dverb = 0.15, hall = 0.1, mute = 0|
    var vac = EnvGen.ar(Env([0, 1, 0], [0.005, decay], [2, -5]), doneAction: 2);
    var car = SinOsc.ar(freq, SinOsc.ar(freq * ratio) * fm * vac.squared);
    var sig = (car * (1 + (fold * 3))).fold2(1);
    sig = LPF.ar(sig, (vac.squared * bright.linexp(0, 1, 500, 4500)) + 150) * vac;
    sig = HPF.ar(LPF.ar(LPF.ar((sig * 1.4).tanh, 2800), 2800), 160) * amp;   // v15: no fold partials in the presence zone
    sig = Pan2.ar(sig, pan);
    Out.ar(busDrums, sig * (1 - mute));
    Out.ar(busDverb, sig * dverb * (1 - mute));
    Out.ar(busUdel, sig * udel * (1 - mute));
    Out.ar(busKdel, sig * kdel * (1 - mute));
    Out.ar(busSherman, sig * sherman * (1 - mute));
    Out.ar(busHall, sig * hall * (1 - mute));
  }),

  // BLEEP (v15): a tiny modular blip. A sine with a touch of phase
  // modulation through a very short low-pass gate; fixed notes, dark (below
  // 2.6 kHz), 4 ms rise: a blip, never a click or a zap.
  SynthDef(\bleep, { |freq = 800, amp = 0.1, decay = 0.06, fm = 0.8, pan = 0, udel = 0.3, kdel = 0, sherman = 0.1,
      dverb = 0.2, hall = 0.1, mute = 0|
    var vac = EnvGen.ar(Env([0, 1, 0], [0.004, decay], [2, -6]), doneAction: 2);
    var sig = SinOsc.ar(freq, SinOsc.ar(freq * 2) * fm * 0.6 * vac);
    sig = LPF.ar(LPF.ar(sig, 2600), 2600) * vac * amp;
    sig = Pan2.ar(sig, pan);
    Out.ar(busDrums, sig * (1 - mute));
    Out.ar(busDverb, sig * dverb * (1 - mute));
    Out.ar(busUdel, sig * udel * (1 - mute));
    Out.ar(busKdel, sig * kdel * (1 - mute));
    Out.ar(busSherman, sig * sherman * (1 - mute));
    Out.ar(busHall, sig * hall * (1 - mute));
  }),

  // ANCIENT PAD (v15): a distant temple choir and harmonium. Open fifths and
  // octaves on the root (4x, 6x, 8x, 12x: no thirds, nothing modern), each
  // voice three detuned pulses, through vowel formants that drift slowly
  // between "u", "o" and "a" (the timbre moves, the notes never do), a breath
  // of noise in the formants, a slow tremolo. Dark: nothing above 3 kHz.
  SynthDef(\pad, { |freq = 49, amp = 0.2, atk = 6, sus = 10, rel = 8, hall = 0.6, mute = 0|
    var env = EnvGen.kr(Env.linen(atk, sus, rel, 1, \sin), doneAction: 2);
    var notes = [4, 6, 8, 12] * freq, gains = [1, 0.8, 0.55, 0.3];
    var sig = { |ch|
      var src = Mix.fill(4, { |i| Mix(Pulse.ar(notes[i] * [0.996, 1, 1.0035] * (1 + (ch * 0.0015)), 0.4)) * gains[i] }) * 0.12;
      var v = LFNoise2.kr(0.04 + (ch * 0.01)).range(0, 1);
      var ex = src + PinkNoise.ar(0.12);
      (BPF.ar(ex, v.linexp(0, 1, 330, 700), 0.14) * 3) + (BPF.ar(ex, v.linexp(0, 1, 700, 1150), 0.12) * 2)
        + (BPF.ar(ex, 2400, 0.1) * 0.5) + (LPF.ar(src, 350) * 0.6)
    } ! 2;
    sig = [sig[0].value(0), sig[1].value(1)];
    sig = LPF.ar((sig * 1.2).tanh, 3000) * (1 + (SinOsc.kr(0.17, [0, 1.3]) * 0.12)) * env * amp;
    sig = HPF.ar(sig, 170);
    Out.ar(busAtmos, sig * (1 - mute));
    Out.ar(busHall, sig * hall * (1 - mute));
  }),

  // TEMPLE (v15): a dark field texture for a denser atmosphere. A distant
  // rumble (brown noise, breathing), a slow wind in the low mids, and far-off
  // creaks (sparse resonant knocks on the root's 9th, 11th and 13th
  // harmonics). Nothing above 3 kHz: dense, never bright.
  SynthDef(\temple, { |freq = 49, amp = 0.2, atk = 8, sus = 30, rel = 8, creak = 0.5, mute = 0|
    var env = EnvGen.kr(Env.linen(atk, sus, rel, 1, \sin), doneAction: 2);
    var rumble = LPF.ar([BrownNoise.ar, BrownNoise.ar], 550) * LFNoise2.kr(0.08).range(0.4, 1) * 0.6;
    var wind = BPF.ar([PinkNoise.ar, PinkNoise.ar], LFNoise2.kr([0.04, 0.035]).exprange(400, 1400), 0.3)
      * LFNoise2.kr(0.06).range(0.2, 1) * 0.9;
    var ck = Ringz.ar(Dust.ar([0.35, 0.3] * creak), freq * [9, 13], 1.1) + Ringz.ar(Dust.ar([0.2, 0.25] * creak), freq * [11, 6], 0.8);
    var sig = rumble + wind + (LPF.ar(ck, 2500) * 0.06);
    sig = HPF.ar(HPF.ar(LPF.ar((sig * 1.5).tanh, 3000), 260), 260) * env * amp;   // above the kick's body
    Out.ar(busAtmos, sig * (1 - mute));
    Out.ar(busHall, sig * 0.5 * (1 - mute));
  }),

  // MACHINE ROOM (v14): a distant industrial room tone for the breaks.
  // Brown noise through a tube (a comb tuned to the key) and a slowly
  // drifting low-pass, breathing. Filters move, nothing changes pitch.
  SynthDef(\machine, { |freq = 98, amp = 0.3, atk = 4, sus = 10, rel = 6, mute = 0|
    var env = EnvGen.kr(Env.linen(atk, sus, rel, 1, \sin), doneAction: 2);
    var n = [BrownNoise.ar, BrownNoise.ar];
    var sig = CombC.ar(LPF.ar(n, 1200), 0.05, 1 / freq, 1.2) * 0.08;
    sig = sig + (LPF.ar(n, LFNoise2.kr([0.06, 0.05]).exprange(250, 1100)) * 0.3);
    sig = (sig * 1.5).tanh * LFNoise2.kr(0.15).range(0.55, 1.0);
    sig = HPF.ar(sig, 140) * env * amp;
    Out.ar(busAtmos, sig * (1 - mute));
    Out.ar(busHall, sig * 0.4 * (1 - mute));
  }),

  // Tape echo (used twice: bass, and the kick's FM layer): left/right
  // times differ, cross-fed, every repeat thinner, darker and a little more
  // saturated, with slight wobble. Can feed the Sherman too.
  SynthDef(\echo, { |in = 22, tL = 0.4, tR = 0.53, fb = 0.5, mix = 0.8, hp = 250, lp = 2800, toSherman = 0, mute = 0, room = 0.12, duckAmt = 0.35|
    var input = In.ar(in, 2);
    var duck = 1 - (In.kr(ctlDuck).clip(0, 1) * duckAmt);   // ducked delays: the repeats breathe around the kick
    var local = LocalIn.ar(2);
    var dl = DelayC.ar(input + local, 2, [tL, tR] + (LFNoise2.kr(0.5 ! 2) * 0.002));
    LocalOut.ar((LPF.ar(HPF.ar(dl.reverse, hp), lp) * fb * 1.3).tanh / 1.3);
    dl = dl * duck;
    Out.ar(busSpace, dl * mix * (1 - mute));
    Out.ar(busRoom, dl * mix * room * (1 - mute));
    Out.ar(busHall, dl * 0.25 * (1 - mute));
    Out.ar(busSherman, dl * toSherman);
  }),

  // SHERMAN-STYLE FILTERBANK: input overdriven, then a resonant low-pass
  // pushed near self-oscillation and saturated again, then a band-pass
  // above it in series. Cutoff = envelope follower (every hit squelches) +
  // LFO over 7 sixteenths (polymeter against the bar) + a random step on
  // every 16th (smoothed). "swell" is the hand on the knobs: resonance up,
  // drive up, level up (the cutoff does not sweep, so nothing slides in pitch).
  SynthDef(\sherman, { |base = 250, reso = 0.75, envAmt = 0.3, lfoAmt = 0.15, drive = 4, mix = 1, step = 0.134,
      swell = 0, swellTime = 1, mute = 0, room = 0.18, duckAmt = 0.25|
    var in = In.ar(busSherman, 2);
    var duck = 1 - (In.kr(ctlDuck).clip(0, 1) * duckAmt);
    var sw = VarLag.kr(swell, swellTime, 4);
    var env = Amplitude.kr(in.sum, 0.002, 0.12);
    var lfo = SinOsc.kr(1 / (step * 7));
    var sh = Lag.kr(TRand.kr(-1, 1, Impulse.kr(1 / step)), 0.01);
    var cut = (base.lag(2) * (2 ** ((envAmt * (env * 6).clip(0, 1) * 4) + (lfoAmt * lfo * 1.5) + (sh * 0.6)))).clip(40, 12000);
    var rq = (reso.lag(2).linlin(0, 1, 0.7, 0.05) * (1 - (sw * 0.6))).max(0.02);
    var x = (in * drive.lag(2) * (1 + (sw * 2))).tanh;
    var y1 = (RLPF.ar(x, cut, rq) * 1.6).tanh;
    var y2 = BPF.ar(y1, (cut * 2.4).clip(40, 14000), rq * 1.5) * 2;
    var sig = LPF.ar(LeakDC.ar(((y1 * 0.6) + (y2 * 0.9)).tanh), 5500) * 0.5 * mix * (1 + (sw * 1.5));   // v14: darker (presence spikes)
    sig = [sig[0], (sig[1] * 0.6) + (DelayN.ar(sig[1], 0.02, 0.011) * 0.5)];   // a little width
    sig = HPF.ar(sig, 150);   // the Sherman is a mid-range effect: no boom from it
    sig = BPeakEQ.ar(sig, 850, 1.0, -2.5);   // v14: it stacked on the bass at 850 Hz
    sig = sig * duck;
    Out.ar(busSpace, sig * (1 - mute));
    Out.ar(busRoom, sig * room * (1 - mute));
    Out.ar(busHall, sig * 0.1 * (1 - mute));
  }),

  // PARALLEL SATURATION SEND (the Decapitator / Hamburger / Sherman logic):
  // what's sent here is pushed into saturation and mixed back underneath
  // the clean signal.
  //  * "decap": lows pushed (low shelf) into an asymmetric tube-like drive,
  //    then a dark tone filter: thick, warm, low saturation
  //  * "burger": a harder diode-style clip on the mids, contoured
  //  * a share continues into the Sherman for the resonant squelch
  SynthDef(\satbus, { |drive = 3, burger = 0.3, tone = 2200, mix = 0.6, toSherman = 0.25, room = 0.1, kroom = 0.15, duckAmt = 0.6, kgain = 1.5|
    // channels 0-1: the bass layers; 2-3: the kick. Same processing, but each
    // pair saturates on its own, so the kick never squeezes the bass
    var in = In.ar(busSat, 2) ++ In.ar(busKsat, 2);
    var x = BLowShelf.ar(in, 150, 1, 2);   // gentle low push (a big one added boom and limiter pumping)
    var decap = ((x * drive.lag(2)) + 0.25).tanh - 0.25.tanh;
    var burg = (BPF.ar(in, 900, 1.5) * drive.lag(2) * 3).clip2(0.5) * 2;
    var sig, bass, kick;
    decap = LPF.ar(decap, tone.lag(2));
    burg = LPF.ar(HPF.ar(burg, 300), 3000);
    sig = LeakDC.ar(decap + (burg * burger)) * mix;
    sig = BPeakEQ.ar(sig, 100, 0.8, -4.5);   // warmth without boom in 70-150 Hz (reference match)
    sig = Compander.ar(sig, sig, 0.3, 1, 0.5, 0.005, 0.1);
    bass = sig[0..1] * (1 - (In.kr(ctlDuck).clip(0, 1) * duckAmt));   // the bass grit ducks under the kick
    kick = sig[2..3] * kgain;   // the kick's weight comes from its own harmonics, not the shared tape
    Out.ar(busSpace, bass);
    Out.ar(busRoom, (bass * room) + (kick * kroom));
    Out.ar(busKickM, kick);
    Out.ar(busSherman, (bass + kick) * toSherman);
  }),

  // DRUM BUS: one compressor across kick, hats and percussion (glue), then
  // gentle asymmetric saturation, then into the shared space.
  SynthDef(\drumbus, { |thresh = 0.3, sat = 1.25, room = 0.22, duckAmt = 0.3|
    var in = In.ar(busDrums, 2);
    var kick = In.ar(busKick, 2);
    // the percussion is glued by its OWN level (the kick no longer pumps it)
    var sig = Compander.ar(in, in, thresh, 1, 1/3, 0.005, 0.12);
    sig = ((sig * sat) + 0.1).tanh - 0.1.tanh;
    sig = sig / (sat * (1 - 0.1.tanh.squared));
    sig = BPeakEQ.ar(sig, 200, 1.0, -1.5);   // drum bodies out of the low-mids
    kick = BPeakEQ.ar(kick, 100, 1.0, -1.5);   // reference match: kick weight stays in the sub, not the boom
    kick = BPeakEQ.ar(kick, 200, 1.0, -1.5);
    sig = sig * (1 - (In.kr(ctlDuck).clip(0, 1) * duckAmt));   // percussion breathes around the kick
    Out.ar(busSpace, sig);
    Out.ar(busRoom, sig * room);
    // the kick travels to the master on its own wire: it shares the room and
    // the tape's character, but it never drives the saturators the rest of
    // the mix goes through (that would turn everything down on every hit)
    Out.ar(busKickM, kick);
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
    sig = HPF.ar(sig, 90) * env * amp;
    Out.ar(busAtmos, sig * (1 - mute));
    Out.ar(busHall, sig * 0.3 * (1 - mute));
    Out.ar(busGrain, sig * 0.2);
  }),

  // Metal: 16 inharmonic modes (bowl, gong, bell, plate, bar, sheet), struck
  // (0), scraped (1) or bowed (2), optionally bent. "hard" = mallet: soft
  // felt to steel (tamed so it never gets harsh). "dist" = distance.
  SynthDef(\metal, { |freq = 200, amp = 0.2, dur = 4, mode = 0, bend = 1, drive = 1, pan = 0,
      dist = 0.3, hard = 0.5, hall = 0.4, gsend = 0.4, out = 34, dverb = 0, udel = 0, mute = 0|
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
    Out.ar(out, sig * (1 - mute));
    Out.ar(busHall, sig * (hall + (dist * 0.4)) * (1 - mute));
    Out.ar(busDverb, sig * dverb * (1 - mute));
    Out.ar(busUdel, sig * udel * (1 - mute));
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
    sig = Pan2.ar(LPF.ar(HPF.ar(sig, 90), 5000), (pan + (LFNoise2.kr(0.03) * 0.45)).clip(-1, 1)) * env * amp;
    Out.ar(busAtmos, sig * (1 - mute));
    Out.ar(busHall, sig * hall * (1 - mute));
    Out.ar(busGrain, sig * gsend);
  }),

  // Frame drum / ritual tom: a skin (membrane modes) whose pitch settles
  // after the hit, plus the low thump of the palm.
  SynthDef(\frame, { |freq = 90, amp = 0.4, decay = 0.6, hard = 0.3, pan = 0, dverb = 0.18, pdrop = 1.0, mute = 0|
    var exc = LPF.ar(Decay2.ar(Impulse.ar(0), 0.002, 0.012) * (1 + PinkNoise.ar(0.4)), hard.linexp(0, 1, 800, 4500));   // v14: softer strike
    var penv = EnvGen.kr(Env([pdrop, 1], [0.05], -4));
    var sig = DynKlank.ar(`[[1, 1.593, 2.136, 2.296, 2.653, 2.918, 3.156],
      [1, 0.5, 0.4, 0.3, 0.25, 0.2, 0.15] * 0.1, [1, 0.6, 0.5, 0.4, 0.3, 0.25, 0.2] * decay], exc, freq * penv);
    var thump = SinOsc.ar(freq * EnvGen.ar(Env([1.15, 1], [0.015]))) * EnvGen.ar(Env.perc(0.002, decay * 0.5)) * 0.5;
    sig = HPF.ar(((sig + thump) * 1.5).tanh, 170);   // v14: out of the kick's body
    DetectSilence.ar(sig, 0.0002, 0.2, doneAction: 2);
    sig = Pan2.ar(sig, pan) * amp;
    Out.ar(busDrums, sig * (1 - mute));
    Out.ar(busDverb, sig * dverb * (1 - mute));
  }),

  // Shaker / seed rattle.
  SynthDef(\shaker, { |amp = 0.1, pan = 0, mute = 0|
    var env = EnvGen.ar(Env([0, 1, 0], [0.004, 0.07], [2, -5]), doneAction: 2);
    var sig = LPF.ar(BPF.ar(WhiteNoise.ar, Rand(3500, 5500), 1.0), 7000) * env * amp;
    Out.ar(busDrums, Pan2.ar(sig, pan) * (1 - mute));
    Out.ar(busDverb, Pan2.ar(sig, pan) * 0.2 * (1 - mute));
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
    Out.ar(busAtmos, sig * (1 - mute));
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
    Out.ar(busAtmos, sig * (1 - mute));
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
    Out.ar(busAtmos, sig * 0.6);
    Out.ar(busHall, sig * 0.6);
  }),

  // DRUM PLATE: short and tight, pre-delayed a few ms, high-passed so the
  // kick's sub never gets reverb, low-passed so it never gets splashy.
  SynthDef(\dverb, { |mix = 1|
    var in = HPF.ar(In.ar(busDverb, 2), 300);
    var sig = JPverb.ar(DelayN.ar(in, 0.02, 0.008), 0.9, 0.45, 0.6, 0.707, 0.1, 1.5, 1, 1, 0.7, 400, 7000);
    sig = sig * (1 - (In.kr(ctlDuck).clip(0, 1) * 0.3));   // the plate ducks: the kick stays dry and clear
    Out.ar(busSpace, LPF.ar(sig, 8000) * mix);
  }),

  // ATMOS BUS: everything atmospheric. EQ clean-up (high-pass, a dip where
  // the low-mids get muddy, a softer top), slow gentle compression, and a
  // slight duck under the kick so the drums breathe through the ambience.
  // It also MOVES, slowly: the stereo image drifts, the tone drifts (low and
  // high shelves wander a few dB over tens of seconds) and the level
  // breathes. Nothing changes pitch.
  SynthDef(\atmosbus, { |duckAmt = 0.3, room = 0.25|
    var in = In.ar(busAtmos, 2);
    var duck = 1 - (In.kr(ctlDuck).clip(0, 1) * duckAmt);
    var sig = HPF.ar(in, 70);
    sig = BPeakEQ.ar(sig, 350, 1.0, -2.5);
    sig = BLowShelf.ar(sig, 200, 1, 2);    // weight
    sig = BHiShelf.ar(sig, 6000, 1, -3);
    sig = Compander.ar(sig, sig, 0.2, 1, 0.5, 0.03, 0.4);
    // slow image drift as a BALANCE move (a rotator flips polarity at wide
    // angles, which hollows the atmosphere out in mono)
    sig = Balance2.ar(sig[0], sig[1], LFNoise2.kr(0.025) * 0.35);
    sig = BLowShelf.ar(sig, 250, 1, LFNoise2.kr(0.04).range(-3, 1.5));
    sig = BHiShelf.ar(sig, 3000, 1, LFNoise2.kr(0.035).range(-4, 1.5));
    sig = sig * LFNoise2.kr(0.05).range(0.8, 1.05);
    Out.ar(busSpace, sig * duck);
    Out.ar(busRoom, sig * duck * room);
  }),

  // BASS BUS: compression, distortion, mono below 150 Hz, and a heavy duck
  // under every kick so kick and bass punch through each other.
  SynthDef(\bassbus, { |duckAmt = 0.95, sat = 2.0, midSat = 1.2, room = 0.1|
    var in = In.ar(busLow, 2);
    var duckC = In.kr(ctlDuck).clip(0, 1);
    var sig = HPF.ar(HPF.ar(in, 55), 55);  // the kick owns everything below 55 Hz
    var mid, side, lo, hi;
    sig = Compander.ar(sig, sig, 0.3, 1, 1/3, 0.005, 0.15);
    sig = BPeakEQ.ar(sig, 90, 0.8, -1.5);    // reference match: less boom in 60-120 (v13: less dip, nine-reference median)
    sig = BPeakEQ.ar(sig, 180, 1.0, -1);
    sig = BPeakEQ.ar(sig, 135, 1.2, -2);     // v14: the kick's body (measured masking)
    sig = BPeakEQ.ar(sig, 280, 1.0, -1.5);   // keep the low-mids clean
    sig = sig + ((LPF.ar(HPF.ar(sig, 150), 1200) * 2.5).tanh * 0.18);  // parallel mid grit: heard on phones
    sig = ((sig * sat) + 0.15).tanh - 0.15.tanh;
    sig = sig / (sat * (1 - 0.15.tanh.squared));
    // frequency-split sidechain: the lows get out of the kick's way completely,
    // the mids a little less, so the texture stays continuous without pumping
    lo = LPF.ar(LPF.ar(sig, 220), 220);   // v14: 150-220 Hz ducks fully too (it masked the kick's body)
    hi = sig - lo;
    sig = (lo * (1 - (duckC * duckAmt))) + (hi * (1 - (duckC * 0.75)));
    mid = (sig[0] + sig[1]) * 0.5;
    side = HPF.ar((sig[0] - sig[1]) * 0.5, 160);
    sig = [mid + side, mid - side];
    // WIDE MID SATURATION (where mental tekno lives): the 250 Hz - 3.5 kHz band
    // is saturated separately left and right (different drive, micro-delays),
    // then mixed in parallel: dense, aggressive, wide mids; lows stay mono.
    {
      var m = LPF.ar(HPF.ar(sig.sum * 0.5, 250), 3500);
      var l = ((DelayN.ar(m, 0.01, 0.003) * 2.3) + 0.2).tanh - 0.2.tanh;
      var r = ((DelayN.ar(m, 0.01, 0.005) * 2.6) + 0.15).tanh - 0.15.tanh;
      sig = sig + (LPF.ar(LPF.ar([l, r], 3400), 3400) * midSat);   // smooth, not fizzy
    }.value;
    Out.ar(busSpace, sig * 1.25);
    Out.ar(busRoom, sig * 1.25 * room);
  }),

  // THE SHARED ROOM: every sound passes through the same short early
  // reflections and the same console saturation: one physical space.
  SynthDef(\space, { |roomMix = 1, sat = 1.15|
    var in = In.ar(busSpace, 2);
    // the room is a SEND/RETURN: every group sends its own amount (atmosphere
    // most, drums some, bass little, the sub nothing), high-passed at 200 Hz
    var room = JPverb.ar(HPF.ar(HPF.ar(In.ar(busRoom, 2), 200), 200), 0.55, 0.5, 0.5, 0.707, 0.1, 1.0, 1, 1, 0.8, 400, 7000);
    var sig = in + (room * roomMix);
    // saturate only above 120 Hz: the kick's sub must not squeeze what rides on top
    {
      var lo = LPF.ar(LPF.ar(sig, 120), 120), hi = sig - lo;
      hi = ((hi * sat) + 0.08).tanh - 0.08.tanh;
      hi = hi / (sat * (1 - 0.08.tanh.squared));
      sig = lo + hi;
    }.value;
    Out.ar(busMix, sig);
  }),

  // Master: long hall (pre-delayed, widened), then the tape, a de-harsh EQ
  // for the 3-4 kHz zone, glue and a limiter.
  SynthDef(\master, { |drive = 1.4, bias = 0.12, wow = 0.3, hiss = 0.002, hallMix = 0.8, gain = 1, clip = 1.5, loClip = 1.6, bodyMix = 0.16, exMix = 0.45,
      fade = 1, fadeTime = 3|
    var sig = In.ar(busMix, 2);
    var kick = In.ar(busKickM, 2);
    var hall = JPverb.ar(DelayN.ar(HPF.ar(In.ar(busHall, 2), 160), 0.05, 0.045),
      8, 0.4, 2.6, 0.707, 0.3, 0.5, 1.0, 0.8, 0.5, 250, 4500);
    var d = drive.lag(5), w = wow.lag(5), mid, side, lo, md, hi, body, harsh;
    mid = (hall[0] + hall[1]) * 0.5;
    side = (hall[0] - hall[1]) * 0.5 * 1.15;   // wide, but still safe in mono
    hall = [mid + side, mid - side];
    sig = sig + (hall * hallMix.lag(4));
    sig = HPF.ar(sig, 24);
    // everything below 160 Hz in mono: the kick, sub and bass hit the center as one
    mid = (sig[0] + sig[1]) * 0.5;
    side = HPF.ar(HPF.ar((sig[0] - sig[1]) * 0.5, 160), 160) * 0.85;   // v14: mids a little narrower (references' width)
    sig = [mid + side, mid - side];
    sig = sig + (HPF.ar(sig, 3000) * 0.5);
    {
      var tlo = LPF.ar(LPF.ar(sig, 120), 120), thi = sig - tlo;   // the tape saturates above 120 Hz
      thi = ((thi * d) + bias).tanh - bias.tanh;
      thi = thi / (d * (1 - bias.tanh.squared));
      sig = tlo + thi;
    }.value;
    // the kick through its own tape stage: same drive and bias, no intermodulation
    // with the rest of the mix
    {
      var k = HPF.ar(kick, 24), klo, khi;
      k = k + (HPF.ar(k, 3000) * 0.3);   // v14: gentler on the kick (presence spikes)
      klo = LPF.ar(LPF.ar(k, 120), 120); khi = k - klo;
      khi = ((khi * d) + bias).tanh - bias.tanh;
      khi = khi / (d * (1 - bias.tanh.squared));
      sig = sig + klo + khi;
    }.value;
    sig = BPeakEQ.ar(sig, 600, 0.7, 2.5);    // reference match: the mids carry this music
    sig = BPeakEQ.ar(sig, 1200, 0.7, 6.5);     // mental tekno lives in the mids
    sig = BPeakEQ.ar(sig, 1900, 0.8, 5.5);     // v13: nine-reference match (800 Hz-2.5 kHz was 4 dB short)
    sig = BPeakEQ.ar(sig, 3500, 0.6, 4.5);   // reference match: presence (spikes are caught by the de-harsher)
    // RESONANCE CONTROL: narrow cuts only where root-tuned layers stack on the
    // same frequency (measured: 4x root sticks out +11 dB). The resonant
    // character everywhere else stays.
    // LOW-MID BODY: parallel compression of 120-400 Hz (what a phone can play),
    // mixed under the dry signal: a dense, continuous low end, not one ringing note.
    body = LPF.ar(LPF.ar(HPF.ar(sig, 120), 400), 400);
    body = Compander.ar(body, body, 0.08, 1, 1/6, 0.003, 0.08) * 3;
    sig = sig + (body * bodyMix);
    \resoF.kr([196, 294, 392, 441, 686, 1960, 2842, 3205]).do { |f, i|
      sig = BPeakEQ.ar(sig, f, 0.15, \resoG.kr([-10, -5, -4, -3.5, -3, -4, -4, -4])[i]);
    };
    // DYNAMIC DE-HARSH: the 2.6 kHz zone is pulled down only while it jumps
    // out above the rest of the mix (bells, metal partials), up to 9 dB.
    harsh = Amplitude.kr(BPF.ar(sig.sum * 0.5, 2600, 0.6), 0.005, 0.1).ampdb
      - Amplitude.kr(sig.sum * 0.5, 0.005, 0.1).ampdb;
    sig = BPeakEQ.ar(sig, 2600, 0.6, ((harsh + 17).clip(0, 9)).neg.lag(0.02));
    sig = BHiShelf.ar(sig, 9000, 1, -5.5);       // reference match: less air (above the presence zone)
    // PRESENCE EXCITER: the 1.5-6 kHz band saturated in parallel; it adds the
    // dense upper harmonics distorted mids have in this music (not EQ hiss)
    {
      var ex = LPF.ar(HPF.ar(sig, 1500), 6000);
      ex = HPF.ar(LPF.ar((ex * 4).tanh, 6000), 2200);
      sig = sig + (ex * exMix);
    }.value;
    sig = BPeakEQ.ar(sig, 320, 1.0, -2.5);   // make room in the low-mids
    sig = BPeakEQ.ar(sig, 250, 0.6, -1.5);   // broad: weight in the sub, not in the mud

    sig = DelayC.ar(sig, 0.03, 0.006 + (LFNoise2.kr(0.7) * 0.0012 * w) + (SinOsc.kr(5.8) * 0.00015 * w));
    sig = sig + HPF.ar(PinkNoise.ar(hiss.lag(5) ! 2), 1500);
    sig = LPF.ar(sig, 12500);
    sig = LeakDC.ar(sig);
    // 3-band (multiband / spectral) compression: lows, mids and highs are
    // controlled separately, so a loud kick doesn't push the cymbals down
    // and a bright hat burst doesn't squash the low end.
    lo = LPF.ar(LPF.ar(sig, 150), 150);
    hi = HPF.ar(HPF.ar(sig, 3500), 3500);
    md = sig - lo - hi;
    lo = Compander.ar(lo, lo, 0.35, 1, 1/2, 0.02, 0.15);
    md = Compander.ar(md, md, 0.6, 1, 1/2, 0.008, 0.15);    // catches spikes, not every kick
    hi = Compander.ar(hi, hi, 0.2, 1, 1/2.5, 0.0015, 0.08);    // rounds off spiky highs, not every kick
    lo = (lo * loClip).tanh / loClip;        // kick clipping: unity for quiet lows, rounds only the kick's peaks
    sig = lo + md + hi;
    sig = sig * gain * Lag.kr(fade, fadeTime);
    sig = (sig * clip).tanh / clip.tanh;      // soft clipper: rounds the peaks before the limiter
    sig = Limiter.ar(sig, 0.89, 0.01);   // ceiling -1 dBFS
    Out.ar(0, sig);
  })
];

// ---------------------------------------------------------------------------
// HELPERS
// ---------------------------------------------------------------------------
score.add([0.0, ['/g_new', 1, 0, 0]]);
defs.do { |d| score.add([0.0, ['/d_recv', d.asBytes]]) };

// MUTE=layer1,layer2 silences layers (their sidechain/duck signals still run)
on = { |layer|
  var muted = ("MUTE".getenv ? "").split($,).includesEqual(layer);
  if((solo.isNil or: { solo == layer }) and: { muted.not }) { 0 } { 1 }
};
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
    \cymbal, { Array.fill(16, { |k| ((k + 1) ** 1.32) * rrand(0.98, 1.02) }) },
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
zapP = euclid.(1, 12, 12.rand);     // zaps: 1 in 12 (unused; kept so the seeded patterns stay the same)
gritP = euclid.(3, 11, 0);          // grit: 3 in 11 (v13)
boomP = Array.fill(20, 0).put(6, 1).put(13, 1);   // booms: 20-step cycle (5 beats) between the kicks
growlP = euclid.(2, 11, 11.rand);   // growls: 2 in 11
lowP = euclid.(3, 13, 13.rand);     // low growls: 3 in 13
ksat = [0.6, 1.0, 0.75, 1.2, 0.8];  // kick saturation send per step: 5-cycle
kdrive = [1.4, 2.2, 1.8, 2.8, 1.6];  // kick distortion per step: 5-cycle against 4 kicks a bar
ksend = [0.35, 0.9, 0.5, 1.0, 0.4, 0.8, 0.3];  // kick echo send per step: 7-cycle
kdistC = [0.7, 1.0, 0.85, 1.3, 0.75];            // kick distortion layer per step: 5-cycle
kpA = [1, 0, 0, 1, 0, 0, 1, 0, 1, 0];              // kick 3-3-2-2 (10 steps) in the rite
kpB = [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0];  // kick 4-4-3-3 (14 steps) in the conjuration
hatA = euclid.(5, 13, 13.rand);                  // closed hats 5-in-13
hatO = euclid.(3, 11, 11.rand);                  // open hats 3-in-11
hatG = euclid.(3, 7, 7.rand);                    // ghost ticks 3-in-7
hatTone = [0.3, 0.6, 0.45, 0.8, 0.5, 0.7, 0.35]; // hat colour per step: 7-cycle
kpC = kpA ++ kpB;                                 // kick 3-3-2-2-4-4-3-3 (24 steps) in the peak
seqAP = euclid.(5, 13, 13.rand);                  // modular sequence A: 5 in 13
seqBP = euclid.(4, 11, 11.rand);                  // modular sequence B: 4 in 11
seqCP = euclid.(3, 7, 7.rand);                    // modular sequence C: 3 in 7
seqAN = Array.fill(3, { pick.(260, 520) });       // fixed notes from the root's harmonic field
seqBN = Array.fill(2, { pick.(520, 1100) });
seqCN = Array.fill(2, { pick.(300, 600) });
bleepP = euclid.(3, 19, 19.rand);                // bleeps: 3 in 19
bleepQ = euclid.(2, 23, 23.rand);                // and 2 in 23
bleepN = Array.fill(5, { pick.(600, 1800) });    // fixed notes

// v15 ARRANGEMENT (4:30): which bars each element plays in ([from, to) in bars).
// Elements rotate phrase by phrase: a few change every 8 bars, so the groove
// keeps moving and the mix never gets crowded.
ranges = (
  frame: [[8, 32], [52, 60], [68, 76], [100, 108]],
  tom: [[20, 42], [60, 74], [88, 112]],
  shaker: [[24, 43], [56, 75], [84, 112]],
  tingsha: [[10, 28], [68, 76]],
  anvil: [[28, 40], [92, 104]],
  clank: [[52, 75], [84, 112]],
  ticks: [[52, 64], [84, 92], [100, 108]],
  hand3: [[34, 40], [64, 72]],
  pulse5: [[68, 75], [88, 96]],
  sparse: [[22, 43], [60, 75], [84, 112]],
  grit: [[24, 43], [52, 75], [84, 112]],
  openhat: [[28, 43], [52, 75], [84, 112]],
  ghost: [[52, 75], [84, 112]],
  boom: [[14, 43], [52, 75], [84, 116]],
  growl: [[24, 43], [52, 75], [84, 108]],
  lowg: [[32, 43], [60, 75], [76, 83], [88, 108]],
  seqA: [[28, 52], [100, 116]],
  seqB: [[56, 75], [76, 83]],
  seqC: [[84, 100]],
  bleep: [[12, 43], [46, 52], [60, 75], [78, 84], [90, 112], [114, 122]]
);
inR = { |el, bar| ranges[el].any { |r| (bar >= r[0]) and: { bar < r[1] } } };

// ---------------------------------------------------------------------------
// FX CHAIN (created first, at the tail, so every source runs before it)
// ---------------------------------------------------------------------------
// order: screech echo -> kick echo -> Sherman -> grains -> drum plate -> drum bus
//        -> atmos bus -> shared room -> master
echo = Synth.basicNew(\echo, server);
score.add([0.01, echo.newMsg(Group.basicNew(server, 1),
  [\in, busSdel, \tL, 3 * stepDur, \tR, 4 * stepDur, \fb, 0.3, \mix, 0.4, \hp, 300, \lp, 4000,
   \toSherman, 0.2, \mute, on.("screech")], \addToTail)]);
kdel = Synth.basicNew(\echo, server);
score.add([0.0102, kdel.newMsg(Group.basicNew(server, 1),
  [\in, busKdel, \tL, 3 * stepDur, \tR, 6 * stepDur, \fb, 0.35, \mix, 0.7, \hp, 180, \lp, 4000,
   \toSherman, 0.4, \mute, on.("kick")], \addToTail)]);
udel = Synth.basicNew(\echo, server);   // UNSYNCED delay: 311 ms / 467 ms, floats off the grid
score.add([0.0103, udel.newMsg(Group.basicNew(server, 1),
  [\in, busUdel, \tL, 0.311, \tR, 0.467, \fb, 0.25, \mix, 0.35, \hp, 400, \lp, 5000, \toSherman, 0], \addToTail)]);
wdel = Synth.basicNew(\echo, server);   // screech echo: half note left, dotted half right
score.add([0.01031, wdel.newMsg(Group.basicNew(server, 1),
  [\in, busWdel, \tL, 8 * stepDur, \tR, 12 * stepDur, \fb, 0.45, \mix, 0.8, \hp, 160, \lp, 2200,
   \toSherman, 0, \mute, on.("screech")], \addToTail)]);
whd = Synth.basicNew(\echo, server);    // whale echo: long and unsynced, dark repeats into the hall
score.add([0.01032, whd.newMsg(Group.basicNew(server, 1),
  [\in, busWhd, \tL, 0.83, \tR, 1.27, \fb, 0.55, \mix, 0.7, \hp, 150, \lp, 1800,
   \toSherman, 0, \mute, on.("whales")], \addToTail)]);
satbus = Synth.basicNew(\satbus, server);
score.add([0.01035, satbus.newMsg(Group.basicNew(server, 1), [\drive, 3, \burger, 0.3, \mix, 0.26], \addToTail)]);
sherman = Synth.basicNew(\sherman, server);
score.add([0.0104, sherman.newMsg(Group.basicNew(server, 1),
  [\step, stepDur, \base, 250, \reso, 0.8, \drive, 4, \mix, 0.5, \mute, on.("sherman")], \addToTail)]);
grainer = Synth.basicNew(\grainer, server);
score.add([0.0106, grainer.newMsg(Group.basicNew(server, 1),
  [\density, 2, \gdur, 0.3, \amp, 1.4, \spread, 0.05, \rev, 0.3, \mute, on.("grains")], \addToTail)]);
dverb = Synth.basicNew(\dverb, server);
score.add([0.0108, dverb.newMsg(Group.basicNew(server, 1), [], \addToTail)]);
drumbus = Synth.basicNew(\drumbus, server);
score.add([0.011, drumbus.newMsg(Group.basicNew(server, 1), [], \addToTail)]);
atmosbus = Synth.basicNew(\atmosbus, server);
score.add([0.0112, atmosbus.newMsg(Group.basicNew(server, 1), [], \addToTail)]);
bassbus = Synth.basicNew(\bassbus, server);
score.add([0.0114, bassbus.newMsg(Group.basicNew(server, 1), [], \addToTail)]);
space = Synth.basicNew(\space, server);
score.add([0.012, space.newMsg(Group.basicNew(server, 1), [], \addToTail)]);
master = Synth.basicNew(\master, server);
score.add([0.013, master.newMsg(Group.basicNew(server, 1), [\gain, 0.86, \clip, 1.2,
  \resoF, root * [4, 6, 8, 9, 14, 40, 58, 65.4]], \addToTail)]);

// ---------------------------------------------------------------------------
// FORM (v15, 4:30): sections in bars
// ---------------------------------------------------------------------------
//   0-8 invocation · 8-20 procession · 20-44 the rite · 44-52 the void (break)
//   52-76 conjuration · 76-84 the descent (break) · 84-112 the peak
//   112-124 closing. Breaks drop the kick and the sub and keep the textures
//   (as the references do). Phrases are 8 bars counted from bar 4, so every
//   section starts on a phrase. Elements ROTATE phrase by phrase (`ranges`),
//   so a few things change every 8 bars and the mix never gets crowded.

// ---------------------------------------------------------------------------
// LONG GESTURES
// ---------------------------------------------------------------------------
// gong: bowed swell to open, struck at the doors of each section
add.(0.05, \metal, [\freq, pick.(55, 110), \amp, 0.7, \dur, 14, \mode, 2, \drive, 1, \pan, 0, \dist, 0.4,
  \hall, 0.5] ++ metalSpec.(\gong, 4), "metal");
[[20, 0.45], [44, 0.3], [52, 0.45], [84, 0.45], [112, 0.4]].do { |ga|
  var b = ga[0];
  add.(at.(b), \metal, [\freq, pick.(55, 110), \amp, ga[1], \dur, 0.5, \mode, 0, \hard, 0.15, \pan, rrand(-0.2, 0.2),
    \dist, 0.35, \hall, 0.6] ++ metalSpec.(\gong, 4), "metal");
};
[40, 76].do { |b|
  add.(at.(b), \metal, [\freq, pick.(55, 110), \amp, 0.55, \dur, 4 * 16 * stepDur, \mode, 2, \pan, 0, \dist, 0.3]
    ++ metalSpec.(\gong, 3), "metal");
};

// singing bowls through the opening, the void, the descent and the closing
[[1, 12], [5, 14], [44.2, 10], [77, 10], [116, 12]].do { |p|
  add.(at.(p[0]), \metal, [\freq, pick.(180, 520), \amp, 0.45, \dur, p[1], \mode, 2, \pan, rrand(-0.7, 0.7),
    \dist, 0.5] ++ metalSpec.(\bowl, 3), "metal");
};

// the chant: voices enter one by one; a full choir in the void
add.(at.(0, 4), \chant, [\freq, root * 2, \amp, 0.45, \atk, 10, \sus, 30, \rel, 12, \overtone, 8, \pan, -0.2], "chant");
add.(at.(4), \chant, [\freq, root * 3, \amp, 0.30, \atk, 10, \sus, 26, \rel, 12, \overtone, 6, \pan, 0.3], "chant");
add.(at.(16), \chant, [\freq, root * 2.004, \amp, 0.35, \atk, 8, \sus, 40, \rel, 10, \overtone, 9, \pan, 0.1], "chant");
[[2, 0.27, -0.4, 8], [3, 0.2, 0.4, 6], [4, 0.15, -0.1, 5], [2.997, 0.15, 0.2, 10]].do { |v|
  add.(at.(43, 12), \chant, [\freq, root * v[0], \amp, v[1], \atk, 2, \sus, 14, \rel, 8, \overtone, v[3],
    \pan, v[2], \hall, 0.8], "chant");
};
add.(at.(54), \chant, [\freq, root * 2, \amp, 0.35, \atk, 10, \sus, 36, \rel, 8, \overtone, 8, \pan, -0.2], "chant");
add.(at.(66), \chant, [\freq, root * 3, \amp, 0.25, \atk, 8, \sus, 22, \rel, 8, \overtone, 12, \pan, 0.3], "chant");
add.(at.(76), \chant, [\freq, root * 2, \amp, 0.4, \atk, 4, \sus, 12, \rel, 6, \overtone, 10, \pan, 0, \hall, 0.8], "chant");
add.(at.(90), \chant, [\freq, root * 2.004, \amp, 0.3, \atk, 10, \sus, 26, \rel, 8, \overtone, 7, \pan, 0.2], "chant");
add.(at.(114), \chant, [\freq, root * 2, \amp, 0.45, \atk, 6, \sus, 18, \rel, 12, \overtone, 8, \pan, 0, \hall, 0.8], "chant");

// ANCIENT PADS (v15): a distant temple choir and harmonium, open fifths
// on the root. In the void, the descent, under the second half of the peak
// and in the closing.
[[43.5, 6, 14, 7, 0.32], [76, 4, 14, 6, 0.32], [96, 10, 22, 6, 0.42], [112, 6, 18, 10, 0.3]].do { |p|
  add.(at.(p[0]), \pad, [\freq, root, \atk, p[1], \sus, p[2], \rel, p[3], \amp, p[4]], "pad");
};

// braams at the phrase doors (swells at 14, 40 and 80; the 40 one on the b2: dread)
[[14, 1.5, 1], [28, 0.05, 1], [36, 0.05, 1], [40, 2.5, 1.0595], [52, 0.05, 1], [60, 0.05, 1], [68, 0.05, 1],
 [80, 3, 1], [84, 0.05, 1], [96, 0.05, 1], [104, 0.05, 1]].do { |b|
  add.(at.(b[0]), \braam, [\freq, root * b[2], \amp, 0.2, \atk, b[1], \dur, rrand(4.5, 6.5), \bright, rrand(0.5, 0.8)], "braam");
};

// SWELLS (the industrial bass screeches): grinding swells into phrase
// downbeats, about one every 8 bars. The 41 one runs into the void, the 49
// one is the riser out of it, the 79 one is the long one alone in the descent.
[[26, 2], [34, 2], [41, 3], [49, 3], [58, 2], [66, 2], [72, 2], [79, 5], [90, 2], [98, 2], [106, 2], [110, 2]].do { |sw|
  var t0 = at.(sw[0]), dur = sw[1] * 16 * stepDur;
  add.(t0, \swell, [\freq, root, \dur, dur, \amp, 0.42, \drive, rrand(3.5, 5.5), \pan, 0, \sherman, 0.15], "screech");
};

// WHALES, sparse, far in the background: [bar, shape, ratio of the glide, growl]
[[4, 0, 1.3, 0], [10, 1, 1.6, 0], [16, 0, 0.75, 0.4], [26, 1, 1.5, 0], [45, 1, 1.8, 0], [48, 0, 0.7, 0.5],
 [62, 1, 1.4, 0], [77, 0, 1.25, 0.3], [81, 1, 1.6, 0], [96, 0, 0.8, 0.4], [114, 1, 1.5, 0], [120, 0, 1.25, 0.3]].do { |w|
  var f = root * [4, 6, 8].choose;
  var inGroove = ((w[0] >= 20) and: { w[0] < 44 }) or: { (w[0] >= 52) and: { w[0] < 76 } } or: { (w[0] >= 84) and: { w[0] < 112 } };
  add.(at.(w[0]), \whale, [\freq, f, \freq2, f * w[2], \dur, rrand(2.5, 4.5), \shape, w[1], \growl, w[3],
    \amp, if(inGroove) { 0.055 } { 0.1 }, \pan, rrand(-0.7, 0.7)], "whales");
};

// TORTURED CYMBALS at the doors of the piece: a different one every time.
// [bar, length in bars, mode (0 bow, 1 crash, 2 scrape, 3 reverse), body, pan]
[[8, 3, 0, \cymbal, -0.6], [18, 2, 3, \gong, 0.55], [32, 2.5, 2, \plate, -0.45], [40, 3.5, 0, \sheet, 0.6],
 [44, 3, 1, \cymbal, -0.3], [60, 2.5, 3, \plate, 0.5], [70, 3, 2, \cymbal, -0.6], [76, 4, 0, \gong, 0.4],
 [83, 1, 3, \cymbal, -0.5], [94, 2.5, 2, \sheet, 0.55], [104, 3, 1, \plate, -0.4], [110, 5, 0, \gong, 0.45]].do { |c|
  add.(at.(c[0]), \cymbal, [\freq, if(c[3] == \gong) { pick.(90, 180) } { pick.(220, 450) },
    \amp, [0.56, 0.36, 0.48, 0.48][c[2]], \dur, c[1] * 16 * stepDur,
    \bend, if(c[2] == 0) { rrand(0.985, 0.995) } { 1 }, \chatter, rrand(0.3, 0.8), \mode, c[2],
    \tone, rrand(3500, 5500), \pan, c[4], \hall, rrand(0.4, 0.7), \sherman, 0.1]
    ++ metalSpec.(c[3], if(c[2] == 1) { 5 } { 3 }), "cymbals");
};

// subs under the three big entries
[20, 52, 84].do { |b| add.(at.(b), \sub, [\freq, root, \amp, 0.5, \dur, 6], "sub") };

// tortured bowed sheet metal, bending down
[32, 60, 92, 104].do { |b|
  add.(at.(b) + rrand(0, 1.0), \metal, [\freq, pick.(70, 220), \amp, 0.5, \dur, rrand(10.0, 14.0), \mode, 2,
    \bend, rrand(0.84, 0.92), \drive, 2, \pan, rrand(-0.6, 0.6), \dist, 0.3] ++ metalSpec.(\sheet, 2), "metal");
};

// industrial noise bed through the second half of the conjuration and the peak
add.(at.(60), \noisewall, [\amp, 0.15, \atk, 14, \sus, (16 * 16 * stepDur) - 14, \rel, 1.5], "noise");
add.(at.(84), \noisewall, [\amp, 0.16, \atk, 10, \sus, (26 * 16 * stepDur) - 10, \rel, 3], "noise");

// detritus
add.(at.(2), \detritus, [\amp, 0.45, \density, 3, \dur, 18 * 16 * stepDur], "detritus");
add.(at.(52), \detritus, [\amp, 0.4, \density, 5, \dur, 24 * 16 * stepDur], "detritus");
add.(at.(86), \detritus, [\amp, 0.45, \density, 6, \dur, 28 * 16 * stepDur], "detritus");

// TEMPLE (v15): the dark field texture, denser atmosphere through the whole
// piece: [bar, bars, level, creaks]. Louder in the breaks, lower under the grooves.
[[2, 18, 0.5, 0.6], [20, 24, 0.58, 0.4], [44, 8, 0.6, 1.0], [52, 24, 0.55, 0.5], [76, 8, 0.6, 1.0],
 [84, 28, 0.5, 0.6], [112, 12, 0.55, 0.8]].do { |t|
  add.(at.(t[0]), \temple, [\freq, root, \amp, t[2], \creak, t[3], \atk, 6, \sus, (t[1] * 16 * stepDur) - 6, \rel, 6], "temple");
};

// MACHINE ROOM: a distant industrial room tone in the breaks and the closing
[[42, 12, 14], [74, 10, 18], [114, 14, 18]].do { |m|
  add.(at.(m[0]), \machine, [\freq, root * 2, \amp, 0.5, \atk, m[1], \sus, m[2], \rel, 8], "machine");
};

// ---------------------------------------------------------------------------
// THE GRID
// ---------------------------------------------------------------------------
(bars * 16).do { |s|
  var bar = s div: 16, st = s % 16, time = s * stepDur;
  var ph = (bar - 4) % 8;   // position in the 8-bar phrase
  var rite = (bar >= 20) and: { bar < 44 };
  var conj = (bar >= 52) and: { bar < 76 };
  var peak = (bar >= 84) and: { bar < 112 };
  var closeK = (bar >= 112) and: { bar < 118 };   // the closing: the kick walks out
  var groove = rite or: { conj } or: { peak };
  var hush = ((bar == 43) or: { bar == 75 }) and: { st >= 8 };
  var x = bar + (st / 16);

  // kick: half-time in the procession (FM tok up front: that rhythm stays),
  // then POLYMETERS, never a straight four: 3-3-2-2 (10 steps) in the rite,
  // 4-4-3-3 (14 steps) in the conjuration, both joined (3-3-2-2-4-4-3-3,
  // 24 steps against the 16-step bar) in the peak, 4-4-3-3 again in its last
  // 8 bars, 3-3-2-2 as it walks out. Bar downbeats always anchored.
  if(hush.not) {
    var kfm, ksh, kd, kdi, hit = false, kamp = 0.63;
    kfm = pw.(x, [[12, 0.5], [19.9, 0.5], [20, 0.2], [52, 0.25], [84, 0.28], [112, 0.3]]);
    ksh = pw.(x, [[12, 0.3], [20, 0.45], [52, 0.65]]);
    kdi = pw.(x, [[12, 0.2], [20, 0.35], [32, 0.5], [42, 0.85], [44, 0.5], [52, 0.5], [72, 0.8], [84, 0.6],
      [96, 0.9], [104, 1.0], [112, 0.6]]);
    kd = pw.(x, [[12, 1.0], [20, 1.0], [42, 1.4], [52, 1.1], [74, 1.4], [84, 1.2], [96, 1.5], [104, 1.6], [112, 1.2]]);
    if((bar >= 12) and: { bar < 20 } and: { (st == 0) or: { st == 8 } }) { hit = true; kamp = 0.42 };
    if(rite and: { (st == 0) or: { kpA[s % 10] == 1 } }) { hit = true };
    if(conj and: { (st == 0) or: { kpB[s % 14] == 1 } }) { hit = true; kamp = 0.72 };
    if(peak and: { bar < 104 } and: { (st == 0) or: { kpC[s % 24] == 1 } }) { hit = true; kamp = 0.72 };
    if(peak and: { bar >= 104 } and: { (st == 0) or: { kpB[s % 14] == 1 } }) { hit = true; kamp = 0.72 };
    if(closeK and: { (st == 0) or: { kpA[s % 10] == 1 } }) { hit = true; kamp = (118 - x).linlin(0, 6, 0.35, 0.62) };
    if(hit) {
      add.(time, \kick, [\freq, root, \amp, kamp, \fm, kfm, \drive, kdrive[kN % 5] * kd,
        \dist, kdi * kdistC[kN % 5], \sherman, ksh, \kdel, ksend[kN % 7] * (if(bar < 20) { 0.55 } { 0.45 }),
        \sat, ksat[kN % 5] * pw.(x, [[12, 0.3], [20, 0.5], [42, 0.8], [52, 0.6], [74, 0.8], [84, 0.7], [104, 0.9], [112, 0.6]])], "kick");
      kN = kN + 1;
    };
    if(groove and: { st == 14 } and: { bar % 4 == 3 } and: { hit.not }) {
      add.(time, \kick, [\freq, root, \amp, 0.4, \fm, kfm, \drive, 4, \dist, kdi, \sherman, ksh, \kdel, 0.8], "kick");
    };
  };

  // hats: a hypnotic pulse on every 3rd sixteenth. Accents cycle in 4s (a
  // 12-step loop) in the rite and the conjuration, in 5s (a 15-step loop) in
  // the peak. An open hat on a fixed off-step, a ghost tick before the
  // downbeat. In the last bar of every phrase the hats breathe out for the second half.
  if(hush.not) {
    var ap = if((s div: 3).even) { 0.5 } { -0.45 };   // closed hats alternate sides: width up top
    var breath = (ph == 7) and: { st >= 8 };
    if(breath.not) {
      if((bar >= 14) and: { bar < 20 } and: { s % 6 == 0 }) {
        add.(time, \hat, [\amp, 0.055, \decay, 0.03, \pan, ap, \tone, 0.5], "hats");
      };
      if((groove or: { closeK and: { bar < 116 } }) and: { s % 3 == 0 }) {
        var k = if(peak) { (s div: 3) % 5 } { (s div: 3) % 4 };
        var acc = if(peak) { [1, 0.5, 0.75, 0.45, 0.6] } { [1, 0.55, 0.8, 0.45] };
        var dec = if(peak) { [0.03, 0.02, 0.05, 0.02, 0.035] } { [0.03, 0.02, 0.045, 0.02] };
        var ton = if(peak) { [0.5, 0.65, 0.4, 0.7, 0.55] } { [0.5, 0.65, 0.4, 0.7] };
        add.(time, \hat, [\amp, 0.1 * acc[k] * [1, 0.8, 1.15, 0.9, 1.1, 0.75][((bar - 4) div: 8) % 6], \decay, dec[k],
          \tone, ton[k], \pan, ap, \sherman, 0.03], "hats");
      };
      if(inR.(\openhat, bar) and: { st == (if(bar.odd) { 10 } { 6 }) }) {
        add.(time, \hat, [\amp, 0.095, \decay, 0.18, \tone, 0.35, \pan, -0.35, \sherman, 0.08, \udel, 0.1], "hats");
      };
      if(inR.(\ghost, bar) and: { st == 15 }) {
        add.(time, \hat, [\amp, 0.05, \decay, 0.02, \tone, 0.8, \pan, 0.6], "hats");
      };
    };
  };

  // sparse percussion: hand skin 3-in-16 (left) and hollow log 2-in-7
  // (right), their colour cycling in 3s and 5s; a gravel roll grows into
  // every phrase's downbeat.
  if(hush.not and: { inR.(\sparse, bar) }) {
    if(rimP[st] == 1) {
      var c = s div: 16;
      add.(time, \hand, [\freq, root * [8, 12, 10][c % 3], \amp, 1.6 * [1, 0.75, 0.9, 0.7, 0.85][c % 5],
        \decay, [0.16, 0.22, 0.13][c % 3], \rise, 0.003, \tone, 1800, \slap, 0.3, \drive, 2.2,
        \pan, -0.45 + ((time * 2pi * 0.23).sin * 0.12), \sherman, 0.2, \kdel, 0.15, \dverb, 0.25], "sparse");
    };
    if(blockP[s % 7] == 1) {
      var c = s div: 7;
      add.(time, \hand, [\freq, pick.(220, 480), \amp, 2.5 * [1, 0.7, 0.85][c % 3], \decay, [0.14, 0.2, 0.11, 0.17, 0.12][c % 5],
        \rise, 0.005, \tone, 1200, \slap, 0.1, \drive, 2.6, \ratios, [1, 2.32, 3.9, 5.1], \gains, [1, 0.4, 0.2, 0.1],
        \pan, 0.5 + ((time * 2pi * 0.13).cos * 0.12), \sherman, 0.15, \udel, 0.2, \dverb, 0.2], "sparse");
    };
    if((ph == 7) and: { st >= 12 }) {
      add.(time, \grit, [\freq, root * 8, \amp, (st - 11) * 0.12, \decay, 0.07, \crackle, 0.6, \comb, 0.6,
        \pan, (st - 13.5) * 0.3, \sherman, 0.25, \dverb, 0.3], "grit");
    };
  };

  // GRIT: noisy percussion here and there. 3-in-11, only in the second half
  // of each 8-bar phrase, so the texture breathes over the phrase while the
  // floor stays the same. Colour cycles in 3s and 4s; never on the beat.
  if(hush.not and: { inR.(\grit, bar) } and: { ph >= 4 } and: { gritP[s % 11] == 1 } and: { st % 4 != 0 }) {
    var g = s div: 11;
    add.(time, \grit, [\freq, root * [8, 12, 16][g % 3], \amp, 0.6 * [1, 0.6, 0.8, 0.55][g % 4],
      \decay, [0.09, 0.2, 0.12, 0.3][g % 4], \crackle, [0.3, 0.7, 0.5][g % 3], \comb, [0.4, 0.7][g % 2],
      \lo, [500, 700, 400][g % 3], \hi, [2600, 3200, 2200][g % 3],
      \pan, [0.35, -0.3, 0.55, -0.5][g % 4], \sherman, 0.2, \dverb, 0.3, \udel, [0, 0.25][g % 2]], "grit");
  };
  // EXHALE: a breath of resonant noise over the last beat of each phrase
  // (and mid-phrase in the peak), cut dead on the next downbeat
  if(groove and: { st == 12 } and: { (ph == 7) or: { peak and: { ph == 3 } } } and: { hush.not }) {
    add.(time, \exhale, [\freq, root * [12, 16, 20][bar % 3], \amp, 0.22, \dur, 4 * stepDur,
      \rq, 0.25, \hall, 0.35], "grit");
  };

  // MODULAR SEQUENCES: sparse low-pass-gate pings, a hand on a modular.
  // FIXED notes from the root's harmonic field, no glides; what moves is the
  // timbre (FM depth, folding, gate length) on cycles of 2, 3, 5 and 7.
  // v15: about 10 dB louder in the grooves (they were 28-32 dB under the mix)
  // and a little brighter, so they read through the mids.
  //  A: 5-in-13, three notes, mid register: the second half of the rite, alone in the void, the end of the peak
  //  B: 4-in-11, two higher notes, metallic, with the tempo echo: the conjuration and the descent
  //  C: 3-in-7, woody "bongo" folds: the first part of the peak
  if(hush.not) {
    if(inR.(\seqA, bar) and: { seqAP[s % 13] == 1 }) {
      var n = s div: 13;
      add.(time, \lpg, [\freq, seqAN[n % 3], \amp, if(groove) { 0.45 } { 0.22 },
        \decay, [0.25, 0.5, 0.35][n % 3], \fm, [0.8, 1.6, 1.1, 2.0, 1.3][n % 5], \ratio, 2,
        \fold, [0.1, 0.4][n % 2], \bright, 0.7, \pan, (time * 2pi * 0.07).sin * 0.5,
        \sherman, 0.2, \udel, if(groove) { 0.25 } { 0.45 }, \hall, if(groove) { 0.1 } { 0.3 }], "seq");
    };
    if(inR.(\seqB, bar) and: { seqBP[s % 11] == 1 }) {
      var n = s div: 11;
      add.(time, \lpg, [\freq, seqBN[n % 2], \amp, 0.42, \decay, [0.15, 0.3, 0.2][n % 3],
        \fm, [1.2, 2.0, 1.5, 2.4, 1.0, 1.8, 2.2][n % 7], \ratio, 3.5, \fold, [0.2, 0.5, 0.3][n % 3], \bright, 0.7,
        \pan, if(n.even) { 0.45 } { -0.4 }, \sherman, 0.15, \kdel, 0.35, \udel, 0, \hall, if(groove) { 0.08 } { 0.25 }], "seq");
    };
    if(inR.(\seqC, bar) and: { seqCP[s % 7] == 1 }) {
      var n = s div: 7;
      add.(time, \lpg, [\freq, seqCN[n % 2], \amp, 0.5, \decay, [0.12, 0.2, 0.15, 0.25, 0.1][n % 5],
        \fm, [2.2, 1.4, 2.8][n % 3], \ratio, 1, \fold, [0.6, 0.35, 0.8][n % 3], \bright, 0.6,
        \pan, -0.25 + ((n % 3) * 0.25), \sherman, 0.3, \udel, 0.15, \hall, 0.05], "seq");
    };
    // BLEEPS (v15): tiny modular blips, very sparse (3 in 19 and 2 in 23,
    // two cycles that drift against each other and the bar), each cycling
    // through fixed notes; they float off into the unsynced echo.
    if(inR.(\bleep, bar) and: { (bleepP[s % 19] == 1) or: { bleepQ[s % 23] == 1 } }) {
      var n = s div: 19, q = bleepQ[s % 23] == 1;
      add.(time, \bleep, [\freq, if(q) { bleepN[(s div: 23) % 2 + 3] } { bleepN[n % 3] }, \amp, if(groove) { 0.24 } { 0.17 },
        \decay, [0.05, 0.09, 0.06, 0.12][n % 4], \fm, [0.4, 1.0, 0.7][n % 3],
        \pan, if(q) { [-0.6, 0.6][(s div: 23) % 2] } { [0.3, -0.2, 0.65, -0.5, 0.1][n % 5] },
        \udel, [0.35, 0.15, 0.5][n % 3], \sherman, 0.1, \dverb, 0.2, \hall, if(groove) { 0.05 } { 0.2 }], "seq");
    };
  };

  // BASS: sparse distorted layers on the ROOT only (no pitch changes), all
  // through the bass bus. Variation comes from timbre cycles of different
  // lengths (3, 5, 7), so the growl keeps changing colour while staying
  // hypnotic.
  if(hush.not) {
    // boom: 20-step cycle, from the procession on
    if(inR.(\boom, bar) and: { boomP[s % 20] == 1 }) {
      add.(time, \boom, [\freq, root, \amp, if(groove) { 0.6 } { 0.45 },
        \decay, [0.7, 1.0, 0.8, 1.2][s % 4], \comb, [0.15, 0.4, 0.25][s % 3],
        \drive, pw.(x, [[14, 2], [42, 3], [52, 2.8], [74, 3.2], [84, 3], [104, 3.5], [112, 2.5]]), \sherman, 0.3, \sat, 0.25], "bass");
    };
    // growl: 2 in 11, never on the beat; colour cycles in 3s, 5s and 7s, and
    // every 4th cycle it doubles (a second hit two 16ths later)
    if(inR.(\growl, bar) and: { growlP[s % 11] == 1 } and: { st % 4 != 0 }) {
      var g = s div: 11;
      var args = [\freq, root * 2, \amp, 0.42, \dur, stepDur * [2, 4, 3, 6, 2][g % 5],
        \fmAmt, [1.2, 3.0, 2.0, 4.0, 1.6, 2.6, 3.4][g % 7], \cut, [550, 900, 700][g % 3],
        \drive, pw.(x, [[24, 3.5], [52, 4], [84, 4.2], [104, 4.5]]), \sherman, 0.7, \dly, 0.08, \sat, [0.15, 0.4][g % 2],
        \pan, 0];
      add.(time, \growl, args, "bass");
      if(g % 4 == 3) { add.(time + (2 * stepDur), \growl, args ++ [\amp, 0.3, \pan, 0], "bass") };
    };
    // low growl: an octave under, 3 in 13. Alone with the drone in the descent.
    if(inR.(\lowg, bar) and: { lowP[s % 13] == 1 } and: { st % 4 != 0 }) {
      var g = s div: 13;
      add.(time, \growl, [\freq, root, \amp, 0.45, \dur, stepDur * [3, 5, 4, 6][g % 4],
        \fmAmt, [1.0, 2.2, 1.6][g % 3], \cut, [400, 650, 500, 750, 550][g % 5], \drive, 3.2,
        \sherman, 0.6, \dly, if(groove) { 0 } { 0.12 }, \sat, 0.35, \pan, 0], "bass");
    };
  };
  // drone swells under the phrases (root only)
  if((st == 0) and: { [24, 32, 40, 56, 64, 72, 78, 88, 96, 104].includes(bar) }) {
    add.(time, \bdrone, [\freq, root, \amp, 0.3,
      \atk, stepDur * 16 * 1.5, \sus, stepDur * 16 * 1, \rel, stepDur * 16 * 1.5,
      \cut, 450, \drive, 3], "bass");
  };

  // frame drums: 5 in 12 (3 against 4), tuned above the kick (8x and 12x the root)
  if(inR.(\frame, bar) and: { frame[s % 12] == 1 } and: { hush.not }) {
    var low = (s % 12) < 6;
    add.(time, \frame, [\freq, root * (if(low) { 8 } { 12 }), \amp, (if(low) { 0.08 } { 0.056 }) * rrand(0.85, 1.0),
      \decay, if(low) { 0.6 } { 0.4 }, \hard, rrand(0.2, 0.4), \pan, if(low) { -0.4 } { 0.45 }], "perc");
  };

  // ritual tom: tresillo, tuned at 7x the root (out of the kick's body);
  // a roll into each break and into the peak
  if(inR.(\tom, bar) and: { tom[s % 8] == 1 } and: { hush.not }) {
    add.(time, \frame, [\freq, root * 7, \amp, 0.085, \decay, 0.8, \hard, 0.45, \pan, -0.1], "perc");
  };
  [42, 74, 82].do { |rb|
    if(((bar == rb) and: { st % 2 == 0 }) or: { (bar == (rb + 1)) and: { st < 8 } }) {
      add.(time, \frame, [\freq, root * 7, \amp, (x - rb).linlin(0, 1.5, 0.08, 0.2), \decay, 0.6, \hard, 0.5, \pdrop, 1.0,
        \pan, rrand(-0.2, 0.2)], "perc");
    };
  };

  // shaker: every 16th, accents every 3
  if(inR.(\shaker, bar) and: { hush.not } and: { s % 11 != 5 }) {
    add.(time, \shaker, [\amp, (if(s % 3 == 0) { 0.032 } { 0.014 }) * [1, 0.6, 0.8, 0.5, 0.9, 0.6, 0.7][s % 7],
      \pan, 0.6 + ((s * 0.05).sin * 0.1)], "perc");
  };

  // ritual bells (tingsha): every 6 sixteenths, two pitches alternating
  if(inR.(\tingsha, bar) and: { hush.not } and: { s % 6 == 0 }) {
    add.(time, \metal, [\freq, tingsha[(s div: 6) % 2], \amp, if(groove) { 0.1 } { 0.08 }, \dur, 0.5,
      \mode, 0, \hard, 0.35, \pan, if((s div: 6).even) { -0.5 } { 0.5 }, \dist, 0.55, \hall, 0.3, \gsend, 0.5,
      \out, busDrums, \dverb, 0.2, \udel, 0.1]
      ++ metalSpec.(\bell, 1.2), "metal");
  };

  // anvil: 3 in 5
  if(inR.(\anvil, bar) and: { anvil[s % 5] == 1 } and: { hush.not }) {
    add.(time, \metal, [\freq, pick.(500, 1100), \amp, 0.15, \dur, 0.3, \mode, 0, \hard, 0.55, \pan, 0.65,
      \dist, 0.3, \hall, 0.0, \gsend, 0.3, \out, busDrums, \dverb, 0.3] ++ metalSpec.(\bar, 0.35), "metal");
  };

  // clank backbeat in the conjuration and the peak
  if(inR.(\clank, bar) and: { st == 12 } and: { hush.not }) {
    add.(time, \metal, [\freq, pick.(250, 450), \amp, 0.35, \dur, 0.3, \mode, 0, \hard, 0.6, \drive, 2,
      \pan, -0.3, \dist, 0.2, \hall, 0.05, \gsend, 0.5, \out, busDrums, \dverb, 0.35] ++ metalSpec.(\plate, 0.4), "metal");
  };

  // bell tolls in the invocation, the void, the descent and the closing
  if((st == 0) and: {
    ((bar < 8) and: { bar.even }) or: { (bar >= 44) and: { bar < 52 } } or: { (bar >= 76) and: { bar < 84 } and: { bar.even } }
      or: { (bar >= 116) and: { bar.even } }
  }) {
    add.(time, \metal, [\freq, bellF, \amp, if(groove.not and: { bar > 8 } and: { bar < 116 }) { 0.28 } { 0.45 }, \dur, 0.5,
      \mode, 0, \hard, 0.3, \pan, rrand(-0.3, 0.3),
      \dist, 0.6, \hall, 0.6] ++ metalSpec.(\bell, 3), "metal");
  };
};

// ---------------------------------------------------------------------------
// TRUE POLYRHYTHMS
// ---------------------------------------------------------------------------
// hand drums on triplet 8ths (12 per bar): 3 against the 4-beat kick
(bars * 12).do { |k|
  var beatT = 60 / bpm;
  var time = k * (beatT / 3);
  var bar = k div: 12;
  var pat = [1, 0, 1, 1, 0, 1];   // 4 of 6, repeating
  if(inR.(\hand3, bar)) {
    if(pat[k % 6] == 1) {
      var accentT = (k % 3 == 0);
      add.(time, \frame, [\freq, root * (if(accentT) { 6 } { 8 }), \amp, if(accentT) { 0.07 } { 0.04 },
        \decay, 0.25, \hard, 0.6, \pan, if(k.even) { 0.25 } { -0.25 }, \dverb, 0.35], "perc");
    };
  };
};
// a metal pulse of 5 per bar: 5 against 4
(bars * 5).do { |k|
  var time = k * (stepDur * 16 / 5);
  var bar = k div: 5;
  if(inR.(\pulse5, bar)) {
    add.(time, \metal, [\freq, tingsha[0] * 0.5, \amp, if(k % 5 == 0) { 0.14 } { 0.08 }, \dur, 0.3, \mode, 0,
      \hard, 0.4, \pan, -0.55, \dist, 0.4, \hall, 0.05, \gsend, 0.3, \out, busDrums, \dverb, 0.2, \udel, 0.1]
      ++ metalSpec.(\bell, 0.6), "metal");
  };
};
// metal ticks 4-in-9 (a polymeter on the 16th grid)
(bars * 16).do { |s|
  var bar = s div: 16;
  var tick = euclid.(4, 9, 0);
  if(inR.(\ticks, bar) and: { tick[s % 9] == 1 }) {
    add.(s * stepDur, \metal, [\freq, pick.(1200, 2200), \amp, 0.06, \dur, 0.2, \mode, 0, \hard, 0.3,
      \pan, 0.75, \dist, 0.5, \hall, 0.0, \gsend, 0.2, \out, busDrums,
      \dverb, 0.2, \udel, 0.15] ++ metalSpec.(\bar, 0.2), "metal");
  };
};

// ---------------------------------------------------------------------------
// AUTOMATION
// ---------------------------------------------------------------------------
set.(at.(8), grainer, [\density, 5, \gdur, 0.2, \spread, 0.2]);
set.(at.(20), grainer, [\density, 9, \gdur, 0.12, \spread, 0.4, \rev, 0.35]);
set.(at.(44), grainer, [\density, 4, \gdur, 0.6, \spread, 0.02, \rev, 0.5]);
set.(at.(52), grainer, [\density, 12, \gdur, 0.1, \spread, 0.6, \rev, 0.4]);
set.(at.(76), grainer, [\density, 4, \gdur, 0.5, \spread, 0.05, \rev, 0.55]);
set.(at.(84), grainer, [\density, 14, \gdur, 0.08, \spread, 0.7, \rev, 0.45]);
set.(at.(112), grainer, [\density, 4, \gdur, 0.5, \spread, 0.05, \rev, 0.6, \amp, 0.9]);
[[20, 0.65], [44, 0.85], [52, 0.65], [76, 0.85], [84, 0.65], [112, 0.85]].do { |h|
  set.(at.(h[0]), master, [\hallMix, h[1]]);
};
set.(at.(52), master, [\drive, 1.5]);
set.(at.(20), sherman, [\base, 280, \reso, 0.85, \drive, 4.5]);
set.(at.(40), sherman, [\mix, 0.15]);   // calm the Sherman in the build: no squelch on the roll
set.(at.(44), sherman, [\mix, 0.5, \base, 180, \reso, 0.85, \drive, 3]);
set.(at.(52), sherman, [\base, 320, \reso, 0.9, \drive, 5, \lfoAmt, 0.2]);
set.(at.(76), sherman, [\base, 200, \reso, 0.8, \drive, 3]);
set.(at.(84), sherman, [\base, 300, \reso, 0.9, \drive, 5, \lfoAmt, 0.25]);
set.(at.(112), sherman, [\base, 200, \reso, 0.7, \drive, 3]);
set.(at.(112), master, [\drive, 1.5, \wow, 1.2, \hiss, 0.004]);
set.(at.(118), master, [\wow, 2.2, \drive, 1.8, \hiss, 0.005]);
set.(at.(120), master, [\fadeTime, 7, \fade, 0]);

score.add([total, [\c_set, 0, 0]]);
score.sort;

("RITE  seed " ++ seed ++ "  root " ++ root.round(0.01) ++ " Hz  ->  " ++ outPath).postln;
score.recordNRT(outPath ++ ".osc", outPath, nil, 48000, "WAV", "int24", opts, "", total, {
  File.delete(outPath ++ ".osc");
  "done".postln;
  0.exit;
});
)
