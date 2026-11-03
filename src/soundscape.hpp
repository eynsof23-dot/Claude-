#pragma once
// soundscape.hpp — the conductor.
//
// The instruments don't decide anything. The conductor plans the piece:
//
//   1. FORM.  A "movement" is cut into sections of 6-12 bars. Each section
//      gets an intensity from an arc: quiet start, a long build, a climax
//      around 55-75% of the way in, then a fall. Real film cues are shaped
//      exactly like this.
//   2. ORCHESTRATION.  Each section decides which instruments are present:
//      ostinato when tense, drums only at the top, heartbeat in the uneasy
//      middle, choir now and then, bells mostly in the quiet parts.
//   3. TRANSITIONS.  When the next section is much louder, a riser is
//      scheduled to end exactly on its downbeat, where a boom (and often a
//      braam) lands. A sudden drop also gets a boom: the floor gives way.
//   4. HARMONY.  Chords change on bar lines, faster when it's intense.
//
// Same seed -> same piece (same form, chords and events on any compiler;
// bit-identical audio when rebuilt with the same compiler).

#include <cstdio>
#include <string>
#include <vector>

#include "effects.hpp"
#include "engine.hpp"
#include "harmony.hpp"
#include "layers.hpp"

enum LayerId {
  kDrone,
  kPad,
  kChoir,
  kBraam,
  kBooms,
  kDrums,
  kHeartbeat,
  kRisers,
  kBells,
  kWind,
  kOstinato,
  kNumLayers
};

inline const char* layerName(int id) {
  static const char* n[kNumLayers] = {"drone", "pad",    "choir", "braam", "booms",   "drums",
                                      "heartbeat", "risers", "bells", "wind",  "ostinato"};
  return n[id];
}

class Soundscape {
 public:
  using Options = EngineOptions;
  static std::vector<const char*> layerNames() {
    std::vector<const char*> v;
    for (int i = 0; i < kNumLayers; ++i) v.push_back(layerName(i));
    return v;
  }

  explicit Soundscape(const Options& o) : opt_(o), sr_(o.sr) {
    using dsp::mixSeed;
    ctx_.sr = sr_;
    rng_ = dsp::Rng(mixSeed(o.seed, 100));
    total_ = o.duration > 0 ? uint64_t(o.duration * double(sr_)) : 0;

    drone_.init(sr_, mixSeed(o.seed, 1), 65.0f);
    pad_.init(sr_, mixSeed(o.seed, 2));
    choir_.init(sr_, mixSeed(o.seed, 3));
    braam_.init(sr_, mixSeed(o.seed, 4));
    booms_.init(sr_, mixSeed(o.seed, 5));
    drums_.init(sr_, mixSeed(o.seed, 6));
    heart_.init(sr_, mixSeed(o.seed, 7));
    risers_.init(sr_, mixSeed(o.seed, 8));
    bells_.init(sr_, mixSeed(o.seed, 9));
    wind_.init(sr_, mixSeed(o.seed, 10));
    ost_.init(sr_, mixSeed(o.seed, 11));
    harm_.init(mixSeed(o.seed, 12), 38, 0);

    reverb_.init(sr_, 7.5f, 1.6f, 3800.0f, mixSeed(o.seed, 20));
    delay_.init(sr_, 2.0f);
    glue_.init(sr_, -14.0f, 2.0f, 0.02f, 0.5f);
    dcL_.setCutoff(22.0f, sr_);
    dcR_.setCutoff(22.0f, sr_);

    planMovement(0.0);
    const Section& s = sections_[0];
    applyMovement(s);
    if (opt_.log) {
      std::fprintf(stderr, "\n  seed %llu  ·  %s %s  ·  %.0f BPM  ·  %s\n\n",
                   (unsigned long long)o.seed, harmony::noteName(s.keyRoot), harmony::modeName(s.mode),
                   double(s.tempo), o.duration > 0 ? clock(o.duration).c_str() : "endless");
    }
    startSection(0);
  }

  bool done() const { return total_ > 0 && pos_ >= total_; }
  uint64_t position() const { return pos_; }
  uint64_t totalFrames() const { return total_; }
  uint64_t invalidSamples() const { return 0; }

