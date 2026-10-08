#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "1.1.0"
#endif

/*  v1.1 startup screen.
    Amari Labs mark inside a glowing eclipse, "AMARI LABS", the ARCANE ECLIPSE
    nameplate and the version, over the dimmed, out-of-focus panel. Any key or
    click "powers on" the panel: the screen fades out and calls onDismissed.
    Visual only — audio is never blocked by it.                                */
class AESplash : public juce::Component, private juce::Timer, private juce::KeyListener
{
public:
    std::function<void()> onDismissed;

    AESplash()
    {
        setOpaque(false);
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(true);
        startTimerHz(30);
    }

    ~AESplash() override { detachKeys(); }

    // Keys are caught two ways: directly when we have focus, and through a key
    // listener on the window itself (hosts often give the plugin window focus
    // only after it is already on screen, or never to a child component).
    void parentHierarchyChanged() override { attachKeys(); }
    void visibilityChanged() override      { attachKeys(); if (isShowing()) grabKeyboardFocus(); }

    void paint(juce::Graphics& g) override
    {
        using juce::Colour;
        const float w = (float) getWidth(), h = (float) getHeight();
        const float sx = w / 1200.f, sy = h / 823.f;          // designed at 1200 x 823

        // dimmed, out-of-focus panel: the art drawn tiny then scaled up = a cheap blur
        g.fillAll(Colour(0xff07050c));
        if (auto& b = blurredPanel(); b.isValid()) {
            g.setOpacity(0.13f);
            g.drawImage(b, getLocalBounds().toFloat());
            g.setOpacity(1.f);
        }
        { juce::ColourGradient bloom(Colour(0x4d5a28aa), w * 0.5f, h * 0.38f,
                                     Colour(0x005a28aa), w * 0.5f, h * 0.38f + 480.f * sy, true);
          g.setGradientFill(bloom); g.fillRect(getLocalBounds()); }
        { juce::ColourGradient vig(Colour(0x0007050c), w * 0.5f, h * 0.5f,
                                   Colour(0xe607050c), 0.f, 0.f, true);
          vig.addColour(0.55, Colour(0x0007050c));
          g.setGradientFill(vig); g.fillRect(getLocalBounds()); }

        // eclipse: soft corona rings, black disc, thin bright rim
        const juce::Point<float> c (600.f * sx, 290.f * sy);
        const float R = 155.f * sy;
        {
            const float outer = R + 150.f * sy, k0 = R / outer;
            juce::ColourGradient corona (Colour(0xff9b59ff).withAlpha(0.55f), c.x, c.y,
                                         Colour(0x009b59ff), c.x + outer, c.y, true);
            corona.addColour(k0,                         Colour(0xff9b59ff).withAlpha(0.55f));
            corona.addColour(k0 + (1.0 - k0) * 0.12,     Colour(0xff8a4cf0).withAlpha(0.32f));
            corona.addColour(k0 + (1.0 - k0) * 0.40,     Colour(0xff7a40e0).withAlpha(0.12f));
            g.setGradientFill(corona);
            g.fillEllipse(c.x - outer, c.y - outer, outer * 2.f, outer * 2.f);
        }
        g.setColour(Colour(0xffb784ff).withAlpha(0.55f));
        g.fillEllipse(c.x - R - 4.f, c.y - R - 4.f, (R + 4.f) * 2.f, (R + 4.f) * 2.f);
        g.setColour(Colour(0xff05030a));
        g.fillEllipse(c.x - R, c.y - R, R * 2.f, R * 2.f);
        g.setColour(Colour(0xffe8d4ff).withAlpha(0.85f));
        g.drawEllipse(c.x - R, c.y - R, R * 2.f, R * 2.f, 1.4f);

        // Amari Labs mark
        static juce::Image mark = juce::ImageCache::getFromMemory(BinaryData::logo_amari_mark_v2_png,
                                                                 BinaryData::logo_amari_mark_v2_pngSize);
        if (mark.isValid()) {
            const float mw = 330.f * sx, mh = mw * (float) mark.getHeight() / (float) mark.getWidth();
            juce::Rectangle<float> r (0.f, 0.f, mw, mh);
            r.setCentre(c.x, 296.f * sy);
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImage(mark, r, juce::RectanglePlacement::stretchToFit);
        }

        auto spaced = [&](const juce::String& t, juce::Font f, float kern, Colour col, float y, float hgt) {
            f.setExtraKerningFactor(kern);
            g.setFont(f); g.setColour(col);
            g.drawText(t, juce::Rectangle<float>(0.f, y * sy, w, hgt * sy), juce::Justification::centred, false);
        };
        const juce::Font sans (juce::FontOptions(13.f * sy).withStyle("Bold"));

        spaced("AMARI LABS", sans, 0.68f, Colour(0xffbdb3d6), 468.f, 22.f);

        // ARCANE — silver nameplate lettering with a drop shadow
        {
            juce::Font f = cinzel(94.f * sy); f.setExtraKerningFactor(0.20f);
            g.setFont(f);
            auto box = juce::Rectangle<float>(0.f, 500.f * sy, w, 100.f * sy);
            g.setColour(Colour(0x99000000));
            g.drawText("ARCANE", box.translated(0.f, 4.f * sy), juce::Justification::centred, false);
            juce::ColourGradient silver(Colour(0xffffffff), 0.f, box.getY() + 14.f * sy,
                                        Colour(0xff9a9db0), 0.f, box.getBottom() - 14.f * sy, false);
            silver.addColour(0.45, Colour(0xffd9dbe3));
            g.setGradientFill(silver);
            g.drawText("ARCANE", box, juce::Justification::centred, false);
        }
        // ECLIPSE with fading rules either side
        {
            juce::Font f = cinzel(30.f * sy); f.setExtraKerningFactor(0.48f);
            const float y = 604.f * sy, hh = 34.f * sy;
            const float tw = juce::GlyphArrangement::getStringWidth(f, "ECLIPSE");
            const float lineW = 120.f * sx, gap = 22.f * sx, cy = y + hh * 0.5f;
            juce::ColourGradient l(Colour(0x009b59ff), c.x - tw * 0.5f - gap - lineW, cy, Colour(0xff9b59ff), c.x - tw * 0.5f - gap, cy, false);
            g.setGradientFill(l); g.fillRect(c.x - tw * 0.5f - gap - lineW, cy - 1.f, lineW, 2.f);
            juce::ColourGradient r(Colour(0xff9b59ff), c.x + tw * 0.5f + gap, cy, Colour(0x009b59ff), c.x + tw * 0.5f + gap + lineW, cy, false);
            g.setGradientFill(r); g.fillRect(c.x + tw * 0.5f + gap, cy - 1.f, lineW, 2.f);
            g.setFont(f);
            g.setColour(Colour(0x55a064ff));
            g.drawText("ECLIPSE", juce::Rectangle<float>(0.f, y, w, hh).expanded(0.f, 1.f), juce::Justification::centred, false);
            g.setColour(Colour(0xffb57cff));
            g.drawText("ECLIPSE", juce::Rectangle<float>(0.f, y, w, hh), juce::Justification::centred, false);
        }

        juce::String ver (JucePlugin_VersionString);          // "1.1.0" -> "1.1"
        if (ver.endsWith(".0")) ver = ver.dropLastCharacters(2);
        spaced("VERSION " + ver, juce::Font(juce::FontOptions(12.f * sy)), 0.42f, Colour(0xff8f86a8), 646.f, 20.f);

        // pulsing prompt
        const float pulse = 0.55f + 0.45f * std::sin((float) phase);
        spaced("PRESS ANY KEY OR CLICK TO START", juce::Font(juce::FontOptions(13.f * sy).withStyle("Bold")),
               0.46f, Colour(0xffe9e1ff).withAlpha(0.45f + 0.55f * pulse), 733.f, 22.f);
        {
            juce::Font f (juce::FontOptions(13.f * sy).withStyle("Bold")); f.setExtraKerningFactor(0.46f);
            const float tw = juce::GlyphArrangement::getStringWidth(f, "PRESS ANY KEY OR CLICK TO START");
            const float dy = 744.f * sy, r = 3.5f * sy;
            for (float dx : { c.x - tw * 0.5f - 22.f * sx, c.x + tw * 0.5f + 16.f * sx }) {
                g.setColour(Colour(0xff9b59ff).withAlpha(0.25f * pulse));
                g.fillEllipse(dx - r * 2.2f, dy - r * 2.2f, r * 4.4f, r * 4.4f);
                g.setColour(Colour(0xff9b59ff).withAlpha(0.6f + 0.4f * pulse));
                g.fillEllipse(dx - r, dy - r, r * 2.f, r * 2.f);
            }
        }
        spaced(juce::String::fromUTF8("\xc2\xa9") + " 2026 AMARI LABS", juce::Font(juce::FontOptions(10.f * sy)),
               0.3f, Colour(0xff5d5672), 790.f, 16.f);
    }

