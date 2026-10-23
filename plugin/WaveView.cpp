#include "WaveView.h"

#include <cmath>

#include "Assets.h"
#include "ParamIds.h"

namespace {
const float kPi = 3.14159265f;
inline float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
inline float mapDb(float amp, float floorDb, float rangeDb) { return clamp01((20.0f * std::log10(amp + 1.0e-6f) - floorDb) / rangeDb); }
inline float follow(float cur, float target, float up, float down) { return cur + (target - cur) * (target > cur ? up : down); }

constexpr int kLowNote = 48;                                        // the keyboard starts at this MIDI note (a C)
constexpr int kWhiteKeys = 15;                                      // two octaves and the C above
const int kWhiteSemis[7] = {0, 2, 4, 5, 7, 9, 11};
bool blackAfter(int whiteIndex) { const int w = whiteIndex % 7; return w == 0 || w == 1 || w == 3 || w == 4 || w == 5; }

juce::Path makeWavePath(const mote::Sample& s, bool upper) {
    juce::Path p;
    const int n = static_cast<int>(s.ovMax.size());
    if (n < 2) return p;
    auto shape = [](float v) { return (v < 0 ? -1.0f : 1.0f) * std::pow(std::min(1.0f, std::fabs(v) / 0.9f), 0.72f); };
    (void)upper;
    for (int i = 0; i < n; ++i) {
        const float x = static_cast<float>(i) / (n - 1);
        const float y = -shape(s.ovMax[static_cast<size_t>(i)]);
        if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
    }
    for (int i = n - 1; i >= 0; --i) p.lineTo(static_cast<float>(i) / (n - 1), -shape(s.ovMin[static_cast<size_t>(i)]));
    p.closeSubPath();
    return p;
}
} // namespace

WaveView::WaveView(MoteProcessor& p) : proc_(p) {
    setOpaque(true);
    startTimerHz(30);
}

WaveView::~WaveView() { stopTimer(); }

void WaveView::setLook(juce::Colour accent, juce::Colour deep) {
    accent_ = accent;
    deep_ = deep;
    repaint();
}

void WaveView::setClip(VideoClipPtr clip) {
    clip_ = std::move(clip);
    clipStart_ = juce::Time::getMillisecondCounterHiRes() * 0.001;
    repaint();
}

void WaveView::say(const juce::String& text) {
    message_ = text;
    messageUntil_ = juce::Time::getMillisecondCounterHiRes() * 0.001 + 4.5;
    repaint();
}

// ------------------------------------------------------------------------------------------------ meters
void WaveView::readMeters() {
    const float level = proc_.visLevel.exchange(0.0f, std::memory_order_relaxed);
    const float lo = mapDb(std::sqrt(proc_.visLo.load(std::memory_order_relaxed)), -56.0f, 40.0f);
    const float mid = mapDb(std::sqrt(proc_.visMid.load(std::memory_order_relaxed)), -58.0f, 40.0f);
    const float hi = mapDb(std::sqrt(proc_.visHi.load(std::memory_order_relaxed)), -62.0f, 40.0f);
    energy_ = follow(energy_, level, 0.45f, 0.08f);
    lo_ = follow(lo_, lo, 0.4f, 0.1f);
    mid_ = follow(mid_, mid, 0.4f, 0.1f);
    hi_ = follow(hi_, hi, 0.5f, 0.2f);
    grains_ = follow(grains_, clamp01(static_cast<float>(proc_.engine().activeGrains()) / 90.0f), 0.4f, 0.1f);
    const double beat = proc_.uiBeat.load(std::memory_order_relaxed);
    beatPulse_ = std::exp(-static_cast<float>(beat - std::floor(beat)) * 5.0f);
}

void WaveView::rebuildWave() {
    auto a = proc_.sampleA(), b = proc_.sampleB();
    if (a != drawnA_) {
        drawnA_ = a;
        waveA_ = a ? makeWavePath(*a, true) : juce::Path();
    }
    if (b != drawnB_) {
        drawnB_ = b;
        waveB_ = b ? makeWavePath(*b, true) : juce::Path();
    }
}

