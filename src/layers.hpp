#pragma once
// layers.hpp — the instruments.
//
// Each layer is one recipe. Read them as patch notes: every one of them is
// something you can rebuild in Vital / Serum / your DAW's stock synth with
// the same settings. The comments say *why* each choice is made.
//
// Every layer writes into a Bus with three destinations:
//   dry   - straight to the speakers
//   rev   - send to the big reverb
//   dly   - send to the ping-pong echo
// Choosing how much of each sound goes dry vs. into the room is one of the
// biggest "why does mine sound flat?" fixes in sound design.

#include <array>
#include <vector>

#include "dsp.hpp"

namespace layers {

using namespace dsp;

// What the conductor tells every instrument, once per block.
struct Context {
  float sr = 48000.0f;
  double time = 0.0;
  float intensity = 0.0f;  // 0..1, smoothed: the master "tension" knob
  float choir = 0.0f;      // 0..1, how present the choir is
  float ostinato = 0.0f;   // 0..1, how present the pulse is
  float keyHz = 65.0f;     // the drone's root
};

struct Bus {
  static constexpr int kMax = 64;
  int n = 0;
  float dryL[kMax], dryR[kMax], revL[kMax], revR[kMax], dlyL[kMax], dlyR[kMax];
  void clear(int frames) {
    n = frames;
    for (int i = 0; i < frames; ++i) dryL[i] = dryR[i] = revL[i] = revR[i] = dlyL[i] = dlyR[i] = 0.0f;
  }
  void add(int i, float l, float r, float dry, float rev, float dly = 0.0f) {
    dryL[i] += l * dry;
    dryR[i] += r * dry;
    revL[i] += l * rev;
    revR[i] += r * rev;
    dlyL[i] += l * dly;
    dlyR[i] += r * dly;
  }
};

// ===========================================================================
// DRONE: the floor everything stands on.
//   * a sine one octave below the root = pure sub. Sent dry, never into the
//     reverb (reverb on sub = mud).
//   * tanh() saturation on the sub adds a quiet 3rd harmonic so the bass is
//     still "heard" on laptop speakers and phones.
//   * two saws detuned -6/+6 cents, one per ear: the slow beating between
//     them makes the drone feel alive and wide.
//   * a fifth on top, quieter, for weight.
//   * a low-pass that opens with intensity and drifts on its own: the drone
//     "breathes" instead of sitting still.
// ===========================================================================
class Drone {
 public:
  void init(float sr, uint64_t seed, float rootHz) {
    sr_ = sr;
    hz_ = target_ = rootHz;
    cut_.init(mixSeed(seed, 1), 8.0f, 20.0f);
    amp_.init(mixSeed(seed, 2), 6.0f, 14.0f);
    Rng r(seed);
    sA_.setPhase(r.uni());
    sB_.setPhase(r.uni());
    sC_.setPhase(r.uni());
    sD_.setPhase(r.uni());
  }
  void setRoot(float hz) { target_ = hz; }

  void render(const Context& c, Bus& b) {
    float dt = float(b.n) / sr_;
    hz_ += (target_ - hz_) * onePoleCoef(6.0f, sr_ / float(b.n));  // slow glide on key changes
    float cd = cut_.advance(dt), ad = amp_.advance(dt);
    float cutoff = (90.0f + 420.0f * std::pow(c.intensity, 1.4f)) * (1.0f + 0.35f * cd);
    fL_.set(cutoff, 0.9f, sr_);
    fR_.set(cutoff * 1.07f, 0.9f, sr_);
    float level = (0.8f + 0.2f * ad) * (0.4f + 0.6f * c.intensity);
    float fA = hz_ * cents(-6.0f), fB = hz_ * cents(6.0f);
    float fC = hz_ * 1.5f * cents(3.0f), fD = hz_ * 1.5f * cents(-3.0f);
    for (int i = 0; i < b.n; ++i) {
      float sub = std::tanh(1.6f * sub_.process(hz_ * 0.5f, sr_)) * 0.16f * level;
      float l = fL_.lp(sA_.process(fA, sr_) + 0.45f * sC_.process(fC, sr_)) * 0.07f * level;
      float r = fR_.lp(sB_.process(fB, sr_) + 0.45f * sD_.process(fD, sr_)) * 0.07f * level;
      b.add(i, sub, sub, 1.0f, 0.0f);
      b.add(i, l, r, 0.8f, 0.35f);
    }
  }

