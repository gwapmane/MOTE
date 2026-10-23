#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace {
juce::ParameterID pid(const juce::String& id) { return juce::ParameterID{id, 1}; }
using Text = std::function<juce::String(float)>;

std::unique_ptr<juce::AudioParameterFloat> knob(const juce::String& id, const juce::String& name, float def, Text text = {}) {
    return std::make_unique<juce::AudioParameterFloat>(
        pid(id), name, juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f), def,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([text](float v, int) {
            return text ? text(v) : juce::String(juce::roundToInt(v * 100.0f)) + " %";
        }));
}

std::unique_ptr<juce::AudioParameterBool> toggle(const juce::String& id, const juce::String& name, bool def) {
    return std::make_unique<juce::AudioParameterBool>(pid(id), name, def);
}

std::unique_ptr<juce::AudioParameterChoice> choice(const juce::String& id, const juce::String& name, juce::StringArray names, int def) {
    return std::make_unique<juce::AudioParameterChoice>(pid(id), name, names, def);
}

juce::String seconds(float s) { return s < 1.0f ? juce::String(juce::roundToInt(s * 1000.0f)) + " ms" : juce::String(s, 2) + " s"; }

const char* const kModuleNames[mote::kNumTargets] = {"Position", "Scatter", "Density", "Size", "Pitch", "Jitter", "Spread", "Filter", "Morph"};

