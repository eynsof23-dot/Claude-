#pragma once
// ritual_instruments.hpp — the instruments of the ritual track.
//
// Everything is synthesised: the "field recordings", the whales and the
// cymbals are models, not samples. Each class is a recipe; the comments
// say what makes it sound the way it does.

#include <array>
#include <vector>

#include "acoustic.hpp"
#include "dsp.hpp"

namespace ritual {

using namespace dsp;

// A block of stereo audio plus two sends: the big dark hall ("big") and a
// small stone room ("room") for the percussion.
struct Lane {
  static constexpr int kMax = 64;
  int n = 0;
  float dl[kMax], dr[kMax], bl[kMax], br[kMax], rl[kMax], rr[kMax];
  void clear(int frames) {
    n = frames;
    for (int i = 0; i < frames; ++i) dl[i] = dr[i] = bl[i] = br[i] = rl[i] = rr[i] = 0.0f;
  }
  void add(int i, float l, float r, float dry, float big, float room = 0.0f) {
    dl[i] += l * dry;
    dr[i] += r * dry;
    bl[i] += l * big;
    br[i] += r * big;
    rl[i] += l * room;
    rr[i] += r * room;
  }
};

// sin() that never returns the tiny negative values sinf(pi) can produce:
// pow() of a negative number with a fractional exponent is NaN.
inline float sinPos(float x) { return std::max(0.0f, std::sin(x)); }

inline std::vector<int> euclid(int k, int n, int rotate) {
  std::vector<int> p(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) p[size_t((i + rotate) % n)] = ((i * k) % n) < k ? 1 : 0;
  return p;
}

// ===========================================================================
// TAPE: asymmetric saturation + wow & flutter.
// A bias offset before tanh() means positive and negative halves clip
// differently. That produces EVEN harmonics (2nd, 4th), which sound warm
// and thick rather than harsh, the signature of driven tape and tubes.
// High frequencies are boosted before the drive and cut after (like tape's
// pre/de-emphasis), so the highs saturate a little more and stay smooth.
// ===========================================================================
class Tape {
 public:
  void init(float sr, uint64_t seed) {
    sr_ = sr;
    dL_.init(int(0.012f * sr));
    dR_.init(int(0.012f * sr));
    wow_.init(seed, 1.5f, 4.0f);
    for (auto* f : {&preL_, &preR_, &postL_, &postR_}) f->setCutoff(3000.0f, sr);
    dcL_.setCutoff(12.0f, sr);
    dcR_.setCutoff(12.0f, sr);
  }
  // drive ~1 (gentle glue) .. 5 (heavy); bias 0..0.4; wobble 0..1
  void set(float drive, float bias, float wobble) {
    d_ = std::max(drive, 0.05f);
    b_ = bias;
    t0_ = std::tanh(d_ * b_);
    // auto-gain: unity for quiet signals, so drive changes the tone and
    // squashes the peaks instead of just making everything louder
    norm_ = 1.0f / (d_ * (1.0f - t0_ * t0_));
    wobble_ = wobble;
  }
  void process(float& l, float& r) {
    if (wobble_ > 0.0f) {
      wobPh_ += 6.3f / sr_;
      if (wobPh_ >= 1.0f) wobPh_ -= 1.0f;
      float w = 0.003f * sr_ + wobble_ * sr_ * 0.0006f * (wowNow_ + 0.35f * std::sin(kTwoPi * wobPh_));
      dL_.write(l);
      dR_.write(r);
      l = dL_.tapf(w);
      r = dR_.tapf(w);
    }
    l = shape(l + 0.6f * preL_.hp(l));
    r = shape(r + 0.6f * preR_.hp(r));
    l = dcL_.hp(l - 0.375f * postL_.hp(l));
    r = dcR_.hp(r - 0.375f * postR_.hp(r));
  }
  void block(float seconds) { wowNow_ = wow_.advance(seconds); }

 private:
  float shape(float x) const { return (std::tanh(d_ * (x + b_)) - t0_) * norm_; }
  float sr_ = 48000, d_ = 1, b_ = 0, t0_ = 0, norm_ = 1, wobble_ = 0, wobPh_ = 0, wowNow_ = 0;
  DelayLine dL_, dR_;
  Drift wow_;
  OnePole preL_, preR_, postL_, postR_, dcL_, dcR_;
};

// ===========================================================================
// ECHO: tempo-synced, cross-feeding left <-> right, with filters and
// saturation INSIDE the feedback loop. Every repeat gets thinner (HP),
// darker (LP) and grittier (tanh), like an old tape echo. Different
// left/right times (3/16 vs 1/4) make the repeats bounce across the room.
// ===========================================================================
class Echo {
 public:
  void init(float sr, float maxSeconds, float hpHz, float lpHz) {
    sr_ = sr;
    L_.init(int(maxSeconds * sr) + 8);
    R_.init(int(maxSeconds * sr) + 8);
    hpL_.setCutoff(hpHz, sr);
    hpR_.setCutoff(hpHz, sr);
    lpL_.setCutoff(lpHz, sr);
    lpR_.setCutoff(lpHz, sr);
  }
  void set(float leftSeconds, float rightSeconds, float feedback) {
    tL_ = leftSeconds * sr_;
    tR_ = rightSeconds * sr_;
    fb_ = feedback;
  }
  void process(float inL, float inR, float& outL, float& outR) {
    float dl = L_.tapf(tL_), dr = R_.tapf(tR_);
    float fl = lpL_.lp(hpL_.hp(dl)) * fb_, fr = lpR_.lp(hpR_.hp(dr)) * fb_;
    L_.write(inL + std::tanh(1.3f * fr) / 1.3f);
    R_.write(inR + std::tanh(1.3f * fl) / 1.3f);
    outL = dl;
    outR = dr;
  }

