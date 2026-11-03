#pragma once
// ritual.hpp — a composed 4:30 dark ritual track: tekno drive crossed with
// ambient, IDM and a cinematic industrial atmosphere.
//
// Unlike the other two styles this one is ARRANGED: a fixed timeline of
// 148 bars (4:21 at 136 BPM) plus a tail = 4:30. The seed picks the key,
// the rhythmic patterns, the cymbal tunings, the glitches and every whale
// call and field-recording event, so each seed is a different take on the
// same arrangement.
//
//   bars   0-20   AWAKENING     bowed tortured cymbals, field recordings, whales
//   bars  20-36   GATHERING     primal percussion in polymeters, muffled bass
//   bars  36-44   THE CALL      kick enters, the bass filter opens, riser, silence
//   bars  44-80   RITUAL I      kick + distorted echoing bass + percussion + clank
//   bars  80-100  DESCENT       beat drops out: whales, cymbals, neurobass, drum roll
//   bars 100-132  RITUAL II     the peak: neurobass, anvil, bones, more glitch
//   bars 132-148  DISSOLUTION   instruments leave one by one
//
// RHYTHM. Everything sits on a 16th-note grid, but the patterns have
// different lengths, so they cycle against the 16-step bar (polymeter):
//   frame drums 12 steps (5 hits)   -> 3 against 4
//   log drum     7 steps (3 hits)   -> realigns with the bar every 7 bars
//   anvil        5 steps, bones 9 steps, shaker accents every 3 steps
//   bass         3 notes per beat but a 7-note pitch cycle
//   neurobass    gate on 12 steps; its two LFOs repeat every 6 and 5 steps
// The kick is a plain four-on-the-floor anchor; everything else rotates
// around it. At the end of phrases the IDM stutter grabs a slice of the
// beat and loops it, sliding its pitch and crushing its bits.

#include <cstdio>
#include <string>
#include <vector>

#include "effects.hpp"
#include "engine.hpp"
#include "harmony.hpp"
#include "ritual_instruments.hpp"
#include "textures.hpp"

class RitualTrack {
 public:
  enum Layer { kCymbals, kWhales, kField, kAtmosphere, kKick, kPerc, kBass, kNeuro, kFilterbank, kNumLayers };
  static std::vector<const char*> layerNames() {
    return {"cymbals", "whales", "field", "atmosphere", "kick", "perc", "bass", "neuro", "filterbank"};
  }
  static constexpr int kBars = 148;
  static constexpr double kTail = 8.83;
  static constexpr float kDefaultBpm = 136.0f;
  static double lengthFor(float bpm) { return double(kBars) * 240.0 / double(bpm) + kTail; }