juce::StringArray sourceNames() {
    juce::StringArray a;
    for (int i = 0; i <= mote::sources::kCount; ++i) a.add(mote::sources::name(i));
    return a;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout MoteProcessor::createLayout() {
    using namespace mote;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ---- the knobs ----
    layout.add(knob(ids::position, "Position", 0.30f));
    layout.add(knob(ids::scatter, "Scatter", 0.15f, [](float v) { return juce::String(juce::roundToInt(map::scatterFrac(v) * 200.0f)) + " %"; }));
    layout.add(knob(ids::density, "Density", 0.55f, [](float v) { return juce::String(map::densityHz(v), 1) + " /s"; }));
    layout.add(knob(ids::size, "Size", 0.50f, [](float v) { return seconds(map::sizeMs(v) * 0.001f); }));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        pid(ids::pitch), "Pitch", juce::NormalisableRange<float>(-24.0f, 24.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("st").withStringFromValueFunction([](float v, int) {
            return (v > 0.0f ? "+" : "") + juce::String(juce::roundToInt(v)) + " st";
        })));
    layout.add(knob(ids::jitter, "Jitter", 0.0f, [](float v) { return "+-" + juce::String(map::jitterSemis(v), 1) + " st"; }));
    layout.add(knob(ids::spread, "Spread", 0.5f));
    layout.add(knob(ids::speed, "Speed", 0.5f, [](float v) { return juce::String(map::speedMul(v), 2) + "x"; }));
    layout.add(knob(ids::magnet, "Magnet", 0.0f));
    juce::StringArray notes, scales, walks, divs, ftypes, shapes, syncs;
    for (int i = 0; i < 12; ++i) notes.add(noteName(i));
    for (int i = 0; i < kNumScales; ++i) scales.add(scaleName(i));
    for (int i = 0; i < kNumWalk; ++i) walks.add(walkName(i));
    for (int i = 0; i < kNumDivs; ++i) divs.add(divName(i));
    for (int i = 0; i < kNumFilterTypes; ++i) ftypes.add(filterTypeName(i));
    for (int i = 0; i < kNumRampShapes; ++i) shapes.add(rampShapeName(i));
    for (int i = 0; i < kNumSyncs; ++i) syncs.add(syncName(i));
    layout.add(choice(ids::root, "Key", notes, 0));
    layout.add(choice(ids::scale, "Scale", scales, 0));
    layout.add(knob(ids::attack, "Attack", 0.15f, [](float v) { return seconds(map::attackSec(v)); }));
    layout.add(knob(ids::decay, "Decay", 0.40f, [](float v) { return seconds(map::decaySec(v)); }));
    layout.add(knob(ids::sustain, "Sustain", 0.85f));
    layout.add(knob(ids::release, "Release", 0.45f, [](float v) { return seconds(map::releaseSec(v)); }));
    layout.add(std::make_unique<juce::AudioParameterInt>(pid(ids::voices), "Voices", 1, 16, 8));
    layout.add(knob(ids::level, "Level", 0.87f, [](float v) { return juce::String(map::levelDb(v), 1) + " dB"; }));
    layout.add(knob(ids::delayMix, "Delay", 0.0f));
    layout.add(knob(ids::reverbMix, "Reverb", 0.0f));

    // ---- levers ----
    layout.add(choice(ids::play, "Play mode", {"Hold", "Travel", "Bounce"}, 0));
    layout.add(toggle(ids::morphOn, "Morph on", false));
    layout.add(toggle(ids::cohereOn, "Cohere on", false));
    layout.add(toggle(ids::walkOn, "Walk on", false));
    layout.add(toggle(ids::repeatOn, "Repeat on", false));
    layout.add(toggle(ids::filterOn, "Filter on", false));
    layout.add(toggle(ids::compOn, "Comp on", false));
    layout.add(knob(ids::morphAmt, "Morph", 0.5f));
    layout.add(knob(ids::cohereAmt, "Cohere", 0.7f));
    layout.add(choice(ids::walkPattern, "Walk pattern", walks, 0));
    layout.add(choice(ids::repeatDiv, "Repeat length", divs, 1));
    layout.add(knob(ids::filterCut, "Filter cutoff", 0.6f, [](float v) { return juce::String(juce::roundToInt(map::cutoffHz(v))) + " Hz"; }));
    layout.add(knob(ids::compAmt, "Comp amount", 0.5f));

    // ---- footswitches ----
    layout.add(toggle(ids::power, "On", true));
    layout.add(toggle(ids::latch, "Latch", false));
    layout.add(toggle(ids::latchHold, "Latch (hold)", false));

    // ---- under the hood ----
    layout.add(choice(ids::source, "Sample", sourceNames(), 1));
    layout.add(choice(ids::morphSource, "Morph sample", sourceNames(), 3));
    layout.add(choice(ids::sampleKey, "Sample's own note", notes, 9));
    layout.add(knob(ids::grainShape, "Grain edges", 0.35f));
    layout.add(knob(ids::reverse, "Reverse chance", 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        pid(ids::tune), "Fine tune", juce::NormalisableRange<float>(-50.0f, 50.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withLabel("cents")));
    layout.add(choice(ids::filterType, "Filter type", ftypes, 0));
    layout.add(knob(ids::filterRes, "Filter resonance", 0.3f));
    layout.add(knob(ids::filterSpread, "Filter spread", 0.25f));
    layout.add(knob(ids::filterKey, "Filter key-follow", 0.0f));
    layout.add(choice(ids::walkRate, "Walk step", divs, 3));
    layout.add(std::make_unique<juce::AudioParameterInt>(pid(ids::walkSteps), "Walk steps", 2, 16, 8));
    layout.add(knob(ids::walkGate, "Walk gate", 0.85f));
    layout.add(knob(ids::repeatDecay, "Repeat fade", 1.0f));
    layout.add(choice(ids::delayDiv, "Delay time", divs, 2));
    layout.add(knob(ids::delayFb, "Delay feedback", 0.4f));
    layout.add(knob(ids::delayTone, "Delay tone", 0.6f));
    layout.add(knob(ids::reverbSize, "Reverb size", 0.5f));
    layout.add(knob(ids::reverbDecay, "Reverb decay", 0.55f));
    layout.add(knob(ids::reverbDamp, "Reverb damping", 0.5f));
    layout.add(toggle(ids::trails, "Trails", true));

    // ---- ramps and macros ----
    const char* rid[2][4] = {{ids::r1Rate, ids::r1Depth, ids::r1Shape, ids::r1Sync}, {ids::r2Rate, ids::r2Depth, ids::r2Shape, ids::r2Sync}};
    for (int r = 0; r < 2; ++r) {
        const juce::String n = "Ramp " + juce::String(r + 1);
        layout.add(knob(rid[r][0], n + " rate", 0.3f, [](float v) { return juce::String(map::rampHz(v), 2) + " Hz"; }));
        layout.add(knob(rid[r][1], n + " depth", 0.0f));
        layout.add(choice(rid[r][2], n + " shape", shapes, 0));
        layout.add(choice(rid[r][3], n + " lock to tempo", syncs, 0));
        for (int t = 0; t < kNumTargets; ++t) layout.add(toggle("r" + juce::String(r + 1) + "t" + juce::String(t), n + " > " + kModuleNames[t], false));
    }
    const char* mid[2][2] = {{ids::m1Value, ids::m1Depth}, {ids::m2Value, ids::m2Depth}};
    for (int k = 0; k < 2; ++k) {
        const juce::String n = "Macro " + juce::String(k + 1);
        layout.add(knob(mid[k][0], n, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(pid(mid[k][1]), n + " depth", juce::NormalisableRange<float>(-1.0f, 1.0f, 0.0f), 0.5f,
                                                               juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; })));
        for (int t = 0; t < kNumTargets; ++t) layout.add(toggle("m" + juce::String(k + 1) + "t" + juce::String(t), n + " > " + kModuleNames[t], false));
    }
    layout.add(std::make_unique<juce::AudioParameterFloat>(pid(ids::manualBpm), "Tempo (if no DAW)", juce::NormalisableRange<float>(40.0f, 240.0f, 0.1f), 120.0f,
                                                           juce::AudioParameterFloatAttributes().withLabel("BPM")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(pid(ids::outputGain), "Output Gain", juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f,
                                                           juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + " dB"; })));
    return layout;
}