 private:
  float sr_ = 48000, tL_ = 1000, tR_ = 1000, fb_ = 0.4f;
  DelayLine L_, R_;
  OnePole hpL_, hpR_, lpL_, lpR_;
};

// ===========================================================================
// BOWED CYMBAL, TORTURED.
// A cymbal has hundreds of dense, inharmonic modes; we model 40. A bow
// excites mostly the high ones, so the excitation is noise band-passed
// high, which makes it squeal and sing. "Torture" is three things:
//   * bending: all modes are retuned together by a few percent, as if the
//     cymbal were pressed or bent while bowed. The pitch warps and groans.
//   * chatter: stick-slip friction as a train of tiny jerks (60-240/s), a
//     grinding buzz under the tone.
//   * a moving screech: the noise band sweeps, picking out different
//     clusters of partials.
// ===========================================================================
class BowedCymbal {
 public:
  static constexpr int kModes = 40;

  void init(float sr, uint64_t seed, float f0, float decay) {
    sr_ = sr;
    rng_ = Rng(seed);
    bend_.init(mixSeed(seed, 1), 2.0f, 6.0f);
    press_.init(mixSeed(seed, 2), 1.0f, 4.0f);
    screech_.init(mixSeed(seed, 3), 1.5f, 5.0f);
    chatterRate_.init(mixSeed(seed, 4), 0.7f, 3.0f);
    orbit_ = rng_.uni();
    orbitHz_ = rng_.range(0.02f, 0.05f);
    if (rng_.chance(0.5f)) orbitHz_ = -orbitHz_;
    float norm = 0.0f;
    for (int k = 0; k < kModes; ++k) {
      Mode& m = m_[size_t(k)];
      int base = k / 2;
      float f = f0 * std::pow(float(base + 1), 1.32f) * (1.0f + rng_.range(-0.02f, 0.02f));
      if (k & 1) f *= 1.0f + rng_.range(0.002f, 0.006f);  // doublet
      if (f > sr * 0.42f) f = sr * 0.42f * rng_.range(0.7f, 1.0f);
      m.w = kTwoPi * f / sr;
      float t60 = decay * std::pow(f0 / f, 0.3f) * rng_.range(0.7f, 1.3f);
      m.r = std::exp(-6.9077553f / (t60 * sr));
      float amp = std::pow(1.0f / float(base + 1), 0.4f) * rng_.range(0.5f, 1.0f);
      m.gImp = std::sin(m.w) * amp;
      m.gCont = m.gImp * std::sqrt(2.0f * (1.0f - m.r * m.r));
      Pan p = pan(rng_.range(-0.9f, 0.9f));
      m.l = p.l;
      m.r2 = p.r;
      norm += amp * amp;
    }
    float g = 1.0f / std::sqrt(norm);
    for (auto& m : m_) {
      m.gImp *= g;
      m.gCont *= g;
    }
    noiseBp_.set(4000.0f, 1.2f, sr);
  }
  void setBow(float pressure, float torture) {
    pressTarget_ = pressure;
    torture_ = torture;
  }
  void strike(float vel) { pendingHit_ += vel; }

  void render(Lane& lane, float gain) {
    float dt = float(lane.n) / sr_;
    float tor = torture_;
    float bend = 1.0f + tor * 0.03f * bend_.advance(dt);
    for (auto& m : m_) {
      float w = std::min(m.w * bend, 2.9f);
      m.b1 = 2.0f * m.r * std::cos(w);
      m.b2 = m.r * m.r;
    }
    float pd = press_.advance(dt);
    float sc = screech_.advance(dt) * 0.5f + 0.5f;
    noiseBp_.set(1500.0f * std::pow(6.0f, sc * (0.4f + 0.6f * tor)), 1.0f + 3.0f * tor, sr_);
    float chatterHz = 60.0f + 180.0f * (chatterRate_.advance(dt) * 0.5f + 0.5f);
    orbit_ += orbitHz_ * dt;
    Pan orbit = pan(0.7f * std::sin(kTwoPi * orbit_));
    float oL = orbit.l * 1.41421356f, oR = orbit.r * 1.41421356f;
    float pk = onePoleCoef(0.4f, sr_);
    for (int i = 0; i < lane.n; ++i) {
      press_v_ += (pressTarget_ * (0.75f + 0.25f * pd) - press_v_) * pk;
      float cont = noiseBp_.bp(rng_.bipolar()) * press_v_ * 2.0f;
      float imp = 0.0f;
      chPh_ += chatterHz / sr_;
      if (chPh_ >= 1.0f) {
        chPh_ -= 1.0f;
        imp = press_v_ * tor * rng_.range(0.0f, 0.12f);
      }
      if (pendingHit_ > 0.0f) {
        imp += pendingHit_;
        pendingHit_ = 0.0f;
      }
      float l = 0.0f, r = 0.0f;
      for (auto& m : m_) {
        float y = m.b1 * m.y1 - m.b2 * m.y2 + m.gImp * imp + m.gCont * cont;
        m.y2 = m.y1;
        m.y1 = y;
        l += y * m.l;
        r += y * m.r2;
      }
      l = std::tanh(1.5f * l) / 1.5f * gain * oL;
      r = std::tanh(1.5f * r) / 1.5f * gain * oR;
      lane.add(i, l, r, 0.6f, 0.75f);
    }
  }

