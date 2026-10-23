// MOTE offline renderer. Plays a factory preset (or your own settings) through the same engine as the plugin and writes a WAV,
// measures loudness, and checks how different the presets sound from each other.
//
//   mote_render list                         every factory preset with its number
//   mote_render play <number|name> out.wav   one preset as a short chord phrase  [--sample file.wav] [--seconds 12]
//   mote_render all <folder>                 every preset as <folder>/NN name.wav
//   mote_render report [levels.json]         loudness + "how different" table; writes levels.json (the level knob that puts every
//                                            preset at the same loudness) when a file name is given
//
// The phrase: three chords played and released, 12 seconds in all, so you hear the attack, the held sound and the tail.
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../core/preset_apply.h"
#include "../core/presets_factory.h"
#include "wav.h"

using namespace mote;

namespace {

constexpr double kRate = 48000.0;
constexpr double kPhraseSeconds = 12.0;
constexpr double kTargetRmsDb = -21.5; // the loudness every preset is levelled to (measured while the chords sound)
constexpr double kMaxPeakDb = -1.0; // sparse plucks and drum hits are peaky: they may peak here, a little under their pad-like neighbours

struct Chord { double on, off; int notes[3]; };
const Chord kPhrase[] = {
    {0.0, 3.2, {48, 55, 60}},
    {3.6, 6.4, {50, 57, 62}},
    {6.8, 9.2, {52, 59, 64}},
};

struct Result {
    wav::Audio audio;
    double rmsDb = -100, peakDb = -100;
    double feat[16] = {};
};

SamplePtr gOwn; // a sample given with --sample

SamplePtr sourceFor(int idx) {
    if (idx == sources::kCustom) return gOwn ? gOwn : sources::make(1, kRate);
    return sources::make(idx, kRate);
}

Setup setupFromPreset(const presetdata::FactoryPreset& fp) {
    Setup s;
    const char* const* names = presetdata::soundIdNames();
    for (int i = 0; i < presetdata::kNumSoundIds; ++i) setNormalised(s, names[i], fp.v[i]);
    return s;
}

// energy in 10 bands (low-pass differences), a one-number sense of how much the loudness moves, and stereo width
void features(const wav::Audio& a, size_t from, size_t to, double* f) {
    static const double edges[] = {120, 250, 500, 1000, 2000, 3500, 6000, 9000, 14000};
    const int nb = 9;
    double lp[2][9][2] = {};
    double energy[10] = {};
    double mid = 0, side = 0;
    double a_[9];
    for (int k = 0; k < nb; ++k) a_[k] = 1.0 - std::exp(-2.0 * 3.14159265358979 * edges[k] / a.rate);
    for (size_t i = from; i < to; ++i) {
        const float m = 0.5f * (a.l[i] + a.r[i]);
        const float sd = 0.5f * (a.l[i] - a.r[i]);
        mid += m * m;
        side += sd * sd;
        double prev = 0.0;
        for (int k = 0; k < nb; ++k) {
            lp[0][k][0] += a_[k] * (m - lp[0][k][0]);
            lp[0][k][1] += a_[k] * (lp[0][k][0] - lp[0][k][1]);
            const double y = lp[0][k][1];
            energy[k] += (y - prev) * (y - prev);
            prev = y;
        }
        energy[nb] += (m - prev) * (m - prev);
    }
    double tot = 1e-12;
    for (double e : energy) tot += e;
    for (int k = 0; k <= nb; ++k) f[k] = 10.0 * std::log10(energy[k] / tot + 1e-6) / 6.0; // about -3 .. 0 each
    // movement: how much the 100 ms loudness wanders
    const size_t win = static_cast<size_t>(a.rate * 0.1);
    std::vector<double> env;
    for (size_t i = from; i + win < to; i += win) {
        double s = 0;
        for (size_t k = 0; k < win; ++k) s += a.l[i + k] * a.l[i + k] + a.r[i + k] * a.r[i + k];
        env.push_back(10.0 * std::log10(s / (2 * win) + 1e-10));
    }
    double mean = 0, var = 0;
    for (double e : env) mean += e;
    mean /= std::max<size_t>(1, env.size());
    for (double e : env) var += (e - mean) * (e - mean);
    f[10] = std::sqrt(var / std::max<size_t>(1, env.size())) / 3.0;
    f[11] = 10.0 * std::log10(side / (mid + 1e-12) + 1e-4) / 6.0;
    // how busy: the loudness changes between neighbouring 100 ms windows
    double flux = 0;
    for (size_t i = 1; i < env.size(); ++i) flux += std::fabs(env[i] - env[i - 1]);
    f[12] = flux / std::max<size_t>(1, env.size()) / 1.5;
}

Result render(const Setup& su, double gainDb = 0.0, double seconds = kPhraseSeconds) {
    Result res;
    Engine e;
    e.prepare(kRate);
    SamplePtr a = sourceFor(su.source), b = sourceFor(su.morphSource);
    e.setSamples(a.get(), b.get());
    Params p = su.p;
    p.outputGainDb = static_cast<float>(gainDb);
    e.setParams(p);
    Transport t;
    t.bpm = 100.0;
    e.setTransport(t);
    const size_t total = static_cast<size_t>(seconds * kRate);
    res.audio.rate = static_cast<int>(kRate);
    res.audio.l.assign(total, 0.0f);
    res.audio.r.assign(total, 0.0f);
    size_t pos = 0;
    std::vector<std::pair<size_t, std::pair<int, bool>>> events; // sample, (note, on)
    for (const Chord& c : kPhrase)
        for (int n : c.notes) {
            events.push_back({static_cast<size_t>(c.on * kRate), {n, true}});
            events.push_back({static_cast<size_t>(c.off * kRate), {n, false}});
        }
    std::sort(events.begin(), events.end(), [](auto& x, auto& y) { return x.first < y.first; });
    size_t ei = 0;
    while (pos < total) {
        size_t next = total;
        if (ei < events.size()) next = std::min(next, events[ei].first);
        next = std::min(next, pos + 512);
        while (ei < events.size() && events[ei].first <= pos) {
            if (events[ei].second.second) e.noteOn(events[ei].second.first, 0.85f);
            else e.noteOff(events[ei].second.first);
            ++ei;
        }
        if (next > pos) {
            e.process(&res.audio.l[pos], &res.audio.r[pos], static_cast<int>(next - pos));
            pos = next;
        }
    }
    // loudness while the chords sound (0.5 s .. 9 s), peak over everything
    double s = 0, pk = 0;
    const size_t i0 = static_cast<size_t>(0.5 * kRate), i1 = static_cast<size_t>(9.0 * kRate);
    for (size_t i = 0; i < total; ++i) pk = std::max(pk, static_cast<double>(std::max(std::fabs(res.audio.l[i]), std::fabs(res.audio.r[i]))));
    for (size_t i = i0; i < std::min(i1, total); ++i) s += 0.5 * (res.audio.l[i] * res.audio.l[i] + res.audio.r[i] * res.audio.r[i]);
    res.rmsDb = 10.0 * std::log10(s / std::max<size_t>(1, std::min(i1, total) - i0) + 1e-12);
    res.peakDb = 20.0 * std::log10(pk + 1e-9);
    features(res.audio, i0, std::min(i1, total), res.feat);
    return res;
}

int findPreset(const std::string& what) {
    const auto& bank = presetdata::factoryPresets();
    char* end = nullptr;
    const long n = std::strtol(what.c_str(), &end, 10);
    if (end != what.c_str() && *end == 0 && n >= 1 && n <= static_cast<long>(bank.size())) return static_cast<int>(n) - 1;
    for (size_t i = 0; i < bank.size(); ++i) {
        std::string nm = bank[i].name;
        std::string lo = nm, w = what;
        std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
        std::transform(w.begin(), w.end(), w.begin(), ::tolower);
        if (lo == w) return static_cast<int>(i);
    }
    return -1;
}

std::string safeName(std::string s) {
    for (char& c : s) if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
    return s;
}

bool loadOwn(const char* path) {
    wav::Audio a;
    std::string err;
    if (!wav::read(path, a, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return false; }
    std::vector<float> l = a.l, r = a.r;
    gOwn = sampleutil::make("Your sample", a.rate, std::move(l), std::move(r));
    return true;
}

// how far the "level" knob has to move to change the loudness by dB
float levelForGain(float level, double dB) {
    const double db = map::levelDb(level) + dB;
    return static_cast<float>(std::min(1.0, std::max(0.0, (db + 40.0) / 46.0)));
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: mote_render list | play <n|name> out.wav | all <folder> | report [levels.json]\n");
        return 1;
    }
    const auto& bank = presetdata::factoryPresets();
    std::string cmd = argv[1];
    double seconds = kPhraseSeconds;
    for (int i = 2; i + 1 < argc; ++i) {
        if (!std::strcmp(argv[i], "--sample") && !loadOwn(argv[i + 1])) return 1;
        if (!std::strcmp(argv[i], "--seconds")) seconds = std::atof(argv[i + 1]);
    }

    if (cmd == "list") {
        for (size_t i = 0; i < bank.size(); ++i)
            std::printf("%3zu  %-10s %-24s %s\n", i + 1, bank[i].category, bank[i].name, bank[i].hint);
        return 0;
    }
    if (cmd == "play" && argc >= 4) {
        const int idx = findPreset(argv[2]);
        if (idx < 0) { std::fprintf(stderr, "no such preset\n"); return 1; }
        const Result r = render(setupFromPreset(bank[idx]), 0.0, seconds);
        wav::write16(argv[3], r.audio);
        std::printf("%s: loudness %.1f dB, peak %.1f dB -> %s\n", bank[idx].name, r.rmsDb, r.peakDb, argv[3]);
        return 0;
    }
    if (cmd == "all" && argc >= 3) {
        for (size_t i = 0; i < bank.size(); ++i) {
            const Result r = render(setupFromPreset(bank[i]));
            char fn[512];
            std::snprintf(fn, sizeof fn, "%s/%02zu %s.wav", argv[2], i + 1, safeName(bank[i].name).c_str());
            wav::write16(fn, r.audio);
            std::printf("%3zu %-24s loudness %6.1f dB  peak %6.1f dB\n", i + 1, bank[i].name, r.rmsDb, r.peakDb);
        }
        return 0;
    }
    if (cmd == "report") {
        const char* levelsPath = argc >= 3 && argv[2][0] != '-' ? argv[2] : nullptr;
        std::vector<Result> rs;
        std::vector<float> newLevel;
        int bad = 0;
        std::printf("%3s %-10s %-24s %8s %8s %8s   %s\n", "#", "category", "name", "loud dB", "peak dB", "set lvl", "nearest other preset (distance)");
        for (size_t i = 0; i < bank.size(); ++i) {
            const Setup su = setupFromPreset(bank[i]);
            Result r = render(su);
            // bring to the target loudness (the level knob is a plain gain before the limiter), but keep the peak in check
            double need = kTargetRmsDb - r.rmsDb;
            need = std::min(need, kMaxPeakDb - r.peakDb);
            const float lv = levelForGain(su.p.level, need);
            newLevel.push_back(lv);
            if (levelsPath) r = render(su, need); // measure again at the new level (the soft limiter is not exactly linear)
            rs.push_back(r);
        }
        for (size_t i = 0; i < rs.size(); ++i) {
            double best = 1e9;
            size_t bi = 0;
            for (size_t j = 0; j < rs.size(); ++j) {
                if (j == i) continue;
                double d = 0;
                for (int k = 0; k < 13; ++k) d += (rs[i].feat[k] - rs[j].feat[k]) * (rs[i].feat[k] - rs[j].feat[k]);
                d = std::sqrt(d);
                if (d < best) { best = d; bi = j; }
            }
            const bool close = best < 0.35;
            const bool off = rs[i].rmsDb < kTargetRmsDb - 4.5 && levelsPath;
            if (close) ++bad;
            std::printf("%3zu %-10s %-24s %8.1f %8.1f %8.3f   %s (%.2f)%s%s\n", i + 1, bank[i].category, bank[i].name, rs[i].rmsDb, rs[i].peakDb,
                        newLevel[i], bank[bi].name, best, close ? "  <-- too similar" : "", off ? "  <-- loudness off" : "");
        }
        std::printf("%zu presets, %d too similar to another\n", rs.size(), bad);
        if (levelsPath) {
            FILE* f = std::fopen(levelsPath, "w");
            if (!f) { std::fprintf(stderr, "cannot write %s\n", levelsPath); return 1; }
            std::fprintf(f, "{\n");
            for (size_t i = 0; i < bank.size(); ++i)
                std::fprintf(f, "  \"%s\": %.4f%s\n", bank[i].name, newLevel[i], i + 1 < bank.size() ? "," : "");
            std::fprintf(f, "}\n");
            std::fclose(f);
            std::printf("level suggestions -> %s\n", levelsPath);
        }
        return 0;
    }
    std::fprintf(stderr, "unknown command\n");
    return 1;
}
