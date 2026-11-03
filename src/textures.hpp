#pragma once
// textures.hpp — granular clouds, noise gestures, air and pressure.
//
// The granulator is the "tape studio" of the piece. Everything the physical
// objects play is recorded onto a 12-second loop of tape. The granulator
// then reads tiny fragments (grains, 10-400 ms) from that recent past and
// scatters them: transposed, reversed, sliding in pitch, thrown around the
// stereo field. That way the piece keeps transforming its own material, which
// is the core gesture of electroacoustic music since musique concrete. Freeze
// the tape and the grains hover over a single instant forever.

#include <array>
#include <vector>

#include "acoustic.hpp"
#include "dsp.hpp"

namespace textures {

using namespace dsp;

// Bus with a "tape" send: what goes there is recorded for the granulator.
struct EBus {
  static constexpr int kMax = 64;
  int n = 0;
  float dryL[kMax], dryR[kMax], revL[kMax], revR[kMax], tape[kMax];
  void clear(int frames) {
    n = frames;
    for (int i = 0; i < frames; ++i) dryL[i] = dryR[i] = revL[i] = revR[i] = tape[i] = 0.0f;
  }
  void add(int i, float l, float r, float dry, float rev, float tapeAmt = 0.0f) {
    dryL[i] += l * dry;
    dryR[i] += r * dry;
    revL[i] += l * rev;
    revR[i] += r * rev;
    tape[i] += (l + r) * 0.5f * tapeAmt;
  }
};

// ===========================================================================
// GRANULATOR
// ===========================================================================
class Granulator {
 public:
  struct Params {
    float density = 0.0f;   // grains per second
    float durMs = 80.0f;    // grain length
    float backSec = 1.0f;   // how far into the past grains are taken from
    float spreadSec = 2.0f; // ...plus a random amount up to this
    float jitterSt = 0.0f;  // random detune, semitones
    float reverse = 0.0f;   // probability a grain plays backwards
    float chirp = 0.0f;     // probability a grain slides an octave up/down
    float sine = 0.0f;      // probability a grain is a pure sine from the spectrum ("glitter")
    float amp = 0.0f;
    std::vector<float> ratios{1.0f};  // transpositions to choose from
  };

  void init(float sr, uint64_t seed, float seconds) {
    sr_ = sr;
    rng_ = Rng(seed);
    int n = 1;
    while (n < int(seconds * sr)) n <<= 1;
    tape_.assign(size_t(n), 0.0f);
    mask_ = n - 1;
    for (int i = 0; i < kWin; ++i) win_[size_t(i)] = 0.5f - 0.5f * std::cos(kTwoPi * float(i) / float(kWin - 1));
  }
  void setParams(const Params& p) { p_ = p; }
  void setField(const std::vector<float>& f) { field_ = f; }
  void freeze(bool on) { frozen_ = on; }
  bool frozen() const { return frozen_; }
  void record(float x) {
    if (frozen_) return;
    tape_[size_t(w_)] = x;
    w_ = (w_ + 1) & mask_;
  }
  // Instantly silence (for hard cuts), with a 15 ms fade so nothing clicks.
  void choke() {
    fade_ = 1.0f;
    fading_ = true;
    density_ = 0.0f;
    amp_ = 0.0f;
  }
  // Extra density on top of the scene's (for "grain burst" gestures).
  void setBoost(float grainsPerSecond) { boost_ = grainsPerSecond; }

  void render(EBus& tapeBus, EBus& sineBus) {
    int n = tapeBus.n;
    float k = onePoleCoef(2.5f, sr_ / float(n));
    density_ += (p_.density - density_) * k;
    amp_ += (p_.amp - amp_) * k;
    float fadeK = decayCoef(0.015f, sr_);
    float dens = density_ + boost_;
    for (int i = 0; i < n; ++i) {
      if (fading_) {
        fade_ *= fadeK;
        if (fade_ < 1e-3f) {
          for (auto& g : g_) g.active = false;
          fading_ = false;
          fade_ = 1.0f;
        }
      }
      if (dens > 0.05f && !fading_) {
        untilNext_ -= 1.0f;
        if (untilNext_ <= 0.0f) {
          spawn();
          untilNext_ += sr_ / dens * -std::log(1.0f - rng_.uni() * 0.999f);
        }
      } else if (untilNext_ < 0.0f) {
        untilNext_ = 0.0f;
      }
      for (auto& g : g_) {
        if (!g.active) continue;
        float ph = float(g.age) / float(g.len);
        float w = win_[size_t(ph * float(kWin - 1))];
        float s;
        if (g.sine) {
          s = std::sin(kTwoPi * g.phase);
          g.phase += g.freq / sr_;
          if (g.phase >= 1.0f) g.phase -= 1.0f;
        } else {
          s = readTape(g.pos);
          g.pos += double(lerp(g.r0, g.r1, ph));
        }
        float y = s * w * g.amp * amp_ * (fading_ ? fade_ : 1.0f);
        (g.sine ? sineBus : tapeBus).add(i, y * g.pn.l, y * g.pn.r, 0.75f, 0.5f);
        if (++g.age >= g.len) g.active = false;
      }
    }
  }