 private:
  float sr_ = 48000.0f, hz_ = 65.0f, target_ = 65.0f;
  Sine sub_;
  Saw sA_, sB_, sC_, sD_;
  Svf fL_, fR_;
  Drift cut_, amp_;
};

// ===========================================================================
// PAD: warm, dark string-like chords.
//   * 3 saws per note: one left, one centre, one right, each a few cents
//     apart. That spread is what makes a pad "wide".
//   * low-pass around 300 Hz when quiet, up to ~2.5 kHz at the climax.
//     Dark sound design is mostly *removing* the highs.
//   * key tracking: higher notes get a slightly brighter filter so the top
//     of the chord doesn't vanish.
//   * slow attack (4-7 s) and release (6-9 s): chords crossfade into each
//     other instead of switching. That is the cinematic swell.
//   * a few cents of slow random pitch wobble, like old tape.
// ===========================================================================
class Pad {
 public:
  void init(float sr, uint64_t seed) {
    sr_ = sr;
    rng_ = Rng(seed);
  }
  void setChord(const std::vector<int>& notes) {
    for (auto& v : v_)
      if (v.active) v.env.off();
    for (int n : notes) freeVoice().start(n, sr_, rng_);
  }

  void render(const Context& c, Bus& b) {
    float dt = float(b.n) / sr_;
    float lvl = 0.05f * (0.3f + 0.9f * c.intensity);
    float bright = 260.0f + 2100.0f * std::pow(c.intensity, 1.5f);
    for (auto& v : v_) {
      if (!v.active) continue;
      if (v.env.idle()) {
        v.active = false;
        continue;
      }
      float d = v.drift.advance(dt);
      float cutoff = bright * std::pow(v.hz / 130.0f, 0.3f) * (1.0f + 0.25f * d);
      v.fL.set(cutoff, 0.65f, sr_);
      v.fR.set(cutoff * 1.05f, 0.65f, sr_);
      float f[3];
      for (int k = 0; k < 3; ++k) f[k] = v.hz * v.det[k] * cents(4.0f * d);
      for (int i = 0; i < b.n; ++i) {
        float e = v.env.process() * lvl;
        float l = 0.0f, r = 0.0f;
        for (int k = 0; k < 3; ++k) {
          float s = v.saw[k].process(f[k], sr_);
          l += s * v.pan[k].l;
          r += s * v.pan[k].r;
        }
        b.add(i, v.fL.lp(l) * e, v.fR.lp(r) * e, 0.6f, 0.55f);
      }
    }
  }

 private:
  struct Voice {
    bool active = false;
    float hz = 0.0f, det[3] = {1, 1, 1};
    Saw saw[3];
    Pan pan[3];
    Svf fL, fR;
    Env env;
    Drift drift;
    void start(int note, float sr, Rng& rng) {
      active = true;
      hz = mtof(float(note));
      static const float kPan[3] = {-0.75f, 0.0f, 0.75f};
      static const float kDet[3] = {-8.0f, 0.0f, 8.0f};
      for (int k = 0; k < 3; ++k) {
        saw[k].setPhase(rng.uni());
        det[k] = cents(kDet[k] + rng.range(-2.5f, 2.5f));
        pan[k] = dsp::pan(kPan[k] + rng.range(-0.2f, 0.2f));
      }
      env.reset();
      float att = rng.range(4.0f, 7.0f);
      env.set(att, rng.range(6.0f, 9.0f), sr);
      env.on();
      drift.init(rng.next(), 3.0f, 9.0f);
      fL.reset();
      fR.reset();
    }
  };
  Voice& freeVoice() {
    for (auto& v : v_)
      if (!v.active) return v;
    Voice* best = &v_[0];  // steal the quietest
    for (auto& v : v_)
      if (v.env.value() < best->env.value()) best = &v;
    return *best;
  }
  float sr_ = 48000.0f;
  Rng rng_;
  std::array<Voice, 16> v_;
};

// ===========================================================================
// CHOIR: ghostly "aah / ooh" voices.
// A human voice is a buzzy source (vocal cords ~ a saw wave) shaped by the
// mouth. The mouth is a set of resonances called formants. So:
//   * 2 slightly detuned saws + a little breath noise
//   * through 3 band-pass filters at the formant frequencies of a vowel
//   * the vowel slowly morphs a -> o -> u (the filters glide)
//   * vibrato fades in after the note starts, like a real singer
//   * almost all of it goes to the reverb: distant, not in your face.
// ===========================================================================
class Choir {
 public:
  void init(float sr, uint64_t seed) {
    sr_ = sr;
    rng_ = Rng(seed);
  }
  void setChord(const std::vector<int>& notes) {
    for (auto& v : v_)
      if (v.active) v.env.off();
    for (int n : notes) freeVoice().start(n, sr_, rng_);
  }

