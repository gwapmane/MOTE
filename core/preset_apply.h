// MOTE - turns a list of normalised parameter values (0..1, as presets store them) into engine Params, using exactly the
// same mapping as the plugin's parameter layout. Used by the offline tools to listen to / measure the factory presets
// without any plugin code.
#pragma once
#include <cmath>
#include <cstring>
#include <string>

#include "grain.h"
#include "sources.h"

namespace mote {

struct Setup {
    Params p;
    int source = 1, morphSource = 3;
};

namespace detail {
inline int pick(float x, int n) { return std::min(n - 1, std::max(0, static_cast<int>(std::lround(x * (n - 1))))); }
} // namespace detail

// returns false for an id it does not know
inline bool setNormalised(Setup& s, const std::string& id, float x) {
    Params& p = s.p;
    using detail::pick;
    auto is = [&](const char* n) { return id == n; };
    if (is("position")) p.position = x; else if (is("scatter")) p.scatter = x; else if (is("density")) p.density = x;
    else if (is("size")) p.size = x; else if (is("pitch")) p.pitchSemis = std::round(-24.0f + 48.0f * x);
    else if (is("jitter")) p.jitter = x; else if (is("spread")) p.spread = x; else if (is("speed")) p.speed = x;
    else if (is("magnet")) p.magnet = x; else if (is("root")) p.root = pick(x, 12); else if (is("scale")) p.scale = pick(x, kNumScales);
    else if (is("attack")) p.attack = x; else if (is("decay")) p.decay = x; else if (is("sustain")) p.sustain = x;
    else if (is("release")) p.release = x; else if (is("voices")) p.voices = 1 + static_cast<int>(std::lround(15.0f * x));
    else if (is("level")) p.level = x; else if (is("delayMix")) p.delayMix = x; else if (is("reverbMix")) p.reverbMix = x;
    else if (is("play")) p.play = pick(x, kNumPlay);
    else if (is("morphOn")) p.morphOn = x > 0.5f; else if (is("cohereOn")) p.cohereOn = x > 0.5f; else if (is("walkOn")) p.walkOn = x > 0.5f;
    else if (is("repeatOn")) p.repeatOn = x > 0.5f; else if (is("filterOn")) p.filterOn = x > 0.5f; else if (is("compOn")) p.compOn = x > 0.5f;
    else if (is("morphAmt")) p.morph = x; else if (is("cohereAmt")) p.cohere = x; else if (is("walkPattern")) p.walkPattern = pick(x, kNumWalk);
    else if (is("repeatDiv")) p.repeatDiv = pick(x, kNumDivs); else if (is("filterCut")) p.filter = x; else if (is("compAmt")) p.compAmt = x;
    else if (is("source")) s.source = pick(x, sources::kCount + 1); else if (is("morphSource")) s.morphSource = pick(x, sources::kCount + 1);
    else if (is("sampleKey")) p.sampleKey = pick(x, 12);
    else if (is("grainShape")) p.grainShape = x; else if (is("reverse")) p.reverse = x; else if (is("tune")) p.tuneCents = -50.0f + 100.0f * x;
    else if (is("filterType")) p.filterType = pick(x, kNumFilterTypes); else if (is("filterRes")) p.filterRes = x;
    else if (is("filterSpread")) p.filterSpread = x; else if (is("filterKey")) p.filterKey = x;
    else if (is("walkRate")) p.walkRate = pick(x, kNumDivs); else if (is("walkSteps")) p.walkSteps = 2 + static_cast<int>(std::lround(14.0f * x));
    else if (is("walkGate")) p.walkGate = x; else if (is("repeatDecay")) p.repeatDecay = x;
    else if (is("delayDiv")) p.delayDiv = pick(x, kNumDivs); else if (is("delayFb")) p.delayFb = x; else if (is("delayTone")) p.delayTone = x;
    else if (is("reverbSize")) p.reverbSize = x; else if (is("reverbDecay")) p.reverbDecay = x; else if (is("reverbDamp")) p.reverbDamp = x;
    else if (is("trails")) p.trails = x > 0.5f;
    else if (is("r1Rate")) p.ramp[0].rate = x; else if (is("r1Depth")) p.ramp[0].depth = x;
    else if (is("r1Shape")) p.ramp[0].shape = pick(x, kNumRampShapes); else if (is("r1Sync")) p.ramp[0].sync = pick(x, 8);
    else if (is("r2Rate")) p.ramp[1].rate = x; else if (is("r2Depth")) p.ramp[1].depth = x;
    else if (is("r2Shape")) p.ramp[1].shape = pick(x, kNumRampShapes); else if (is("r2Sync")) p.ramp[1].sync = pick(x, 8);
    else if (is("m1Value")) p.macro[0].value = x; else if (is("m1Depth")) p.macro[0].depth = -1.0f + 2.0f * x;
    else if (is("m2Value")) p.macro[1].value = x; else if (is("m2Depth")) p.macro[1].depth = -1.0f + 2.0f * x;
    else if (id.size() == 4 && (id[0] == 'r' || id[0] == 'm') && (id[1] == '1' || id[1] == '2') && id[2] == 't' && id[3] >= '0' && id[3] <= '8') {
        const int k = id[1] - '1', t = id[3] - '0';
        uint32_t& mask = id[0] == 'r' ? p.ramp[k].targets : p.macro[k].targets;
        if (x > 0.5f) mask |= 1u << t; else mask &= ~(1u << t);
    } else return false;
    return true;
}

} // namespace mote