// the parameters the audio thread reads, found once
struct MoteProcessor::Raw {
    std::atomic<float> *position, *scatter, *density, *size, *pitch, *jitter, *spread, *speed, *magnet, *root, *scale, *attack, *decay, *sustain,
        *release, *voices, *level, *delayMix, *reverbMix, *play, *morphOn, *cohereOn, *walkOn, *repeatOn, *filterOn, *compOn, *morphAmt,
        *cohereAmt, *walkPattern, *repeatDiv, *filterCut, *compAmt, *power, *latch, *latchHold, *sampleKey, *grainShape, *reverse, *tune,
        *filterType, *filterRes, *filterSpread, *filterKey, *walkRate, *walkSteps, *walkGate, *repeatDecay, *delayDiv, *delayFb, *delayTone,
        *reverbSize, *reverbDecay, *reverbDamp, *trails, *manualBpm, *outputGain, *source, *morphSource;
    std::atomic<float> *rRate[2], *rDepth[2], *rShape[2], *rSync[2], *rT[2][mote::kNumTargets];
    std::atomic<float> *mValue[2], *mDepth[2], *mT[2][mote::kNumTargets];
};

MoteProcessor::MoteProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "MOTE", createLayout()), raw_(std::make_unique<Raw>()) {
    auto get = [this](const juce::String& id) { return apvts.getRawParameterValue(id); };
    auto& r = *raw_;
#define MOTE_GET(n) r.n = get(ids::n);
    MOTE_GET(position) MOTE_GET(scatter) MOTE_GET(density) MOTE_GET(size) MOTE_GET(pitch) MOTE_GET(jitter) MOTE_GET(spread) MOTE_GET(speed)
    MOTE_GET(magnet) MOTE_GET(root) MOTE_GET(scale) MOTE_GET(attack) MOTE_GET(decay) MOTE_GET(sustain) MOTE_GET(release) MOTE_GET(voices)
    MOTE_GET(level) MOTE_GET(delayMix) MOTE_GET(reverbMix) MOTE_GET(play) MOTE_GET(morphOn) MOTE_GET(cohereOn) MOTE_GET(walkOn)
    MOTE_GET(repeatOn) MOTE_GET(filterOn) MOTE_GET(compOn) MOTE_GET(morphAmt) MOTE_GET(cohereAmt) MOTE_GET(walkPattern) MOTE_GET(repeatDiv)
    MOTE_GET(filterCut) MOTE_GET(compAmt) MOTE_GET(power) MOTE_GET(latch) MOTE_GET(latchHold) MOTE_GET(sampleKey) MOTE_GET(grainShape)
    MOTE_GET(reverse) MOTE_GET(tune) MOTE_GET(filterType) MOTE_GET(filterRes) MOTE_GET(filterSpread) MOTE_GET(filterKey) MOTE_GET(walkRate)
    MOTE_GET(walkSteps) MOTE_GET(walkGate) MOTE_GET(repeatDecay) MOTE_GET(delayDiv) MOTE_GET(delayFb) MOTE_GET(delayTone) MOTE_GET(reverbSize)
    MOTE_GET(reverbDecay) MOTE_GET(reverbDamp) MOTE_GET(trails) MOTE_GET(manualBpm) MOTE_GET(outputGain) MOTE_GET(source) MOTE_GET(morphSource)
#undef MOTE_GET
    const char* rid[2][4] = {{ids::r1Rate, ids::r1Depth, ids::r1Shape, ids::r1Sync}, {ids::r2Rate, ids::r2Depth, ids::r2Shape, ids::r2Sync}};
    const char* mid[2][2] = {{ids::m1Value, ids::m1Depth}, {ids::m2Value, ids::m2Depth}};
    for (int k = 0; k < 2; ++k) {
        r.rRate[k] = get(rid[k][0]); r.rDepth[k] = get(rid[k][1]); r.rShape[k] = get(rid[k][2]); r.rSync[k] = get(rid[k][3]);
        r.mValue[k] = get(mid[k][0]); r.mDepth[k] = get(mid[k][1]);
        for (int t = 0; t < mote::kNumTargets; ++t) {
            r.rT[k][t] = get("r" + juce::String(k + 1) + "t" + juce::String(t));
            r.mT[k][t] = get("m" + juce::String(k + 1) + "t" + juce::String(t));
        }
    }
    syncSources();
    startTimerHz(20);
}