    void mouseDown(const juce::MouseEvent&) override { dismiss(); }
    bool keyPressed(const juce::KeyPress&) override  { dismiss(); return true; }
    bool keyPressed(const juce::KeyPress&, juce::Component*) override
    {
        if (! isVisible() || fading) return false;
        dismiss(); return true;
    }

    void dismiss()
    {
        if (fading) return;
        fading = true;
        fadeStartMs = juce::Time::getMillisecondCounterHiRes();
    }

private:
    double phase = 0.0, fadeStartMs = 0.0;
    bool fading = false;
    static constexpr double kFadeMs = 450.0;

    juce::Component::SafePointer<juce::Component> keyHost;

    void attachKeys()
    {
        auto* top = getTopLevelComponent();
        if (top == this || top == keyHost.getComponent()) return;
        detachKeys();
        keyHost = top;
        if (top != nullptr) top->addKeyListener(this);
    }
    void detachKeys()
    {
        if (auto* k = keyHost.getComponent()) k->removeKeyListener(this);
        keyHost = nullptr;
    }

    void timerCallback() override
    {
        phase += 0.12;
        attachKeys();
        if (! fading && isShowing() && ! hasKeyboardFocus(false) && phase < 12.0)   // first ~3 s
            grabKeyboardFocus();
        if (fading) {
            const double t = (juce::Time::getMillisecondCounterHiRes() - fadeStartMs) / kFadeMs;
            if (t >= 1.0) {
                stopTimer();
                detachKeys();
                setVisible(false);
                if (onDismissed) onDismissed();      // owner may delete us here
                return;
            }
            setAlpha((float) (1.0 - t * t));         // ease out
        }
        repaint();
    }

