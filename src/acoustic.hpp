#pragma once
// acoustic.hpp — physical models: things that get struck, scraped and bowed.
//
// Electroacoustic music is built from *sound objects* rather than notes. A
// sound object is two things:
//
//   an EXCITER  (how energy goes in: a strike, a bounce, a scrape, a bow,
//                a trickle of dust)
//   a BODY      (what rings: a metal plate, a glass, a bowl, a string)
//
// Keeping them separate is the most useful idea in this file. The same plate
// struck, scraped or bowed gives three completely different sounds that still
// clearly come from the same "object". That gives the listener something to
// hold on to without any melody.

#include <array>

#include "dsp.hpp"

namespace acoustic {

using namespace dsp;

// ---------------------------------------------------------------------------
// MATERIALS. Each one is a list of mode frequencies (as ratios of the lowest
// mode) measured from real objects. Non-integer ratios are why a bell or a
// plate doesn't sound like a "note". Doublets are pairs of nearly identical
// modes (real objects are never perfectly symmetric); they beat against each
// other, which gives bowls and bells their slow shimmering wobble.
// ---------------------------------------------------------------------------
enum class Material { Bar, Plate, Bowl, Bell, Glass, Wood, Skin };

struct MaterialDef {
  const char* name;
  std::vector<float> ratios;
  float t60;      // decay of the lowest mode (s)
  float dampExp;  // how much faster high modes die (0 = same, 1 = much faster)
  bool doublets;
  float lo, hi;   // comfortable fundamental range (Hz)
};

inline const MaterialDef& material(Material m) {
  static const MaterialDef d[] = {
      {"bar", {1.0f, 2.756f, 5.404f, 8.933f, 13.344f, 18.638f}, 3.0f, 0.6f, false, 180, 1100},
      {"plate",
       {1.0f, 1.35f, 1.67f, 2.13f, 2.39f, 2.84f, 3.17f, 3.52f, 3.98f, 4.41f, 4.97f, 5.43f, 6.12f},
       6.0f, 0.35f, true, 55, 260},
      {"bowl", {1.0f, 2.71f, 5.15f, 8.43f, 12.39f, 17.0f}, 12.0f, 0.5f, true, 110, 520},
      {"bell", {0.5f, 1.0f, 1.183f, 1.506f, 2.0f, 2.514f, 2.662f, 3.011f, 4.166f, 5.433f, 6.796f}, 7.0f,
       0.5f, true, 140, 700},
      {"glass", {1.0f, 2.32f, 4.25f, 6.63f, 9.38f, 12.5f}, 4.0f, 0.4f, false, 500, 2400},
      {"wood", {1.0f, 3.92f, 9.24f, 15.8f}, 0.5f, 1.0f, false, 140, 600},
      {"skin", {1.0f, 1.593f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.501f}, 0.8f, 0.8f, false, 60, 220},
  };
  return d[int(m)];
}

// ---------------------------------------------------------------------------
// MODAL BODY: one tuned two-pole resonator per mode. Strike it with a click
// and it rings like the material. Every instance jitters its ratios slightly
// so no two plates are identical.
// ---------------------------------------------------------------------------
class ModalBody {
 public:
  static constexpr int kMaxModes = 26;

  void setup(Material mat, float f0, float decayScale, float brightness, float sr, Rng& rng) {
    const MaterialDef& d = material(mat);
    n_ = 0;
    float norm = 0.0f;
    for (size_t k = 0; k < d.ratios.size(); ++k) {
      float ratio = d.ratios[k] * (1.0f + rng.range(-0.006f, 0.006f));
      int copies = d.doublets ? 2 : 1;
      for (int c = 0; c < copies && n_ < kMaxModes; ++c) {
        float f = f0 * ratio * (c ? 1.0f + rng.range(0.0008f, 0.003f) : 1.0f);
        if (f > sr * 0.45f || f < 20.0f) continue;
        float t60 = d.t60 * decayScale * std::pow(f0 / f, d.dampExp) * rng.range(0.8f, 1.2f);
        float w = kTwoPi * f / sr;
        float r = std::exp(-6.9077553f / (std::max(t60, 0.02f) * sr));
        // Brightness = where it was hit and with what: a hard hit near the
        // edge excites high modes, a soft hit in the middle mostly the low.
        float amp = rng.range(0.3f, 1.0f) * std::pow(1.0f / float(k + 1), 1.3f * (1.0f - brightness));
        Mode& m = m_[size_t(n_++)];
        m.b1 = 2.0f * r * std::cos(w);
        m.b2 = r * r;
        m.gImp = std::sin(w) * amp;                         // impulse in -> amplitude `amp`
        m.gCont = m.gImp * std::sqrt(2.0f * (1.0f - r * r));  // noise in -> same level, any Q
        m.y1 = m.y2 = 0.0f;
        Pan p = pan(((n_ & 1) ? -1.0f : 1.0f) * rng.range(0.15f, 0.85f));  // modes spread L/R
        m.l = p.l;
        m.r = p.r;
        norm += amp * amp;
      }
    }
    float g = 1.0f / std::sqrt(std::max(norm, 1e-6f));
    for (int k = 0; k < n_; ++k) {
      m_[size_t(k)].gImp *= g;
      m_[size_t(k)].gCont *= g;
    }
  }

