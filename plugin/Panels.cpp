#include "Panels.h"

#include "Assets.h"
#include "ParamIds.h"
#include "VideoLoop.h"

namespace {
const std::vector<juce::Colour> kBodyColours = {
    juce::Colour(0xff0e3b40), // Deep teal
    juce::Colour(0xff0f1f45), // Midnight (the original)
    juce::Colour(0xff11301f), // Forest
    juce::Colour(0xff3a1236), // Plum
    juce::Colour(0xff430000), // Wine
    juce::Colour(0xff2a2b2e), // Graphite
    juce::Colour(0xffb99a6c), // Sand
    juce::Colour(0xffd9d2c3), // Bone
};
const std::vector<juce::Colour> kLightColours = {
    juce::Colour(0xff4FC3F7), // Electric
    juce::Colour(0xffE8405E), // Rose
    juce::Colour(0xffFF7A2E), // Ember
    juce::Colour(0xffFFC247), // Amber (the original)
    juce::Colour(0xff7FE060), // Lime
    juce::Colour(0xff36D6C8), // Aqua
    juce::Colour(0xff9B6BFF), // Violet
    juce::Colour(0xffF050C8), // Magenta
    juce::Colour(0xffF2F2F2), // White
};

bool sameRgb(juce::Colour a, juce::Colour b) { return (a.getARGB() & 0x00ffffff) == (b.getARGB() & 0x00ffffff); }

void styleLabel(juce::Label& l, float size, juce::Colour colour, int weight = 400) {
    l.setFont(assets::font(weight, size));
    l.setColour(juce::Label::textColourId, colour);
    l.setInterceptsMouseClicks(false, false);
}

// the "pick any colour" popup
struct ColourPopup : public juce::ColourSelector, private juce::ChangeListener {
    explicit ColourPopup(juce::Colour start) : juce::ColourSelector(showSliders | showColourspace, 4, 4) {
        setCurrentColour(start, juce::dontSendNotification);
        setSize(264, 296);
        addChangeListener(this);
    }
    ~ColourPopup() override { removeChangeListener(this); }
    std::function<void(juce::Colour)> onColour;

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override {
        if (onColour) onColour(getCurrentColour().withAlpha(1.0f));
    }
};
} // namespace

// ================================================================================================ FlowPage
void FlowPage::addHeader(const juce::String& title) {
    Row r;
    r.header = title;
    r.h = 18;
    r.gap = 18;
    rows_.push_back(std::move(r));
}

void FlowPage::addRow(std::vector<juce::Component*> comps, int height, int gapAbove) {
    for (auto* c : comps)
        if (c != nullptr) addAndMakeVisible(*c);
    Row r;
    r.comps = std::move(comps);
    r.h = height;
    r.gap = gapAbove;
    rows_.push_back(std::move(r));
}

int FlowPage::layoutFor(int width) {
    constexpr int pad = 14, gapX = 8;
    int y = 4;
    for (auto& r : rows_) {
        y += r.gap;
        r.y = y;
        const int n = static_cast<int>(r.comps.size());
        if (n > 0) {
            const int w = (width - 2 * pad - gapX * (n - 1)) / n;
            for (int i = 0; i < n; ++i)
                if (r.comps[static_cast<size_t>(i)] != nullptr) r.comps[static_cast<size_t>(i)]->setBounds(pad + i * (w + gapX), y, w, r.h);
        }
        y += r.h;
    }
    y += 20;
    setSize(width, y);
    return y;
}

void FlowPage::paint(juce::Graphics& g) {
    for (auto& r : rows_) {
        if (r.header.isEmpty()) continue;
        g.setColour(ui::dim);
        g.setFont(assets::font(600, 11.0f).withExtraKerningFactor(0.14f));
        g.drawText(r.header.toUpperCase(), 14, r.y, getWidth() - 28, r.h - 3, juce::Justification::centredLeft, false);
        g.setColour(ui::line);
        g.fillRect(14, r.y + r.h - 1, getWidth() - 28, 1);
    }
}

// ================================================================================================ SwatchRow
juce::Rectangle<int> SwatchRow::tile(int i) const {
    const float p = pitch();
    return juce::Rectangle<float>(p * static_cast<float>(i), 0.0f, p, static_cast<float>(getHeight())).toNearestInt();
}