MoteProcessor::~MoteProcessor() { stopTimer(); }

// ---------------------------------------------------------------------------------------------- samples
mote::SamplePtr MoteProcessor::builtin(int id) {
    static std::mutex m;
    static std::array<mote::SamplePtr, mote::sources::kCount> cache;
    id = juce::jlimit(0, mote::sources::kCount - 1, id);
    std::lock_guard<std::mutex> lock(m);
    auto& slot = cache[static_cast<size_t>(id)];
    if (slot == nullptr) slot = mote::sources::make(id, 48000.0);
    return slot;
}

mote::SamplePtr MoteProcessor::sampleA() const { std::lock_guard<std::mutex> l(sampleLock_); return curA_; }
mote::SamplePtr MoteProcessor::sampleB() const { std::lock_guard<std::mutex> l(sampleLock_); return curB_; }
bool MoteProcessor::hasCustom(bool b) const { std::lock_guard<std::mutex> l(sampleLock_); return (b ? customB_ : customA_) != nullptr; }
juce::String MoteProcessor::customName(bool b) const {
    std::lock_guard<std::mutex> l(sampleLock_);
    const auto& s = b ? customB_ : customA_;
    return s ? juce::String(s->name) : juce::String();
}

void MoteProcessor::publish(mote::SamplePtr a, mote::SamplePtr b) {
    std::lock_guard<std::mutex> l(sampleLock_);
    const auto now = juce::Time::currentTimeMillis();
    if (a != curA_) { if (curA_) retired_.push_back({curA_, now}); curA_ = a; }
    if (b != curB_) { if (curB_) retired_.push_back({curB_, now}); curB_ = b; }
    pubA_.store(curA_.get());
    pubB_.store(curB_.get());
}

void MoteProcessor::syncSources() {
    const int a = juce::jlimit(0, mote::sources::kCount, juce::roundToInt(raw_->source->load()));
    const int b = juce::jlimit(0, mote::sources::kCount, juce::roundToInt(raw_->morphSource->load()));
    mote::SamplePtr sa, sb;
    {
        std::lock_guard<std::mutex> l(sampleLock_);
        sa = a < mote::sources::kCount ? nullptr : customA_;
        sb = b < mote::sources::kCount ? nullptr : customB_;
    }
    if (a < mote::sources::kCount) sa = builtin(a);
    if (sa == nullptr) sa = builtin(1);
    if (b < mote::sources::kCount) sb = builtin(b);
    if (sb == nullptr) sb = builtin(3);
    mote::SamplePtr ca, cb;
    {
        std::lock_guard<std::mutex> l(sampleLock_);
        ca = curA_; cb = curB_;
    }
    if (sa != ca || sb != cb) publish(sa, sb);
    shownSrcA_ = a;
    shownSrcB_ = b;
}

void MoteProcessor::timerCallback() {
    const int a = juce::roundToInt(raw_->source->load()), b = juce::roundToInt(raw_->morphSource->load());
    if (a != shownSrcA_ || b != shownSrcB_) syncSources();
    // the host may not call us while nothing plays: then the window follows the knobs directly
    const double clockNow = engine_.now();
    if (clockNow == lastClock_) engine_.showIdle(readParams());
    lastClock_ = clockNow;
    // let go of samples nobody can be reading any more
    std::lock_guard<std::mutex> l(sampleLock_);
    const auto now = juce::Time::currentTimeMillis();
    retired_.erase(std::remove_if(retired_.begin(), retired_.end(), [now](const Retired& r) { return now - r.at > 8000; }), retired_.end());
}

