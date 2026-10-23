// MOTE - small DSP building blocks shared by the engine and the effects. Plain C++, no JUCE.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace mote {

constexpr double kPi = 3.14159265358979323846;

namespace dsp {

inline float clamp01(float v) { return std::min(1.0f, std::max(0.0f, v)); }
inline double flushDenorm(double v) { return std::fabs(v) < 1e-20 ? 0.0 : v; }
inline float flushDenormF(float v) { return std::fabs(v) < 1e-18f ? 0.0f : v; }

struct OnePole {
    float a = 0.0f, z = 0.0f;
    void setCutoff(double fs, double f) { a = static_cast<float>(1.0 - std::exp(-2.0 * kPi * f / fs)); }
    float lp(float x) { z += a * (x - z); return z; }
    float hp(float x) { z += a * (x - z); return x - z; }
    void reset() { z = 0.0f; }
};

// a state-variable filter (the "TPT" form: stable and clean even when the cutoff changes quickly)
struct Svf {
    float g = 0.1f, k = 1.0f, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    void set(double fs, double f, double q) {
        f = std::min(std::max(f, 15.0), fs * 0.45);
        g = static_cast<float>(std::tan(kPi * f / fs));
        k = static_cast<float>(1.0 / std::max(0.3, q));
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }
    // returns lp, bp, hp
    inline void process(float v0, float& lp, float& bp, float& hp) {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = flushDenormF(2.0f * v1 - ic1);
        ic2 = flushDenormF(2.0f * v2 - ic2);
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
};

// a delay line with a fractional read
class DelayLine {
public:
    void init(int maxSamples) {
        size_ = 1;
        while (size_ < maxSamples + 4) size_ <<= 1;
        buf_.assign(static_cast<size_t>(size_), 0.0f);
        mask_ = size_ - 1;
        w_ = 0;
    }
    void clear() { std::fill(buf_.begin(), buf_.end(), 0.0f); }
    inline void write(float x) { buf_[static_cast<size_t>(w_)] = x; w_ = (w_ + 1) & mask_; }
    // the sample written 'd' samples ago (d >= 1)
    inline float read(float d) const {
        const int di = static_cast<int>(d);
        const float f = d - static_cast<float>(di);
        const int i0 = (w_ - di) & mask_, i1 = (i0 - 1) & mask_;
        return buf_[static_cast<size_t>(i0)] + (buf_[static_cast<size_t>(i1)] - buf_[static_cast<size_t>(i0)]) * f;
    }
    int capacity() const { return size_; }

private:
    std::vector<float> buf_;
    int size_ = 0, mask_ = 0, w_ = 0;
};

// a small stereo reverb: 8 delay lines mixed through a Hadamard matrix, with a low-pass in the feedback so the tail
// darkens as it fades, and two all-pass diffusers on the way in.
class Reverb {
public:
    void prepare(double fs) {
        fs_ = fs;
        static const int base[kN] = {1103, 1277, 1481, 1663, 1861, 2053, 2269, 2477}; // at 48 kHz, mutually "unrelated"
        for (int i = 0; i < kN; ++i) {
            baseLen_[i] = base[i] * static_cast<float>(fs / 48000.0);
            line_[i].init(static_cast<int>(baseLen_[i] * 2.2f) + 8);
            damp_[i].reset();
        }
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 2; ++k) {
                const int len = static_cast<int>((k == 0 ? 142 : 107) * (c == 0 ? 1.0 : 1.13) * fs / 48000.0) + 1;
                apLen_[c][k] = len;
                ap_[c][k].init(len + 4);
                ap_[c][k].clear();
            }
        pre_.init(static_cast<int>(fs * 0.25));
        set(0.5f, 0.5f, 0.5f);
        curSize_ = size_;
        updateCoefs();
        wetComp_ = wetCompTarget_;
    }
    // size 0..1 (room size), decay 0..1 (about 0.3 s .. 12 s), damp 0..1 (how fast the highs die)
    void set(float size, float decay, float damp) {
        size_ = 0.45f + 1.4f * size;
        const float rt60 = 0.3f * std::pow(40.0f, decay);
        rt60_ = rt60;
        damp01_ = damp;
        dampA_ = static_cast<float>(1.0 - std::exp(-2.0 * kPi * (16000.0 * std::pow(0.08, static_cast<double>(damp)) + 600.0) / fs_));
    }
    void process(float inL, float inR, float& outL, float& outR) {
        curSize_ += (size_ - curSize_) * 0.0008f;
        if (--coefCount_ <= 0) updateCoefs(); // the per-line loss and the level compensation, every 16 samples
        float x[2] = {inL, inR};
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 2; ++k) { // Schroeder all-pass: y = -g x + z, z' = x + g y
                const float d = ap_[c][k].read(static_cast<float>(apLen_[c][k]));
                const float y = -0.6f * x[c] + d;
                ap_[c][k].write(x[c] + 0.6f * y);
                x[c] = y;
            }
        const float mono = 0.5f * (x[0] + x[1]);
        float v[kN];
        for (int i = 0; i < kN; ++i) {
            const float y = line_[i].read(baseLen_[i] * curSize_);
            damp_[i].z += dampA_ * (y - damp_[i].z);
            v[i] = damp_[i].z * gain_[i];
        }
        // Hadamard mixing (fast Walsh-Hadamard transform), scaled to be lossless
        for (int s = 1; s < kN; s <<= 1)
            for (int i = 0; i < kN; i += 2 * s)
                for (int j = i; j < i + s; ++j) {
                    const float a = v[j], b = v[j + s];
                    v[j] = a + b;
                    v[j + s] = a - b;
                }
        const float norm = 0.35355339f;
        for (int i = 0; i < kN; ++i) {
            const float inj = (i & 1) ? x[1] : x[0];
            line_[i].write(flushDenormF(v[i] * norm + 0.5f * inj + 0.5f * mono));
        }
        // the tail is kept at about the loudness of the input however long or large it is (see updateCoefs)
        outL = wetComp_ * 0.30f * (v[0] - v[2] + v[4] - v[6] + 0.4f * (v[1] + v[3]));
        outR = wetComp_ * 0.30f * (v[1] - v[3] + v[5] - v[7] + 0.4f * (v[0] + v[2]));
    }
    void clear() {
        for (int i = 0; i < kN; ++i) { line_[i].clear(); damp_[i].reset(); }
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 2; ++k) ap_[c][k].clear();
    }