 private:
  static constexpr int kWin = 2048;
  struct Grain {
    bool active = false, sine = false;
    double pos = 0.0;
    float r0 = 1, r1 = 1, amp = 0, freq = 0, phase = 0;
    int len = 1, age = 0;
    Pan pn;
  };

  float readTape(double p) const {
    double fl = std::floor(p);
    long long i = (long long)fl;
    float f = float(p - fl);
    float a = tape_[size_t(i & mask_)];
    float b = tape_[size_t((i + 1) & mask_)];
    return a + (b - a) * f;
  }

  void spawn() {
    Grain* g = nullptr;
    for (auto& x : g_)
      if (!x.active) {
        g = &x;
        break;
      }
    if (!g) return;
    g->active = true;
    g->age = 0;
    g->len = std::max(64, int(p_.durMs * 0.001f * sr_ * rng_.range(0.6f, 1.4f)));
    g->pn = pan(rng_.bipolar());
    g->sine = !field_.empty() && rng_.chance(p_.sine);
    if (g->sine) {
      static const float kOct[3] = {1.0f, 2.0f, 4.0f};
      float f = field_[size_t(rng_.irange(0, int(field_.size()) - 1))];
      f *= kOct[rng_.irange(0, 2)];
      while (f > 7000.0f) f *= 0.5f;
      g->freq = f;
      g->phase = 0.0f;
      g->amp = rng_.range(0.02f, 0.06f);
      return;
    }
    float r = p_.ratios[size_t(rng_.irange(0, int(p_.ratios.size()) - 1))];
    r *= cents(p_.jitterSt * 100.0f * rng_.bipolar());
    if (rng_.chance(p_.reverse)) r = -r;
    g->r0 = r;
    g->r1 = rng_.chance(p_.chirp) ? r * (rng_.chance(0.5f) ? 0.5f : 2.0f) : r;
    float maxr = std::max(std::fabs(g->r0), std::fabs(g->r1));
    float span = float(g->len) * maxr + 64.0f;
    float back = (p_.backSec + p_.spreadSec * rng_.uni()) * sr_;
    back = clampf(back, span, float(mask_) - span);
    g->pos = double(w_) - double(back);
    g->amp = rng_.range(0.5f, 1.0f);
  }

  float sr_ = 48000.0f;
  Rng rng_;
  std::vector<float> tape_, field_;
  int mask_ = 0, w_ = 0;
  bool frozen_ = false, fading_ = false;
  float fade_ = 1.0f, untilNext_ = 0.0f, density_ = 0.0f, amp_ = 0.0f, boost_ = 0.0f;
  Params p_;
  std::array<float, kWin> win_{};
  std::array<Grain, 160> g_;
};

// ===========================================================================
// SWEEPS: filtered-noise gestures that move through space: whooshes,
// exhales, things passing by. A band-pass glides between two frequencies
// while the sound travels along a path.
// ===========================================================================
class Sweeps {
 public:
  void init(float sr, uint64_t seed) {
    sr_ = sr;
    rng_ = Rng(seed);
  }
  // shape 0: swell (rise and fall), 1: attack then fade, 2: rise, cut at the end
  void trigger(float f0, float f1, float seconds, float amp, int shape, acoustic::Spatial::Path path) {
    for (auto& v : v_) {
      if (v.active) continue;
      v.active = true;
      v.t = 0.0f;
      v.dur = seconds;
      v.f0 = f0;
      v.f1 = f1;
      v.amp = amp;
      v.shape = shape;
      v.q = rng_.range(1.5f, 5.0f);
      v.sp.set(path, seconds, rng_);
      v.fL.reset();
      v.fR.reset();
      v.env = 0.0f;
      v.kill = false;
      return;
    }
  }
  void choke() {
    for (auto& v : v_) v.kill = true;  // fade out over ~15 ms
  }
  void render(EBus& b) {
    float dt = float(b.n) / sr_;
    for (auto& v : v_) {
      if (!v.active) continue;
      float p = v.t / v.dur;
      if (p >= 1.0f) {
        v.active = false;
        continue;
      }
      float f = v.f0 * std::pow(v.f1 / v.f0, p);
      v.fL.set(f, v.q, sr_);
      v.fR.set(f * 1.03f, v.q, sr_);
      v.sp.block(dt, sr_);
      v.t += dt;
      float pe = std::min(1.0f, v.t / v.dur);
      float target = v.shape == 0   ? std::pow(std::sin(kPi * pe), 2.0f)
                     : v.shape == 1 ? smoothstep(0.0f, 0.03f, pe) * (1.0f - pe) * (1.0f - pe)
                                    : std::pow(pe, 2.5f) * (1.0f - smoothstep(0.97f, 1.0f, pe));  // riser
      target *= v.amp;
      float step = (target - v.env) / float(b.n);
      float killK = decayCoef(0.015f, sr_);
      for (int i = 0; i < b.n; ++i) {
        if (v.kill)
          v.env *= killK;
        else
          v.env += step;
        float l = v.fL.bp(rng_.bipolar()) * v.env;
        float r = v.fR.bp(rng_.bipolar()) * v.env;
        v.sp.apply(l, r);
        b.add(i, l, r, v.sp.dry(), v.sp.wet(), 0.3f);
      }
      if (v.kill && v.env < 1e-4f) v.active = false;
    }
  }

