// MOTE - tempo and divisions. The DAW tells the plugin the tempo and where the song is; without a DAW the plugin
// follows its own "Tempo" slider.
#pragma once
#include <algorithm>

namespace mote {

// Where the host is. hostValid: the DAW gave a song position; playing: the transport is running.
struct Transport {
    double bpm = 120.0;
    double ppq = 0.0;
    bool hostValid = false;
    bool playing = false;
    int beatsPerBar = 4;
};

// note divisions, in quarter-note beats (0 for "1 bar": the length of a bar)
constexpr int kNumDivs = 8;
inline const char* divName(int i) {
    static const char* n[kNumDivs] = {"1/4", "1/8", "1/8 triplet", "1/16", "1/16 triplet", "1/32", "1/2", "1 bar"};
    return n[std::min(kNumDivs - 1, std::max(0, i))];
}
inline double divBeats(int i, double beatsPerBar) {
    static const double b[kNumDivs] = {1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 2.0, 0.0};
    i = std::min(kNumDivs - 1, std::max(0, i));
    return i == 7 ? beatsPerBar : b[i];
}

// ramp / LFO lock: free, or one cycle per ...
constexpr int kNumSyncs = 8;
inline const char* syncName(int i) {
    static const char* n[kNumSyncs] = {"Free", "4 bars", "2 bars", "1 bar", "1/2 note", "1/4 note", "1/8 note", "1/16 note"};
    return n[std::min(kNumSyncs - 1, std::max(0, i))];
}
inline double syncBeats(int i, double beatsPerBar) {
    const double b[kNumSyncs] = {0.0, 4 * beatsPerBar, 2 * beatsPerBar, beatsPerBar, 2.0, 1.0, 0.5, 0.25};
    return b[std::min(kNumSyncs - 1, std::max(0, i))];
}

} // namespace mote
