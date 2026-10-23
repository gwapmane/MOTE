// MOTE - the pedal: the artwork, drawn at whatever size the window is, and every control on it.
//
//   knobs         drag up/down (or left/right) - hold Shift for fine control - double-click to reset, mouse wheel works too
//   levers        click above / below the lever to move it (the module levers: above = ON, below = OFF)
//   footswitches  left: tap = on / off.  right: tap = Latch on / off, HOLD = Latch only while held
//   A / LIVE / B  click the left / middle / right of the small lever
//
// All the art is drawn at "design size" (the pedal is 1120 x 960) and scaled; hardware sprites are pre-shrunk to the
// exact pixel size on screen, so it stays sharp at every window size and on high-DPI screens.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

#include "Assets.h"
#include "PluginProcessor.h"
#include "Settings.h"
#include "WaveView.h"

class PedalView : public juce::Component, private juce::Timer {
public:
    explicit PedalView(MoteProcessor& p);
    ~PedalView() override { stopTimer(); }

    // design size of the whole view (the pedal plus a margin for the jacks and the floor)
    static constexpr float kDesignW = 1208.0f, kDesignH = 1020.0f;
    static constexpr float kPedalX = 44.0f, kPedalY = 20.0f;

    void setScale(float scale); // logical pixels per design pixel; also resizes the view
    float scale() const { return scale_; }
    void setLook(const LookSettings& look, const juce::File& photoFile);
    void setClip(VideoClipPtr clip) { wave_->setClip(std::move(clip)); }
    WaveView& wave() { return *wave_; }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    struct Hit { enum Kind { None, Knob, Toggle, Foot } kind = None; int index = 0; };

    void timerCallback() override;
    Hit hitTest(juce::Point<float> designPos) const;
    void showMidiMenu(Hit hit, juce::Point<int> where);
    juce::Point<float> toPedal(juce::Point<float> logicalPos) const;

    // state helpers
    float knobValue(int i) const;       // 0..1
    juce::String knobText(int i) const; // what the knob is set to, in words
    int togglePos(int t) const;         // where the lever sits, 0 = up .. 2 = down
    int toggleValue(int t) const;       // what it is set to (for noticing changes)
    void setKnob(int i, float v01);
    void setTogglePos(int t, int pos);
    juce::RangedAudioParameter* param(const char* id) const { return proc_.apvts.getParameter(id); }
    bool boolParam(const char* id) const { return proc_.apvts.getRawParameterValue(id)->load() > 0.5f; }

    // drawing
    void ensureCaches(double phys);
    void rebuildStatic(double u);
    void rebuildThemeSprites(double u);
    void paintStatic(juce::Graphics& g, double u);
    void paintDynamic(juce::Graphics& g);
    void paintWordmark(juce::Graphics& g);
    void paintKnob(juce::Graphics& g, int k);
    void drawTracked(juce::Graphics& g, const juce::String& text, float cx, float cy, const juce::Font& font, float tracking) const;
    void blit(juce::Graphics& g, int sprite, float ax, float ay) const;
    void blitSet(juce::Graphics& g, const assets::Sprites& set, int sprite, float ax, float ay) const;
    void blitImage(juce::Graphics& g, const juce::Image& img, float x, float y) const;
    juce::Colour tint(juce::Colour original) const; // re-colour one of the original wine-red light colours
    juce::Rectangle<float> wordmarkArea() const;

    MoteProcessor& proc_;
    std::unique_ptr<WaveView> wave_;

    float scale_ = 0.70f;
    LookSettings look_;
    juce::File photoFile_;

    juce::Image bodyFull_;    // the coloured body at 2240 x 1920
    bool bodyDirty_ = true;
    bool themeDirty_ = true;
    double u_ = 0.0;          // physical pixels per design pixel the caches were made for
    bool light_ = false;      // the body is light: printed text is drawn in dark ink
    juce::Colour ink_{0xffe9e6e1};
    assets::Sprites sprites_, mini_; // mini_: the knob sprites at 80 %, for the small knobs
    juce::Image bodyScaled_, staticLayer_;
    juce::Image dotLit_, dotOff_, ledOn_, ledOff_;
    juce::Path wordPath_;

    // interaction
    Hit drag_;
    int hoverKnob_ = -1;
    float dragValue_ = 0.0f;
    juce::Point<float> lastDragPos_;
    float wheelAccum_ = 0.0f;
    std::array<bool, 2> pressed_{{false, false}};
    std::array<bool, 2> holdActive_{{false, false}};
    std::array<double, 2> pressedAt_{{0.0, 0.0}};

    // what was drawn last time (so the timer only repaints on a real change)
    std::array<float, 64> shown_{};
    int wordKey_ = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PedalView)
};