int SwatchRow::indexAt(juce::Point<int> pos) const {
    if (pos.y < 0 || pos.y >= getHeight() || pos.x < 0) return -1;
    const int i = static_cast<int>(static_cast<float>(pos.x) / pitch());
    return i <= static_cast<int>(colours_.size()) ? i : -1;
}

void SwatchRow::paint(juce::Graphics& g) {
    const float p = pitch();
    const float r = juce::jmin(11.5f, p * 0.5f - 3.0f);
    bool matched = false;
    for (int i = 0; i <= static_cast<int>(colours_.size()); ++i) {
        const juce::Point<float> c(p * static_cast<float>(i) + p * 0.5f, getHeight() * 0.5f);
        const bool custom = i == static_cast<int>(colours_.size());
        bool sel = false;
        if (!custom) {
            sel = sameRgb(colours_[static_cast<size_t>(i)], selected_);
            matched = matched || sel;
        } else {
            sel = !matched;
        }
        if (custom) {
            for (int k = 0; k < 12; ++k) {
                juce::Path wedge;
                const float a0 = juce::MathConstants<float>::twoPi * static_cast<float>(k) / 12.0f;
                const float a1 = juce::MathConstants<float>::twoPi * static_cast<float>(k + 1) / 12.0f;
                wedge.addPieSegment(c.x - r, c.y - r, 2 * r, 2 * r, a0, a1 + 0.02f, 0.0f);
                g.setColour(juce::Colour::fromHSV(static_cast<float>(k) / 12.0f, 0.75f, 0.95f, 1.0f));
                g.fillPath(wedge);
            }
            if (sel) { // show the chosen custom colour in the middle
                g.setColour(selected_);
                g.fillEllipse(c.x - r * 0.45f, c.y - r * 0.45f, r * 0.9f, r * 0.9f);
            }
        } else {
            juce::ColourGradient gr(colours_[static_cast<size_t>(i)].brighter(0.35f), c.x - r * 0.5f, c.y - r * 0.7f,
                                    colours_[static_cast<size_t>(i)].darker(0.25f), c.x + r * 0.6f, c.y + r * 0.8f, false);
            g.setGradientFill(gr);
            g.fillEllipse(c.x - r, c.y - r, 2 * r, 2 * r);
        }
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.drawEllipse(c.x - r, c.y - r, 2 * r, 2 * r, 1.0f);
        if (sel) {
            g.setColour(juce::Colours::white.withAlpha(0.95f));
            g.drawEllipse(c.x - r - 3.5f, c.y - r - 3.5f, 2 * r + 7.0f, 2 * r + 7.0f, 1.8f);
        } else if (i == hover_) {
            g.setColour(ui::lineHi);
            g.drawEllipse(c.x - r - 3.5f, c.y - r - 3.5f, 2 * r + 7.0f, 2 * r + 7.0f, 1.4f);
        }
    }
}

void SwatchRow::mouseMove(const juce::MouseEvent& e) {
    const int i = indexAt(e.getPosition());
    if (i != hover_) {
        hover_ = i;
        repaint();
    }
}

void SwatchRow::mouseDown(const juce::MouseEvent& e) {
    const int i = indexAt(e.getPosition());
    if (i < 0) return;
    if (i < static_cast<int>(colours_.size())) {
        selected_ = colours_[static_cast<size_t>(i)];
        repaint();
        if (onPick) onPick(selected_);
    } else if (onCustom) {
        onCustom(tile(i));
    }
}

// ================================================================================================ rows with controls
LabelledSlider::LabelledSlider(const juce::String& text, double lo, double hi, double step) {
    label.setText(text, juce::dontSendNotification);
    styleLabel(label, 13.0f, ui::text);
    label.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(label);
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 54, 20);
    slider.setRange(lo, hi, step);
    slider.setScrollWheelEnabled(false); // so the mouse wheel scrolls the page instead
    addAndMakeVisible(slider);
}

void LabelledSlider::resized() {
    auto r = getLocalBounds();
    label.setBounds(r.removeFromLeft(r.getWidth() * 38 / 100));
    slider.setBounds(r);
}

ParamSlider::ParamSlider(juce::AudioProcessorValueTreeState& s, const char* id, const juce::String& text) : LabelledSlider(text, 0.0, 1.0, 0.0) {
    att_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(s, id, slider);
}

ParamToggle::ParamToggle(juce::AudioProcessorValueTreeState& s, const char* id, const juce::String& text) {
    setButtonText(text);
    att_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(s, id, *this);
}