  RitualTrack(const EngineOptions& o, float bpm) : opt_(o), sr_(o.sr), bpm_(bpm) {
    using dsp::mixSeed;
    rng_ = dsp::Rng(mixSeed(o.seed, 300));
    total_ = uint64_t(lengthFor(bpm) * double(sr_));
    sixteenth_ = 15.0f / bpm;
    rootMidi_ = 28 + rng_.irange(0, 5);  // E1 .. A1
    rootHz_ = dsp::mtof(float(rootMidi_));

    float f0 = rng_.range(220.0f, 300.0f);
    cym_[0].init(sr_, mixSeed(o.seed, 1), f0, 9.0f);
    f0 = rng_.range(330.0f, 450.0f);
    cym_[1].init(sr_, mixSeed(o.seed, 2), f0, 7.0f);
    f0 = rng_.range(160.0f, 210.0f);
    cym_[2].init(sr_, mixSeed(o.seed, 3), f0, 11.0f);
    whales_.init(sr_, mixSeed(o.seed, 4));
    field_.init(sr_, mixSeed(o.seed, 5));
    drone_.init(sr_, mixSeed(o.seed, 6), rootHz_ * 2.0f);
    kick_.init(sr_, mixSeed(o.seed, 7), rootHz_);
    perc_.init(sr_, mixSeed(o.seed, 8));
    bass_.init(sr_);
    neuro_.init(sr_, mixSeed(o.seed, 9));
    gran_.init(sr_, mixSeed(o.seed, 10), 10.0f);
    sweeps_.init(sr_, mixSeed(o.seed, 11));

    bassTape_.init(sr_, mixSeed(o.seed, 12));
    kickTape_.init(sr_, mixSeed(o.seed, 13));
    percTape_.init(sr_, mixSeed(o.seed, 14));
    cymTape_.init(sr_, mixSeed(o.seed, 15));
    masterTape_.init(sr_, mixSeed(o.seed, 16));
    kickTape_.set(1.6f, 0.2f, 0.0f);
    percTape_.set(2.0f, 0.15f, 0.15f);
    cymTape_.set(1.8f, 0.3f, 0.4f);
    masterTape_.set(1.2f, 0.1f, 0.25f);

    for (auto* st : {&stutKick_, &stutPerc_, &stutBass_, &stutNeuro_}) st->init(sr_);
    fbank_.init(sr_, mixSeed(o.seed, 19));
    fbEcho_.init(sr_, 1.0f, 300.0f, 4500.0f);
    fbEcho_.set(3.0f * sixteenth_, 5.0f * sixteenth_, 0.55f);
    bassEcho_.init(sr_, 1.0f, 220.0f, 3200.0f);
    bassEcho_.set(3.0f * sixteenth_, 4.0f * sixteenth_, 0.5f);  // 3/16 left, 1/4 right
    whaleEcho_.init(sr_, 2.5f, 150.0f, 2200.0f);
    whaleEcho_.set(8.0f * sixteenth_, 6.0f * sixteenth_, 0.45f);
    big_.init(sr_, 9.0f, 1.8f, 4500.0f, mixSeed(o.seed, 17));
    room_.init(sr_, 1.2f, 0.45f, 6500.0f, mixSeed(o.seed, 18));
    drumGlue_.init(sr_, -12.0f, 3.0f, 0.005f, 0.15f);
    masterGlue_.init(sr_, -14.0f, 2.0f, 0.02f, 0.4f);
    dcL_.setCutoff(20.0f, sr_);
    dcR_.setCutoff(20.0f, sr_);

    using ritual::euclid;
    frame_ = euclid(5, 12, rng_.irange(0, 11));
    log_ = euclid(3, 7, rng_.irange(0, 6));
    metal_ = euclid(2, 5, rng_.irange(0, 4));
    rattle_ = euclid(4, 9, rng_.irange(0, 8));
    deep_ = {1, 0, 0, 1, 0, 0, 1, 0};  // tresillo: 3-3-2
    bassAccent_ = euclid(2, 5, 0);
    neuro_.setGate(euclid(7, 12, 0));
    bassPat_ = {0, 0, 12, 0, 0, 7, 0};
    static const int kColour[6] = {1, 3, -2, 13, 10, 15};
    for (int j = 0; j < 2; ++j) {
      int p = rng_.irange(1, 6);
      bassPat_[size_t(p)] = kColour[rng_.irange(0, 5)];
    }
    bassPeak_ = bassPat_;
    int p = rng_.irange(1, 6);
    bassPeak_[size_t(p)] = 12;
    p = rng_.irange(1, 6);
    bassPeak_[size_t(p)] = 15;

    if (opt_.log) {
      std::fprintf(stderr, "\n  seed %llu  ·  ritual  ·  %s Phrygian (kick & sub on %.0f Hz)  ·  %.0f BPM  ·  %s\n",
                   (unsigned long long)o.seed, harmony::noteName(rootMidi_ % 12), double(rootHz_), double(bpm),
                   clock(lengthFor(bpm)).c_str());
      std::fprintf(stderr, "  bass cycle: %s\n", intervals(bassPat_).c_str());
      std::fprintf(stderr, "  frame drums %s  ·  log %s  ·  anvil %s  ·  bones %s\n\n", pattern(frame_).c_str(),
                   pattern(log_).c_str(), pattern(metal_).c_str(), pattern(rattle_).c_str());
    }
  }

  bool done() const { return pos_ >= total_; }
  uint64_t position() const { return pos_; }
  uint64_t totalFrames() const { return total_; }
  uint64_t invalidSamples() const { return invalid_; }