void WaveView::timerCallback() {
    if (!still_) {
        now_ = juce::Time::getMillisecondCounterHiRes() * 0.001;
        readMeters();
    }
    rebuildWave();
    if (isShowing()) repaint();
}

// ------------------------------------------------------------------------------------------------ drawing
void WaveView::drawBackdrop(juce::Graphics& g) const {
    juce::ColourGradient bg(juce::Colour(0xff03080a).interpolatedWith(deep_, 0.35f), 0, 0, juce::Colour(0xff091014).interpolatedWith(deep_, 0.25f), 0, kH, false);
    g.setGradientFill(bg);
    g.fillRect(0.0f, 0.0f, kW, kH);
    // a soft glow behind the middle that breathes with the level
    juce::ColourGradient glow(accent_.withAlpha(0.05f + 0.13f * energy_ + 0.05f * beatPulse_), kW * 0.5f, waveArea().getCentreY(),
                              accent_.withAlpha(0.0f), kW * 0.5f, waveArea().getCentreY() + 150.0f, true);
    g.setGradientFill(glow);
    g.fillRect(0.0f, 0.0f, kW, kH);
    // faint graph lines: a tick every tenth of the sample, and the centre line
    const auto wa = waveArea();
    g.setColour(juce::Colours::white.withAlpha(0.035f));
    for (int i = 0; i <= 10; ++i) g.fillRect(wa.getX() + wa.getWidth() * static_cast<float>(i) / 10.0f - 0.5f, wa.getY(), 1.0f, wa.getHeight());
    g.setColour(accent_.withAlpha(0.18f));
    g.fillRect(wa.getX(), wa.getCentreY() - 0.5f, wa.getWidth(), 1.0f);
}

void WaveView::drawWave(juce::Graphics& g) const {
    const auto wa = waveArea();
    const float half = wa.getHeight() * 0.5f - 6.0f;
    const auto tf = juce::AffineTransform::scale(wa.getWidth(), half).translated(wa.getX(), wa.getCentreY());
    const bool morph = proc_.apvts.getRawParameterValue(ids::morphOn)->load() > 0.5f;
    if (morph && !waveB_.isEmpty()) { // the second sample, as a ghost
        const float amt = proc_.engine().morphShown();
        g.setColour(accent_.interpolatedWith(juce::Colours::white, 0.5f).withAlpha(0.10f + 0.28f * amt));
        g.strokePath(waveB_, juce::PathStrokeType(1.4f), tf);
    }
    if (waveA_.isEmpty()) return;
    juce::ColourGradient fill(accent_.withAlpha(0.20f), 0, wa.getCentreY() - half, accent_.withAlpha(0.20f), 0, wa.getCentreY() + half, false);
    fill.addColour(0.5, accent_.interpolatedWith(juce::Colours::white, 0.15f).withAlpha(0.62f));
    g.setGradientFill(fill);
    g.fillPath(waveA_, tf);
    g.setColour(accent_.interpolatedWith(juce::Colours::white, 0.45f).withAlpha(0.55f));
    g.strokePath(waveA_, juce::PathStrokeType(0.9f), tf);
}

void WaveView::drawGrains(juce::Graphics& g) const {
    const auto wa = waveArea();
    const auto& e = proc_.engine();
    auto s = drawnA_;
    const double secs = s ? std::max(0.05, s->seconds()) : 1.0;
    const double now = e.now();
    for (int i = 0; i < mote::Engine::kVizGrains; ++i) {
        const auto& z = e.viz(i);
        const double age = now - z.born.load(std::memory_order_relaxed);
        if (age < 0.0 || age > 0.95) continue;
        const float life = 1.0f - static_cast<float>(age / 0.95);
        const float amp = clamp01(z.amp.load(std::memory_order_relaxed));
        const float x = xOf(clamp01(z.pos.load(std::memory_order_relaxed)));
        const float w = juce::jlimit(2.0f, 70.0f, static_cast<float>(z.seconds.load(std::memory_order_relaxed) / secs) * wa.getWidth());
        const float semis = juce::jlimit(-24.0f, 24.0f, z.semis.load(std::memory_order_relaxed));
        const float y = wa.getCentreY() - semis / 24.0f * (wa.getHeight() * 0.5f - 18.0f);
        const float hgt = 8.0f + 18.0f * amp;
        const float a = std::pow(life, 1.4f) * (0.30f + 0.70f * amp);
        const auto c = accent_.interpolatedWith(juce::Colours::white, 0.25f + 0.35f * life);
        // the grain's real length as a faint soft block, with a thin bright core where it starts
        g.setColour(c.withAlpha(a * 0.13f));
        g.fillRoundedRectangle(x - w * 0.5f, y - hgt * 0.5f - 2.0f, w, hgt + 4.0f, juce::jmin(5.0f, w * 0.5f));
        const float core = juce::jlimit(2.0f, 5.0f, w * 0.18f);
        g.setColour(c.withAlpha(a * 0.28f));
        g.fillRoundedRectangle(x - core * 0.5f - 2.0f, y - hgt * 0.5f - 2.0f, core + 4.0f, hgt + 4.0f, 3.0f);
        g.setColour(c.withAlpha(a * 0.95f));
        g.fillRoundedRectangle(x - core * 0.5f, y - hgt * 0.5f, core, hgt, core * 0.5f);
    }
}

