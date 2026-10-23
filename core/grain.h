// MOTE - the granular engine. Plain C++ (no JUCE), so it can be tested and rendered offline.
//
// A sample is cut into tiny windowed pieces ("grains"). Each voice (one per key you hold) fires grains from a chosen
// point of the sample, over and over; where each one starts, how long it lasts, how high it plays and where it sits
// in the stereo field are chosen a little at random, which turns a sample into a cloud.
//
//   POSITION / SCATTER      where in the sample the grains start, and how far they stray from that point
//   DENSITY / SIZE          how many grains per second, and how long each lasts
//   PITCH / JITTER          transposition, and how far each grain's pitch strays (optionally locked to a scale)
//   SPREAD                  how wide the grains are scattered across the stereo field
//   PLAY (hold, travel, bounce) + SPEED   whether the starting point stays put or moves through the sample
//   MAGNET                  pulls grains to the nearest hit (transient) of the sample
//   MORPH / COHERE / WALK / REPEAT / FILTER / COMP   the six modules (see Params)
//
// Everything runs on a 32-sample grid; grains start on the exact sample.
#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <vector>

#include "dsp.h"
#include "sample.h"
#include "tempo.h"

namespace mote {

// ---------------------------------------------------------------------------------------------- names and maps
constexpr int kNumScales = 11;
inline const char* scaleName(int i) {
    static const char* n[kNumScales] = {"Chromatic", "Major", "Minor", "Dorian", "Mixolydian", "Pentatonic", "Minor pent.", "Blues",
                                        "Whole tone", "Harmonic minor", "Lydian"};
    return n[std::min(kNumScales - 1, std::max(0, i))];
}
inline int scaleMask(int i) { // bit k = the note k semitones above the root belongs to the scale
    static const int m[kNumScales] = {0xfff,
                                      0b101010110101,  // major: 0 2 4 5 7 9 11
                                      0b010110101101,  // minor: 0 2 3 5 7 8 10
                                      0b011010101101,  // dorian: 0 2 3 5 7 9 10
                                      0b011010110101,  // mixolydian: 0 2 4 5 7 9 10
                                      0b001010010101,  // pentatonic: 0 2 4 7 9
                                      0b010010101001,  // minor pentatonic: 0 3 5 7 10
                                      0b010011101001,  // blues: 0 3 5 6 7 10
                                      0b010101010101,  // whole tone: 0 2 4 6 8 10
                                      0b100110101101,  // harmonic minor: 0 2 3 5 7 8 11
                                      0b101010101101 | 0};
    // lydian: 0 2 4 6 7 9 11
    if (i == 10) return 0b101011010101;
    return m[std::min(kNumScales - 1, std::max(0, i))];
}
inline const char* noteName(int i) {
    static const char* n[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return n[((i % 12) + 12) % 12];
}

constexpr int kNumPlay = 3;
inline const char* playName(int i) {
    static const char* n[kNumPlay] = {"HOLD", "TRAVEL", "BOUNCE"};
    return n[std::min(kNumPlay - 1, std::max(0, i))];
}
constexpr int kNumWalk = 7;
inline const char* walkName(int i) {
    static const char* n[kNumWalk] = {"Forward", "Backward", "Ping-pong", "Random", "Drunk", "Skip", "Chain"};
    return n[std::min(kNumWalk - 1, std::max(0, i))];
}
constexpr int kNumFilterTypes = 3;
inline const char* filterTypeName(int i) {
    static const char* n[kNumFilterTypes] = {"Low-pass", "High-pass", "Band-pass"};
    return n[std::min(kNumFilterTypes - 1, std::max(0, i))];
}
constexpr int kNumRampShapes = 4;
inline const char* rampShapeName(int i) {
    static const char* n[kNumRampShapes] = {"Sine", "Triangle", "Saw", "Random"};
    return n[std::min(kNumRampShapes - 1, std::max(0, i))];
}

// the nine things a ramp or a macro can move
enum Target { kTPosition, kTScatter, kTDensity, kTSize, kTPitch, kTJitter, kTSpread, kTFilter, kTMorph, kNumTargets };
inline const char* targetName(int t) {
    static const char* n[kNumTargets] = {"Position", "Scatter", "Density", "Size", "Pitch", "Jitter", "Spread", "Filter", "Morph"};
    return n[std::min(kNumTargets - 1, std::max(0, t))];
}

namespace map { // knob (0..1) -> real units
inline float densityHz(float x) { return 0.5f * std::pow(400.0f, dsp::clamp01(x)); }              // 0.5 .. 200 grains/s
inline float sizeMs(float x) { return 8.0f * std::pow(75.0f, dsp::clamp01(x)); }                   // 8 .. 600 ms
inline float jitterSemis(float x) { return 12.0f * dsp::clamp01(x); }                              // +- 0 .. 12 semitones
inline float scatterFrac(float x) { x = dsp::clamp01(x); return 0.5f * x * x; }                    // +- 0 .. 50 % of the sample
inline float cutoffHz(float x) { return 40.0f * std::pow(400.0f, dsp::clamp01(x)); }               // 40 Hz .. 16 kHz
inline float speedMul(float x) { return std::pow(2.0f, (dsp::clamp01(x) - 0.5f) * 6.0f); }         // 0.125x .. 8x
inline float attackSec(float x) { return 0.001f * std::pow(5000.0f, dsp::clamp01(x)); }           // 1 ms .. 5 s
inline float decaySec(float x) { return 0.005f * std::pow(1000.0f, dsp::clamp01(x)); }            // 5 ms .. 5 s
inline float releaseSec(float x) { return 0.005f * std::pow(1600.0f, dsp::clamp01(x)); }          // 5 ms .. 8 s
inline float levelDb(float x) { return -40.0f + 46.0f * dsp::clamp01(x); }                         // -40 .. +6 dB
inline float rampHz(float x) { return 0.03f * std::pow(500.0f, dsp::clamp01(x)); }                 // 0.03 .. 15 Hz
} // namespace map

// ---------------------------------------------------------------------------------------------- parameters
struct RampParams {
    float rate = 0.3f, depth = 0.0f;
    int shape = 0, sync = 0;
    uint32_t targets = 0; // bit per Target
};
struct MacroParams {
    float value = 0.0f, depth = 0.5f; // depth -1..1
    uint32_t targets = 0;
};

struct Params {
    // the knobs, 0..1 (before ramps and macros are added)
    float position = 0.3f, scatter = 0.15f, density = 0.55f, size = 0.5f, jitter = 0.0f, spread = 0.5f;
    float pitchSemis = 0.0f;            // -24 .. +24
    float speed = 0.5f, magnet = 0.0f;
    int root = 0, scale = 0, sampleKey = 0; // the key to lock to, and the note the sample itself sounds like (0 = C)
    float attack = 0.15f, decay = 0.4f, sustain = 0.85f, release = 0.45f;
    int voices = 8;
    float level = 0.87f, delayMix = 0.0f, reverbMix = 0.0f;
    int play = 0; // 0 hold, 1 travel, 2 bounce
    // the six modules: lever + knob
    bool morphOn = false, cohereOn = false, walkOn = false, repeatOn = false, filterOn = false, compOn = false;
    float morph = 0.5f, cohere = 0.7f, filter = 0.6f, compAmt = 0.5f;
    int walkPattern = 0, repeatDiv = 1;
    // under the hood
    float grainShape = 0.35f;    // 0 soft .. 1 hard edges
    float reverse = 0.0f;        // chance a grain plays backwards
    float tuneCents = 0.0f;
    float glide = 0.0f;          // not used by the grains: reserved
    int filterType = 0;
    float filterRes = 0.3f, filterSpread = 0.25f, filterKey = 0.0f;
    int walkRate = 3, walkSteps = 8;
    float walkGate = 0.85f;
    float repeatDecay = 1.0f;
    int delayDiv = 2;
    float delayFb = 0.4f, delayTone = 0.6f;
    float reverbSize = 0.5f, reverbDecay = 0.55f, reverbDamp = 0.5f;
    bool on = true, latch = false, trails = true;
    float outputGainDb = 0.0f;
    RampParams ramp[2];
    MacroParams macro[2];
};

// the slices WALK steps through: the sample's transients, or 16 equal parts if it has too few
inline std::vector<float> sliceList(const Sample* s) {
    std::vector<float> out;
    if (s != nullptr && s->size() > 0 && s->onsets.size() >= 4) {
        for (int o : s->onsets) out.push_back(static_cast<float>(o) / static_cast<float>(s->size()));
    } else {
        for (int i = 0; i < 16; ++i) out.push_back(static_cast<float>(i) / 16.0f);
    }
    return out;
}

// ---------------------------------------------------------------------------------------------- the engine
class Engine {
public:
    static constexpr int kMaxVoices = 16, kMaxGrains = 320, kSub = 32, kVizGrains = 96;

    struct Viz { // one recently fired grain, for the window
        std::atomic<float> pos{0.0f}, pan{0.0f}, semis{0.0f}, seconds{0.1f}, amp{0.0f};
        std::atomic<double> born{-10.0};
    };

    void prepare(double sr) {
        sr_ = sr;
        grains_.assign(kMaxGrains, Grain());
        for (int w = 0; w < kWin; ++w) ramp_[w] = 0.5f - 0.5f * std::cos(3.14159265f * static_cast<float>(w) / (kWin - 1));
        delayL_.init(static_cast<int>(sr * 2.6));
        delayR_.init(static_cast<int>(sr * 2.6));
        reverb_.prepare(sr);
        repeatRing_[0].assign(static_cast<size_t>(sr * 8.0), 0.0f);
        repeatRing_[1].assign(static_cast<size_t>(sr * 8.0), 0.0f);
        loopBuf_[0].assign(static_cast<size_t>(sr * 8.0), 0.0f);
        loopBuf_[1].assign(static_cast<size_t>(sr * 8.0), 0.0f);
        reset();
    }
    void reset() {
        for (auto& g : grains_) g.active = false;
        for (auto& v : voices_) { v = Voice(); v.rng = 0x9e3779b9u * static_cast<uint32_t>(&v - voices_.data() + 1); }
        delayL_.clear(); delayR_.clear(); reverb_.clear();
        std::fill(repeatRing_[0].begin(), repeatRing_[0].end(), 0.0f);
        std::fill(repeatRing_[1].begin(), repeatRing_[1].end(), 0.0f);
        repeatW_ = 0; repeatState_ = 0; repeatMix_ = 0.0f; repeatRearm_ = false;
        powerGain_ = P_.on ? 1.0f : 0.0f;
        curLevel_ = -1.0f;
        beat_ = 0.0;
        for (auto& f : delayLp_) f = 0.0f;
        limEnv_ = 0.0f;
    }

    // the samples to play (A, and the one MORPH blends in). Called from the audio thread; the caller keeps them alive.
    void setSamples(const Sample* a, const Sample* b) {
        if (a != sa_) {
            sa_ = a;
            slices_ = sliceList(sa_);
            onsets01_.clear();
            if (sa_ != nullptr && sa_->size() > 0)
                for (int o : sa_->onsets) onsets01_.push_back(static_cast<float>(o) / static_cast<float>(sa_->size()));
        }
        sb_ = b != nullptr ? b : a;
    }
    void setParams(const Params& p) {
        const bool wasOn = P_.on, wasLatch = P_.latch;
        P_ = p;
        if (wasOn && !P_.on) for (auto& v : voices_) if (v.stage != Voice::Idle) { v.stage = Voice::Rel; v.relStep = v.env / static_cast<float>(sr_ * 0.02); }
        if (wasLatch && !P_.latch) releaseLatched();
    }
    void setTransport(const Transport& t) { T_ = t; }

    void noteOn(int note, float vel) {
        if (!P_.on) return;
        const int limit = std::max(1, std::min(kMaxVoices, P_.voices));
        int pick = -1;
        for (int i = 0; i < limit; ++i) if (voices_[static_cast<size_t>(i)].stage == Voice::Idle) { pick = i; break; }
        if (pick < 0) { // steal: the quietest one, preferring voices already fading
            float best = 1e9f;
            for (int i = 0; i < limit; ++i) {
                const auto& v = voices_[static_cast<size_t>(i)];
                const float score = v.env + (v.stage == Voice::Rel ? 0.0f : 1.0f) + v.age * 1e-6f * -1.0f;
                if (score < best) { best = score; pick = i; }
            }
        }
        auto& v = voices_[static_cast<size_t>(pick)];
        const auto keepRng = v.rng;
        v = Voice();
        v.rng = keepRng;
        v.gen = ++genCounter_;
        v.stage = Voice::Att;
        v.note = note;
        v.vel = vel;
        v.held = true;
        v.timer = 0.0;
        v.travel = 0.0;
        v.startBeat = beat_;
    }
    void noteOff(int note) {
        for (auto& v : voices_)
            if (v.stage != Voice::Idle && v.stage != Voice::Rel && v.held && v.note == note) {
                v.held = false;
                if (P_.latch) v.pendingRelease = true;
                else beginRelease(v);
            }
    }
    void allNotesOff() { for (auto& v : voices_) if (v.stage != Voice::Idle) { v.held = false; beginRelease(v); } }
    void setPitchBend(float semis) { bendTarget_ = semis; }

    // renders n samples (any n)
    void process(float* outL, float* outR, int n) {
        if (T_.hostValid && T_.playing) beat_ = T_.ppq;
        int done = 0;
        while (done < n) {
            const int m = std::min(kSub, n - done);
            renderSub(outL + done, outR + done, m);
            done += m;
        }
    }

    // ---- what the window shows ----
    int voiceCount() const { return kMaxVoices; }
    float voicePos(int v) const { return voicePos_[static_cast<size_t>(v)].load(std::memory_order_relaxed); }
    float voiceLevel(int v) const { return voiceLvl_[static_cast<size_t>(v)].load(std::memory_order_relaxed); }
    const Viz& viz(int i) const { return viz_[static_cast<size_t>(i)]; }
    double now() const { return clock_.load(std::memory_order_relaxed); }
    int walkStep() const { return walkStep_.load(std::memory_order_relaxed); }
    float walkSlicePos() const { return walkSlice_.load(std::memory_order_relaxed); }
    bool repeating() const { return repeatState_ == 2; }
    // for the window when the host is not calling process() (nothing playing, track idle): show the knobs as they are
    void showIdle(const Params& p) {
        posShown_.store(dsp::clamp01(p.position), std::memory_order_relaxed);
        scatterShown_.store(map::scatterFrac(p.scatter), std::memory_order_relaxed);
    }
    float posShown() const { return posShown_.load(std::memory_order_relaxed); }
    float scatterShown() const { return scatterShown_.load(std::memory_order_relaxed); }
    float filterShown() const { return filterShown_.load(std::memory_order_relaxed); }
    float morphShown() const { return morphShown_.load(std::memory_order_relaxed); }
    float rampShown(int i) const { return rampShown_[static_cast<size_t>(i)].load(std::memory_order_relaxed); }
    double beat() const { return beat_; }
    int activeGrains() const { return activeGrains_.load(std::memory_order_relaxed); }
    float outputPeak() { return peakOut_.exchange(0.0f, std::memory_order_relaxed); }

private:
    static constexpr int kWin = 129;

    struct Grain {
        bool active = false;
        const Sample* a = nullptr;
        const Sample* b = nullptr;
        double posA = 0.0, posB = 0.0, incA = 1.0, incB = 1.0;
        int len = 1, age = 0, delay = 0, voice = 0;
        uint32_t gen = 0;
        float gl = 1.0f, gr = 1.0f, mix = 0.0f, edge = 0.4f;
        int ftype = -1; // -1 no filter
        dsp::Svf fl, fr;
        float squash = 0.0f, squashInv = 1.0f;
    };
    struct Voice {
        enum Stage { Idle, Att, Dec, Sus, Rel } stage = Idle;
        int note = 60;
        float vel = 1.0f, env = 0.0f, relStep = 0.0f;
        bool held = false, pendingRelease = false;
        double timer = 0.0, travel = 0.0, startBeat = 0.0;
        float age = 0.0f;
        uint32_t gen = 0, rng = 1;
        float pos = 0.0f;
        float rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return static_cast<float>(rng & 0xffffff) / 16777215.0f; }
    };

    // ---- small helpers ----
    static inline float tanhFast(float x) {
        if (x > 3.0f) return 1.0f;
        if (x < -3.0f) return -1.0f;
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }
    static inline double wrapPos(double x, int n) {
        x = std::fmod(x, static_cast<double>(n));
        return x < 0 ? x + n : x;
    }
    static inline uint32_t hash32(uint32_t x) {
        x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
        return x;
    }
    static inline float hash01(uint32_t k, uint32_t salt) { return static_cast<float>(hash32(k * 2654435761u + salt) & 0xffffff) / 16777215.0f; }

    void beginRelease(Voice& v) {
        v.stage = Voice::Rel;
        const float rel = std::max(0.004f, map::releaseSec(P_.release));
        v.relStep = std::max(v.env, 0.001f) / static_cast<float>(rel * sr_);
    }
    void releaseLatched() {
        for (auto& v : voices_) if (v.stage != Voice::Idle && v.pendingRelease) { v.pendingRelease = false; if (!v.held) beginRelease(v); }
    }

    float lfoValue(int shape, double phase, int idx) const {
        const double fr = phase - std::floor(phase);
        switch (shape) {
            case 0: return static_cast<float>(std::sin(2.0 * kPi * phase));
            case 1: return static_cast<float>(1.0 - 4.0 * std::fabs(fr - 0.5));
            case 2: return static_cast<float>(2.0 * fr - 1.0);
            default: { // smooth random: a new value each cycle, glided into
                const uint32_t k = static_cast<uint32_t>(static_cast<int64_t>(std::floor(phase)) & 0x7fffffff);
                const float a = hash01(k, 17u + static_cast<uint32_t>(idx) * 101u) * 2.0f - 1.0f;
                const float b = hash01(k + 1u, 17u + static_cast<uint32_t>(idx) * 101u) * 2.0f - 1.0f;
                const float s = static_cast<float>(fr * fr * (3.0 - 2.0 * fr));
                return a + (b - a) * s;
            }
        }
    }

    // one value of a target after the ramps and macros have been added (0..1; Pitch is in semitones)
    float modded(int t, float base, const float* off) const {
        return dsp::clamp01(base + off[t]);
    }

    inline float readHermite(const std::vector<float>& d, double idx) const {
        const int n = static_cast<int>(d.size());
        int i1 = static_cast<int>(idx); // idx is never negative here (positions are wrapped)
        const float f = static_cast<float>(idx - i1);
        auto wrap = [n](int i) { i %= n; return i < 0 ? i + n : i; };
        float y0, y1, y2, y3;
        if (i1 >= 1 && i1 < n - 2) {
            const float* p = d.data() + i1;
            y0 = p[-1]; y1 = p[0]; y2 = p[1]; y3 = p[2];
        } else {
            y0 = d[static_cast<size_t>(wrap(i1 - 1))]; y1 = d[static_cast<size_t>(wrap(i1))];
            y2 = d[static_cast<size_t>(wrap(i1 + 1))]; y3 = d[static_cast<size_t>(wrap(i1 + 2))];
        }
        const float c0 = y1, c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }

    float quantiseSemis(float semis) const {
        if (P_.scale <= 0) return semis;
        const int mask = scaleMask(P_.scale);
        const float absNote = static_cast<float>(P_.sampleKey) + semis;
        const int base = static_cast<int>(std::floor(absNote + 0.5f));
        int best = base;
        float bestD = 1e9f;
        for (int k = -6; k <= 6; ++k) {
            const int n = base + k;
            const int deg = (((n - P_.root) % 12) + 12) % 12;
            if (!(mask & (1 << deg))) continue;
            const float d = std::fabs(static_cast<float>(n) - absNote);
            if (d < bestD) { bestD = d; best = n; }
        }
        return static_cast<float>(best - P_.sampleKey);
    }

    int nearestSlot(int stepIdx) {
        const int n = std::max<int>(1, static_cast<int>(slices_.size()));
        const int len = std::max(2, std::min(16, P_.walkSteps));
        const int s = ((stepIdx % len) + len) % len;
        switch (P_.walkPattern) {
            case 0: return s % n;
            case 1: return (n - 1 - (s % n) + n) % n;
            case 2: { const int period = std::max(1, 2 * n - 2); const int c = s % period; return c < n ? c : period - c; }
            case 3: return static_cast<int>(hash32(static_cast<uint32_t>(stepIdx) * 7919u + 13u) % static_cast<uint32_t>(n));
            case 4: return drunk_ % n;
            case 5: return (s * 2 + (s * 2 >= n ? 1 : 0)) % n;
            default: return (s % 2 == 0) ? 0 : 1 + (s / 2) % std::max(1, n - 1);
        }
    }

    // ---- fire one grain ----
    void spawn(Voice& v, int vi, int offset, const float* off, float rate, double dtSamples) {
        if (sa_ == nullptr || sa_->size() < 16) return;
        // find a slot
        int slot = -1;
        float oldest = -1.0f;
        for (int i = 0; i < kMaxGrains; ++i) {
            auto& g = grains_[static_cast<size_t>(i)];
            if (!g.active) { slot = i; break; }
            const float prog = static_cast<float>(g.age) / static_cast<float>(std::max(1, g.len));
            if (prog > oldest) { oldest = prog; slot = i; }
        }
        auto& g = grains_[static_cast<size_t>(slot)];
        const float a = P_.cohereOn ? dsp::clamp01(P_.cohere) : 0.0f;

        // where
        float base;
        if (P_.walkOn) base = walkSlice_.load(std::memory_order_relaxed);
        else base = v.pos;
        const float scat = map::scatterFrac(modded(kTScatter, P_.scatter, off)) * (1.0f - 0.9f * a);
        float pos = base + (v.rnd() * 2.0f - 1.0f) * scat;
        if (P_.magnet > 0.001f && !onsets01_.empty()) {
            auto it = std::lower_bound(onsets01_.begin(), onsets01_.end(), pos);
            float best = onsets01_.front();
            float bd = 1e9f;
            if (it != onsets01_.end()) { bd = std::fabs(*it - pos); best = *it; }
            if (it != onsets01_.begin()) { const float p = *(it - 1); if (std::fabs(p - pos) < bd) { bd = std::fabs(p - pos); best = p; } }
            const float pull = P_.magnet * P_.magnet * 0.5f + 0.5f * P_.magnet;
            pos += (best - pos) * pull;
        }
        pos -= std::floor(pos);

        // how high
        const float jit = map::jitterSemis(modded(kTJitter, P_.jitter, off));
        float semisBase = static_cast<float>(v.note - 60) + P_.pitchSemis + off[kTPitch] * 24.0f + P_.tuneCents * 0.01f + bend_;
        float semisFree = semisBase * (1.0f - a);
        float semis = semisFree + (v.rnd() * 2.0f - 1.0f) * jit * (1.0f - 0.8f * a);
        semis = quantiseSemis(semis);
        const double ratio = std::pow(2.0, static_cast<double>(semis) / 12.0);

        // how long
        const float userLenS = map::sizeMs(modded(kTSize, P_.size, off)) * 0.001f;
        const float lenS = a > 0.0f ? std::max(userLenS * (1.0f - a) + 2.2f / std::max(20.0f, rate) * a, userLenS * 0.5f) : userLenS;
        g.len = std::max(16, static_cast<int>(lenS * static_cast<float>(sr_)));

        // pan
        const float spread = modded(kTSpread, P_.spread, off);
        const float pan = (v.rnd() * 2.0f - 1.0f) * spread;
        const float ang = (pan + 1.0f) * 0.7853982f;
        g.gl = std::cos(ang) * 1.41421356f;
        g.gr = std::sin(ang) * 1.41421356f;

        // loudness: grains overlap, so fewer / shorter ones need no turning down, many long ones do
        const float overlap = rate * lenS;
        const float norm = 0.8f / std::sqrt(std::max(1.0f, overlap * 0.6f));
        const float amp = norm * (0.35f + 0.65f * v.vel);
        g.gl *= amp;
        g.gr *= amp;

        g.a = sa_;
        g.incA = ratio * sa_->sr / sr_;
        g.posA = static_cast<double>(pos) * sa_->size();
        const float mAmt = P_.morphOn ? modded(kTMorph, P_.morph, off) : 0.0f;
        g.mix = mAmt;
        g.b = nullptr;
        if (mAmt > 0.001f && sb_ != nullptr && sb_->size() > 16) {
            g.b = sb_;
            g.incB = ratio * sb_->sr / sr_;
            g.posB = static_cast<double>(pos) * sb_->size();
        }
        if (v.rnd() < P_.reverse) {
            g.incA = -g.incA;
            g.incB = -g.incB;
            g.posA += (static_cast<double>(g.len) * -g.incA);
            g.posB += (static_cast<double>(g.len) * -g.incB);
        }
        g.edge = 0.5f - 0.46f * P_.grainShape;
        // filter
        g.ftype = -1;
        if (P_.filterOn) {
            const float cut = map::cutoffHz(modded(kTFilter, P_.filter, off));
            const float spreadOct = (v.rnd() * 2.0f - 1.0f) * P_.filterSpread * 2.5f;
            const float key = P_.filterKey * (semisBase) / 12.0f;
            const double fc = static_cast<double>(cut) * std::pow(2.0, static_cast<double>(spreadOct + key));
            const double q = 0.55 + 7.0 * P_.filterRes * P_.filterRes;
            g.ftype = P_.filterType;
            g.fl.set(sr_, fc, q);
            g.fr = g.fl;
            g.fl.reset();
            g.fr.reset();
        }
        g.squash = P_.compOn ? 0.05f + P_.compAmt * 9.0f : 0.0f;
        g.squashInv = g.squash > 0.0f ? 0.9f / tanhFast(g.squash) / std::sqrt(1.0f + g.squash) : 1.0f; // (the last part keeps the level near where it was)
        g.age = 0;
        g.delay = offset;
        g.voice = vi;
        g.gen = v.gen;
        g.active = true;
        (void)dtSamples;

        // tell the window
        const int vz = vizHead_;
        vizHead_ = (vizHead_ + 1) % kVizGrains;
        auto& z = viz_[static_cast<size_t>(vz)];
        z.pos.store(pos, std::memory_order_relaxed);
        z.pan.store(pan, std::memory_order_relaxed);
        z.semis.store(semis, std::memory_order_relaxed);
        z.seconds.store(lenS, std::memory_order_relaxed);
        z.amp.store(std::min(1.0f, amp * 1.2f), std::memory_order_relaxed);
        z.born.store(clockTime_ + static_cast<double>(offset) / sr_, std::memory_order_relaxed);
    }

    // ---- one 32-sample slice ----
    void renderSub(float* outL, float* outR, int m) {
        const double bps = T_.bpm / 60.0 / sr_; // beats per sample
        const double bpb = static_cast<double>(std::max(1, T_.beatsPerBar));

        // ---- ramps and macros -> an offset for each target (normalised units; pitch in "knob widths") ----
        float off[kNumTargets] = {};
        for (int r = 0; r < 2; ++r) {
            const auto& rp = P_.ramp[r];
            double ph;
            if (rp.sync > 0) {
                const double len = syncBeats(rp.sync, bpb);
                ph = beat_ / std::max(1e-6, len);
            } else {
                freePhase_[r] += static_cast<double>(map::rampHz(rp.rate)) * m / sr_;
                ph = freePhase_[r];
            }
            const float v = lfoValue(rp.shape, ph, r);
            rampShown_[static_cast<size_t>(r)].store(v * rp.depth, std::memory_order_relaxed);
            if (rp.depth <= 0.0f) continue;
            for (int t = 0; t < kNumTargets; ++t)
                if (rp.targets & (1u << t)) off[t] += rp.depth * v * 0.5f;
        }
        for (int k = 0; k < 2; ++k) {
            const auto& mp = P_.macro[k];
            if (mp.depth == 0.0f || mp.value <= 0.0f) continue;
            for (int t = 0; t < kNumTargets; ++t)
                if (mp.targets & (1u << t)) off[t] += mp.depth * mp.value * 0.5f;
        }
        const float posN = modded(kTPosition, P_.position, off);
        const float density = map::densityHz(modded(kTDensity, P_.density, off));
        posShown_.store(posN, std::memory_order_relaxed);
        scatterShown_.store(map::scatterFrac(modded(kTScatter, P_.scatter, off)), std::memory_order_relaxed);
        filterShown_.store(modded(kTFilter, P_.filter, off), std::memory_order_relaxed);
        morphShown_.store(modded(kTMorph, P_.morph, off), std::memory_order_relaxed);

        bend_ += (bendTarget_ - bend_) * 0.2f;

        // ---- WALK: which slice, and whether this part of the step is open ----
        bool gateOpen = true;
        if (P_.walkOn && !slices_.empty()) {
            const double stepBeats = std::max(1e-6, divBeats(P_.walkRate, bpb));
            const double sp = beat_ / stepBeats;
            const int stepIdx = static_cast<int>(std::floor(sp));
            if (stepIdx != lastWalkStep_) {
                if (P_.walkPattern == 4) { // drunk: wander one slice at a time
                    const float r = hash01(static_cast<uint32_t>(stepIdx), 31u);
                    const int n = static_cast<int>(slices_.size());
                    drunk_ = (drunk_ + (r < 0.4f ? -1 : (r < 0.8f ? 1 : 0)) + n) % n;
                }
                lastWalkStep_ = stepIdx;
                walkStep_.store(((stepIdx % std::max(2, std::min(16, P_.walkSteps))) + std::max(2, std::min(16, P_.walkSteps))) % std::max(2, std::min(16, P_.walkSteps)), std::memory_order_relaxed);
                walkSlice_.store(slices_[static_cast<size_t>(nearestSlot(stepIdx) % static_cast<int>(slices_.size()))], std::memory_order_relaxed);
            }
            gateOpen = (sp - std::floor(sp)) < std::max(0.05f, P_.walkGate);
        }

        // ---- voices fire grains ----
        const float secsA = sa_ != nullptr ? static_cast<float>(sa_->seconds()) : 1.0f;
        const float speed = map::speedMul(P_.speed);
        std::array<std::array<float, kSub>, kMaxVoices>& busL = busL_;
        std::array<std::array<float, kSub>, kMaxVoices>& busR = busR_;
        bool anyVoice = false;
        for (int vi = 0; vi < kMaxVoices; ++vi) {
            auto& v = voices_[static_cast<size_t>(vi)];
            if (v.stage == Voice::Idle) { voicePos_[static_cast<size_t>(vi)].store(-1.0f, std::memory_order_relaxed); continue; }
            anyVoice = true;
            // the start point: fixed, or moving through the sample
            if (P_.play == 0) v.pos = posN;
            else {
                v.travel += static_cast<double>(speed) * m / (sr_ * std::max(0.05f, secsA));
                const double u = static_cast<double>(posN) + v.travel;
                if (P_.play == 1) v.pos = static_cast<float>(u - std::floor(u));
                else { const double w = std::fmod(u, 2.0); v.pos = static_cast<float>(w < 1.0 ? w : 2.0 - w); }
            }
            voicePos_[static_cast<size_t>(vi)].store(P_.walkOn ? walkSlice_.load(std::memory_order_relaxed) : v.pos, std::memory_order_relaxed);

            // pitch-synchronous grains ("cohere"): the grain rate follows the note
            const float a = P_.cohereOn ? dsp::clamp01(P_.cohere) : 0.0f;
            float rate = density;
            if (a > 0.0f) {
                const float semisBase = static_cast<float>(v.note - 60) + P_.pitchSemis + off[kTPitch] * 24.0f + P_.tuneCents * 0.01f + bend_;
                const float fNote = 261.6256f * std::pow(2.0f, semisBase / 12.0f);
                rate = std::exp(std::log(density) * (1.0f - a) + std::log(std::max(20.0f, fNote)) * a);
            }
            const double interval = sr_ / static_cast<double>(rate);
            while (v.timer < m) {
                const int k = std::max(0, static_cast<int>(v.timer));
                if (gateOpen) spawn(v, vi, k, off, rate, interval);
                const float randomness = 1.0f + (v.rnd() * 2.0f - 1.0f) * 0.5f * (1.0f - a);
                v.timer += interval * static_cast<double>(randomness);
            }
            v.timer -= m;
            std::fill(busL[static_cast<size_t>(vi)].begin(), busL[static_cast<size_t>(vi)].begin() + m, 0.0f);
            std::fill(busR[static_cast<size_t>(vi)].begin(), busR[static_cast<size_t>(vi)].begin() + m, 0.0f);
        }

        // ---- the grains ----
        int active = 0;
        for (auto& g : grains_) {
            if (!g.active) continue;
            auto& v = voices_[static_cast<size_t>(g.voice)];
            if (v.gen != g.gen || v.stage == Voice::Idle) { g.active = false; continue; }
            ++active;
            float* bl = busL[static_cast<size_t>(g.voice)].data();
            float* br = busR[static_cast<size_t>(g.voice)].data();
            const int nA = g.a->size();
            const bool useB = g.b != nullptr;
            const int nB = useB ? g.b->size() : 1;
            const float invLen = 1.0f / static_cast<float>(g.len);
            const float edge = g.edge, invEdge = 1.0f / edge;
            const float mixB = g.mix, mixA = 1.0f - g.mix;
            // a mono sample needs one channel read and one filter, not two
            const bool mono = !g.a->stereo && (!useB || !g.b->stereo);
            int i = g.delay;
            g.delay = 0;
            for (; i < m && g.age < g.len; ++i, ++g.age) {
                const float t = static_cast<float>(g.age) * invLen;
                const float d = std::min(t, 1.0f - t) * invEdge;
                float w = 1.0f;
                if (d < 1.0f) w = ramp_[static_cast<size_t>(d * (kWin - 1))];
                double ia = g.posA;
                float sl = readHermite(g.a->l, ia);
                float sr = mono ? sl : readHermite(g.a->r, ia);
                g.posA += g.incA;
                if (g.posA >= nA || g.posA < 0) g.posA = wrapPos(g.posA, nA);
                if (useB) {
                    const double ib = g.posB;
                    const float bl2 = readHermite(g.b->l, ib);
                    const float br2 = mono ? bl2 : readHermite(g.b->r, ib);
                    g.posB += g.incB;
                    if (g.posB >= nB || g.posB < 0) g.posB = wrapPos(g.posB, nB);
                    sl = sl * mixA + bl2 * mixB;
                    sr = mono ? sl : sr * mixA + br2 * mixB;
                }
                if (g.ftype >= 0) {
                    float lp, bp, hp;
                    g.fl.process(sl, lp, bp, hp);
                    sl = g.ftype == 0 ? lp : (g.ftype == 1 ? hp : bp);
                    if (mono) sr = sl;
                    else {
                        g.fr.process(sr, lp, bp, hp);
                        sr = g.ftype == 0 ? lp : (g.ftype == 1 ? hp : bp);
                    }
                }
                if (g.squash > 0.0f) {
                    const float dd = g.squash;
                    sl = tanhFast(sl * dd) * g.squashInv;
                    sr = mono ? sl : tanhFast(sr * dd) * g.squashInv;
                }
                bl[i] += sl * w * g.gl;
                br[i] += sr * w * g.gr;
            }
            if (g.age >= g.len) g.active = false;
        }
        activeGrains_.store(active, std::memory_order_relaxed);

        // ---- each voice's envelope, then the sum ----
        float dryL[kSub], dryR[kSub];
        std::fill(dryL, dryL + m, 0.0f);
        std::fill(dryR, dryR + m, 0.0f);
        const float attS = std::max(1.0f, map::attackSec(P_.attack) * static_cast<float>(sr_));
        const float decS = std::max(1.0f, map::decaySec(P_.decay) * static_cast<float>(sr_));
        const float sus = dsp::clamp01(P_.sustain);
        for (int vi = 0; vi < kMaxVoices; ++vi) {
            auto& v = voices_[static_cast<size_t>(vi)];
            if (v.stage == Voice::Idle) { voiceLvl_[static_cast<size_t>(vi)].store(0.0f, std::memory_order_relaxed); continue; }
            const float* bl = busL[static_cast<size_t>(vi)].data();
            const float* br = busR[static_cast<size_t>(vi)].data();
            for (int i = 0; i < m; ++i) {
                switch (v.stage) {
                    case Voice::Att: v.env += 1.0f / attS; if (v.env >= 1.0f) { v.env = 1.0f; v.stage = Voice::Dec; } break;
                    case Voice::Dec: v.env -= (1.0f - sus) / decS; if (v.env <= sus) { v.env = sus; v.stage = Voice::Sus; } break;
                    case Voice::Sus: v.env = sus; break;
                    case Voice::Rel: v.env -= v.relStep; if (v.env <= 0.0f) { v.env = 0.0f; v.stage = Voice::Idle; } break;
                    default: break;
                }
                const float e = v.env * v.env; // a gentle curve: the ear hears amplitude, not volts
                dryL[i] += bl[i] * e;
                dryR[i] += br[i] * e;
                if (v.stage == Voice::Idle) break;
            }
            v.age += static_cast<float>(m) / static_cast<float>(sr_);
            voiceLvl_[static_cast<size_t>(vi)].store(v.env, std::memory_order_relaxed);
            if (v.stage == Voice::Idle) v.gen = 0;
        }
        (void)anyVoice;

        // ---- REPEAT (beat repeat) on the voices' sum ----
        {
            const double div = std::max(1e-6, divBeats(P_.repeatDiv, bpb));
            const int loopLen = std::max(64, std::min(static_cast<int>(repeatRing_[0].size()) - 8, static_cast<int>(div * 60.0 / std::max(20.0, T_.bpm) * sr_)));
            const double b0 = beat_;
            const double nextGrid = (std::floor(b0 / div) + 1.0) * div;
            int boundaryAt = -1;
            if (nextGrid - b0 <= m * bps + 1e-12) boundaryAt = std::max(0, static_cast<int>((nextGrid - b0) / bps));
            const size_t cap = repeatRing_[0].size();
            for (int i = 0; i < m; ++i) {
                repeatRing_[0][repeatW_] = dryL[i];
                repeatRing_[1][repeatW_] = dryR[i];
                repeatW_ = (repeatW_ + 1) % cap;
                if (P_.repeatOn && repeatState_ == 0) repeatState_ = 1;
                if (!P_.repeatOn && repeatState_ == 1) repeatState_ = 0;
                if ((repeatState_ == 1 || (repeatState_ == 2 && repeatRearm_)) && i == boundaryAt) { // capture the slice that just went by (again, once a fading repeat has died away)
                    for (int k = 0; k < loopLen; ++k) {
                        const size_t src = (repeatW_ + cap - static_cast<size_t>(loopLen) + static_cast<size_t>(k)) % cap;
                        loopBuf_[0][static_cast<size_t>(k)] = repeatRing_[0][src];
                        loopBuf_[1][static_cast<size_t>(k)] = repeatRing_[1][src];
                    }
                    repeatLen_ = loopLen;
                    repeatPos_ = 0;
                    repeatGain_ = 1.0f;
                    repeatState_ = 2;
                    repeatRearm_ = false;
                }
                const float target = (repeatState_ == 2 && P_.repeatOn && !repeatRearm_) ? 1.0f : 0.0f;
                repeatMix_ += (target - repeatMix_) * 0.004f;
                if (repeatState_ == 2) {
                    const int len = repeatLen_;
                    const int fade = std::min(len / 4, static_cast<int>(sr_ * 0.0015));
                    float g = repeatGain_;
                    if (repeatPos_ < fade) g *= static_cast<float>(repeatPos_) / static_cast<float>(std::max(1, fade));
                    else if (repeatPos_ > len - fade) g *= static_cast<float>(len - repeatPos_) / static_cast<float>(std::max(1, fade));
                    const float rl = loopBuf_[0][static_cast<size_t>(repeatPos_)] * g;
                    const float rr = loopBuf_[1][static_cast<size_t>(repeatPos_)] * g;
                    dryL[i] = dryL[i] * (1.0f - repeatMix_) + rl * repeatMix_;
                    dryR[i] = dryR[i] * (1.0f - repeatMix_) + rr * repeatMix_;
                    if (++repeatPos_ >= len) {
                        repeatPos_ = 0;
                        repeatGain_ *= std::min(1.0f, std::max(0.0f, P_.repeatDecay));
                        if (repeatGain_ < 0.04f && P_.repeatDecay < 0.999f) repeatRearm_ = true; // faded out: the sound comes back and the next beat is caught again
                    }
                    if (!P_.repeatOn && repeatMix_ < 0.002f) repeatState_ = 0;
                }
            }
        }

        // ---- effects, level, safety ----
        const float powerTarget = P_.on ? 1.0f : 0.0f;
        const float lvlTarget = std::pow(10.0f, (map::levelDb(P_.level) + P_.outputGainDb) / 20.0f) * (P_.level <= 0.001f ? 0.0f : 1.0f);
        if (curLevel_ < 0.0f) curLevel_ = lvlTarget;
        const double beatSec = 60.0 / std::max(20.0, T_.bpm);
        const float delayTarget = static_cast<float>(std::min(2.4, std::max(0.02, divBeats(P_.delayDiv, bpb) * beatSec))) * static_cast<float>(sr_);
        if (delayCur_ < 0.0f) delayCur_ = delayTarget;
        reverb_.set(P_.reverbSize, P_.reverbDecay, P_.reverbDamp);
        const float dMix = P_.delayMix, rMix = P_.reverbMix;
        const float fb = std::min(0.92f, P_.delayFb);
        const float delayIn = std::sqrt(1.0f - fb * fb); // the more it feeds back, the less goes in, so long echoes do not pile up
        const float toneA = 0.05f + 0.9f * P_.delayTone * P_.delayTone;
        float peak = 0.0f;
        for (int i = 0; i < m; ++i) {
            curLevel_ += (lvlTarget - curLevel_) * 0.002f;
            powerGain_ += (powerTarget - powerGain_) * 0.01f;
            delayCur_ += (delayTarget - delayCur_) * 0.0004f;
            float l = dryL[i], r = dryR[i];
            // ping-pong delay
            const float dl = delayL_.read(std::max(2.0f, delayCur_)), dr = delayR_.read(std::max(2.0f, delayCur_));
            delayLp_[0] += toneA * (dr - delayLp_[0]);
            delayLp_[1] += toneA * (dl - delayLp_[1]);
            delayL_.write(dsp::flushDenormF(l * delayIn + delayLp_[0] * fb));
            delayR_.write(dsp::flushDenormF(r * delayIn + delayLp_[1] * fb));
            l += dl * dMix * 0.9f;
            r += dr * dMix * 0.9f;
            // reverb
            float rl = 0.0f, rr = 0.0f;
            reverb_.process(l, r, rl, rr);
            l += rl * rMix;
            r += rr * rMix;
            const float g = curLevel_ * (P_.trails ? 1.0f : powerGain_);
            l *= g;
            r *= g;
            if (P_.trails) { // power off: the new sound stops, the tails ring out; the dry part is already released
            }
            // a transparent soft limit: untouched below 0.7, never above 1.0
            auto lim = [](float x) {
                const float a = std::fabs(x);
                if (a <= 0.7f) return x;
                const float o = 0.7f + 0.3f * tanhFast((a - 0.7f) / 0.3f);
                return x < 0 ? -o : o;
            };
            l = lim(l);
            r = lim(r);
            outL[i] = l;
            outR[i] = r;
            peak = std::max(peak, std::max(std::fabs(l), std::fabs(r)));
        }
        float cur = peakOut_.load(std::memory_order_relaxed);
        while (peak > cur && !peakOut_.compare_exchange_weak(cur, peak, std::memory_order_relaxed)) {}

        beat_ += m * bps;
        clockTime_ += static_cast<double>(m) / sr_;
        clock_.store(clockTime_, std::memory_order_relaxed);
        if (!P_.on && powerGain_ < 0.0005f && !P_.trails) { delayL_.clear(); delayR_.clear(); reverb_.clear(); }
    }

    // ---- state ----
    double sr_ = 44100.0;
    Params P_;
    Transport T_;
    const Sample* sa_ = nullptr;
    const Sample* sb_ = nullptr;
    std::vector<float> slices_, onsets01_;
    std::vector<Grain> grains_;
    std::array<Voice, kMaxVoices> voices_{};
    std::array<std::array<float, kSub>, kMaxVoices> busL_{}, busR_{};
    float ramp_[kWin] = {};
    uint32_t genCounter_ = 0;
    double beat_ = 0.0;
    double freePhase_[2] = {0.0, 0.0};
    int lastWalkStep_ = -1000000, drunk_ = 0;
    float bend_ = 0.0f, bendTarget_ = 0.0f;
    double clockTime_ = 0.0;

    // effects
    dsp::DelayLine delayL_, delayR_;
    dsp::Reverb reverb_;
    float delayLp_[2] = {0.0f, 0.0f};
    float delayCur_ = -1.0f, curLevel_ = -1.0f, powerGain_ = 1.0f, limEnv_ = 0.0f;
    // repeat
    std::vector<float> repeatRing_[2], loopBuf_[2];
    size_t repeatW_ = 0;
    int repeatState_ = 0, repeatLen_ = 0, repeatPos_ = 0; // 0 off, 1 armed, 2 playing
    float repeatGain_ = 1.0f, repeatMix_ = 0.0f;
    bool repeatRearm_ = false;

    // what the window reads
    std::array<std::atomic<float>, kMaxVoices> voicePos_{}, voiceLvl_{};
    std::array<Viz, kVizGrains> viz_;
    int vizHead_ = 0;
    std::atomic<double> clock_{0.0};
    std::atomic<int> walkStep_{0}, activeGrains_{0};
    std::atomic<float> walkSlice_{0.0f}, posShown_{0.0f}, scatterShown_{0.0f}, filterShown_{0.0f}, morphShown_{0.0f}, peakOut_{0.0f};
    std::array<std::atomic<float>, 2> rampShown_{};
};

} // namespace mote