 private:
  struct Mode {
    float w = 0, r = 0, b1 = 0, b2 = 0, gImp = 0, gCont = 0, y1 = 0, y2 = 0, l = 0, r2 = 0;
  };
  float sr_ = 48000, pressTarget_ = 0, press_v_ = 0, torture_ = 0, chPh_ = 0, pendingHit_ = 0;
  float orbit_ = 0, orbitHz_ = 0.03f;
  Rng rng_;
  Drift bend_, press_, screech_, chatterRate_;
  Svf noiseBp_;
  std::array<Mode, kModes> m_;
};

// ===========================================================================
// WHALES. A humpback call is a tone with a few harmonics that glides slowly
// over a wide range, with vibrato and sometimes a rough growl (amplitude
// modulation at 20-35 Hz). Under water the highs are gone, so everything
// goes through a low-pass, then far away into the big hall.
//   moan  - low, slow glide
//   cry   - rises and falls in an arch
//   groan - a moan with growl
//   whoop - a quick upward sweep
// ===========================================================================
class Whales {
 public:
  enum Kind { Moan, Cry, Groan, Whoop };
  void init(float sr, uint64_t seed) {
    sr_ = sr;
    rng_ = Rng(seed);
  }
  void call(Kind kind, float rootHz) {
    Voice* v = nullptr;
    for (auto& x : v_)
      if (!x.active) {
        v = &x;
        break;
      }
    if (!v) return;
    v->active = true;
    v->kind = kind;
    v->t = 0.0f;
    v->phase = 0.0f;
    // tuned loosely to the key: start and end on octaves / fifths of the root
    static const float kRatio[4] = {2.0f, 3.0f, 4.0f, 6.0f};
    v->f0 = rootHz * kRatio[rng_.irange(0, 3)];
    v->f0 *= cents(rng_.range(-30.0f, 30.0f));
    switch (kind) {
      case Moan:
        v->dur = rng_.range(2.5f, 5.0f);
        v->f1 = v->f0 * rng_.range(0.6f, 1.5f);
        break;
      case Cry:
        v->dur = rng_.range(1.5f, 3.5f);
        v->f1 = v->f0 * rng_.range(2.0f, 4.0f);
        break;
      case Groan:
        v->dur = rng_.range(2.0f, 4.0f);
        v->f0 *= 0.5f;
        v->f1 = v->f0 * rng_.range(0.7f, 1.2f);
        break;
      case Whoop:
        v->dur = rng_.range(0.8f, 1.6f);
        v->f1 = v->f0 * rng_.range(2.5f, 5.0f);
        break;
    }
    v->vibHz = rng_.range(3.0f, 6.5f);
    v->vibDepth = rng_.range(0.004f, 0.015f);
    v->growlHz = rng_.range(18.0f, 35.0f);
    v->growl = kind == Groan ? rng_.range(0.4f, 0.8f) : rng_.range(0.0f, 0.15f);
    v->amp = rng_.range(0.6f, 1.0f);
    for (int h = 0; h < 5; ++h) v->w[h] = std::pow(rng_.range(0.3f, 0.9f), float(h)) * (h == 0 ? 1.0f : 0.7f);
    v->lp.reset();
    v->lp.set(rng_.range(1200.0f, 2600.0f), 0.8f, sr_);
    v->sp.set(acoustic::Spatial::Drift, v->dur, rng_);
    v->sp.distant(rng_);
  }
  void render(Lane& lane, float gain) {
    float dt = float(lane.n) / sr_;
    for (auto& v : v_) {
      if (!v.active) continue;
      v.sp.block(dt, sr_);
      for (int i = 0; i < lane.n; ++i) {
        float p = v.t / v.dur;
        if (p >= 1.0f) {
          v.active = false;
          break;
        }
        float f;
        switch (v.kind) {
          case Cry:
            f = v.f0 * std::pow(v.f1 / v.f0, sinPos(kPi * p));
            break;
          case Whoop:
            f = v.f0 * std::pow(v.f1 / v.f0, p * p);
            break;
          default:
            f = v.f0 * std::pow(v.f1 / v.f0, smoothstep(0.0f, 1.0f, p));
            break;
        }
        f *= 1.0f + v.vibDepth * std::sin(kTwoPi * v.vibHz * v.t);
        v.phase += f / sr_;
        if (v.phase >= 1.0f) v.phase -= 1.0f;
        float y = 0.0f;
        for (int h = 0; h < 5; ++h) y += v.w[h] * std::sin(kTwoPi * float(h + 1) * v.phase);
        float growl = 1.0f - v.growl * (0.5f + 0.5f * std::sin(kTwoPi * v.growlHz * v.t));
        float env = std::pow(sinPos(kPi * p), 0.6f) * v.amp * growl;
        y = v.lp.lp(y) * env * gain * 0.3f;
        float l = y, r = y;
        v.sp.apply(l, r);
        lane.add(i, l, r, 0.45f, 0.9f);
        v.t += 1.0f / sr_;
      }
    }
  }