  // xi: impulsive excitation, xc: continuous (noise-like) excitation
  void process(float xi, float xc, float& outL, float& outR) {
    float l = 0.0f, r = 0.0f;
    for (int k = 0; k < n_; ++k) {
      Mode& m = m_[size_t(k)];
      float y = m.b1 * m.y1 - m.b2 * m.y2 + m.gImp * xi + m.gCont * xc;
      m.y2 = m.y1;
      m.y1 = y;
      l += y * m.l;
      r += y * m.r;
    }
    outL = l;
    outR = r;
  }

 private:
  struct Mode {
    float b1, b2, gImp, gCont, y1, y2, l, r;
  };
  std::array<Mode, kMaxModes> m_{};
  int n_ = 0;
};

// ---------------------------------------------------------------------------
// STRING: Karplus-Strong waveguide. A delay line one period long, fed back
// through a low-pass. Extras that make it "prepared":
//   * glide: the delay length can slide, like changing tension mid-note.
//   * rattle: above a threshold, noise is injected in proportion to the
//     string's motion, like a bolt or paper clip touching a piano string.
// ---------------------------------------------------------------------------
class StringModel {
 public:
  void init(float sr) {
    sr_ = sr;
    d_.init(int(sr / 25.0f) + 8);
    dc_.setCutoff(20.0f, sr);
  }
  void setup(float hz, float t60, float brightness, float rattle) {
    d_.init(int(sr_ / 25.0f) + 8);  // clear
    delay_ = target_ = sr_ / hz;
    glideK_ = 0.0f;
    fb_ = std::exp(-6.9077553f / (t60 * hz));
    lp_ = OnePole();
    lp_.setCutoff(clampf(hz * (2.0f + 14.0f * brightness), 200.0f, sr_ * 0.45f), sr_);
    rattle_ = rattle;
    contGain_ = std::sqrt(1.0f - fb_ * fb_) * 2.0f;
  }
  void glideTo(float hz, float seconds) {
    target_ = clampf(sr_ / hz, 4.0f, sr_ / 30.0f);
    glideK_ = onePoleCoef(seconds / 3.0f, sr_);
  }
  float process(float xi, float xc, Rng& rng) {
    delay_ += (target_ - delay_) * glideK_;
    float y = lp_.lp(d_.tapf(delay_));
    if (rattle_ > 0.0f) {
      float a = std::fabs(y);
      if (a > 0.04f) y += rng.bipolar() * rattle_ * (a - 0.04f);
    }
    if (std::fabs(y) > 1.0f) y = std::tanh(y);
    d_.write(y * fb_ + xi + xc * contGain_);
    return dc_.hp(y);
  }

 private:
  float sr_ = 48000.0f, delay_ = 100.0f, target_ = 100.0f, glideK_ = 0.0f, fb_ = 0.99f;
  float rattle_ = 0.0f, contGain_ = 0.1f;
  DelayLine d_;
  OnePole lp_, dc_;
};

// ---------------------------------------------------------------------------
// EXCITERS: the gestures. These are the "verbs" of the piece.
// ---------------------------------------------------------------------------
class Exciter {
 public:
  enum Type { Strike, Taps, Scrape, Bow, Dust };