ParamCombo::ParamCombo(juce::AudioProcessorValueTreeState& s, const char* id, const juce::String& text) {
    label_.setText(text, juce::dontSendNotification);
    styleLabel(label_, 13.0f, ui::text);
    addAndMakeVisible(label_);
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(s.getParameter(id))) box_.addItemList(choice->choices, 1);
    addAndMakeVisible(box_);
    att_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(s, id, box_);
}

void ParamCombo::resized() {
    auto r = getLocalBounds();
    label_.setBounds(r.removeFromLeft(r.getWidth() * 38 / 100));
    box_.setBounds(r.reduced(0, 1));
}

// ================================================================================================ SavePanel
SavePanel::SavePanel(MoteProcessor& p) : proc_(p) {
    title_.setText("Save as a preset", juce::dontSendNotification);
    styleLabel(title_, 17.0f, ui::text, 500);
    note_.setJustificationType(juce::Justification::topLeft);
    styleLabel(note_, 12.0f, ui::dim);
    name_.setFont(assets::font(500, 16.0f));
    name_.setSelectAllWhenFocused(true);
    name_.setInputRestrictions(48);
    name_.setIndents(10, 6);
    name_.onTextChange = [this] { updateNote(); };
    name_.onReturnKey = [this] { save(); };
    name_.onEscapeKey = [this] { close(); };
    save_.onClick = [this] { save(); };
    cancel_.onClick = [this] { close(); };
    for (auto* c : std::initializer_list<juce::Component*>{&title_, &note_, &name_, &save_, &cancel_}) addAndMakeVisible(c);
    setVisible(false);
    setWantsKeyboardFocus(false);
}

void SavePanel::open() {
    proc_.presets.rescanUser();
    name_.setText(proc_.presets.suggestName(), false);
    updateNote();
    setVisible(true);
    toFront(true);
    juce::Component::SafePointer<SavePanel> safe(this);
    juce::MessageManager::callAsync([safe] {
        if (safe != nullptr && safe->isShowing()) {
            safe->name_.grabKeyboardFocus();
            safe->name_.selectAll();
        }
    });
}

void SavePanel::close() {
    setVisible(false);
    if (onDone) onDone();
}

void SavePanel::save() {
    if (name_.getText().trim().isEmpty()) {
        note_.setText("Type a name first.", juce::dontSendNotification);
        return;
    }
    if (proc_.presets.saveUser(name_.getText())) close();
    else note_.setText("Could not write the file. Is the MOTE folder in your AppData writable?", juce::dontSendNotification);
}

void SavePanel::updateNote() {
    const auto n = name_.getText().trim();
    bool exists = false;
    for (auto& e : proc_.presets.entries())
        if (e.user && e.name.equalsIgnoreCase(n)) exists = true;
    note_.setText(exists ? "A preset with this name already exists. Saving will replace it."
                         : "Saves the knobs, switches and options as they are now. You will find it under My Presets.",
                  juce::dontSendNotification);
    note_.setColour(juce::Label::textColourId, exists ? juce::Colour(0xffE8A06E) : ui::dim);
}

void SavePanel::resized() {
    card_ = juce::Rectangle<int>(0, 0, juce::jmin(330, getWidth() - 24), 204).withCentre({getWidth() / 2, getHeight() * 2 / 5});
    auto r = card_.reduced(18, 16);
    title_.setBounds(r.removeFromTop(26));
    r.removeFromTop(8);
    name_.setBounds(r.removeFromTop(36));
    r.removeFromTop(8);
    auto bottom = r.removeFromBottom(34);
    note_.setBounds(r);
    cancel_.setBounds(bottom.removeFromLeft(bottom.getWidth() / 2 - 4));
    bottom.removeFromLeft(8);
    save_.setBounds(bottom);
}

void SavePanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black.withAlpha(0.62f));
    juce::DropShadow(juce::Colours::black.withAlpha(0.7f), 24, {0, 8}).drawForRectangle(g, card_);
    g.setColour(ui::panel);
    g.fillRoundedRectangle(card_.toFloat(), 10.0f);
    g.setColour(ui::lineHi);
    g.drawRoundedRectangle(card_.toFloat().reduced(0.5f), 10.0f, 1.0f);
}

void SavePanel::mouseDown(const juce::MouseEvent& e) {
    if (!card_.contains(e.getPosition())) close();
}