  void render(float* outL, float* outR, int frames) {
    using ritual::Lane;
    int off = 0;
    while (off < frames) {
      int n = std::min(32, frames - off);
      for (auto& l : lane_) l.clear(n);
      tick(n);
      float dt = float(n) / sr_;
      for (auto* t : {&bassTape_, &kickTape_, &percTape_, &cymTape_, &masterTape_}) t->block(dt);
      fbank_.block(dt);

      for (auto& c : cym_) c.render(lane_[kCymbals], 1.0f);
      whales_.render(lane_[kWhales], whaleGain_);
      field_.render(lane_[kField]);
      drone_.render(lane_[kAtmosphere], atmoGain_);
      gbusA_.clear(n);
      gbusB_.clear(n);
      gran_.render(gbusA_, gbusB_);
      sweeps_.render(gbusB_);
      for (int i = 0; i < n; ++i)
        lane_[kAtmosphere].add(i, gbusA_.dryL[i] + gbusB_.dryL[i], gbusA_.dryR[i] + gbusB_.dryR[i], 1.0f, 0.0f);
      for (int i = 0; i < n; ++i) {
        lane_[kAtmosphere].bl[i] += gbusA_.revL[i] + gbusB_.revL[i];
        lane_[kAtmosphere].br[i] += gbusA_.revR[i] + gbusB_.revR[i];
      }
      kick_.render(lane_[kKick], duck_);
      perc_.render(lane_[kPerc]);
      bass_.render(sub_, midL_, midR_, n);
      neuro_.render(nL_, nR_, n, blockStep_ / 4.0, double(bpm_) / 60.0 / double(sr_), neuroAmt_);

      // mix levels, applied before tape and bus compression so those react
      // to sensible levels (kick is the anchor)
      for (int id : {kCymbals, kWhales, kField, kAtmosphere, kKick, kPerc}) {
        Lane& L = lane_[size_t(id)];
        float g = kLevel[id];
        for (int i = 0; i < n; ++i) {
          L.dl[i] *= g;
          L.dr[i] *= g;
          L.bl[i] *= g;
          L.br[i] *= g;
          L.rl[i] *= g;
          L.rr[i] *= g;
        }
      }
      for (int i = 0; i < n; ++i) {
        midL_[i] *= kLevel[kBass];
        midR_[i] *= kLevel[kBass];
        sub_[i] *= kLevel[kBass];
        nL_[i] *= kLevel[kNeuro];
        nR_[i] *= kLevel[kNeuro];
      }
      for (int i = 0; i < n; ++i) {
        Lane& cy = lane_[kCymbals];
        Lane& wh = lane_[kWhales];
        gran_.record(finite(0.5f * (cy.dl[i] + cy.dr[i]) + 0.5f * (wh.dl[i] + wh.dr[i])));
        cymTape_.process(cy.dl[i], cy.dr[i]);

        float wl, wr;
        whaleEcho_.process(finite(wh.dl[i]) * 0.5f, finite(wh.dr[i]) * 0.5f, wl, wr);
        wh.add(i, wl, wr, 0.5f, 0.6f);

        // bass: tape drive on the mids, echo on the driven mids, sub clean
        float ml = midL_[i], mr = midR_[i];
        bassTape_.process(ml, mr);
        float el, er;
        bassEcho_.process(finite(ml) * 0.55f, finite(mr) * 0.55f, el, er);
        float duck = 1.0f - 0.8f * duck_[i];
        lane_[kBass].add(i, (sub_[i] + ml) * duck, (sub_[i] + mr) * duck, 1.0f, 0.0f);
        lane_[kBass].add(i, el * duck, er * duck, 0.7f, 0.25f);

        float nd = 1.0f - 0.6f * duck_[i];
        lane_[kNeuro].add(i, nL_[i] * nd, nR_[i] * nd, 1.0f, 0.15f);
        float ad = 1.0f - 0.25f * duck_[i];
        lane_[kAtmosphere].dl[i] *= ad;
        lane_[kAtmosphere].dr[i] *= ad;

        // drum bus: one compressor across kick + percussion, then tape
        Lane& k = lane_[kKick];
        Lane& pc = lane_[kPerc];
        float dg = drumGlue_.gain(finite(k.dl[i] + pc.dl[i]), finite(k.dr[i] + pc.dr[i]));
        for (Lane* L : {&k, &pc}) {
          L->dl[i] *= dg;
          L->dr[i] *= dg;
          L->rl[i] *= dg;
          L->rr[i] *= dg;
        }
        kickTape_.process(k.dl[i], k.dr[i]);
        percTape_.process(pc.dl[i], pc.dr[i]);

        // IDM stutter, same trigger on every rhythm lane
        if (pendingGlitch_.on && pendingGlitch_.wait-- <= 0) {
          pendingGlitch_.on = false;
          for (auto* st : {&stutKick_, &stutPerc_, &stutBass_, &stutNeuro_})
            st->trigger(pendingGlitch_.slice, pendingGlitch_.dur, pendingGlitch_.rate, pendingGlitch_.crush);
        }
        stutKick_.process(k.dl[i], k.dr[i]);
        stutPerc_.process(pc.dl[i], pc.dr[i]);
        stutBass_.process(lane_[kBass].dl[i], lane_[kBass].dr[i]);
        stutNeuro_.process(lane_[kNeuro].dl[i], lane_[kNeuro].dr[i]);

        // filterbank: organic + synthetic sources sent in, wet echo out
        {
          float fl = 0.0f, fr = 0.0f;
          static const int kSrc[6] = {kPerc, kField, kCymbals, kBass, kNeuro, kWhales};
          for (int j = 0; j < 6; ++j) {
            fl += finite(lane_[size_t(kSrc[j])].dl[i]) * fbSend_[j];
            fr += finite(lane_[size_t(kSrc[j])].dr[i]) * fbSend_[j];
          }
          fbank_.process(fl, fr, (blockStep_ + double(i) * stepsPerSample()) / 4.0);
          float xl, xr;
          fbEcho_.process(fl * 0.6f, fr * 0.6f, xl, xr);
          Lane& F = lane_[kFilterbank];
          F.add(i, fl, fr, 0.7f * kLevel[kFilterbank], 0.6f * kLevel[kFilterbank]);
          F.add(i, xl, xr, 0.6f * kLevel[kFilterbank], 0.5f * kLevel[kFilterbank]);
        }

        // mix
        float mL = 0, mR = 0, bL = 0, bR = 0, rL = 0, rR = 0;
        for (int id = 0; id < kNumLayers; ++id) {
          if ((opt_.muted >> id) & 1u) continue;
          const Lane& L = lane_[size_t(id)];
          mL += finite(L.dl[i]);
          mR += finite(L.dr[i]);
          bL += finite(L.bl[i]);
          bR += finite(L.br[i]);
          rL += finite(L.rl[i]);
          rR += finite(L.rr[i]);
        }
        float ol, orr, ql, qr;
        big_.process(bL, bR, ol, orr);
        room_.process(rL, rR, ql, qr);
        float l = dcL_.hp(mL + ol * 0.9f + ql * 0.8f);
        float r = dcR_.hp(mR + orr * 0.9f + qr * 0.8f);
        float g = masterGlue_.gain(l, r) * kMaster;
        l *= g;
        r *= g;
        masterTape_.process(l, r);
        double t = double(pos_ + uint64_t(i)) / double(sr_);
        float fade = dsp::smoothstep(0.0f, 0.3f, float(t)) *
                     dsp::smoothstep(0.0f, 6.0f, float(double(total_) / double(sr_) - t));
        outL[off + i] = dsp::softClip(l) * fade;
        outR[off + i] = dsp::softClip(r) * fade;
      }
      pos_ += uint64_t(n);
      off += n;
    }
  }

