#pragma once
// electroacoustic.hpp — the composer for the electroacoustic style.
//
// There are no chords, no tempo and no melody. The piece is a chain of
// SCENES. Each scene is a world with its own materials and behaviour:
//
//   METAL    plates, bells and bars: struck, scraped, bounced, bowed
//   DUST     glass and wood under a fine rain of tiny impulses, pointillist
//   BOWED    singing bowls and bells, endless swells, smeared grains
//   SWARM    dense, fast granular storm; everything flying around
//   STRINGS  prepared strings: plucked, rattling, sliding in pitch
//   VOID     almost nothing: pressure, air, a distant event, long tails
//
// Coherence without harmony: each scene picks a SPECTRUM (a set of related
// frequencies, e.g. the partials of a stretched harmonic series). Every
// object in the scene is tuned to it, so all its sounds belong together
// even though nothing is "in a key". Spectralist composers (Grisey, Murail)
// work the same way.
//
// Form: an energy arc across the piece, a contour inside each scene (rise,
// fall, arch) and two kinds of transition. A crossfade lets one world morph
// into the next. A HARD CUT chokes everything into silence (only the reverb
// tail survives), waits, then opens the new world with an attack. That
// montage cut is a signature gesture of acousmatic music.

#include <cstdio>
#include <initializer_list>
#include <string>
#include <vector>

#include "acoustic.hpp"
#include "effects.hpp"
#include "engine.hpp"
#include "textures.hpp"

class Electroacoustic {
 public:
  enum Layer { kResonators, kStrings, kGrains, kGlitter, kSweeps, kAir, kSub, kNumLayers };
  static std::vector<const char*> layerNames() {
    return {"resonators", "strings", "grains", "glitter", "sweeps", "air", "sub"};
  }

  explicit Electroacoustic(const EngineOptions& o) : opt_(o), sr_(o.sr) {
    using dsp::mixSeed;
    rng_ = dsp::Rng(mixSeed(o.seed, 200));
    noise_ = dsp::Rng(mixSeed(o.seed, 201));
    total_ = o.duration > 0 ? uint64_t(o.duration * double(sr_)) : 0;
    for (auto& ob : obj_) ob.str.init(sr_);
    gran_.init(sr_, mixSeed(o.seed, 202), 12.0f);
    sweeps_.init(sr_, mixSeed(o.seed, 203));
    air_.init(sr_, mixSeed(o.seed, 204));
    sub_.init(sr_, mixSeed(o.seed, 205));
    reverb_.init(sr_, 5.0f, 1.3f, 7000.0f, mixSeed(o.seed, 206));
    glue_.init(sr_, -16.0f, 2.0f, 0.02f, 0.5f);
    dcL_.setCutoff(20.0f, sr_);
    dcR_.setCutoff(20.0f, sr_);
    if (opt_.log)
      std::fprintf(stderr, "\n  seed %llu  ·  electroacoustic  ·  %s\n\n", (unsigned long long)o.seed,
                   o.duration > 0 ? clock(o.duration).c_str() : "endless");
    planNext();
    enterScene(0);
  }

  bool done() const { return total_ > 0 && pos_ >= total_; }
  uint64_t position() const { return pos_; }
  uint64_t totalFrames() const { return total_; }
  uint64_t invalidSamples() const { return invalid_; }