  void render(const Context& c, Bus& b) {
    static const float kVowels[3][3] = {
        {800.0f, 1150.0f, 2800.0f},  // a
        {450.0f, 800.0f, 2830.0f},   // o
        {325.0f, 700.0f, 2530.0f},   // u
    };
    static const float kQ[3] = {8.0f, 10.0f, 12.0f};
    static const float kGain[3] = {1.0f, 0.5f, 0.25f};
    float dt = float(b.n) / sr_;
    float lvl = 0.10f * c.choir;
    for (auto& v : v_) {
      if (!v.active) continue;
      if (v.env.idle()) {
        v.active = false;
        continue;
      }
      v.age += dt;
      float x = v.vowel.advance(dt) * 0.5f + 0.5f;  // 0..1
      float pos = x * 2.0f;
      int i0 = std::min(1, int(pos));
      float fr = pos - float(i0);
      for (int k = 0; k < 3; ++k)
        v.f[k].set(lerp(kVowels[i0][k], kVowels[i0 + 1][k], fr), kQ[k], sr_);
      // vibrato depth in "ratio per unit" ~ cents * ln2 / 1200
      float vib = 18.0f * smoothstep(0.5f, 2.5f, v.age) * 0.000577f;
      for (int i = 0; i < b.n; ++i) {
        float m = 1.0f + vib * v.vib.process(v.vibHz, sr_);
        float src = v.a.process(v.hz * m, sr_) + v.b.process(v.hz * 1.004f * m, sr_) +
                    0.15f * rng_.bipolar();
        float y = 0.0f;
        for (int k = 0; k < 3; ++k) y += v.f[k].bp(src) * kGain[k];
        y *= v.env.process() * lvl;
        b.add(i, y * v.pan.l, y * v.pan.r, 0.3f, 0.9f);
      }
    }
  }

 private:
  struct Voice {
    bool active = false;
    float hz = 0.0f, vibHz = 5.0f, age = 0.0f;
    Saw a, b;
    Sine vib;
    Svf f[3];
    Env env;
    Drift vowel;
    Pan pan;
    void start(int note, float sr, Rng& rng) {
      active = true;
      hz = mtof(float(note));
      age = 0.0f;
      vibHz = rng.range(4.6f, 5.6f);
      a.setPhase(rng.uni());
      b.setPhase(rng.uni());
      env.reset();
      float att = rng.range(3.0f, 5.0f);
      env.set(att, rng.range(5.0f, 7.0f), sr);
      env.on();
      vowel.init(rng.next(), 4.0f, 10.0f);
      pan = dsp::pan(rng.range(-0.6f, 0.6f));
    }
  };
  Voice& freeVoice() {
    for (auto& v : v_)
      if (!v.active) return v;
    Voice* best = &v_[0];
    for (auto& v : v_)
      if (v.env.value() < best->env.value()) best = &v;
    return *best;
  }
  float sr_ = 48000.0f;
  Rng rng_;
  std::array<Voice, 8> v_;
};

// ===========================================================================
// BRAAM: the huge low brass-like blast of trailers.
//   * root + octave + fifth-above-octave, low in the register.
//   * 3 detuned saws per note, spread left / centre / right.
//   * DISTORTION BEFORE THE FILTER: tanh() drive creates dense harmonics,
//     then the low-pass sweeps through them. That sweep (filter envelope:
//     fast open, ~2 s close) with a bit of resonance is what reads as "brass".
//   * sometimes a slow 1-2 s swell instead of a hit.
// ===========================================================================
class Braam {
 public:
  void init(float sr, uint64_t seed) {
    sr_ = sr;
    rng_ = Rng(seed);
  }
  void trigger(int rootMidi, float strength, float attack) {
    Voice* v = &v_[0];
    for (auto& x : v_)
      if (!x.active) {
        v = &x;
        break;
      }
    static const int kIv[3] = {0, 12, 19};
    static const float kW[3] = {1.0f, 0.8f, 0.5f};
    static const float kDet[3] = {-11.0f, 0.0f, 11.0f};
    v->active = true;
    for (int n = 0; n < 3; ++n) {
      v->hz[n] = mtof(float(rootMidi + kIv[n]));
      v->w[n] = kW[n];
      for (int k = 0; k < 3; ++k) {
        v->saw[n][k].setPhase(rng_.uni());
        v->det[n][k] = cents(kDet[k] + rng_.range(-3.0f, 3.0f));
      }
    }
    v->rise = 1.0f;
    v->riseK = std::exp(-1.0f / (attack * sr_ / 3.0f));
    v->amp = 0.3f * strength;
    v->ampK = decayCoef(rng_.range(6.0f, 9.0f), sr_);
    v->fenv = 1.0f;
    v->fenvK = decayCoef(rng_.range(2.5f, 4.0f), sr_);
    v->fattack = attack;
    v->bright = 0.6f + 0.4f * strength;
    v->t = 0.0f;
    v->fL.reset();
    v->fR.reset();
  }