 private:
  static constexpr float kMaster = 1.4f;
  //                                  cym   whales field atmo  kick  perc   bass   neuro  filterbank
  static constexpr float kLevel[9] = {0.2f, 0.8f, 1.0f, 1.3f, 1.4f, 0.18f, 0.32f, 0.42f, 0.4f};
  struct Pt {
    float bar, v;
  };
  static float curve(const std::vector<Pt>& c, float bar) {
    if (bar <= c.front().bar) return c.front().v;
    for (size_t i = 0; i + 1 < c.size(); ++i)
      if (bar < c[i + 1].bar) return dsp::lerp(c[i].v, c[i + 1].v, (bar - c[i].bar) / (c[i + 1].bar - c[i].bar));
    return c.back().v;
  }

  float finite(float x) {
    if (std::isfinite(x)) return x;
    ++invalid_;
    return 0.0f;
  }
  static std::string clock(double t) {
    char buf[16];
    int s = int(t);
    std::snprintf(buf, sizeof buf, "%d:%02d", s / 60, s % 60);
    return buf;
  }
  static std::string pattern(const std::vector<int>& p) {
    std::string s;
    for (int x : p) s += x ? "x" : ".";
    return s;
  }
  static std::string intervals(const std::vector<int>& p) {
    std::string s;
    for (int x : p) s += (s.empty() ? "" : " ") + std::to_string(x);
    return s + "  (semitones above the root, 7-note cycle over 3-note beats)";
  }
  void log(const std::string& msg) {
    if (opt_.log)
      std::fprintf(stderr, "  [%s]  %s\n", clock(double(pos_) / double(sr_)).c_str(), msg.c_str());
  }