// ================================================================================================ SettingsPanel
SettingsPanel::SettingsPanel(MoteProcessor& p)
    : proc_(p), bodySwatches_(kBodyColours), lightSwatches_(kLightColours) {
    // ---- header ----
    for (auto* b : {&tabLook_, &tabSound_}) addAndMakeVisible(b);
    addAndMakeVisible(done_);
    addAndMakeVisible(viewport_);
    viewport_.setScrollBarsShown(true, false);
    viewport_.setScrollBarThickness(8);
    tabLook_.onClick = [this] { showTab(0); };
    tabSound_.onClick = [this] { showTab(1); };
    done_.onClick = [this] {
        flushEdit();
        if (onClose) onClose();
    };

    // ---- LOOK page ----
    lookPage_.addHeader("Body colour");
    lookPage_.addRow({&bodySwatches_}, 34, 8);
    lookPage_.addHeader("Lights colour");
    lookPage_.addRow({&lightSwatches_}, 34, 8);
    lookPage_.addHeader("Texture");
    static const char* texNames[5] = {"Plain", "Brushed", "Hammered", "Slate", "Marble"};
    std::vector<juce::Component*> texRow;
    for (int i = 0; i < 5; ++i) {
        texButtons_[i].setButtonText(texNames[i]);
        texButtons_[i].onClick = [this, i] {
            edit([i](LookSettings& l) {
                l.texture = i;
                if (i > 0 && l.textureStrength < 0.05f) l.textureStrength = 0.85f;
            });
        };
        texRow.push_back(&texButtons_[i]);
    }
    lookPage_.addRow(texRow, 30, 8);
    lookPage_.addRow({&strength_}, 26, 8);
    lookPage_.addHeader("Your own picture");
    lookPage_.addRow({&photoChoose_, &photoRemove_}, 30, 8);
    styleLabel(photoName_, 12.0f, ui::dim);
    lookPage_.addRow({&photoName_}, 16, 6);
    lookPage_.addRow({&photoOpacity_}, 26, 6);
    lookPage_.addHeader("Window: video, GIF or picture");
    lookPage_.addRow({&videoChoose_, &videoRemove_}, 30, 8);
    styleLabel(videoStatus_, 12.0f, ui::text);
    videoStatus_.setJustificationType(juce::Justification::topLeft);
    lookPage_.addRow({&videoStatus_}, 34, 6);
    styleLabel(videoHelp_, 11.5f, ui::dim);
    videoHelp_.setJustificationType(juce::Justification::topLeft);
    videoHelp_.setText("It plays behind the waveform and brightens and pulses with the sound. A wide picture fits best (it is cropped to the "
                       "window's shape). GIF, JPG and PNG work everywhere; MP4, MOV, WMV and AVI work on Windows. The first 12 seconds are used.",
                       juce::dontSendNotification);
    lookPage_.addRow({&videoHelp_}, 48, 2);
    lookPage_.addHeader("Window size");
    static const char* sizeNames[3] = {"Small", "Medium", "Large"};
    std::vector<juce::Component*> sizeRow;
    for (int i = 0; i < 3; ++i) {
        sizeButtons_[i].setButtonText(sizeNames[i]);
        sizeButtons_[i].onClick = [this, i] { edit([i](LookSettings& l) { l.size = i; }); };
        sizeRow.push_back(&sizeButtons_[i]);
    }
    lookPage_.addRow(sizeRow, 30, 8);
    lookPage_.addRow({&resetLook_}, 30, 24);

    bodySwatches_.onPick = [this](juce::Colour c) { edit([c](LookSettings& l) { l.body = c; }); };
    bodySwatches_.onCustom = [this](juce::Rectangle<int> t) { openCustomColour(true, t, bodySwatches_); };
    lightSwatches_.onPick = [this](juce::Colour c) { edit([c](LookSettings& l) { l.accent = c; }); };
    lightSwatches_.onCustom = [this](juce::Rectangle<int> t) { openCustomColour(false, t, lightSwatches_); };

    strength_.slider.setDoubleClickReturnValue(true, 0.85);
    strength_.slider.onValueChange = [this] { if (!refreshing_) queueSliders(); };
    strength_.slider.onDragEnd = [this] { flushEdit(); };
    photoOpacity_.slider.setDoubleClickReturnValue(true, 0.9);
    photoOpacity_.slider.onValueChange = [this] { if (!refreshing_) queueSliders(); };
    photoOpacity_.slider.onDragEnd = [this] { flushEdit(); };

    photoChoose_.onClick = [this] { choosePhoto(); };
    photoRemove_.onClick = [this] { edit([](LookSettings& l) { l.photoFile = {}; }); };
    videoChoose_.onClick = [this] { chooseVideo(); };
    videoRemove_.onClick = [this] {
        if (onVideoRemoved) onVideoRemoved();
    };
    resetLook_.onClick = [this] {
        edit([](LookSettings& l) {
            const auto& d = Settings::defaults();
            l.body = d.body;
            l.accent = d.accent;
            l.texture = d.texture;
            l.textureStrength = d.textureStrength;
            l.photoFile = {};
            l.photoOpacity = d.photoOpacity;
        });
    };

    // ---- SOUND page ----
    auto& apvts = p.apvts;
    auto slider = [&](const char* id, const char* text) {
        auto s = std::make_unique<ParamSlider>(apvts, id, text);
        soundPage_.addRow({s.get()}, 26, 6);
        owned_.push_back(std::move(s));
    };
    auto combo = [&](const char* id, const char* text) {
        auto s = std::make_unique<ParamCombo>(apvts, id, text);
        soundPage_.addRow({s.get()}, 28, 6);
        owned_.push_back(std::move(s));
    };
    auto toggle = [&](const char* id, const char* text) {
        auto s = std::make_unique<ParamToggle>(apvts, id, text);
        soundPage_.addRow({s.get()}, 26, 6);
        owned_.push_back(std::move(s));
    };
    auto note = [&](juce::Label& l, const char* text, int height) {
        styleLabel(l, 11.5f, ui::dim);
        l.setJustificationType(juce::Justification::topLeft);
        l.setText(text, juce::dontSendNotification);
        soundPage_.addRow({&l}, height, 4);
    };
    auto noteOwned = [&](const char* text, int height) {
        auto l = std::make_unique<juce::Label>();
        styleLabel(*l, 11.5f, ui::dim);
        l->setJustificationType(juce::Justification::topLeft);
        l->setText(text, juce::dontSendNotification);
        soundPage_.addRow({l.get()}, height, 4);
        owned_.push_back(std::move(l));
    };
    // a ramp or macro's nine "moves this knob" switches, three to a row
    auto targets = [&](const char* prefix) {
        for (int row = 0; row < 3; ++row) {
            std::vector<juce::Component*> comps;
            for (int col = 0; col < 3; ++col) {
                const int t = row * 3 + col;
                auto s = std::make_unique<ParamToggle>(apvts, (juce::String(prefix) + juce::String(t)).toRawUTF8(), mote::targetName(t));
                comps.push_back(s.get());
                owned_.push_back(std::move(s));
            }
            soundPage_.addRow(comps, 26, row == 0 ? 8 : 2);
        }
    };

    soundPage_.addHeader("Your sample");
    note(sampleNote_, "Drag a sound file onto the window, or load one here. WAV, AIFF, FLAC, OGG and MP3 work (the first 40 seconds are used). "
                      "Your sample is saved inside your project. MORPH blends in the second sample while the Morph lever is on.", 74);
    soundPage_.addRow({&loadA_, &loadB_}, 30, 8);
    soundPage_.addRow({&forgetA_, &forgetB_}, 30, 4);
    styleLabel(sampleStatus_, 12.0f, ui::text);
    sampleStatus_.setJustificationType(juce::Justification::topLeft);
    soundPage_.addRow({&sampleStatus_}, 34, 6);
    combo(ids::source, "Sample");
    combo(ids::morphSource, "Morph sample");
    combo(ids::sampleKey, "Sample's own note");
    noteOwned("Sample's own note: the note your sample sounds like. It lines up the keyboard and the Scale lock (Key / Scale) with it.", 44);
    loadA_.onClick = [this] { if (onLoadSample) onLoadSample(false); };
    loadB_.onClick = [this] { if (onLoadSample) onLoadSample(true); };
    forgetA_.onClick = [this] { proc_.clearCustom(false); updateSamples(); };
    forgetB_.onClick = [this] { proc_.clearCustom(true); updateSamples(); };

    soundPage_.addHeader("Grains");
    slider(ids::grainShape, "Grain edges");
    slider(ids::reverse, "Backwards chance");
    slider(ids::tune, "Fine tune");
    toggle(ids::trails, "Let notes ring out when switched off");

    soundPage_.addHeader("Filter (lever on)");
    combo(ids::filterType, "Type");
    slider(ids::filterRes, "Resonance");
    slider(ids::filterSpread, "Spread between grains");
    slider(ids::filterKey, "Follow the key you play");

    soundPage_.addHeader("Walk (lever on)");
    noteOwned("Steps through the slices of your sample in time with your DAW. The small knob picks the order.", 32);
    combo(ids::walkRate, "Step length");
    slider(ids::walkSteps, "Number of steps");
    slider(ids::walkGate, "Gate");

    soundPage_.addHeader("Repeat (lever on)");
    noteOwned("Grabs the last bit of sound and loops it in time. The small knob picks how long the loop is.", 32);
    slider(ids::repeatDecay, "Fade of the loop");

    soundPage_.addHeader("Delay");
    combo(ids::delayDiv, "Time");
    slider(ids::delayFb, "Feedback");
    slider(ids::delayTone, "Tone");

    soundPage_.addHeader("Reverb");
    slider(ids::reverbSize, "Size");
    slider(ids::reverbDecay, "Decay");
    slider(ids::reverbDamp, "Damping");

    soundPage_.addHeader("Ramp 1 (moves knobs by itself)");
    slider(ids::r1Rate, "Rate");
    slider(ids::r1Depth, "Depth");
    combo(ids::r1Shape, "Shape");
    combo(ids::r1Sync, "Lock cycle to tempo");
    targets("r1t");
    soundPage_.addHeader("Ramp 2");
    slider(ids::r2Rate, "Rate");
    slider(ids::r2Depth, "Depth");
    combo(ids::r2Shape, "Shape");
    combo(ids::r2Sync, "Lock cycle to tempo");
    targets("r2t");

    soundPage_.addHeader("Macro 1 (one knob moves several)");
    slider(ids::m1Value, "Macro 1");
    slider(ids::m1Depth, "Amount (can go negative)");
    targets("m1t");
    soundPage_.addHeader("Macro 2");
    slider(ids::m2Value, "Macro 2");
    slider(ids::m2Depth, "Amount (can go negative)");
    targets("m2t");

    soundPage_.addHeader("Tempo (follows your DAW)");
    note(tempoNote_, "Walk, Repeat, the delay and tempo-locked ramps follow your DAW's tempo. Without a DAW, the slider below is used.", 44);
    {
        auto s = std::make_unique<ParamSlider>(apvts, ids::manualBpm, "Tempo (no DAW)");
        manualBpm_ = s.get();
        soundPage_.addRow({s.get()}, 26, 8);
        owned_.push_back(std::move(s));
    }
    styleLabel(tempoStatus_, 12.0f, ui::text);
    tempoStatus_.setJustificationType(juce::Justification::topLeft);
    soundPage_.addRow({&tempoStatus_}, 36, 6);

    soundPage_.addHeader("Output");
    slider(ids::outputGain, "Output level");

    soundPage_.addHeader("MIDI learn");
    styleLabel(midiHelp_, 11.5f, ui::dim);
    midiHelp_.setJustificationType(juce::Justification::topLeft);
    midiHelp_.setText("Control any knob or switch from a MIDI controller: pick it here, press Learn, then move a knob on your controller. "
                      "You can also right-click a knob or footswitch on the pedal. "
                      "In FL Studio, first set the plugin's MIDI Input port (gear icon > Settings > MIDI) to your controller's port.",
                      juce::dontSendNotification);
    soundPage_.addRow({&midiHelp_}, 96, 8);
    for (auto* prm : proc_.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(prm)) {
            midiIds_.add(rp->paramID);
            midiTarget_.addItem(rp->getName(40), midiIds_.size());
        }
    midiTarget_.setSelectedId(1, juce::dontSendNotification);
    soundPage_.addRow({&midiTarget_, &midiLearn_}, 30, 4);
    styleLabel(midiList_, 12.0f, ui::text);
    midiList_.setJustificationType(juce::Justification::topLeft);
    soundPage_.addRow({&midiList_}, 96, 8);
    soundPage_.addRow({&midiForgetAll_}, 30, 4);
    midiLearn_.onClick = [this] {
        if (proc_.midiLearnTarget() != nullptr) {
            proc_.cancelMidiLearn();
        } else if (const int i = midiTarget_.getSelectedId() - 1; i >= 0 && i < midiIds_.size()) {
            if (auto* p = proc_.apvts.getParameter(midiIds_[i])) proc_.startMidiLearn(p);
        }
        updateMidi();
    };
    midiForgetAll_.onClick = [this] {
        proc_.clearAllMidiMappings();
        proc_.cancelMidiLearn();
        updateMidi();
    };
    midiPoll_.fn = [this] { updateMidi(); updateTempo(); updateSamples(); };
    midiPoll_.startTimerHz(4);

    settings_->addListener(this);
    showTab(0);
    refresh();
    updateMidi();
    updateTempo();
    updateSamples();
}

