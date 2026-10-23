#include "PedalView.h"

#include <cmath>
#include <functional>

#include "Body.h"
#include "ParamIds.h"

namespace {

struct Pt { float x, y; };

// ---- where everything sits on the pedal (design pixels, pedal-local: the pedal is 1120 x 960) ----
constexpr float kPW = 1120.0f, kPH = 960.0f;
constexpr float kMini = 0.80f; // the small knobs are drawn at 80 %

struct KnobDef { const char* id; const char* name; float x, y; bool mini; int steps; bool bipolar; };
// 0..8 big knobs (the grain), 9..18 small knobs (envelope, key, output), 19..24 small knobs beside the module levers, 25 the sample selector
constexpr int kNumKnobs = 26;
const KnobDef kKnobs[kNumKnobs] = {
    {ids::position, "POSITION", 84, 430, false, 0, false},  {ids::scatter, "SCATTER", 203, 430, false, 0, false},
    {ids::density, "DENSITY", 322, 430, false, 0, false},   {ids::size, "SIZE", 441, 430, false, 0, false},
    {ids::pitch, "PITCH", 560, 430, false, 49, true},       {ids::jitter, "JITTER", 679, 430, false, 0, false},
    {ids::spread, "SPREAD", 798, 430, false, 0, false},     {ids::speed, "SPEED", 917, 430, false, 0, false},
    {ids::magnet, "MAGNET", 1036, 430, false, 0, false},
    {ids::attack, "ATTACK", 112, 734, true, 0, false},      {ids::decay, "DECAY", 212, 734, true, 0, false},
    {ids::sustain, "SUSTAIN", 311, 734, true, 0, false},    {ids::release, "RELEASE", 411, 734, true, 0, false},
    {ids::root, "KEY", 510, 734, true, 12, false},          {ids::scale, "SCALE", 610, 734, true, 11, false},
    {ids::voices, "VOICES", 709, 734, true, 16, false},     {ids::level, "LEVEL", 809, 734, true, 0, false},
    {ids::delayMix, "DELAY", 908, 734, true, 0, false},     {ids::reverbMix, "REVERB", 1008, 734, true, 0, false},
    {ids::morphAmt, "BLEND", 176, 592, true, 0, false},     {ids::cohereAmt, "LOCK", 342, 592, true, 0, false},
    {ids::walkPattern, "PATTERN", 508, 592, true, 7, false}, {ids::repeatDiv, "LENGTH", 674, 592, true, 8, false},
    {ids::filterCut, "CUTOFF", 840, 592, true, 0, false},   {ids::compAmt, "SQUASH", 1006, 592, true, 0, false},
    {ids::source, "SAMPLE", 740, 852, true, 8, false},
};

// levers: 0..5 the module on/off levers, 6 PLAY, 7 the A / LIVE / B lever
struct ModuleDef { const char* onId; const char* title; };
const ModuleDef kModules[6] = {{ids::morphOn, "MORPH"}, {ids::cohereOn, "COHERE"}, {ids::walkOn, "WALK"},
                               {ids::repeatOn, "REPEAT"}, {ids::filterOn, "FILTER"}, {ids::compOn, "COMP"}};
constexpr float kTileX0 = 64.0f, kTilePitch = 166.0f, kTileY = 510.0f, kTileW = 150.0f, kTileH = 144.0f;
constexpr float kLeverY = 592.0f;
constexpr int kNumToggles = 8;
const Pt kTog[kNumToggles] = {{100, kLeverY}, {266, kLeverY}, {432, kLeverY}, {598, kLeverY}, {764, kLeverY}, {930, kLeverY}, {380, 852}, {560, 852}};
const char* const kPlayText[3] = {"HOLD", "TRAVEL", "BOUNCE"};
constexpr Pt kFoot[2] = {{110, 852}, {1010, 852}};
constexpr Pt kLed[3] = {{214, 852}, {560, 792}, {906, 852}};
constexpr double kHoldMs = 350.0;

// the window
constexpr float kGlassX = 50.0f, kGlassY = 100.0f; // WaveView sits here (1020 x 242)

const double kPi = 3.14159265358979323846;
const juce::Colour kRefAccent(0xffE8405E); // the colour the original light sprites were drawn for

inline float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

juce::ColourGradient radial(float cx, float cy, float radius) {
    juce::ColourGradient gr(juce::Colours::transparentBlack, cx, cy, juce::Colours::transparentBlack, cx + radius, cy, true);
    gr.clearColours();
    return gr;
}

constexpr int firstKnobSprite = atlas::knob_seat_ov, lastKnobSprite = atlas::knob_gloss;

} // namespace

// =================================================================================================== set-up
PedalView::PedalView(MoteProcessor& p) : proc_(p) {
    wave_ = std::make_unique<WaveView>(p);
    addAndMakeVisible(*wave_);
    // the wordmark, as one outline so it can be drawn many times for the echo trails
    {
        const auto font = assets::font(600, 46.0f);
        juce::GlyphArrangement ga;
        float x = 0.0f;
        for (auto c : juce::String("MOTE")) {
            const auto s = juce::String::charToString(c);
            ga.addLineOfText(font, s, x, 0.0f);
            x += font.getStringWidthFloat(s) + 22.0f;
        }
        ga.createPath(wordPath_);
        const auto b = wordPath_.getBounds();
        wordPath_.applyTransform(juce::AffineTransform::translation(560.0f - b.getCentreX(), 50.0f - b.getCentreY()));
    }
    setBufferedToImage(true);
    setOpaque(true);
    startTimerHz(30);
    setScale(scale_);
}

void PedalView::setScale(float s) {
    scale_ = s;
    themeDirty_ = true;
    setSize(juce::roundToInt(kDesignW * s), juce::roundToInt(kDesignH * s));
    repaint();
}

void PedalView::resized() {
    const float s = scale_;
    wave_->setBounds(juce::Rectangle<float>((kPedalX + kGlassX) * s, (kPedalY + kGlassY) * s, WaveView::kW * s, WaveView::kH * s).toNearestInt());
    themeDirty_ = true;
}

