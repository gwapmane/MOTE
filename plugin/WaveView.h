// MOTE - the wide window in the middle of the pedal. It shows the sample you are playing as a waveform, and everything
// the grain engine is doing to it, live: the position marker and its scatter band, every grain as it fires (higher up =
// pitched higher), the play-heads of held notes, the slices WALK steps through, the second sample MORPH blends in.
// Drag a sound file onto it (or click the name in the corner) to play your own sample. Drag across it to move Position.
// A small keyboard along the bottom lets you play the sound with the mouse. A video, GIF or picture of your own can sit
// behind the waveform: it brightens and pulses with the sound.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

#include "PluginProcessor.h"
#include "VideoLoop.h"

class WaveView : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer {
public:
    static constexpr float kW = 1020.0f, kH = 242.0f; // design size (drawn at this size, then scaled)

    explicit WaveView(MoteProcessor& p);
    ~WaveView() override;

    void setLook(juce::Colour accent, juce::Colour deep);
    void setClip(VideoClipPtr clip);
    void setStill(bool still) { still_ = still; } // for the test harness
    float energy() const { return energy_; }       // how loud the output is, 0..1 (smoothed)
    float grainActivity() const { return grains_; }

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray&, int, int) override { dropHover_ = true; repaint(); }
    void fileDragExit(const juce::StringArray&) override { dropHover_ = false; repaint(); }
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // a sample file chosen from outside (the Sound panel's buttons); shows any error in the window
    void loadFile(const juce::File& f, bool slotB);
    void chooseFile(bool slotB);

private:
    void timerCallback() override;
    void readMeters();
    void rebuildWave();
    void showSampleMenu(bool slotB);
    void say(const juce::String& text);
    int keyAt(juce::Point<float> p) const; // MIDI note under a design-space point, or -1
    juce::Rectangle<float> waveArea() const { return {14.0f, 30.0f, 992.0f, 168.0f}; }
    juce::Rectangle<float> keysArea() const { return {150.0f, 206.0f, 720.0f, 28.0f}; }
    juce::Rectangle<float> nameChip() const { return {12.0f, 6.0f, 330.0f, 22.0f}; }
    juce::Rectangle<float> morphChip() const { return {350.0f, 6.0f, 250.0f, 22.0f}; }
    float xOf(float pos01) const { return waveArea().getX() + pos01 * waveArea().getWidth(); }

    void drawBackdrop(juce::Graphics& g) const;
    void drawWave(juce::Graphics& g) const;
    void drawGrains(juce::Graphics& g) const;
    void drawMarker(juce::Graphics& g) const;
    void drawHud(juce::Graphics& g) const;
    void drawKeys(juce::Graphics& g) const;
    void drawGlass(juce::Graphics& g) const;

    MoteProcessor& proc_;
    juce::Colour accent_{0xff4fc3f7}, deep_{0xff05070a};
    VideoClipPtr clip_;
    double clipStart_ = 0.0, now_ = 0.0;
    bool still_ = false;

    // what the waveform picture was built from
    mote::SamplePtr drawnA_, drawnB_;
    juce::Path waveA_, waveB_;

    float energy_ = 0.0f, lo_ = 0.0f, mid_ = 0.0f, hi_ = 0.0f, grains_ = 0.0f, beatPulse_ = 0.0f;
    bool dropHover_ = false, draggingPos_ = false, hoverChip_ = false;
    int heldKey_ = -1;
    juce::String message_;
    double messageUntil_ = 0.0;
    std::unique_ptr<juce::FileChooser> chooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveView)
};