  void render(float* outL, float* outR, int frames) {
    int off = 0;
    while (off < frames) {
      int n = std::min(32, frames - off);
      bus_.clear(n);
      scratch_.clear(n);
      tick(n);
      drone_.render(ctx_, busFor(kDrone));
      pad_.render(ctx_, busFor(kPad));
      choir_.render(ctx_, busFor(kChoir));
      braam_.render(ctx_, busFor(kBraam));
      booms_.render(ctx_, busFor(kBooms));
      drums_.render(ctx_, busFor(kDrums));
      heart_.render(ctx_, busFor(kHeartbeat));
      risers_.render(ctx_, busFor(kRisers));
      bells_.render(ctx_, busFor(kBells));
      wind_.render(ctx_, busFor(kWind));
      ost_.render(ctx_, busFor(kOstinato));

      for (int i = 0; i < n; ++i) {
        float dl, dr, rl, rr;
        delay_.process(bus_.dlyL[i], bus_.dlyR[i], dl, dr);
        reverb_.process(bus_.revL[i] + dl * 0.5f, bus_.revR[i] + dr * 0.5f, rl, rr);
        float l = dcL_.hp(bus_.dryL[i] + dl * 0.5f + rl * 0.7f);
        float r = dcR_.hp(bus_.dryR[i] + dr * 0.5f + rr * 0.7f);
        float g = glue_.gain(l, r) * 1.5f;
        double t = double(pos_ + uint64_t(i)) / double(sr_);
        float fade = dsp::smoothstep(0.0f, 4.0f, float(t));
        if (total_ > 0) fade *= dsp::smoothstep(0.0f, 8.0f, float(opt_.duration - t));
        outL[off + i] = dsp::softClip(l * g) * fade;
        outR[off + i] = dsp::softClip(r * g) * fade;
      }
      pos_ += uint64_t(n);
      off += n;
    }
  }

 private:
  struct Section {
    double start = 0, len = 0;
    float intensity = 0;
    bool ostinato = false, drums = false, heartbeat = false, choir = false;
    float bellRate = 0;  // bell motifs per second
    int movement = 0, keyRoot = 38, mode = 0;
    float tempo = 80;
  };
  enum EventType { kEvRiser, kEvBoom, kEvBraam };
  struct Event {
    double time;
    EventType type;
    float a;
  };

  layers::Bus& busFor(int id) { return (opt_.muted >> id) & 1u ? scratch_ : bus_; }

  static std::string clock(double t) {
    char buf[16];
    int s = int(t);
    std::snprintf(buf, sizeof buf, "%02d:%02d", s / 60, s % 60);
    return buf;
  }
  void log(const char* fmt, const std::string& a = "") {
    if (!opt_.log) return;
    std::fprintf(stderr, "  [%s]  ", clock(double(pos_) / double(sr_)).c_str());
    std::fprintf(stderr, fmt, a.c_str());
    std::fprintf(stderr, "\n");
  }