void PedalView::setLook(const LookSettings& look, const juce::File& photoFile) {
    const bool bodyChanged = look.body != look_.body || look.texture != look_.texture || look.textureStrength != look_.textureStrength ||
                             look.photoFile != look_.photoFile || look.photoOpacity != look_.photoOpacity;
    look_ = look;
    photoFile_ = photoFile;
    if (bodyChanged) bodyDirty_ = true;
    themeDirty_ = true;
    wave_->setLook(look_.accent, body::seatA(look_.body));
    repaint();
}

juce::Colour PedalView::tint(juce::Colour c) const {
    const float dh = look_.accent.getHue() - kRefAccent.getHue();
    float sScale = kRefAccent.getSaturation() > 0.0f ? look_.accent.getSaturation() / kRefAccent.getSaturation() : 1.0f;
    sScale = juce::jlimit(0.0f, 1.4f, sScale);
    float h = c.getHue() + dh;
    h -= std::floor(h);
    return juce::Colour::fromHSV(h, clamp01(c.getSaturation() * sScale), c.getBrightness(), c.getFloatAlpha());
}

// =================================================================================================== state
float PedalView::knobValue(int i) const {
    if (auto* p = param(kKnobs[i].id)) return p->getValue();
    return 0.0f;
}

juce::String PedalView::knobText(int i) const {
    if (auto* p = param(kKnobs[i].id)) return p->getCurrentValueAsText().toUpperCase();
    return {};
}

int PedalView::togglePos(int t) const {
    if (t < 6) return boolParam(kModules[t].onId) ? 0 : 2;
    if (t == 6) return juce::jlimit(0, 2, juce::roundToInt(proc_.apvts.getRawParameterValue(ids::play)->load()));
    return proc_.presets.slot();
}

int PedalView::toggleValue(int t) const { return togglePos(t); }

void PedalView::setKnob(int i, float v) {
    auto* p = param(kKnobs[i].id);
    if (p == nullptr) return;
    v = clamp01(v);
    const int steps = kKnobs[i].steps;
    if (steps > 1) v = std::round(v * (steps - 1)) / static_cast<float>(steps - 1);
    p->setValueNotifyingHost(v);
}

void PedalView::setTogglePos(int t, int pos) {
    if (t == 7) {
        proc_.presets.setSlot(juce::jlimit(0, 2, pos));
        return;
    }
    if (t < 6) {
        if (auto* p = param(kModules[t].onId)) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(pos <= 1 ? 1.0f : 0.0f);
            p->endChangeGesture();
        }
        return;
    }
    pos = juce::jlimit(0, 2, pos);
    if (auto* p = param(ids::play)) {
        p->beginChangeGesture();
        p->setValueNotifyingHost(static_cast<float>(pos) / 2.0f);
        p->endChangeGesture();
    }
}

// =================================================================================================== caches
void PedalView::ensureCaches(double phys) {
    const double u = scale_ * phys;
    if (bodyDirty_) {
        bodyFull_ = body::build(look_, photoFile_);
        bodyDirty_ = false;
        themeDirty_ = true;
        // is the body light? (average brightness of a sample of its pixels)
        double sum = 0.0;
        int n = 0;
        for (int y = 40; y < bodyFull_.getHeight(); y += 24)
            for (int x = 40; x < bodyFull_.getWidth(); x += 24) {
                const auto c = bodyFull_.getPixelAt(x, y);
                sum += 0.299 * c.getFloatRed() + 0.587 * c.getFloatGreen() + 0.114 * c.getFloatBlue();
                ++n;
            }
        light_ = n > 0 && sum / n > 0.42;
        ink_ = light_ ? juce::Colour(0xff1c1215) : juce::Colour(0xffe9e6e1);
    }
    if (std::abs(u - u_) > 1.0e-6) {
        u_ = u;
        sprites_.build(u);
        mini_.build(u * kMini, firstKnobSprite, lastKnobSprite);
        themeDirty_ = true;
    }
    if (themeDirty_) {
        rebuildThemeSprites(u);
        rebuildStatic(u);
        themeDirty_ = false;
    }
}