 private:
  struct Voice {
    bool active = false;
    Kind kind = Moan;
    float t = 0, dur = 1, phase = 0, f0 = 200, f1 = 200, vibHz = 4, vibDepth = 0.01f;
    float growlHz = 25, growl = 0, amp = 1, w[5] = {};
    Svf lp;
    acoustic::Spatial sp;
  };
  float sr_ = 48000;
  Rng rng_;
  std::array<Voice, 5> v_;
};

// ===========================================================================
// FIELD RECORDINGS (synthesised).
//   wind    - pink noise through wandering band-passes, one per ear
//   creek   - hundreds of tiny "bubbles": a sine that rises in pitch as it
//             decays in a few ms. Water burbling is literally this
//             (Minnaert resonance of bubbles).
//   insects - crickets: a ~4.5 kHz tone switched on and off in pulse trains
//   drips   - a single bubble now and then, into the cave reverb
//   rumble  - distant thunder: brown noise below 90 Hz, swelling slowly
// ===========================================================================
class Field {
 public:
  void init(float sr, uint64_t seed) {
    sr_ = sr;
    rng_ = Rng(seed);
    cL_.init(mixSeed(seed, 1), 4.0f, 11.0f);
    cR_.init(mixSeed(seed, 2), 4.0f, 11.0f);
    gust_.init(mixSeed(seed, 3), 3.0f, 9.0f);
    rumbleSwell_.init(mixSeed(seed, 4), 5.0f, 12.0f);
    creekLp_.setCutoff(6000.0f, sr);
    rumbleLp_.setCutoff(90.0f, sr);
    for (int c = 0; c < 2; ++c) {
      cricket_[c].hz = rng_.range(4200.0f, 4900.0f);
      cricket_[c].pn = pan(c == 0 ? rng_.range(-0.9f, -0.5f) : rng_.range(0.5f, 0.9f));
      cricket_[c].next = rng_.range(0.0f, 1.0f);
    }
  }
  void setLevels(float wind, float creek, float insects, float drips, float rumble) {
    wind_ = wind;
    creek_ = creek;
    insects_ = insects;
    drips_ = drips;
    rumble_ = rumble;
  }
  void render(Lane& lane) {
    float dt = float(lane.n) / sr_;
    float gust = 0.55f + 0.45f * gust_.advance(dt);
    wL_.set(150.0f * std::pow(10.0f, cL_.advance(dt) * 0.5f + 0.5f), 0.8f, sr_);
    wR_.set(150.0f * std::pow(10.0f, cR_.advance(dt) * 0.5f + 0.5f), 0.8f, sr_);
    float rumble = rumble_ * std::max(0.0f, rumbleSwell_.advance(dt));
    // bubbles: Poisson spawning, rate depends on creek level
    float bubbleRate = 60.0f * creek_;
    for (int i = 0; i < lane.n; ++i) {
      if (creek_ > 0.001f && rng_.uni() < bubbleRate / sr_) spawnBubble(false);
      if (drips_ > 0.001f && rng_.uni() < 0.35f * drips_ / sr_) spawnBubble(true);
      float l = 0.0f, r = 0.0f, bl = 0.0f, br = 0.0f;
      // wind
      float nl = pL_.process(rng_), nr = pR_.process(rng_);
      l += wL_.bp(nl) * wind_ * gust * 0.5f;
      r += wR_.bp(nr) * wind_ * gust * 0.5f;
      // bubbles & drips
      float cl = 0.0f, cr = 0.0f;
      for (auto& b : bub_) {
        if (!b.active) continue;
        float y = std::sin(kTwoPi * b.ph) * b.amp;
        b.ph += b.f / sr_;
        if (b.ph >= 1.0f) b.ph -= 1.0f;
        b.f *= b.rise;
        b.amp *= b.decay;
        if (b.amp < 1e-4f) b.active = false;
        if (b.drip) {
          bl += y * b.pn.l;
          br += y * b.pn.r;
        } else {
          cl += y * b.pn.l;
          cr += y * b.pn.r;
        }
      }
      l += creekLp_.lp(cl) * creek_;
      r += creekLp_.lp(cr) * creek_;  // shared state is fine: it's noise
      l += bl * drips_;
      r += br * drips_;
      // crickets
      for (auto& c : cricket_) {
        c.t += 1.0f / sr_;
        if (c.t >= c.next) {
          c.t = 0.0f;
          c.pulses = rng_.irange(3, 5);
          c.next = rng_.range(0.6f, 1.3f);
        }
        float pulsePos = c.t * 28.0f;
        float g = 0.0f;
        if (pulsePos < float(c.pulses)) g = sinPos(kPi * (pulsePos - std::floor(pulsePos)));
        c.ph += c.hz / sr_;
        if (c.ph >= 1.0f) c.ph -= 1.0f;
        float y = std::sin(kTwoPi * c.ph) * g * insects_ * 0.025f;
        l += y * c.pn.l;
        r += y * c.pn.r;
      }
      // rumble
      brown_ = brown_ * 0.998f + rng_.bipolar() * 0.05f;
      float th = rumbleLp_.lp(brown_) * rumble * 1.5f;
      l += th;
      r += th;
      lane.add(i, l, r, 0.8f, 0.35f);
      lane.add(i, bl * drips_, br * drips_, 0.0f, 0.8f);  // drips: mostly cave
    }
  }

 private:
  struct Bubble {
    bool active = false, drip = false;
    float f = 1000, ph = 0, amp = 0, rise = 1, decay = 0.99f;
    Pan pn;
  };
  struct Cricket {
    float hz = 4500, ph = 0, t = 0, next = 1;
    int pulses = 4;
    Pan pn;
  };
  void spawnBubble(bool drip) {
    for (auto& b : bub_) {
      if (b.active) continue;
      b.active = true;
      b.drip = drip;
      b.ph = 0.0f;
      b.f = drip ? rng_.range(700.0f, 1400.0f) : 500.0f * std::pow(5.0f, rng_.uni());
      float life = drip ? rng_.range(0.04f, 0.08f) : rng_.range(0.01f, 0.04f);
      b.decay = decayCoef(life, sr_);
      b.rise = std::pow(drip ? 2.5f : 1.8f, 1.0f / (life * sr_));
      b.amp = drip ? rng_.range(0.15f, 0.3f) : rng_.range(0.02f, 0.08f);
      b.pn = pan(rng_.range(-0.9f, 0.9f));
      return;
    }
  }
  float sr_ = 48000, wind_ = 0, creek_ = 0, insects_ = 0, drips_ = 0, rumble_ = 0, brown_ = 0;
  Rng rng_;
  PinkNoise pL_, pR_;
  Svf wL_, wR_;
  Drift cL_, cR_, gust_, rumbleSwell_;
  OnePole creekLp_, rumbleLp_;
  std::array<Bubble, 64> bub_;
  std::array<Cricket, 2> cricket_;
};

// ===========================================================================
// DRONE: the dark floor. Root, fifth and octave from detuned saws through a
// slowly breathing low-pass.
// ===========================================================================
class Drone {
 public:
  void init(float sr, uint64_t seed, float rootHz) {
    sr_ = sr;
    hz_ = rootHz;
    cut_.init(seed, 6.0f, 15.0f);
    Rng r(seed);
    for (auto& s : saw_) s.setPhase(r.uni());
  }
  void render(Lane& lane, float gain) {
    float dt = float(lane.n) / sr_;
    float c = 140.0f * std::pow(3.0f, cut_.advance(dt) * 0.5f + 0.5f);
    fL_.set(c, 0.8f, sr_);
    fR_.set(c * 1.1f, 0.8f, sr_);
    static const float kRatio[6] = {1.0f, 1.0f, 1.5f, 1.5f, 2.0f, 2.0f};
    static const float kCents[6] = {-7.0f, 7.0f, -4.0f, 4.0f, -9.0f, 9.0f};
    for (int i = 0; i < lane.n; ++i) {
      float l = 0.0f, r = 0.0f;
      for (int k = 0; k < 6; ++k) {
        float s = saw_[size_t(k)].process(hz_ * kRatio[k] * cents(kCents[k]), sr_);
        (k & 1 ? r : l) += s;
      }
      l = fL_.lp(l) * gain * 0.06f;
      r = fR_.lp(r) * gain * 0.06f;
      lane.add(i, l, r, 0.7f, 0.5f);
    }
  }