private:
    static constexpr int kN = 8;
    double fs_ = 48000.0;
    DelayLine line_[kN], ap_[2][2], pre_;
    OnePole damp_[kN];
    float baseLen_[kN] = {};
    int apLen_[2][2] = {};
    float size_ = 1.0f, curSize_ = 1.0f, rt60_ = 2.0f, dampA_ = 0.3f, damp01_ = 0.5f;
    float gain_[kN] = {};
    float wetComp_ = 1.0f, wetCompTarget_ = 1.0f;
    int coefCount_ = 0;

    // Per-line loss for the chosen decay time, and a gain that keeps the loudness steady. A long, large or bright
    // reverb piles up much more energy than a short, small or dark one (measured: nearly 30 dB apart), so the wet
    // signal is scaled by the inverse of a fitted model of that build-up (fit error about 0.4 dB over the whole range).
    void updateCoefs() {
        coefCount_ = 16;
        double fg = 0.0, f = 0.0;
        for (int i = 0; i < kN; ++i) {
            const float len = baseLen_[i] * curSize_;
            const double g2 = std::pow(10.0, -6.0 * len / (fs_ * rt60_));
            gain_[i] = static_cast<float>(std::sqrt(g2));
            f += 1.0 / (1.0 - g2) / kN;
            fg += g2 / (1.0 - g2) / kN;
        }
        const double predDb = -0.267 * 10.0 * std::log10(f) + 1.003 * 10.0 * std::log10(fg + 1e-9) - 3.819 * damp01_ + 2.454;
        wetCompTarget_ = static_cast<float>(std::pow(10.0, -predDb / 20.0) * 0.55); // 0.55: at full mix the tail sits about 5 dB under the input (bass-heavy sounds add a few dB more)
        wetComp_ += (wetCompTarget_ - wetComp_) * 0.05f;
    }
};

} // namespace dsp
} // namespace mote