  // ---- per-block control ----------------------------------------------------
  void tick(int n) {
    float dt = float(n) / sr_;
    float bar = float(step16_ / 16.0);

    static const std::vector<Pt> kCym = {{0, 0.3f},   {6, 0.5f},   {16, 0.6f},  {20, 0.35f}, {34, 0.3f},
                                         {42, 0.65f}, {44, 0.15f}, {79, 0.15f}, {80, 0.25f}, {92, 0.3f},
                                         {98, 0.6f},
                                         {100, 0.15f}, {131, 0.15f}, {134, 0.45f}, {148, 0.4f}};
    static const std::vector<Pt> kTorture = {{0, 0.15f},  {14, 0.7f},  {20, 0.45f}, {36, 0.5f},
                                             {42, 1.0f},  {44, 0.35f}, {80, 0.55f}, {98, 1.0f},
                                             {100, 0.4f}, {132, 0.7f}, {148, 0.3f}};
    static const std::vector<Pt> kWhaleGain = {{0, 0},   {5, 0},       {9, 1},      {34, 0.8f}, {44, 0.55f},
                                               {80, 1},  {100, 0.55f}, {132, 0.8f}, {148, 1}};
    static const std::vector<Pt> kWhaleRate = {{0, 0},      {6, 0.14f},   {34, 0.1f},   {44, 0.035f},
                                               {80, 0.18f}, {100, 0.04f}, {132, 0.14f}, {148, 0.1f}};
    static const std::vector<Pt> kAtmo = {{0, 0.3f},   {14, 0.6f},   {44, 0.5f}, {80, 0.55f},
                                          {100, 0.6f}, {132, 0.7f}, {148, 0.5f}};
    static const std::vector<Pt> kWind = {{0, 0.8f},    {20, 0.6f},  {44, 0.25f}, {80, 0.7f},
                                          {100, 0.25f}, {132, 0.6f}, {148, 1.0f}};
    static const std::vector<Pt> kCreek = {{0, 0.7f},  {20, 0.5f},  {36, 0.2f},  {44, 0.1f},
                                           {80, 0.5f}, {100, 0.1f}, {132, 0.5f}, {148, 0.8f}};
    static const std::vector<Pt> kInsects = {{0, 0.8f}, {20, 0.5f}, {36, 0}, {132, 0}, {138, 0.7f}, {148, 1}};
    static const std::vector<Pt> kDrips = {{0, 0.6f}, {36, 0.3f}, {80, 0.9f}, {100, 0.2f}, {132, 0.6f}, {148, 0.6f}};
    static const std::vector<Pt> kRumble = {{0, 0}, {78, 0}, {80, 0.6f}, {90, 0.2f}, {132, 0.15f}, {148, 0.5f}};
    static const std::vector<Pt> kBassOpen = {{28, 0.05f}, {36, 0.15f}, {44, 0.55f},  {62, 0.65f},
                                              {78, 0.8f},  {100, 0.7f}, {116, 0.9f},  {132, 0.85f}};
    static const std::vector<Pt> kBassDrive = {{28, 1.5f}, {44, 2.5f}, {80, 3.0f}, {116, 4.0f}};

    // cymbals: three bows, the third one the most tortured
    float base = curve(kCym, bar), torture = curve(kTorture, bar);
    static const float kStart[3] = {0.0f, 4.0f, 10.0f};
    for (int c = 0; c < 3; ++c) {
      float act = dsp::smoothstep(kStart[c], kStart[c] + 3.0f, bar);
      float sw = 0.0f;
      if (swellT_[c] >= 0.0f) {
        swellT_[c] += dt;
        float sp = swellT_[c] / swellDur_[c];
        if (sp >= 1.0f)
          swellT_[c] = -1.0f;
        else
          sw = ritual::sinPos(dsp::kPi * sp);
      }
      cym_[size_t(c)].setBow(base * act + 0.5f * sw, std::min(1.0f, torture * (c == 2 ? 1.25f : 1.0f)));
    }

    whaleGain_ = curve(kWhaleGain, bar);
    atmoGain_ = curve(kAtmo, bar);
    field_.setLevels(curve(kWind, bar), curve(kCreek, bar), curve(kInsects, bar), curve(kDrips, bar),
                     curve(kRumble, bar));
    bass_.setFilter(curve(kBassOpen, bar));
    bassTape_.set(curve(kBassDrive, bar), 0.25f, 0.2f);

    if (whaleGain_ > 0.05f && rng_.chance(curve(kWhaleRate, bar) * dt)) {
      static const char* kName[4] = {"moan", "cry", "groan", "whoop"};
      int kind = pickKind();
      whales_.call(ritual::Whales::Kind(kind), rootHz_);
      if (opt_.log && whaleLogged_++ < 3) log(std::string("whale ") + kName[kind]);
    }

    textures::Granulator::Params gp;
    gp.density = 4.0f + 14.0f * atmoGain_;
    gp.durMs = 260.0f;
    gp.backSec = 1.0f;
    gp.spreadSec = 6.0f;
    gp.jitterSt = 0.03f;
    gp.reverse = 0.4f;
    gp.chirp = 0.05f;
    gp.amp = 0.6f * atmoGain_;
    gp.ratios = {0.5f, 1.0f, 0.75f};
    gran_.setParams(gp);

    // filterbank: what goes in, and how hard it's pushed
    static const std::vector<Pt> kFbPerc = {{20, 0}, {24, 0.6f}, {36, 0.4f}, {44, 0.3f}, {80, 0.15f},
                                            {100, 0.45f}, {132, 0.35f}, {144, 0}};
    static const std::vector<Pt> kFbField = {{0, 0.3f}, {20, 0.45f}, {44, 0.15f}, {80, 0.5f},
                                             {100, 0.15f}, {132, 0.5f}, {148, 0.5f}};
    static const std::vector<Pt> kFbCym = {{0, 0.15f}, {20, 0.3f}, {44, 0.15f}, {80, 0.3f},
                                           {100, 0.2f}, {132, 0.35f}, {148, 0.25f}};
    static const std::vector<Pt> kFbBass = {{28, 0}, {44, 0.25f}, {79, 0.25f}, {80, 0}, {100, 0.45f},
                                            {132, 0.45f}, {136, 0}};
    static const std::vector<Pt> kFbNeuro = {{84, 0.3f}, {100, 0.25f}, {108, 0.6f}, {132, 0}};
    static const std::vector<Pt> kFbWhale = {{0, 0}, {80, 0.25f}, {100, 0.15f}, {132, 0.3f}, {148, 0.3f}};
    static const std::vector<Pt> kFbReso = {{0, 0.55f}, {44, 0.7f}, {80, 0.85f}, {100, 0.8f},
                                            {132, 0.9f}, {148, 0.7f}};
    static const std::vector<Pt> kFbBase = {{0, 300}, {44, 250}, {80, 180}, {100, 300}, {132, 200}};
    static const std::vector<Pt> kFbDrive = {{0, 3}, {44, 4}, {100, 6}, {148, 4}};
    static const std::vector<Pt> kFbLfo = {{0, 0.3f}, {80, 0.6f}, {100, 0.5f}};
    fbSend_[0] = curve(kFbPerc, bar);
    fbSend_[1] = curve(kFbField, bar);
    fbSend_[2] = curve(kFbCym, bar);
    fbSend_[3] = curve(kFbBass, bar);
    fbSend_[4] = curve(kFbNeuro, bar);
    fbSend_[5] = curve(kFbWhale, bar);
    fbank_.set(curve(kFbBase, bar), curve(kFbReso, bar), 0.7f, curve(kFbLfo, bar), curve(kFbDrive, bar));

    // neurobass level
    bool neuroOn = (bar >= 84.0f && bar < 99.75f) || (bar >= 108.0f && bar < 132.0f);
    float nt = neuroOn ? (bar < 100.0f ? 0.45f : 0.9f) : 0.0f;
    neuroAmt_ += (nt - neuroAmt_) * dsp::onePoleCoef(0.05f, sr_ / float(n));

    // 16th-note sequencer, sample accurate
    double perSample = double(bpm_) / 60.0 * 4.0 / double(sr_);
    double from = step16_, to = step16_ + double(n) * perSample;
    for (double k = std::ceil(from); k < to; k += 1.0) onStep((long long)k, int((k - from) / perSample));
    blockStep_ = from;
    step16_ = to;
  }