  void render(const Context&, Bus& b) {
    for (auto& v : v_) {
      if (!v.active) continue;
      // filter opens over the attack, then closes slowly
      float open = smoothstep(0.0f, v.fattack + 0.05f, v.t);
      float cutoff = 60.0f + (250.0f + 3200.0f * v.bright) * v.fenv * open;
      v.fL.set(cutoff, 1.4f, sr_);
      v.fR.set(cutoff * 1.04f, 1.4f, sr_);
      v.t += float(b.n) / sr_;
      for (int i = 0; i < b.n; ++i) {
        float l = 0.0f, r = 0.0f;
        for (int n = 0; n < 3; ++n) {
          float s0 = v.saw[n][0].process(v.hz[n] * v.det[n][0], sr_);
          float s1 = v.saw[n][1].process(v.hz[n] * v.det[n][1], sr_);
          float s2 = v.saw[n][2].process(v.hz[n] * v.det[n][2], sr_);
          l += (s0 + 0.6f * s1) * v.w[n];
          r += (s2 + 0.6f * s1) * v.w[n];
        }
        v.rise *= v.riseK;
        float a = (1.0f - v.rise) * v.amp;
        v.amp *= v.ampK;
        v.fenv *= v.fenvK;
        l = v.fL.lp(std::tanh(l * 0.9f)) * a;
        r = v.fR.lp(std::tanh(r * 0.9f)) * a;
        b.add(i, l, r, 0.85f, 0.5f);
      }
      if (v.amp < 1e-4f) v.active = false;
    }
  }

