// MOTE - look and feel settings that belong to the USER (not to a song): body colour, light colour,
// texture, your own picture, the window video / picture and the window size. They live in
//   %APPDATA%\MOTE\settings.xml
// and are shared by every MOTE window that is open.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

struct LookSettings {
    juce::Colour body{0xff0f1f45};   // the pedal's powder coat
    juce::Colour accent{0xffFFC247}; // the "lights": jewels, dots, pinstripe, knob marks, the waveform and grains
    int texture = 0;                 // 0 plain, 1 brushed, 2 hammered, 3 slate, 4 marble
    float textureStrength = 0.85f;   // 0..1.2
    juce::String photoFile;          // name of the picture inside the MOTE folder ("" = none)
    float photoOpacity = 0.9f;       // 0..1: how much of the picture shows over the body colour
    int size = 1;                    // 0 small, 1 medium, 2 large
    juce::String videoPath;          // the user's video / GIF / picture ("" = none)
    bool pedalOnly = false;          // hide the top bar and the settings panel: just the pedal

    bool operator==(const LookSettings& o) const {
        return body == o.body && accent == o.accent && texture == o.texture &&
               textureStrength == o.textureStrength && photoFile == o.photoFile &&
               photoOpacity == o.photoOpacity && size == o.size && videoPath == o.videoPath && pedalOnly == o.pedalOnly;
    }
    bool operator!=(const LookSettings& o) const { return !(*this == o); }
};

class Settings {
public:
    struct Listener {
        virtual ~Listener() = default;
        virtual void lookChanged() = 0;
    };

    Settings();

    const LookSettings& look() const { return look_; }
    void setLook(const LookSettings& l); // saves, then tells every listener (message thread only)

    void addListener(Listener* l) { listeners_.add(l); }
    void removeListener(Listener* l) { listeners_.remove(l); }

    static juce::File folder();         // %APPDATA%\MOTE
    juce::File photoPath() const;       // the imported picture, if any
    juce::String importPhoto(const juce::File& src); // copies the picture into the folder; returns its name

    static const LookSettings& defaults();

private:
    void load();
    void save() const;

    LookSettings look_;
    juce::ListenerList<Listener> listeners_;
};