  // ---- 1. FORM -----------------------------------------------------------
  void planMovement(double start) {
    int m = movements_++;
    int keyRoot = 36 + rng_.irange(0, 6);  // C2 .. F#2
    int mode = rng_.irange(0, 2);
    if (m > 0) {  // endless mode: modulate to a related key
      static const int kMoves[6] = {-5, -3, -2, 2, 3, 5};
      const Section& prev = sections_.back();
      keyRoot = prev.keyRoot + kMoves[rng_.irange(0, 5)];
      while (keyRoot < 34) keyRoot += 12;
      while (keyRoot > 45) keyRoot -= 12;
      if (rng_.chance(0.5f)) mode = prev.mode;
    }
    float tempo = float(rng_.irange(68, 92));
    double bar = 240.0 / double(tempo);
    double len = opt_.duration > 0 ? opt_.duration : double(rng_.irange(60, 100)) * bar;
    float peak = rng_.range(0.55f, 0.75f);

    size_t first = sections_.size();
    double t = 0;
    while (t < len - 1e-6) {
      double sl = double(rng_.irange(6, 12)) * bar;
      if (len - (t + sl) < 4.0 * bar) sl = len - t;
      Section s;
      s.start = start + t;
      s.len = sl;
      s.movement = m;
      s.keyRoot = keyRoot;
      s.mode = mode;
      s.tempo = tempo;
      float p = float((t + sl * 0.5) / len);
      float arc = p < peak ? std::pow(p / peak, 1.6f) : std::pow((1.0f - p) / (1.0f - peak), 0.9f);
      s.intensity = dsp::clampf(0.08f + 0.85f * arc + rng_.range(-0.12f, 0.12f), 0.03f, 1.0f);
      sections_.push_back(s);
      t += sl;
    }
    size_t last = sections_.size() - 1;
    // guarantee one real climax, a quiet opening and a quiet ending
    size_t top = first;
    for (size_t k = first; k <= last; ++k)
      if (sections_[k].intensity > sections_[top].intensity) top = k;
    sections_[top].intensity = std::max(sections_[top].intensity, 0.9f);
    sections_[first].intensity = std::min(sections_[first].intensity, 0.12f);
    if (last > first) sections_[last].intensity = 0.06f;

    for (size_t k = first; k <= last; ++k) {
      Section& s = sections_[k];
      float I = s.intensity;
      s.ostinato = I > 0.42f && rng_.chance(0.85f);
      s.drums = I > 0.7f;
      s.heartbeat = !s.drums && I > 0.25f && I < 0.65f && rng_.chance(0.55f);
      s.choir = rng_.chance(0.2f + 0.5f * (1.0f - std::fabs(I - 0.5f) * 2.0f));
      s.bellRate = 0.03f + 0.12f * (1.0f - I);
    }
  }

  void applyMovement(const Section& s) {
    tempo_ = s.tempo;
    harm_.setKey(s.keyRoot, s.mode);
    ctx_.keyHz = dsp::mtof(float(s.keyRoot));
    drone_.setRoot(ctx_.keyHz);
    delay_.set(float(45.0 / double(tempo_)), 0.45f);  // dotted-8th echo
    nextChord_ = double(pos_) / double(sr_);
  }

  void startSection(size_t k) {
    if (k > 0 && sections_[k].movement != sections_[k - 1].movement) {
      applyMovement(sections_[k]);
      log("── new movement: %s", std::string(harmony::noteName(sections_[k].keyRoot)) + " " +
                                       harmony::modeName(sections_[k].mode));
    }
    const Section& s = sections_[k];
    ost_.newPattern();
    newDrumPattern();
    if (opt_.log) {
      std::string bar;
      for (int i = 0; i < 10; ++i) bar += (float(i) < s.intensity * 10.0f) ? "#" : ".";
      std::string what;
      if (s.ostinato) what += " ostinato";
      if (s.drums) what += " drums";
      if (s.heartbeat) what += " heartbeat";
      if (s.choir) what += " choir";
      char buf[160];
      std::snprintf(buf, sizeof buf, "section %zu  [%s] %.2f %s", k + 1, bar.c_str(), double(s.intensity),
                    what.c_str());
      log("%s", buf);
    }

    // Look ahead and set up the transition into the next section.
    if (k + 1 >= sections_.size() && opt_.duration <= 0) planMovement(movementEnd());
    if (k + 1 < sections_.size()) {
      const Section& n = sections_[k + 1];
      float jump = n.intensity - s.intensity;
      if (jump > 0.18f) {
        double rl = std::min(s.len, double(rng_.range(2.5f, 5.0f)));
        events_.push_back({n.start - rl, kEvRiser, float(rl)});
        events_.push_back({n.start, kEvBoom, n.intensity});
        if (n.intensity > 0.55f && rng_.chance(0.75f)) events_.push_back({n.start, kEvBraam, n.intensity});
      } else if (jump < -0.3f) {
        events_.push_back({n.start, kEvBoom, 0.5f});
      }
    }
  }

  double movementEnd() const { return sections_.back().start + sections_.back().len; }

  void newDrumPattern() {
    drumPat_.fill(0.0f);
    drumPat_[0] = 1.0f;
    if (rng_.chance(0.8f)) drumPat_[8] = 0.75f;
    for (int s = 1; s < 16; ++s)
      if (s != 8 && rng_.chance(0.16f)) drumPat_[size_t(s)] = rng_.range(0.35f, 0.6f);
    if (rng_.chance(0.5f)) {
      drumPat_[14] = 0.5f;
      drumPat_[15] = 0.4f;
    }
  }