  void render(float* outL, float* outR, int frames) {
    int off = 0;
    while (off < frames) {
      int n = std::min(32, frames - off);
      bus_.clear(n);
      scratch_.clear(n);
      tick(n);
      for (auto& ob : obj_)
        if (ob.active) renderObject(ob, busFor(ob.isString ? kStrings : kResonators));
      gran_.render(busFor(kGrains), busFor(kGlitter));
      sweeps_.render(busFor(kSweeps));
      air_.render(busFor(kAir));
      sub_.render(busFor(kSub));

      for (int i = 0; i < n; ++i) {
        // Muted layers still feed the tape, so `--solo grains` still works.
        gran_.record(finite(bus_.tape[i] + scratch_.tape[i]));
        float rl, rr;
        reverb_.process(finite(bus_.revL[i]), finite(bus_.revR[i]), rl, rr);
        float l = dcL_.hp(finite(bus_.dryL[i]) + rl * 0.8f);
        float r = dcR_.hp(finite(bus_.dryR[i]) + rr * 0.8f);
        float g = glue_.gain(l, r) * 1.4f;
        double t = double(pos_ + uint64_t(i)) / double(sr_);
        float fade = dsp::smoothstep(0.0f, 2.0f, float(t));
        if (total_ > 0) fade *= dsp::smoothstep(0.0f, 8.0f, float(opt_.duration - t));
        outL[off + i] = dsp::softClip(l * g) * fade;
        outR[off + i] = dsp::softClip(r * g) * fade;
      }
      pos_ += uint64_t(n);
      off += n;
    }
  }

 private:
  using Material = acoustic::Material;
  using Path = acoustic::Spatial::Path;
  enum SceneType { Metal, Dust, Bowed, Swarm, Strings, Void, kNumScenes };

  struct Scene {
    SceneType type = Void;
    double start = 0, len = 10, gap = 0;
    float energy = 0;
    bool cut = false;
    int contour = 0;  // 0 steady, 1 rising, 2 falling, 3 arch
    int spectrum = 0;
    float f0 = 60, stretch = 1;
    std::vector<float> field;
  };

  struct Object {
    bool active = false, isString = false, choking = false;
    int wait = 0, quietBlocks = 0;
    float gain = 1, age = 0, maxLife = 30, choke = 1, chokeK = 1, lastPeak = 0;
    acoustic::Exciter ex;
    acoustic::ModalBody body;
    acoustic::StringModel str;
    acoustic::Spatial sp;
  };

  static const char* sceneName(int t) {
    static const char* n[kNumScenes] = {"METAL", "DUST", "BOWED", "SWARM", "STRINGS", "VOID"};
    return n[t];
  }
  // Safety net: a single NaN reaching the reverb or the tape would poison
  // them for the rest of the piece. Drop it instead.
  float finite(float x) {
    if (std::isfinite(x)) return x;
    ++invalid_;
    return 0.0f;
  }
  textures::EBus& busFor(int id) { return (opt_.muted >> id) & 1u ? scratch_ : bus_; }

  static std::string clock(double t) {
    char buf[16];
    int s = int(t);
    std::snprintf(buf, sizeof buf, "%02d:%02d", s / 60, s % 60);
    return buf;
  }
  void log(const std::string& msg) {
    if (opt_.log)
      std::fprintf(stderr, "  [%s]  %s\n", clock(double(pos_) / double(sr_)).c_str(), msg.c_str());
  }
  int pickWeighted(std::initializer_list<float> w) {
    float total = 0.0f;
    for (float x : w) total += x;
    float r = rng_.uni() * total;
    int i = 0;
    for (float x : w) {
      r -= x;
      if (r < 0.0f) return i;
      ++i;
    }
    return int(w.size()) - 1;
  }

