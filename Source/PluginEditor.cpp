#include "PluginEditor.h"

static const juce::Colour kPurple  { 0xff9B7FD4 };
static const juce::Colour kPurDim  { 0xff7B5EA7 };
static const juce::Colour kDark    { 0xff0e0e14 };
static const juce::Colour kCard    { 0xff16161e };
static const juce::Colour kCardBd  { 0xff2a2a3a };
static const juce::Colour kText    { 0xffddddee };
static const juce::Colour kMuted   { 0xff888899 };

const juce::String ArcaneEclipseEditor::kChainLabels[8] =
    { "GATE","COMP","DRIVE","AMP","CAB","EQ","DELAY","REVERB" };

// ── ArcaneKnobLAF ─────────────────────────────────────────────────────────────
ArcaneKnobLAF::ArcaneKnobLAF()
{
    setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff1e1e2a));
    setColour(juce::TextButton::textColourOffId,  kText);
    setColour(juce::TextButton::buttonOnColourId, kPurple);
    setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1a1a26));
    setColour(juce::ComboBox::textColourId,       kText);
    setColour(juce::ComboBox::outlineColourId,    kCardBd);
    setColour(juce::ComboBox::arrowColourId,      kPurple);
    setColour(juce::PopupMenu::backgroundColourId,    juce::Colour(0xff1a1a26));
    setColour(juce::PopupMenu::textColourId,          kText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, kPurDim);
    setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
    setColour(juce::Slider::thumbColourId,        kPurple);
    setColour(juce::Slider::trackColourId,        kPurple);
    setColour(juce::Slider::backgroundColourId,   kCardBd);
}

void ArcaneKnobLAF::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                      float pos, float startA, float endA, juce::Slider&)
{
    auto b = juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h).reduced(4.f);
    auto c = b.getCentre();
    float r = juce::jmin(b.getWidth(),b.getHeight()) * .5f;
    float ang = startA + pos*(endA-startA);
    float px = c.x + r*.72f*std::sin(ang);
    float py = c.y - r*.72f*std::cos(ang);
    // Purple pointer line
    g.setColour(kPurple);
    g.drawLine(c.x, c.y, px, py, 2.5f);
    // Tip dot
    g.fillEllipse(px-3.f, py-3.f, 6.f, 6.f);
}

void ArcaneKnobLAF::drawLabel(juce::Graphics& g, juce::Label& lbl)
{
    g.setColour(lbl.findColour(juce::Label::textColourId));
    g.setFont(lbl.getFont());
    g.drawFittedText(lbl.getText(), lbl.getLocalBounds(), lbl.getJustificationType(), 1);
}

void ArcaneKnobLAF::drawButtonBackground(juce::Graphics& g, juce::Button& btn,
                                          const juce::Colour&, bool, bool isDown)
{
    auto bgCol = btn.findColour(juce::TextButton::buttonColourId);
    if (bgCol.getAlpha() < 10) return;
    auto b = btn.getLocalBounds().toFloat().reduced(.5f);
    g.setColour(isDown ? kPurple.withAlpha(.3f) : bgCol);
    g.fillRoundedRectangle(b, 4.f);
    g.setColour(isDown ? kPurple : kCardBd);
    g.drawRoundedRectangle(b, 4.f, 1.f);
}

void ArcaneKnobLAF::drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) {}

void ArcaneKnobLAF::drawComboBox(juce::Graphics& g, int w, int h, bool,
                                   int, int, int, int, juce::ComboBox& cb)
{
    g.setColour(juce::Colour(0xff1a1a26));
    g.fillRoundedRectangle(0,0,(float)w,(float)h,4.f);
    g.setColour(kCardBd);
    g.drawRoundedRectangle(.5f,.5f,(float)w-1,(float)h-1,4.f,1.f);
    g.setColour(kPurple);
    g.setFont(juce::Font(10.f));
    g.drawText(cb.getText(), 8, 0, w-20, h, juce::Justification::centredLeft);
    // Arrow
    float ax=(float)w-14,ay=(float)h/2;
    g.drawLine(ax,ay-3,ax+5,ay+3,1.5f); g.drawLine(ax+5,ay+3,ax+10,ay-3,1.5f);
}