  // ---- per-block control --------------------------------------------------
  void tick(int n) {
    double t = double(pos_) / double(sr_);
    double tEnd = double(pos_ + uint64_t(n)) / double(sr_);
    ctx_.time = t;

    while (cur_ + 1 < sections_.size() && t >= sections_[cur_ + 1].start) startSection(++cur_);
    const Section& s = sections_[cur_];

    float rate = sr_ / float(n);
    ctx_.intensity += (s.intensity - ctx_.intensity) * dsp::onePoleCoef(6.0f, rate);
    ctx_.choir += ((s.choir ? 1.0f : 0.0f) - ctx_.choir) * dsp::onePoleCoef(5.0f, rate);
    ctx_.ostinato += ((s.ostinato ? 1.0f : 0.0f) - ctx_.ostinato) * dsp::onePoleCoef(2.5f, rate);

    if (t >= nextChord_) {
      changeChord(t);
      static const int kCalm[4] = {4, 4, 6, 8};
      static const int kTense[3] = {2, 2, 4};
      int bars = ctx_.intensity > 0.6f ? kTense[rng_.irange(0, 2)] : kCalm[rng_.irange(0, 3)];
      nextChord_ += double(bars) * 240.0 / double(tempo_);
    }

    // scheduled events (sample-accurate where it matters)
    for (size_t i = 0; i < events_.size();) {
      const Event e = events_[i];
      if (e.time >= tEnd) {
        ++i;
        continue;
      }
      events_.erase(events_.begin() + long(i));
      int wait = std::max(0, int((e.time - t) * double(sr_)));
      switch (e.type) {
        case kEvRiser:
          risers_.trigger(e.a, 0.7f);
          log("riser (%s)", std::to_string(int(e.a + 0.5f)) + "s");
          break;
        case kEvBoom:
          booms_.trigger(boom(e.a), wait);
          log("%s", "BOOM");
          break;
        case kEvBraam:
          braam_.trigger(braamRoot(), e.a, rng_.chance(0.3f) ? rng_.range(1.0f, 2.0f) : 0.03f);
          lastBraam_ = t;
          log("%s", "BRAAM");
          break;
      }
    }

    // 16th-note sequencer
    double perSample = double(tempo_) / 60.0 * 4.0 / double(sr_);
    double from = step16_, to = step16_ + double(n) * perSample;
    for (double k = std::ceil(from); k < to; k += 1.0) onStep(s, (long long)k, int((k - from) / perSample));
    step16_ = to;

    // bells: random motifs, more of them when it's quiet
    if (rng_.chance(s.bellRate * float(n) / sr_)) bellMotif();
  }

  void onStep(const Section& s, long long k, int wait) {
    int st = int(k % 16);
    ost_.step(k, wait);
    if (s.drums && ctx_.intensity > 0.55f && drumPat_[size_t(st)] > 0.0f)
      drums_.trigger(taiko(drumPat_[size_t(st)]), wait);
    if (s.heartbeat && st % 4 == 0) {
      heart_.trigger(heartbeat(1.0f), wait);
      heart_.trigger(heartbeat(0.6f), wait + int(0.24f * sr_));
    }
    if (st == 0 && ctx_.intensity > 0.75f && rng_.chance(0.2f)) booms_.trigger(boom(0.6f), wait);
  }

  // ---- 4. HARMONY ---------------------------------------------------------
  void changeChord(double t) {
    const harmony::Chord& c = harm_.next(ctx_.intensity);
    std::vector<int> padNotes = c.upper;
    padNotes.insert(padNotes.begin(), c.bass);
    pad_.setChord(padNotes);

    std::vector<int> high;
    for (size_t i = c.upper.size() > 3 ? c.upper.size() - 3 : 0; i < c.upper.size(); ++i)
      high.push_back(c.upper[i] < 60 ? c.upper[i] + 12 : c.upper[i]);
    choir_.setChord(high);

    int r = 43 + (c.rootPc - 7 + 12) % 12;  // G2 .. F#3
    ost_.setTones({r, r + 12, r + 7, r + (c.minor ? 3 : 4)});

    log("chord %s", c.name + " (" + c.roman + ")");
    if (ctx_.intensity > 0.62f && t - lastBraam_ > 10.0 && rng_.chance(0.45f)) {
      braam_.trigger(braamRoot(), ctx_.intensity, rng_.chance(0.3f) ? rng_.range(1.0f, 2.0f) : 0.03f);
      lastBraam_ = t;
      log("%s", "BRAAM");
    }
  }

