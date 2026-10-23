#!/usr/bin/env python3
"""Writes core/presets_factory.h. Edit the preset table below (tools/presets_table.py), not the header.

A preset is a dict of overrides on DEFAULTS, in the units a person would say them in:
  knobs 0..1, pitch in semitones, tune in cents, voices 1..16, walkSteps 2..16, macro depth -1..1,
  choices by name (or index), switches True / False.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from presets_table import PRESETS
import json

def load_levels():
    """tools/levels.json is written by `mote_render report tools/levels.json`: the level knob that puts every preset at the same loudness."""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "levels.json")
    try:
        with open(path) as f: return json.load(f)
    except (OSError, ValueError):
        return {}

SOURCES = ["Guitar Chords", "Warm Pad", "Choir Ah", "Glass Bell", "Tape Dust", "Drum Loop", "Pluck Run", "Your sample"]
SOURCE_KEYS = [4, 9, 9, 4, 0, 0, 5, 0]
SCALES = ["Chromatic", "Major", "Minor", "Dorian", "Mixolydian", "Pentatonic", "Minor pent.", "Blues", "Whole tone", "Harmonic minor", "Fifths"]
PLAY = ["Hold", "Travel", "Bounce"]
WALK = ["Forward", "Backward", "Ping-pong", "Random", "Drunk", "Skip", "Chain"]
DIVS = ["1/4", "1/8", "1/8 triplet", "1/16", "1/16 triplet", "1/32", "1/2", "1 bar"]
FILT = ["Low-pass", "High-pass", "Band-pass"]
SHAPES = ["Sine", "Triangle", "Saw", "Random"]
SYNCS = ["Free", "4 bars", "2 bars", "1 bar", "1/2 note", "1/4 note", "1/8 note", "1/16 note"]
TARGETS = ["position", "scatter", "density", "size", "pitch", "jitter", "spread", "filter", "morph"]

# (id, kind, default, extra)   kinds: knob, pitch, tune, voices, steps, bip (-1..1), choice(list), bool
P = []
def add(i, kind, default, extra=None): P.append((i, kind, default, extra))
for i, d in [("position", .30), ("scatter", .15), ("density", .55), ("size", .50)]: add(i, "knob", d)
add("pitch", "pitch", 0)
for i, d in [("jitter", 0.0), ("spread", .5), ("speed", .5), ("magnet", 0.0)]: add(i, "knob", d)
add("scale", "choice", "Chromatic", SCALES)
for i, d in [("attack", .15), ("decay", .40), ("sustain", .85), ("release", .45)]: add(i, "knob", d)
add("voices", "voices", 8)
for i, d in [("level", .87), ("delayMix", 0.0), ("reverbMix", 0.0)]: add(i, "knob", d)
add("play", "choice", "Hold", PLAY)
for i in ["morphOn", "cohereOn", "walkOn", "repeatOn", "filterOn", "compOn"]: add(i, "bool", False)
add("morphAmt", "knob", .5); add("cohereAmt", "knob", .7)
add("walkPattern", "choice", "Forward", WALK); add("repeatDiv", "choice", "1/8", DIVS)
add("filterCut", "knob", .6); add("compAmt", "knob", .5)
add("source", "choice", "Warm Pad", SOURCES); add("morphSource", "choice", "Glass Bell", SOURCES)
add("sampleKey", "key", 9)
add("grainShape", "knob", .35); add("reverse", "knob", 0.0); add("tune", "tune", 0)
add("filterType", "choice", "Low-pass", FILT)
for i, d in [("filterRes", .3), ("filterSpread", .25), ("filterKey", 0.0)]: add(i, "knob", d)
add("walkRate", "choice", "1/16", DIVS); add("walkSteps", "steps", 8); add("walkGate", "knob", .85)
add("repeatDecay", "knob", 1.0)
add("delayDiv", "choice", "1/8 triplet", DIVS); add("delayFb", "knob", .4); add("delayTone", "knob", .6)
for i, d in [("reverbSize", .5), ("reverbDecay", .55), ("reverbDamp", .5)]: add(i, "knob", d)
add("trails", "bool", True)
for r in (1, 2):
    add(f"r{r}Rate", "knob", .3); add(f"r{r}Depth", "knob", 0.0)
    add(f"r{r}Shape", "choice", "Sine", SHAPES); add(f"r{r}Sync", "choice", "Free", SYNCS)
for k in (1, 2):
    add(f"m{k}Value", "knob", 0.0); add(f"m{k}Depth", "bip", .5)
for pre in ("r1", "r2", "m1", "m2"):
    for t in range(9): add(f"{pre}t{t}", "bool", False)
IDS = [p[0] for p in P]
DEF = {p[0]: p[2] for p in P}
KIND = {p[0]: (p[1], p[3]) for p in P}

def norm(i, v):
    kind, extra = KIND[i]
    if kind == "knob": return max(0.0, min(1.0, float(v)))
    if kind == "pitch": return (max(-24, min(24, float(v))) + 24) / 48.0
    if kind == "tune": return (max(-50, min(50, float(v))) + 50) / 100.0
    if kind == "voices": return (max(1, min(16, int(v))) - 1) / 15.0
    if kind == "steps": return (max(2, min(16, int(v))) - 2) / 14.0
    if kind == "bip": return (max(-1.0, min(1.0, float(v))) + 1) / 2.0
    if kind == "key": return int(v) / 11.0
    if kind == "bool": return 1.0 if v else 0.0
    if kind == "choice":
        idx = extra.index(v) if isinstance(v, str) else int(v)
        return idx / (len(extra) - 1)
    raise ValueError(kind)

def resolve(spec):
    """Targets are written as ramp1=[names], macro2=[names]; expand them into the switch ids. Own-key follows the source."""
    s = dict(spec)
    for key, pre in (("ramp1", "r1"), ("ramp2", "r2"), ("macro1", "m1"), ("macro2", "m2")):
        for t in s.pop(key, []):
            s[f"{pre}t{TARGETS.index(t)}"] = True
    if "source" in s and "sampleKey" not in s:
        s["sampleKey"] = SOURCE_KEYS[SOURCES.index(s["source"])]
    for k in s:
        if k not in KIND: raise KeyError(f"unknown parameter {k!r}")
    return s

def row(spec):
    s = resolve(spec)
    return [norm(i, s.get(i, DEF[i])) for i in IDS]

def main(out):
    lines = ["// GENERATED by tools/pack_gen.py - edit tools/presets_table.py, not this file.", "#pragma once", "#include <vector>", "",
             "namespace mote {", "namespace presetdata {", f"constexpr int kNumSoundIds = {len(IDS)};",
             "inline const char* const* soundIdNames() {",
             "    static const char* const n[kNumSoundIds] = {" + ", ".join(f'"{i}"' for i in IDS) + "};", "    return n;", "}", "",
             "struct FactoryPreset {", "    const char *category, *name, *hint;", "    float v[kNumSoundIds]; // normalised 0..1, in soundIdNames() order", "};", "",
             "inline const std::vector<FactoryPreset>& factoryPresets() {", "    static const std::vector<FactoryPreset> bank = {"]
    levels = load_levels()
    for cat, name, hint, spec in PRESETS:
        if name in levels: spec = dict(spec, level=levels[name])
        vals = ", ".join(f"{x:.4f}f" for x in row(spec))
        lines.append(f'        {{"{cat}", "{name}", "{hint}", {{{vals}}}}},')
    lines += ["    };", "    return bank;", "}", "} // namespace presetdata", "} // namespace mote", ""]
    open(out, "w").write("\n".join(lines))
    print(f"{len(PRESETS)} presets, {len(IDS)} parameters -> {out}")

if __name__ == "__main__":
    main(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "core", "presets_factory.h"))
