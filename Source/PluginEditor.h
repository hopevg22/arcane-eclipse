#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "AEActivationDialog.h"
#include "AESplash.h"

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
    void drawButtonText(juce::Graphics&,juce::TextButton&,bool,bool) override;
    void drawToggleButton(juce::Graphics&,juce::ToggleButton&,bool,bool) override {}
    void drawLinearSlider(juce::Graphics&,int,int,int,int,float,float,float,
                          juce::Slider::SliderStyle,juce::Slider&) override;
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
    juce::String namPath2, irPath2;          // v1.1: amp 2 (Dual Amp/IR)
    juce::String odPath;                     // v1.1.1: overdrive pedal capture
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
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void resized() override;

private:
    void timerCallback() override;
    // v1.1 re-skin: the render (background.png) carries all static art; these
    // paint only the LIVE state on top of it (text, glows, meters, LEDs).
    void paintHeaderLive(juce::Graphics&);
    void paintChainLive(juce::Graphics&);
    void paintPedalsLive(juce::Graphics&);
    void paintCabLive(juce::Graphics&);
    void paintScenesLive(juce::Graphics&);
    void paintFooterLive(juce::Graphics&);
    void paintVU(juce::Graphics&,juce::Rectangle<float>,float);
    void paintTuner(juce::Graphics&);
    void setTunerVisible(bool v);
    juce::Rectangle<int> chainNodeBounds(int i) const;
    void showTypeMenu(int fx);                 // 0 = MOD, 1 = DELAY, 2 = REVERB
    juce::String typeName(int fx) const;
    void showFieldMenu(bool ir);               // MODEL / IR field dropdown
    void showODMenu();                         // overdrive capture pill
    void showStereoMenu();                     // footer: mono / stereo / doubler
    // A/B compare (v1.1.1)
    SceneData snapshotNow();
    void applySnapshot(const SceneData& s);
    void abToggle(); void showABMenu(); void abReset();
    SceneData abSnap[2]; bool abHas[2] = { false, false }; int abSide = 0;
    void matchAmpLevels();
    double matchFlashMs = 0.0; juce::String matchMsg;

    void saveScene(int slot);
    void loadScene(int slot);
    void refreshSceneButtons();
    void savePresets(); void loadPresets(); juce::File getPresetsFile();
    // v1.1: model loading handles .nam and .aecap; per-slot export/import
    void loadModelFile(const juce::File& f);          // LOAD MODEL -> the amp being edited
    int  editSlot() const;                            // amp the MODEL/IR controls act on (0/1)
    void updateDualUI();                              // show/hide the 1-2 + MIX row
    void exportScene(int slot);                        // write one slot to a .aetone file
    void importScene(int slot);                        // read one slot from a .aetone file
    void resetToDefault();
    void deleteScene(int slot);
    void renameScene(int slot);

    // ── Layout ────────────────────────────────────────────────────────────────
    static constexpr int W=1200,H=823;
    static constexpr int kTopH=54,kFootH=44;   // tuner screen header/footer bands

    ArcaneEclipseProcessor& proc;
    AELAF laf;
    float vuIn=0.f,vuOut=0.f;
    bool tunerVisible=false;
    float tunerHz=0.f;
    juce::String tunerNote;
    float tunerCents=0.f;
    int activeScene=-1; int currentBank=0;
    int editAmp = 0;                                   // v1.1: 0 = amp 1, 1 = amp 2
    // v1.1: 8 banks x 4 slots = 32 scenes (was 5 banks / 20)
    static constexpr int kSlotsPerBank = 4;
    static constexpr int kNumBanks     = 8;
    static constexpr int kNumScenes    = kNumBanks * kSlotsPerBank;
    SceneData scenes[kNumScenes];

    // Strip knobs
    AEKnob kInput,kGate,kComp,kOutput;
    // Amp knobs
    AEKnob kGain,kBass,kMid,kTreble,kPresence,kMaster;
    // FX knobs
    AEKnob kODDrive,kODTone,kODLevel;
    AEKnob kModRate,kModDepth,kModMix;
    AEKnob kDTime,kDFeedback,kDMix;
    AEKnob kRDecay,kRSize,kRMix,kRHiCut;
    std::vector<AEKnob*> allKnobs;
    struct NodeLearn { juce::Component* comp; juce::String pid; int nodeIdx; };
    std::vector<NodeLearn> nodeLearns;
    struct ActLearn { juce::Component* comp; int action; };  // patch/bank nav MIDI
    std::vector<ActLearn> actLearns;
    int learningAction = -1;
    void stepPreset(int d); void stepBank(int d); void cancelMidiLearn();
    juce::String learningID;
    std::unique_ptr<juce::AlertWindow> renameWindow;
    std::unique_ptr<AEActivationDialog> activationDialog;
    // v1.1 free trial: license gate + header "TRIAL · N DAYS LEFT" pill
    juce::TextButton trialBtn;
    AEAccess licAccess = AEAccess::Licensed;
    int licCheckTick = 0;
    void showLicenseGate(AEActivationDialog::Mode m);
    void maybeShowLicenseGate();
    std::unique_ptr<AESplash> splash;                 // v1.1 startup screen
    void showSplash();
    void closeLicenseGate();
    void updateLicenseState();
    CreditsPanel creditsPanel;
    std::unique_ptr<juce::TextButton> helpBtn;
    void showCredits();

    // Toggles
    juce::ToggleButton tbGate{""}, tbComp{""};
    juce::ToggleButton stompOD{""}, stompMod{""}, stompDelay{""}, stompReverb{""};
    juce::ToggleButton tbCab{""}, tbTuner{""};
    juce::ToggleButton stereoBtn{""};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attStereo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        attGate,attComp,attOD,attMod,attDelay,attReverb,attCab;
    // v1.1: pedal footswitches (second attachment to the same on/off params)
    juce::ToggleButton fsOD{""}, fsMod{""}, fsDelay{""}, fsReverb{""};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        attFsOD,attFsMod,attFsDelay,attFsReverb;

    // Load buttons + v1.1 invisible hotspots over the baked art
    juce::TextButton btnLoadModel{"LOAD MODEL"},btnLoadIR{"LOAD IR"};
    juce::TextButton fieldModel, fieldIR, dualBtn, typeBtn[3];
    // v1.1 Dual Amp/IR: [1] --MIX-- [2] row under the DUAL AMP/IR button
    juce::TextButton ampSelBtn[2];
    juce::TextButton odBtn;                          // overdrive pedal capture pill
    juce::TextButton powerBtn, abBtn, matchBtn;      // header power (bypass), [ ] (A/B), dual MATCH
    juce::Slider trimSl[2] { juce::Slider(juce::Slider::LinearVertical, juce::Slider::NoTextBox),
                             juce::Slider(juce::Slider::LinearVertical, juce::Slider::NoTextBox) };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attTrim[2];
    // tuner animation state (v1.1.1 redesign)
    float tunerDispCents = 0.f, tunerStrobe = 0.f, tunerSignal = 0.f;
    double tunerLastTick = 0.0;
    juce::Slider dualMix{juce::Slider::LinearHorizontal, juce::Slider::NoTextBox};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attDual;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attDualMix;
    std::vector<juce::Component*> hiddenByTuner;
    double fsDelayDownMs = 0.0;                      // v1.1.1 tap/hold timing on the DELAY footswitch
    bool   fsDelayTapped = false;
    bool   delayTapMode() const;

    // Scene bar (4 slots + 2 bank buttons)
    juce::TextButton sceneBtn[4];
    juce::TextButton bankPrev, bankNext;
    // Header preset nav + save
    juce::ToggleButton presetPrev, presetNext;
    juce::TextButton headerSave{"SAVE"};

    std::unique_ptr<juce::FileChooser> chooserModel,chooserIR,chooserExport,chooserImport,chooserOD;
    juce::TooltipWindow tooltipWin{this, 600};
    static const juce::String kChainLabels[9];
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArcaneEclipseEditor)
};
