#include "AEActivationDialog.h"

static const juce::Colour kActBack  {0xe8060410};
static const juce::Colour kActSurf  {0xff15121d};
static const juce::Colour kActField {0xff0c0a12};
static const juce::Colour kActBd    {0xff2e2a3e};
static const juce::Colour kActPurp  {0xff9B59FF};
static const juce::Colour kActBtn2  {0xff2a2338};
static const juce::Colour kActText  {0xfff0f0ff};
static const juce::Colour kActMuted {0xffbcbce2};
static const juce::Colour kActErr   {0xffff6b6b};

static juce::Font actFont (float h, bool bold = false)
{
    juce::Font f { juce::FontOptions (h) };
    return bold ? f.boldened() : f;
}

AEActivationDialog::AEActivationDialog (Mode initial)
{
    setOpaque(false);

    auto setupField=[this](juce::Label& l, const juce::String& txt,
                           juce::TextEditor& e, const juce::String& hint){
        l.setText(txt, juce::dontSendNotification);
        l.setFont(actFont(9.5f, true));
        l.setColour(juce::Label::textColourId, kActMuted);
        l.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        addChildComponent(l);
        e.setFont(actFont(12.f));
        e.setColour(juce::TextEditor::backgroundColourId, kActField);
        e.setColour(juce::TextEditor::textColourId, kActText);
        e.setColour(juce::TextEditor::outlineColourId, kActBd);
        e.setColour(juce::TextEditor::focusedOutlineColourId, kActPurp);
        e.setTextToShowWhenEmpty(hint, kActMuted.withAlpha(0.45f));
        e.setJustification(juce::Justification::centredLeft);
        e.onReturnKey=[this]{ attemptActivation(); };
        addChildComponent(e);
    };
    setupField(nameLbl,   "FULL NAME",     nameEdit,   "Enter the name on your license");
    setupField(emailLbl,  "EMAIL",         emailEdit,  "Enter your registered email");
    setupField(serialLbl, "SERIAL NUMBER", serialEdit, "e.g. AE-00001");
    setupField(codeLbl,   "LICENSE CODE",  codeEdit,   "Paste your license code here");
    codeEdit.setFont(juce::Font(juce::FontOptions().withName(juce::Font::getDefaultMonospacedFontName()).withHeight(11.f)));

    errorLabel.setFont(actFont(11.f));
    errorLabel.setColour(juce::Label::textColourId, kActErr);
    errorLabel.setJustificationType(juce::Justification::centred);
    addChildComponent(errorLabel);

    auto styleBtn=[this](juce::TextButton& b, bool primary){
        b.setColour(juce::TextButton::buttonColourId,  primary ? kActPurp : kActBtn2);
        b.setColour(juce::TextButton::buttonOnColourId, primary ? kActPurp : kActBtn2);
        b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        b.setColour(juce::TextButton::textColourOnId,  juce::Colours::white);
        addChildComponent(b);
    };
    styleBtn(primaryBtn, true);
    styleBtn(secondBtn, false);
    styleBtn(thirdBtn,  false);
    styleBtn(activateBtn, true);
    styleBtn(backBtn, false);

    activateBtn.onClick=[this]{ attemptActivation(); };
    backBtn.onClick=[this]{ setMode(returnMode); };

    setMode(initial);
}