// the small pieces that carry the lights colour: dots and the indicator jewels
void PedalView::rebuildThemeSprites(double u) {
    auto make = [&](float w, float h, const std::function<void(juce::Graphics&)>& draw) {
        juce::Image im(juce::Image::ARGB, juce::jmax(1, juce::roundToInt(w * u)), juce::jmax(1, juce::roundToInt(h * u)), true);
        juce::Graphics g(im);
        g.addTransform(juce::AffineTransform::scale(static_cast<float>(u)));
        draw(g);
        return im;
    };
    dotLit_ = make(15, 15, [&](juce::Graphics& g) {
        const auto lit = look_.accent.interpolatedWith(juce::Colours::white, 0.7f);
        auto glow = radial(7.5f, 7.5f, 7.0f);
        glow.addColour(0.0, look_.accent.withAlpha(0.65f));
        glow.addColour(0.45, look_.accent.withAlpha(0.25f));
        glow.addColour(1.0, look_.accent.withAlpha(0.0f));
        g.setGradientFill(glow);
        g.fillEllipse(0.5f, 0.5f, 14, 14);
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillEllipse(5.0f, 5.5f, 5, 5);
        auto gr = radial(6.75f, 6.5f, 3.6f);
        gr.addColour(0.0, juce::Colours::white);
        gr.addColour(0.6, lit);
        gr.addColour(1.0, look_.accent);
        g.setGradientFill(gr);
        g.fillEllipse(5.0f, 5.0f, 5, 5);
    });
    dotOff_ = make(15, 15, [&](juce::Graphics& g) {
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.fillEllipse(5.0f, 6.0f, 5, 5);
        auto gr = radial(6.75f, 6.5f, 3.6f);
        gr.addColour(0.0, tint(juce::Colour(0xffa83a5a)));
        gr.addColour(0.6, tint(juce::Colour(0xff7a1d38)));
        gr.addColour(1.0, tint(juce::Colour(0xff4a0f21)));
        g.setGradientFill(gr);
        g.fillEllipse(5.0f, 5.0f, 5, 5);
    });
    auto lens = [&](bool on) {
        return make(36, 36, [&](juce::Graphics& g) {
            const float cx = 18, cy = 18;
            if (on) {
                auto glow = radial(cx, cy, 13.0f);
                const auto c = tint(juce::Colour(0xffe8405e));
                glow.addColour(0.0, c.withAlpha(0.60f));
                glow.addColour(0.62, c.withAlpha(0.45f));
                glow.addColour(1.0, c.withAlpha(0.0f));
                g.setGradientFill(glow);
                g.fillEllipse(cx - 13, cy - 13, 26, 26);
            }
            auto gr = radial(cx - 2.24f, cy - 3.2f, 15.2f);
            if (on) {
                gr.addColour(0.0, tint(juce::Colour(0xffffc2cf)));
                gr.addColour(0.36, tint(juce::Colour(0xffe8405e)));
                gr.addColour(0.82, tint(juce::Colour(0xff9a1535)));
                gr.addColour(1.0, tint(juce::Colour(0xff9a1535)));
            } else {
                gr.addColour(0.0, tint(juce::Colour(0xff7a3347)));
                gr.addColour(0.55, tint(juce::Colour(0xff4a1022)));
                gr.addColour(1.0, tint(juce::Colour(0xff2a0712)));
            }
            g.setGradientFill(gr);
            g.fillEllipse(cx - 8, cy - 8, 16, 16);
            g.setColour((on ? juce::Colour(0xff3c000f).withAlpha(0.5f) : juce::Colour(0xff140006).withAlpha(0.7f)));
            g.drawEllipse(cx - 7.5f, cy - 7.5f, 15, 15, 1.0f);
            if (!on) {
                juce::Path hole;
                hole.addEllipse(cx - 8, cy - 8, 16, 16);
                juce::Path frame;
                frame.setUsingNonZeroWinding(false);
                frame.addRectangle(hole.getBounds().expanded(12));
                frame.addPath(hole);
                g.saveState();
                g.reduceClipRegion(hole);
                juce::DropShadow(juce::Colours::black.withAlpha(0.6f), 3, {0, 2}).drawForPath(g, frame);
                g.restoreState();
            }
        });
    };
    ledOn_ = lens(true);
    ledOff_ = lens(false);
}

void PedalView::rebuildStatic(double u) {
    const int W = juce::jmax(1, juce::roundToInt(getWidth() * (u / scale_)));
    const int H = juce::jmax(1, juce::roundToInt(getHeight() * (u / scale_)));
    bodyScaled_ = assets::resample(bodyFull_, bodyFull_.getBounds(), juce::roundToInt(kPW * u), juce::roundToInt(kPH * u));
    staticLayer_ = juce::Image(juce::Image::ARGB, W, H, true);
    juce::Graphics g(staticLayer_);
    g.addTransform(juce::AffineTransform::scale(static_cast<float>(u)));
    paintStatic(g, u);
}

// =================================================================================================== drawing helpers
void PedalView::blitImage(juce::Graphics& g, const juce::Image& img, float x, float y) const {
    const float px = static_cast<float>(std::round(x * u_) / u_), py = static_cast<float>(std::round(y * u_) / u_);
    g.drawImage(img, juce::Rectangle<float>(px, py, static_cast<float>(img.getWidth() / u_), static_cast<float>(img.getHeight() / u_)),
                juce::RectanglePlacement::stretchToFit);
}

void PedalView::blit(juce::Graphics& g, int sprite, float ax, float ay) const {
    const auto& m = assets::Sprites::meta(sprite);
    blitImage(g, sprites_.get(sprite), ax + m.ox, ay + m.oy);
}

// a sprite from a set built at its own density; drawn without pixel snapping (used inside a scaled transform)
void PedalView::blitSet(juce::Graphics& g, const assets::Sprites& set, int sprite, float ax, float ay) const {
    const auto& m = assets::Sprites::meta(sprite);
    const auto& img = set.get(sprite);
    const double k = set.scale();
    g.drawImage(img, juce::Rectangle<float>(ax + m.ox, ay + m.oy, static_cast<float>(img.getWidth() / k), static_cast<float>(img.getHeight() / k)),
                juce::RectanglePlacement::stretchToFit);
}

void PedalView::drawTracked(juce::Graphics& g, const juce::String& text, float cx, float cy, const juce::Font& font, float tracking) const {
    float w = -tracking;
    for (auto c : text) w += font.getStringWidthFloat(juce::String::charToString(c)) + tracking;
    float x = cx - w * 0.5f;
    g.setFont(font);
    for (auto c : text) {
        const auto s = juce::String::charToString(c);
        const float cw = font.getStringWidthFloat(s);
        g.drawText(s, juce::Rectangle<float>(x - 2.0f, cy - font.getHeight() * 0.5f, cw + 4.0f, font.getHeight()), juce::Justification::centred, false);
        x += cw + tracking;
    }
}

juce::Rectangle<float> PedalView::wordmarkArea() const {
    return {kPedalX + 330.0f, kPedalY + 4.0f, 460.0f, 76.0f};
}