 private:
  struct Voice {
    bool active = false;
    float hz[3] = {}, w[3] = {}, det[3][3] = {};
    Saw saw[3][3];
    Svf fL, fR;
    float rise = 0, riseK = 0, amp = 0, ampK = 0, fenv = 0, fenvK = 0, fattack = 0, bright = 0, t = 0;
  };
  float sr_ = 48000.0f;
  Rng rng_;
  std::array<Voice, 3> v_;
};

// ===========================================================================
// IMPACTS: booms, taiko-style drums, heartbeat. One engine, three presets
// (see Soundscape::boom/taiko/heartbeat).
//   * a sine whose pitch drops fast (e.g. 150 Hz -> 40 Hz in ~60 ms): this
//     is exactly how an 808 / sub kick / cinematic boom works.
//   * the boom's tail is TUNED TO THE KEY (it lands on the drone's root),
//     so hits feel like part of the music instead of sound effects.
//   * a short burst of filtered noise on top = the "skin" / transient.
//   * tanh() glues body and click together and adds punch.
// ===========================================================================
struct ImpactParams {
  float f0 = 150, f1 = 40;   // pitch start / end (Hz)
  float pitchT = 0.06f;      // pitch-drop time constant (s)
  float t60 = 2.0f;          // body decay
  float noise = 0.5f;        // transient amount
  float noiseT60 = 0.08f;
  float noiseCut = 2500.0f;  // transient brightness
  float amp = 0.5f;
  float pan = 0.0f;
  float rev = 0.4f;          // reverb send
};

class Impacts {
 public:
  void init(float sr, uint64_t seed) {
    sr_ = sr;
    rng_ = Rng(seed);
  }
  void trigger(const ImpactParams& p, int wait) {
    Voice* v = &v_[0];
    for (auto& x : v_) {
      if (!x.active) {
        v = &x;
        break;
      }
      if (x.ae < v->ae) v = &x;
    }
    v->active = true;
    v->wait = wait;
    v->p = p;
    v->pe = 1.0f;
    v->peK = std::exp(-1.0f / (p.pitchT * sr_));
    v->ae = 1.0f;
    v->aeK = decayCoef(p.t60, sr_);
    v->ne = 1.0f;
    v->neK = decayCoef(p.noiseT60, sr_);
    v->nf.reset();
    v->nf.set(p.noiseCut, 0.7f, sr_);
    v->osc.setPhase(0.0f);  // always start at zero crossing: no click
    v->pn = pan(p.pan);
  }
  void render(const Context&, Bus& b) {
    for (auto& v : v_) {
      if (!v.active) continue;
      for (int i = 0; i < b.n; ++i) {
        if (v.wait > 0) {
          --v.wait;
          continue;
        }
        float hz = v.p.f1 + (v.p.f0 - v.p.f1) * v.pe;
        v.pe *= v.peK;
        float body = v.osc.process(hz, sr_) * v.ae;
        v.ae *= v.aeK;
        float n = v.nf.lp(rng_.bipolar()) * v.ne * v.p.noise;
        v.ne *= v.neK;
        float y = std::tanh(1.4f * (body + n)) * v.p.amp;
        b.add(i, y * v.pn.l, y * v.pn.r, 1.0f, v.p.rev);
      }
      if (v.wait <= 0 && v.ae < 1e-4f && v.ne < 1e-4f) v.active = false;
    }
  }

 private:
  struct Voice {
    bool active = false;
    int wait = 0;
    ImpactParams p;
    Sine osc;
    Svf nf;
    Pan pn;
    float pe = 0, peK = 0, ae = 0, aeK = 0, ne = 0, neK = 0;
  };
  float sr_ = 48000.0f;
  Rng rng_;
  std::array<Voice, 10> v_;
};

// ===========================================================================
// RISERS: tension before a hit.
//   * pink noise through a band-pass that sweeps 180 Hz -> 6 kHz while the
//     resonance increases, getting louder on an exponential curve.
//   * independent noise per ear = very wide.
//   * it stops dead the instant the impact lands. The contrast is the trick.
// ===========================================================================
class Risers {
 public:
  void init(float sr, uint64_t seed) {
    sr_ = sr;
    rng_ = Rng(seed);
  }
  void trigger(float length, float level) {
    for (auto& v : v_) {
      if (v.active) continue;
      v.active = true;
      v.t = 0.0f;
      v.len = length;
      v.level = level;
      v.amp = 0.0f;
      v.fL.reset();
      v.fR.reset();
      return;
    }
  }
  void render(const Context&, Bus& b) {
    float dt = float(b.n) / sr_;
    for (auto& v : v_) {
      if (!v.active) continue;
      float p = v.t / v.len;
      if (p >= 1.0f) {
        v.active = false;
        continue;
      }
      float center = 180.0f * std::pow(35.0f, p);
      float q = 0.9f + 2.5f * p;
      v.fL.set(center, q, sr_);
      v.fR.set(center * 1.06f, q, sr_);
      v.t += dt;
      float pn = std::min(1.0f, v.t / v.len);
      float target = v.level * std::pow(pn, 2.5f) * (1.0f - smoothstep(0.985f, 1.0f, pn));
      float step = (target - v.amp) / float(b.n);
      for (int i = 0; i < b.n; ++i) {
        v.amp += step;
        float l = v.fL.bp(v.nL.process(rng_)) * v.amp;
        float r = v.fR.bp(v.nR.process(rng_)) * v.amp;
        b.add(i, l, r, 0.7f, 0.6f);
      }
    }
  }