  // ---- FORM ---------------------------------------------------------------
  void planNext() {
    Scene s;
    bool first = scenes_.empty();
    const Scene* prev = first ? nullptr : &scenes_.back();
    s.start = first ? 0.0 : prev->start + prev->len;

    double p;
    if (opt_.duration > 0) {
      p = std::min(1.0, (s.start + 10.0) / opt_.duration);
    } else {
      p = std::fmod(s.start, 300.0) / 300.0;  // endless: a new arc every 5 minutes
    }
    float peak = 0.62f;
    float pf = float(p);
    float arc = pf < peak ? std::pow(pf / peak, 1.4f) : std::pow((1.0f - pf) / (1.0f - peak), 0.9f);
    s.energy = dsp::clampf(0.12f + 0.8f * arc + rng_.range(-0.18f, 0.18f), 0.03f, 1.0f);

    bool ending = opt_.duration > 0 && opt_.duration - s.start < 60.0;
    float E = s.energy;
    float w[kNumScenes] = {1.3f, 1.5f * (1.0f - E) + 0.2f, 1.2f, 3.0f * E * E, 1.0f, 2.0f * (1.0f - E) * (1.0f - E)};
    if (first || ending) {
      w[Metal] = w[Swarm] = w[Strings] = 0.0f;
      s.energy = std::min(s.energy, ending ? 0.15f : 0.3f);
    }
    if (prev) w[prev->type] = 0.0f;
    float total = 0.0f;
    for (float x : w) total += x;
    float r = rng_.uni() * total;
    int type = 0;
    for (; type < kNumScenes - 1; ++type) {
      r -= w[type];
      if (r < 0.0f) break;
    }
    s.type = SceneType(type);

    static const float kLen[kNumScenes][2] = {{20, 45}, {20, 45}, {25, 55}, {15, 35}, {20, 45}, {8, 20}};
    s.len = double(rng_.range(kLen[type][0], kLen[type][1]));
    if (opt_.duration > 0 && opt_.duration - (s.start + s.len) < 15.0) s.len = opt_.duration - s.start;

    s.cut = !first && rng_.chance(0.35f + 0.3f * std::fabs(s.energy - prev->energy));
    s.gap = s.cut ? double(rng_.range(0.3f, 2.0f) * (s.type == Void ? 2.0f : 1.0f)) : 0.0;
    s.contour = rng_.irange(0, 3);
    makeSpectrum(s);
    scenes_.push_back(s);
  }

  // The scene's pitch world: ~9 partials of one fundamental.
  void makeSpectrum(Scene& s) {
    s.f0 = 40.0f * std::exp2(rng_.range(0.0f, 1.6f));
    s.spectrum = rng_.irange(0, 2);
    s.stretch = rng_.range(0.97f, 1.08f);
    s.field.clear();
    if (s.spectrum == 2) {
      for (float rr : acoustic::material(Material::Bell).ratios) s.field.push_back(s.f0 * 2.0f * rr);
    } else {
      s.field.push_back(s.f0);
      while (s.field.size() < 9) {
        int k = rng_.chance(0.7f) ? rng_.irange(2, 16) : rng_.irange(17, 32);
        if (s.spectrum == 1 && k % 2 == 0) ++k;  // odd partials only: hollow, clarinet-like
        float f = s.f0 * std::pow(float(k), s.stretch);
        bool dup = false;
        for (float x : s.field) dup |= std::fabs(x - f) < 1.0f;
        if (!dup) s.field.push_back(f);
      }
    }
    std::sort(s.field.begin(), s.field.end());
  }

  std::string spectrumName(const Scene& s) const {
    char buf[64];
    if (s.spectrum == 0)
      std::snprintf(buf, sizeof buf, "harmonic series on %.0f Hz, stretched x%.2f", double(s.f0), double(s.stretch));
    else if (s.spectrum == 1)
      std::snprintf(buf, sizeof buf, "odd partials of %.0f Hz", double(s.f0));
    else
      std::snprintf(buf, sizeof buf, "bell spectrum on %.0f Hz", double(s.f0 * 2.0f));
    return buf;
  }

  void enterScene(size_t k) {
    cur_ = k;
    opened_ = false;
    const Scene& s = scenes_[k];
    if (s.cut) {
      for (auto& ob : obj_)
        if (ob.active) startChoke(ob, 0.012f);
      gran_.choke();
      gran_.freeze(false);
      sweeps_.choke();
      air_.setLevel(0.0f, true);
      sub_.setLevel(0.0f, true);
    }
    gran_.setField(s.field);
    float airA = s.field[size_t(rng_.irange(1, int(s.field.size()) - 1))];
    float airB = s.field[size_t(rng_.irange(1, int(s.field.size()) - 1))];
    air_.setPitches(airA, airB);
    float sh = s.f0;
    while (sh > 50.0f) sh *= 0.5f;
    while (sh < 28.0f) sh *= 2.0f;
    sub_.setPitch(sh);
    if (opt_.log) {
      std::string bar;
      for (int i = 0; i < 10; ++i) bar += float(i) < s.energy * 10.0f ? "#" : ".";
      static const char* kContour[4] = {"steady", "rising", "falling", "arch"};
      char buf[200];
      std::snprintf(buf, sizeof buf, "%s── %-7s [%s] %s  ·  %s", s.cut ? "CUT " : "", sceneName(s.type), bar.c_str(),
                    kContour[s.contour], spectrumName(s).c_str());
      log(buf);
    }
    if (k + 1 >= scenes_.size() && (opt_.duration <= 0 || s.start + s.len < opt_.duration - 1e-6)) planNext();
  }