// =================================================================================================== the still parts
void PedalView::paintStatic(juce::Graphics& g, double /*u*/) {
    // ---- the floor under the pedal ----
    {
        juce::ColourGradient floor(juce::Colour(0xff1b2024), 0, 0, juce::Colour(0xff0a0c0e), 0, kDesignH, false);
        floor.addColour(0.52, juce::Colour(0xff12161a));
        g.setGradientFill(floor);
        g.fillRect(0.0f, 0.0f, kDesignW, kDesignH);
        g.saveState();
        g.addTransform(juce::AffineTransform::scale(0.62f * kDesignW, 0.46f * kDesignH).translated(0.5f * kDesignW, 0.40f * kDesignH));
        auto gr = radial(0, 0, 1.0f);
        gr.addColour(0.0, juce::Colour(0xffe4e6e9).withAlpha(0.12f));
        gr.addColour(0.72, juce::Colour(0xffe4e6e9).withAlpha(0.0f));
        gr.addColour(1.0, juce::Colour(0xffe4e6e9).withAlpha(0.0f));
        g.setGradientFill(gr);
        g.fillEllipse(-1, -1, 2, 2);
        g.restoreState();
    }

    g.addTransform(juce::AffineTransform::translation(kPedalX, kPedalY));
    const juce::Colour pin = look_.accent.darker(0.35f).withAlpha(0.9f);

    // ---- side jacks, tucked behind the enclosure ----
    blit(g, atlas::jack_l, -34, 426);
    blit(g, atlas::jack_r, kPW - 6, 426);
    blit(g, atlas::jack_l, -34, 562);
    blit(g, atlas::jack_r, kPW - 6, 562);

    // ---- the pedal's shadow on the floor ----
    {
        auto shadow = [&](float inset, float corner, int dx, int dy, int blur, float alpha) {
            juce::Path p;
            p.addRoundedRectangle(inset, inset, kPW - 2 * inset, kPH - 2 * inset, juce::jmax(2.0f, corner - inset));
            juce::DropShadow(juce::Colours::black.withAlpha(alpha), blur, {dx, dy}).drawForPath(g, p);
        };
        shadow(18.0f, 30.0f, 34, 70, 48, 0.65f);
        shadow(2.0f, 30.0f, 10, 24, 17, 0.50f);
        shadow(0.0f, 30.0f, 2, 5, 4, 0.45f);
        shadow(0.0f, 30.0f, 0, 1, 1, 0.75f);
    }

    // ---- the body ----
    {
        juce::Path shape;
        shape.addRoundedRectangle(0.0f, 0.0f, kPW, kPH, 30.0f);
        g.saveState();
        g.reduceClipRegion(shape);
        blitImage(g, bodyScaled_, 0, 0);
        g.restoreState();
    }
    // ---- printed pinstripe ----
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawRoundedRectangle(11.625f, 12.625f, kPW - 26.25f, 933.75f, 19.4f, 1.25f);
    g.setColour(pin);
    g.drawRoundedRectangle(11.625f, 11.625f, kPW - 26.25f, 933.75f, 19.4f, 1.25f);

    // ---- the window plate: a bevelled metal frame with the glass set into it ----
    {
        const juce::Rectangle<float> glass(kGlassX, kGlassY, WaveView::kW, WaveView::kH);
        const auto well = glass.expanded(6.0f);
        const auto plate = well.expanded(10.0f);
        juce::Path pp;
        pp.addRoundedRectangle(plate, 26.0f);
        juce::DropShadow(juce::Colours::black.withAlpha(0.55f), 14, {0, 5}).drawForPath(g, pp);
        juce::ColourGradient m(juce::Colour(0xffb3b9bf), plate.getX(), plate.getY(), juce::Colour(0xff373c42), plate.getRight(), plate.getBottom(), false);
        m.addColour(0.45, juce::Colour(0xff6c737a));
        m.addColour(0.7, juce::Colour(0xff9aa0a6));
        g.setGradientFill(m);
        g.fillPath(pp);
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawRoundedRectangle(plate.reduced(0.7f), 25.5f, 1.2f);
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.drawRoundedRectangle(plate.reduced(1.9f), 24.5f, 1.0f);
        // the dark well around the glass
        g.setColour(juce::Colour(0xff07080a));
        g.fillRoundedRectangle(well, 20.0f);
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.drawRoundedRectangle(well, 20.0f, 1.5f);
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.drawRoundedRectangle(well.expanded(0.8f), 20.5f, 0.8f);
        // four screws
        const Pt screws[4] = {{well.getX() + 1, well.getY() + 1}, {well.getRight() - 1, well.getY() + 1},
                              {well.getX() + 1, well.getBottom() - 1}, {well.getRight() - 1, well.getBottom() - 1}};
        for (int i = 0; i < 4; ++i) {
            const float x = screws[i].x, y = screws[i].y;
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.fillEllipse(x - 4.0f, y - 3.2f, 8.0f, 8.0f);
            auto gr = radial(x - 1.0f, y - 1.2f, 4.6f);
            gr.addColour(0.0, juce::Colour(0xffe2e5e8));
            gr.addColour(0.7, juce::Colour(0xff8a9097));
            gr.addColour(1.0, juce::Colour(0xff4e545a));
            g.setGradientFill(gr);
            g.fillEllipse(x - 3.5f, y - 3.5f, 7.0f, 7.0f);
            g.setColour(juce::Colour(0xff20252a));
            const float a = 0.6f + 1.3f * static_cast<float>(i);
            g.drawLine(x - 2.6f * std::cos(a), y - 2.6f * std::sin(a), x + 2.6f * std::cos(a), y + 2.6f * std::sin(a), 1.0f);
        }
    }

    // ---- printed panels behind the six module levers ----
    for (int m = 0; m < 6; ++m) {
        const juce::Rectangle<float> r(kTileX0 + kTilePitch * static_cast<float>(m), kTileY, kTileW, kTileH);
        g.setColour(juce::Colours::black.withAlpha(light_ ? 0.05f : 0.14f));
        g.fillRoundedRectangle(r, 14.0f);
        g.setColour(ink_.withAlpha(0.20f));
        g.drawRoundedRectangle(r.reduced(0.5f), 14.0f, 1.0f);
        g.setColour(ink_.withAlpha(0.82f));
        drawTracked(g, kModules[m].title, r.getX() + 76.0f, r.getY() + 17.0f, assets::font(500, 11.5f), 2.6f);
        g.setColour(look_.accent.withAlpha(0.45f));
        g.fillRect(r.getX() + 20.0f, r.getY() + 31.0f, kTileW - 40.0f, 1.0f);
    }
    // ---- captions over the lower rows ----
    {
        g.setColour(ink_.withAlpha(0.5f));
        const auto cap = assets::font(500, 9.5f);
        auto rule = [&](float x0, float x1, const char* text, float y) {
            drawTracked(g, text, (x0 + x1) * 0.5f, y, cap, 3.0f);
            const float tw = cap.getStringWidthFloat(text) + 3.0f * static_cast<float>(juce::String(text).length());
            g.setColour(ink_.withAlpha(0.18f));
            g.fillRect(x0, y - 0.5f, (x1 - x0) * 0.5f - tw * 0.5f - 10.0f, 1.0f);
            g.fillRect((x0 + x1) * 0.5f + tw * 0.5f + 10.0f, y - 0.5f, (x1 - x0) * 0.5f - tw * 0.5f - 10.0f, 1.0f);
            g.setColour(ink_.withAlpha(0.5f));
        };
        rule(70.0f, 452.0f, "ENVELOPE", 676.0f);
        rule(476.0f, 644.0f, "SCALE LOCK", 676.0f);
        rule(670.0f, 1050.0f, "VOICES AND FX", 676.0f);
    }

    // ---- knob seats, cast shadows and rings ----
    for (int i = 0; i < kNumKnobs; ++i) {
        const auto& d = kKnobs[i];
        const float s = d.mini ? kMini : 1.0f;
        g.saveState();
        g.addTransform(juce::AffineTransform::scale(s, s, d.x, d.y));
        const float ax = d.x - 49, ay = d.y - 49;
        juce::Path disc;
        disc.addEllipse(ax, ay, 98, 98);
        auto gr = radial(d.x, d.y, 69.3f);
        gr.addColour(0.0, body::seatA(look_.body));
        gr.addColour(0.56, body::seatB(look_.body));
        gr.addColour(1.0, body::seatC(look_.body));
        g.setGradientFill(gr);
        g.fillPath(disc);
        if (d.mini) {
            blitSet(g, mini_, atlas::knob_seat_ov, ax, ay);
            blitSet(g, mini_, atlas::knob_shadow, ax, ay);
            blitSet(g, mini_, atlas::knob_ring, ax, ay);
        } else {
            blit(g, atlas::knob_seat_ov, ax, ay);
            blit(g, atlas::knob_shadow, ax, ay);
            blit(g, atlas::knob_ring, ax, ay);
        }
        g.restoreState();
    }
    // ---- lever seats and nuts ----
    for (int t = 0; t < kNumToggles; ++t) {
        const float ax = kTog[t].x - 32, ay = kTog[t].y - 32;
        juce::Path disc;
        disc.addEllipse(ax + 10, ay + 10, 44, 44);
        auto gr = radial(kTog[t].x, kTog[t].y, 31.1f);
        gr.addColour(0.0, body::seatA(look_.body));
        gr.addColour(0.70, body::seatB(look_.body));
        gr.addColour(1.0, body::seatC(look_.body));
        g.setGradientFill(gr);
        g.fillPath(disc);
        blit(g, atlas::tog_seat_ov, ax, ay);
        blit(g, atlas::tog_nut, ax, ay);
    }
    // ---- indicator housings and the footswitch bases ----
    for (int i = 0; i < 3; ++i) blit(g, atlas::led_housing, kLed[i].x - 13, kLed[i].y - 13);
    for (int i = 0; i < 2; ++i) blit(g, atlas::foot_base, kFoot[i].x - 70, kFoot[i].y - 70);

    // ---- printed text that never changes ----
    g.setColour(ink_.withAlpha(0.78f));
    drawTracked(g, "PLAY", kTog[6].x, 806.0f, assets::font(500, 10.5f), 2.2f);
    drawTracked(g, "ON", kFoot[0].x, 938.0f, assets::font(500, 11.0f), 2.6f);
    drawTracked(g, "LATCH", kFoot[1].x, 938.0f, assets::font(500, 11.0f), 2.6f);
    g.setColour(ink_.withAlpha(0.45f));
    drawTracked(g, "HOLD = MOMENTARY", kFoot[1].x, 951.0f, assets::font(400, 7.5f), 1.4f);
    g.setColour(ink_.withAlpha(0.78f));
    drawTracked(g, "A", kTog[7].x - 50.0f, kTog[7].y, assets::font(500, 11.0f), 1.0f);
    drawTracked(g, "B", kTog[7].x + 50.0f, kTog[7].y, assets::font(500, 11.0f), 1.0f);
    g.setColour(ink_.withAlpha(0.5f));
    drawTracked(g, "LIVE", kTog[7].x, kTog[7].y + 40.0f, assets::font(400, 8.5f), 2.0f);
    // short rules either side of the wordmark
    g.setColour(look_.accent.withAlpha(0.5f));
    g.fillRect(430.0f, 76.0f, 260.0f, 1.0f);
}