    static juce::Font cinzel(float height)
    {
        static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor(
            BinaryData::CinzelSemiBold_ttf, (size_t) BinaryData::CinzelSemiBold_ttfSize);
        return tf != nullptr ? juce::Font(juce::FontOptions(tf).withHeight(height))
                             : juce::Font(juce::FontOptions(height).withStyle("Bold"));
    }

    static juce::Image& blurredPanel()
    {
        // built once: shrink, gaussian-blur, scale back up (smooth, no blocky pixels)
        static juce::Image img = [] {
            auto bg = juce::ImageCache::getFromMemory(BinaryData::background_png, BinaryData::background_pngSize);
            if (! bg.isValid()) return juce::Image();
            auto small = bg.convertedToFormat(juce::Image::ARGB).rescaled(300, 206, juce::Graphics::highResamplingQuality);
            juce::Image blurred (juce::Image::ARGB, small.getWidth(), small.getHeight(), true);
            juce::ImageConvolutionKernel k (9);
            k.createGaussianBlur(3.2f);
            k.applyToImage(blurred, small, small.getBounds());
            juce::Image big (juce::Image::ARGB, 600, 412, true);
            { juce::Graphics gg (big);
              gg.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
              gg.drawImage(blurred, big.getBounds().toFloat()); }
            juce::Image out (juce::Image::ARGB, 600, 412, true);
            k.applyToImage(out, big, big.getBounds());
            return out;
        }();
        return img;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AESplash)
};