  double stepsPerSample() const { return double(bpm_) / 60.0 * 4.0 / double(sr_); }

  int pickKind() {
    float r = rng_.uni() * 7.5f;
    if (r < 3.0f) return 0;
    if (r < 5.0f) return 1;
    if (r < 6.5f) return 2;
    return 3;
  }

  // ---- the arrangement, step by step ---------------------------------------
  void onStep(long long k, int wait) {
    int bar = int(k / 16), s = int(k % 16);
    if (bar >= kBars) return;
    if (s == 0) sectionEvents(bar, wait);
    bool groove = (bar >= 44 && bar < 80) || (bar >= 100 && bar < 132);
    fbank_.step(groove ? 0.6f : 0.25f);
    bool hush = (bar == 43 && s >= 8) || (bar == 99 && s >= 12);  // silence before the drops
    using K = ritual::Perc::Kind;

    // cymbal swells on 8-bar phrases inside the grooves
    if (groove && s == 0 && bar % 8 == 0 && rng_.chance(0.6f)) {
      int c = rng_.irange(0, 2);
      swellT_[c] = 0.0f;
      swellDur_[c] = 4.0f * 16.0f * sixteenth_;
    }

    // IDM glitches: a stutter on the last beat of most 8-bar phrases, and
    // rare micro-stutters inside them
    if (groove && bar % 8 == 7 && s == 12 && rng_.chance(0.65f)) {
      glitch(wait, false);
    } else if (groove && (s == 6 || s == 14) && rng_.chance(bar >= 100 ? 0.07f : 0.04f)) {
      glitch(wait, true);
    }

    // kick: half-time while it arrives, then four on the floor
    bool kick = false;
    float kv = 1.0f;
    if (!hush) {
      if (bar >= 36 && bar < 40)
        kick = s == 0 || s == 8;
      else if ((bar >= 40 && bar < 44) || groove || (bar >= 132 && bar < 140))
        kick = s % 4 == 0;
      if (groove && s == 14 && rng_.chance(0.25f)) {
        kick = true;
        kv = 0.55f;
      }
    }
    if (kick) kick_.trigger(kv, wait);

    // bass: rolls in the three 16ths between kicks; 7-note pitch cycle
    bool bassOn = !hush && ((bar >= 28 && bar < 80) || (bar >= 100 && bar < 136));
    if (bassOn && s % 4 != 0 && (bar >= 36 || s % 4 == 2)) {
      int idx = bassCount_++;
      const auto& pat = (bar >= 116 && bar < 132) ? bassPeak_ : bassPat_;
      float vel = bassAccent_[size_t(idx % 5)] ? 1.0f : 0.72f;
      bass_.note(dsp::mtof(float(rootMidi_ + 12 + pat[size_t(idx % 7)])), vel, sixteenth_ * 0.7f, wait);
    }

    if (hush) return;
    float breakDamp = (bar >= 80 && bar < 100) ? 0.6f : 1.0f;

    // frame drums: 5 hits in 12 steps, low and high skin
    if (((bar >= 20 && bar < 80) || (bar >= 84 && bar < 144)) && frame_[size_t(k % 12)]) {
      bool low = (k % 12) % 2 == 0;
      float v = (low ? 0.85f : 0.6f) * breakDamp * rng_.range(0.85f, 1.0f);
      perc_.hit(K::Frame, rootHz_ * (low ? 2.0f : 3.0f), v, low ? -0.35f : 0.3f, wait);
    }
    // deep drum: tresillo; in the break a sparse pulse, then a roll that builds
    bool deep = false;
    float dv = 0.9f;
    if ((bar >= 36 && bar < 80) || (bar >= 100 && bar < 144)) {
      deep = deep_[size_t(k % 8)] != 0;
    } else if (bar >= 80 && bar < 96) {
      deep = s == 0 && bar % 2 == 0;
    } else if (bar >= 96 && bar < 100) {
      deep = bar < 98 ? s % 4 == 0 : (bar < 99 ? s % 2 == 0 : true);
      dv = 0.4f + 0.6f * float(k - 96 * 16) / 64.0f;
    }
    if (deep) perc_.hit(K::Deep, rootHz_ * 1.5f, dv, -0.1f, wait);
    // industrial clank on the backbeat
    bool clank = false;
    if (bar >= 44 && bar < 80) clank = s == 12 && bar % 2 == 1;
    if (bar >= 100 && bar < 132) clank = s == 12 || (bar >= 116 && s == 4);
    if (clank || (bar >= 100 && bar < 132 && s == 7 && rng_.chance(0.15f))) {
      float v = clank ? 0.8f : 0.4f;
      perc_.hit(K::Clank, rng_.range(280.0f, 420.0f), v, 0.1f, wait);
    }
    // shaker: every 16th, accent every 3 steps (3 against 4)
    if ((bar >= 24 && bar < 80) || (bar >= 100 && bar < 140)) {
      if (!rng_.chance(0.12f)) {
        float v = (k % 3 == 0 ? 0.8f : 0.35f) * rng_.range(0.8f, 1.0f);
        float p = 0.35f + 0.35f * std::sin(float(k) * 0.05f);
        perc_.hit(K::Shaker, 0.0f, v, p, wait);
      }
    }
    // log drum: 3 hits in 7 steps
    if (((bar >= 28 && bar < 80) || (bar >= 100 && bar < 136)) && log_[size_t(k % 7)]) {
      static const int kNotes[4] = {0, 3, 7, 10};
      int note = kNotes[logCount_++ % 4];
      perc_.hit(K::Log, dsp::mtof(float(rootMidi_ + 36 + note)), 0.7f, 0.45f, wait);
    }
    // anvil: 2 in 5
    if (((bar >= 56 && bar < 80) || (bar >= 108 && bar < 132)) && metal_[size_t(k % 5)]) {
      float hz = rootHz_ * 32.0f * (k % 2 ? 1.5f : 1.0f);
      perc_.hit(K::Metal, hz, 0.5f, -0.55f, wait);
    }
    // bones: 4 in 9
    if (((bar >= 64 && bar < 80) || (bar >= 112 && bar < 132)) && rattle_[size_t(k % 9)]) {
      float hz = rng_.range(2500.0f, 3800.0f);
      float p = rng_.range(-0.8f, 0.8f);
      perc_.hit(K::Rattle, hz, 0.5f, p, wait);
    }
    // neurobass: a new note every 2 bars
    bool neuroOn = (bar >= 84 && bar < 100) || (bar >= 108 && bar < 132);
    if (neuroOn && s == 0 && bar % 2 == 0) {
      static const int kN[4] = {0, 1, 0, -2};
      neuro_.note(dsp::mtof(float(rootMidi_ + 12 + kN[(bar / 2) % 4])));
    }
  }