  // One hit. Hardness = mallet: soft felt (0) to steel (1).
  void strike(float vel, float hardness, float sr) {
    reset(Strike, sr);
    vel_ = vel;
    hard_ = hardness;
    hit(vel);
  }
  // A train of hits whose spacing and level change geometrically:
  //   ratio < 1, ampRatio < 1  -> a bouncing ball (accelerating, fading)
  //   ratio > 1                -> slowing taps (rallentando)
  void taps(float vel, float hardness, float interval, float ratio, float ampRatio, int hits, float sr) {
    reset(Taps, sr);
    vel_ = vel;
    hard_ = hardness;
    interval_ = interval * sr;
    ratio_ = ratio;
    ampRatio_ = ampRatio;
    hitsLeft_ = hits - 1;
    countdown_ = interval_;
    hit(vel);
  }
  // Friction across a surface: noise with jagged, random "teeth" plus
  // occasional catches. Roughness = how coarse the teeth are.
  void scrape(float vel, float seconds, float roughness, float sr) {
    reset(Scrape, sr);
    vel_ = vel;
    dur_ = seconds * sr;
    period_ = sr / (15.0f + 200.0f * (1.0f - roughness));
    tone_.setCutoff(1500.0f + 5000.0f * (1.0f - roughness), sr);
  }
  // A bow (or a finger round a glass rim): continuous soft noise whose
  // pressure swells and fades, with a slight unsteadiness.
  void bow(float vel, float seconds, float sr) {
    reset(Bow, sr);
    vel_ = vel;
    dur_ = seconds * sr;
    tone_.setCutoff(900.0f, sr);
  }
  // Random tiny impulses (rain, sand, crackle), `rate` per second.
  void dust(float vel, float rate, float seconds, float sr) {
    reset(Dust, sr);
    vel_ = vel;
    rate_ = rate / sr;
    dur_ = seconds * sr;
  }

  void process(Rng& rng, float& imp, float& cont) {
    imp = 0.0f;
    cont = 0.0f;
    float p = dur_ > 0.0f ? t_ / dur_ : 0.0f;
    switch (type_) {
      case Strike:
      case Taps:
        if (type_ == Taps && hitsLeft_ > 0 && --countdown_ <= 0.0f) {
          vel_ *= ampRatio_;
          interval_ *= ratio_;
          countdown_ += std::max(interval_, sr_ * 0.012f);
          --hitsLeft_;
          hit(vel_ * rng.range(0.8f, 1.0f));
        }
        imp = tone_.lp(rng.bipolar() * burst_ + click_);
        click_ = 0.0f;
        burst_ *= burstK_;
        break;
      case Scrape: {
        if (--shCount_ <= 0.0f) {
          float u = rng.uni();
          sh_ = u * u;
          shCount_ = period_ * rng.range(0.4f, 1.6f);
          if (rng.chance(0.08f)) imp = sh_ * vel_ * 0.6f;  // a catch
        }
        float env = std::pow(std::max(0.0f, std::sin(kPi * clampf(p, 0.0f, 1.0f))), 0.7f) * vel_;
        cont = tone_.lp(rng.bipolar()) * (0.25f + sh_) * env * 1.6f;
        imp *= env;
        break;
      }
      case Bow: {
        drift_ += (rng.bipolar() - drift_) * 0.0005f;
        // max(0, ...): sinf(pi) is a tiny negative number, and pow() of a negative
        // number with a fractional exponent is NaN.
        float env = std::pow(std::max(0.0f, std::sin(kPi * clampf(p, 0.0f, 1.0f))), 1.5f) * vel_ *
                    (1.0f + 6.0f * drift_);
        cont = tone_.lp(rng.bipolar()) * env * 0.8f;
        break;
      }
      case Dust:
        if (rng.uni() < rate_) {
          float env = smoothstep(0.0f, 0.1f, p) * (1.0f - smoothstep(0.85f, 1.0f, p));
          float a = rng.range(0.2f, 1.0f);
          imp = vel_ * env * a * (rng.chance(0.5f) ? 1.0f : -1.0f);
        }
        break;
    }
    t_ += 1.0f;
  }

  bool done() const {
    switch (type_) {
      case Strike:
        return burst_ < 1e-4f;
      case Taps:
        return hitsLeft_ <= 0 && burst_ < 1e-4f;
      default:
        return t_ >= dur_;
    }
  }

