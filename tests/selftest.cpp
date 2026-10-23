// MOTE - checks of the grain engine, with no plugin code: it must make sound, stay finite and under the limiter, give
// the same audio whatever the block size, stay at a sensible level whatever the knobs do, and not hog the CPU.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "../core/grain.h"
#include "../core/preset_apply.h"
#include "../core/sources.h"

using namespace mote;

static int g_fail = 0, g_checks = 0;
#define CHECK(cond, ...)                         \
    do {                                         \
        ++g_checks;                              \
        if (!(cond)) {                           \
            ++g_fail;                            \
            std::printf("  FAIL  ");             \
            std::printf(__VA_ARGS__);            \
            std::printf("\n");                   \
        }                                        \
    } while (0)

struct Stats { double rms = 0, peak = 0; bool finite = true; double tailRms = 0; };

static double dbOf(double x) { return 20.0 * std::log10(x + 1e-9); }

// renders `secs` with the given notes held for `hold` seconds (then released), in blocks of `block`
static Stats render(Engine& e, double secs, double hold, const std::vector<int>& notes, int block, std::vector<float>* keep = nullptr, double sr = 48000.0) {
    const int total = static_cast<int>(secs * sr), holdN = static_cast<int>(hold * sr);
    std::vector<float> l(static_cast<size_t>(block)), r(static_cast<size_t>(block));
    Stats s;
    double sum = 0, tail = 0;
    long cnt = 0, tcnt = 0;
    bool on = false, off = false;
    for (int pos = 0; pos < total; pos += block) {
        const int n = std::min(block, total - pos);
        // note events land on block boundaries here; the block-size test uses a fixed event grid instead (see below)
        if (!on) { for (int k : notes) e.noteOn(k, 0.9f); on = true; }
        if (!off && pos >= holdN) { for (int k : notes) e.noteOff(k); off = true; }
        e.process(l.data(), r.data(), n);
        for (int i = 0; i < n; ++i) {
            const float a = l[static_cast<size_t>(i)], b = r[static_cast<size_t>(i)];
            if (!std::isfinite(a) || !std::isfinite(b)) s.finite = false;
            s.peak = std::max({s.peak, static_cast<double>(std::fabs(a)), static_cast<double>(std::fabs(b))});
            sum += 0.5 * (a * a + b * b);
            ++cnt;
            if (pos + i > total - static_cast<int>(0.5 * sr)) { tail += 0.5 * (a * a + b * b); ++tcnt; }
            if (keep) { keep->push_back(a); keep->push_back(b); }
        }
    }
    s.rms = std::sqrt(sum / std::max(1L, cnt));
    s.tailRms = std::sqrt(tail / std::max(1L, tcnt));
    return s;
}

static void setup(Engine& e, const Params& p, int srcId = 1, int srcB = 3, double sr = 48000.0) {
    e.prepare(sr);
    static SamplePtr keepA, keepB;
    keepA = sources::make(srcId, sr);
    keepB = sources::make(srcB, sr);
    e.setSamples(keepA.get(), keepB.get());
    e.setParams(p);
    Transport t;
    t.bpm = 120;
    t.playing = true;
    t.hostValid = false;
    e.setTransport(t);
}