  void openScene() {
    opened_ = true;
    const Scene& s = scenes_[cur_];
    double remain = s.start + s.len - now();
    float len = float(remain) + 4.0f;
    if (s.cut && s.type != Void) {
      if (rng_.chance(0.7f)) {
        static const Material kOnset[3] = {Material::Plate, Material::Bell, Material::Skin};
        Material m = kOnset[rng_.irange(0, 2)];
        float f = pickFreq(m);
        float decay = rng_.range(0.8f, 1.4f);
        Object& o = newModal(m, f, decay, 0.9f, 0.7f, Path::Still, 1.0f, 0);
        o.ex.strike(0.9f, 0.9f, sr_);
        log(std::string("attack · ") + acoustic::material(m).name);
      } else {
        gPluck(false, 1.0f);
        log("attack · string");
      }
    }
    switch (s.type) {
      case Metal:
        if (rng_.chance(0.6f)) gBow(rng_.chance(0.5f) ? Material::Plate : Material::Bell, len, 0.3f);
        break;
      case Dust: {
        gDust(Material::Glass, len, rng_.range(2.0f, 8.0f), 0.3f);
        Material m = rng_.chance(0.5f) ? Material::Bar : Material::Wood;
        gDust(m, len, rng_.range(1.0f, 4.0f), 0.35f);
        break;
      }
      case Bowed: {
        gBow(Material::Bowl, rng_.range(10.0f, 20.0f), 0.45f);
        Material m = rng_.chance(0.5f) ? Material::Bell : Material::Glass;
        float dur = rng_.range(10.0f, 20.0f);
        gBow(m, dur, 0.35f, int(rng_.range(2.0f, 6.0f) * sr_));
        break;
      }
      case Strings:
        if (rng_.chance(0.6f)) gStringBow(len, 0.35f);
        break;
      case Swarm:
        gBurst();
        break;
      case Void:
      default:
        break;
    }
  }

  // ---- per-block control ----------------------------------------------------
  double now() const { return double(pos_) / double(sr_); }

