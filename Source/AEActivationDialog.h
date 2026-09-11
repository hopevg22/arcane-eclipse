#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "AELicense.h"

class AEActivationDialog : public juce::Component
{
public:
    AEActivationDialog();
    void paint(juce::Graphics&) override;
    void resized() override;

    std::function<void()> onActivated;   // called after successful activation

private:
    juce::Label  titleLabel, subtitleLabel, errorLabel;
    juce::Label  nameLbl, emailLbl, serialLbl, codeLbl;
    juce::TextEditor nameEdit, emailEdit, serialEdit, codeEdit;
    juce::TextButton activateBtn{"ACTIVATE"};

    void attemptActivation();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AEActivationDialog)
};