  void glitch(int wait, bool micro) {
    static const float kRate[5] = {1.0f, 1.0f, 0.5f, 2.0f, 0.7f};
    static const int kCrush[4] = {0, 0, 4, 8};
    int slice = int(sixteenth_ * sr_ * (rng_.chance(0.5f) ? 1.0f : 0.5f));
    int dur = int(sixteenth_ * sr_ * (micro ? 2.0f : 4.0f));
    float rate = kRate[rng_.irange(0, 4)];
    int crush = kCrush[rng_.irange(0, 3)];
    pendingGlitch_ = {true, wait, slice, dur, rate, crush};
    if (!micro) log(std::string("glitch") + (rate < 1.0f ? " (pitch falls)" : rate > 1.0f ? " (pitch rises)" : "") +
                    (crush ? " (bitcrushed)" : ""));
  }

  void sectionEvents(int bar, int wait) {
    switch (bar) {
      case 0:
        log("AWAKENING     bowed tortured cymbals, field recordings");
        break;
      case 20:
        log("GATHERING     frame drums, shaker, log drum; muffled bass at bar 28");
        break;
      case 36:
        log("THE CALL      kick enters half-time, bass filter opens");
        break;
      case 42:
      case 98:
        sweeps_.trigger(180.0f, 7000.0f, 2.0f * 16.0f * sixteenth_, 0.5f, 2, acoustic::Spatial::Approach);
        log("riser");
        break;
      case 44:
      case 100:
        kick_.boom(1.0f, wait);
        for (auto& c : cym_) c.strike(0.5f);
        log(bar == 44 ? "RITUAL I      kick + rolling bass with echo, percussion, industrial clank"
                      : "RITUAL II     the peak: neurobass, anvil, bones, more glitch");
        break;
      case 80:
        kick_.boom(0.8f, wait);
        for (auto& c : cym_) c.strike(0.35f);
        log("DESCENT       beat out: whales, tortured cymbals, thunder; neurobass at bar 84");
        break;
      case 96:
        log("drum roll");
        break;
      case 132:
        log("DISSOLUTION   neuro out; bass leaves at 136, kick at 140");
        break;
      case 146:
        log("tail");
        break;
      default:
        break;
    }
  }