  void tick(int n) {
    double t = now();
    float dt = float(n) / sr_;
    while (cur_ + 1 < scenes_.size() && t >= scenes_[cur_ + 1].start) enterScene(cur_ + 1);
    const Scene& s = scenes_[cur_];
    double local = t - s.start;
    float p = dsp::clampf(float(local / s.len), 0.0f, 1.0f);
    float c = s.contour == 0 ? 1.0f
              : s.contour == 1 ? 0.55f + 0.7f * p
              : s.contour == 2 ? 1.25f - 0.7f * p
                               : 0.6f + 0.6f * std::sin(dsp::kPi * p);
    energy_ = dsp::clampf(s.energy * c, 0.0f, 1.0f);
    bool inGap = local < s.gap;
    if (!inGap && !opened_) openScene();

    // textures follow the scene
    textures::Granulator::Params gp = grainParams(s.type, energy_);
    if (inGap) gp.density = gp.amp = 0.0f;
    gran_.setParams(gp);
    static const float kAirLvl[kNumScenes] = {0.25f, 0.45f, 0.2f, 0.2f, 0.15f, 0.4f};
    static const float kSubLvl[kNumScenes] = {0.25f, 0.05f, 0.5f, 0.45f, 0.3f, 0.6f};
    air_.setLevel(inGap ? 0.0f : kAirLvl[s.type] * (0.6f + 0.6f * energy_));
    sub_.setLevel(inGap ? 0.0f : kSubLvl[s.type] * (0.5f + 0.5f * energy_));

    // grain burst envelope: a cloud that thickens and thins out again
    if (burstDur_ > 0.0f) {
      burstT_ += dt;
      float bp = burstT_ / burstDur_;
      gran_.setBoost(bp < 1.0f ? burstAmt_ * std::pow(std::sin(dsp::kPi * bp), 2.0f) : 0.0f);
      if (bp >= 1.0f) burstDur_ = 0.0f;
    }
    if (gran_.frozen() && t >= freezeUntil_) {
      gran_.freeze(false);
      log("thaw");
    }

    // gestures: a Poisson process, denser with more energy
    bool ending = total_ > 0 && t > opt_.duration - 6.0;
    static const float kRate[kNumScenes] = {0.5f, 0.3f, 0.18f, 1.3f, 0.6f, 0.07f};
    if (opened_ && !ending && rng_.chance(kRate[s.type] * (0.4f + 1.2f * energy_) * dt)) gesture(s.type);
  }

  textures::Granulator::Params grainParams(SceneType type, float E) const {
    textures::Granulator::Params g;
    switch (type) {
      case Metal:
        g = {8 + 20 * E, 90, 0.5f, 3, 0.1f, 0.2f, 0.05f, 0.0f, 1.00f, {1.0f, 0.5f, 2.0f}};
        break;
      case Dust:
        g = {14 + 10 * E, 30, 0.3f, 4, 0.0f, 0.1f, 0.0f, 0.6f, 1.00f, {1.0f, 2.0f, 4.0f}};
        break;
      case Bowed:
        g = {20 + 10 * E, 320, 0.5f, 2, 0.06f, 0.4f, 0.0f, 0.0f, 1.10f, {1.0f, 0.5f, 1.5f}};
        break;
      case Swarm:
        g = {50 + 130 * E, 60, 0.2f, 3, 1.5f, 0.35f, 0.3f, 0.2f, 1.00f, {0.25f, 0.5f, 1.0f, 2.0f, 3.0f}};
        break;
      case Strings:
        g = {8 + 14 * E, 140, 0.4f, 3, 0.05f, 0.5f, 0.1f, 0.0f, 1.00f, {1.0f, 0.5f, 0.75f}};
        break;
      case Void:
        g = {2, 300, 1.0f, 6, 0.0f, 0.5f, 0.0f, 0.3f, 0.80f, {0.5f, 1.0f}};
        break;
      default:
        break;
    }
    return g;
  }