// =================================================================================================== the moving parts
void PedalView::paintWordmark(juce::Graphics& g) {
    const float e = clamp01(wave_->energy());
    const float grains = wave_->grainActivity();
    const float live = e * (0.35f + 0.65f * grains);
    const float density = knobValue(2), scatter = knobValue(1), size = knobValue(3);
    // echo trails: more of them with Density, further apart with Scatter; they brighten with the sound
    const int n = 3 + juce::roundToInt(density * 5.0f);
    const float step = 3.0f + 14.0f * scatter;
    for (int k = n; k >= 1; --k) {
        const float side = (k % 2 == 1) ? -1.0f : 1.0f;
        const float dx = side * std::ceil(k * 0.5f) * step;
        const float dy = -1.6f * static_cast<float>(k);
        const float sc = 1.0f + 0.010f * static_cast<float>(k);
        const float alpha = (0.035f + 0.40f * live) * std::pow(0.8f, static_cast<float>(k - 1));
        const auto tf = juce::AffineTransform::scale(sc, sc, 560.0f, 50.0f).translated(dx, dy);
        g.setColour(look_.accent.withAlpha(alpha));
        g.fillPath(wordPath_, tf);
    }
    const float halo = (0.10f + 0.5f * live) * (0.4f + 0.6f * size);
    g.setColour(look_.accent.withAlpha(halo * 0.5f));
    g.strokePath(wordPath_, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(look_.accent.withAlpha(halo * 0.7f));
    g.strokePath(wordPath_, juce::PathStrokeType(2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(ink_.interpolatedWith(look_.accent.interpolatedWith(juce::Colours::white, 0.6f), clamp01(live * 0.8f)));
    g.fillPath(wordPath_);
}

void PedalView::paintKnob(juce::Graphics& g, int k) {
    const auto& d = kKnobs[k];
    const float s = d.mini ? kMini : 1.0f;
    const float cx = d.x, cy = d.y, ax = cx - 49, ay = cy - 49;
    const float v = knobValue(k);
    g.saveState();
    g.addTransform(juce::AffineTransform::scale(s, s, cx, cy));
    // ---- the dots around it ----
    {
        const int steps = d.steps;
        const bool selector = steps >= 2 && steps <= 16;
        const int n = selector ? steps : 11;
        const int sel = juce::roundToInt(v * (n - 1));
        for (int i = 0; i < n; ++i) {
            const double a = (-135.0 + i * 270.0 / (n - 1)) * kPi / 180.0;
            const float x = cx + 56.0f * static_cast<float>(std::sin(a));
            const float y = cy - 56.0f * static_cast<float>(std::cos(a));
            bool on;
            if (selector) on = i == sel;
            else if (d.bipolar) on = (sel >= n / 2) ? (i >= n / 2 && i <= sel) : (i <= n / 2 && i >= sel);
            else on = i <= sel;
            const auto& img = on ? dotLit_ : dotOff_;
            g.drawImage(img, juce::Rectangle<float>(x - 7.5f, y - 7.5f, static_cast<float>(img.getWidth() / u_), static_cast<float>(img.getHeight() / u_)),
                        juce::RectanglePlacement::stretchToFit);
        }
    }
    // ---- the knob ----
    const float ang = static_cast<float>((-135.0 + v * 270.0) * kPi / 180.0);
    {
        const auto& m = assets::Sprites::meta(atlas::knob_knurl);
        const auto& set = d.mini ? mini_ : sprites_;
        const auto& img = set.get(atlas::knob_knurl);
        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(ang, cx, cy));
        g.drawImage(img, juce::Rectangle<float>(ax + m.ox, ay + m.oy, static_cast<float>(img.getWidth() / set.scale()), static_cast<float>(img.getHeight() / set.scale())),
                    juce::RectanglePlacement::stretchToFit);
        g.restoreState();
    }
    if (d.mini) blitSet(g, mini_, atlas::knob_cap, ax, ay);
    else blit(g, atlas::knob_cap, ax, ay);
    {   // the indicator line, in the lights colour
        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(ang, cx, cy));
        const juce::Rectangle<float> bar(ax + 47, ay + 19, 4, 21);
        g.setColour(juce::Colour(0xffedeff0).withAlpha(0.65f));
        g.fillRoundedRectangle(bar.translated(0, 1), 2.0f);
        juce::ColourGradient gr(tint(juce::Colour(0xff4f0f23)), bar.getX(), 0, tint(juce::Colour(0xff4a0f21)), bar.getRight(), 0, false);
        gr.addColour(0.5, tint(juce::Colour(0xff942548)));
        g.setGradientFill(gr);
        g.fillRoundedRectangle(bar, 2.0f);
        g.restoreState();
    }
    if (d.mini) blitSet(g, mini_, atlas::knob_gloss, ax, ay);
    else blit(g, atlas::knob_gloss, ax, ay);
    g.restoreState();
    // ---- its name - or its value while you are on it ----
    const bool showValue = (hoverKnob_ == k) || (drag_.kind == Hit::Knob && drag_.index == k);
    g.setColour(showValue ? look_.accent.interpolatedWith(juce::Colours::white, light_ ? 0.0f : 0.55f) : ink_.withAlpha(0.8f));
    const float size = d.mini ? 9.5f : 10.5f;
    drawTracked(g, showValue ? knobText(k) : juce::String(d.name), cx, cy + (d.mini ? 48.0f : 57.0f),
                assets::font(showValue ? 600 : 500, size), showValue ? 1.2f : (d.mini ? 1.8f : 2.2f));
}

void PedalView::paintDynamic(juce::Graphics& g) {
    paintWordmark(g);
    for (int k = 0; k < kNumKnobs; ++k) paintKnob(g, k);

    // ---- the module levers: ON above, OFF below ----
    for (int m = 0; m < 6; ++m) {
        const bool on = togglePos(m) == 0;
        const float x = kTog[m].x;
        g.setColour(on ? look_.accent.interpolatedWith(juce::Colours::white, light_ ? 0.0f : 0.5f) : ink_.withAlpha(0.45f));
        drawTracked(g, "ON", x, kLeverY - 41.0f, assets::font(on ? 600 : 400, 9.5f), 1.8f);
        g.setColour(!on ? ink_.withAlpha(0.9f) : ink_.withAlpha(0.45f));
        drawTracked(g, "OFF", x, kLeverY + 43.0f, assets::font(!on ? 600 : 400, 9.5f), 1.8f);
    }
    // ---- PLAY lever legends (to the right of the lever) ----
    {
        const int pos = togglePos(6);
        for (int i = 0; i < 3; ++i) {
            const bool on = pos == i;
            const float rowY = kTog[6].y - 7 + (i - 1) * 14;
            const auto font = assets::font(on ? 600 : 400, 10.0f);
            const juce::String text = kPlayText[i];
            const float w = font.getStringWidthFloat(text);
            g.setColour(on ? ink_ : ink_.withAlpha(0.5f));
            g.setFont(font);
            g.drawText(text, juce::Rectangle<float>(kTog[6].x + 39, rowY - 1.0f, w + 6.0f, 14.0f), juce::Justification::centredLeft, false);
            const float dotX = kTog[6].x + 30, dotY = rowY + 5;
            if (on) {
                auto glow = radial(dotX + 2, dotY + 2, 5.0f);
                glow.addColour(0.0, look_.accent.withAlpha(0.65f));
                glow.addColour(1.0, look_.accent.withAlpha(0.0f));
                g.setGradientFill(glow);
                g.fillEllipse(dotX - 3, dotY - 3, 10, 10);
                g.setColour(look_.accent.interpolatedWith(juce::Colours::white, 0.55f));
            } else {
                g.setColour(look_.accent.darker(0.5f));
            }
            g.fillEllipse(dotX, dotY, 4, 4);
        }
    }
    // ---- the levers ----
    for (int t = 0; t < kNumToggles; ++t) {
        const float ax = kTog[t].x - 32, ay = kTog[t].y - 32;
        const int pos = togglePos(t);
        int id;
        if (t == 7) id = pos == 0 ? atlas::tog_lh_left : (pos == 1 ? atlas::tog_lv_mid : atlas::tog_lh_right);
        else if (t < 6) id = pos == 0 ? atlas::tog_lv_up : atlas::tog_lv_down;
        else id = pos == 0 ? atlas::tog_lv_up : (pos == 1 ? atlas::tog_lv_mid : atlas::tog_lv_down);
        blit(g, id, ax, ay);
    }
    // ---- indicator jewels: On, a preset slot is in use, Latch ----
    {
        const bool on[3] = {boolParam(ids::power), proc_.presets.slot() != 1, boolParam(ids::latch) || boolParam(ids::latchHold)};
        for (int i = 0; i < 3; ++i) {
            const float x = kLed[i].x, y = kLed[i].y;
            if (on[i]) {
                auto gr = radial(x, y, 66.5f);
                const auto c = tint(juce::Colour(0xffe2364f));
                gr.addColour(0.0, c.withAlpha(0.34f));
                gr.addColour(0.64, c.withAlpha(0.0f));
                gr.addColour(1.0, c.withAlpha(0.0f));
                g.setGradientFill(gr);
                g.fillEllipse(x - 47, y - 47, 94, 94);
            }
            blitImage(g, on[i] ? ledOn_ : ledOff_, x - 18, y - 18);
            g.saveState();
            g.addTransform(juce::AffineTransform::rotation(static_cast<float>(-30.0 * kPi / 180.0), x - 13 + 10.5f, y - 13 + 8.5f));
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.fillEllipse(x - 13 + 8, y - 13 + 7, 5, 3);
            g.restoreState();
        }
    }
    // ---- the footswitch caps ----
    for (int i = 0; i < 2; ++i) blit(g, pressed_[static_cast<size_t>(i)] ? atlas::foot_cap_down : atlas::foot_cap_up, kFoot[i].x - 70, kFoot[i].y - 70);
}

void PedalView::paint(juce::Graphics& g) {
    const double phys = g.getInternalContext().getPhysicalPixelScaleFactor();
    ensureCaches(phys);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(staticLayer_, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    g.saveState();
    g.addTransform(juce::AffineTransform::scale(scale_));
    g.addTransform(juce::AffineTransform::translation(kPedalX, kPedalY));
    paintDynamic(g);
    g.restoreState();
}

// =================================================================================================== mouse
juce::Point<float> PedalView::toPedal(juce::Point<float> p) const {
    return {p.x / scale_ - kPedalX, p.y / scale_ - kPedalY};
}

PedalView::Hit PedalView::hitTest(juce::Point<float> d) const {
    Hit h;
    for (int i = 0; i < kNumKnobs; ++i)
        if (d.getDistanceFrom({kKnobs[i].x, kKnobs[i].y}) <= (kKnobs[i].mini ? 41.0f : 50.0f)) { h.kind = Hit::Knob; h.index = i; return h; }
    for (int t = 0; t < 7; ++t)
        if (std::abs(d.x - kTog[t].x) <= 30.0f && std::abs(d.y - kTog[t].y) <= 34.0f) { h.kind = Hit::Toggle; h.index = t; return h; }
    if (std::abs(d.x - kTog[7].x) <= 62.0f && std::abs(d.y - kTog[7].y) <= 26.0f) { h.kind = Hit::Toggle; h.index = 7; return h; }
    for (int i = 0; i < 2; ++i)
        if (d.getDistanceFrom({kFoot[i].x, kFoot[i].y}) <= 36.0f) { h.kind = Hit::Foot; h.index = i; return h; }
    return h;
}

void PedalView::mouseMove(const juce::MouseEvent& e) {
    const auto h = hitTest(toPedal(e.position));
    setMouseCursor(h.kind == Hit::Knob ? juce::MouseCursor::UpDownResizeCursor
                   : h.kind == Hit::None ? juce::MouseCursor::NormalCursor : juce::MouseCursor::PointingHandCursor);
    const int hk = h.kind == Hit::Knob ? h.index : -1;
    if (hk != hoverKnob_) {
        hoverKnob_ = hk;
        repaint();
    }
}

void PedalView::mouseExit(const juce::MouseEvent&) {
    if (hoverKnob_ != -1) {
        hoverKnob_ = -1;
        repaint();
    }
}

// right-click a control: assign a MIDI controller knob to it
void PedalView::showMidiMenu(Hit hit, juce::Point<int> where) {
    const char* id = nullptr;
    if (hit.kind == Hit::Knob) id = kKnobs[hit.index].id;
    else if (hit.kind == Hit::Toggle && hit.index < 6) id = kModules[hit.index].onId;
    else if (hit.kind == Hit::Toggle && hit.index == 6) id = ids::play;
    else if (hit.kind == Hit::Foot) id = hit.index == 0 ? ids::power : ids::latch;
    auto* p = id != nullptr ? param(id) : nullptr;
    if (p == nullptr) return;
    const int cc = proc_.midiCcOf(p);
    juce::PopupMenu m;
    m.addItem(1, "Learn MIDI for " + p->getName(40) + "  (then move your controller)");
    if (cc >= 0) m.addItem(2, "Forget MIDI CC " + juce::String(cc));
    if (proc_.midiLearnTarget() != nullptr) m.addItem(3, "Cancel MIDI learn");
    juce::Component::SafePointer<PedalView> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(where.x, where.y, 1, 1)).withParentComponent(nullptr),
                    [safe, p](int r) {
                        if (safe == nullptr) return;
                        if (r == 1) safe->proc_.startMidiLearn(p);
                        else if (r == 2) safe->proc_.clearMidiMapping(p);
                        else if (r == 3) safe->proc_.cancelMidiLearn();
                    });
}