 private:
  struct Voice {
    bool active = false;
    float t = 0, len = 1, level = 0, amp = 0;
    Svf fL, fR;
    PinkNoise nL, nR;
  };
  float sr_ = 48000.0f;
  Rng rng_;
  std::array<Voice, 3> v_;
};

// ===========================================================================
// BELLS: cold, metallic music-box tones (FM synthesis).
//   * a sine (carrier) whose phase is wobbled by another sine (modulator).
//   * NON-INTEGER frequency ratios (3.5, 1.41, 2.76...) give inharmonic,
//     bell-like partials. Integer ratios would sound like an organ instead.
//   * the modulation depth decays faster than the volume: bright "ting"
//     at the start, pure tone in the tail. That's how real bells behave.
//   * a second carrier 2 cents sharp makes the tail shimmer.
//   * mostly sent to echo + reverb: they should sound far away.
// ===========================================================================
class Bells {
 public:
  void init(float sr, uint64_t seed) {
    sr_ = sr;
    rng_ = Rng(seed);
  }
  void trigger(int midi, float vel, float t60, float ratio, float index, float panPos, int wait) {
    Voice* v = &v_[0];
    for (auto& x : v_) {
      if (!x.active) {
        v = &x;
        break;
      }
      if (x.amp < v->amp) v = &x;
    }
    v->active = true;
    v->wait = wait;
    v->hz = mtof(float(midi));
    v->ratio = ratio;
    v->idx = index;
    v->idxK = decayCoef(t60 * 0.25f, sr_);
    v->amp = vel;
    v->ampK = decayCoef(t60, sr_);
    v->att = 0.0f;
    v->car.setPhase(0.0f);
    v->car2.setPhase(rng_.uni());
    v->mod.setPhase(0.0f);
    v->pn = pan(panPos);
  }
  void render(const Context&, Bus& b) {
    float attK = onePoleCoef(0.002f, sr_);
    for (auto& v : v_) {
      if (!v.active) continue;
      for (int i = 0; i < b.n; ++i) {
        if (v.wait > 0) {
          --v.wait;
          continue;
        }
        float m = v.mod.process(v.hz * v.ratio, sr_) * v.idx * 0.159155f;
        v.idx *= v.idxK;
        float y = v.car.process(v.hz, sr_, m) + 0.6f * v.car2.process(v.hz * 1.0012f, sr_, m);
        v.att += (1.0f - v.att) * attK;
        y *= v.amp * v.att;
        v.amp *= v.ampK;
        b.add(i, y * v.pn.l, y * v.pn.r, 0.35f, 0.75f, 0.5f);
      }
      if (v.wait <= 0 && v.amp < 1e-4f) v.active = false;
    }
  }

 private:
  struct Voice {
    bool active = false;
    int wait = 0;
    float hz = 0, ratio = 1, idx = 0, idxK = 0, amp = 0, ampK = 0, att = 0;
    Sine car, car2, mod;
    Pan pn;
  };
  float sr_ = 48000.0f;
  Rng rng_;
  std::array<Voice, 16> v_;
};

// ===========================================================================
// WIND: the air in the room.
//   * pink noise, separate per ear, through wandering band-pass filters.
//   * a second, very narrow (high-Q) band that occasionally swells: this
//     produces those eerie whistling howls.
//   * louder when the music is quiet, so the space never feels empty.
// ===========================================================================
class Wind {
 public:
  void init(float sr, uint64_t seed) {
    sr_ = sr;
    rng_ = Rng(seed);
    cL_.init(mixSeed(seed, 1), 4.0f, 11.0f);
    cR_.init(mixSeed(seed, 2), 4.0f, 11.0f);
    q_.init(mixSeed(seed, 3), 5.0f, 12.0f);
    amp_.init(mixSeed(seed, 4), 3.0f, 9.0f);
    whF_.init(mixSeed(seed, 5), 6.0f, 15.0f);
    whA_.init(mixSeed(seed, 6), 5.0f, 13.0f);
  }
  void render(const Context& c, Bus& b) {
    float dt = float(b.n) / sr_;
    float q = 0.7f + 0.8f * (q_.advance(dt) * 0.5f + 0.5f);
    bL_.set(180.0f * std::pow(8.0f, cL_.advance(dt) * 0.5f + 0.5f), q, sr_);
    bR_.set(180.0f * std::pow(8.0f, cR_.advance(dt) * 0.5f + 0.5f), q, sr_);
    float wf = 500.0f * std::pow(4.0f, whF_.advance(dt) * 0.5f + 0.5f);
    wL_.set(wf, 25.0f, sr_);
    wR_.set(wf * 1.01f, 25.0f, sr_);
    float wh = std::max(0.0f, whA_.advance(dt));
    wh = wh * wh * 0.9f;
    float level = 0.26f * (1.15f - 0.75f * c.intensity) * (0.6f + 0.4f * amp_.advance(dt));
    for (int i = 0; i < b.n; ++i) {
      float nl = pL_.process(rng_), nr = pR_.process(rng_);
      float l = (bL_.bp(nl) + wL_.bp(nl) * wh) * level;
      float r = (bR_.bp(nr) + wR_.bp(nr) * wh) * level;
      b.add(i, l, r, 0.7f, 0.4f);
    }
  }