void MoteProcessor::setSource(int index, bool slotB) {
    index = juce::jlimit(0, mote::sources::kCount, index);
    if (auto* p = apvts.getParameter(slotB ? ids::morphSource : ids::source)) {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(index)));
        p->endChangeGesture();
    }
    if (!slotB && index < mote::sources::kCount)
        if (auto* k = apvts.getParameter(ids::sampleKey)) k->setValueNotifyingHost(k->convertTo0to1(static_cast<float>(mote::sources::keyOf(index))));
    syncSources();
}

mote::SamplePtr MoteProcessor::decodeFile(const juce::File& f, juce::String& error) {
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(f));
    if (reader == nullptr) { error = "That file could not be read. WAV, AIFF, FLAC, OGG and MP3 work."; return nullptr; }
    const double sr = reader->sampleRate;
    if (sr < 8000.0 || sr > 384000.0 || reader->lengthInSamples < 64) { error = "That file is empty or has an odd sample rate."; return nullptr; }
    const juce::int64 maxLen = static_cast<juce::int64>(sr * 40.0);
    const bool trimmed = reader->lengthInSamples > maxLen;
    const int n = static_cast<int>(std::min<juce::int64>(reader->lengthInSamples, maxLen));
    const int ch = static_cast<int>(std::min<unsigned int>(2u, reader->numChannels));
    juce::AudioBuffer<float> buf(ch, n);
    reader->read(&buf, 0, n, 0, true, ch > 1);
    std::vector<float> l(buf.getReadPointer(0), buf.getReadPointer(0) + n), r;
    if (ch > 1) r.assign(buf.getReadPointer(1), buf.getReadPointer(1) + n);
    notice_ = trimmed ? "Trimmed to the first 40 seconds." : juce::String();
    return mote::sampleutil::make(f.getFileNameWithoutExtension().toStdString(), sr, std::move(l), std::move(r));
}

juce::String MoteProcessor::loadSampleFile(const juce::File& f, bool slotB) {
    juce::String err;
    auto s = decodeFile(f, err);
    if (s == nullptr) return err;
    {
        std::lock_guard<std::mutex> l(sampleLock_);
        (slotB ? customB_ : customA_) = s;
    }
    saveCustomToState();
    setSource(mote::sources::kCustom, slotB);
    syncSources();
    return {};
}

void MoteProcessor::clearCustom(bool slotB) {
    {
        std::lock_guard<std::mutex> l(sampleLock_);
        (slotB ? customB_ : customA_) = nullptr;
    }
    saveCustomToState();
    if (static_cast<int>(juce::roundToInt((slotB ? raw_->morphSource : raw_->source)->load())) == mote::sources::kCustom) setSource(slotB ? 3 : 1, slotB);
    else syncSources();
}

// the user's own samples travel with the project: 16-bit, base64, inside the plugin state
void MoteProcessor::saveCustomToState() {
    for (int slot = 0; slot < 2; ++slot) {
        const juce::String key = slot == 0 ? "customA" : "customB";
        mote::SamplePtr s;
        {
            std::lock_guard<std::mutex> l(sampleLock_);
            s = slot == 0 ? customA_ : customB_;
        }
        if (s == nullptr) {
            apvts.state.removeProperty(key, nullptr);
            apvts.state.removeProperty(key + "Info", nullptr);
            continue;
        }
        const int n = s->size();
        const int ch = s->stereo ? 2 : 1;
        juce::MemoryBlock mb(static_cast<size_t>(n) * static_cast<size_t>(ch) * 2u);
        auto* d = static_cast<int16_t*>(mb.getData());
        for (int i = 0; i < n; ++i) {
            d[i * ch] = static_cast<int16_t>(juce::jlimit(-32767.0f, 32767.0f, s->l[static_cast<size_t>(i)] * 32767.0f));
            if (ch == 2) d[i * ch + 1] = static_cast<int16_t>(juce::jlimit(-32767.0f, 32767.0f, s->r[static_cast<size_t>(i)] * 32767.0f));
        }
        apvts.state.setProperty(key, juce::Base64::toBase64(mb.getData(), mb.getSize()), nullptr);
        apvts.state.setProperty(key + "Info", juce::String(s->name) + "|" + juce::String(s->sr, 1) + "|" + juce::String(ch), nullptr);
    }
}