void PedalView::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) {
        drag_ = {};
        showMidiMenu(hitTest(toPedal(e.position)), e.getScreenPosition());
        return;
    }
    drag_ = hitTest(toPedal(e.position));
    lastDragPos_ = e.position;
    if (drag_.kind == Hit::Knob) {
        if (auto* p = param(kKnobs[drag_.index].id)) p->beginChangeGesture();
        dragValue_ = knobValue(drag_.index);
    } else if (drag_.kind == Hit::Toggle) {
        const auto d = toPedal(e.position);
        const int t = drag_.index;
        if (t == 7) {
            const float dx = d.x - kTog[7].x;
            setTogglePos(7, dx < -14.0f ? 0 : (dx > 14.0f ? 2 : 1));
        } else if (t < 6) {
            setTogglePos(t, d.y < kTog[t].y ? 0 : 2); // above the middle = ON, below = OFF
        } else {
            const int cur = togglePos(t);
            setTogglePos(t, d.y < kTog[t].y ? cur - 1 : cur + 1);
        }
        repaint();
    } else if (drag_.kind == Hit::Foot) {
        const auto i = static_cast<size_t>(drag_.index);
        pressed_[i] = true;
        holdActive_[i] = false;
        pressedAt_[i] = juce::Time::getMillisecondCounterHiRes();
        repaint();
    }
}