void SettingsPanel::updateTempo() {
    const float bpm = proc_.tempoBpm.load();
    juce::String t;
    t << "Tempo: " << juce::String(bpm, 1) << " BPM"
      << (proc_.tempoFromHost.load() ? "  (from your DAW)" : "  (from the slider above; your DAW sent none)");
    if (t != shownTempo_) {
        shownTempo_ = t;
        tempoStatus_.setText(t, juce::dontSendNotification);
    }
}

void SettingsPanel::updateSamples() {
    juce::String t;
    auto name = [&](bool b) {
        const int src = juce::roundToInt(proc_.apvts.getRawParameterValue(b ? ids::morphSource : ids::source)->load());
        if (src == mote::sources::kCustom) return proc_.hasCustom(b) ? proc_.customName(b) : juce::String("(none loaded)");
        return juce::String(mote::sources::name(src));
    };
    t << "Playing: " << name(false) << "\nMorph to: " << name(true);
    if (t != shownSamples_) {
        shownSamples_ = t;
        sampleStatus_.setText(t, juce::dontSendNotification);
    }
    forgetA_.setEnabled(proc_.hasCustom(false));
    forgetB_.setEnabled(proc_.hasCustom(true));
}

void SettingsPanel::updateMidi() {
    const auto s = proc_.midiMappingSummary();
    if (s != shownMidiSummary_) {
        shownMidiSummary_ = s;
        midiList_.setText(s, juce::dontSendNotification);
    }
    const bool learning = proc_.midiLearnTarget() != nullptr;
    midiLearn_.setButtonText(learning ? "Waiting..." : "Learn");
    midiLearn_.setToggleState(learning, juce::dontSendNotification);
}

