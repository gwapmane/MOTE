// MOTE - a sample, as the grain engine sees it: stereo floats, plus the transient positions found in it (for MAGNET and
// the WALK slices) and a small picture of the waveform (for the window). Built once when a sample is loaded, never
// changed after.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mote {

constexpr int kOverviewCols = 2000;

struct Sample {
    std::string name;
    double sr = 44100.0;
    std::vector<float> l, r;           // same length; r == l for a mono sample
    std::vector<int> onsets;           // sample indices of the transients, ascending (always at least one: 0)
    std::vector<float> ovMin, ovMax;   // kOverviewCols columns: lowest and highest value (left + right averaged)
    float peak = 0.0f;
    bool stereo = false;

    int size() const { return static_cast<int>(l.size()); }
    double seconds() const { return sr > 0 ? static_cast<double>(l.size()) / sr : 0.0; }
};
using SamplePtr = std::shared_ptr<const Sample>;

namespace sampleutil {

inline void findOnsets(Sample& s) {
    s.onsets.clear();
    const int n = s.size();
    const int hop = std::max(64, static_cast<int>(s.sr / 172.0)); // ~5.8 ms
    const int win = hop * 2;
    const int frames = n / hop;
    if (frames < 8) { s.onsets.push_back(0); return; }
    std::vector<float> e(static_cast<size_t>(frames), 0.0f), flux(static_cast<size_t>(frames), 0.0f);
    for (int f = 0; f < frames; ++f) {
        double acc = 0.0;
        const int a = f * hop, b = std::min(n, a + win);
        for (int i = a; i < b; ++i) {
            const double m = 0.5 * (s.l[static_cast<size_t>(i)] + s.r[static_cast<size_t>(i)]);
            acc += m * m;
        }
        e[static_cast<size_t>(f)] = static_cast<float>(std::log1p(60.0 * std::sqrt(acc / std::max(1, b - a))));
    }
    float maxFlux = 0.0f;
    for (int f = 1; f < frames; ++f) {
        flux[static_cast<size_t>(f)] = std::max(0.0f, e[static_cast<size_t>(f)] - e[static_cast<size_t>(f - 1)]);
        maxFlux = std::max(maxFlux, flux[static_cast<size_t>(f)]);
    }
    const int minGap = std::max(2, static_cast<int>(0.09 * s.sr / hop));
    int last = -minGap;
    for (int f = 2; f < frames - 1; ++f) {
        double m = 0.0;
        int c = 0;
        for (int k = std::max(1, f - 24); k <= std::min(frames - 1, f + 24); ++k) { m += flux[static_cast<size_t>(k)]; ++c; }
        m /= std::max(1, c);
        const float thr = static_cast<float>(m * 1.8 + 0.12 * maxFlux);
        const float v = flux[static_cast<size_t>(f)];
        if (v > thr && v >= flux[static_cast<size_t>(f - 1)] && v >= flux[static_cast<size_t>(f + 1)] && f - last >= minGap) {
            s.onsets.push_back(std::max(0, (f - 1) * hop));
            last = f;
            if (s.onsets.size() >= 96) break;
        }
    }
    if (s.onsets.empty() || s.onsets.front() > hop * 4) s.onsets.insert(s.onsets.begin(), 0);
}

inline void makeOverview(Sample& s) {
    s.ovMin.assign(kOverviewCols, 0.0f);
    s.ovMax.assign(kOverviewCols, 0.0f);
    const int n = s.size();
    if (n == 0) return;
    for (int c = 0; c < kOverviewCols; ++c) {
        const int a = static_cast<int>(static_cast<long long>(n) * c / kOverviewCols);
        const int b = std::max(a + 1, static_cast<int>(static_cast<long long>(n) * (c + 1) / kOverviewCols));
        float lo = 1e9f, hi = -1e9f;
        const int step = std::max(1, (b - a) / 64);
        for (int i = a; i < b && i < n; i += step) {
            const float v = 0.5f * (s.l[static_cast<size_t>(i)] + s.r[static_cast<size_t>(i)]);
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
        s.ovMin[static_cast<size_t>(c)] = lo > hi ? 0.0f : lo;
        s.ovMax[static_cast<size_t>(c)] = lo > hi ? 0.0f : hi;
    }
}

// Takes the audio, normalises it to a peak of 0.9 (so every sample starts at a similar level), and analyses it.
inline SamplePtr make(const std::string& name, double sr, std::vector<float> l, std::vector<float> r) {
    auto s = std::make_shared<Sample>();
    s->name = name;
    s->sr = sr;
    s->stereo = !r.empty() && r.size() == l.size();
    if (r.size() != l.size()) r = l;
    float pk = 0.0f;
    for (size_t i = 0; i < l.size(); ++i) pk = std::max(pk, std::max(std::fabs(l[i]), std::fabs(r[i])));
    const float g = pk > 1e-6f ? 0.9f / pk : 1.0f;
    for (auto& v : l) v *= g;
    for (auto& v : r) v *= g;
    s->l = std::move(l);
    s->r = std::move(r);
    s->peak = pk * g;
    findOnsets(*s);
    makeOverview(*s);
    return s;
}

} // namespace sampleutil
} // namespace mote
