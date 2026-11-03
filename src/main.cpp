// main.cpp — command line, WAV writer and live streaming.
//
//   soundscape                          electroacoustic, 3 minutes -> soundscape.wav
//   soundscape --style cinematic        the tonal, trailer-like style
//   soundscape --style ritual           the composed 4:30 ritual / industrial track
//   soundscape -s 42 -d 300             a specific 5-minute piece
//   soundscape --raw | ffplay ...       endless, live (see README)
//   soundscape --solo grains,glitter    hear single layers to learn how they sound

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "electroacoustic.hpp"
#include "ritual.hpp"
#include "soundscape.hpp"

#if defined(__SSE__) || defined(_M_X64)
#include <xmmintrin.h>
#endif
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {

// 24-bit PCM WAV: plays everywhere and imports into any DAW.
class WavWriter {
 public:
  bool open(const std::string& path, int sr) {
    f_ = std::fopen(path.c_str(), "wb");
    if (!f_) return false;
    sr_ = sr;
    header(0);
    return true;
  }
  void write(const float* l, const float* r, int n) {
    buf_.resize(size_t(n) * 6);
    unsigned char* p = buf_.data();
    for (int i = 0; i < n; ++i) {
      for (float x : {l[i], r[i]}) {
        long v = std::lrint(double(dsp::clampf(x, -1.0f, 1.0f)) * 8388607.0);
        *p++ = (unsigned char)(v & 0xFF);
        *p++ = (unsigned char)((v >> 8) & 0xFF);
        *p++ = (unsigned char)((v >> 16) & 0xFF);
      }
    }
    std::fwrite(buf_.data(), 1, buf_.size(), f_);
    frames_ += uint32_t(n);
  }
  void close() {
    if (!f_) return;
    std::fseek(f_, 0, SEEK_SET);
    header(frames_ * 6u);
    std::fclose(f_);
    f_ = nullptr;
  }

 private:
  void u32(uint32_t v) {
    unsigned char b[4] = {(unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16),
                          (unsigned char)(v >> 24)};
    std::fwrite(b, 1, 4, f_);
  }
  void u16(uint16_t v) {
    unsigned char b[2] = {(unsigned char)v, (unsigned char)(v >> 8)};
    std::fwrite(b, 1, 2, f_);
  }
  void header(uint32_t dataBytes) {
    std::fwrite("RIFF", 1, 4, f_);
    u32(36 + dataBytes);
    std::fwrite("WAVEfmt ", 1, 8, f_);
    u32(16);
    u16(1);  // PCM
    u16(2);  // stereo
    u32(uint32_t(sr_));
    u32(uint32_t(sr_) * 6u);
    u16(6);
    u16(24);
    std::fwrite("data", 1, 4, f_);
    u32(dataBytes);
  }
  FILE* f_ = nullptr;
  int sr_ = 48000;
  uint32_t frames_ = 0;
  std::vector<unsigned char> buf_;
};

void printLayers(const char* style, const std::vector<const char*>& names) {
  std::fprintf(stderr, "  %-16s", style);
  for (const char* n : names) std::fprintf(stderr, "%s ", n);
  std::fprintf(stderr, "\n");
}

void usage() {
  std::fprintf(stderr,
               "soundscape — generative dark soundscapes\n\n"
               "usage: soundscape [options]\n"
               "      --style NAME     electroacoustic (default), cinematic or ritual\n"
               "      --bpm N          tempo of the ritual track (default 136; length follows)\n"
               "  -o, --out FILE       WAV file to write (default: soundscape.wav)\n"
               "  -d, --duration SEC   length in seconds (default: 180)\n"
               "  -s, --seed N         same seed = same piece (default: random)\n"
               "      --raw            stream 32-bit float stereo to stdout, endless\n"
               "                       unless --duration is given (pipe into a player)\n"
               "      --rate HZ        sample rate (default: 48000)\n"
               "      --solo LIST      play only these layers (comma separated)\n"
               "      --mute LIST      silence these layers\n"
               "  -q, --quiet          don't print the score\n\n"
               "layers:\n");
  printLayers("electroacoustic", Electroacoustic::layerNames());
  printLayers("cinematic", Soundscape::layerNames());
  printLayers("ritual", RitualTrack::layerNames());
}

bool parseLayers(const std::string& list, const std::vector<const char*>& names, unsigned& mask) {
  size_t start = 0;
  while (start <= list.size()) {
    size_t end = list.find(',', start);
    if (end == std::string::npos) end = list.size();
    std::string name = list.substr(start, end - start);
    bool found = false;
    for (size_t i = 0; i < names.size(); ++i)
      if (name == names[i]) {
        mask |= 1u << i;
        found = true;
      }
    if (!found) {
      std::fprintf(stderr, "unknown layer '%s' for this style. Layers are:", name.c_str());
      for (const char* n : names) std::fprintf(stderr, " %s", n);
      std::fprintf(stderr, "\n");
      return false;
    }
    start = end + 1;
  }
  return true;
}

}  // namespace

