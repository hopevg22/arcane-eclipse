#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// ── Colours ───────────────────────────────────────────────────────────────────
namespace AEC {
    static const juce::Colour Bg      { 0xff0d0d12 };
    static const juce::Colour Surface { 0xff13131a };
    static const juce::Colour Card    { 0xff1a1a24 };
    static const juce::Colour CardBd  { 0xff2a2a3e };
    static const juce::Colour Purple  { 0xff8B5CF6 };
    static const juce::Colour PurDim  { 0xff6D45D4 };
    static const juce::Colour PurGlow { 0x338B5CF6 };
    static const juce::Colour Text    { 0xfff0f0ff };
    static const juce::Colour Muted   { 0xff7878aa };
    static const juce::Colour Strip   { 0xff0f0f18 };
}

// ── LookAndFeel ───────────────────────────────────────────────────────────────
class ArcLAF : public juce::LookAndFeel_V4
{
public:
    ArcLAF();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float pos, float startA, float endA, juce::Slider&) override;
    void drawLabel(juce::Graphics&, juce::Label&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>&,
                           bool, bool, bool, bool, bool,
                           const juce::String&, const juce::String&,
                           const juce::Drawable*, const juce::Colour*) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override { return juce::Font(11.f); }
};

// ── Knob widget ───────────────────────────────────────────────────────────────
struct AKnob {
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Label  nameLabel, valLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;

    void setup(juce::Component*, juce::AudioProcessorValueTreeState&,
               const juce::String& paramId, const juce::String& name, ArcLAF*);
    void place(int cx, int cy, int sz); // sz = diameter
};

// ── Stomp button ──────────────────────────────────────────────────────────────
class StompButton : public juce::ToggleButton
{
public:
    StompButton() : juce::ToggleButton("") {}
    void paint(juce::Graphics& g) override;
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

    // ── paint sections ────────────────────────────────────────────────────────
    void paintTopBar(juce::Graphics&);
    void paintStrip(juce::Graphics&);
    void paintChain(juce::Graphics&);
    void paintAmpHead(juce::Graphics&);
    void paintFXSection(juce::Graphics&);
    void paintCabSection(juce::Graphics&);
    void paintFooter(juce::Graphics&);
    void paintVU(juce::Graphics&, juce::Rectangle<float>, float lvl);
    void paintChainNode(juce::Graphics&, int idx, juce::Rectangle<int>, bool active);

    juce::Rectangle<int> chainNodeBounds(int i) const;

    // ── layout constants ──────────────────────────────────────────────────────
    static constexpr int W = 1100, H = 680;
    static constexpr int kTopH   = 50;
    static constexpr int kStripH = 120;
    static constexpr int kAmpH   = 210;
    static constexpr int kFXH    = 230;
    static constexpr int kFootH  = 36;
    // kCabH fills remaining space

    ArcaneEclipseProcessor& proc;
    ArcLAF laf;
    int activeNode = 3;
    float vuIn = 0.f, vuOut = 0.f;

    // Strip knobs
    AKnob kInput, kGate, kComp, kOutput;

    // Amp knobs
    AKnob kGain, kBass, kMid, kTreble, kPresence, kMaster;

    // OD knobs
    AKnob kODDrive, kODTone, kODLevel;
    // Mod knobs
    AKnob kModRate, kModDepth, kModMix;
    // Delay knobs
    AKnob kDTime, kDFeedback, kDMix;
    // Reverb knobs
    AKnob kRDecay, kRSize, kRMix;

    // Stomps
    StompButton stompOD, stompMod, stompDelay, stompReverb;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        attOD, attMod, attDelay, attReverb, attCab;
    juce::ToggleButton tbCab{""};

    // Gate and Comp toggles in strip
    juce::ToggleButton tbGate{""}, tbComp{""};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        attGate, attComp2;

    // X clear buttons for model and IR
    juce::TextButton btnClearModel{"x"}, btnClearIR{"x"};

    // Dropdowns
    juce::ComboBox comboModType, comboDelayType, comboReverbType, comboCab;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        attModType, attDelayType, attReverbType;

    // Buttons
    juce::TextButton btnLoadModel{"LOAD MODEL"}, btnLoadIR{"LOAD IR"}, btnBrowse{"BROWSE"};
    juce::Slider sliderDist { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };

    // Cab tabs
    juce::TextButton tabCab{"CABINET"}, tabIR{"IR LOADER"};
    int activeCabTab = 0;

    std::unique_ptr<juce::FileChooser> chooserModel, chooserIR;

    static const juce::String kChainLabels[9];
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArcaneEclipseEditor)
};
