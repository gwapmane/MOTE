// Tiny WAV reader/writer for the offline tools (PCM 8/16/24/32, float32; any channel count).
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace wav {

struct Audio {
    int rate = 48000;
    std::vector<float> l, r; // always stereo; mono input is duplicated
    size_t frames() const { return l.size(); }
};

inline uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }
inline uint16_t rd16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

inline bool read(const std::string& path, Audio& a, std::string& err) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { err = "cannot open " + path; return false; }
    std::vector<uint8_t> d;
    uint8_t tmp[65536];
    size_t n;
    while ((n = std::fread(tmp, 1, sizeof tmp, f)) > 0) d.insert(d.end(), tmp, tmp + n);
    std::fclose(f);
    if (d.size() < 44 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4)) {
        err = "not a WAV file";
        return false;
    }
    int fmt = 0, ch = 0, bits = 0;
    const uint8_t* data = nullptr;
    size_t dataLen = 0;
    size_t p = 12;
    while (p + 8 <= d.size()) {
        const uint32_t sz = rd32(&d[p + 4]);
        if (!std::memcmp(&d[p], "fmt ", 4)) {
            fmt = rd16(&d[p + 8]);
            ch = rd16(&d[p + 10]);
            a.rate = static_cast<int>(rd32(&d[p + 12]));
            bits = rd16(&d[p + 22]);
            if (fmt == 0xFFFE && sz >= 26) fmt = rd16(&d[p + 32]); // extensible
        } else if (!std::memcmp(&d[p], "data", 4)) {
            data = &d[p + 8];
            dataLen = std::min<size_t>(sz, d.size() - p - 8);
        }
        p += 8 + sz + (sz & 1);
    }
    if (!data || ch < 1 || bits == 0) { err = "malformed WAV"; return false; }
    const int bytes = bits / 8;
    const size_t frames = dataLen / (static_cast<size_t>(bytes) * ch);
    a.l.resize(frames);
    a.r.resize(frames);
    for (size_t i = 0; i < frames; ++i) {
        float v[2] = {0, 0};
        for (int c = 0; c < std::min(ch, 2); ++c) {
            const uint8_t* s = data + (i * ch + c) * bytes;
            float x = 0;
            if (fmt == 3 && bits == 32) { float t; std::memcpy(&t, s, 4); x = t; }
            else if (bits == 16) x = static_cast<int16_t>(rd16(s)) / 32768.0f;
            else if (bits == 24) x = (static_cast<int32_t>((s[0] << 8) | (s[1] << 16) | (static_cast<uint32_t>(s[2]) << 24)) >> 8) / 8388608.0f;
            else if (bits == 32) x = static_cast<int32_t>(rd32(s)) / 2147483648.0f;
            else if (bits == 8) x = (s[0] - 128) / 128.0f;
            v[c] = x;
        }
        a.l[i] = v[0];
        a.r[i] = ch > 1 ? v[1] : v[0];
    }
    return true;
}

inline void w32(FILE* f, uint32_t v) { uint8_t b[4] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)}; std::fwrite(b, 1, 4, f); }
inline void w16(FILE* f, uint16_t v) { uint8_t b[2] = {uint8_t(v), uint8_t(v >> 8)}; std::fwrite(b, 1, 2, f); }

// 16-bit PCM stereo (universally playable). Values are clipped.
inline bool write16(const std::string& path, const Audio& a) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const uint32_t dataBytes = static_cast<uint32_t>(a.frames() * 4);
    std::fwrite("RIFF", 1, 4, f); w32(f, 36 + dataBytes); std::fwrite("WAVEfmt ", 1, 8, f);
    w32(f, 16); w16(f, 1); w16(f, 2); w32(f, static_cast<uint32_t>(a.rate)); w32(f, static_cast<uint32_t>(a.rate) * 4); w16(f, 4); w16(f, 16);
    std::fwrite("data", 1, 4, f); w32(f, dataBytes);
    for (size_t i = 0; i < a.frames(); ++i) {
        for (float x : {a.l[i], a.r[i]}) {
            float c = x > 1.0f ? 1.0f : (x < -1.0f ? -1.0f : x);
            w16(f, static_cast<uint16_t>(static_cast<int16_t>(c * 32767.0f)));
        }
    }
    std::fclose(f);
    return true;
}

} // namespace wav
