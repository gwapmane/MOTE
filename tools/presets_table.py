# category, name, hint, overrides   (units: see pack_gen.py)
#
# Loudness is not set here by hand: `mote_render report tools/levels.json` measures every preset playing the same chords and
# writes the level knob that puts them all at the same volume; pack_gen.py then uses that. `level=` below is only the start.
#
# Things to know when reading this table:
#   density  .3 = a few grains a second, .55 = a smooth cloud, .9 = a very dense wall
#   size     .2 = tiny blips, .5 = soft, .8 = long swells
#   scatter  how far from the "position" marker the grains wander      jitter = random pitch wobble per grain
#   speed    .5 is normal; lower = slower travel, higher = faster       (only matters for PLAY = Travel / Bounce)
#   ramp1 / ramp2 = list of what the ramp moves;  macro1 / macro2 = list of what the macro knob moves

PRESETS = [
    # ------------------------------------------------------------------------------------------------ Pads
    ("Pads", "Warm Cloud", "A soft, slow cloud of the pad", dict(source="Warm Pad", position=.3, scatter=.25, density=.62, size=.62, attack=.45, release=.6, reverbMix=.25, level=.8)),
    ("Pads", "Slow Tide", "The pad drifts in and out like water, over four bars", dict(source="Warm Pad", position=.35, scatter=.2, density=.6, size=.6, attack=.5, release=.65, spread=.85, reverbMix=.3, ramp1=["position"], r1Rate=.3, r1Depth=.55, r1Sync="4 bars", r1Shape="Sine")),
    ("Pads", "Choir Haze", "Voices melted into a soft, dark haze", dict(source="Choir Ah", position=.25, scatter=.5, density=.55, size=.5, jitter=.12, attack=.4, release=.7, spread=.9, filterOn=True, filterCut=.5, filterRes=.15, reverbMix=.4, reverbSize=.7, reverbDecay=.65)),
    ("Pads", "Glass Pad", "Bells smeared into a long shiny pad", dict(source="Glass Bell", position=.1, scatter=.3, density=.78, size=.82, attack=.6, release=.75, spread=1.0, reverbMix=.4, reverbDecay=.7, grainShape=.2)),
    ("Pads", "Dusk Strings", "A dark, low-lit pad an octave down", dict(source="Warm Pad", pitch=-12, position=.5, scatter=.12, density=.64, size=.66, attack=.4, release=.6, filterOn=True, filterCut=.4, filterRes=.2, compOn=True, compAmt=.4, reverbMix=.2)),
    ("Pads", "Minor Light", "Choir grains falling onto a sad, gentle scale", dict(source="Choir Ah", position=.3, scatter=.3, density=.55, size=.55, jitter=.35, scale="Minor", attack=.4, release=.6, spread=.8, reverbMix=.35)),
    ("Pads", "Open Fifths", "Hollow, wide, ancient-sounding stacks", dict(source="Warm Pad", position=.45, scatter=.2, density=.6, size=.62, jitter=.5, scale="Fifths", attack=.35, release=.6, spread=.9, delayMix=.2, delayDiv="1/4", delayFb=.5)),
    ("Pads", "Breathing Wall", "A wall of sound that swells and shrinks by itself", dict(source="Warm Pad", position=.6, scatter=.3, density=.7, size=.5, attack=.45, release=.65, ramp1=["size"], r1Rate=.25, r1Depth=.5, r1Sync="2 bars", r1Shape="Triangle", reverbMix=.3)),
    ("Pads", "Morning Mist", "Light, high and quiet, like breath on a window", dict(source="Choir Ah", pitch=12, position=.2, scatter=.4, density=.5, size=.7, attack=.6, release=.8, spread=1.0, filterOn=True, filterCut=.55, reverbMix=.5, reverbSize=.8, reverbDecay=.8)),

    # ------------------------------------------------------------------------------------------------ Textures
    ("Textures", "Tape Snow", "Fine grains of tape dust drifting about", dict(source="Tape Dust", position=.5, scatter=.6, density=.7, size=.3, spread=.9, compOn=True, compAmt=.5, delayMix=.25, level=.85)),
    ("Textures", "Static Garden", "Crackle and hiss, trimmed bright and thin", dict(source="Tape Dust", position=.7, scatter=.7, density=.8, size=.2, spread=1.0, filterOn=True, filterType="High-pass", filterCut=.5, filterRes=.2, compOn=True, compAmt=.6, reverbMix=.2)),
    ("Textures", "Dust Storm", "Rough grit blowing backwards and forwards", dict(source="Tape Dust", position=.4, scatter=.8, density=.88, size=.35, reverse=.5, jitter=.2, spread=1.0, grainShape=.6, reverbMix=.15)),
    ("Textures", "Rust Bloom", "Slow, heavy rust that opens up over time", dict(source="Tape Dust", pitch=-12, position=.3, scatter=.5, density=.6, size=.82, attack=.5, release=.7, filterOn=True, filterCut=.38, filterRes=.4, reverbMix=.35, ramp1=["filter"], r1Rate=.22, r1Depth=.5, r1Sync="4 bars")),
    ("Textures", "Shimmer Fall", "Tiny bells tumbling down a bright scale", dict(source="Glass Bell", pitch=12, position=.2, scatter=.5, density=.58, size=.3, jitter=.5, scale="Pentatonic", spread=1.0, delayMix=.4, delayDiv="1/8", delayFb=.55, reverbMix=.35)),
    ("Textures", "Wire Rain", "Thin plucked wires pattering on a roof", dict(source="Pluck Run", position=.5, scatter=.6, density=.72, size=.18, spread=.9, grainShape=.5, delayMix=.25, reverbMix=.3)),
    ("Textures", "Radio Fog", "A voice heard through a band of static", dict(source="Choir Ah", position=.5, scatter=.45, density=.62, size=.5, filterOn=True, filterType="Band-pass", filterCut=.5, filterRes=.65, filterSpread=.5, spread=.8, reverbMix=.25)),
    ("Textures", "Frozen Breath", "One moment of voice held still and stretched", dict(source="Choir Ah", position=.42, scatter=.02, density=.9, size=.95, attack=.6, release=.85, spread=1.0, reverbMix=.45, reverbSize=.8, reverbDecay=.85, reverbDamp=.6)),

    # ------------------------------------------------------------------------------------------------ Plucks
    ("Plucks", "Glass Rain", "Bright little bell grains falling on a scale", dict(source="Glass Bell", position=.15, scatter=.3, density=.56, size=.3, jitter=.1, scale="Pentatonic", spread=.8, reverbMix=.3, level=.8)),
    ("Plucks", "Music Box Scatter", "A music box that can't remember the tune", dict(source="Glass Bell", position=.1, scatter=.45, density=.46, size=.26, jitter=.5, scale="Major", attack=.05, release=.4, spread=.9, delayMix=.35, delayDiv="1/8 triplet", delayFb=.5, reverbMix=.25)),
    ("Plucks", "Guitar Pollen", "Specks of picked guitar, floating", dict(source="Guitar Chords", position=.3, scatter=.35, density=.58, size=.26, attack=.03, release=.45, spread=.8, grainShape=.3, reverbMix=.3, delayMix=.15)),
    ("Plucks", "Harp Drops", "Soft drops that land on a pentatonic harp", dict(source="Pluck Run", position=.25, scatter=.3, density=.58, size=.36, jitter=.6, scale="Pentatonic", attack=.04, release=.55, spread=1.0, reverbMix=.4, reverbDecay=.65)),
    ("Plucks", "Marimba Walk", "A little wooden pattern marching along by itself", dict(source="Pluck Run", position=.0, scatter=.02, density=.7, size=.26, attack=.02, release=.3, walkOn=True, walkPattern="Forward", walkRate="1/8", walkSteps=8, walkGate=.6, reverbMix=.2)),
    ("Plucks", "Kalimba Dust", "Low thumb-piano notes with a soft tail", dict(source="Glass Bell", pitch=-5, position=.12, scatter=.15, density=.42, size=.28, jitter=.2, scale="Mixolydian", attack=.04, release=.5, spread=.7, delayMix=.3, delayDiv="1/4", reverbMix=.25)),
    ("Plucks", "Pizzicato Cloud", "Short plucks swarming like insects", dict(source="Pluck Run", pitch=7, position=.6, scatter=.5, density=.66, size=.14, attack=.02, release=.25, spread=.9, grainShape=.6)),

    # ------------------------------------------------------------------------------------------------ Rhythm
    ("Rhythm", "Slice Walk", "Steps through the loop one slice at a time", dict(source="Drum Loop", position=.0, scatter=.0, density=.8, size=.4, attack=.01, release=.2, walkOn=True, walkPattern="Forward", walkRate="1/16", walkSteps=16, walkGate=.8, spread=.5)),
    ("Rhythm", "Stutter Kit", "The beat catches and stutters on every hit", dict(source="Drum Loop", position=.0, scatter=.0, density=.7, size=.35, attack=.01, release=.2, repeatOn=True, repeatDiv="1/16", repeatDecay=.55, reverbMix=.15)),
    ("Rhythm", "Broken Beat", "Slices of the loop thrown in random order", dict(source="Drum Loop", position=.0, scatter=.0, density=.82, size=.38, attack=.01, release=.15, walkOn=True, walkPattern="Random", walkRate="1/16", walkSteps=16, walkGate=.7, delayMix=.2, delayDiv="1/8")),
    ("Rhythm", "Half-Time Crush", "Slowed, squashed and heavy", dict(source="Drum Loop", position=.0, speed=.3, play="Travel", scatter=.02, density=.75, size=.45, attack=.01, release=.3, compOn=True, compAmt=.8, filterOn=True, filterCut=.55, reverbMix=.1)),
    ("Rhythm", "Drunk Drummer", "Stumbles around the loop, never in the same place twice", dict(source="Drum Loop", pitch=-3, position=.0, scatter=.05, density=.8, size=.4, attack=.01, release=.2, walkOn=True, walkPattern="Drunk", walkRate="1/8", walkSteps=12, walkGate=.75, filterOn=True, filterCut=.6, delayMix=.25, delayDiv="1/8 triplet", delayFb=.5)),
    ("Rhythm", "Tom Cloud", "Low drums rumbling together", dict(source="Drum Loop", pitch=-12, position=.1, scatter=.4, density=.7, size=.55, attack=.02, release=.45, spread=.7, reverbMix=.3, reverbSize=.7)),
    ("Rhythm", "Gated Pulse", "The pad chopped into a steady pulse", dict(source="Warm Pad", position=.4, scatter=.1, density=.7, size=.45, attack=.02, release=.2, walkOn=True, walkPattern="Forward", walkRate="1/8", walkSteps=4, walkGate=.4, spread=.7, reverbMix=.2)),
    ("Rhythm", "Chop Chords", "Guitar chords hacked into jumpy pieces", dict(source="Guitar Chords", position=.0, scatter=.02, density=.7, size=.4, attack=.01, release=.2, walkOn=True, walkPattern="Skip", walkRate="1/8", walkSteps=8, walkGate=.7, delayMix=.2, delayDiv="1/8 triplet", delayFb=.45)),
    ("Rhythm", "Ping-Pong Strum", "Chords bouncing back and forth across themselves", dict(source="Guitar Chords", position=.0, scatter=.02, density=.7, size=.36, attack=.01, release=.25, walkOn=True, walkPattern="Ping-pong", walkRate="1/16", walkSteps=8, walkGate=.65, spread=.8, reverbMix=.15)),

    # ------------------------------------------------------------------------------------------------ Drones
    ("Drones", "Sub Choir", "A choir so low it's felt more than heard", dict(source="Choir Ah", pitch=-24, position=.4, scatter=.05, density=.78, size=.75, attack=.6, release=.8, filterOn=True, filterCut=.35, compOn=True, compAmt=.5, reverbMix=.25)),
    ("Drones", "Machine Hum", "A steady electrical hum with a slow wobble", dict(source="Tape Dust", pitch=-24, position=.5, scatter=.1, density=.85, size=.6, attack=.4, release=.6, filterOn=True, filterCut=.3, filterRes=.5, ramp1=["filter"], r1Rate=.18, r1Depth=.3, r1Sync="2 bars")),
    ("Drones", "Cathedral Floor", "Huge and dark, with a very long echo", dict(source="Warm Pad", pitch=-12, position=.3, scatter=.08, density=.7, size=.88, attack=.65, release=.85, reverbMix=.55, reverbSize=.9, reverbDecay=.9, reverbDamp=.4)),
    ("Drones", "Black Sun", "Slow and heavy with one bright edge", dict(source="Warm Pad", pitch=-24, position=.55, scatter=.05, density=.5, size=.95, attack=.7, release=.9, filterOn=True, filterCut=.5, filterRes=.55, reverbMix=.35, compOn=True, compAmt=.6)),
    ("Drones", "Iron Organ", "Guitar chords locked into a single organ-like tone", dict(source="Guitar Chords", pitch=-12, position=.3, scatter=.05, density=.85, size=.5, attack=.2, release=.5, cohereOn=True, cohereAmt=.85, spread=.4, compOn=True, compAmt=.5)),
    ("Drones", "Earth Tone", "A calm, round, low voice that just stays put", dict(source="Choir Ah", pitch=-12, position=.5, scatter=.03, density=.82, size=.7, attack=.5, release=.7, spread=.6, filterOn=True, filterCut=.5)),

    # ------------------------------------------------------------------------------------------------ Motion
    ("Motion", "Slow Scan", "The sound creeps through the sample and ends up somewhere else", dict(source="Warm Pad", position=.0, play="Travel", speed=.35, scatter=.1, density=.62, size=.6, attack=.3, release=.6, spread=.8, reverbMix=.3)),
    ("Motion", "Tape Rewind", "Races forward through the loop, then spins back", dict(source="Drum Loop", position=.0, play="Bounce", speed=.8, scatter=.04, density=.8, size=.3, attack=.01, release=.25, reverse=.4, spread=.7)),
    ("Motion", "Pendulum", "Swings gently back and forth through the voice", dict(source="Choir Ah", position=.0, play="Bounce", speed=.38, scatter=.1, density=.65, size=.58, attack=.35, release=.6, spread=.9, reverbMix=.35)),
    ("Motion", "Rising Tension", "Grains get faster, smaller and higher, building up", dict(source="Glass Bell", position=.1, scatter=.2, density=.55, size=.5, attack=.5, release=.6, ramp1=["density", "pitch"], r1Rate=.12, r1Depth=.7, r1Sync="4 bars", r1Shape="Saw", reverbMix=.35, spread=.9)),
    ("Motion", "Sunrise", "Light slowly spreading across the pad", dict(source="Warm Pad", position=.0, play="Travel", speed=.28, scatter=.15, density=.64, size=.65, attack=.7, release=.7, filterOn=True, filterCut=.35, ramp1=["filter"], r1Rate=.15, r1Depth=.6, r1Sync="4 bars", r1Shape="Saw", reverbMix=.35)),
    ("Motion", "Wandering Voice", "A voice that wanders off and comes back", dict(source="Choir Ah", pitch=-5, position=.6, scatter=.15, density=.62, size=.5, attack=.25, release=.55, spread=.9, filterOn=True, filterType="Band-pass", filterCut=.5, filterRes=.4, ramp1=["position"], r1Rate=.35, r1Depth=.4, r1Shape="Random", r1Sync="1 bar", ramp2=["spread", "filter"], r2Rate=.2, r2Depth=.5, r2Sync="2 bars", reverbMix=.3)),

    # ------------------------------------------------------------------------------------------------ Synthy
    ("Synthy", "Choir Lead", "The choir turned into a smooth playable synth", dict(source="Choir Ah", position=.3, scatter=.04, density=.75, size=.4, attack=.1, release=.4, cohereOn=True, cohereAmt=.9, spread=.6, delayMix=.15, reverbMix=.2)),
    ("Synthy", "Glass Lead", "Clear bell tones played like a lead", dict(source="Glass Bell", position=.1, scatter=.02, density=.8, size=.35, attack=.03, release=.4, cohereOn=True, cohereAmt=.95, spread=.5, delayMix=.25, delayDiv="1/8", reverbMix=.2)),
    ("Synthy", "Pad Saw", "A buzzy, thick pad with an edge", dict(source="Warm Pad", position=.4, scatter=.05, density=.85, size=.35, attack=.3, release=.5, cohereOn=True, cohereAmt=.75, grainShape=.85, spread=.8, filterOn=True, filterCut=.62, reverbMix=.15)),
    ("Synthy", "Reed Voice", "Nasal and reedy, like an old accordion", dict(source="Choir Ah", position=.55, scatter=.03, density=.8, size=.28, attack=.12, release=.35, cohereOn=True, cohereAmt=.8, grainShape=.75, filterOn=True, filterType="Band-pass", filterCut=.55, filterRes=.5, compOn=True, compAmt=.4)),
    ("Synthy", "Breath Lead", "A single airy voice that follows what you play", dict(source="Choir Ah", position=.2, scatter=.1, density=.7, size=.32, attack=.15, release=.4, cohereOn=True, cohereAmt=.6, jitter=.04, spread=.5, reverbMix=.3)),
    ("Synthy", "Sync Bell", "A bright ringing tone with a little shimmer", dict(source="Glass Bell", pitch=12, position=.05, scatter=.03, density=.82, size=.3, attack=.02, release=.45, cohereOn=True, cohereAmt=.7, spread=.8, delayMix=.3, delayDiv="1/4", delayFb=.5, reverbMix=.3)),

    # ------------------------------------------------------------------------------------------------ Morph
    ("Morph", "Pad Into Bell", "A warm pad that slowly turns into glass", dict(source="Warm Pad", morphSource="Glass Bell", morphOn=True, morphAmt=.0, position=.5, scatter=.12, density=.5, size=.4, attack=.4, release=.6, spread=.9, delayMix=.15, ramp1=["morph"], r1Rate=.2, r1Depth=1.0, r1Sync="4 bars", r1Shape="Triangle", reverbMix=.3)),
    ("Morph", "Choir In Dust", "A voice with crackle growing through it", dict(source="Choir Ah", morphSource="Tape Dust", morphOn=True, morphAmt=.45, position=.4, scatter=.3, density=.7, size=.5, attack=.4, release=.6, spread=.9, reverbMix=.25)),
    ("Morph", "Guitar Through Glass", "Plucked chords with a shimmer of bell", dict(source="Guitar Chords", morphSource="Glass Bell", morphOn=True, morphAmt=.5, position=.3, scatter=.25, density=.72, size=.34, attack=.03, release=.45, spread=.9, delayMix=.2, reverbMix=.3)),
    ("Morph", "Drums Into Pad", "A beat blurred into something soft", dict(source="Drum Loop", morphSource="Warm Pad", morphOn=True, morphAmt=.6, position=.0, scatter=.15, density=.8, size=.45, attack=.1, release=.5, spread=.8, reverbMix=.3)),
    ("Morph", "Slow Blend", "Two voices trade places, very slowly", dict(source="Choir Ah", morphSource="Warm Pad", morphOn=True, morphAmt=.5, position=.4, scatter=.18, density=.62, size=.6, attack=.4, release=.65, spread=.85, ramp1=["morph"], r1Rate=.3, r1Depth=.9, r1Sync="4 bars", r1Shape="Sine", reverbMix=.35)),
    ("Morph", "Knob Blend", "The macro knob fades between two sounds", dict(source="Pluck Run", morphSource="Glass Bell", morphOn=True, morphAmt=.2, position=.3, scatter=.3, density=.6, size=.25, attack=.03, release=.45, spread=.9, macro1=["morph"], m1Value=.5, m1Depth=1.0, reverbMix=.3)),

    # ------------------------------------------------------------------------------------------------ Glitch
    ("Glitch", "Grain Storm", "Everything flying everywhere at once", dict(source="Tape Dust", position=.5, scatter=.9, density=.95, size=.1, jitter=1.0, spread=1.0, grainShape=.5, level=.7, reverbMix=.2)),
    ("Glitch", "Shredder", "Plucks played backwards and chewed up", dict(source="Pluck Run", position=.3, scatter=.7, density=.88, size=.12, reverse=1.0, jitter=.3, spread=1.0, grainShape=.7)),
    ("Glitch", "Tiny Robots", "A swarm of tiny bleeping voices", dict(source="Choir Ah", pitch=12, position=.5, scatter=.6, density=.92, size=.06, jitter=.5, scale="Whole tone", spread=1.0, grainShape=.8, delayMix=.2)),
    ("Glitch", "Broken Radio", "A signal fading in and out of range", dict(source="Choir Ah", position=.5, scatter=.8, density=.7, size=.25, jitter=.4, filterOn=True, filterType="High-pass", filterCut=.7, filterRes=.5, ramp1=["filter", "density"], r1Rate=.5, r1Depth=.5, r1Shape="Random", r1Sync="1/4 note")),
    ("Glitch", "Slice Machine", "Pulls the grains toward each drum hit", dict(source="Drum Loop", position=.0, scatter=.5, density=.78, size=.28, magnet=1.0, attack=.01, release=.2, spread=.8)),
    ("Glitch", "Digital Rain", "Falling pitches, each one stranger than the last", dict(source="Glass Bell", position=.2, scatter=.4, density=.6, size=.2, jitter=.8, scale="Blues", spread=1.0, ramp1=["pitch", "scatter"], r1Rate=.4, r1Depth=.7, r1Sync="1/2 note", r1Shape="Saw", delayMix=.3, delayDiv="1/16", delayFb=.55)),
    ("Glitch", "Glass Chew", "Bells caught in a crunchy squeeze", dict(source="Glass Bell", position=.15, scatter=.4, density=.85, size=.18, grainShape=.9, compOn=True, compAmt=.9, filterOn=True, filterCut=.7, filterRes=.5, spread=.8)),
]
