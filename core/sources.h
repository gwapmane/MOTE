// MOTE - the built-in sources: seven short sounds made in code (no audio files to ship), used by the factory presets
// and as a starting point before you drop in a sample of your own. The same sound every time.
#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "dsp.h"
#include "sample.h"

namespace mote {
namespace sources {

constexpr int kCount = 7;
constexpr int kCustom = 7; // "Your sample": whatever you dropped in
inline const char* name(int i) {
    static const char* n[kCount + 1] = {"Guitar Chords", "Warm Pad", "Choir Ah", "Glass Bell", "Tape Dust", "Drum Loop", "Pluck Run", "Your sample"};
    return n[std::min(kCount, std::max(0, i))];
}
// the pitch (as a note, 0 = C) each built-in sound is built around: what ROOT should say for the Scale lock to line up
inline int keyOf(int i) {
    static const int k[kCount + 1] = {4, 9, 9, 4, 0, 0, 5, 0};
    return k[std::min(kCount, std::max(0, i))];
}

namespace detail {

struct Rng {
    uint32_t s = 2463534242u;
    explicit Rng(uint32_t seed = 1) : s(2463534242u ^ (seed * 2654435761u)) {}
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return static_cast<float>(s & 0xffffff) / 16777215.0f; } // 0..1
    float bi() { return next() * 2.0f - 1.0f; }
};

struct Buf {
    std::vector<float> l, r;
    double sr;
    Buf(double fs, double seconds) : l(static_cast<size_t>(fs * seconds), 0.0f), r(l), sr(fs) {}
    int n() const { return static_cast<int>(l.size()); }
    void add(int i, float v, float pan) { // pan -1..1
        if (i < 0 || i >= n()) return;
        const float a = (pan + 1.0f) * 0.7853982f;
        l[static_cast<size_t>(i)] += v * std::cos(a) * 1.4142f;
        r[static_cast<size_t>(i)] += v * std::sin(a) * 1.4142f;
    }
};

// Karplus-Strong plucked string
inline void pluck(Buf& b, double t0, double f, double amp, float pan, float decay, uint32_t seed, float bright = 0.5f) {
    const int N = std::max(4, static_cast<int>(b.sr / f));
    std::vector<float> d(static_cast<size_t>(N));
    Rng rng(seed);
    float lp = 0.0f;
    for (int i = 0; i < N; ++i) {
        const float x = rng.bi();
        lp += (x - lp) * (0.25f + 0.7f * bright);
        d[static_cast<size_t>(i)] = lp * 1.6f;
    }
    int idx = 0;
    const int start = static_cast<int>(t0 * b.sr);
    for (int i = start; i < b.n(); ++i) {
        const float y = d[static_cast<size_t>(idx)];
        const int nx = idx + 1 == N ? 0 : idx + 1;
        d[static_cast<size_t>(idx)] = 0.5f * (y + d[static_cast<size_t>(nx)]) * decay;
        idx = nx;
        b.add(i, y * static_cast<float>(amp), pan);
        if (i - start > static_cast<int>(7.0 * b.sr)) break;
    }
}

inline std::vector<float> table(const std::vector<float>& harmonicAmps) {
    constexpr int T = 2048;
    std::vector<float> t(T, 0.0f);
    for (size_t h = 0; h < harmonicAmps.size(); ++h)
        for (int i = 0; i < T; ++i)
            t[static_cast<size_t>(i)] += harmonicAmps[h] * static_cast<float>(std::sin(2.0 * kPi * static_cast<double>((h + 1) * static_cast<size_t>(i)) / T));
    return t;
}
inline float readTable(const std::vector<float>& t, double ph) {
    const double x = (ph - std::floor(ph)) * static_cast<double>(t.size());
    const size_t i = static_cast<size_t>(x), j = (i + 1) % t.size();
    const float f = static_cast<float>(x - static_cast<double>(i));
    return t[i] + (t[j] - t[i]) * f;
}

} // namespace detail

inline SamplePtr guitarChords(double sr) {
    detail::Buf b(sr, 7.0);
    const double e[6] = {82.41, 123.47, 164.81, 196.00, 246.94, 369.99};   // E minor 9
    const double c[6] = {130.81, 164.81, 196.00, 246.94, 293.66, 392.00};  // C major 9
    for (int k = 0; k < 6; ++k) {
        detail::pluck(b, 0.02 + 0.024 * k, e[k], 0.9, -0.5f + 0.2f * static_cast<float>(k), 0.9965f, 11u + static_cast<uint32_t>(k), 0.45f);
        detail::pluck(b, 3.52 + 0.02 * k, c[k], 0.9, -0.5f + 0.2f * static_cast<float>(k), 0.9965f, 31u + static_cast<uint32_t>(k), 0.55f);
    }
    return sampleutil::make("Guitar Chords", sr, std::move(b.l), std::move(b.r));
}

inline SamplePtr warmPad(double sr) {
    detail::Buf b(sr, 8.0);
    std::vector<float> amps;
    for (int h = 1; h <= 28; ++h) amps.push_back(1.0f / static_cast<float>(h) * std::exp(-static_cast<float>(h) / 9.0f));
    const auto tab = detail::table(amps);
    const double notes[6] = {110.0, 164.81, 196.0, 246.94, 261.63, 329.63}; // A minor 9
    const double det[3] = {-0.0035, 0.0, 0.0042};
    for (int k = 0; k < 6; ++k)
        for (int d = 0; d < 3; ++d) {
            double ph = 0.37 * k + 0.19 * d;
            const double inc = notes[k] * (1.0 + det[d]) / sr;
            const float pan = -0.6f + 0.24f * static_cast<float>(k) + 0.12f * static_cast<float>(d - 1);
            for (int i = 0; i < b.n(); ++i) {
                const double t = static_cast<double>(i) / sr;
                const float env = static_cast<float>(std::min(1.0, t / 2.2) * (t > 6.0 ? std::max(0.0, 1.0 - (t - 6.0) / 2.0) : 1.0));
                const float wob = 1.0f + 0.002f * static_cast<float>(std::sin(2.0 * kPi * (0.31 + 0.07 * d) * t + k));
                ph += inc * wob;
                b.add(i, detail::readTable(tab, ph) * env * 0.18f, pan);
            }
        }
    return sampleutil::make("Warm Pad", sr, std::move(b.l), std::move(b.r));
}

inline SamplePtr choirAh(double sr) {
    detail::Buf b(sr, 8.0);
    const double f0s[4] = {110.0, 164.81, 220.0, 277.18};
    auto vowelTable = [](double f0, const double* F, const double* bw) {
        std::vector<float> a;
        for (int h = 1; h <= 40; ++h) {
            const double f = f0 * h;
            double g = 0.02;
            for (int k = 0; k < 3; ++k) g += (k == 0 ? 1.0 : (k == 1 ? 0.7 : 0.35)) * std::exp(-0.5 * std::pow((f - F[k]) / bw[k], 2.0));
            a.push_back(static_cast<float>(g / std::pow(static_cast<double>(h), 0.55)));
        }
        return detail::table(a);
    };
    const double ahF[3] = {730, 1090, 2440}, ahB[3] = {90, 110, 160}, ohF[3] = {570, 840, 2410}, ohB[3] = {80, 100, 150};
    detail::Rng rng(7);
    for (int v = 0; v < 4; ++v) {
        const auto ta = vowelTable(f0s[v], ahF, ahB), to = vowelTable(f0s[v], ohF, ohB);
        double ph = 0.1 * v;
        const float pan = -0.7f + 0.47f * static_cast<float>(v);
        for (int i = 0; i < b.n(); ++i) {
            const double t = static_cast<double>(i) / sr;
            const float env = static_cast<float>(std::min(1.0, t / 1.6) * (t > 6.4 ? std::max(0.0, 1.0 - (t - 6.4) / 1.6) : 1.0));
            const double vib = 1.0 + 0.004 * std::sin(2.0 * kPi * (5.1 + 0.3 * v) * t) * std::min(1.0, t / 2.0);
            ph += f0s[v] * vib / sr;
            const float mix = static_cast<float>(0.5 - 0.5 * std::cos(kPi * std::min(1.0, std::max(0.0, (t - 3.5) / 2.0))));
            const float s = detail::readTable(ta, ph) * (1.0f - mix) + detail::readTable(to, ph) * mix;
            b.add(i, (s + 0.02f * rng.bi()) * env * 0.35f, pan);
        }
    }
    return sampleutil::make("Choir Ah", sr, std::move(b.l), std::move(b.r));
}

inline SamplePtr glassBell(double sr) {
    detail::Buf b(sr, 8.0);
    const double ratio[6] = {1.0, 2.76, 5.40, 8.93, 13.34, 18.64}, amp[6] = {1.0, 0.6, 0.45, 0.3, 0.2, 0.12};
    const double hz[4] = {659.26, 493.88, 783.99, 587.33}, at[4] = {0.0, 1.25, 2.7, 4.4};
    detail::Rng rng(3);
    for (int n = 0; n < 4; ++n) {
        const float pan = (n & 1) ? 0.45f : -0.45f;
        const int s0 = static_cast<int>(at[n] * sr);
        for (int p = 0; p < 6; ++p) {
            const double f = hz[n] * ratio[p], tau = 2.2 / std::pow(ratio[p], 0.62);
            if (f > sr * 0.45) continue;
            const double w = 2.0 * kPi * f / sr;
            for (int i = s0; i < b.n(); ++i) {
                const double t = static_cast<double>(i - s0) / sr;
                if (t > tau * 7.0) break;
                b.add(i, static_cast<float>(amp[p] * std::exp(-t / tau) * std::sin(w * (i - s0) + 0.7 * p) * 0.22), pan);
            }
        }
        for (int i = 0; i < static_cast<int>(0.004 * sr); ++i)
            b.add(s0 + i, rng.bi() * 0.25f * (1.0f - static_cast<float>(i) / static_cast<float>(0.004 * sr)), pan);
    }
    return sampleutil::make("Glass Bell", sr, std::move(b.l), std::move(b.r));
}

inline SamplePtr tapeDust(double sr) {
    detail::Buf b(sr, 6.0);
    detail::Rng rng(5);
    float lpL = 0, lpR = 0, bpL = 0, bpR = 0, bp2L = 0, bp2R = 0;
    for (int i = 0; i < b.n(); ++i) {
        const double t = static_cast<double>(i) / sr;
        const float nL = rng.bi(), nR = rng.bi();
        lpL += (nL - lpL) * 0.18f;
        lpR += (nR - lpR) * 0.18f;
        const float fc = 0.04f + 0.03f * static_cast<float>(std::sin(2.0 * kPi * 0.33 * t));
        bpL += (lpL - bpL) * fc; bp2L += (bpL - bp2L) * fc;
        bpR += (lpR - bpR) * fc; bp2R += (bpR - bp2R) * fc;
        const float breath = 0.5f + 0.5f * static_cast<float>(std::sin(2.0 * kPi * 0.21 * t));
        const float hum = 0.05f * static_cast<float>(std::sin(2.0 * kPi * 55.0 * t) * (0.6 + 0.4 * std::sin(2.0 * kPi * 0.5 * t)));
        b.l[static_cast<size_t>(i)] = lpL * 0.12f + (bpL - bp2L) * 2.6f * breath + hum;
        b.r[static_cast<size_t>(i)] = lpR * 0.12f + (bpR - bp2R) * 2.6f * breath + hum * 0.9f;
    }
    for (int i = 0; i < b.n(); ++i)
        if (rng.next() < 0.0009f) { // vinyl crackle
            const float a = (rng.next() > 0.85f ? 1.0f : 0.25f) * rng.next();
            const float pan = rng.bi();
            for (int k = 0; k < 40 && i + k < b.n(); ++k) b.add(i + k, a * rng.bi() * std::exp(-static_cast<float>(k) / 7.0f), pan);
        }
    return sampleutil::make("Tape Dust", sr, std::move(b.l), std::move(b.r));
}

inline SamplePtr drumLoop(double sr) {
    detail::Buf b(sr, 4.8);
    detail::Rng rng(9);
    const double step = 60.0 / 100.0 / 4.0; // sixteenth notes at 100 bpm
    auto kick = [&](double t0, float a) {
        const int s0 = static_cast<int>(t0 * sr);
        double ph = 0;
        for (int i = 0; i < static_cast<int>(0.45 * sr) && s0 + i < b.n(); ++i) {
            const double t = static_cast<double>(i) / sr;
            ph += 2.0 * kPi * (45.0 + 110.0 * std::exp(-t / 0.035)) / sr;
            b.add(s0 + i, a * static_cast<float>(std::sin(ph) * std::exp(-t / 0.16)) + (i < 60 ? a * 0.3f * rng.bi() : 0.0f), 0.0f);
        }
    };
    auto snare = [&](double t0, float a) {
        const int s0 = static_cast<int>(t0 * sr);
        float prev = 0;
        for (int i = 0; i < static_cast<int>(0.35 * sr) && s0 + i < b.n(); ++i) {
            const double t = static_cast<double>(i) / sr;
            const float n = rng.bi();
            const float hp = n - prev;
            prev = n;
            b.add(s0 + i, a * (0.55f * hp * static_cast<float>(std::exp(-t / 0.075)) + 0.5f * static_cast<float>(std::sin(2.0 * kPi * 190.0 * t) * std::exp(-t / 0.05))), 0.05f);
        }
    };
    auto hat = [&](double t0, float a, double tau, float pan) {
        const int s0 = static_cast<int>(t0 * sr);
        float prev = 0, prev2 = 0;
        for (int i = 0; i < static_cast<int>(tau * 7.0 * sr) && s0 + i < b.n(); ++i) {
            const double t = static_cast<double>(i) / sr;
            const float n = rng.bi();
            const float h = n - 2.0f * prev + prev2;
            prev2 = prev;
            prev = n;
            b.add(s0 + i, a * 0.5f * h * static_cast<float>(std::exp(-t / tau)), pan);
        }
    };
    for (int bar = 0; bar < 2; ++bar) {
        const double o = bar * 16 * step;
        for (int s : {0, 5, 8, 10 + bar * 2}) kick(o + s * step, 1.0f);
        for (int s : {4, 12}) snare(o + s * step, 0.9f);
        for (int s = 0; s < 16; s += 2) hat(o + s * step, s % 4 == 0 ? 0.8f : 0.5f, 0.022, 0.3f);
        hat(o + 14 * step, 0.7f, 0.11, -0.2f);
        if (bar == 1) snare(o + 15 * step, 0.5f);
    }
    return sampleutil::make("Drum Loop", sr, std::move(b.l), std::move(b.r));
}

inline SamplePtr pluckRun(double sr) {
    detail::Buf b(sr, 6.2);
    const double nt[16] = {146.83, 174.61, 220.0, 261.63, 293.66, 220.0, 174.61, 220.0, 130.81, 164.81, 196.0, 261.63, 329.63, 261.63, 196.0, 164.81};
    for (int i = 0; i < 16; ++i)
        detail::pluck(b, 0.05 + 0.25 * i, nt[i], 0.8, -0.4f + 0.05f * static_cast<float>(i % 9), 0.9945f, 100u + static_cast<uint32_t>(i), 0.6f);
    return sampleutil::make("Pluck Run", sr, std::move(b.l), std::move(b.r));
}

inline SamplePtr make(int id, double sr = 44100.0) {
    switch (std::min(kCount - 1, std::max(0, id))) {
        case 0: return guitarChords(sr);
        case 1: return warmPad(sr);
        case 2: return choirAh(sr);
        case 3: return glassBell(sr);
        case 4: return tapeDust(sr);
        case 5: return drumLoop(sr);
        default: return pluckRun(sr);
    }
}

} // namespace sources
} // namespace mote