SettingsPanel::~SettingsPanel() {
    midiPoll_.stopTimer();
    stopTimer();
    settings_->removeListener(this);
    viewport_.setViewedComponent(nullptr, false);
}

void SettingsPanel::showTab(int tab) {
    tab_ = tab;
    tabLook_.setToggleState(tab == 0, juce::dontSendNotification);
    tabSound_.setToggleState(tab == 1, juce::dontSendNotification);
    viewport_.setViewedComponent(tab == 0 ? static_cast<juce::Component*>(&lookPage_) : static_cast<juce::Component*>(&soundPage_), false);
    viewport_.setViewPosition(0, 0);
    resized();
}

void SettingsPanel::resized() {
    auto r = getLocalBounds();
    auto head = r.removeFromTop(48).reduced(12, 9);
    done_.setBounds(head.removeFromRight(64));
    tabLook_.setBounds(head.removeFromLeft(78));
    head.removeFromLeft(6);
    tabSound_.setBounds(head.removeFromLeft(88));
    viewport_.setBounds(r.withTrimmedLeft(1));
    const int w = viewport_.getWidth() - viewport_.getScrollBarThickness();
    lookPage_.layoutFor(w);
    soundPage_.layoutFor(w);
}

void SettingsPanel::paint(juce::Graphics& g) {
    g.fillAll(ui::bg);
    g.setColour(ui::line);
    g.fillRect(0, 0, 1, getHeight());
    g.fillRect(0, 47, getWidth(), 1);
}