void WaveView::drawMarker(juce::Graphics& g) const {
    const auto wa = waveArea();
    const auto& e = proc_.engine();
    auto* mode = proc_.apvts.getRawParameterValue(ids::play);
    const bool walk = proc_.apvts.getRawParameterValue(ids::walkOn)->load() > 0.5f;
    const bool magnet = proc_.apvts.getRawParameterValue(ids::magnet)->load() > 0.02f;
    // slice ticks (what WALK steps through and MAGNET pulls to)
    if ((walk || magnet) && drawnA_) {
        const auto list = mote::sliceList(drawnA_.get());
        g.setColour(juce::Colours::white.withAlpha(walk ? 0.22f : 0.12f));
        for (float p : list) g.fillRect(xOf(p) - 0.5f, wa.getY() + 2.0f, 1.0f, wa.getHeight() - 4.0f);
        if (walk) {
            const float cur = e.walkSlicePos();
            float next = 1.0f;
            for (float p : list)
                if (p > cur + 1.0e-4f) { next = p; break; }
            juce::ColourGradient gr(accent_.withAlpha(0.30f), 0, wa.getY(), accent_.withAlpha(0.06f), 0, wa.getBottom(), false);
            g.setGradientFill(gr);
            g.fillRect(juce::Rectangle<float>(xOf(cur), wa.getY(), std::max(2.0f, xOf(next) - xOf(cur)), wa.getHeight()));
        }
    }
    // the scatter band around the position
    const float pos = clamp01(e.posShown());
    const float sc = e.scatterShown();
    if (sc > 0.002f) {
        const float x0 = xOf(clamp01(pos - sc)), x1 = xOf(clamp01(pos + sc));
        juce::ColourGradient gr(accent_.withAlpha(0.0f), x0, 0, accent_.withAlpha(0.0f), x1, 0, false);
        gr.addColour(0.5, accent_.withAlpha(0.20f));
        g.setGradientFill(gr);
        g.fillRect(juce::Rectangle<float>(x0, wa.getY(), std::max(2.0f, x1 - x0), wa.getHeight()));
    }
    // play-heads of held notes (they move in TRAVEL and BOUNCE)
    if (mode != nullptr && mode->load() > 0.5f) {
        for (int v = 0; v < mote::Engine::kMaxVoices; ++v) {
            const float lv = e.voiceLevel(v);
            if (lv < 0.02f) continue;
            g.setColour(juce::Colours::white.withAlpha(0.15f + 0.55f * clamp01(lv)));
            g.fillRect(xOf(clamp01(e.voicePos(v))) - 0.5f, wa.getY() + 3.0f, 1.0f, wa.getHeight() - 6.0f);
        }
    }
    // the position line and its handle
    const float x = xOf(pos);
    g.setColour(accent_.withAlpha(0.35f));
    g.fillRect(x - 2.0f, wa.getY(), 4.0f, wa.getHeight());
    g.setColour(accent_.interpolatedWith(juce::Colours::white, 0.7f));
    g.fillRect(x - 0.75f, wa.getY(), 1.5f, wa.getHeight());
    juce::Path tri;
    tri.addTriangle(x - 6.0f, wa.getY() - 1.0f, x + 6.0f, wa.getY() - 1.0f, x, wa.getY() + 8.0f);
    g.fillPath(tri);
    juce::Path tri2;
    tri2.addTriangle(x - 6.0f, wa.getBottom() + 1.0f, x + 6.0f, wa.getBottom() + 1.0f, x, wa.getBottom() - 8.0f);
    g.fillPath(tri2);
}