 private:
  float sr_ = 48000, hz_ = 50;
  std::array<Saw, 6> saw_;
  Svf fL_, fR_;
  Drift cut_;
};

// ===========================================================================
// KICK, tuned to the root of the key so it locks with the bass. A sine
// dropping fast from ~180 Hz, a short click for the beater, then tanh.
// It also outputs a "duck" envelope: the bass and neuro dip under every
// kick (sidechain), which is what makes kick + bass feel like one engine.
// The same voice, slower and deeper, makes the transition booms.
// ===========================================================================
class Kick {
 public:
  void init(float sr, uint64_t seed, float rootHz) {
    sr_ = sr;
    rng_ = Rng(seed);
    root_ = rootHz < 45.0f ? rootHz * 1.0f : rootHz;
    click_.set(3000.0f, 0.7f, sr);
  }
  void trigger(float vel, int wait) { start(vel, wait, false); }
  void boom(float vel, int wait) { start(vel, wait, true); }

  // duck[i] = 0..1 (1 right on the kick)
  void render(Lane& lane, float* duck) {
    for (int i = 0; i < lane.n; ++i) duck[i] = 0.0f;
    for (auto& v : v_) {
      if (!v.active) continue;
      for (int i = 0; i < lane.n; ++i) {
        if (v.wait > 0) {
          --v.wait;
          continue;
        }
        float t = v.age / sr_;
        float f = v.f1 + (v.f0 - v.f1) * std::exp(-t / v.pitchT);
        float body = std::sin(kTwoPi * v.ph) * std::exp(-t * 6.9f / v.t60) * std::min(1.0f, v.age / 48.0f);
        v.ph += f / sr_;
        if (v.ph >= 1.0f) v.ph -= 1.0f;
        float cl = click_.bp(rng_.bipolar()) * std::exp(-t / 0.004f) * v.click;
        // tekno kick: driven hard into an asymmetric clipper
        float y = (std::tanh(3.2f * (body + cl) + 0.25f) - 0.2449f) * v.vel * 0.5f;
        lane.add(i, y, y, 1.0f, v.big, 0.04f);
        if (!v.isBoom) duck[i] = std::max(duck[i], std::exp(-t / 0.09f));
        v.age += 1.0f;
      }
      if (v.age / sr_ > v.t60 * 1.2f) v.active = false;
    }
  }

 private:
  struct Voice {
    bool active = false, isBoom = false;
    int wait = 0;
    float age = 0, ph = 0, f0 = 180, f1 = 50, pitchT = 0.03f, t60 = 0.5f, click = 0.3f, vel = 1, big = 0.02f;
  };
  void start(float vel, int wait, bool isBoom) {
    Voice* v = &v_[0];
    for (auto& x : v_)
      if (!x.active) {
        v = &x;
        break;
      }
    v->active = true;
    v->isBoom = isBoom;
    v->wait = wait;
    v->age = 0.0f;
    v->ph = 0.0f;
    v->vel = vel;
    if (isBoom) {
      v->f0 = 140.0f;
      v->f1 = root_;
      v->pitchT = 0.08f;
      v->t60 = 3.0f;
      v->click = 0.6f;
      v->big = 0.35f;
    } else {
      v->f0 = 210.0f;
      v->f1 = root_;
      v->pitchT = 0.026f;
      v->t60 = 0.42f;
      v->click = 0.35f;
      v->big = 0.02f;
    }
  }
  float sr_ = 48000, root_ = 50;
  Rng rng_;
  Svf click_;
  std::array<Voice, 4> v_;
};

// ===========================================================================
// PRIMAL PERCUSSION: hand frame drums, a deep ritual drum, a shaker,
// log drum, a small anvil and bone rattles. All modal models (skin / wood /
// bar) or shaped noise, mostly into a small stone room.
// ===========================================================================
class Perc {
 public:
  enum Kind { Frame, Deep, Shaker, Log, Metal, Rattle, Clank };
  void init(float sr, uint64_t seed) {
    sr_ = sr;
    rng_ = Rng(seed);
  }
  void hit(Kind kind, float hz, float vel, float panPos, int wait) {
    Voice* v = &v_[0];
    for (auto& x : v_) {
      if (!x.active) {
        v = &x;
        break;
      }
      if (x.level < v->level) v = &x;
    }
    v->active = true;
    v->kind = kind;
    v->wait = wait;
    v->age = 0.0f;
    v->vel = vel;
    v->level = 1.0f;
    v->quiet = 0;
    v->pn = pan(panPos);
    v->modal = kind != Shaker;
    v->tone = 0.0f;
    v->ph = 0.0f;
    v->crush = 0;
    v->noise = 0.0f;
    switch (kind) {
      case Frame:
        v->body.setup(acoustic::Material::Skin, hz, 1.2f, 0.25f, sr_, rng_);
        v->ex.strike(vel, 0.25f, sr_);
        v->tone = 0.5f;  // low "thump" of the palm
        v->f0 = hz * 1.6f;
        v->f1 = hz;
        v->toneT60 = 0.25f;
        v->room = 0.4f;
        break;
      case Deep:
        v->body.setup(acoustic::Material::Skin, hz, 1.5f, 0.45f, sr_, rng_);
        v->ex.strike(vel * 0.7f, 0.45f, sr_);
        v->tone = 1.0f;
        v->f0 = hz * 2.4f;
        v->f1 = hz;
        v->toneT60 = 0.9f;
        v->room = 0.45f;
        break;
      case Log:
        v->body.setup(acoustic::Material::Wood, hz, 1.4f, 0.7f, sr_, rng_);
        v->ex.strike(vel, 0.7f, sr_);
        v->room = 0.45f;
        break;
      case Metal:
        v->body.setup(acoustic::Material::Bar, hz, 0.35f, 0.95f, sr_, rng_);
        v->ex.strike(vel * 0.6f, 0.95f, sr_);
        v->room = 0.5f;
        break;
      case Rattle: {
        v->body.setup(acoustic::Material::Wood, hz, 0.3f, 0.9f, sr_, rng_);
        float interval = rng_.range(0.012f, 0.02f);
        v->ex.taps(vel * 0.6f, 0.9f, interval, 1.15f, 0.7f, rng_.irange(3, 5), sr_);
        v->room = 0.35f;
        break;
      }
      case Clank:
        // Industrial backbeat: a steel plate hit hard, a crack of noise, and
        // the whole thing bit-crushed so it sounds like a broken sampler.
        v->body.setup(acoustic::Material::Plate, hz, 0.25f, 1.0f, sr_, rng_);
        v->ex.strike(vel, 1.0f, sr_);
        v->nf.reset();
        v->nf.set(rng_.range(1400.0f, 2400.0f), 0.8f, sr_);
        v->noise = vel;
        v->shakeT60 = 0.14f;
        v->crush = 3;
        v->room = 0.55f;
        break;
      case Shaker:
        v->nf.reset();
        v->nf.set(rng_.range(6000.0f, 8500.0f), 1.3f, sr_);
        v->shakeT60 = rng_.range(0.06f, 0.11f);
        v->second = rng_.range(0.008f, 0.016f);
        v->room = 0.25f;
        break;
    }
  }
  void render(Lane& lane) {
    for (auto& v : v_) {
      if (!v.active) continue;
      float peak = 0.0f;
      for (int i = 0; i < lane.n; ++i) {
        if (v.wait > 0) {
          --v.wait;
          continue;
        }
        float t = v.age / sr_;
        float y = 0.0f, l = 0.0f, r = 0.0f;
        if (v.modal) {
          float xi, xc;
          v.ex.process(rng_, xi, xc);
          v.body.process(xi, xc, l, r);
          l *= 1.6f;
          r *= 1.6f;
        }
        if (v.tone > 0.0f) {
          float f = v.f1 + (v.f0 - v.f1) * std::exp(-t / 0.035f);
          y += std::sin(kTwoPi * v.ph) * std::exp(-t * 6.9f / v.toneT60) * v.tone * v.vel;
          v.ph += f / sr_;
          if (v.ph >= 1.0f) v.ph -= 1.0f;
        }
        if (v.kind == Clank) {
          y += v.nf.bp(rng_.bipolar()) * v.noise * std::exp(-t * 6.9f / v.shakeT60) * 2.5f;
        }
        if (v.kind == Shaker) {
          float e = smoothstep(0.0f, 0.004f, t) * std::exp(-t * 6.9f / v.shakeT60);
          float t2 = t - v.second;
          if (t2 > 0.0f) e += 0.6f * smoothstep(0.0f, 0.003f, t2) * std::exp(-t2 * 6.9f / v.shakeT60);
          y += v.nf.bp(rng_.bipolar()) * e * v.vel * 0.5f;
        }
        l = std::tanh(l + y);
        r = std::tanh(r + y);
        if (v.crush > 0) {  // sample-rate reduction + 5-bit quantise
          if (v.holdN-- <= 0) {
            v.holdN = v.crush;
            v.holdL = std::round(l * 16.0f) / 16.0f;
            v.holdR = std::round(r * 16.0f) / 16.0f;
          }
          l = v.holdL;
          r = v.holdR;
        }
        l *= v.pn.l * 1.41421356f;
        r *= v.pn.r * 1.41421356f;
        peak = std::max(peak, std::fabs(l) + std::fabs(r));
        lane.add(i, l, r, 1.0f, 0.06f, v.room);
        v.age += 1.0f;
      }
      v.level = peak;
      if (v.wait <= 0 && v.age > sr_ * 0.05f) {
        v.quiet = peak < 2e-5f ? v.quiet + 1 : 0;
        if (v.quiet > 6 || v.age > sr_ * 4.0f) v.active = false;
      }
    }
  }