void MoteProcessor::loadCustomFromState() {
    for (int slot = 0; slot < 2; ++slot) {
        const juce::String key = slot == 0 ? "customA" : "customB";
        mote::SamplePtr made;
        const auto text = apvts.state.getProperty(key).toString();
        const auto info = juce::StringArray::fromTokens(apvts.state.getProperty(key + "Info").toString(), "|", "");
        if (text.isNotEmpty() && info.size() == 3) {
            juce::MemoryOutputStream out;
            if (juce::Base64::convertFromBase64(out, text) && out.getDataSize() >= 4) {
                const int ch = juce::jlimit(1, 2, info[2].getIntValue());
                const int n = static_cast<int>(out.getDataSize() / (2u * static_cast<size_t>(ch)));
                const auto* d = static_cast<const int16_t*>(out.getData());
                std::vector<float> l(static_cast<size_t>(n)), r;
                if (ch == 2) r.resize(static_cast<size_t>(n));
                for (int i = 0; i < n; ++i) {
                    l[static_cast<size_t>(i)] = static_cast<float>(d[i * ch]) / 32767.0f;
                    if (ch == 2) r[static_cast<size_t>(i)] = static_cast<float>(d[i * ch + 1]) / 32767.0f;
                }
                made = mote::sampleutil::make(info[0].toStdString(), info[1].getDoubleValue(), std::move(l), std::move(r));
            }
        }
        std::lock_guard<std::mutex> l(sampleLock_);
        (slot == 0 ? customA_ : customB_) = made;
    }
}

// ---------------------------------------------------------------------------------------------- audio
void MoteProcessor::prepareToPlay(double sampleRate, int) {
    sampleRate_ = sampleRate;
    engine_.prepare(sampleRate);
    engine_.setSamples(pubA_.load(), pubB_.load());
    engine_.setParams(readParams());
    vizLpLo_ = vizLpHi_ = vizEnvLo_ = vizEnvMid_ = vizEnvHi_ = 0.0f;
    lastAudition_ = -1;
    sustainPedal_ = false;
}

bool MoteProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

mote::Params MoteProcessor::readParams() const {
    using namespace mote;
    const auto& r = *raw_;
    auto idx = [](std::atomic<float>* a) { return static_cast<int>(a->load() + 0.5f); };
    Params p;
    p.position = r.position->load(); p.scatter = r.scatter->load(); p.density = r.density->load(); p.size = r.size->load();
    p.pitchSemis = r.pitch->load(); p.jitter = r.jitter->load(); p.spread = r.spread->load();
    p.speed = r.speed->load(); p.magnet = r.magnet->load();
    p.root = idx(r.root); p.scale = idx(r.scale); p.sampleKey = idx(r.sampleKey);
    p.attack = r.attack->load(); p.decay = r.decay->load(); p.sustain = r.sustain->load(); p.release = r.release->load();
    p.voices = idx(r.voices);
    p.level = r.level->load(); p.delayMix = r.delayMix->load(); p.reverbMix = r.reverbMix->load();
    p.play = idx(r.play);
    p.morphOn = r.morphOn->load() > 0.5f; p.cohereOn = r.cohereOn->load() > 0.5f; p.walkOn = r.walkOn->load() > 0.5f;
    p.repeatOn = r.repeatOn->load() > 0.5f; p.filterOn = r.filterOn->load() > 0.5f; p.compOn = r.compOn->load() > 0.5f;
    p.morph = r.morphAmt->load(); p.cohere = r.cohereAmt->load(); p.filter = r.filterCut->load(); p.compAmt = r.compAmt->load();
    p.walkPattern = idx(r.walkPattern); p.repeatDiv = idx(r.repeatDiv);
    p.grainShape = r.grainShape->load(); p.reverse = r.reverse->load(); p.tuneCents = r.tune->load();
    p.filterType = idx(r.filterType); p.filterRes = r.filterRes->load(); p.filterSpread = r.filterSpread->load(); p.filterKey = r.filterKey->load();
    p.walkRate = idx(r.walkRate); p.walkSteps = idx(r.walkSteps); p.walkGate = r.walkGate->load();
    p.repeatDecay = r.repeatDecay->load();
    p.delayDiv = idx(r.delayDiv); p.delayFb = r.delayFb->load(); p.delayTone = r.delayTone->load();
    p.reverbSize = r.reverbSize->load(); p.reverbDecay = r.reverbDecay->load(); p.reverbDamp = r.reverbDamp->load();
    p.on = r.power->load() > 0.5f;
    p.latch = r.latch->load() > 0.5f || r.latchHold->load() > 0.5f || sustainPedal_;
    p.trails = r.trails->load() > 0.5f;
    p.outputGainDb = r.outputGain->load();
    for (int k = 0; k < 2; ++k) {
        p.ramp[k].rate = r.rRate[k]->load(); p.ramp[k].depth = r.rDepth[k]->load();
        p.ramp[k].shape = idx(r.rShape[k]); p.ramp[k].sync = idx(r.rSync[k]);
        p.macro[k].value = r.mValue[k]->load(); p.macro[k].depth = r.mDepth[k]->load();
        for (int t = 0; t < kNumTargets; ++t) {
            if (r.rT[k][t]->load() > 0.5f) p.ramp[k].targets |= 1u << t;
            if (r.mT[k][t]->load() > 0.5f) p.macro[k].targets |= 1u << t;
        }
    }
    return p;
}