template <class EngineT>
int run(EngineT& scape, const EngineOptions& opt, bool raw, const std::string& out) {
  const int kChunk = 1024;
  std::vector<float> l(kChunk), r(kChunk), inter(kChunk * 2);

  if (raw) {
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#else
    std::signal(SIGPIPE, SIG_IGN);  // player closed: just stop
#endif
    while (!scape.done()) {
      int n = kChunk;
      if (scape.totalFrames() > 0)
        n = int(std::min<uint64_t>(uint64_t(kChunk), scape.totalFrames() - scape.position()));
      scape.render(l.data(), r.data(), n);
      for (int i = 0; i < n; ++i) {
        inter[size_t(2 * i)] = l[size_t(i)];
        inter[size_t(2 * i + 1)] = r[size_t(i)];
      }
      if (std::fwrite(inter.data(), sizeof(float), size_t(2 * n), stdout) != size_t(2 * n)) break;
      std::fflush(stdout);
    }
    return 0;
  }

  WavWriter wav;
  if (!wav.open(out, int(opt.sr))) {
    std::fprintf(stderr, "cannot write %s\n", out.c_str());
    return 1;
  }
  auto t0 = std::chrono::steady_clock::now();
  float peak = 0.0f;
  uint64_t bad = 0;
  while (!scape.done()) {
    int n = int(std::min<uint64_t>(uint64_t(kChunk), scape.totalFrames() - scape.position()));
    scape.render(l.data(), r.data(), n);
    for (int i = 0; i < n; ++i) {
      float a = std::fabs(l[size_t(i)]), b = std::fabs(r[size_t(i)]);
      if (!std::isfinite(a) || !std::isfinite(b)) ++bad;
      peak = std::max(peak, std::max(a, b));
    }
    wav.write(l.data(), r.data(), n);
  }
  wav.close();
  double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  std::fprintf(stderr, "\n  wrote %s  (%.0f s of audio in %.1f s, peak %.1f dBFS)\n", out.c_str(), opt.duration,
               secs, 20.0 * std::log10(double(peak) + 1e-12));
  bad += scape.invalidSamples();
  if (bad) {
    std::fprintf(stderr, "  WARNING: %llu invalid (NaN/inf) samples - please report this seed\n",
                 (unsigned long long)bad);
    return 3;
  }
  return 0;
}

int main(int argc, char** argv) {
#if defined(__SSE__) || defined(_M_X64)
  // Flush denormals to zero: decaying reverb tails otherwise become very slow.
  _mm_setcsr(_mm_getcsr() | 0x8040);
#endif

  EngineOptions opt;
  opt.seed = (uint64_t(std::random_device{}()) << 32) ^
             uint64_t(std::chrono::high_resolution_clock::now().time_since_epoch().count());
  opt.seed %= 1000000;  // short seeds are easier to note down
  std::string out = "soundscape.wav", style = "electroacoustic", soloList, muteList;
  bool raw = false, durationGiven = false;
  float bpm = RitualTrack::kDefaultBpm;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto value = [&]() -> std::string {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "missing value for %s\n", a.c_str());
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "-h" || a == "--help") {
      usage();
      return 0;
    } else if (a == "--style") {
      style = value();
    } else if (a == "--bpm") {
      bpm = float(std::atof(value().c_str()));
    } else if (a == "-o" || a == "--out") {
      out = value();
    } else if (a == "-d" || a == "--duration") {
      opt.duration = std::atof(value().c_str());
      durationGiven = true;
    } else if (a == "-s" || a == "--seed") {
      opt.seed = std::strtoull(value().c_str(), nullptr, 10);
    } else if (a == "--raw") {
      raw = true;
    } else if (a == "--rate") {
      opt.sr = float(std::atof(value().c_str()));
    } else if (a == "--solo") {
      soloList = value();
    } else if (a == "--mute") {
      muteList = value();
    } else if (a == "-q" || a == "--quiet") {
      opt.log = false;
    } else {
      std::fprintf(stderr, "unknown option '%s'\n\n", a.c_str());
      usage();
      return 2;
    }
  }
  bool cinematic = style == "cinematic", ritualStyle = style == "ritual";
  if (!cinematic && !ritualStyle && style != "electroacoustic" && style != "electro") {
    std::fprintf(stderr, "unknown style '%s' (use electroacoustic, cinematic or ritual)\n", style.c_str());
    return 2;
  }
  if (ritualStyle) {
    if (bpm < 60.0f || bpm > 200.0f) {
      std::fprintf(stderr, "--bpm must be between 60 and 200\n");
      return 2;
    }
    if (durationGiven) std::fprintf(stderr, "note: the ritual track is arranged; its length follows --bpm\n");
    opt.duration = RitualTrack::lengthFor(bpm);
    durationGiven = true;
  }
  if (opt.sr < 8000.0f || opt.sr > 192000.0f) {
    std::fprintf(stderr, "sample rate must be between 8000 and 192000\n");
    return 2;
  }
  if (raw && !durationGiven) opt.duration = 0.0;  // endless
  if (!raw && opt.duration <= 0.0) {
    std::fprintf(stderr, "duration must be positive\n");
    return 2;
  }
  std::vector<const char*> names = cinematic     ? Soundscape::layerNames()
                                   : ritualStyle ? RitualTrack::layerNames()
                                                 : Electroacoustic::layerNames();
  unsigned solo = 0, mute = 0;
  if (!soloList.empty() && !parseLayers(soloList, names, solo)) return 2;
  if (!muteList.empty() && !parseLayers(muteList, names, mute)) return 2;
  opt.muted = mute | (solo ? ~solo : 0u);

  int rc;
  if (cinematic) {
    Soundscape scape(opt);
    rc = run(scape, opt, raw, out);
  } else if (ritualStyle) {
    RitualTrack scape(opt, bpm);
    rc = run(scape, opt, raw, out);
  } else {
    Electroacoustic scape(opt);
    rc = run(scape, opt, raw, out);
  }
  if (!raw && rc == 0) {
    if (ritualStyle)
      std::fprintf(stderr, "  replay this exact track with:  soundscape --style ritual -s %llu --bpm %g\n\n",
                   (unsigned long long)opt.seed, double(bpm));
    else
      std::fprintf(stderr, "  replay this exact piece with:  soundscape --style %s -s %llu -d %g\n\n",
                   cinematic ? "cinematic" : "electroacoustic", (unsigned long long)opt.seed, opt.duration);
  }
  return rc;
}
