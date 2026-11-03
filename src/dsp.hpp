#pragma once
// dsp.hpp — the small toolbox every sound in the piece is built from.
//
// Nothing in here is "musical" yet: oscillators, filters, envelopes, noise,
// delay lines and a deterministic random generator. layers.hpp is where
// these get combined into actual sounds.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace dsp {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

inline float mtof(float midi) { return 440.0f * std::exp2((midi - 69.0f) / 12.0f); }
inline float cents(float c) { return std::exp2(c / 1200.0f); }
inline float clampf(float x, float lo, float hi) { return std::min(hi, std::max(lo, x)); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstep(float e0, float e1, float x) {
  float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}
// One-pole smoothing coefficient: covers ~63% of a step after `seconds`
// when called `rate` times per second.
inline float onePoleCoef(float seconds, float rate) {
  return seconds <= 0.0f ? 1.0f : 1.0f - std::exp(-1.0f / (seconds * rate));
}
// Per-sample multiplier for an exponential decay reaching -60 dB after t60 s.
inline float decayCoef(float t60, float sr) { return std::exp(-6.9077553f / (t60 * sr)); }

struct Pan {
  float l = 0.70710678f, r = 0.70710678f;
};
// Constant-power pan law, p in [-1 (left) .. +1 (right)]. Keeps loudness
// constant as a sound moves across the stereo field.
inline Pan pan(float p) {
  float a = (clampf(p, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi;
  return {std::cos(a), std::sin(a)};
}

// ---------------------------------------------------------------------------
// Random numbers. Hand-rolled (splitmix64) instead of <random> so that the
// same seed produces the same piece on every compiler and platform.
// ---------------------------------------------------------------------------
class Rng {
 public:
  explicit Rng(uint64_t seed = 1) : s_(seed) {}
  uint64_t next() {
    uint64_t z = (s_ += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }
  float uni() { return float(next() >> 40) * (1.0f / 16777216.0f); }  // [0, 1)
  float range(float a, float b) { return a + (b - a) * uni(); }
  float bipolar() { return uni() * 2.0f - 1.0f; }
  int irange(int a, int b) { return a + int(next() % uint64_t(b - a + 1)); }  // inclusive
  bool chance(float p) { return uni() < p; }

 private:
  uint64_t s_;
};

// Derive independent sub-seeds so every layer gets its own random stream.
inline uint64_t mixSeed(uint64_t a, uint64_t b) {
  Rng r(a ^ (b * 0xD1B54A32D192ED03ull));
  return r.next();
}

// ---------------------------------------------------------------------------
// Oscillators
// ---------------------------------------------------------------------------

// PolyBLEP smooths the saw's hard reset so it doesn't alias. Without it,
// high notes get a harsh, metallic "digital" fizz.
inline float polyBlep(float t, float dt) {
  if (t < dt) {
    t /= dt;
    return t + t - t * t - 1.0f;
  }
  if (t > 1.0f - dt) {
    t = (t - 1.0f) / dt;
    return t * t + t + t + 1.0f;
  }
  return 0.0f;
}

class Saw {
 public:
  void setPhase(float p) { ph_ = p; }
  float process(float freq, float sr) {
    float dt = freq / sr;
    float y = 2.0f * ph_ - 1.0f - polyBlep(ph_, dt);
    ph_ += dt;
    if (ph_ >= 1.0f) ph_ -= 1.0f;
    return y;
  }

 private:
  float ph_ = 0.0f;
};

class Sine {
 public:
  void setPhase(float p) { ph_ = p; }
  // `pm` is phase modulation in cycles (used for FM bells).
  float process(float freq, float sr, float pm = 0.0f) {
    float y = std::sin(kTwoPi * (ph_ + pm));
    ph_ += freq / sr;
    if (ph_ >= 1.0f) ph_ -= 1.0f;
    return y;
  }

 private:
  float ph_ = 0.0f;
};

// Pink noise (Paul Kellet's economy filter): equal energy per octave, so it
// sounds like wind or surf rather than the hiss of white noise.
class PinkNoise {
 public:
  float process(Rng& rng) {
    float w = rng.bipolar();
    b0_ = 0.99765f * b0_ + w * 0.0990460f;
    b1_ = 0.96300f * b1_ + w * 0.2965164f;
    b2_ = 0.57000f * b2_ + w * 1.0526913f;
    return (b0_ + b1_ + b2_ + w * 0.1848f) * 0.22f;
  }

 private:
  float b0_ = 0, b1_ = 0, b2_ = 0;
};

// ---------------------------------------------------------------------------
// Filters
// ---------------------------------------------------------------------------

// State-variable filter (Cytomic / Andrew Simper "TPT" form). Gives low-,
// band- and high-pass at once, stays stable when the cutoff is swept fast,
// which matters because almost every sound here has a moving filter.
class Svf {
 public:
  struct Out {
    float lp, bp, hp;
  };
  void set(float cutoff, float q, float sr) {
    cutoff = clampf(cutoff, 10.0f, sr * 0.45f);
    g_ = std::tan(kPi * cutoff / sr);
    k_ = 1.0f / q;
    a1_ = 1.0f / (1.0f + g_ * (g_ + k_));
    a2_ = g_ * a1_;
    a3_ = g_ * a2_;
  }
  Out process(float v0) {
    float v3 = v0 - ic2_;
    float v1 = a1_ * ic1_ + a2_ * v3;
    float v2 = ic2_ + a2_ * ic1_ + a3_ * v3;
    ic1_ = 2.0f * v1 - ic1_;
    ic2_ = 2.0f * v2 - ic2_;
    return {v2, v1, v0 - k_ * v1 - v2};
  }
  float lp(float x) { return process(x).lp; }
  float bp(float x) { return process(x).bp * k_; }  // normalised: 0 dB at centre
  float hp(float x) { return process(x).hp; }
  void reset() { ic1_ = ic2_ = 0.0f; }

 private:
  float g_ = 0, k_ = 1, a1_ = 0, a2_ = 0, a3_ = 0, ic1_ = 0, ic2_ = 0;
};

class OnePole {
 public:
  void setCutoff(float hz, float sr) { a_ = 1.0f - std::exp(-kTwoPi * hz / sr); }
  float lp(float x) {
    z_ += a_ * (x - z_);
    return z_;
  }
  float hp(float x) { return x - lp(x); }

 private:
  float a_ = 1.0f, z_ = 0.0f;
};

// ---------------------------------------------------------------------------
// Envelopes & modulation
// ---------------------------------------------------------------------------

// Gate envelope with exponential attack/release. `attack` and `release` are
// roughly the time to get 95% of the way there.
class Env {
 public:
  void set(float attack, float release, float sr) {
    a_ = onePoleCoef(attack / 3.0f, sr);
    r_ = onePoleCoef(release / 3.0f, sr);
  }
  void on() { target_ = 1.0f; }
  void off() { target_ = 0.0f; }
  void reset() { v_ = target_ = 0.0f; }
  float process() {
    v_ += (target_ - v_) * (target_ > v_ ? a_ : r_);
    return v_;
  }
  bool released() const { return target_ == 0.0f; }
  bool idle() const { return target_ == 0.0f && v_ < 1e-4f; }
  float value() const { return v_; }

 private:
  float a_ = 1, r_ = 1, v_ = 0, target_ = 0;
};

// Smooth random wandering in [-1, 1]. Glides to a new random target every
// few seconds. This is the "life" in the piece: nothing static, every filter
// and level slowly breathes, the way a real player never repeats exactly.
class Drift {
 public:
  void init(uint64_t seed, float minPeriod, float maxPeriod) {
    rng_ = Rng(seed);
    minP_ = minPeriod;
    maxP_ = maxPeriod;
    a_ = rng_.bipolar();
    b_ = rng_.bipolar();
    period_ = rng_.range(minP_, maxP_);
    t_ = rng_.uni();
  }
  float advance(float seconds) {
    t_ += seconds / period_;
    while (t_ >= 1.0f) {
      t_ -= 1.0f;
      a_ = b_;
      b_ = rng_.bipolar();
      period_ = rng_.range(minP_, maxP_);
    }
    v_ = lerp(a_, b_, 0.5f - 0.5f * std::cos(kPi * t_));
    return v_;
  }
  float value() const { return v_; }
  float unipolar() const { return 0.5f + 0.5f * v_; }

 private:
  Rng rng_;
  float minP_ = 1, maxP_ = 2, a_ = 0, b_ = 0, period_ = 1, t_ = 0, v_ = 0;
};

// ---------------------------------------------------------------------------
// Delay lines
// ---------------------------------------------------------------------------
class DelayLine {
 public:
  void init(int maxDelay) {
    int n = 1;
    while (n < maxDelay + 2) n <<= 1;
    buf_.assign(size_t(n), 0.0f);
    mask_ = n - 1;
    w_ = 0;
  }
  void write(float x) {
    buf_[size_t(w_)] = x;
    w_ = (w_ + 1) & mask_;
  }
  // Integer delay in samples (1 = the sample written last).
  float tap(int d) const { return buf_[size_t((w_ - d) & mask_)]; }
  // Fractional delay with linear interpolation (for modulated delays).
  float tapf(float d) const {
    float rp = float(w_) - d;
    float fl = std::floor(rp);
    int i = int(fl);
    float f = rp - fl;
    float a = buf_[size_t(i & mask_)];
    float b = buf_[size_t((i + 1) & mask_)];
    return a + (b - a) * f;
  }

 private:
  std::vector<float> buf_;
  int mask_ = 0, w_ = 0;
};

// Schroeder allpass: smears transients into a dense cloud without colouring
// the tone. Used in front of the reverb.
class Allpass {
 public:
  void init(int len, float g) {
    d_.init(len);
    len_ = len;
    g_ = g;
  }
  float process(float x) {
    float z = d_.tap(len_);
    float w = x + g_ * z;
    d_.write(w);
    return z - g_ * w;
  }

 private:
  DelayLine d_;
  int len_ = 1;
  float g_ = 0.5f;
};

}  // namespace dsp