  void gesture(SceneType type) {
    using M = Material;
    switch (type) {
      case Metal:
        switch (pickWeighted({4, 1.5f, 2, 1, 1, 0.5f, 0.5f})) {
          case 0: gStrike(pick3(M::Plate, M::Bell, M::Bar)); break;
          case 1: gTaps(pick3(M::Bar, M::Wood, M::Bell), 0); break;
          case 2: gScrape(M::Plate); break;
          case 3: gSweep(); break;
          case 4: gBurst(); break;
          case 5: gFreeze(); break;
          default: gBow(M::Plate, rng_.range(6, 14), 0.3f); break;
        }
        break;
      case Dust:
        switch (pickWeighted({3, 1.5f, 1, 1})) {
          case 0: gStrike(M::Glass); break;
          case 1: gTaps(M::Glass, rng_.irange(0, 2)); break;
          case 2: gSweep(); break;
          default: gBurst(); break;
        }
        break;
      case Bowed:
        switch (pickWeighted({3, 1.5f, 1, 0.7f})) {
          case 0: {
            Material m = pick3(M::Bowl, M::Bell, M::Glass);
            gBow(m, rng_.range(8, 20), 0.4f);
            break;
          }
          case 1: gStrike(M::Bowl, 0.25f); break;
          case 2: gFreeze(); break;
          default: gSweep(); break;
        }
        break;
      case Swarm:
        switch (pickWeighted({3, 1.5f, 1.5f, 1.5f, 2, 0.7f, 0.4f})) {
          case 0: gStrike(pick3(M::Plate, M::Skin, M::Glass)); break;
          case 1: {
            Material m = pick3(M::Wood, M::Bar, M::Glass);
            gTaps(m, rng_.irange(0, 2));
            break;
          }
          case 2: gScrape(pick3(M::Plate, M::Bell, M::Bar)); break;
          case 3: gSweep(); break;
          case 4: gBurst(); break;
          case 5: gPluck(rng_.chance(0.5f)); break;
          default: gFreeze(); break;
        }
        break;
      case Strings:
        switch (pickWeighted({4, 1.5f, 1.5f, 1, 1, 0.5f})) {
          case 0: gPluck(false); break;
          case 1: gPluck(true); break;
          case 2: gStrike(rng_.chance(0.5f) ? M::Wood : M::Skin); break;
          case 3: gScrape(M::Plate); break;
          case 4: gTaps(M::Wood, rng_.irange(0, 2)); break;
          default: gSweep(); break;
        }
        break;
      case Void:
        switch (pickWeighted({2, 1, 0.5f})) {
          case 0: gStrike(pick3(M::Bell, M::Bowl, M::Plate), 1.0f, true); break;
          case 1: gSweep(); break;
          default: gBurst(); break;
        }
        break;
      default:
        break;
    }
  }

  // ---- gestures --------------------------------------------------------------
  Material pick3(Material a, Material b, Material c) {
    int i = rng_.irange(0, 2);
    return i == 0 ? a : i == 1 ? b : c;
  }
  float pickFreq(Material m) {
    const auto& d = acoustic::material(m);
    return pickFreq(d.lo, d.hi);
  }
  // A frequency from the scene's spectrum, moved by octaves into [lo, hi].
  float pickFreq(float lo, float hi) {
    const auto& f = scenes_[cur_].field;
    float x = f[size_t(rng_.irange(0, int(f.size()) - 1))];
    while (x < lo) x *= 2.0f;
    while (x > hi) x *= 0.5f;
    return std::max(x, 20.0f);
  }
  float vel(float base) { return base * (0.45f + 0.55f * energy_) * rng_.range(0.7f, 1.0f); }

  Object& alloc() {
    for (auto& o : obj_)
      if (!o.active) return o;
    Object* best = &obj_[0];
    for (auto& o : obj_)
      if (o.lastPeak < best->lastPeak) best = &o;
    return *best;
  }
  Object& prep(bool isString, float gain, Path path, float pathDur, int wait, float life) {
    Object& o = alloc();
    o.active = true;
    o.isString = isString;
    o.choking = false;
    o.choke = 1.0f;
    o.wait = wait;
    o.quietBlocks = 0;
    o.gain = gain;
    o.age = 0.0f;
    o.maxLife = life + 30.0f;
    o.lastPeak = 1.0f;
    o.sp.set(path, pathDur, rng_);
    return o;
  }
  Object& newModal(Material m, float f, float decay, float bright, float gain, Path path, float pathDur, int wait,
                   float life = 0.0f) {
    Object& o = prep(false, gain, path, pathDur, wait, life);
    o.body.setup(m, f, decay, bright, sr_, rng_);
    return o;
  }
  void startChoke(Object& o, float seconds) {
    o.choking = true;
    o.chokeK = dsp::decayCoef(seconds, sr_);
  }