int main() {
    std::printf("MOTE engine self-test\n");

    // 1. silence in, silence out; no notes means no sound
    {
        Params p;
        Engine e; setup(e, p);
        const auto s = render(e, 0.5, 0.0, {}, 512);
        CHECK(s.peak < 1e-6, "no notes should be silent (peak %.6f)", s.peak);
    }

    // 2. every built-in sample plays: audible, finite, under the limiter, and it dies away after the release
    for (int src = 0; src < sources::kCount; ++src) {
        Params p;
        p.release = 0.3f;
        Engine e; setup(e, p, src);
        const auto s = render(e, 5.0, 2.0, {60}, 512);
        std::printf("  %-14s rms %6.1f dB  peak %6.1f dB  tail %6.1f dB\n", sources::name(src), dbOf(s.rms), dbOf(s.peak), dbOf(s.tailRms));
        CHECK(s.finite, "%s: output not finite", sources::name(src));
        CHECK(s.peak <= 1.0001, "%s: peak %.3f over full scale", sources::name(src), s.peak);
        CHECK(dbOf(s.rms) > -45.0, "%s: too quiet (%.1f dB)", sources::name(src), dbOf(s.rms));
        CHECK(dbOf(s.tailRms) < -70.0, "%s: does not die away (tail %.1f dB)", sources::name(src), dbOf(s.tailRms));
    }

    // 3. the same audio whatever the block size
    {
        Params p;
        p.scatter = 0.4f; p.jitter = 0.3f; p.density = 0.7f; p.reverse = 0.3f; p.delayMix = 0.3f; p.reverbMix = 0.3f; p.filterOn = true;
        std::vector<float> a, b, c;
        { Engine e; setup(e, p); render(e, 1.5, 1.024, {55, 62}, 64, &a); }
        { Engine e; setup(e, p); render(e, 1.5, 1.024, {55, 62}, 512, &b); }
        { Engine e; setup(e, p); render(e, 1.5, 1.024, {55, 62}, 4096, &c); }
        double d1 = 0, d2 = 0;
        for (size_t i = 0; i < a.size() && i < b.size(); ++i) d1 = std::max(d1, static_cast<double>(std::fabs(a[i] - b[i])));
        for (size_t i = 0; i < a.size() && i < c.size(); ++i) d2 = std::max(d2, static_cast<double>(std::fabs(a[i] - c[i])));
        std::printf("  block-size difference: 64 vs 512 %.2e, 64 vs 4096 %.2e\n", d1, d2);
        CHECK(d1 < 1e-4 && d2 < 1e-4, "output depends on the block size (%.2e / %.2e)", d1, d2);
    }

    // 4. level stays sensible across the Density and Size knobs (the autovol should keep it inside a narrow window)
    {
        double lo = 1e9, hi = -1e9;
        std::printf("  level by density x size (dB rms):\n        size  ");
        const float sz[4] = {0.05f, 0.3f, 0.6f, 0.95f};
        for (float s : sz) std::printf("%6.2f", s);
        std::printf("\n");
        for (float d : {0.05f, 0.3f, 0.55f, 0.8f, 1.0f}) {
            std::printf("  dens %.2f ", d);
            for (float s : sz) {
                Params p;
                p.density = d; p.size = s;
                Engine e; setup(e, p);
                const auto st = render(e, 3.0, 3.0, {60}, 512);
                const double db = dbOf(st.rms);
                std::printf("%6.1f", db);
                if (map::densityHz(d) * map::sizeMs(s) / 1000.0f >= 1.2f) { lo = std::min(lo, db); hi = std::max(hi, db); } // grains overlap
                CHECK(st.finite && st.peak <= 1.0001, "density %.2f size %.2f: bad output", d, s);
            }
            std::printf("\n");
        }
        std::printf("  where grains overlap (density x size >= 1.2): spread %.1f dB\n", hi - lo);
        CHECK(hi - lo < 3.0, "level varies by %.1f dB where grains overlap", hi - lo);
    }

    // 5. polyphony: 16 notes at full level never clip, and the engine keeps within its grain pool
    {
        Params p;
        p.voices = 16; p.density = 1.0f; p.size = 0.8f; p.level = 1.0f;
        Engine e; setup(e, p);
        const auto s = render(e, 3.0, 3.0, {40, 43, 47, 52, 55, 59, 60, 64, 67, 71, 72, 76, 79, 83, 84, 88}, 512);
        std::printf("  16 voices: rms %.1f dB peak %.1f dB\n", dbOf(s.rms), dbOf(s.peak));
        CHECK(s.finite && s.peak <= 1.0001, "16 voices: peak %.3f", s.peak);
    }

    // 6. every module and every option, one at a time
    auto module = [&](const char* name, Params p, int src = 1) {
        Engine e; setup(e, p, src);
        const auto s = render(e, 3.0, 2.5, {57, 64}, 512);
        std::printf("  %-26s rms %6.1f dB  peak %6.1f dB\n", name, dbOf(s.rms), dbOf(s.peak));
        CHECK(s.finite, "%s: not finite", name);
        CHECK(s.peak <= 1.0001, "%s: peak %.3f", name, s.peak);
        CHECK(dbOf(s.rms) > -60.0, "%s: silent", name);
        return s;
    };
    {
        Params p; p.morphOn = true; p.morph = 0.5f; module("morph 50%", p);
        p.morph = 1.0f; module("morph 100%", p);
        Params c; c.cohereOn = true; c.cohere = 1.0f; module("cohere 100%", c);
        c.cohere = 0.4f; module("cohere 40%", c);
        for (int pat = 0; pat < kNumWalk; ++pat) {
            Params w; w.walkOn = true; w.walkPattern = pat; w.walkRate = 3; module((std::string("walk ") + walkName(pat)).c_str(), w, 5);
        }
        for (int d = 0; d < kNumDivs; ++d) {
            Params r; r.repeatOn = true; r.repeatDiv = d; module((std::string("repeat ") + divName(d)).c_str(), r, 5);
        }
        for (int t = 0; t < kNumFilterTypes; ++t) {
            Params f; f.filterOn = true; f.filterType = t; f.filter = 0.35f; f.filterRes = 0.8f; module((std::string("filter ") + filterTypeName(t)).c_str(), f);
        }
        Params k; k.compOn = true; k.compAmt = 1.0f; module("comp 100%", k);
        Params m; m.magnet = 1.0f; module("magnet 100%", m, 5);
        for (int sc = 0; sc < kNumScales; ++sc) { Params q; q.scale = sc; q.jitter = 0.8f; q.scatter = 0.5f; module((std::string("scale ") + scaleName(sc)).c_str(), q); }
        for (int pm = 1; pm < kNumPlay; ++pm) { Params q; q.play = pm; q.speed = 0.7f; module(playName(pm), q); }
        Params fx; fx.delayMix = 1.0f; fx.delayFb = 0.9f; fx.reverbMix = 1.0f; fx.reverbDecay = 1.0f; fx.reverbSize = 1.0f; module("delay + reverb maxed", fx);
        Params ex; ex.density = 1.0f; ex.size = 1.0f; ex.scatter = 1.0f; ex.jitter = 1.0f; ex.reverse = 1.0f; ex.spread = 1.0f; module("everything extreme", ex);
        Params lo; lo.density = 0.0f; lo.size = 0.0f; module("sparsest", lo);
    }

    // 7. ramps and macros reach every target without breaking anything
    for (int t = 0; t < kNumTargets; ++t) {
        Params p;
        p.ramp[0].depth = 1.0f; p.ramp[0].rate = 0.7f; p.ramp[0].targets = 1u << t; p.ramp[0].shape = t % kNumRampShapes;
        p.ramp[1].depth = 1.0f; p.ramp[1].sync = 5; p.ramp[1].targets = 1u << t;
        p.macro[0].value = 1.0f; p.macro[0].depth = -1.0f; p.macro[0].targets = 1u << t;
        p.morphOn = true; p.filterOn = true;
        module((std::string("ramp/macro -> ") + targetName(t)).c_str(), p);
    }

    // 8. power switch, latch, trails
    {
        Params p; p.release = 0.2f;
        Engine e; setup(e, p);
        std::vector<float> l(512), r(512);
        e.noteOn(60, 1.0f);
        for (int i = 0; i < 60; ++i) e.process(l.data(), r.data(), 512);
        p.on = false; p.trails = false; e.setParams(p);
        double after = 0;
        for (int i = 0; i < 90; ++i) { e.process(l.data(), r.data(), 512); if (i > 60) for (float v : l) after = std::max(after, static_cast<double>(std::fabs(v))); }
        CHECK(after < 1e-4, "power off with trails off should fall silent (%.5f)", after);
        // latch: the note keeps sounding after note-off
        Params q; q.latch = true; q.release = 0.1f;
        Engine e2; setup(e2, q);
        e2.noteOn(60, 1.0f);
        for (int i = 0; i < 20; ++i) e2.process(l.data(), r.data(), 512);
        e2.noteOff(60);
        double rms = 0;
        for (int i = 0; i < 100; ++i) { e2.process(l.data(), r.data(), 512); if (i > 80) for (float v : l) rms += v * v; }
        CHECK(rms > 1e-6, "latch should hold the note after note-off");
        q.latch = false; e2.setParams(q);
        double rms2 = 1;
        for (int i = 0; i < 200; ++i) { e2.process(l.data(), r.data(), 512); if (i == 199) { rms2 = 0; for (float v : l) rms2 += v * v; } }
        CHECK(rms2 < 1e-8, "letting go of latch should release the held note");
    }

    // 9. odd samples: a very short one, a mono one, another sample rate
    {
        std::vector<float> tiny(100);
        for (size_t i = 0; i < tiny.size(); ++i) tiny[i] = std::sin(static_cast<float>(i) * 0.3f);
        auto s = sampleutil::make("tiny", 48000.0, tiny, {});
        Engine e; e.prepare(48000.0);
        e.setSamples(s.get(), s.get());
        Params p; p.reverse = 1.0f; p.scatter = 1.0f; p.size = 1.0f; p.speed = 0.9f; p.play = 1;
        e.setParams(p);
        const auto st = render(e, 2.0, 1.5, {60}, 512);
        CHECK(st.finite && st.peak <= 1.0001, "tiny sample: bad output");
        for (double sr : {22050.0, 44100.0, 96000.0}) {
            Params q;
            Engine e2; setup(e2, q, 1, 3, sr);
            const auto s2 = render(e2, 2.0, 1.5, {60}, 512, nullptr, sr);
            std::printf("  sample rate %6.0f: rms %.1f dB\n", sr, dbOf(s2.rms));
            CHECK(s2.finite && s2.peak <= 1.0001 && dbOf(s2.rms) > -45.0, "sample rate %.0f: bad output", sr);
        }
    }

    // 10. CPU: the heaviest settings must stay far below real time
    {
        Params p;
        p.voices = 16; p.density = 1.0f; p.size = 0.9f; p.scatter = 0.5f; p.jitter = 0.5f; p.filterOn = true; p.compOn = true; p.morphOn = true;
        p.cohereOn = false; p.delayMix = 0.4f; p.reverbMix = 0.4f; p.walkOn = true; p.repeatOn = true;
        Engine e; setup(e, p);
        const auto t0 = std::chrono::steady_clock::now();
        render(e, 6.0, 6.0, {36, 40, 43, 47, 50, 53, 57, 60, 64, 67, 71, 74, 77, 81, 84, 88}, 512);
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("  worst case: %.2f s of work for 6 s of audio  (%.0f %% of one core)\n", secs, secs / 6.0 * 100.0);
        CHECK(secs / 6.0 < 0.6, "too heavy: %.0f %% of one core", secs / 6.0 * 100.0);
    }

    std::printf("%d checks, %d failed\n", g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
