#!/usr/bin/env python3
"""Mix analysis for RITE and its reference tracks (numpy + ffmpeg only).

Any format ffmpeg reads works (wav, mp3, flac, opus...).

  mixcheck.py bands  FILE[@START] ...   band balance, crest, stereo correlation, tempo
  mixcheck.py reso   FILE[@START] ...   narrow resonance peaks (fine vs 1-octave smoothing)
  mixcheck.py mono   FILE ...           mono-sum loss over the whole file
  mixcheck.py loud   FILE[@START] ...   LUFS and true peak (ffmpeg ebur128)
  mixcheck.py pilot  FULL NOKICK        pilot-tone squash test (see below)

FILE@START analyses 16 s from START seconds (default 0.5), e.g. rite.wav@121.
Compare the loudest 16 s of each reference with RITE's peak (about 121 s)
and its first drive (about 75 s).

Pilot test: add SynthDef(\\pilot, { Out.ar(busSpace, (SinOsc.ar([237, 1013,
3970, 9900]).sum * 0.01) ! 2) }) to a copy of rite.scd, start it after the
master, and render twice: normally and with MUTE=kick. This prints how far
each tone drops while the kick plays. About 0-1 dB is glue; more means the
kick squashes the mix.
"""
import subprocess, sys
import numpy as np

SR = 48000
BANDS = [(20, 60, "sub"), (60, 120, "low"), (120, 300, "lowmid"), (300, 800, "mid-lo"),
         (800, 2500, "mid"), (2500, 6000, "presence"), (6000, 15000, "air")]


def load(spec, dur=None):
    path, _, start = spec.partition("@")
    cmd = ["ffmpeg", "-v", "error", "-ss", start or "0.5", "-i", path]
    if dur: cmd += ["-t", str(dur)]
    cmd += ["-f", "f32le", "-ac", "2", "-ar", str(SR), "-"]
    raw = subprocess.run(cmd, capture_output=True, check=True).stdout
    return np.frombuffer(raw, dtype=np.float32).reshape(-1, 2).astype(np.float64)


def db(v): return 20 * np.log10(v + 1e-12)


def band(x, lo, hi):
    X = np.fft.rfft(x, axis=0); f = np.fft.rfftfreq(len(x), 1 / SR)
    X[(f < lo) | (f >= hi)] = 0
    return np.fft.irfft(X, n=len(x), axis=0)


def tempo(x):
    m = x.mean(axis=1); hop = 512
    e = np.array([np.sqrt((m[i:i + 1024] ** 2).mean()) for i in range(0, len(m) - 1024, hop)])
    o = np.maximum(np.diff(e), 0); o -= o.mean()
    ac = np.correlate(o, o, mode="full")[len(o) - 1:]
    lags = np.arange(len(ac)); bpm = 60 * SR / hop / np.maximum(lags, 1)
    ok = (bpm > 90) & (bpm < 180)
    return 60 * SR / hop / lags[ok][np.argmax(ac[ok])]


def cmd_bands(specs):
    res = {}
    for s in specs:
        x = load(s, 16); tot = np.sqrt((x ** 2).mean()); row = []
        for lo, hi, _ in BANDS:
            b = band(x, lo, hi); L, R = b[:, 0], b[:, 1]
            rms = np.sqrt((b ** 2).mean())
            corr = (L * R).mean() / (np.sqrt((L ** 2).mean() * (R ** 2).mean()) + 1e-12)
            row.append((db(rms) - db(tot), db(np.abs(b).max() / (rms + 1e-12)), corr))
        res[s] = (row, tempo(x), db(np.abs(x).max() / tot))
    names = [s.split("/")[-1][:24] for s in specs]
    print("%-9s" % "band" + "".join("%24s" % n for n in names))
    print("%-9s" % "" + "".join("%24s" % "level  crest   corr" for _ in names))
    for i, (_, _, bn) in enumerate(BANDS):
        print("%-9s" % bn + "".join("     %6.1f %6.1f %6.2f" % res[s][0][i] for s in specs))
    for s, n in zip(specs, names):
        print("%s: tempo ~ %.0f BPM, full-band crest %.1f dB" % (n, res[s][1], res[s][2]))


def cmd_reso(specs):
    for s in specs:
        m = load(s, 16).mean(axis=1); N = 16384
        P = np.mean([np.abs(np.fft.rfft(m[i:i + N] * np.hanning(N))) ** 2
                     for i in range(0, len(m) - N, N // 2)], axis=0)
        f = np.fft.rfftfreq(N, 1 / SR); sp = 10 * np.log10(P + 1e-20)

        def smooth(frac):
            out = np.empty_like(sp)
            for i, fc in enumerate(f):
                if fc <= 0: out[i] = sp[i]; continue
                sel = (f >= fc * 2 ** (-frac / 2)) & (f <= fc * 2 ** (frac / 2))
                out[i] = 10 * np.log10(np.mean(10 ** (sp[sel] / 10)))
            return out
        d = smooth(1 / 24) - smooth(1.0)
        zone = (f > 300) & (f < 6000)
        print("%s  peakiness 300 Hz-6 kHz: %.2f dB" % (s, np.sqrt((d[zone] ** 2).mean())))
        idx = [i for i in range(1, len(d) - 1) if 150 < f[i] < 6000 and d[i] > 4
               and d[i] >= d[i - 1] and d[i] >= d[i + 1]]
        idx = sorted(sorted(idx, key=lambda i: -d[i])[:10])
        print("   peaks: " + ", ".join("%d Hz +%.1f" % (f[i], d[i]) for i in idx))


def cmd_mono(files):
    for fn in files:
        x = load(fn)
        st = np.sqrt((x ** 2).mean()); mono = np.sqrt((x.mean(axis=1) ** 2).mean())
        print("%s  mono-sum loss: %.2f dB" % (fn, db(mono / st)))


def cmd_loud(specs):
    for s in specs:
        path, _, start = s.partition("@")
        out = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-ss", start or "0", "-t", "50", "-i", path,
                              "-af", "ebur128=peak=true", "-f", "null", "-"], capture_output=True, text=True).stderr
        summ = out[out.rfind("Summary"):]
        vals = [l.strip() for l in summ.splitlines() if l.strip().startswith(("I:", "Peak:"))]
        print("%s  %s" % (s, "   ".join(vals)))


def cmd_pilot(full, nok):
    A = load(full).mean(axis=1); B = load(nok).mean(axis=1)

    def track(x, f0, a, b, win=0.1):
        t = np.arange(len(x)) / SR; seg = slice(int(a * SR), int(b * SR))
        z = x[seg] * np.exp(-2j * np.pi * f0 * t[seg]); n = int(win * SR)
        return 2 * np.abs(np.convolve(z, np.ones(n) / n, mode="valid")[::int(0.01 * SR)])
    for a, b, name in [(55, 100, "the rite"), (113, 160, "the peak")]:
        print("== %s: how far each pilot tone drops while the kick plays" % name)
        for f0 in [237, 1013, 3970, 9900]:
            ta, tb = track(A, f0, a, b), track(B, f0, a, b)
            print("  %5d Hz   average %+5.1f dB" % (f0, db(np.median(ta) / np.median(tb))))


if __name__ == "__main__":
    if len(sys.argv) < 3: sys.exit(__doc__)
    c, args = sys.argv[1], sys.argv[2:]
    {"bands": cmd_bands, "reso": cmd_reso, "mono": cmd_mono, "loud": cmd_loud,
     "pilot": lambda a: cmd_pilot(*a)}[c](args)
