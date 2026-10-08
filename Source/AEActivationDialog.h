#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "AELicense.h"

/*  License gate shown over the editor.
      Welcome     — first run: start the 7-day trial, activate, or buy
      TrialActive — reminder with days left: continue, activate, or buy
      Expired     — trial over (audio muted): buy or activate
      Activate    — the serial / code form                                    */
class AEActivationDialog : public juce::Component
{
public:
    enum class Mode { Welcome, TrialActive, Expired, Activate };

    explicit AEActivationDialog (Mode initial = Mode::Activate);
    void paint(juce::Graphics&) override;
    void resized() override;

    void setMode (Mode m);
    Mode getMode() const { return mode; }

    std::function<void()> onActivated;    // license accepted
    std::function<void()> onStartTrial;   // "start 7-day free trial"
    std::function<void()> onContinue;     // "continue trial"

private:
    Mode mode = Mode::Activate, returnMode = Mode::Activate;

    juce::Label  errorLabel;
    juce::Label  nameLbl, emailLbl, serialLbl, codeLbl;
    juce::TextEditor nameEdit, emailEdit, serialEdit, codeEdit;
    juce::TextButton activateBtn{"ACTIVATE"}, backBtn{"BACK"};
    juce::TextButton primaryBtn, secondBtn, thirdBtn;

    juce::Rectangle<int> card() const;
    void attemptActivation();
    void openBuyPage();
    void goToActivate();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AEActivationDialog)
};