void WaveView::drawHud(juce::Graphics& g) const {
    auto s = drawnA_;
    // the sample name chip (click it to choose another)
    {
        const auto r = nameChip();
        g.setColour(juce::Colours::black.withAlpha(hoverChip_ ? 0.55f : 0.35f));
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(accent_.withAlpha(hoverChip_ ? 0.8f : 0.4f));
        g.drawRoundedRectangle(r.reduced(0.5f), 5.0f, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.88f));
        g.setFont(assets::font(500, 12.0f));
        const juce::String txt = s ? (juce::String(s->name).toUpperCase() + "   " + juce::String(s->seconds(), 1) + " s") : juce::String("NO SAMPLE");
        g.drawText(txt, r.reduced(10.0f, 0.0f).withTrimmedRight(16.0f), juce::Justification::centredLeft, true);
        juce::Path arrow;
        arrow.addTriangle(r.getRight() - 16.0f, r.getCentreY() - 2.5f, r.getRight() - 8.0f, r.getCentreY() - 2.5f, r.getRight() - 12.0f, r.getCentreY() + 3.0f);
        g.fillPath(arrow);
    }
    if (proc_.apvts.getRawParameterValue(ids::morphOn)->load() > 0.5f) {
        const auto r = morphChip();
        auto b = proc_.sampleB();
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(accent_.withAlpha(0.3f));
        g.drawRoundedRectangle(r.reduced(0.5f), 5.0f, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.65f));
        g.setFont(assets::font(500, 11.5f));
        g.drawText("MORPH TO   " + (b ? juce::String(b->name).toUpperCase() : juce::String("-")), r.reduced(10.0f, 0.0f), juce::Justification::centredLeft, true);
    }
    // status chips on the right
    auto& e = proc_.engine();
    juce::StringArray chips;
    if (proc_.apvts.getRawParameterValue(ids::walkOn)->load() > 0.5f) chips.add("WALK " + juce::String(e.walkStep() + 1) + "/" + juce::String(juce::roundToInt(proc_.apvts.getRawParameterValue(ids::walkSteps)->load())));
    if (e.repeating()) chips.add("REPEAT");
    chips.add(juce::String(juce::roundToInt(proc_.tempoBpm.load())) + " BPM" + (proc_.tempoFromHost.load() ? "" : " (own)"));
    float x = kW - 12.0f;
    g.setFont(assets::font(500, 11.0f));
    for (int i = chips.size() - 1; i >= 0; --i) {
        const float w = g.getCurrentFont().getStringWidthFloat(chips[i]) + 18.0f;
        const juce::Rectangle<float> r(x - w, 6.0f, w, 22.0f);
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(accent_.withAlpha(i == chips.size() - 1 ? 0.25f : 0.6f));
        g.drawRoundedRectangle(r.reduced(0.5f), 5.0f, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(i == chips.size() - 1 ? 0.55f : 0.9f));
        g.drawText(chips[i], r, juce::Justification::centred);
        x -= w + 6.0f;
    }
    // a hint when nothing of yours is loaded
    if (!proc_.hasCustom(false)) {
        g.setColour(juce::Colours::white.withAlpha(0.38f));
        g.setFont(assets::font(500, 9.5f));
        g.drawFittedText("DROP A SOUND FILE\nON THIS WINDOW", juce::Rectangle<float>(keysArea().getRight() + 14.0f, keysArea().getY() - 2.0f, 126.0f, 32.0f).toNearestInt(),
                         juce::Justification::centredLeft, 2, 1.0f);
    }
    const double nowS = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto note = proc_.sampleNotice();
    juce::String say = nowS < messageUntil_ ? message_ : note;
    if (say.isNotEmpty()) {
        g.setColour(juce::Colour(0xffffd27a).withAlpha(0.95f));
        g.setFont(assets::font(500, 11.5f));
        g.drawText(say, juce::Rectangle<float>(12.0f, 30.0f, kW - 24.0f, 16.0f), juce::Justification::centredLeft);
    }
}

