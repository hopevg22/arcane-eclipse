#include "AEActivationDialog.h"

static const juce::Colour kActBg    {0xff0f0f16};
static const juce::Colour kActSurf  {0xff1a1a24};
static const juce::Colour kActBd    {0xff2e2e3e};
static const juce::Colour kActPurp  {0xff9B59FF};
static const juce::Colour kActText  {0xfff0f0ff};
static const juce::Colour kActMuted {0xffbcbce2};
static const juce::Colour kActErr   {0xffff6b6b};

AEActivationDialog::AEActivationDialog()
{
    setSize(540, 520);
    setOpaque(true);

    auto setupLabel=[this](juce::Label& l, const juce::String& txt, float sz,
                           juce::Colour col, bool bold=false){
        l.setText(txt, juce::dontSendNotification);
        l.setFont(bold ? juce::Font(sz).boldened() : juce::Font(sz));
        l.setColour(juce::Label::textColourId, col);
        l.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(l);
    };
    auto setupField=[this](juce::Label& l, const juce::String& txt,
                           juce::TextEditor& e, const juce::String& hint){
        l.setText(txt, juce::dontSendNotification);
        l.setFont(juce::Font(9.f).boldened());
        l.setColour(juce::Label::textColourId, kActMuted);
        l.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(l);
        e.setFont(juce::Font(11.f));
        e.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0c0c14));
        e.setColour(juce::TextEditor::textColourId, kActText);
        e.setColour(juce::TextEditor::outlineColourId, kActBd);
        e.setColour(juce::TextEditor::focusedOutlineColourId, kActPurp);
        e.setTextToShowWhenEmpty(hint, kActBd);
        e.setJustification(juce::Justification::centredLeft);
        e.onReturnKey=[this]{ attemptActivation(); };
        addAndMakeVisible(e);
    };

    setupLabel(titleLabel,    "ARCANE ECLIPSE",           22, kActText, true);
    setupLabel(subtitleLabel, "License Activation",        12, kActPurp);
    setupLabel(errorLabel,    "",                          10, kActErr);

    setupField(nameLbl,   "FULL NAME",     nameEdit,   "Enter the name on your license");
    setupField(emailLbl,  "EMAIL",         emailEdit,  "Enter your registered email");
    setupField(serialLbl, "SERIAL NUMBER", serialEdit, "e.g. AE-00001");
    setupField(codeLbl,   "LICENSE CODE",  codeEdit,   "Paste your license code here");
    codeEdit.setFont(juce::Font(juce::FontOptions().withName(juce::Font::getDefaultMonospacedFontName()).withHeight(10.f)));

    activateBtn.setColour(juce::TextButton::buttonColourId,  kActPurp);
    activateBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    activateBtn.onClick=[this]{ attemptActivation(); };
    addAndMakeVisible(activateBtn);

    exitBtn.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xff252532));
    exitBtn.setColour(juce::TextButton::textColourOffId, kActMuted);
    exitBtn.onClick=[]{
        juce::JUCEApplicationBase::quit();
    };
    addAndMakeVisible(exitBtn);
}

void AEActivationDialog::resized()
{
    int W=getWidth(), cx=W/2;
    titleLabel   .setBounds(0,  30, W, 28);
    subtitleLabel.setBounds(0,  60, W, 18);

    int fx=60, fw=W-120, fh=28, gap=10;
    int y=108;
    auto field=[&](juce::Label& l, juce::TextEditor& e){
        l.setBounds(fx, y, fw, 14); e.setBounds(fx, y+16, fw, fh); y+=fh+16+gap;
    };
    field(nameLbl,  nameEdit);
    field(emailLbl, emailEdit);
    field(serialLbl,serialEdit);
    field(codeLbl,  codeEdit);

    errorLabel.setBounds(fx, y, fw, 16); y+=22;
    activateBtn.setBounds(cx-90, y, 110, 34);
    exitBtn    .setBounds(cx+6,  y, 84,  34);
}

void AEActivationDialog::paint(juce::Graphics& g)
{
    g.fillAll(kActBg);
    // Card
    g.setColour(kActSurf);
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(24),12.f);
    g.setColour(kActPurp);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(24.5f),12.f,1.6f);
    // Top accent line
    g.setColour(kActPurp.withAlpha(0.6f));
    g.fillRect(48, 92, getWidth()-96, 1);
    // Bottom help text
    g.setFont(juce::Font(8.5f)); g.setColour(kActMuted.withAlpha(0.6f));
    g.drawText("Your license allows activation on up to 2 machines.",
               0, getHeight()-28, getWidth(), 14, juce::Justification::centred);
    g.drawText("Contact support if you need to transfer your license.",
               0, getHeight()-16, getWidth(), 14, juce::Justification::centred);
}

void AEActivationDialog::attemptActivation()
{
    errorLabel.setText("", juce::dontSendNotification);
    juce::String errMsg;
    bool ok = AELicenseManager::getInstance().activate(
        nameEdit.getText(), emailEdit.getText(),
        serialEdit.getText(), codeEdit.getText(), errMsg);
    if (ok) {
        if (onActivated) onActivated();
    } else {
        errorLabel.setText(errMsg, juce::dontSendNotification);
        errorLabel.setColour(juce::Label::textColourId, kActErr);
    }
}