void PedalView::mouseDrag(const juce::MouseEvent& e) {
    if (drag_.kind != Hit::Knob) return;
    const auto delta = e.position - lastDragPos_;
    lastDragPos_ = e.position;
    // up or to the right turns it up; 220 design pixels is the whole range
    float change = ((-delta.y) + delta.x) / (220.0f * scale_);
    if (e.mods.isShiftDown()) change *= 0.2f;
    dragValue_ = clamp01(dragValue_ + change);
    setKnob(drag_.index, dragValue_);
    repaint();
}

void PedalView::mouseUp(const juce::MouseEvent&) {
    if (drag_.kind == Hit::Knob) {
        if (auto* p = param(kKnobs[drag_.index].id)) p->endChangeGesture();
    } else if (drag_.kind == Hit::Foot) {
        const auto i = static_cast<size_t>(drag_.index);
        if (holdActive_[i]) {
            if (auto* p = param(ids::latchHold)) { p->setValueNotifyingHost(0.0f); p->endChangeGesture(); }
        } else if (auto* p = param(drag_.index == 0 ? ids::power : ids::latch)) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
            p->endChangeGesture();
        }
        pressed_[i] = false;
        holdActive_[i] = false;
        repaint();
    }
    drag_ = {};
}

void PedalView::mouseDoubleClick(const juce::MouseEvent& e) {
    const auto h = hitTest(toPedal(e.position));
    if (h.kind != Hit::Knob) return;
    if (auto* p = param(kKnobs[h.index].id)) {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->getDefaultValue());
        p->endChangeGesture();
        dragValue_ = p->getDefaultValue();
        repaint();
    }
}