void WaveView::drawKeys(juce::Graphics& g) const {
    const auto ka = keysArea();
    const float ww = ka.getWidth() / kWhiteKeys;
    const int held = heldKey_;
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.setFont(assets::font(500, 9.5f));
    g.drawText("PLAY", juce::Rectangle<float>(ka.getX() - 70.0f, ka.getY(), 60.0f, ka.getHeight()), juce::Justification::centredRight);
    // white keys
    for (int i = 0; i < kWhiteKeys; ++i) {
        const int note = kLowNote + (i / 7) * 12 + kWhiteSemis[i % 7];
        const juce::Rectangle<float> r(ka.getX() + ww * i + 0.5f, ka.getY(), ww - 1.0f, ka.getHeight());
        const bool on = note == held;
        g.setColour(on ? accent_.interpolatedWith(juce::Colours::white, 0.5f).withAlpha(0.95f) : juce::Colours::white.withAlpha(0.20f));
        g.fillRoundedRectangle(r, 2.5f);
    }
    // black keys
    for (int i = 0; i < kWhiteKeys - 1; ++i) {
        if (!blackAfter(i)) continue;
        const int note = kLowNote + (i / 7) * 12 + kWhiteSemis[i % 7] + 1;
        const juce::Rectangle<float> r(ka.getX() + ww * (i + 1) - ww * 0.3f, ka.getY(), ww * 0.6f, ka.getHeight() * 0.62f);
        const bool on = note == held;
        g.setColour(on ? accent_ : juce::Colour(0xff060a0c).withAlpha(0.92f));
        g.fillRoundedRectangle(r, 2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.drawRoundedRectangle(r.reduced(0.5f), 2.0f, 0.8f);
    }
}

void WaveView::drawGlass(juce::Graphics& g) const {
    const float edge = 22.0f;
    for (int side = 0; side < 4; ++side) {
        juce::Rectangle<float> r;
        juce::ColourGradient gr;
        switch (side) {
            case 0: r = {0, 0, kW, edge};         gr = juce::ColourGradient(juce::Colours::black.withAlpha(0.7f), 0, 0, juce::Colours::transparentBlack, 0, edge, false); break;
            case 1: r = {0, kH - edge, kW, edge}; gr = juce::ColourGradient(juce::Colours::black.withAlpha(0.7f), 0, kH, juce::Colours::transparentBlack, 0, kH - edge, false); break;
            case 2: r = {0, 0, edge, kH};         gr = juce::ColourGradient(juce::Colours::black.withAlpha(0.6f), 0, 0, juce::Colours::transparentBlack, edge, 0, false); break;
            default: r = {kW - edge, 0, edge, kH}; gr = juce::ColourGradient(juce::Colours::black.withAlpha(0.6f), kW, 0, juce::Colours::transparentBlack, kW - edge, 0, false); break;
        }
        g.setGradientFill(gr);
        g.fillRect(r);
    }
    juce::Path sheen;
    sheen.addTriangle(0, 0, kW * 0.55f, 0, 0, kH * 0.9f);
    juce::ColourGradient sg(juce::Colours::white.withAlpha(0.06f), 0, 0, juce::Colours::white.withAlpha(0.0f), kW * 0.28f, kH * 0.4f, false);
    g.setGradientFill(sg);
    g.fillPath(sheen);
    if (dropHover_) {
        g.setColour(accent_.withAlpha(0.22f));
        g.fillRect(0.0f, 0.0f, kW, kH);
        g.setColour(accent_.interpolatedWith(juce::Colours::white, 0.5f));
        g.drawRoundedRectangle(6.0f, 6.0f, kW - 12.0f, kH - 12.0f, 12.0f, 2.5f);
        g.setFont(assets::font(600, 26.0f));
        g.drawText("DROP TO PLAY THIS SOUND", juce::Rectangle<float>(0, 0, kW, kH), juce::Justification::centred);
    }
}

void WaveView::paint(juce::Graphics& g) {
    const float s = getWidth() / kW;
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    juce::Path clipShape;
    clipShape.addRoundedRectangle(0.0f, 0.0f, static_cast<float>(getWidth()), static_cast<float>(getHeight()), 14.0f * s);
    g.fillAll(juce::Colour(0xff030405));
    g.reduceClipRegion(clipShape);
    g.addTransform(juce::AffineTransform::scale(s));

    drawBackdrop(g);
    if (clip_ != nullptr && !clip_->frames.empty()) {
        const double t = now_ - clipStart_;
        const auto& frame = clip_->frames[static_cast<size_t>(clip_->frameAt(t))];
        g.drawImage(frame, juce::Rectangle<float>(0, 0, kW, kH), juce::RectanglePlacement::stretchToFit);
        // the picture breathes with the sound: dim when quiet, brighter and tinted when it plays
        const float bright = clamp01(0.30f + 0.55f * energy_ + 0.25f * grains_);
        g.setColour(juce::Colours::black.withAlpha(clamp01(1.0f - bright)));
        g.fillRect(0.0f, 0.0f, kW, kH);
        g.setColour(accent_.withAlpha(0.05f + 0.14f * energy_ + 0.08f * beatPulse_));
        g.fillRect(0.0f, 0.0f, kW, kH);
    }
    drawWave(g);
    drawMarker(g);
    drawGrains(g);
    drawHud(g);
    drawKeys(g);
    drawGlass(g);
}

// ------------------------------------------------------------------------------------------------ mouse
int WaveView::keyAt(juce::Point<float> p) const {
    const auto ka = keysArea();
    if (!ka.expanded(0.0f, 2.0f).contains(p)) return -1;
    const float ww = ka.getWidth() / kWhiteKeys;
    if (p.y < ka.getY() + ka.getHeight() * 0.62f)
        for (int i = 0; i < kWhiteKeys - 1; ++i) {
            if (!blackAfter(i)) continue;
            const juce::Rectangle<float> r(ka.getX() + ww * (i + 1) - ww * 0.3f, ka.getY(), ww * 0.6f, ka.getHeight() * 0.62f);
            if (r.contains(p)) return kLowNote + (i / 7) * 12 + kWhiteSemis[i % 7] + 1;
        }
    const int i = juce::jlimit(0, kWhiteKeys - 1, static_cast<int>((p.x - ka.getX()) / ww));
    return kLowNote + (i / 7) * 12 + kWhiteSemis[i % 7];
}

void WaveView::mouseMove(const juce::MouseEvent& e) {
    const auto p = e.position / (getWidth() / kW);
    const bool chip = nameChip().contains(p);
    if (chip != hoverChip_) {
        hoverChip_ = chip;
        repaint();
    }
    setMouseCursor(chip ? juce::MouseCursor::PointingHandCursor
                        : (waveArea().expanded(0.0f, 4.0f).contains(p) ? juce::MouseCursor::LeftRightResizeCursor
                                                                         : (keysArea().contains(p) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor)));
}

void WaveView::mouseDown(const juce::MouseEvent& e) {
    const auto p = e.position / (getWidth() / kW);
    if (e.mods.isPopupMenu()) {
        if (auto* prm = proc_.apvts.getParameter(ids::position)) {
            const int cc = proc_.midiCcOf(prm);
            juce::PopupMenu m;
            m.addItem(1, "Learn MIDI for Position  (then move your controller)");
            if (cc >= 0) m.addItem(2, "Forget MIDI CC " + juce::String(cc));
            juce::Component::SafePointer<WaveView> safe(this);
            m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(e.getScreenX(), e.getScreenY(), 1, 1)), [safe, prm](int r) {
                if (safe == nullptr) return;
                if (r == 1) safe->proc_.startMidiLearn(prm);
                else if (r == 2) safe->proc_.clearMidiMapping(prm);
            });
        }
        return;
    }
    if (nameChip().contains(p)) { showSampleMenu(false); return; }
    if (proc_.apvts.getRawParameterValue(ids::morphOn)->load() > 0.5f && morphChip().contains(p)) { showSampleMenu(true); return; }
    const int key = keyAt(p);
    if (key >= 0) {
        heldKey_ = key;
        proc_.auditionOn(key);
        repaint();
        return;
    }
    if (waveArea().expanded(0.0f, 6.0f).contains(p)) {
        draggingPos_ = true;
        if (auto* prm = proc_.apvts.getParameter(ids::position)) prm->beginChangeGesture();
        mouseDrag(e);
    }
}

