#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <mutex>
#include <vector>

#include "../core/grain.h"
#include "../core/sources.h"
#include "ParamIds.h"
#include "PresetManager.h"

class MoteProcessor : public juce::AudioProcessor, private juce::Timer {
public:
    MoteProcessor();
    ~MoteProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MOTE"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets{apvts}; // factory + user presets and the A / LIVE / B slots (state lives in apvts.state)

    // ---- samples ----
    // The sample the window shows (message thread). Built-in sounds are made once, on first use.
    mote::SamplePtr sampleA() const;
    mote::SamplePtr sampleB() const;
    // Loads a sound file into the "your sample" slot (A = the main one, B = the one MORPH blends in). Returns an error
    // text, or an empty string. Also switches the source to "Your sample".
    juce::String loadSampleFile(const juce::File& f, bool slotB);
    void clearCustom(bool slotB);
    bool hasCustom(bool slotB) const;
    juce::String customName(bool slotB) const;
    juce::String sampleNotice() const { return notice_; } // e.g. "Trimmed to the first 40 seconds"
    void setSource(int index, bool slotB); // 0..6 built in, 7 = your sample (also tunes "sample key" for built-ins)

    // ---- play from the window: the mouse holds a note ----
    void auditionOn(int note) { auditionNote_.store(note); }
    void auditionOff() { auditionNote_.store(-1); }
    int auditionNote() const { return auditionNote_.load(); }

    // What the window listens to. Written by the audio thread, read by the editor ~30 times a second.
    //   visLevel: loudest block level since the last read (0..1, -50 dB .. -10 dB)
    //   visLo / visMid / visHi: smoothed energy below 250 Hz, 250 Hz..2.5 kHz and above 2.5 kHz
    std::atomic<float> visLevel{0.0f}, visLo{0.0f}, visMid{0.0f}, visHi{0.0f};
    std::atomic<double> uiBeat{0.0};
    const mote::Engine& engine() const { return engine_; }

    // ---- MIDI control: any MIDI CC can drive any parameter ("MIDI learn"). Saved with the project. ----
    void startMidiLearn(juce::RangedAudioParameter* p) { learnTarget_.store(p); }
    void cancelMidiLearn() { learnTarget_.store(nullptr); }
    juce::RangedAudioParameter* midiLearnTarget() const { return learnTarget_.load(); }
    int midiCcOf(const juce::RangedAudioParameter* p) const; // -1 when none
    void clearMidiMapping(const juce::RangedAudioParameter* p);
    void clearAllMidiMappings();
    juce::String midiMappingSummary() const;
    std::atomic<int> learnedCounter{0}, learnedCc{-1};
    std::atomic<juce::RangedAudioParameter*> learnedParam{nullptr};

    // ---- tempo: what the audio thread is following (the Sound tab shows it) ----
    std::atomic<float> tempoBpm{120.0f};
    std::atomic<bool> tempoFromHost{false};

    static mote::SamplePtr builtin(int id); // made on first use, shared by every instance

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    mote::Params readParams() const;
    mote::Transport readTransport();
    void handleCc(int cc, int value);
    static float ccToNormalised(const juce::RangedAudioParameter& p, int value);
    void saveMidiMap();
    void loadMidiMap();

    void timerCallback() override;
    void syncSources();
    void publish(mote::SamplePtr a, mote::SamplePtr b);
    void saveCustomToState();
    void loadCustomFromState();
    mote::SamplePtr decodeFile(const juce::File& f, juce::String& error);

    std::array<std::atomic<juce::RangedAudioParameter*>, 128> ccMap_{};
    std::atomic<juce::RangedAudioParameter*> learnTarget_{nullptr};

    mote::Engine engine_;
    float vizLpLo_ = 0.0f, vizLpHi_ = 0.0f, vizEnvLo_ = 0.0f, vizEnvMid_ = 0.0f, vizEnvHi_ = 0.0f;
    double sampleRate_ = 44100.0;
    bool sustainPedal_ = false;
    std::atomic<int> auditionNote_{-1};
    int lastAudition_ = -1;
    double lastClock_ = -1.0;
    std::vector<float> monoScratch_;

    // samples: the message thread owns these; the audio thread only reads the two raw pointers
    mutable std::mutex sampleLock_;
    mote::SamplePtr curA_, curB_, customA_, customB_;
    std::atomic<const mote::Sample*> pubA_{nullptr}, pubB_{nullptr};
    struct Retired { mote::SamplePtr s; juce::int64 at; };
    std::vector<Retired> retired_; // kept alive for a few seconds after a swap, so grains still reading them finish
    juce::String notice_;
    int shownSrcA_ = -1, shownSrcB_ = -1;
    juce::String stateKeyShown_;

    struct Raw;
    std::unique_ptr<Raw> raw_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MoteProcessor)
};
