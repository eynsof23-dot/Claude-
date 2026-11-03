#pragma once
// harmony.hpp — chords, and why they sound dark.
//
// The progression is a weighted random walk (a Markov chain) over chords
// borrowed from the minor key. Cinematic darkness mostly comes from:
//   * i  -> bVI   (e.g. Cm -> Abmaj7): the classic "epic and sad" move.
//   * bII         (Cm -> Db): the Phrygian half-step, pure dread.
//   * chromatic mediants (Cm -> Abm, Cm -> E): chords a third away that
//     share one note but feel like the floor shifted. Used more as the
//     intensity rises.
// Underneath, the drone holds the key's root no matter what, so every chord
// change grinds against a pedal tone. That tension is the whole point.

#include <string>
#include <vector>

#include "dsp.hpp"

namespace harmony {

struct ChordDef {
  int offset;  // semitones above the key root
  bool minor;
  const char* roman;
};

inline const std::vector<ChordDef>& table() {
  static const std::vector<ChordDef> t = {
      {0, true, "i"},   {8, false, "bVI"}, {3, false, "bIII"}, {10, false, "bVII"}, {5, true, "iv"},
      {7, true, "v"},   {1, false, "bII"}, {8, true, "bvi"},   {4, false, "III"},
  };
  return t;
}

inline const char* noteName(int pc) {
  static const char* n[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
  return n[((pc % 12) + 12) % 12];
}

inline const char* modeName(int mode) {
  static const char* n[3] = {"Aeolian", "Phrygian", "Harmonic minor"};
  return n[mode];
}

inline const int* modeScale(int mode) {
  static const int s[3][7] = {
      {0, 2, 3, 5, 7, 8, 10},  // Aeolian (natural minor)
      {0, 1, 3, 5, 7, 8, 10},  // Phrygian: the b2 is the darkest note there is
      {0, 2, 3, 5, 7, 8, 11},  // Harmonic minor: the raised 7th sounds ancient
  };
  return s[mode];
}

struct Chord {
  int rootPc = 0;
  bool minor = true;
  std::vector<int> pcs;    // pitch classes in the chord
  int bass = 36;           // pad bass note (MIDI)
  std::vector<int> upper;  // pad upper voicing (MIDI, ascending)
  std::string name, roman;
};

class Harmony {
 public:
  void init(uint64_t seed, int keyRoot, int mode) {
    rng_ = dsp::Rng(seed);
    setKey(keyRoot, mode);
  }
  void setKey(int keyRoot, int mode) {
    keyRoot_ = keyRoot;
    mode_ = mode;
    cur_ = -1;  // next chord is the new tonic
  }
  int keyRoot() const { return keyRoot_; }
  int mode() const { return mode_; }
  const Chord& chord() const { return chord_; }

  const Chord& next(float intensity) {
    const auto& t = table();
    int pick = 0;
    if (cur_ >= 0) {
      float w[9];
      float dark = 0.5f + intensity;  // more chromatic colour when it's tense
      w[0] = cur_ >= 6 ? 4.0f : 2.5f;  // after a strange chord, come home
      w[1] = 3.0f;
      w[2] = 1.5f;
      w[3] = 1.5f;
      w[4] = 2.0f;
      w[5] = 0.8f;
      w[6] = (mode_ == 1 ? 1.8f : 0.6f) * dark;
      w[7] = 0.6f * dark;
      w[8] = 0.5f * dark;
      w[cur_] = 0.0f;
      float total = 0.0f;
      for (float x : w) total += x;
      float r = rng_.uni() * total;
      for (pick = 0; pick < 8; ++pick) {
        r -= w[pick];
        if (r < 0.0f) break;
      }
    }
    cur_ = pick;
    build(t[size_t(pick)]);
    return chord_;
  }

 private:
  void build(const ChordDef& d) {
    Chord c;
    c.rootPc = (keyRoot_ + d.offset) % 12;
    c.minor = d.minor;
    c.roman = d.roman;
    int third = d.minor ? 3 : 4;
    std::vector<int> iv = {0, third, 7};
    std::string ext;
    float r = rng_.uni();
    if (r < 0.40f) {
      // plain triad
    } else if (r < 0.75f) {
      iv.push_back(2);  // add9: open, aching
      ext = "add9";
    } else if (d.minor) {
      iv.push_back(10);
      ext = "7";
    } else if (d.offset == 10) {
      iv.push_back(10);
      ext = "7";
    } else {
      iv.push_back(11);  // maj7 on bVI / bIII: bittersweet
      ext = "maj7";
    }
    c.name = std::string(noteName(c.rootPc)) + (d.minor ? "m" : "") + ext;
    for (int i : iv) c.pcs.push_back((c.rootPc + i) % 12);

    c.bass = 36 + c.rootPc;  // C2..B2
    // Voice leading: put every chord tone in the octave closest to where the
    // last chord sat, so the pad moves by small steps instead of jumping.
    float sum = 0.0f;
    for (int pc : c.pcs) {
      int best = 0;
      float bestDist = 1e9f;
      for (int n = 52; n <= 76; ++n) {
        if (n % 12 != pc) continue;
        float dist = std::fabs(float(n) - center_);
        if (dist < bestDist) {
          bestDist = dist;
          best = n;
        }
      }
      c.upper.push_back(best);
      sum += float(best);
    }
    std::sort(c.upper.begin(), c.upper.end());
    center_ = dsp::clampf(0.5f * center_ + 0.5f * sum / float(c.upper.size()) + rng_.range(-1.5f, 1.5f),
                          58.0f, 67.0f);
    chord_ = c;
  }

  dsp::Rng rng_;
  int keyRoot_ = 38, mode_ = 0, cur_ = -1;
  float center_ = 62.0f;
  Chord chord_;
};

}  // namespace harmony