void WaveView::mouseDrag(const juce::MouseEvent& e) {
    const auto p = e.position / (getWidth() / kW);
    if (draggingPos_) {
        if (auto* prm = proc_.apvts.getParameter(ids::position)) prm->setValueNotifyingHost(clamp01((p.x - waveArea().getX()) / waveArea().getWidth()));
        repaint();
    } else if (heldKey_ >= 0) {
        const int key = keyAt(p);
        if (key >= 0 && key != heldKey_) {
            heldKey_ = key;
            proc_.auditionOn(key);
            repaint();
        }
    }
}

void WaveView::mouseUp(const juce::MouseEvent&) {
    if (draggingPos_) {
        if (auto* prm = proc_.apvts.getParameter(ids::position)) prm->endChangeGesture();
        draggingPos_ = false;
    }
    if (heldKey_ >= 0) {
        heldKey_ = -1;
        proc_.auditionOff();
        repaint();
    }
}

void WaveView::mouseDoubleClick(const juce::MouseEvent& e) {
    const auto p = e.position / (getWidth() / kW);
    if (waveArea().contains(p))
        if (auto* prm = proc_.apvts.getParameter(ids::position)) {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost(prm->getDefaultValue());
            prm->endChangeGesture();
        }
}

// ------------------------------------------------------------------------------------------------ samples
void WaveView::showSampleMenu(bool slotB) {
    juce::PopupMenu m;
    m.addSectionHeader(slotB ? "Morph to ..." : "Play ...");
    const int current = juce::roundToInt(proc_.apvts.getRawParameterValue(slotB ? ids::morphSource : ids::source)->load());
    for (int i = 0; i < mote::sources::kCount; ++i) m.addItem(100 + i, mote::sources::name(i), true, current == i);
    if (proc_.hasCustom(slotB)) m.addItem(100 + mote::sources::kCustom, "Your sample: " + proc_.customName(slotB), true, current == mote::sources::kCustom);
    m.addSeparator();
    m.addItem(1, "Load a sound file...");
    if (proc_.hasCustom(slotB)) m.addItem(2, "Forget my sample");
    juce::Component::SafePointer<WaveView> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [safe, slotB](int r) {
        if (safe == nullptr || r == 0) return;
        if (r == 1) safe->chooseFile(slotB);
        else if (r == 2) safe->proc_.clearCustom(slotB);
        else safe->proc_.setSource(r - 100, slotB);
    });
}