 private:
  struct Voice {
    bool active = false, modal = false;
    Kind kind = Frame;
    int wait = 0, quiet = 0;
    float age = 0, vel = 1, level = 0, room = 0.3f;
    float tone = 0, f0 = 100, f1 = 80, toneT60 = 0.3f, ph = 0;
    float shakeT60 = 0.08f, second = 0.01f, noise = 0.0f, holdL = 0, holdR = 0;
    int crush = 0, holdN = 0;
    acoustic::ModalBody body;
    acoustic::Exciter ex;
    Svf nf;
    Pan pn;
  };
  float sr_ = 48000;
  Rng rng_;
  std::array<Voice, 28> v_;
};

// ===========================================================================
// BASS: the rolling, driving line. Mono.
//   * sub: a clean sine one octave down from the mids, kept out of the
//     distortion and echo so the low end stays tight.
//   * mids: two detuned saws through a resonant low-pass with a per-note
//     envelope (growl), then (outside this class) heavy asymmetric tape
//     drive and the tempo echo.
// ===========================================================================
class Bass {
 public:
  void init(float sr) {
    sr_ = sr;
    aK_ = onePoleCoef(0.002f, sr);
    rK_ = onePoleCoef(0.012f, sr);
    fK_ = decayCoef(0.18f, sr);
  }
  void note(float hz, float vel, float gateSeconds, int wait) {
    pending_ = true;
    pWait_ = wait;
    pHz_ = hz;
    pVel_ = vel;
    pGate_ = int(gateSeconds * sr_);
  }
  void setFilter(float open) { open_ = open; }
  void render(float* sub, float* midL, float* midR, int n) {
    float range = 120.0f + 3000.0f * open_ * open_;
    for (int i = 0; i < n; ++i) {
      if (pending_ && pWait_-- <= 0) {
        pending_ = false;
        hz_ = pHz_;
        vel_ = pVel_;
        gate_ = pGate_;
        fenv_ = 1.0f;
      }
      float target = gate_ > 0 ? vel_ : 0.0f;
      if (gate_ > 0) --gate_;
      amp_ += (target - amp_) * (target > amp_ ? aK_ : rK_);
      fenv_ *= fK_;
      fL_.set(60.0f + range * (0.2f + 0.8f * fenv_ * vel_), 1.8f, sr_);
      fR_.set(62.0f + range * (0.2f + 0.8f * fenv_ * vel_), 1.8f, sr_);
      float a = sA_.process(hz_, sr_), b = sB_.process(hz_ * 1.006f, sr_);
      midL[i] = fL_.lp(a + 0.6f * b) * amp_ * 0.5f;
      midR[i] = fR_.lp(b + 0.6f * a) * amp_ * 0.5f;
      sub[i] = std::sin(kTwoPi * subPh_) * amp_ * 0.5f;
      subPh_ += hz_ * 0.5f / sr_;
      if (subPh_ >= 1.0f) subPh_ -= 1.0f;
    }
  }

