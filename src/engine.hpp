#pragma once
// engine.hpp — options shared by both styles.

#include <cstdint>

struct EngineOptions {
  uint64_t seed = 1;
  float sr = 48000.0f;
  double duration = 180.0;  // seconds; <= 0 means endless
  unsigned muted = 0;       // bitmask of the style's layer ids
  bool log = true;
};