void ArcaneKnobLAF::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
    bool, bool, bool isHighlighted, bool, bool, const juce::String& text,
    const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    if (isHighlighted) { g.setColour(kPurDim); g.fillRect(area); }
    g.setColour(isHighlighted ? juce::Colours::white : kText);
    g.setFont(juce::Font(11.f));
    g.drawText(text, area.reduced(8,0), juce::Justification::centredLeft);
}

// ── AKnob ─────────────────────────────────────────────────────────────────────
void AKnob::setup(juce::Component* parent, juce::AudioProcessorValueTreeState& apvts,
                   const juce::String& paramId, ArcaneKnobLAF* laf)
{
    slider.setLookAndFeel(laf);
    parent->addAndMakeVisible(slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramId, slider);
    valLabel.setJustificationType(juce::Justification::centred);
    valLabel.setFont(juce::Font(9.f));
    valLabel.setColour(juce::Label::textColourId, kPurple);
    parent->addAndMakeVisible(valLabel);
    slider.onValueChange = [this]{ valLabel.setText(juce::String(slider.getValue(),1), juce::dontSendNotification); };
    slider.onValueChange();
}

void AKnob::place(int cx, int cy, int sz)
{
    slider.setBounds(cx-sz/2, cy-sz/2, sz, sz);
    valLabel.setBounds(cx-24, cy+sz/2+2, 48, 11);
    valLabel.setVisible(false);
}

