// Parameter IDs. These are what the DAW stores in projects and automation lanes, so once a
// build has shipped NEVER rename or renumber them - add new ones instead.
#pragma once

namespace ids {
// ---- the knobs on the front ----
inline constexpr const char* position = "position";
inline constexpr const char* scatter = "scatter";
inline constexpr const char* density = "density";
inline constexpr const char* size = "size";
inline constexpr const char* pitch = "pitch";
inline constexpr const char* jitter = "jitter";
inline constexpr const char* spread = "spread";
inline constexpr const char* speed = "speed";
inline constexpr const char* magnet = "magnet";
inline constexpr const char* root = "root";
inline constexpr const char* scale = "scale";
inline constexpr const char* attack = "attack";
inline constexpr const char* decay = "decay";
inline constexpr const char* sustain = "sustain";
inline constexpr const char* release = "release";
inline constexpr const char* voices = "voices";
inline constexpr const char* level = "level";
inline constexpr const char* delayMix = "delayMix";
inline constexpr const char* reverbMix = "reverbMix";
// ---- the PLAY lever and the six module levers, each with the small knob beneath it ----
inline constexpr const char* play = "play";
inline constexpr const char* morphOn = "morphOn";
inline constexpr const char* cohereOn = "cohereOn";
inline constexpr const char* walkOn = "walkOn";
inline constexpr const char* repeatOn = "repeatOn";
inline constexpr const char* filterOn = "filterOn";
inline constexpr const char* compOn = "compOn";
inline constexpr const char* morphAmt = "morphAmt";
inline constexpr const char* cohereAmt = "cohereAmt";
inline constexpr const char* walkPattern = "walkPattern";
inline constexpr const char* repeatDiv = "repeatDiv";
inline constexpr const char* filterCut = "filterCut";
inline constexpr const char* compAmt = "compAmt";
// ---- footswitches ----
inline constexpr const char* power = "power";
inline constexpr const char* latch = "latch";
inline constexpr const char* latchHold = "latchHold";
// ---- under the hood ----
inline constexpr const char* source = "source";
inline constexpr const char* morphSource = "morphSource";
inline constexpr const char* sampleKey = "sampleKey";
inline constexpr const char* grainShape = "grainShape";
inline constexpr const char* reverse = "reverse";
inline constexpr const char* tune = "tune";
inline constexpr const char* filterType = "filterType";
inline constexpr const char* filterRes = "filterRes";
inline constexpr const char* filterSpread = "filterSpread";
inline constexpr const char* filterKey = "filterKey";
inline constexpr const char* walkRate = "walkRate";
inline constexpr const char* walkSteps = "walkSteps";
inline constexpr const char* walkGate = "walkGate";
inline constexpr const char* repeatDecay = "repeatDecay";
inline constexpr const char* delayDiv = "delayDiv";
inline constexpr const char* delayFb = "delayFb";
inline constexpr const char* delayTone = "delayTone";
inline constexpr const char* reverbSize = "reverbSize";
inline constexpr const char* reverbDecay = "reverbDecay";
inline constexpr const char* reverbDamp = "reverbDamp";
inline constexpr const char* trails = "trails";
// ---- two ramps (move knobs by themselves) and two macros (one knob moves several) ----
inline constexpr const char* r1Rate = "r1Rate";
inline constexpr const char* r1Depth = "r1Depth";
inline constexpr const char* r1Shape = "r1Shape";
inline constexpr const char* r1Sync = "r1Sync";
inline constexpr const char* r2Rate = "r2Rate";
inline constexpr const char* r2Depth = "r2Depth";
inline constexpr const char* r2Shape = "r2Shape";
inline constexpr const char* r2Sync = "r2Sync";
inline constexpr const char* m1Value = "m1Value";
inline constexpr const char* m1Depth = "m1Depth";
inline constexpr const char* m2Value = "m2Value";
inline constexpr const char* m2Depth = "m2Depth";
inline constexpr const char* manualBpm = "manualBpm";
inline constexpr const char* outputGain = "outputGain";
// The target switches are named r1t0..r1t8, r2t0.., m1t0.., m2t0.. (one per thing a ramp / macro can move)
} // namespace ids
