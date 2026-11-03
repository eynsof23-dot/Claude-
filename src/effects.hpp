#pragma once
// effects.hpp — the space the sounds live in.
//
// In cinematic music the room is half the sound. A dry pad sounds like a
// synth; the same pad through a long, dark, slowly-moving reverb sounds like
// a cathedral. Most "immersion" comes from here.

#include "dsp.hpp"

namespace dsp {

// Feedback-delay-network reverb: 8 delay lines that feed back into each other
// through a mixing matrix. Design choices worth stealing for your own mixes:
//   * high-pass before the reverb (140 Hz): reverberated sub bass is mud.
//   * damping inside the loop (~3.8 kHz): highs die faster than lows, like
//     a real stone hall. This is what makes it "dark" rather than "shiny".
//   * slow modulation of the delay times: removes metallic ringing and gives
//     the tail a gentle chorus.
//   * pre-delay: a short gap before the tail keeps the dry sound clear.
class Reverb {
 public:
  void init(float sr, float rt60, float size, float dampHz, uint64_t seed) {
    static const float kMs[8] = {31.7f, 37.9f, 41.3f, 47.1f, 53.9f, 61.3f, 67.1f, 79.7f};
    static const float kApMs[4] = {4.7f, 3.6f, 12.7f, 9.3f};
    Rng rng(seed);
    sr_ = sr;
    for (int i = 0; i < 8; ++i) {
      len_[i] = kMs[i] * 0.001f * sr * size;
      line_[i].init(int(len_[i] + kModDepth + 4.0f));
      gain_[i] = std::pow(10.0f, -3.0f * len_[i] / (rt60 * sr));
      damp_[i].setCutoff(dampHz, sr);
      lfo_[i].setPhase(rng.uni());
      lfoHz_[i] = rng.range(0.05f, 0.25f);
    }
    for (int c = 0; c < 2; ++c)
      for (int j = 0; j < 4; ++j)
        ap_[c][j].init(int(kApMs[j] * 0.001f * sr * (1.0f + 0.13f * float(c))), 0.62f);
    preLen_ = int(0.03f * sr);
    preL_.init(preLen_);
    preR_.init(preLen_);
    hpL_.setCutoff(140.0f, sr);
    hpR_.setCutoff(140.0f, sr);
  }

  void process(float inL, float inR, float& outL, float& outR) {
    float l = hpL_.hp(inL), r = hpR_.hp(inR);
    preL_.write(l);
    preR_.write(r);
    l = preL_.tap(preLen_);
    r = preR_.tap(preLen_);
    for (int j = 0; j < 4; ++j) {
      l = ap_[0][j].process(l);
      r = ap_[1][j].process(r);
    }
    float s[8], sum = 0.0f;
    for (int i = 0; i < 8; ++i) {
      float mod = lfo_[i].process(lfoHz_[i], sr_) * kModDepth;
      s[i] = damp_[i].lp(line_[i].tapf(len_[i] + mod)) * gain_[i];
      sum += s[i];
    }
    sum *= 0.25f;  // Householder matrix: x - (2/N) * sum(x). Lossless mixing.
    for (int i = 0; i < 8; ++i) line_[i].write(s[i] - sum + ((i & 1) ? r : l));
    outL = (s[0] - s[2] + s[4] - s[6]) * 0.5f;
    outR = (s[1] - s[3] + s[5] - s[7]) * 0.5f;
  }

 private:
  static constexpr float kModDepth = 9.0f;  // samples
  float sr_ = 48000.0f;
  DelayLine line_[8];
  float len_[8] = {}, gain_[8] = {}, lfoHz_[8] = {};
  OnePole damp_[8];
  Sine lfo_[8];
  Allpass ap_[2][4];
  DelayLine preL_, preR_;
  int preLen_ = 1;
  OnePole hpL_, hpR_;
};

// Tempo-synced ping-pong echo, darkened on every repeat (like tape). Only
// the bells and the ostinato use it: echoes on everything turns to soup.
class PingPong {
 public:
  void init(float sr, float maxSeconds) {
    sr_ = sr;
    l_.init(int(maxSeconds * sr) + 4);
    r_.init(int(maxSeconds * sr) + 4);
    lpL_.setCutoff(2500.0f, sr);
    lpR_.setCutoff(2500.0f, sr);
    hp_.setCutoff(300.0f, sr);
  }
  void set(float seconds, float feedback) {
    target_ = seconds * sr_;
    if (cur_ <= 0.0f) cur_ = target_;
    fb_ = feedback;
  }
  void process(float inL, float inR, float& outL, float& outR) {
    cur_ += (target_ - cur_) * 0.0001f;  // glide when the tempo changes
    float dl = l_.tapf(cur_), dr = r_.tapf(cur_);
    l_.write(hp_.hp(0.5f * (inL + inR)) + lpR_.lp(dr) * fb_);
    r_.write(lpL_.lp(dl) * fb_);
    outL = dl;
    outR = dr;
  }

 private:
  float sr_ = 48000.0f, target_ = 0.0f, cur_ = 0.0f, fb_ = 0.5f;
  DelayLine l_, r_;
  OnePole lpL_, lpR_, hp_;
};

// Gentle bus compressor ("glue"): slow-ish, low ratio. It evens out the big
// hits against the quiet passages without squashing them flat.
class Glue {
 public:
  void init(float sr, float thresholdDb, float ratio, float attack, float release) {
    thr_ = thresholdDb;
    slope_ = 1.0f - 1.0f / ratio;
    att_ = onePoleCoef(attack, sr);
    rel_ = onePoleCoef(release, sr);
  }
  float gain(float l, float r) {
    float x = std::max(std::fabs(l), std::fabs(r));
    env_ += (x - env_) * (x > env_ ? att_ : rel_);
    float over = 20.0f * std::log10(env_ + 1e-9f) - thr_;
    return over > 0.0f ? std::pow(10.0f, -over * slope_ / 20.0f) : 1.0f;
  }

 private:
  float thr_ = -12.0f, slope_ = 0.5f, att_ = 1, rel_ = 1, env_ = 0;
};

// Transparent below ~-2 dBFS, then rounds peaks off smoothly towards 0 dBFS.
inline float softClip(float x) {
  float a = std::fabs(x);
  if (a < 0.8f) return x;
  float y = 0.8f + 0.2f * std::tanh((a - 0.8f) / 0.2f);
  return x < 0.0f ? -y : y;
}

}  // namespace dsp