  void gStrike(Material m, float level = 1.0f, bool far = false) {
    float hard = rng_.range(0.2f, 1.0f);
    static const Path kPaths[3] = {Path::Still, Path::Still, Path::Drift};
    float f = pickFreq(m);
    float decay = rng_.range(0.6f, 1.4f);
    Path path = kPaths[rng_.irange(0, 2)];
    float pathDur = rng_.range(2.0f, 8.0f);
    Object& o = newModal(m, f, decay, hard, 0.5f, path, pathDur, 0);
    if (far) o.sp.distant(rng_);
    o.ex.strike(vel(0.9f) * level, hard, sr_);
  }
  // kind 0: bouncing, 1: slowing taps, 2: accelerating crescendo
  void gTaps(Material m, int kind) {
    float hard = rng_.range(0.4f, 1.0f);
    float f = pickFreq(m);
    float decay = rng_.range(0.5f, 1.0f);
    Object& o = newModal(m, f, decay, hard, 0.5f, Path::Drift, 4.0f, 0, 6.0f);
    float v = vel(0.8f);
    if (kind == 0) {
      float interval = rng_.range(0.25f, 0.45f), ratio = rng_.range(0.72f, 0.85f);
      o.ex.taps(v, hard, interval, ratio, 0.8f, rng_.irange(8, 16), sr_);
    } else if (kind == 1) {
      float interval = rng_.range(0.08f, 0.15f), ratio = rng_.range(1.12f, 1.2f);
      o.ex.taps(v, hard, interval, ratio, 0.96f, rng_.irange(6, 12), sr_);
    } else {
      o.ex.taps(v * 0.4f, hard, 0.6f, 0.82f, 1.08f, 12, sr_);
    }
    static const char* kKind[3] = {"bounce", "slowing taps", "accelerando"};
    log(std::string(kKind[kind]) + " · " + acoustic::material(m).name);
  }
  void gScrape(Material m) {
    float dur = rng_.range(0.6f, 4.0f);
    float f = pickFreq(m);
    float decay = rng_.range(0.6f, 1.2f);
    Object& o = newModal(m, f, decay, 0.7f, 0.55f, Path::Drift, dur, 0, dur);
    float v = vel(0.8f);
    o.ex.scrape(v, dur, rng_.uni(), sr_);
    log("scrape · " + std::string(acoustic::material(m).name));
  }
  void gBow(Material m, float dur, float level, int wait = 0) {
    const auto& d = acoustic::material(m);
    float f = pickFreq(d.lo, d.lo * 2.5f);
    float decay = rng_.range(1.0f, 1.5f);
    Object& o = newModal(m, f, decay, 0.3f, 0.5f, Path::Drift, dur, wait, dur);
    o.ex.bow(level * rng_.range(0.8f, 1.0f), dur, sr_);
    log("bow · " + std::string(d.name));
  }
  void gDust(Material m, float dur, float rate, float level) {
    float decay = rng_.range(0.5f, 1.0f);
    Object& o = newModal(m, pickFreq(m), decay, 0.8f, 0.5f, Path::Drift, dur, 0, dur);
    // Many hits on a long-ringing body pile up: scale by the expected overlap.
    float overlap = rate * acoustic::material(m).t60 * decay * 0.5f;
    o.ex.dust(level / std::sqrt(1.0f + overlap), rate, dur, sr_);
    log("dust · " + std::string(acoustic::material(m).name));
  }
  void gPluck(bool glide, float level = 1.0f) {
    float f = pickFreq(40.0f, 330.0f);
    float rattle = rng_.chance(0.5f) ? rng_.range(0.2f, 0.6f) : 0.0f;
    Path path = rng_.chance(0.7f) ? Path::Still : Path::Drift;
    float pathDur = rng_.range(2.0f, 6.0f);
    Object& o = prep(true, 0.5f, path, pathDur, 0, 8.0f);
    float t60 = rng_.range(2.0f, 8.0f), bright = rng_.range(0.2f, 0.8f);
    o.str.setup(f, t60, bright, rattle);
    float v = vel(0.7f) * level;
    o.ex.strike(v, rng_.range(0.3f, 0.9f), sr_);
    if (glide) {
      static const float kTo[4] = {0.5f, 0.75f, 1.5f, 2.0f};
      float to = f * kTo[rng_.irange(0, 3)];
      o.str.glideTo(to, rng_.range(0.5f, 3.0f));
      log("string glide");
    } else if (rattle > 0.0f) {
      log("rattling string");
    }
  }
  void gStringBow(float dur, float level) {
    Object& o = prep(true, 0.5f, Path::Drift, dur, 0, dur);
    o.str.setup(pickFreq(40.0f, 120.0f), 6.0f, 0.3f, 0.0f);
    o.ex.bow(level, dur, sr_);
    log("bowed string");
  }
  void gSweep() {
    bool up = rng_.chance(0.5f);
    float a = rng_.range(150.0f, 500.0f), b = rng_.range(2500.0f, 7000.0f);
    static const Path kPaths[4] = {Path::Flyby, Path::Approach, Path::Recede, Path::Drift};
    int pi = rng_.irange(0, 3);
    float dur = rng_.range(0.6f, 3.0f);
    float v = vel(1.0f);
    int shape = rng_.irange(0, 1);
    sweeps_.trigger(up ? a : b, up ? b : a, dur, v, shape, kPaths[pi]);
    static const char* kName[4] = {"fly-by", "approaching", "receding", "drifting"};
    log(std::string("sweep ") + (up ? "up" : "down") + " · " + kName[pi]);
  }
  void gBurst() {
    burstT_ = 0.0f;
    burstDur_ = rng_.range(1.0f, 4.0f);
    burstAmt_ = rng_.range(60.0f, 200.0f);
    log("grain burst");
  }
  void gFreeze() {
    if (gran_.frozen()) return;
    float secs = rng_.range(3.0f, 10.0f);
    gran_.freeze(true);
    freezeUntil_ = now() + double(secs);
    log("freeze " + std::to_string(int(secs + 0.5f)) + "s");
  }

