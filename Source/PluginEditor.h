#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <BinaryData.h>
#include "PluginProcessor.h"

// ── Palette ──────────────────────────────────────────────────────────────────
namespace AE {
    static const juce::Colour Purple { 0xff9B7FD4 };
    static const juce::Colour PurpleDim { 0xff7B5EA7 };
    static const juce::Colour Dark   { 0xff0e0e14 };
    static const juce::Colour Card   { 0xff16161e };
    static const juce::Colour CardBd { 0xff2a2a3a };
    static const juce::Colour Text   { 0xffddddee };
    static const juce::Colour Muted  { 0xff888899 };
}

// ── Transparent knob LAF — purple pointer only ────────────────────────────────
class ArcaneKnobLAF : public juce::LookAndFeel_V4
{
public:
    ArcaneKnobLAF();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float pos, float startA, float endA, juce::Slider&) override;
    void drawLabel(juce::Graphics&, juce::Label&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool, int, int, int, int, juce::ComboBox&) override;
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>&, bool, bool, bool, bool, bool,
                           const juce::String&, const juce::String&, const juce::Drawable*, const juce::Colour*) override;
};

// ── Knob widget ───────────────────────────────────────────────────────────────
struct AKnob {
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Label  valLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    void setup(juce::Component*, juce::AudioProcessorValueTreeState&, const juce::String& paramId, ArcaneKnobLAF*);
    void place(int cx, int cy, int sz);
};

// ── Main editor ───────────────────────────────────────────────────────────────
class ArcaneEclipseEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ArcaneEclipseEditor(ArcaneEclipseProcessor&);
    ~ArcaneEclipseEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void paintChain(juce::Graphics&);
    void paintChainNode(juce::Graphics&, int idx, juce::Rectangle<int>, bool active);
    void paintVU(juce::Graphics&, float bx, float by, float bw, float bh, float lvl);
    void paintFXOverlays(juce::Graphics&);
    void paintStomp(juce::Graphics&, float cx, float cy, bool on);
    void paintCabOverlay(juce::Graphics&);
    juce::Rectangle<int> chainNodeRect(int i) const;

    ArcaneEclipseProcessor& proc;
    ArcaneKnobLAF laf;
    juce::Image bgImage;
    int activeNode = 3;
    float vuIn = 0.f, vuOut = 0.f;

    // Image is 1577x997, plugin 980x620 — pre-scale all coords
    static constexpr float kImgW = 1577.f, kImgH = 997.f;
    static constexpr int kPlugW = 980, kPlugH = 620;
    static int sx(float x) { return (int)(x * kPlugW / kImgW); }
    static int sy(float y) { return (int)(y * kPlugH / kImgH); }
    static int sw(float w) { return (int)(w * kPlugW / kImgW); }

    // Strip knobs
    AKnob kInput, kGate, kComp, kOutput;
    // Amp knobs
    AKnob kGain, kBass, kMid, kTreble, kPresence, kMaster;
    // Overdrive
    AKnob kODDrive, kODTone, kODLevel;
    // Modulation
    AKnob kModRate, kModDepth, kModMix;
    // Delay
    AKnob kDTime, kDFeedback, kDMix;
    // Reverb
    AKnob kRDecay, kRSize, kRMix;

    // Toggles
    juce::ToggleButton tbOD{""}, tbMod{""}, tbDelay{""}, tbReverb{""}, tbCab{""};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        attOD, attMod, attDelay, attReverb, attCab;

    // Dropdowns
    juce::ComboBox comboModType, comboDelayType, comboReverbType, comboCab;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        attModType, attDelayType, attReverbType;

    // NAM loader on OD card
    juce::TextButton btnBrowseNAM { "BROWSE" };
    juce::Label      labelNAMLoaded;

    // Load buttons
    juce::TextButton btnLoadModel { "LOAD MODEL" }, btnLoadIR { "LOAD IR" };

    // Distance slider
    juce::Slider sliderDist { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };

    // Cab tabs
    juce::TextButton tabCabinet { "CABINET" }, tabIRLoader { "IR LOADER" };
    int activeCabTab = 0;

    std::unique_ptr<juce::FileChooser> chooserModel, chooserIR;

    static const juce::String kChainLabels[8];
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArcaneEclipseEditor)
};