 private:
  float sr_ = 48000, aK_ = 1, rK_ = 1, fK_ = 1, open_ = 0.5f;
  bool pending_ = false;
  int pWait_ = 0, pGate_ = 0, gate_ = 0;
  float pHz_ = 55, pVel_ = 1, hz_ = 55, vel_ = 0, amp_ = 0, fenv_ = 0, subPh_ = 0;
  Saw sA_, sB_;
  Svf fL_, fR_;
};

// ===========================================================================
// NEUROBASS: the snarling, "talking" bass of neurofunk.
//   * source: detuned saws + an FM sine (bright, metallic sidebands)
//   * two band-pass "formants" swept by an LFO locked to the tempo, moving
//     between vowel shapes, so the bass seems to speak.
//   * a comb filter whose delay is swept by a second LFO at a different
//     period: the two LFOs (6 and 5 sixteenths) drift against each other
//     and against the 16-step bar, a polymeter inside the sound itself.
//   * hard distortion, then a high-pass so the sub stays with the bass.
//   * a rhythmic gate on a 12-step euclidean pattern (3 against 4).
// ===========================================================================
class Neuro {
 public:
  void init(float sr, uint64_t seed) {
    sr_ = sr;
    Rng r(seed);
    for (auto& s : saw_) s.setPhase(r.uni());
    combL_.init(int(sr * 0.02f));
    combR_.init(int(sr * 0.02f));
    hpL_.set(110.0f, 0.7f, sr);
    hpR_.set(110.0f, 0.7f, sr);
    gateK_ = onePoleCoef(0.003f, sr);
  }
  void note(float hz) { target_ = hz; }
  void setGate(const std::vector<int>& g) { gatePat_ = g; }
  // beat = position in beats at the start of the block
  void render(float* outL, float* outR, int n, double beat, double beatsPerSample, float amount) {
    static const float kV[3][2] = {{380.0f, 950.0f}, {750.0f, 1900.0f}, {300.0f, 2500.0f}};
    for (int i = 0; i < n; ++i) {
      double b = beat + double(i) * beatsPerSample;
      hz_ += (target_ - hz_) * 0.002f;  // ~10 ms glide between notes
      float lfoA = 0.5f + 0.5f * std::sin(float(kTwoPi * std::fmod(b / 1.5, 1.0)));   // 6 sixteenths
      float lfoC = 0.5f + 0.5f * std::sin(float(kTwoPi * std::fmod(b / 1.25, 1.0)));  // 5 sixteenths
      int step = int(std::floor(b * 4.0));
      float g = gatePat_.empty() ? 1.0f : float(gatePat_[size_t(step % int(gatePat_.size()))]);
      gate_ += (g - gate_) * gateK_;
      if ((i & 7) == 0) {
        float pos = lfoA * 2.0f;
        int i0 = std::min(1, int(pos));
        float fr = pos - float(i0);
        float f1 = lerp(kV[i0][0], kV[i0 + 1][0], fr), f2 = lerp(kV[i0][1], kV[i0 + 1][1], fr);
        fa_.set(f1, 4.0f, sr_);
        fb_.set(f2, 5.0f, sr_);
        fa2_.set(f1 * 1.08f, 4.0f, sr_);
        fb2_.set(f2 * 0.93f, 5.0f, sr_);
      }
      float fm = std::sin(kTwoPi * fmPh_) * (1.5f + 3.0f * lfoC);
      fmPh_ += hz_ * 2.0f / sr_;
      if (fmPh_ >= 1.0f) fmPh_ -= 1.0f;
      float sine = std::sin(kTwoPi * carPh_ + fm);
      carPh_ += hz_ / sr_;
      if (carPh_ >= 1.0f) carPh_ -= 1.0f;
      float sl = saw_[0].process(hz_ * 0.995f, sr_) + 0.7f * sine;
      float sr2 = saw_[1].process(hz_ * 1.005f, sr_) + 0.7f * sine;
      float l = fa_.bp(sl) + 0.7f * fb_.bp(sl) + 0.3f * sl;
      float r = fa2_.bp(sr2) + 0.7f * fb2_.bp(sr2) + 0.3f * sr2;
      float cd = (0.002f + 0.006f * lfoC) * sr_;
      float cl = combL_.tapf(cd), cr = combR_.tapf(cd * 1.13f);
      combL_.write(l + cl * 0.6f);
      combR_.write(r + cr * 0.6f);
      l = std::tanh(3.5f * (l + cl) + 0.3f) - 0.2913f;  // asymmetric
      r = std::tanh(3.5f * (r + cr) + 0.3f) - 0.2913f;
      outL[i] = hpL_.hp(l) * gate_ * amount * 0.35f;
      outR[i] = hpR_.hp(r) * gate_ * amount * 0.35f;
    }
  }