mote::Transport MoteProcessor::readTransport() {
    mote::Transport t;
    t.bpm = 0.0;
    bool fromHost = false;
    if (auto* ph = getPlayHead())
        if (const auto pos = ph->getPosition()) {
            if (const auto b = pos->getBpm(); b.hasValue() && *b > 1.0) { t.bpm = *b; fromHost = true; }
            if (const auto q = pos->getPpqPosition(); q.hasValue()) { t.hostValid = true; t.ppq = *q; }
            t.playing = pos->getIsPlaying();
            if (const auto ts = pos->getTimeSignature(); ts.hasValue() && ts->denominator > 0)
                t.beatsPerBar = juce::jlimit(1, 16, static_cast<int>(std::round(4.0 * ts->numerator / ts->denominator)));
        }
    if (!fromHost) t.bpm = raw_->manualBpm->load();
    t.bpm = std::round(juce::jlimit(20.0, 400.0, t.bpm) * 100.0) / 100.0;
    tempoBpm.store(static_cast<float>(t.bpm), std::memory_order_relaxed);
    tempoFromHost.store(fromHost, std::memory_order_relaxed);
    return t;
}

void MoteProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    const int numOut = getTotalNumOutputChannels();
    buffer.clear();

    engine_.setSamples(pubA_.load(), pubB_.load());
    // the sustain pedal and MIDI controllers are read first, so this block already plays with them
    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        if (m.isController()) {
            if (m.getControllerNumber() == 64) sustainPedal_ = m.getControllerValue() >= 64;
            handleCc(m.getControllerNumber(), m.getControllerValue());
        }
    }
    engine_.setParams(readParams());
    engine_.setTransport(readTransport());

    // the mouse in the window can hold a note
    const int aud = auditionNote_.load();
    if (aud != lastAudition_) {
        if (lastAudition_ >= 0) engine_.noteOff(lastAudition_);
        if (aud >= 0) engine_.noteOn(aud, 0.9f);
        lastAudition_ = aud;
    }

    float* outL = buffer.getWritePointer(0);
    float* outR = numOut > 1 ? buffer.getWritePointer(1) : nullptr;
    if (outR == nullptr) {
        monoScratch_.assign(static_cast<size_t>(total), 0.0f);
        outR = monoScratch_.data();
    }

    int pos = 0;
    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        const int t = juce::jlimit(0, total, meta.samplePosition);
        if (t > pos) { engine_.process(outL + pos, outR + pos, t - pos); pos = t; }
        if (m.isNoteOn()) engine_.noteOn(m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) engine_.noteOff(m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine_.allNotesOff();
        else if (m.isPitchWheel()) engine_.setPitchBend((static_cast<float>(m.getPitchWheelValue()) - 8192.0f) / 8192.0f * 2.0f);
    }
    if (total > pos) engine_.process(outL + pos, outR + pos, total - pos);
    if (numOut == 1) for (int i = 0; i < total; ++i) outL[i] = 0.5f * (outL[i] + outR[i]);

    // what the window listens to: loudness and the bass / mid / treble balance of the OUTPUT
    {
        const float aLo = 1.0f - std::exp(-6.2831853f * 250.0f / static_cast<float>(sampleRate_));
        const float aHi = 1.0f - std::exp(-6.2831853f * 2500.0f / static_cast<float>(sampleRate_));
        const float* l = buffer.getReadPointer(0);
        const float* r = numOut > 1 ? buffer.getReadPointer(1) : l;
        float sum = 0.0f, eLo = 0.0f, eMid = 0.0f, eHi = 0.0f;
        for (int i = 0; i < total; ++i) {
            const float mm = 0.5f * (l[i] + r[i]);
            vizLpLo_ += aLo * (mm - vizLpLo_);
            vizLpHi_ += aHi * (mm - vizLpHi_);
            const float lo = vizLpLo_, mid = vizLpHi_ - vizLpLo_, hi = mm - vizLpHi_;
            sum += mm * mm; eLo += lo * lo; eMid += mid * mid; eHi += hi * hi;
        }
        if (total > 0) {
            const float inv = 1.0f / static_cast<float>(total);
            const float rms = std::sqrt(sum * inv);
            const float level = juce::jlimit(0.0f, 1.0f, (20.0f * std::log10(rms + 1.0e-6f) + 50.0f) / 40.0f);
            float cur = visLevel.load(std::memory_order_relaxed);
            while (level > cur && !visLevel.compare_exchange_weak(cur, level, std::memory_order_relaxed)) {}
            const float k = juce::jlimit(0.0f, 1.0f, static_cast<float>(total) / static_cast<float>(sampleRate_) / 0.15f);
            vizEnvLo_ += (eLo * inv - vizEnvLo_) * k;
            vizEnvMid_ += (eMid * inv - vizEnvMid_) * k;
            vizEnvHi_ += (eHi * inv - vizEnvHi_) * k;
            visLo.store(vizEnvLo_, std::memory_order_relaxed);
            visMid.store(vizEnvMid_, std::memory_order_relaxed);
            visHi.store(vizEnvHi_, std::memory_order_relaxed);
        }
    }
    uiBeat.store(engine_.beat(), std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------------------------- MIDI control
float MoteProcessor::ccToNormalised(const juce::RangedAudioParameter& p, int value) {
    const float x = static_cast<float>(juce::jlimit(0, 127, value)) / 127.0f;
    if (p.isDiscrete()) {
        const int n = p.getNumSteps();
        if (n > 1) return static_cast<float>(juce::jmin(n - 1, static_cast<int>(x * static_cast<float>(n)))) / static_cast<float>(n - 1);
    }
    return x;
}

void MoteProcessor::handleCc(int cc, int value) {
    if (cc < 0 || cc >= 120) return;
    if (auto* target = learnTarget_.exchange(nullptr)) {
        for (auto& slot : ccMap_)
            if (slot.load() == target) slot.store(nullptr);
        ccMap_[static_cast<size_t>(cc)].store(target);
        learnedParam.store(target);
        learnedCc.store(cc);
        learnedCounter.fetch_add(1);
    }
    if (auto* p = ccMap_[static_cast<size_t>(cc)].load()) p->setValueNotifyingHost(ccToNormalised(*p, value));
}

int MoteProcessor::midiCcOf(const juce::RangedAudioParameter* p) const {
    for (size_t cc = 0; cc < ccMap_.size(); ++cc)
        if (ccMap_[cc].load() == p) return static_cast<int>(cc);
    return -1;
}

void MoteProcessor::clearMidiMapping(const juce::RangedAudioParameter* p) {
    for (auto& slot : ccMap_)
        if (slot.load() == p) slot.store(nullptr);
}

void MoteProcessor::clearAllMidiMappings() {
    for (auto& slot : ccMap_) slot.store(nullptr);
}

juce::String MoteProcessor::midiMappingSummary() const {
    juce::String s;
    for (size_t cc = 0; cc < ccMap_.size(); ++cc)
        if (auto* p = ccMap_[cc].load()) s << "CC " << static_cast<int>(cc) << "  =  " << p->getName(40) << "\n";
    return s.isEmpty() ? juce::String("No MIDI mappings yet.") : s.trimEnd();
}

void MoteProcessor::saveMidiMap() {
    juce::StringArray parts;
    for (size_t cc = 0; cc < ccMap_.size(); ++cc)
        if (auto* p = ccMap_[cc].load()) parts.add(juce::String(static_cast<int>(cc)) + "=" + p->paramID);
    apvts.state.setProperty("midiMap", parts.joinIntoString(","), nullptr);
}

void MoteProcessor::loadMidiMap() {
    clearAllMidiMappings();
    for (auto& token : juce::StringArray::fromTokens(apvts.state.getProperty("midiMap").toString(), ",", "")) {
        const int cc = token.upToFirstOccurrenceOf("=", false, false).getIntValue();
        if (cc < 0 || cc >= 120) continue;
        if (auto* p = apvts.getParameter(token.fromFirstOccurrenceOf("=", false, false))) ccMap_[static_cast<size_t>(cc)].store(p);
    }
}

juce::AudioProcessorEditor* MoteProcessor::createEditor() { return new MoteEditor(*this); }

void MoteProcessor::getStateInformation(juce::MemoryBlock& destData) {
    saveMidiMap();
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, destData);
}

void MoteProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType())) {
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            loadMidiMap();
            loadCustomFromState();
            syncSources();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MoteProcessor(); }
