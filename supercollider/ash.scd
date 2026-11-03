// ASH — generative electroacoustic / death-industrial piece, 3 minutes.
//
//   0:00  RUST      drone out of tape hiss, bowed sheet metal, scrapes, detritus, a ghost voice
//   0:40  MACHINE   slow limping industrial hits, tritone drone, struck & bouncing metal, grains
//   1:20  FURNACE   sub impact, 50-second noise wall, tortured bowed metal, granular storm -> cut
//   2:10  ASH       reverb tail, drone, distant bells; the tape degrades, wobbles and fades
//
// Every metal object is tuned to harmonics of the drone, so the whole piece
// shares one spectrum. All of it runs through a tape stage on the master:
// pre-emphasis, asymmetric saturation (even harmonics), de-emphasis,
// wow & flutter, hiss.
//
// Render (non-realtime, no audio device needed):
//   SEED=3 OUT=ash.wav sclang ash.scd
// Optional: SOLO=metal (drone metal hits noise ghost sub detritus grains)
// renders one layer only, for stems.
//
// Or in the SuperCollider IDE: just run the file; it writes ash.wav next to it.

(
var seed = ("SEED".getenv ? "1").asInteger;
var solo = "SOLO".getenv;
var outPath = "OUT".getenv ? (thisProcess.nowExecutingPath.dirname +/+ "ash.wav");
var total = 180;
var busDry = 16, busVerb = 18, busGrain = 20;
var opts, server, score, defs, add, set, metalSpec, root, field, pick;
var drone1, drone2, grainer, master, t, i, pat;

thisThread.randSeed = seed;

opts = ServerOptions.new
  .numOutputBusChannels_(2).numInputBusChannels_(0)
  .sampleRate_(48000).memSize_(65536).maxNodes_(4096);
server = Server(\ashNRT, NetAddr("127.0.0.1", 57190), opts);
score = Score.new;

// ---------------------------------------------------------------------------
// SYNTHDEFS
// ---------------------------------------------------------------------------
defs = [

  // Low drone: detuned saws ring-modulated by an inharmonic sine (that hum
  // of machinery), resonant low-pass breathing slowly, then driven. "dirt"
  // morphs it from a dark hum into a grinding industrial roar.
  SynthDef(\drone, { |freq = 40, amp = 0.2, atk = 10, sus = 60, rel = 15, dirt = 0.2,
      vsend = 0.3, gsend = 0.2, mute = 0|
    var env = EnvGen.kr(Env.linen(atk, sus, rel, 1, \sin), doneAction: 2);
    var d = dirt.lag(6);
    var drift = LFNoise2.kr(0.05 ! 4, 0.004, 1);
    var sig = Mix(LFSaw.ar(freq * [1, 1.0031, 2.002, 1.4987] * drift, 0, [1, 0.8, 0.35, 0.3]));
    var rm = sig * SinOsc.ar((freq * 3.97) + LFNoise1.kr(0.1, 3));
    sig = sig + (rm * d * 0.6);
    sig = RLPF.ar(sig, LFNoise2.kr(0.03).exprange(150, 700) * (1 + (d * 1.5)), 0.35);
    sig = (sig * (1 + (d * 5))).tanh;
    sig = LPF.ar(sig, 2500 + (d * 2500));
    sig = [sig, DelayC.ar(sig, 0.05, LFNoise2.kr(0.07).range(0.012, 0.035))];
    sig = sig * env * amp.lag(6);
    Out.ar(busDry, sig * (1 - mute));
    Out.ar(busVerb, sig * vsend * (1 - mute));
    Out.ar(busGrain, sig * gsend);
  }),

  // Metal: 16 inharmonic resonant modes (sheet, plate, bell, bar) excited by
  // a strike (mode 0), a jagged scrape (1) or a bow (2). "bend" slides every
  // mode together over the duration: the metal being bent while it sounds.
  // "dist" = distance: quieter, darker, wetter.
  SynthDef(\metal, { |freq = 200, amp = 0.2, dur = 4, mode = 0, bend = 1, drive = 1, pan = 0,
      dist = 0.3, vsend = 0.4, gsend = 0.4, mute = 0|
    var ratios = \ratios.kr(1 ! 16), amps = \amps.kr(0.05 ! 16), rings = \rings.kr(1 ! 16);
    var bendEnv = Line.kr(1, bend, dur) * LFNoise2.kr(4, 0.004, 1);
    var strike = Decay2.ar(Impulse.ar(0), 0.0005, 0.012) * (1 + PinkNoise.ar(0.5));
    var scrape = BPF.ar(WhiteNoise.ar, LFNoise1.kr(6).exprange(1200, 6000), 0.6)
      * LFNoise0.ar(LFNoise1.kr(1).range(12, 60)).range(0, 1).squared * EnvGen.kr(Env.sine(dur));
    var bow = BPF.ar(PinkNoise.ar, freq * 6, 0.4)
      * EnvGen.kr(Env([0, 1, 0.8, 0], [0.35, 0.35, 0.3] * dur, \sin)) * LFNoise1.kr(7, 0.25, 1);
    var exc = Select.ar(mode, [strike, scrape * 0.4, bow * 0.3]);
    var sig = DynKlank.ar(`[ratios, amps, rings], exc, freq * bendEnv);
    sig = LeakDC.ar((sig * drive).tanh);
    sig = LPF.ar(sig, (1 - dist).linexp(0, 1, 2500, 16000));
    DetectSilence.ar(sig.abs + Line.ar(1, 0, dur), 0.0002, 0.3, doneAction: 2);
    sig = Pan2.ar(sig, pan) * amp * (1 - (dist * 0.6));
    Out.ar(busDry, sig * (1 - mute));
    Out.ar(busVerb, sig * (vsend + (dist * 0.5)) * (1 - mute));
    Out.ar(busGrain, sig * gsend);
  }),

  // Industrial hit: a sine whose pitch collapses onto the root, a crack of
  // noise, a short metallic clang, all driven into the ground and partly
  // sample-rate-crushed. Slow and heavy, not a beat.
  SynthDef(\hit, { |freq = 45, amp = 0.4, decay = 1.2, dirt = 0.6, pan = 0, vsend = 0.35,
      gsend = 0.3, mute = 0|
    var penv = EnvGen.ar(Env([freq * 5, freq * 1.3, freq], [0.03, 0.2], \exp));
    var body = SinOsc.ar(penv) * EnvGen.ar(Env.perc(0.002, decay, 1, -6));
    var crack = HPF.ar(PinkNoise.ar, 900) * EnvGen.ar(Env.perc(0.0005, 0.09, 1, -8));
    var clang = Klank.ar(`[[1, 1.47, 2.36, 3.28, 4.95] * Rand(250, 420), nil, [0.6, 0.45, 0.35, 0.25, 0.15]],
      Impulse.ar(0) * 0.08);
    var sig = body + (crack * 0.35) + (clang * Rand(0.2, 0.6));
    sig = (sig * (1 + (dirt * 7))).tanh * 0.7;
    sig = sig + ((Latch.ar(sig, Impulse.ar(Rand(3000, 8000))) - sig) * dirt * 0.4);
    DetectSilence.ar(sig, 0.0002, 0.2, doneAction: 2);
    sig = Pan2.ar(sig, pan) * amp;
    Out.ar(busDry, sig * (1 - mute));
    Out.ar(busVerb, sig * vsend * (1 - mute));
    Out.ar(busGrain, sig * gsend);
  }),

  // Noise wall: pink noise + crackle through a resonant low-pass that opens
  // over the swell, a metallic comb "tube" inside it, then driven hard.
  // Ends abruptly (rel = 0.05) for the hard cut.
  SynthDef(\noisewall, { |amp = 0.2, atk = 20, sus = 10, rel = 0.05, lo = 200, hi = 3500,
      vsend = 0.3, gsend = 0.3, mute = 0|
    var env = EnvGen.kr(Env([0, 1, 1, 0], [atk, sus, rel], [3, 0, -4]), doneAction: 2);
    var sig = PinkNoise.ar(1 ! 2) + (Crackle.ar(1.95 ! 2) * 0.25);
    var cut = EnvGen.kr(Env([lo, hi, hi], [atk, sus], \exp)) * LFNoise2.kr(0.4 ! 2).range(0.6, 1.5);
    sig = RLPF.ar(sig, cut, 0.3);
    sig = sig + (CombC.ar(sig, 0.05, 1 / LFNoise2.kr(0.1 ! 2).range(40, 70), 1.5) * 0.25);
    sig = (sig * 5).tanh * env * amp;
    Out.ar(busDry, sig * (1 - mute));
    Out.ar(busVerb, sig * vsend * (1 - mute));
    Out.ar(busGrain, sig * gsend);
  }),

  // Ghost: breath noise and a faint buzz through three wandering formant
  // filters. Not quite a voice, mostly sent to the reverb.
  SynthDef(\ghost, { |freq = 110, amp = 0.1, atk = 5, sus = 8, rel = 8, vsend = 0.8, gsend = 0.3, mute = 0|
    var env = EnvGen.kr(Env.linen(atk, sus, rel, 1, \sin), doneAction: 2);
    var src = PinkNoise.ar(0.6) + (Saw.ar(freq * LFNoise2.kr(0.3, 0.01, 1) * [1, 1.007]).sum * 0.15);
    var f = [LFNoise2.kr(0.15).range(300, 800), LFNoise2.kr(0.12).range(700, 1300),
      LFNoise2.kr(0.1).range(2200, 2900)];
    var sig = Mix(Resonz.ar(src, f, 0.04, [1, 0.6, 0.35])) * 6;
    sig = Pan2.ar(sig, LFNoise2.kr(0.05) * 0.7) * env * amp;
    Out.ar(busDry, sig * (1 - mute));
    Out.ar(busVerb, sig * vsend * (1 - mute));
    Out.ar(busGrain, sig * gsend);
  }),

  // Sub drop: the cinematic floor falling away.
  SynthDef(\sub, { |freq = 50, amp = 0.4, dur = 6, mute = 0|
    var sig = SinOsc.ar(EnvGen.kr(Env([freq * 2, freq * 0.6], [dur], \exp)));
    sig = (sig * EnvGen.kr(Env.perc(0.02, dur, 1, -3), doneAction: 2) * 1.6).tanh * amp;
    Out.ar(busDry, (sig ! 2) * (1 - mute));
  }),

  // Detritus: musique-concrete junk. Random tiny resonant clicks (objects,
  // rust flaking off) plus grit, scattered in stereo.
  SynthDef(\detritus, { |amp = 0.1, density = 6, dur = 20, vsend = 0.4, gsend = 0.5, mute = 0|
    var env = EnvGen.kr(Env.linen(2, (dur - 4).max(0.1), 2, 1, \sin), doneAction: 2);
    var trig = Dust.ar(density ! 2);
    var sig = Ringz.ar(trig * TRand.ar(0.1, 1, trig), TExpRand.ar(400, 7000, trig), TExpRand.ar(0.005, 0.08, trig));
    sig = sig + (Dust2.ar((density * 4) ! 2) * 0.15);
    sig = HPF.ar(sig, 200) * env * amp;
    Out.ar(busDry, sig * (1 - mute));
    Out.ar(busVerb, sig * vsend * (1 - mute));
    Out.ar(busGrain, sig * gsend);
  }),

  // Grainer: keeps recording everything sent to it into an 8-second loop
  // and scatters grains of the recent past: transposed, reversed, panned.
  SynthDef(\grainer, { |density = 4, gdur = 0.15, amp = 0.5, spread = 0.3, rev = 0.3, vsend = 0.6, mute = 0|
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
    sig = LeakDC.ar(sig) * amp.lag(3) * (1 - mute);
    Out.ar(busDry, sig);
    Out.ar(busVerb, sig * vsend);
  }),

  // Master: reverb return, then the tape: pre-emphasis, asymmetric
  // saturation with auto-gain, de-emphasis, wow & flutter, hiss, glue.
  SynthDef(\master, { |drive = 1.4, bias = 0.12, wow = 0.3, hiss = 0.004, verbMix = 0.7, gain = 1,
      fade = 1, fadeTime = 3|
    var sig = In.ar(busDry, 2);
    var rev = JPverb.ar(In.ar(busVerb, 2), 7, 0.35, 2.2, 0.707, 0.25, 0.6, 1.0, 0.85, 0.55, 250, 4500);
    var d = drive.lag(5), w = wow.lag(5);
    sig = sig + (rev * verbMix);
    sig = HPF.ar(sig, 22);
    sig = sig + (HPF.ar(sig, 3000) * 0.6);
    sig = ((sig * d) + bias).tanh - bias.tanh;
    sig = sig / (d * (1 - bias.tanh.squared));
    sig = BHiShelf.ar(sig, 3000, 1, -4);
    sig = DelayC.ar(sig, 0.03, 0.006 + (LFNoise2.kr(0.7) * 0.0012 * w) + (SinOsc.kr(5.8) * 0.00015 * w));
    sig = sig + HPF.ar(PinkNoise.ar(hiss.lag(5) ! 2), 1500);
    sig = LPF.ar(sig, 14000);
    sig = LeakDC.ar(sig);
    sig = Compander.ar(sig, sig, 0.25, 1, 0.5, 0.01, 0.25);
    sig = Limiter.ar(sig * gain * Lag.kr(fade, fadeTime), 0.93, 0.01);
    Out.ar(0, sig);
  })
];

// ---------------------------------------------------------------------------
// SCORE HELPERS
// ---------------------------------------------------------------------------
score.add([0.0, ['/g_new', 1, 0, 0]]);
defs.do { |d| score.add([0.0, ['/d_recv', d.asBytes]]) };

add = { |time, name, args, layer|
  var s = Synth.basicNew(name, server);
  var m = if(solo.isNil or: { solo == layer }) { 0 } { 1 };
  score.add([time, s.newMsg(Group.basicNew(server, 1), args ++ [\mute, m], \addToHead)]);
  s
};
set = { |time, node, args| score.add([time, node.setMsg(*args)]) };

// One spectrum for the whole piece: the drone's root and its harmonics.
root = [36.71, 38.89, 41.2, 43.65].choose;
field = root * [3, 4, 5, 6, 7, 8, 9, 11, 13, 16];
pick = { |lo, hi|
  var f = field.choose;
  while { f < lo } { f = f * 2 };
  while { f > hi } { f = f / 2 };
  f
};

metalSpec = { |kind, ring = 1|
  var base = switch(kind,
    \plate, { [1, 1.35, 1.67, 2.13, 2.39, 2.84, 3.17, 3.52, 3.98, 4.41, 4.97, 5.43, 6.12, 6.7, 7.3, 8.1] },
    \bell, { [0.5, 1, 1.183, 1.506, 2, 2.514, 2.662, 3.011, 4.166, 5.433, 6.796, 8.215, 9.1, 10.3, 11.7, 13.2] },
    \bar, { [1, 2.756, 5.404, 8.933, 13.34, 1.01, 2.77, 5.43, 8.97, 3.1, 4.4, 6.9, 11.2, 1.52, 2.21, 7.7] },
    { Array.fill(16, { |k| (k + 1) ** 1.33 }) }  // sheet
  );
  var ratios = base.collect { |r| r * rrand(0.99, 1.01) };
  var amps = ratios.collect { |r, k| rrand(0.3, 1.0) * ((k + 1) ** -0.5) * 0.05 };
  var rings = ratios.collect { |r| rrand(0.6, 1.4) * (r ** -0.4) * ring };
  [\ratios, ratios, \amps, amps, \rings, rings]
};

// ---------------------------------------------------------------------------
// THE PIECE
// ---------------------------------------------------------------------------
grainer = Synth.basicNew(\grainer, server);
score.add([0.01, grainer.newMsg(Group.basicNew(server, 1),
  [\density, 3, \gdur, 0.25, \amp, 1.5, \spread, 0.1, \rev, 0.3,
   \mute, if(solo.isNil or: { solo == "grains" }) { 0 } { 1 }], \addToTail)]);
master = Synth.basicNew(\master, server);
score.add([0.02, master.newMsg(Group.basicNew(server, 1), [\drive, 1.4, \wow, 0.3, \hiss, 0.004, \gain, 1.33],
  \addToTail)]);

// --- RUST 0:00-0:40 ---------------------------------------------------------
drone1 = add.(0.5, \drone, [\freq, root, \amp, 0.07, \atk, 18, \sus, 140, \rel, 20,
  \dirt, 0.15, \vsend, 0.25, \gsend, 0.15], "drone");
add.(3, \detritus, [\amp, 0.48, \density, 3, \dur, 40], "detritus");
[5, 16, 28].do { |tt|
  add.(tt + rrand(0, 3.0), \metal, [\freq, pick.(60, 220), \amp, 0.8, \dur, rrand(10.0, 16.0), \mode, 2,
    \bend, rrand(0.97, 1.0), \drive, 1.5, \pan, rrand(-0.7, 0.7), \dist, rrand(0.5, 0.85)]
    ++ metalSpec.([\sheet, \plate].choose, 2.5), "metal");
};
t = 8;
while { t < 40 } {
  add.(t, \metal, [\freq, pick.(150, 500), \amp, 0.35, \dur, rrand(0.8, 3.0), \mode, 1,
    \pan, rrand(-0.9, 0.9), \dist, rrand(0.3, 0.8), \drive, 1.2] ++ metalSpec.(\sheet, 0.8), "metal");
  t = t + exprand(4.0, 10.0);
};
add.(22, \ghost, [\freq, root * 3, \amp, 0.35, \atk, 8, \sus, 8, \rel, 9], "ghost");

// --- MACHINE 0:40-1:20 -------------------------------------------------------
set.(40, drone1, [\dirt, 0.45, \amp, 0.085]);
set.(40, grainer, [\density, 10, \gdur, 0.12, \spread, 0.5, \amp, 1.07]);
drone2 = add.(42, \drone, [\freq, root * 1.414, \amp, 0.05, \atk, 15, \sus, 70, \rel, 12,
  \dirt, 0.4, \vsend, 0.3, \gsend, 0.2], "drone");
add.(45, \detritus, [\amp, 0.56, \density, 8, \dur, 36], "detritus");
add.(55, \ghost, [\freq, root * 4, \amp, 0.4, \atk, 6, \sus, 6, \rel, 10], "ghost");

// limping pulse: a 3-step pattern of uneven gaps, repeated with human drift
pat = [[1.5, 1.5, 1.0], [2.0, 1.0, 1.0], [1.25, 1.25, 1.5]].choose;
t = 44; i = 0;
while { t < 80 } {
  add.(t, \hit, [\freq, root, \amp, ((t - 44) / 36).linlin(0, 1, 0.31, 0.56) * if(i % 4 == 0) { 1.3 } { 1 },
    \decay, rrand(0.9, 1.6), \dirt, rrand(0.5, 0.7), \pan, rrand(-0.2, 0.2)], "hits");
  t = t + (pat[i % 3] * rrand(0.97, 1.03));
  i = i + 1;
};
t = 46;
while { t < 80 } {
  var kind = [\bell, \plate, \bar].choose;
  var f = pick.(120, 900);
  var n = if(0.25.coin) { rrand(3, 6) } { 1 };   // sometimes a bouncing object
  var gap = rrand(0.25, 0.4), tt = t, a = 0.45;
  n.do {
    add.(tt, \metal, [\freq, f, \amp, a * 0.73, \dur, 0.5, \mode, 0, \drive, rrand(1.0, 3.0),
      \pan, rrand(-0.8, 0.8), \dist, rrand(0.2, 0.8)] ++ metalSpec.(kind, 1), "metal");
    tt = tt + gap; gap = gap * 0.78; a = a * 0.8;
  };
  t = t + exprand(1.5, 5.0);
};
t = 48;
while { t < 80 } {
  add.(t, \metal, [\freq, pick.(150, 600), \amp, 0.28, \dur, rrand(0.6, 2.5), \mode, 1,
    \pan, rrand(-0.9, 0.9), \dist, rrand(0.2, 0.6), \drive, 2] ++ metalSpec.(\sheet, 0.8), "metal");
  t = t + exprand(3.0, 8.0);
};

// --- FURNACE 1:20-2:10 --------------------------------------------------------
add.(80, \sub, [\freq, root, \amp, 0.6, \dur, 7], "sub");
add.(80, \hit, [\freq, root, \amp, 0.6, \decay, 3, \dirt, 0.9], "hits");
3.do { |k|
  add.(80 + (k * 0.02), \metal, [\freq, pick.(100, 500), \amp, 0.5, \dur, 0.5, \mode, 0, \drive, 3,
    \pan, rrand(-0.8, 0.8), \dist, 0.2] ++ metalSpec.([\bell, \plate].choose, 2), "metal");
};
set.(80, drone1, [\dirt, 0.85, \amp, 0.12]);
set.(80, drone2, [\dirt, 0.8]);
set.(80, master, [\drive, 2.2]);
set.(80, grainer, [\density, 30, \gdur, 0.07, \spread, 0.9, \rev, 0.45]);
add.(82, \noisewall, [\amp, 0.28, \atk, 30, \sus, 17.95, \rel, 0.05, \lo, 150, \hi, 3500], "noise");
add.(84, \detritus, [\amp, 0.4, \density, 20, \dur, 46], "detritus");
[84, 100, 112].do { |tt|
  add.(tt + rrand(0, 2.0), \metal, [\freq, pick.(80, 300), \amp, 0.55, \dur, rrand(12.0, 18.0), \mode, 2,
    \bend, rrand(0.8, 0.9), \drive, 4, \pan, rrand(-0.6, 0.6), \dist, 0.2] ++ metalSpec.([\sheet, \plate].choose, 2), "metal");
};
t = 82; i = 0;
while { t < 129.5 } {
  add.(t, \hit, [\freq, root, \amp, 0.62 * if(i % 4 == 0) { 1.25 } { 1 },
    \decay, rrand(0.8, 1.4), \dirt, rrand(0.8, 0.95), \pan, rrand(-0.3, 0.3)], "hits");
  t = t + (pat[i % 3] * 0.7 * rrand(0.97, 1.03));
  i = i + 1;
};
t = 83;
while { t < 128 } {
  add.(t, \metal, [\freq, pick.(150, 700), \amp, 0.4, \dur, rrand(0.5, 2.0), \mode, 1,
    \pan, rrand(-0.9, 0.9), \dist, rrand(0.1, 0.5), \drive, 3] ++ metalSpec.(\sheet, 0.8), "metal");
  t = t + exprand(1.5, 4.0);
};
[95, 108, 120].do { |tt| add.(tt + rrand(0, 3.0), \sub, [\freq, root, \amp, 0.35, \dur, 5], "sub") };

// --- ASH 2:10-3:00 -------------------------------------------------------------
set.(130, drone1, [\dirt, 0.25, \amp, 0.06]);
set.(130, master, [\drive, 1.6]);
set.(130, grainer, [\density, 3, \gdur, 0.5, \spread, 0.05, \rev, 0.6, \amp, 0.95]);
add.(131, \sub, [\freq, root, \amp, 0.3, \dur, 9], "sub");
set.(140, master, [\wow, 1.2, \hiss, 0.006]);
set.(160, master, [\wow, 2.5, \drive, 2.6, \hiss, 0.008]);
4.do {
  add.(rrand(134.0, 165.0), \metal, [\freq, pick.(200, 800), \amp, 0.25, \dur, 0.5, \mode, 0, \drive, 1,
    \pan, rrand(-0.9, 0.9), \dist, 0.9] ++ metalSpec.(\bell, 3), "metal");
};
add.(140, \ghost, [\freq, root * 3, \amp, 0.5, \atk, 10, \sus, 8, \rel, 14], "ghost");
add.(150, \metal, [\freq, pick.(60, 200), \amp, 0.25, \dur, 18, \mode, 2, \bend, 0.95, \drive, 1.5,
  \pan, rrand(-0.5, 0.5), \dist, 0.7] ++ metalSpec.(\sheet, 3), "metal");
set.(173, master, [\fadeTime, 6, \fade, 0]);

score.add([total, [\c_set, 0, 0]]);
score.sort;

("ASH  seed " ++ seed ++ "  root " ++ root.round(0.01) ++ " Hz  ->  " ++ outPath).postln;
score.recordNRT(outPath ++ ".osc", outPath, nil, 48000, "WAV", "int24", opts, "", total, {
  File.delete(outPath ++ ".osc");
  "done".postln;
  0.exit;
});
)