  int braamRoot() const { return 31 + (harm_.chord().rootPc - 7 + 12) % 12; }  // G1 .. F#2

  void bellMotif() {
    const harmony::Chord& c = harm_.chord();
    const int* scale = harmony::modeScale(harm_.mode());
    static const float kRatios[4] = {3.5f, 1.4142f, 2.76f, 5.19f};
    float ratio = kRatios[rng_.irange(0, 3)];
    int count = rng_.irange(1, 4);
    float gap = rng_.chance(0.5f) ? float(30.0 / double(tempo_)) : rng_.range(0.25f, 0.8f);
    for (int j = 0; j < count; ++j) {
      int pc = rng_.chance(0.85f) ? c.pcs[size_t(rng_.irange(0, int(c.pcs.size()) - 1))]
                                  : (harm_.keyRoot() + scale[rng_.irange(0, 6)]) % 12;
      int note = 74 + (pc - 74 % 12 + 12) % 12 + (rng_.chance(0.35f) ? 12 : 0);
      // one draw per statement: argument evaluation order differs between compilers
      float v = rng_.range(0.05f, 0.1f);
      float t60 = rng_.range(3.0f, 8.0f);
      float index = rng_.range(1.5f, 4.0f);
      float pn = rng_.range(-0.8f, 0.8f);
      bells_.trigger(note, v, t60, ratio, index, pn, int(float(j) * gap * sr_));
    }
  }

  // ---- impact presets -----------------------------------------------------
  layers::ImpactParams boom(float size) {
    layers::ImpactParams p;
    p.f0 = 110.0f + 40.0f * size;
    p.f1 = ctx_.keyHz * 0.5f;  // tuned to the key
    p.pitchT = 0.06f;
    p.t60 = 1.5f + 3.0f * size;
    p.noise = 0.6f;
    p.noiseT60 = 0.25f;
    p.noiseCut = 3000.0f;
    p.amp = 0.5f * (0.5f + 0.5f * size);
    p.rev = 0.6f;
    return p;
  }
  layers::ImpactParams taiko(float vel) {
    layers::ImpactParams p;
    p.f0 = 190.0f;
    p.f1 = ctx_.keyHz;
    p.pitchT = 0.025f;
    p.t60 = 0.9f;
    p.noise = 0.35f;
    p.noiseT60 = 0.08f;
    p.noiseCut = 1500.0f;
    p.amp = 0.38f * vel;
    p.pan = rng_.range(-0.3f, 0.3f);
    p.rev = 0.35f;
    return p;
  }
  layers::ImpactParams heartbeat(float vel) {
    layers::ImpactParams p;
    p.f0 = 75.0f;
    p.f1 = 42.0f;
    p.pitchT = 0.04f;
    p.t60 = 0.45f;
    p.noise = 0.0f;
    p.amp = 0.32f * vel;
    p.rev = 0.12f;
    return p;
  }

  Options opt_;
  float sr_;
  uint64_t pos_ = 0, total_ = 0;
  layers::Context ctx_;
  layers::Bus bus_, scratch_;
  dsp::Rng rng_;
  harmony::Harmony harm_;

  layers::Drone drone_;
  layers::Pad pad_;
  layers::Choir choir_;
  layers::Braam braam_;
  layers::Impacts booms_, drums_, heart_;
  layers::Risers risers_;
  layers::Bells bells_;
  layers::Wind wind_;
  layers::Ostinato ost_;

  dsp::PingPong delay_;
  dsp::Reverb reverb_;
  dsp::Glue glue_;
  dsp::OnePole dcL_, dcR_;

  std::vector<Section> sections_;
  std::vector<Event> events_;
  std::array<float, 16> drumPat_{};
  size_t cur_ = 0;
  int movements_ = 0;
  float tempo_ = 80.0f;
  double nextChord_ = 0.0, step16_ = 0.0, lastBraam_ = -100.0;
};