  void renderObject(Object& o, textures::EBus& b) {
    if (o.wait >= b.n) {
      o.wait -= b.n;
      return;
    }
    int start = o.wait;
    o.wait = 0;
    float dt = float(b.n) / sr_;
    o.sp.block(dt, sr_);
    o.age += dt;
    if (!o.choking && o.age > o.maxLife) startChoke(o, 0.5f);
    float peak = 0.0f;
    for (int i = start; i < b.n; ++i) {
      float xi, xc, l, r;
      o.ex.process(noise_, xi, xc);
      if (o.isString) {
        l = r = o.str.process(xi, xc, noise_);
      } else {
        o.body.process(xi, xc, l, r);
      }
      float g = o.gain * (o.isString ? 6.0f : 1.8f);  // a string radiates less than a plate
      if (o.choking) {
        o.choke *= o.chokeK;
        g *= o.choke;
      }
      l *= g;
      r *= g;
      o.sp.apply(l, r);
      peak = std::max(peak, std::fabs(l) + std::fabs(r));
      b.add(i, l, r, o.sp.dry(), o.sp.wet(), 0.8f);
    }
    o.lastPeak = peak;
    if (o.choking && o.choke < 1e-4f) o.active = false;
    if (o.ex.done()) {
      o.quietBlocks = peak < 3e-5f ? o.quietBlocks + 1 : 0;
      if (o.quietBlocks > 8) o.active = false;
    }
  }

  EngineOptions opt_;
  float sr_;
  uint64_t pos_ = 0, total_ = 0, invalid_ = 0;
  dsp::Rng rng_, noise_;
  textures::EBus bus_, scratch_;
  std::array<Object, 32> obj_;
  textures::Granulator gran_;
  textures::Sweeps sweeps_;
  textures::Air air_;
  textures::Sub sub_;
  dsp::Reverb reverb_;
  dsp::Glue glue_;
  dsp::OnePole dcL_, dcR_;

  std::vector<Scene> scenes_;
  size_t cur_ = 0;
  bool opened_ = false;
  float energy_ = 0.0f, burstT_ = 0.0f, burstDur_ = 0.0f, burstAmt_ = 0.0f;
  double freezeUntil_ = 0.0;
};