void PedalView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
    const auto h = hitTest(toPedal(e.position));
    const float dy = w.isReversed ? -w.deltaY : w.deltaY;
    if (h.kind == Hit::Knob) {
        auto* p = param(kKnobs[h.index].id);
        if (p == nullptr) return;
        const int steps = kKnobs[h.index].steps;
        if (steps > 1 && steps <= 16) { // a selector: one step per notch
            wheelAccum_ += dy;
            if (std::abs(wheelAccum_) >= 0.25f) {
                const int step = wheelAccum_ > 0 ? 1 : -1;
                wheelAccum_ = 0;
                p->beginChangeGesture();
                setKnob(h.index, knobValue(h.index) + static_cast<float>(step) / static_cast<float>(steps - 1));
                p->endChangeGesture();
            }
        } else {
            p->beginChangeGesture();
            const float notch = steps > 1 ? 1.0f / static_cast<float>(steps - 1) : 0.0f;
            setKnob(h.index, knobValue(h.index) + (steps > 1 ? (dy > 0 ? notch : (dy < 0 ? -notch : 0.0f)) : juce::jlimit(-0.06f, 0.06f, dy * 0.12f)));
            p->endChangeGesture();
        }
        repaint();
    } else if (h.kind == Hit::Toggle) {
        wheelAccum_ += dy;
        if (std::abs(wheelAccum_) >= 0.25f) {
            const int step = wheelAccum_ > 0 ? -1 : 1; // wheel up = lever up
            wheelAccum_ = 0;
            setTogglePos(h.index, togglePos(h.index) + step);
            repaint();
        }
    }
}

// =================================================================================================== housekeeping
void PedalView::timerCallback() {
    // holding the right footswitch latches only while it is held
    for (int i = 1; i < 2; ++i) {
        const auto k = static_cast<size_t>(i);
        if (pressed_[k] && !holdActive_[k] && juce::Time::getMillisecondCounterHiRes() - pressedAt_[k] >= kHoldMs) {
            holdActive_[k] = true;
            if (auto* p = param(ids::latchHold)) {
                p->beginChangeGesture();
                p->setValueNotifyingHost(1.0f);
            }
        }
    }
    // repaint when something the picture shows has changed (a knob moved by the host, a preset loaded, ...)
    std::array<float, 64> now{};
    for (int i = 0; i < kNumKnobs; ++i) now[static_cast<size_t>(i)] = knobValue(i);
    for (int t = 0; t < kNumToggles; ++t) now[static_cast<size_t>(kNumKnobs + t)] = static_cast<float>(toggleValue(t));
    const size_t b = static_cast<size_t>(kNumKnobs + kNumToggles);
    now[b] = boolParam(ids::power) ? 1.0f : 0.0f;
    now[b + 1] = (boolParam(ids::latch) || boolParam(ids::latchHold)) ? 1.0f : 0.0f;
    now[b + 2] = pressed_[0] ? 1.0f : 0.0f;
    now[b + 3] = pressed_[1] ? 1.0f : 0.0f;
    if (now != shown_) {
        shown_ = now;
        repaint();
        return;
    }
    // the wordmark's echo trails move with the sound: repaint just that strip when they change
    const int key = juce::roundToInt(wave_->energy() * 24.0f) * 16 + juce::roundToInt(wave_->grainActivity() * 8.0f);
    if (key != wordKey_) {
        wordKey_ = key;
        repaint(wordmarkArea().transformedBy(juce::AffineTransform::scale(scale_)).getSmallestIntegerContainer());
    }
}
