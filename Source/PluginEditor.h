#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "AEActivationDialog.h"

// ── Palette ──────────────────────────────────────────────────────────────────
namespace AEP {
    static const juce::Colour Bg      { 0xff141418 };
    static const juce::Colour Surface { 0xff1c1c22 };
    static const juce::Colour Card    { 0xff1e1e26 };
    static const juce::Colour CardBd  { 0xff2e2e3e };
    static const juce::Colour Purple  { 0xff9B59FF };
    static const juce::Colour PurDim  { 0xff7040cc };
    static const juce::Colour Text    { 0xfff0f0ff };
    static const juce::Colour Muted   { 0xff7878aa };
    static const juce::Colour Green   { 0xff44cc88 };
    static const juce::Colour Red     { 0xffcc4444 };
}

// ── LookAndFeel ───────────────────────────────────────────────────────────────
class AELAF : public juce::LookAndFeel_V4
{
public:
    AELAF();
    void drawRotarySlider(juce::Graphics&,int,int,int,int,
                          float,float,float,juce::Slider&) override;
    void drawLabel(juce::Graphics&,juce::Label&) override;
    void drawButtonBackground(juce::Graphics&,juce::Button&,
                              const juce::Colour&,bool,bool) override;
    void drawToggleButton(juce::Graphics&,juce::ToggleButton&,bool,bool) override {}
    void drawComboBox(juce::Graphics&,int,int,bool,int,int,int,int,juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&,juce::Label&) override;
    void drawPopupMenuItem(juce::Graphics&,const juce::Rectangle<int>&,
                           bool,bool,bool,bool,bool,const juce::String&,
                           const juce::String&,const juce::Drawable*,
                           const juce::Colour*) override;
};

// ── Knob widget ───────────────────────────────────────────────────────────────
struct AEKnob {
    juce::Slider slider{juce::Slider::RotaryHorizontalVerticalDrag,juce::Slider::NoTextBox};
    juce::Label  nameLabel, valLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    juce::String paramID;
    void setup(juce::Component*,juce::AudioProcessorValueTreeState&,
               const juce::String& id,const juce::String& name,AELAF*);
    void place(int cx,int cy,int sz,bool showVal=true);
};

// ── Scene data ────────────────────────────────────────────────────────────────
struct SceneData {
    juce::String namPath, irPath, name{"Empty"};
    juce::ValueTree params;
    bool isEmpty() const { return name == "Empty"; }
};

// ── Tuner panel ───────────────────────────────────────────────────────────────
class TunerPanel : public juce::Component, private juce::Timer
{
public:
    TunerPanel();
    void setInputBuffer(const float* data, int numSamples, double sr);
    void paint(juce::Graphics&) override;
private:
    void timerCallback() override;
    float detectedHz = 0.f;
    juce::String noteName;
    float cents = 0.f;
    std::vector<float> buffer;
    double sampleRate = 44100.0;
    float detectPitch();
};

// ── Main editor ───────────────────────────────────────────────────────────────
// ── CreditsPanel ─────────────────────────────────────────────────────────────
class CreditsPanel : public juce::Component
{
public:
    CreditsPanel();
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override { setVisible(false); }
private:
    juce::TextButton closeBtn{"CLOSE"};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreditsPanel)
};

class ArcaneEclipseEditor : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit ArcaneEclipseEditor(ArcaneEclipseProcessor&);
    ~ArcaneEclipseEditor() override;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintTopBar(juce::Graphics&);
    void paintStrip(juce::Graphics&);
    void paintChain(juce::Graphics&);
    void paintChainNode(juce::Graphics&,int,juce::Rectangle<int>,bool);
    void paintAmpHead(juce::Graphics&);
    void paintFXSection(juce::Graphics&);
    void paintCabSection(juce::Graphics&);
    void paintSceneBar(juce::Graphics&);
    void paintFooter(juce::Graphics&);
    void paintVU(juce::Graphics&,juce::Rectangle<float>,float);
    void paintTuner(juce::Graphics&);
    void setTunerVisible(bool v);
    juce::Rectangle<int> chainNodeBounds(int i) const;

    void saveScene(int slot);
    void loadScene(int slot);
    void refreshSceneButtons();
    void deleteScene(int slot);
    void renameScene(int slot);

    // ── Layout ────────────────────────────────────────────────────────────────
    static constexpr int W=1200,H=823;
    static constexpr int kTopH=54,kStripH=124,kAmpH=285,kFXH=258,kSceneH=58,kFootH=44;
    static constexpr int kCabW=346;

    ArcaneEclipseProcessor& proc;
    AELAF laf;
    float vuIn=0.f,vuOut=0.f;
    bool tunerVisible=false;
    float tunerHz=0.f;
    juce::String tunerNote;
    float tunerCents=0.f;
    int activeScene=-1; int currentBank=0;
    juce::String curNAMPath, curIRPath;
    SceneData scenes[20];

    // Strip knobs
    AEKnob kInput,kGate,kComp,kOutput;
    // Amp knobs
    AEKnob kGain,kBass,kMid,kTreble,kPresence,kMaster;
    // FX knobs
    AEKnob kODDrive,kODTone,kODLevel;
    AEKnob kModRate,kModDepth,kModMix;
    AEKnob kDTime,kDFeedback,kDMix;
    AEKnob kRDecay,kRSize,kRMix;
    std::vector<AEKnob*> allKnobs;
    struct NodeLearn { juce::Component* comp; juce::String pid; int nodeIdx; };
    std::vector<NodeLearn> nodeLearns;
    juce::String learningID;
    std::unique_ptr<juce::AlertWindow> renameWindow;
    std::unique_ptr<AEActivationDialog> activationDialog;
    CreditsPanel creditsPanel;
    std::unique_ptr<juce::TextButton> helpBtn;
    void showCredits();

    // Toggles
    juce::ToggleButton tbGate{""}, tbComp{""};
    juce::ToggleButton stompOD{""}, stompMod{""}, stompDelay{""}, stompReverb{""};
    juce::ToggleButton tbCab{""}, tbTuner{""};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        attGate,attComp,attOD,attMod,attDelay,attReverb,attCab;

    // Dropdowns
    juce::ComboBox comboMod,comboDly,comboRvb;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        attModType,attDlyType,attRvbType;

    // Load buttons
    juce::TextButton btnLoadModel{"LOAD MODEL"},btnLoadIR{"LOAD IR"};
    juce::TextButton btnClearModel{"×"},btnClearIR{"×"};

    // Scene bar (4 slots + 2 bank buttons)
    juce::TextButton sceneBtn[4];
    juce::TextButton bankPrev, bankNext;
    // Header preset nav + save
    juce::ToggleButton presetPrev, presetNext;
    juce::TextButton headerSave{"SAVE"};

    // Tuner toggle button
    juce::TextButton btnTuner{"TUNER"};

    std::unique_ptr<juce::FileChooser> chooserModel,chooserIR;
    static const juce::String kChainLabels[9];
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArcaneEclipseEditor)
};