void WaveView::chooseFile(bool slotB) {
    chooser_ = std::make_unique<juce::FileChooser>("Choose a sound file", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3;*.m4a");
    juce::Component::SafePointer<WaveView> safe(this);
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe, slotB](const juce::FileChooser& fc) {
        if (safe == nullptr) return;
        const auto f = fc.getResult();
        if (f.existsAsFile()) safe->loadFile(f, slotB);
    });
}

void WaveView::loadFile(const juce::File& f, bool slotB) {
    const auto err = proc_.loadSampleFile(f, slotB);
    if (err.isNotEmpty()) say(err);
    else if (proc_.sampleNotice().isNotEmpty()) say(proc_.sampleNotice());
    else say({});
    repaint();
}

bool WaveView::isInterestedInFileDrag(const juce::StringArray& files) {
    for (auto& f : files) {
        const auto ext = juce::File(f).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".ogg" || ext == ".mp3" || ext == ".m4a") return true;
    }
    return false;
}

void WaveView::filesDropped(const juce::StringArray& files, int x, int) {
    dropHover_ = false;
    const bool toMorph = proc_.apvts.getRawParameterValue(ids::morphOn)->load() > 0.5f && x > getWidth() * 0.5f; // right half = the morph sample
    for (auto& f : files) {
        const juce::File file(f);
        if (isInterestedInFileDrag(juce::StringArray(f))) {
            loadFile(file, toMorph);
            return;
        }
    }
    say("That is not a sound file. WAV, AIFF, FLAC, OGG and MP3 work.");
}