 private:
  float sr_ = 48000.0f;
  Rng rng_;
  PinkNoise pL_, pR_;
  Svf bL_, bR_, wL_, wR_;
  Drift cL_, cR_, q_, amp_, whF_, whA_;
};

// ===========================================================================
// OSTINATO: the restless 16th-note pulse that drives the tense sections.
//   * plucked bass-synth: 2 detuned saws through a low-pass with its own
//     fast envelope ("filter pluck") - the cutoff jumps up on each note and
//     snaps shut.
//   * accented notes open the filter further: groove comes from dynamics,
//     not just from which notes play.
//   * notes alternate slightly left/right, which makes the rhythm "move".
//   * a little tempo-synced echo fills the gaps between notes.
// ===========================================================================
class Ostinato {
 public:
  void init(float sr, uint64_t seed) {
    sr_ = sr;
    rng_ = Rng(seed);
    aK_ = decayCoef(0.35f, sr);
    fK_ = decayCoef(0.22f, sr);
    newPattern();
  }
  void setTones(const std::vector<int>& tones) { tones_ = tones; }
  void newPattern() {
    // Accents on a 3-3-2 / 3-3-3-3-2-2 grid: the classic restless film pulse.
    static const bool kAcc[2][16] = {
        {1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0},
        {1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0},
    };
    int a = rng_.irange(0, 1);
    for (int s = 0; s < 16; ++s) {
      Step& st = pat_[size_t(s)];
      st.accent = kAcc[a][s];
      st.on = st.accent || rng_.chance(0.8f);
      float r = rng_.uni();
      st.tone = st.accent ? (r < 0.75f ? 0 : 1) : (r < 0.55f ? 0 : r < 0.75f ? 1 : r < 0.9f ? 2 : 3);
    }
  }
  // Called by the conductor on every 16th note; `wait` = sample offset.
  void step(long long index, int wait) {
    const Step& st = pat_[size_t(index & 15)];
    if (!st.on || tones_.empty()) return;
    pending_ = true;
    wait_ = wait;
    pendHz_ = mtof(float(tones_[size_t(st.tone) % tones_.size()]));
    pendVel_ = st.accent ? 1.0f : 0.55f;
  }
  void render(const Context& c, Bus& b) {
    float smK = onePoleCoef(0.002f, sr_);
    float range = 250.0f + 2600.0f * c.intensity;
    float lvl = 0.18f * c.ostinato;
    for (int i = 0; i < b.n; ++i) {
      if (pending_ && wait_-- <= 0) {
        pending_ = false;
        hz_ = pendHz_;
        aenv_ = pendVel_;
        fenv_ = pendVel_;
        side_ = -side_;
        pn_ = pan(0.25f * side_);
      }
      f_.set(120.0f + range * fenv_, 1.1f, sr_);
      float y = f_.lp(sA_.process(hz_, sr_) + sB_.process(hz_ * 1.0035f, sr_));
      g_ += (aenv_ - g_) * smK;  // 2 ms smoothing: no clicks on retrigger
      aenv_ *= aK_;
      fenv_ *= fK_;
      y *= g_ * lvl;
      b.add(i, y * pn_.l, y * pn_.r, 0.8f, 0.22f, 0.25f);
    }
  }

 private:
  struct Step {
    bool on = false, accent = false;
    int tone = 0;
  };
  float sr_ = 48000.0f;
  Rng rng_;
  std::array<Step, 16> pat_;
  std::vector<int> tones_;
  Saw sA_, sB_;
  Svf f_;
  Pan pn_;
  bool pending_ = false;
  int wait_ = 0;
  float pendHz_ = 0, pendVel_ = 0, hz_ = 55, aenv_ = 0, fenv_ = 0, g_ = 0, aK_ = 0, fK_ = 0;
  float side_ = 1.0f;
};

}  // namespace layers