 private:
  struct Voice {
    bool active = false, kill = false;
    float t = 0, dur = 1, f0 = 100, f1 = 1000, amp = 0, q = 2, env = 0;
    int shape = 0;
    acoustic::Spatial sp;
    Svf fL, fR;
  };
  float sr_ = 48000.0f;
  Rng rng_;
  std::array<Voice, 6> v_;
};

// ===========================================================================
// AIR: noise through two feedback comb filters tuned to the scene's
// spectrum. A comb filter turns hiss into a pitched, hollow, tube-like
// whisper; slowly modulating its delay makes it breathe and flange.
// ===========================================================================
class Air {
 public:
  void init(float sr, uint64_t seed) {
    sr_ = sr;
    rng_ = Rng(seed);
    dL_.init(int(sr / 40.0f) + 16);
    dR_.init(int(sr / 40.0f) + 16);
    modL_.init(mixSeed(seed, 1), 3.0f, 9.0f);
    modR_.init(mixSeed(seed, 2), 3.0f, 9.0f);
    hpL_.setCutoff(150.0f, sr);
    hpR_.setCutoff(150.0f, sr);
  }
  void setPitches(float a, float b) {
    tL_ = sr_ / clampf(a, 60.0f, 2000.0f);
    tR_ = sr_ / clampf(b, 60.0f, 2000.0f);
  }
  void setLevel(float level, bool now = false) {
    target_ = level;
    if (now) level_ = level;
  }
  void render(EBus& b) {
    float dt = float(b.n) / sr_;
    level_ += (target_ - level_) * onePoleCoef(4.0f, sr_ / float(b.n));
    float k = onePoleCoef(3.0f, sr_ / float(b.n));
    cL_ += (tL_ - cL_) * k;
    cR_ += (tR_ - cR_) * k;
    float mL = cL_ * (1.0f + 0.01f * modL_.advance(dt));
    float mR = cR_ * (1.0f + 0.01f * modR_.advance(dt));
    for (int i = 0; i < b.n; ++i) {
      float yl = dL_.tapf(mL), yr = dR_.tapf(mR);
      dL_.write(pL_.process(rng_) * 0.25f + yl * 0.93f);
      dR_.write(pR_.process(rng_) * 0.25f + yr * 0.93f);
      float l = hpL_.hp(yl) * level_ * 0.3f, r = hpR_.hp(yr) * level_ * 0.3f;
      b.add(i, l, r, 0.7f, 0.5f);
    }
  }

 private:
  float sr_ = 48000.0f, tL_ = 400, tR_ = 300, cL_ = 400, cR_ = 300, level_ = 0, target_ = 0;
  Rng rng_;
  PinkNoise pL_, pR_;
  DelayLine dL_, dR_;
  Drift modL_, modR_;
  OnePole hpL_, hpR_;
};

// ===========================================================================
// SUB: air pressure more than a note. A very low sine plus rumbling brown
// noise, swelling slowly. Felt in the chest on good speakers, invisible on
// small ones, and it makes silence feel heavy instead of empty.
// ===========================================================================
class Sub {
 public:
  void init(float sr, uint64_t seed) {
    sr_ = sr;
    rng_ = Rng(seed);
    swell_.init(mixSeed(seed, 1), 6.0f, 16.0f);
    lp_.setCutoff(55.0f, sr);
  }
  void setPitch(float hz) { target_ = hz; }
  void setLevel(float level, bool now = false) {
    levelTarget_ = level;
    if (now) level_ = level;
  }
  void render(EBus& b) {
    float dt = float(b.n) / sr_;
    level_ += (levelTarget_ - level_) * onePoleCoef(5.0f, sr_ / float(b.n));
    hz_ += (target_ - hz_) * onePoleCoef(4.0f, sr_ / float(b.n));
    float g = level_ * (0.55f + 0.45f * swell_.advance(dt));
    for (int i = 0; i < b.n; ++i) {
      brown_ = brown_ * 0.998f + rng_.bipolar() * 0.04f;
      float y = (std::tanh(1.5f * osc_.process(hz_, sr_)) * 0.2f + lp_.lp(brown_) * 0.7f) * g;
      b.add(i, y, y, 1.0f, 0.0f);
    }
  }

 private:
  float sr_ = 48000.0f, hz_ = 35, target_ = 35, level_ = 0, levelTarget_ = 0, brown_ = 0;
  Rng rng_;
  Sine osc_;
  Drift swell_;
  OnePole lp_;
};

}  // namespace textures