 private:
  float sr_ = 48000, hz_ = 80, target_ = 80, gate_ = 0, gateK_ = 1, fmPh_ = 0, carPh_ = 0;
  std::array<Saw, 2> saw_;
  Svf fa_, fb_, fa2_, fb2_, hpL_, hpR_;
  DelayLine combL_, combR_;
  std::vector<int> gatePat_;
};

// ===========================================================================
// STUTTER: the IDM glitch. It keeps recording the last second. When
// triggered it grabs the newest slice (a 16th or a 32nd) and loops it
// instead of the live signal, optionally sliding the playback speed (the
// pitch falls or rises as it repeats) and crushing the bits. 2 ms fades at
// the slice edges keep it clicky-on-purpose rather than broken.
// Every rhythm lane gets its own Stutter with the same trigger, so the
// glitches line up across kick, percussion and bass.
// ===========================================================================
class Stutter {
 public:
  void init(float sr) {
    sr_ = sr;
    L_.init(int(sr) + 8);
    R_.init(int(sr) + 8);
    edge_ = std::max(1.0f, 0.002f * sr);
  }
  void trigger(int slice, int duration, float rateEnd, int crush) {
    slice_ = float(std::max(slice, 64));
    left_ = duration;
    dur_ = duration;
    phase_ = 0.0f;
    rateEnd_ = rateEnd;
    crush_ = crush;
    base_ = slice_;  // the slice ends "now": read it from slice_ samples ago
    mix_ = 0.0f;
  }
  void process(float& l, float& r) {
    L_.write(l);
    R_.write(r);
    float target = left_ > 0 ? 1.0f : 0.0f;
    mix_ += (target - mix_) * 0.02f;  // ~1 ms crossfade in and out
    if (mix_ < 1e-4f && left_ <= 0) return;
    float p = dur_ > 0 ? 1.0f - float(left_) / float(dur_) : 1.0f;
    float rate = lerp(1.0f, rateEnd_, p);
    // position inside the frozen slice; the slice drifts back in time as
    // new samples are written, so add the samples elapsed since the trigger
    float elapsed = float(dur_ - left_);
    float within = std::fmod(phase_, slice_);
    float d = base_ + elapsed - within;
    float win = std::min(1.0f, std::min(within, slice_ - within) / edge_);
    float sl = L_.tapf(std::max(1.0f, d)) * win, sr = R_.tapf(std::max(1.0f, d)) * win;
    if (crush_ > 1) {
      if (holdN_-- <= 0) {
        holdN_ = crush_;
        holdL_ = std::round(sl * 24.0f) / 24.0f;
        holdR_ = std::round(sr * 24.0f) / 24.0f;
      }
      sl = holdL_;
      sr = holdR_;
    }
    phase_ += rate;
    if (left_ > 0) --left_;
    l = lerp(l, sl, mix_);
    r = lerp(r, sr, mix_);
  }

 private:
  float sr_ = 48000, slice_ = 1000, base_ = 1000, phase_ = 0, rateEnd_ = 1, mix_ = 0, edge_ = 96;
  float holdL_ = 0, holdR_ = 0;
  int left_ = 0, dur_ = 0, crush_ = 0, holdN_ = 0;
  DelayLine L_, R_;
};

// ===========================================================================
// FILTERBANK, after the Sherman Filterbank: a brutal analog box whose sound
// comes from DRIVE + RESONANCE + MOVEMENT.
//   * the input is overdriven first (tanh)
//   * filter 1: resonant low-pass, pushed close to self-oscillation, then
//     saturated again so the resonant peak screams and folds
//   * filter 2: band-pass tracking above it, in series, for the nasal
//     "talking" quack
//   * the cutoff is thrown around by three things at once: an envelope
//     follower (louder input = brighter: every hit squelches), a tempo
//     LFO on a 7-step cycle (a polymeter against the bar), and a random
//     step on every 16th (sample & hold, smoothed to avoid clicks)
// Feed it organic sounds (drums, water, cymbals, whales) and synthetic ones
// (bass, neuro) and they melt into the same rubbery, wet character.
// ===========================================================================
class Filterbank {
 public:
  void init(float sr, uint64_t seed) {
    sr_ = sr;
    rng_ = Rng(seed);
    drift_.init(mixSeed(seed, 1), 3.0f, 8.0f);
    atk_ = onePoleCoef(0.002f, sr);
    rel_ = onePoleCoef(0.09f, sr);
    shK_ = onePoleCoef(0.006f, sr);
    dcL_.setCutoff(20.0f, sr);
    dcR_.setCutoff(20.0f, sr);
  }
  void set(float baseHz, float reso, float envAmt, float lfoAmt, float drive) {
    base_ = baseHz;
    reso_ = reso;
    envAmt_ = envAmt;
    lfoAmt_ = lfoAmt;
    drive_ = drive;
  }
  void step(float randomAmt) { shTarget_ = rng_.bipolar() * randomAmt; }
  void block(float seconds) { driftNow_ = drift_.advance(seconds); }

  void process(float& l, float& r, double beat) {
    float e = std::max(std::fabs(l), std::fabs(r));
    env_ += (e - env_) * (e > env_ ? atk_ : rel_);
    sh_ += (shTarget_ - sh_) * shK_;
    if ((count_++ & 7) == 0) {
      float lfo = std::sin(float(kTwoPi * std::fmod(beat / 1.75, 1.0)));  // 7 sixteenths
      float oct = envAmt_ * std::min(1.0f, env_ * 6.0f) * 5.0f + lfoAmt_ * lfo * 2.0f + sh_ * 2.0f +
                  0.5f * driftNow_;
      float c1 = clampf(base_ * std::exp2(oct), 30.0f, 15000.0f);
      float q = 0.7f + reso_ * reso_ * 24.0f;
      f1L_.set(c1, q, sr_);
      f1R_.set(c1 * 1.06f, q, sr_);
      f2L_.set(c1 * 2.4f, q * 0.6f, sr_);
      f2R_.set(c1 * 2.6f, q * 0.6f, sr_);
    }
    l = chain(l, f1L_, f2L_, dcL_);
    r = chain(r, f1R_, f2R_, dcR_);
  }

 private:
  float chain(float x, Svf& f1, Svf& f2, OnePole& dc) {
    x = std::tanh(drive_ * x);
    float y1 = std::tanh(1.6f * f1.lp(x));
    float y2 = f2.bp(y1);
    return dc.hp(std::tanh(0.6f * y1 + 0.9f * y2)) * 0.5f;
  }
  float sr_ = 48000, base_ = 300, reso_ = 0.6f, envAmt_ = 0.7f, lfoAmt_ = 0.3f, drive_ = 3.0f;
  float env_ = 0, atk_ = 1, rel_ = 1, sh_ = 0, shTarget_ = 0, shK_ = 1, driftNow_ = 0;
  unsigned count_ = 0;
  Rng rng_;
  Drift drift_;
  Svf f1L_, f1R_, f2L_, f2R_;
  OnePole dcL_, dcR_;
};

}  // namespace ritual