void AEActivationDialog::setMode (Mode m)
{
    if (m == Mode::Activate && mode != Mode::Activate) returnMode = mode;
    mode = m;
    const bool form = (m == Mode::Activate);

    for (juce::Component* c : { (juce::Component*) &nameLbl, (juce::Component*) &emailLbl,
                                (juce::Component*) &serialLbl, (juce::Component*) &codeLbl,
                                (juce::Component*) &nameEdit, (juce::Component*) &emailEdit,
                                (juce::Component*) &serialEdit, (juce::Component*) &codeEdit,
                                (juce::Component*) &errorLabel, (juce::Component*) &activateBtn })
        c->setVisible(form);
    // BACK only when there is somewhere to go back to (a pure activation gate has none)
    backBtn.setVisible(form && returnMode != Mode::Activate);

    primaryBtn.setVisible(!form);
    secondBtn.setVisible(!form);
    thirdBtn.setVisible(!form && m != Mode::Expired);

    switch (m)
    {
        case Mode::Welcome:
            primaryBtn.setButtonText("START 7-DAY FREE TRIAL");
            primaryBtn.onClick=[this]{ if (onStartTrial) onStartTrial(); };
            secondBtn.setButtonText("I HAVE A LICENSE");
            secondBtn.onClick=[this]{ goToActivate(); };
            thirdBtn.setButtonText("BUY LICENSE");
            thirdBtn.onClick=[this]{ openBuyPage(); };
            break;
        case Mode::TrialActive:
            primaryBtn.setButtonText("CONTINUE TRIAL");
            primaryBtn.onClick=[this]{ if (onContinue) onContinue(); };
            secondBtn.setButtonText("ACTIVATE LICENSE");
            secondBtn.onClick=[this]{ goToActivate(); };
            thirdBtn.setButtonText("BUY LICENSE");
            thirdBtn.onClick=[this]{ openBuyPage(); };
            break;
        case Mode::Expired:
            primaryBtn.setButtonText("BUY LICENSE");
            primaryBtn.onClick=[this]{ openBuyPage(); };
            secondBtn.setButtonText("ACTIVATE LICENSE");
            secondBtn.onClick=[this]{ goToActivate(); };
            break;
        case Mode::Activate:
            errorLabel.setText("", juce::dontSendNotification);
            break;
    }
    resized();
    repaint();
}

juce::Rectangle<int> AEActivationDialog::card() const
{
    return getLocalBounds().withSizeKeepingCentre(560, 480);
}

void AEActivationDialog::resized()
{
    auto c = card();
    const int cx = c.getCentreX();

    if (mode == Mode::Activate)
    {
        int fx = c.getX() + 60, fw = c.getWidth() - 120, fh = 30, y = c.getY() + 112;
        auto field=[&](juce::Label& l, juce::TextEditor& e){
            l.setBounds(fx, y, fw, 14); e.setBounds(fx, y + 16, fw, fh); y += fh + 16 + 10;
        };
        field(nameLbl,  nameEdit);
        field(emailLbl, emailEdit);
        field(serialLbl,serialEdit);
        field(codeLbl,  codeEdit);
        errorLabel.setBounds(fx, y, fw, 30); y += 34;
        if (backBtn.isVisible()) {
            backBtn    .setBounds(cx - 160, y, 140, 38);
            activateBtn.setBounds(cx + 20,  y, 140, 38);
        } else {
            activateBtn.setBounds(cx - 70,  y, 140, 38);
        }
        return;
    }

    primaryBtn.setBounds(cx - 150, c.getY() + 318, 300, 46);
    if (mode == Mode::Expired)
        secondBtn.setBounds(cx - 100, c.getY() + 376, 200, 36);
    else {
        secondBtn.setBounds(cx - 182, c.getY() + 376, 174, 36);
        thirdBtn .setBounds(cx + 8,   c.getY() + 376, 174, 36);
    }
}