void SettingsPanel::edit(const std::function<void(LookSettings&)>& fn) {
    if (refreshing_) return;
    flushEdit();
    auto l = settings_->look();
    fn(l);
    settings_->setLook(l);
}

void SettingsPanel::queueEdit(std::function<void(LookSettings&)> fn) {
    pending_ = std::move(fn);
    if (!isTimerRunning()) startTimer(80);
}

void SettingsPanel::flushEdit() {
    stopTimer();
    if (!pending_) return;
    auto fn = std::move(pending_);
    pending_ = nullptr;
    auto l = settings_->look();
    fn(l);
    settings_->setLook(l);
}

void SettingsPanel::timerCallback() { flushEdit(); }

void SettingsPanel::queueSliders() {
    const auto s = static_cast<float>(strength_.slider.getValue());
    const auto o = static_cast<float>(photoOpacity_.slider.getValue());
    queueEdit([s, o](LookSettings& l) {
        l.textureStrength = s;
        l.photoOpacity = o;
    });
}

void SettingsPanel::refresh() {
    const juce::ScopedValueSetter<bool> guard(refreshing_, true);
    const auto& l = settings_->look();
    bodySwatches_.setSelected(l.body);
    lightSwatches_.setSelected(l.accent);
    for (int i = 0; i < 5; ++i) texButtons_[i].setToggleState(l.texture == i, juce::dontSendNotification);
    if (!strength_.slider.isMouseButtonDown()) strength_.slider.setValue(l.textureStrength, juce::dontSendNotification);
    strength_.setEnabled(l.texture > 0);
    strength_.setAlpha(l.texture > 0 ? 1.0f : 0.4f);
    const bool hasPhoto = l.photoFile.isNotEmpty();
    photoRemove_.setEnabled(hasPhoto);
    photoName_.setText(hasPhoto ? "Your picture is showing through the body colour." : "No picture: just the colour and texture.",
                       juce::dontSendNotification);
    if (!photoOpacity_.slider.isMouseButtonDown()) photoOpacity_.slider.setValue(l.photoOpacity, juce::dontSendNotification);
    photoOpacity_.setEnabled(hasPhoto);
    photoOpacity_.setAlpha(hasPhoto ? 1.0f : 0.4f);
    videoRemove_.setEnabled(l.videoPath.isNotEmpty());
    for (int i = 0; i < 3; ++i) sizeButtons_[i].setToggleState(l.size == i, juce::dontSendNotification);
}