// ── Constructor ───────────────────────────────────────────────────────────────
ArcaneEclipseEditor::ArcaneEclipseEditor(ArcaneEclipseProcessor& p)
    : AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&laf);
    setSize(kPlugW, kPlugH);
    bgImage = juce::ImageCache::getFromMemory(BinaryData::background_png, BinaryData::background_pngSize);

    // Strip knobs
    kInput.setup(this,p.apvts,ArcaneEclipseProcessor::idInputGain,  &laf);
    kGate .setup(this,p.apvts,ArcaneEclipseProcessor::idNoiseGate,  &laf);
    kComp .setup(this,p.apvts,ArcaneEclipseProcessor::idCompThresh, &laf);
    kOutput.setup(this,p.apvts,ArcaneEclipseProcessor::idOutputGain,&laf);
    // Amp knobs
    kGain    .setup(this,p.apvts,ArcaneEclipseProcessor::idAmpGain,    &laf); kGain    .valLabel.setVisible(true);
    kBass    .setup(this,p.apvts,ArcaneEclipseProcessor::idAmpBass,    &laf); kBass    .valLabel.setVisible(true);
    kMid     .setup(this,p.apvts,ArcaneEclipseProcessor::idAmpMid,     &laf); kMid     .valLabel.setVisible(true);
    kTreble  .setup(this,p.apvts,ArcaneEclipseProcessor::idAmpTreble,  &laf); kTreble  .valLabel.setVisible(true);
    kPresence.setup(this,p.apvts,ArcaneEclipseProcessor::idAmpPresence,&laf); kPresence.valLabel.setVisible(true);
    kMaster  .setup(this,p.apvts,ArcaneEclipseProcessor::idAmpMaster,  &laf); kMaster  .valLabel.setVisible(true);
    // OD
    kODDrive.setup(this,p.apvts,ArcaneEclipseProcessor::idODDrive,&laf); kODDrive.valLabel.setVisible(true);
    kODTone .setup(this,p.apvts,ArcaneEclipseProcessor::idODTone, &laf); kODTone .valLabel.setVisible(true);
    kODLevel.setup(this,p.apvts,ArcaneEclipseProcessor::idODLevel,&laf); kODLevel.valLabel.setVisible(true);
    // Mod
    kModRate .setup(this,p.apvts,ArcaneEclipseProcessor::idModRate, &laf); kModRate .valLabel.setVisible(true);
    kModDepth.setup(this,p.apvts,ArcaneEclipseProcessor::idModDepth,&laf); kModDepth.valLabel.setVisible(true);
    kModMix  .setup(this,p.apvts,ArcaneEclipseProcessor::idModMix,  &laf); kModMix  .valLabel.setVisible(true);
    // Delay
    kDTime    .setup(this,p.apvts,ArcaneEclipseProcessor::idDelayTime,    &laf); kDTime    .valLabel.setVisible(true);
    kDFeedback.setup(this,p.apvts,ArcaneEclipseProcessor::idDelayFeedback,&laf); kDFeedback.valLabel.setVisible(true);
    kDMix     .setup(this,p.apvts,ArcaneEclipseProcessor::idDelayMix,     &laf); kDMix     .valLabel.setVisible(true);
    // Reverb
    kRDecay.setup(this,p.apvts,ArcaneEclipseProcessor::idReverbDecay,&laf); kRDecay.valLabel.setVisible(true);
    kRSize .setup(this,p.apvts,ArcaneEclipseProcessor::idReverbSize, &laf); kRSize .valLabel.setVisible(true);
    kRMix  .setup(this,p.apvts,ArcaneEclipseProcessor::idReverbMix,  &laf); kRMix  .valLabel.setVisible(true);

    // Toggles
    for (auto* t : { &tbOD,&tbMod,&tbDelay,&tbReverb,&tbCab }) addAndMakeVisible(*t);
    attOD    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idODOn,    tbOD);
    attMod   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idModOn,   tbMod);
    attDelay = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idDelayOn, tbDelay);
    attReverb= std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idReverbOn,tbReverb);
    attCab   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idCabBypass,tbCab);

    // Dropdowns — Modulation type
    comboModType.addItem("Analog Chorus",1); comboModType.addItem("Flanger",2); comboModType.addItem("Tremolo",3);
    comboModType.setSelectedId(1);
    addAndMakeVisible(comboModType);
    attModType = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts,ArcaneEclipseProcessor::idModType,comboModType);

    // Delay type
    comboDelayType.addItem("1/4 D",1); comboDelayType.addItem("1/4",2); comboDelayType.addItem("1/8 D",3); comboDelayType.addItem("1/2",4);
    comboDelayType.setSelectedId(1);
    addAndMakeVisible(comboDelayType);
    attDelayType = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts,ArcaneEclipseProcessor::idDelayType,comboDelayType);

    // Reverb type
    comboReverbType.addItem("Plate",1); comboReverbType.addItem("Hall",2); comboReverbType.addItem("Room",3); comboReverbType.addItem("Spring",4);
    comboReverbType.setSelectedId(1);
    addAndMakeVisible(comboReverbType);
    attReverbType = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts,ArcaneEclipseProcessor::idReverbType,comboReverbType);

    // Cabinet dropdown
    comboCab.addItem("4x12 British Vintage 30",1); comboCab.addItem("2x12 Vintage 30",2); comboCab.addItem("1x12 AlNiCo",3);
    comboCab.setSelectedId(1);
    for (auto* cb : {&comboModType,&comboDelayType,&comboReverbType,&comboCab}) {
        cb->setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        cb->setColour(juce::ComboBox::outlineColourId,    juce::Colours::transparentBlack);
        cb->setColour(juce::ComboBox::textColourId,       juce::Colours::transparentBlack);
        cb->setColour(juce::ComboBox::arrowColourId,      juce::Colours::transparentBlack);
    }
    addAndMakeVisible(comboCab);

    // OD NAM loader
    addAndMakeVisible(btnBrowseNAM);
    btnBrowseNAM.onClick = [this]{
        chooserModel = std::make_unique<juce::FileChooser>("Load NAM Model",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.nam");
        chooserModel->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc){ auto r=fc.getResults(); if(!r.isEmpty()){proc.loadNAMModel(r[0]);repaint();} });
    };
    for (auto* b : {&btnBrowseNAM,&btnLoadModel,&btnLoadIR}) {
        b->setColour(juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
        b->setColour(juce::TextButton::textColourOffId, juce::Colours::transparentBlack);
    }
    addAndMakeVisible(btnLoadModel);
    btnLoadModel.onClick = [this]{
        chooserModel = std::make_unique<juce::FileChooser>("Load NAM Model",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.nam");
        chooserModel->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc){ auto r=fc.getResults(); if(!r.isEmpty()){proc.loadNAMModel(r[0]);repaint();} });
    };
    addAndMakeVisible(btnLoadIR);
    btnLoadIR.onClick = [this]{
        chooserIR = std::make_unique<juce::FileChooser>("Load Cabinet IR",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.wav");
        chooserIR->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc){ auto r=fc.getResults(); if(!r.isEmpty()){proc.loadIR(r[0]);repaint();} });
    };

    sliderDist.setRange(0,30,.1); sliderDist.setValue(2.5);
    sliderDist.setColour(juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
    sliderDist.setColour(juce::Slider::trackColourId,      juce::Colours::transparentBlack);
    sliderDist.setColour(juce::Slider::thumbColourId,      kPurple.withAlpha(.8f));
    addAndMakeVisible(sliderDist);

    tabCabinet.setClickingTogglesState(false); tabIRLoader.setClickingTogglesState(false);
    tabCabinet .onClick=[this]{activeCabTab=0;repaint();};
    tabIRLoader.onClick=[this]{activeCabTab=1;repaint();};
    for (auto* t:{&tabCabinet,&tabIRLoader}) {
        t->setColour(juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
        t->setColour(juce::TextButton::textColourOffId, juce::Colours::transparentBlack);
    }
    addAndMakeVisible(tabCabinet); addAndMakeVisible(tabIRLoader);

    startTimerHz(15);
}

ArcaneEclipseEditor::~ArcaneEclipseEditor() { stopTimer(); setLookAndFeel(nullptr); }
void ArcaneEclipseEditor::timerCallback() { vuIn*=.92f; vuOut*=.92f; repaint(); }

juce::Rectangle<int> ArcaneEclipseEditor::chainNodeRect(int i) const
{
    // Measured from concept image: 8 nodes at cy=92, 58px spacing from cx=267
    static const int nodeX[8] = { 267, 325, 383, 441, 499, 557, 615, 673 };
    return { nodeX[i]-26, 69, 52, 46 };
}

void ArcaneEclipseEditor::resized()
{
    // Strip knobs — measured from concept (1577x997 -> 980x620)
    kInput .place(sx(80),  sy(148), sw(96));
    kGate  .place(sx(208), sy(148), sw(96));
    kComp  .place(sx(1108),sy(148), sw(96));
    kOutput.place(sx(1248),sy(148), sw(96));

    // Amp knobs (cy=274 in plugin)
    kGain    .place(sx(228), sy(440), sw(134));
    kBass    .place(sx(382), sy(440), sw(134));
    kMid     .place(sx(536), sy(440), sw(134));
    kTreble  .place(sx(690), sy(440), sw(134));
    kPresence.place(sx(848), sy(440), sw(134));
    kMaster  .place(sx(1002),sy(440), sw(134));

    // OD card knobs
    kODDrive.place(sx(68),  sy(620), sw(94));
    kODTone .place(sx(158), sy(620), sw(94));
    kODLevel.place(sx(108), sy(708), sw(94));

    // Mod card knobs
    kModRate .place(sx(262), sy(620), sw(94));
    kModDepth.place(sx(352), sy(620), sw(94));
    kModMix  .place(sx(306), sy(708), sw(94));

    // Delay card knobs
    kDTime    .place(sx(462), sy(620), sw(94));
    kDFeedback.place(sx(550), sy(620), sw(94));
    kDMix     .place(sx(504), sy(700), sw(94));

    // Reverb card knobs
    kRDecay.place(sx(652), sy(620), sw(94));
    kRSize .place(sx(742), sy(620), sw(94));
    kRMix  .place(sx(696), sy(700), sw(94));

    // Stomp toggles (invisible, functional)
    tbOD    .setBounds(sx(108)-18, sy(788)-18, 36, 36);
    tbMod   .setBounds(sx(306)-18, sy(788)-18, 36, 36);
    tbDelay .setBounds(sx(504)-18, sy(788)-18, 36, 36);
    tbReverb.setBounds(sx(696)-18, sy(788)-18, 36, 36);

    // Dropdowns
    comboModType  .setBounds(sx(210), sy(830), sw(192), sw(38));
    comboDelayType.setBounds(sx(406), sy(830), sw(192), sw(38));
    comboReverbType.setBounds(sx(600),sy(830), sw(192), sw(38));
    comboCab      .setBounds(sx(986), sy(594), sw(346), sw(38));

    // NAM loader on OD card
    btnBrowseNAM.setBounds(sx(14), sy(830), sw(192), sw(38));

    // Load buttons
    btnLoadModel.setBounds(sx(986), sy(748), sw(166), sw(44));
    btnLoadIR   .setBounds(sx(1164),sy(748), sw(166), sw(44));

    // Distance slider + cab tabs
    sliderDist.setBounds(sx(986), sy(638), sw(346), sw(18));
    tabCabinet .setBounds(sx(744), sy(538), sw(126), sw(36));
    tabIRLoader.setBounds(sx(870), sy(538), sw(126), sw(36));
    tbCab.setBounds(sw(970), sy(538), 16, 16);
}

void ArcaneEclipseEditor::paint(juce::Graphics& g)
{
    // Background image
    if (bgImage.isValid())
        g.drawImage(bgImage, 0, 0, kPlugW, kPlugH, 0, 0, (int)kImgW, (int)kImgH);
    else
    {
        g.fillAll(kDark);
        g.setColour(kPurple);
        g.drawText("Arcane Eclipse — image not loaded", getLocalBounds(), juce::Justification::centred);
    }

    // VU meters — purple segmented
    paintVU(g, 14.f, 66.f, 14.f, 88.f, vuIn);
    paintVU(g, sx(1340.f), 66.f, 14.f, 88.f, vuOut);

    // Signal chain
    paintChain(g);

    // Orange (purple) dots beside GATE and COMP
    g.setColour(kPurple);
    g.fillEllipse((float)(sx(260)), 118.f, 7.f, 7.f);
    g.fillEllipse((float)(sx(1160)),118.f, 7.f, 7.f);

    // FX card overlays
    paintFXOverlays(g);

    // Cabinet overlays
    paintCabOverlay(g);

    // AMP/CAB dots in footer
    g.setColour(proc.isNAMLoaded() ? kPurple : kMuted);
    g.fillEllipse(876.f, 598.f, 8.f, 8.f);
    g.setColour(proc.isIRLoaded() ? kMuted.brighter(.3f) : kMuted.withAlpha(.4f));
    g.fillEllipse(930.f, 598.f, 8.f, 8.f);
}

void ArcaneEclipseEditor::paintVU(juce::Graphics& g, float bx, float by, float bw, float bh, float lvl)
{
    int nSegs = 14;
    float segH = bh / nSegs;
    int lit = (int)(nSegs * juce::jlimit(0.f,1.f,lvl));
    for (int s = 0; s < lit; ++s) {
        float sy2 = by + bh - (s+1)*segH + 1.f;
        if (s >= 12)      g.setColour(juce::Colour(0xffcc2266));
        else if (s >= 10) g.setColour(kPurDim);
        else               g.setColour(kPurple);
        g.fillRoundedRectangle(bx+1, sy2, bw-2, segH-1.5f, 1.f);
    }
    g.setColour(kCardBd);
    g.drawRoundedRectangle(bx, by, bw, bh, 2.f, .5f);
}

void ArcaneEclipseEditor::paintChain(juce::Graphics& g)
{
    for (int i = 0; i < 8; ++i) {
        auto nb = chainNodeRect(i);
        paintChainNode(g, i, nb, i == activeNode);
        if (i < 7) {
            auto nb2 = chainNodeRect(i+1);
            float ax=(float)nb.getRight()+2,ay=(float)nb.getCentreY();
            float ax2=(float)nb2.getX()-2;
            g.setColour(kPurple.withAlpha(.7f));
            g.drawLine(ax,ay,ax2,ay,1.5f);
            g.drawLine(ax2-5,ay-4,ax2,ay,1.5f);
            g.drawLine(ax2-5,ay+4,ax2,ay,1.5f);
        }
    }
}

void ArcaneEclipseEditor::paintChainNode(juce::Graphics& g, int idx,
                                           juce::Rectangle<int> b, bool active)
{
    g.setColour(active ? kPurple.withAlpha(.15f) : juce::Colour(0xff1a1a26).withAlpha(.92f));
    g.fillRoundedRectangle(b.toFloat(), 6.f);
    g.setColour(active ? kPurple : kCardBd);
    g.drawRoundedRectangle(b.toFloat(), 6.f, active ? 2.f : 1.5f);

    auto ib = b.toFloat().reduced(8.f, 7.f);
    float cx=ib.getCentreX(), cy=ib.getCentreY();
    g.setColour(active ? kPurple : kMuted);
    juce::Path p;
    switch(idx) {
        case 0: g.drawLine(ib.getX(),ib.getBottom(),ib.getRight(),ib.getY(),2.f); break;
        case 1: p.startNewSubPath(ib.getX(),ib.getBottom()); p.lineTo(cx,cy+3);
                p.cubicTo(cx,cy+3,cx,cy-3,ib.getRight(),ib.getY());
                g.strokePath(p,juce::PathStrokeType(1.8f)); break;
        case 2: g.fillEllipse(ib.getX(),cy-6,8,8); g.fillEllipse(ib.getX(),cy+1,8,8);
                g.drawLine(ib.getX()+9,cy-2,ib.getRight(),cy-2,1.5f);
                g.drawLine(ib.getX()+9,cy+5,ib.getRight(),cy+5,1.5f); break;
        case 3: g.drawRoundedRectangle(ib,2.f,1.8f);
                g.drawLine(ib.getX(),ib.getY()+6,ib.getRight(),ib.getY()+6,1.f);
                g.fillEllipse(cx-4,cy-3,9,9); break;
        case 4: g.drawRoundedRectangle(ib.reduced(0,1),3.f,1.8f);
                g.drawEllipse(cx-6,cy-5,12,12,1.5f); g.drawEllipse(cx-3,cy-2,6,6,1.f); break;
        case 5: { float pos[]={.4f,.7f,.25f};
                for(int f=0;f<3;++f){ float fx=ib.getX()+f*(ib.getWidth()/2.5f);
                    g.drawLine(fx,ib.getY(),fx,ib.getBottom(),1.2f);
                    g.fillEllipse(fx-3,ib.getY()+pos[f]*ib.getHeight()-3,6,6); } break; }
        case 6: g.drawEllipse(ib.reduced(1),1.8f);
                g.drawLine(cx,cy,cx,ib.getY()+5,1.8f); g.drawLine(cx,cy,cx+5,cy+5,1.8f); break;
        case 7: for(int wave=0;wave<2;++wave){ float wy=cy-2+wave*7.f; p.clear();
                p.startNewSubPath(ib.getX(),wy);
                p.cubicTo(ib.getX()+6,wy-5,ib.getX()+12,wy+5,cx,wy);
                p.cubicTo(cx+5,wy-5,ib.getRight()-5,wy+5,ib.getRight(),wy);
                g.strokePath(p,juce::PathStrokeType(1.5f-wave*.4f)); } break;
    }
    g.setFont(juce::Font(8.f, juce::Font::bold));
    g.setColour(active ? kPurple : kMuted);
    g.drawText(kChainLabels[idx],
               juce::Rectangle<int>(b.getX()-5,b.getBottom()+3,b.getWidth()+10,11),
               juce::Justification::centred);
}

void ArcaneEclipseEditor::paintStomp(juce::Graphics& g, float cx, float cy, bool on)
{
    float r = 20.f;
    juce::ColourGradient sg(juce::Colour(0xff3a3a4a), cx-r*.3f, cy-r*.3f,
                             juce::Colour(0xff0a0a12), cx+r, cy+r, true);
    g.setGradientFill(sg);
    g.fillEllipse(cx-r, cy-r, r*2, r*2);
    g.setColour(juce::Colour(0xff4a4a5a));
    g.drawEllipse(cx-r, cy-r, r*2, r*2, 2.5f);
    g.setColour(juce::Colour(0xff2a2a3a));
    g.drawEllipse(cx-r+4, cy-r+4, (r-4)*2, (r-4)*2, 1.f);
    if (on) { g.setColour(kPurple.withAlpha(.3f)); g.fillEllipse(cx-7, cy+r+1, 14.f, 14.f); }
    g.setColour(on ? kPurple : kMuted);
    g.fillEllipse(cx-4, cy+r+4, 8.f, 8.f);
}

void ArcaneEclipseEditor::paintFXOverlays(juce::Graphics& g)
{
    // FX card title/power for all 4 cards
    const char* titles[] = {"OVERDRIVE","MODULATION","DELAY","REVERB"};
    const int cardX[] = {sx(14), sx(210), sx(406), sx(600)};
    juce::ToggleButton* tbs[] = {&tbOD, &tbMod, &tbDelay, &tbReverb};
    float stompCx[] = {(float)sx(108),(float)sx(306),(float)sx(504),(float)sx(696)};

    for (int i = 0; i < 4; ++i) {
        bool on = tbs[i]->getToggleState();
        g.setFont(juce::Font(11.f, juce::Font::bold));
        g.setColour(on ? kPurple : kMuted);
        g.drawText(titles[i], cardX[i]+10, sy(548), 140, 16, juce::Justification::centredLeft);
        // Power icon
        float pix=(float)(cardX[i]+155), piy=(float)sy(550);
        g.setColour(on ? kPurple : kMuted);
        g.drawEllipse(pix, piy+2, 11.f, 11.f, 1.5f);
        g.fillRect(pix+4, piy-1, 2.f, 6.f);
        // Stomp
        paintStomp(g, stompCx[i], (float)sy(800), on);
    }

    // OD card: NAM LOADER label
    g.setFont(juce::Font(9.f, juce::Font::bold));
    g.setColour(kMuted);
    g.drawText("NAM LOADER", sx(14)+6, sy(756), 110, 12, juce::Justification::centredLeft);

    // Delay: SYNC label + TAP visual
    g.drawText("SYNC", sx(406)+6, sy(762), 50, 12, juce::Justification::centredLeft);
    g.drawText("TAP",  sx(510),   sy(756), 36, 14, juce::Justification::centred);
    g.setColour(kCardBd);
    g.drawRoundedRectangle((float)sx(500), (float)sy(752), 46.f, 18.f, 3.f, 1.f);

    // Reverb: TYPE label
    g.setFont(juce::Font(9.f, juce::Font::bold)); g.setColour(kMuted);
    g.drawText("TYPE", sx(600)+6, sy(762), 50, 12, juce::Justification::centredLeft);
    g.drawText("TYPE", sx(406)+6, sy(762)+14, 50, 12, juce::Justification::centredLeft);
}

void ArcaneEclipseEditor::paintCabOverlay(juce::Graphics& g)
{
    // Active tab underline
    g.setColour(kPurple);
    g.fillRect((float)(activeCabTab==0 ? sx(744) : sx(870)), (float)sy(573), (float)sw(120), 2.f);

    // CABINET / MIC POSITION labels
    g.setFont(juce::Font(9.f, juce::Font::bold));
    g.setColour(kMuted);
    g.drawText("CABINET",      sx(986), sy(576), 80, 12, juce::Justification::centredLeft);
    g.drawText("MIC POSITION", sx(986), sy(627), 90, 12, juce::Justification::centredLeft);
    g.setFont(juce::Font(10.f)); g.setColour(kText);
    g.drawText(juce::String(sliderDist.getValue(),1)+" cm",
               sx(1280), sy(627), 50, 12, juce::Justification::centredRight);

    // MODEL / IR labels
    g.setFont(juce::Font(9.f, juce::Font::bold)); g.setColour(kMuted);
    g.drawText("MODEL", sx(986), sy(653), 60, 12, juce::Justification::centredLeft);
    g.setFont(juce::Font(10.f));
    g.setColour(proc.isNAMLoaded() ? kText : kMuted.withAlpha(.5f));
    g.drawFittedText(proc.isNAMLoaded() ? proc.getLoadedNAMName() : "No model loaded",
                     sx(986), sy(666), sw(346), 14, juce::Justification::centredLeft, 1);

    g.setFont(juce::Font(9.f, juce::Font::bold)); g.setColour(kMuted);
    g.drawText("IR", sx(986), sy(683), 30, 12, juce::Justification::centredLeft);
    g.setFont(juce::Font(10.f));
    g.setColour(proc.isIRLoaded() ? kText : kMuted.withAlpha(.5f));
    g.drawFittedText(proc.isIRLoaded() ? proc.getLoadedIRName() : "No IR loaded",
                     sx(986), sy(696), sw(346), 14, juce::Justification::centredLeft, 1);
}

void ArcaneEclipseEditor::mouseDown(const juce::MouseEvent& e)
{
    for (int i = 0; i < 8; ++i)
        if (chainNodeRect(i).contains(e.getPosition()))
            { activeNode = i; repaint(); return; }
    AudioProcessorEditor::mouseDown(e);
}