void AEActivationDialog::paint(juce::Graphics& g)
{
    g.fillAll(kActBack);

    auto c  = card().toFloat();
    // soft purple halo behind the card
    for (int i = 6; i >= 1; --i) {
        g.setColour(kActPurp.withAlpha(0.035f));
        g.fillRoundedRectangle(c.expanded((float) i * 7.f), 16.f + (float) i * 4.f);
    }
    g.setColour(kActSurf);
    g.fillRoundedRectangle(c, 14.f);
    g.setColour(kActPurp.withAlpha(0.85f));
    g.drawRoundedRectangle(c.reduced(0.5f), 14.f, 1.6f);

    const int x = (int) c.getX(), y = (int) c.getY(), w = (int) c.getWidth();

    // brand + section title
    g.setColour(kActText);
    g.setFont(actFont(23.f, true));
    g.drawText("ARCANE  ECLIPSE", x, y + 28, w, 30, juce::Justification::centred);
    juce::String section = mode == Mode::Activate ? "LICENSE ACTIVATION"
                         : mode == Mode::Expired  ? "FREE TRIAL ENDED" : "7-DAY FREE TRIAL";
    g.setColour(kActPurp);
    g.setFont(actFont(12.f, true));
    g.drawText(section, x, y + 60, w, 18, juce::Justification::centred);
    g.setColour(kActPurp.withAlpha(0.5f));
    g.fillRect(x + 40, y + 90, w - 80, 1);

    if (mode != Mode::Activate)
    {
        auto& lm = AELicenseManager::getInstance();
        const int left = lm.trialDaysLeft();

        juce::String head, body;
        if (mode == Mode::Welcome) {
            head = "Try it free for 7 days";
            body = "Every amp, pedal and feature is fully unlocked during the trial. "
                   "Nothing to pay until you decide to keep it.";
        } else if (mode == Mode::TrialActive) {
            head = left <= 1 ? juce::String("Last day of your trial")
                             : juce::String(left) + " days left in your trial";
            body = "Enjoying Arcane Eclipse? Buy a license to keep playing after the trial. "
                   "Your presets and settings stay exactly as they are.";
        } else {
            head = "Your free trial has ended";
            body = "Thanks for trying Arcane Eclipse. Buy a license to keep playing. "
                   "Your presets and settings are all still here.";
        }

        g.setColour(kActText);
        g.setFont(actFont(27.f, true));
        g.drawText(head, x, y + 116, w, 36, juce::Justification::centred);
        g.setColour(kActMuted);
        g.setFont(actFont(13.5f));
        g.drawFittedText(body, x + 60, y + 160, w - 120, 58, juce::Justification::centredTop, 3, 1.0f);

        // days-used meter (trial reminder + expired)
        if (mode != Mode::Welcome)
        {
            const int used = mode == Mode::Expired ? kAETrialDays
                                                   : juce::jlimit(1, kAETrialDays, kAETrialDays - left + 1);
            const int segW = 40, gap = 6, total = kAETrialDays * segW + (kAETrialDays - 1) * gap;
            int sx = x + (w - total) / 2;
            for (int i = 0; i < kAETrialDays; ++i, sx += segW + gap) {
                const bool on = i < used;
                g.setColour(on ? (mode == Mode::Expired ? juce::Colour(0xffff6b8a) : kActPurp)
                               : juce::Colour(0xff2a2636));
                g.fillRoundedRectangle((float) sx, (float) (y + 238), (float) segW, 8.f, 3.f);
            }
            g.setColour(kActMuted.withAlpha(0.8f));
            g.setFont(actFont(11.f));
            g.drawText(mode == Mode::Expired ? juce::String("7 of 7 days used")
                                             : "Day " + juce::String(used) + " of " + juce::String(kAETrialDays),
                       x, y + 254, w, 16, juce::Justification::centred);
        }
        else
        {
            // three short promises instead of the meter
            g.setColour(kActText.withAlpha(0.9f));
            g.setFont(actFont(12.f, true));
            const char* items[] = { "FULL FEATURES", "NO PAYMENT NEEDED", "KEEP YOUR PRESETS" };
            const int colW = (w - 80) / 3;
            for (int i = 0; i < 3; ++i) {
                const int ix = x + 40 + i * colW;
                g.setColour(kActPurp);
                g.fillEllipse((float) (ix + colW / 2 - 4), (float) (y + 236), 8.f, 8.f);
                g.setColour(kActText.withAlpha(0.9f));
                g.drawText(items[i], ix, y + 252, colW, 16, juce::Justification::centred);
            }
        }
    }

    // footer
    g.setColour(kActMuted.withAlpha(0.6f));
    g.setFont(actFont(10.f));
    g.drawText("A license activates on up to 2 computers.",
               x, y + (int) c.getHeight() - 46, w, 14, juce::Justification::centred);
    g.drawText("Questions? amarilabs.audio@gmail.com",
               x, y + (int) c.getHeight() - 30, w, 14, juce::Justification::centred);
}

void AEActivationDialog::goToActivate()
{
    setMode(Mode::Activate);
    nameEdit.grabKeyboardFocus();
}

void AEActivationDialog::openBuyPage()
{
    juce::URL(kAEBuyURL).launchInDefaultBrowser();
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
    }
}