  EngineOptions opt_;
  float sr_, bpm_, sixteenth_ = 0.12f, rootHz_ = 50.0f;
  int rootMidi_ = 31;
  uint64_t pos_ = 0, total_ = 0, invalid_ = 0;
  double step16_ = 0.0;
  dsp::Rng rng_;

  std::array<ritual::Lane, kNumLayers> lane_;
  textures::EBus gbusA_, gbusB_;
  float duck_[64] = {}, sub_[64] = {}, midL_[64] = {}, midR_[64] = {}, nL_[64] = {}, nR_[64] = {};

  std::array<ritual::BowedCymbal, 3> cym_;
  ritual::Whales whales_;
  ritual::Field field_;
  ritual::Drone drone_;
  ritual::Kick kick_;
  ritual::Perc perc_;
  ritual::Bass bass_;
  ritual::Neuro neuro_;
  textures::Granulator gran_;
  textures::Sweeps sweeps_;
  ritual::Tape bassTape_, kickTape_, percTape_, cymTape_, masterTape_;
  ritual::Echo bassEcho_, whaleEcho_;
  ritual::Stutter stutKick_, stutPerc_, stutBass_, stutNeuro_;
  ritual::Filterbank fbank_;
  ritual::Echo fbEcho_;
  float fbSend_[6] = {};
  double blockStep_ = 0.0;
  struct PendingGlitch {
    bool on = false;
    int wait = 0, slice = 0, dur = 0;
    float rate = 1.0f;
    int crush = 0;
  } pendingGlitch_;
  dsp::Reverb big_, room_;
  dsp::Glue drumGlue_, masterGlue_;
  dsp::OnePole dcL_, dcR_;

  std::vector<int> frame_, log_, metal_, rattle_, deep_, bassAccent_, bassPat_, bassPeak_;
  int bassCount_ = 0, logCount_ = 0, whaleLogged_ = 0;
  float whaleGain_ = 0, atmoGain_ = 0, neuroAmt_ = 0;
  float swellT_[3] = {-1.0f, -1.0f, -1.0f}, swellDur_[3] = {1, 1, 1};
};