void SettingsPanel::setVideoStatus(const juce::String& text, bool isError) {
    videoStatus_.setText(text, juce::dontSendNotification);
    videoStatus_.setColour(juce::Label::textColourId, isError ? juce::Colour(0xffE8A06E) : ui::text);
}

void SettingsPanel::openCustomColour(bool forBody, juce::Rectangle<int> tileInRow, juce::Component& row) {
    auto popup = std::make_unique<ColourPopup>(forBody ? settings_->look().body : settings_->look().accent);
    juce::Component::SafePointer<SettingsPanel> safe(this);
    popup->onColour = [safe, forBody](juce::Colour c) {
        if (safe == nullptr) return;
        safe->queueEdit([c, forBody](LookSettings& l) { (forBody ? l.body : l.accent) = c; });
    };
    auto* top = getTopLevelComponent();
    const auto area = top->getLocalArea(&row, tileInRow);
    juce::CallOutBox::launchAsynchronously(std::move(popup), area, top);
}

void SettingsPanel::choosePhoto() {
    chooser_ = std::make_unique<juce::FileChooser>("Choose a picture for the pedal's body",
                                                   juce::File::getSpecialLocation(juce::File::userPicturesDirectory), "*.jpg;*.jpeg;*.png;*.gif");
    juce::Component::SafePointer<SettingsPanel> safe(this);
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe](const juce::FileChooser& fc) {
        if (safe == nullptr) return;
        const auto f = fc.getResult();
        if (!f.existsAsFile()) return;
        if (!juce::ImageFileFormat::loadFrom(f).isValid()) {
            safe->photoName_.setText("That file could not be opened as a picture. Try a JPG or PNG.", juce::dontSendNotification);
            return;
        }
        const auto name = safe->settings_->importPhoto(f);
        if (name.isEmpty()) {
            safe->photoName_.setText("Could not copy the picture into the MOTE folder.", juce::dontSendNotification);
            return;
        }
        safe->edit([name](LookSettings& l) {
            l.photoFile = name;
            if (l.photoOpacity < 0.2f) l.photoOpacity = 0.9f;
        });
    });
}

void SettingsPanel::chooseVideo() {
#if MOTE_HAVE_MF
    const char* patterns = "*.mp4;*.mov;*.m4v;*.wmv;*.avi;*.gif;*.jpg;*.jpeg;*.png";
#else
    const char* patterns = "*.gif;*.jpg;*.jpeg;*.png";
#endif
    chooser_ = std::make_unique<juce::FileChooser>("Choose a video, GIF or picture for the window",
                                                   juce::File::getSpecialLocation(juce::File::userMoviesDirectory), patterns);
    juce::Component::SafePointer<SettingsPanel> safe(this);
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe](const juce::FileChooser& fc) {
        if (safe == nullptr) return;
        const auto f = fc.getResult();
        if (f.existsAsFile() && safe->onVideoChosen) safe->onVideoChosen(f);
    });
}