 private:
  void reset(Type t, float sr) {
    type_ = t;
    sr_ = sr;
    t_ = 0.0f;
    dur_ = 0.0f;
    hitsLeft_ = 0;
    burst_ = click_ = sh_ = shCount_ = drift_ = 0.0f;
  }
  void hit(float v) {
    // The contact noise lasts a few ms; a resonator sums every sample of it,
    // so its level must shrink with its length or long (soft) hits explode.
    float t60 = 0.002f + 0.02f * (1.0f - hard_);
    click_ = v;
    burst_ = v * 1.5f / std::sqrt(std::max(1.0f, t60 / 6.9f * sr_));
    burstK_ = decayCoef(t60, sr_);
    tone_.setCutoff(300.0f + 12000.0f * hard_ * hard_, sr_);
  }

  Type type_ = Strike;
  float sr_ = 48000.0f, t_ = 0.0f, dur_ = 0.0f, vel_ = 0.0f, hard_ = 0.5f;
  float burst_ = 0.0f, burstK_ = 0.0f, click_ = 0.0f;
  float interval_ = 0.0f, ratio_ = 1.0f, ampRatio_ = 1.0f, countdown_ = 0.0f;
  int hitsLeft_ = 0;
  float period_ = 100.0f, sh_ = 0.0f, shCount_ = 0.0f, rate_ = 0.0f, drift_ = 0.0f;
  OnePole tone_;
};

// ---------------------------------------------------------------------------
// SPACE: where a sound is and how it moves. Distance does three things at
// once, the way it does in a real room: quieter, duller (air eats highs) and
// wetter (more reverb, less direct sound). Movement through that space is
// half of what makes acousmatic music feel immersive.
// ---------------------------------------------------------------------------
class Spatial {
 public:
  enum Path { Still, Drift, Flyby, Approach, Recede };

  void set(Path path, float seconds, Rng& rng) {
    path_ = path;
    dur_ = std::max(seconds, 0.1f);
    t_ = 0.0f;
    pan0_ = rng.range(-0.9f, 0.9f);
    pan1_ = pan0_;
    d0_ = d1_ = rng.range(0.15f, 0.6f);
    switch (path) {
      case Still:
        break;
      case Drift:
        pan1_ = clampf(pan0_ + rng.range(-0.8f, 0.8f), -1.0f, 1.0f);
        d1_ = rng.range(0.1f, 0.7f);
        break;
      case Flyby:
        pan0_ = rng.chance(0.5f) ? -1.0f : 1.0f;
        pan1_ = -pan0_;
        d0_ = d1_ = rng.range(0.6f, 0.9f);
        break;
      case Approach:
        d0_ = 0.95f;
        d1_ = rng.range(0.05f, 0.25f);
        break;
      case Recede:
        d0_ = rng.range(0.05f, 0.25f);
        d1_ = 0.95f;
        break;
    }
  }
  // Put the sound far away and keep it there.
  void distant(Rng& rng) {
    set(Still, 1.0f, rng);
    d0_ = d1_ = rng.range(0.8f, 1.0f);
  }

  void block(float dt, float sr) {
    t_ += dt;
    float p = clampf(t_ / dur_, 0.0f, 1.0f);
    float s = p * p * (3.0f - 2.0f * p);
    float pn = lerp(pan0_, pan1_, s);
    float d = path_ == Flyby ? lerp(d0_, 0.05f, std::sin(kPi * p)) : lerp(d0_, d1_, s);
    Pan g = pan(pn);
    gL_ = g.l * 1.41421356f;
    gR_ = g.r * 1.41421356f;
    gain_ = 1.0f / (1.0f + 2.5f * d);
    dry_ = 1.0f - 0.75f * d;
    wet_ = 0.2f + 0.8f * d;
    lpL_.setCutoff(16000.0f * std::pow(0.14f, d), sr);
    lpR_.setCutoff(16000.0f * std::pow(0.14f, d), sr);
  }
  void apply(float& l, float& r) {
    l = lpL_.lp(l) * gL_ * gain_;
    r = lpR_.lp(r) * gR_ * gain_;
  }
  float dry() const { return dry_; }
  float wet() const { return wet_; }

 private:
  Path path_ = Still;
  float dur_ = 1, t_ = 0, pan0_ = 0, pan1_ = 0, d0_ = 0.3f, d1_ = 0.3f;
  float gL_ = 1, gR_ = 1, gain_ = 1, dry_ = 1, wet_ = 0.3f;
  OnePole lpL_, lpR_;
};

}  // namespace acoustic
