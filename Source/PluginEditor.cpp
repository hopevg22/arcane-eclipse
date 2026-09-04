#include "PluginEditor.h"

static const juce::Colour kPurple { 0xff8B5CF6 };
static const juce::Colour kPurDim { 0xff6D45D4 };
static const juce::Colour kBg     { 0xff0d0d12 };
static const juce::Colour kSurf   { 0xff13131a };
static const juce::Colour kCard   { 0xff1a1a24 };
static const juce::Colour kCardBd { 0xff2a2a3e };
static const juce::Colour kText   { 0xfff0f0ff };
static const juce::Colour kMuted  { 0xff7878aa };
static const juce::Colour kStrip  { 0xff0f0f18 };

const juce::String ArcaneEclipseEditor::kChainLabels[9] =
    {"GATE","COMP","DRIVE","AMP","CAB","EQ","MOD","DELAY","REVERB"};

// ── ArcLAF ────────────────────────────────────────────────────────────────────
ArcLAF::ArcLAF()
{
    setColour(juce::TextButton::buttonColourId,    juce::Colour(0xff1e1e2e));
    setColour(juce::TextButton::textColourOffId,   kText);
    setColour(juce::TextButton::buttonOnColourId,  kPurple);
    setColour(juce::TextButton::textColourOnId,    juce::Colours::white);
    setColour(juce::ComboBox::backgroundColourId,  juce::Colour(0xff1a1a28));
    setColour(juce::ComboBox::textColourId,        kText);
    setColour(juce::ComboBox::outlineColourId,     kCardBd);
    setColour(juce::ComboBox::arrowColourId,       kPurple);
    setColour(juce::PopupMenu::backgroundColourId,               juce::Colour(0xff1a1a28));
    setColour(juce::PopupMenu::textColourId,                     kText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,    kPurDim);
    setColour(juce::PopupMenu::highlightedTextColourId,          juce::Colours::white);
    setColour(juce::Slider::thumbColourId,         kPurple);
    setColour(juce::Slider::trackColourId,         kPurple);
    setColour(juce::Slider::backgroundColourId,    kCardBd);
}

void ArcLAF::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                               float pos, float startA, float endA, juce::Slider&)
{
    auto b = juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h).reduced(4.f);
    auto c = b.getCentre();
    float r = juce::jmin(b.getWidth(),b.getHeight()) * .5f;

    // Outer track ring (dark)
    juce::Path track;
    track.addCentredArc(c.x,c.y,r*.88f,r*.88f,0,startA,endA,true);
    g.setColour(juce::Colour(0xff1e1e2e));
    g.strokePath(track, juce::PathStrokeType(4.f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Filled arc (purple)
    if (pos > 0.005f) {
        juce::Path arc;
        arc.addCentredArc(c.x,c.y,r*.88f,r*.88f,0,startA,startA+(endA-startA)*pos,true);
        g.setColour(kPurple);
        g.strokePath(arc, juce::PathStrokeType(4.f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }

    // Knob body gradient
    juce::ColourGradient kg(juce::Colour(0xff2a2a3e), c.x-r*.3f, c.y-r*.3f,
                             juce::Colour(0xff0d0d18), c.x+r*.5f, c.y+r*.5f, true);
    g.setGradientFill(kg);
    g.fillEllipse(c.x-r*.72f, c.y-r*.72f, r*1.44f, r*1.44f);

    // Knob border
    g.setColour(kCardBd);
    g.drawEllipse(c.x-r*.72f, c.y-r*.72f, r*1.44f, r*1.44f, 1.f);

    // Pointer dot
    float ang = startA + pos*(endA-startA);
    float pr = r*.58f;
    float px = c.x + pr*std::sin(ang);
    float py = c.y - pr*std::cos(ang);
    g.setColour(kPurple);
    g.fillEllipse(px-3.f, py-3.f, 6.f, 6.f);
    // Inner highlight
    g.setColour(juce::Colour(0xff3a3a5a));
    g.drawEllipse(c.x-r*.4f, c.y-r*.4f, r*.8f, r*.8f, .5f);
}

void ArcLAF::drawLabel(juce::Graphics& g, juce::Label& lbl)
{
    g.setColour(lbl.findColour(juce::Label::textColourId));
    g.setFont(lbl.getFont());
    g.drawFittedText(lbl.getText(), lbl.getLocalBounds(), lbl.getJustificationType(), 1);
}

void ArcLAF::drawButtonBackground(juce::Graphics& g, juce::Button& btn,
                                   const juce::Colour&, bool, bool isDown)
{
    auto b = btn.getLocalBounds().toFloat().reduced(.5f);
    bool on = btn.getToggleState();
    auto bg = btn.findColour(juce::TextButton::buttonColourId);
    g.setColour(isDown ? kPurple.withAlpha(.3f) : (on ? kPurple.withAlpha(.2f) : bg));
    g.fillRoundedRectangle(b, 5.f);
    g.setColour(on || isDown ? kPurple : kCardBd);
    g.drawRoundedRectangle(b, 5.f, 1.f);
}

void ArcLAF::drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) {}

void ArcLAF::drawComboBox(juce::Graphics& g, int w, int h, bool,
                           int, int, int, int, juce::ComboBox& cb)
{
    g.setColour(juce::Colour(0xff1a1a28));
    g.fillRoundedRectangle(0,0,(float)w,(float)h,4.f);
    g.setColour(kCardBd);
    g.drawRoundedRectangle(.5f,.5f,(float)w-1.f,(float)h-1.f,4.f,1.f);
    // Arrow
    float ax=(float)w-14, ay=(float)h*.5f;
    g.setColour(kPurple);
    juce::Path arrow;
    arrow.addTriangle(ax,ay-3,ax+8,ay-3,ax+4,ay+3);
    g.fillPath(arrow);
}

void ArcLAF::positionComboBoxText(juce::ComboBox& cb, juce::Label& lbl)
{
    lbl.setBounds(6, 1, cb.getWidth()-22, cb.getHeight()-2);
    lbl.setFont(juce::Font(11.f));
}

void ArcLAF::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
    bool, bool, bool isHighlighted, bool, bool, const juce::String& text,
    const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    if (isHighlighted) { g.setColour(kPurDim.withAlpha(.6f)); g.fillRect(area); }
    g.setColour(isHighlighted ? juce::Colours::white : kText);
    g.setFont(juce::Font(11.f));
    g.drawText(text, area.reduced(8,0), juce::Justification::centredLeft);
}

// ── StompButton ───────────────────────────────────────────────────────────────
void StompButton::paint(juce::Graphics& g)
{
    bool on = getToggleState();
    auto b = getLocalBounds().toFloat().reduced(2.f);
    float cx = b.getCentreX(), cy = b.getCentreY(), r = b.getWidth()*.5f;

    // Outer ring
    g.setColour(juce::Colour(0xff2a2a3e));
    g.fillEllipse(b);
    g.setColour(on ? kPurple : juce::Colour(0xff3a3a5a));
    g.drawEllipse(b, 2.f);

    // Inner body
    juce::ColourGradient bg(juce::Colour(0xff252535), cx-r*.3f, cy-r*.3f,
                             juce::Colour(0xff0d0d18), cx+r*.4f, cy+r*.4f, true);
    g.setGradientFill(bg);
    g.fillEllipse(b.reduced(4.f));

    // LED dot
    g.setColour(on ? kPurple : juce::Colour(0xff2a2a3e));
    g.fillEllipse(cx-4, cy-4, 8, 8);
    if (on) {
        g.setColour(kPurple.withAlpha(.3f));
        g.fillEllipse(cx-7, cy-7, 14, 14);
    }
}

// ── AKnob ─────────────────────────────────────────────────────────────────────
void AKnob::setup(juce::Component* parent, juce::AudioProcessorValueTreeState& apvts,
                   const juce::String& paramId, const juce::String& name, ArcLAF* laf)
{
    slider.setLookAndFeel(laf);
    parent->addAndMakeVisible(slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts,paramId,slider);

    nameLabel.setText(name, juce::dontSendNotification);
    nameLabel.setJustificationType(juce::Justification::centred);
    nameLabel.setFont(juce::Font(9.f, juce::Font::bold));
    nameLabel.setColour(juce::Label::textColourId, kMuted);
    parent->addAndMakeVisible(nameLabel);

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
    nameLabel.setBounds(cx-30, cy+sz/2+2,  60, 13);
    valLabel .setBounds(cx-24, cy+sz/2+15, 48, 12);
}

// ── Constructor ───────────────────────────────────────────────────────────────
ArcaneEclipseEditor::ArcaneEclipseEditor(ArcaneEclipseProcessor& p)
    : AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&laf);
    setSize(W, H);

    // Strip
    kInput .setup(this,p.apvts,ArcaneEclipseProcessor::idInputGain, "INPUT",    &laf);
    kGate  .setup(this,p.apvts,ArcaneEclipseProcessor::idNoiseGate, "GATE",     &laf);
    kComp  .setup(this,p.apvts,ArcaneEclipseProcessor::idCompThresh,"COMP",     &laf);
    kOutput.setup(this,p.apvts,ArcaneEclipseProcessor::idOutputGain,"OUTPUT",   &laf);

    // Amp
    kGain    .setup(this,p.apvts,ArcaneEclipseProcessor::idAmpGain,    "GAIN",    &laf);
    kBass    .setup(this,p.apvts,ArcaneEclipseProcessor::idAmpBass,    "BASS",    &laf);
    kMid     .setup(this,p.apvts,ArcaneEclipseProcessor::idAmpMid,     "MID",     &laf);
    kTreble  .setup(this,p.apvts,ArcaneEclipseProcessor::idAmpTreble,  "TREBLE",  &laf);
    kPresence.setup(this,p.apvts,ArcaneEclipseProcessor::idAmpPresence,"PRESENCE",&laf);
    kMaster  .setup(this,p.apvts,ArcaneEclipseProcessor::idAmpMaster,  "MASTER",  &laf);

    // FX
    kODDrive  .setup(this,p.apvts,ArcaneEclipseProcessor::idODDrive,        "DRIVE",    &laf);
    kODTone   .setup(this,p.apvts,ArcaneEclipseProcessor::idODTone,         "TONE",     &laf);
    kODLevel  .setup(this,p.apvts,ArcaneEclipseProcessor::idODLevel,        "LEVEL",    &laf);
    kModRate  .setup(this,p.apvts,ArcaneEclipseProcessor::idModRate,        "RATE",     &laf);
    kModDepth .setup(this,p.apvts,ArcaneEclipseProcessor::idModDepth,       "DEPTH",    &laf);
    kModMix   .setup(this,p.apvts,ArcaneEclipseProcessor::idModMix,         "MIX",      &laf);
    kDTime    .setup(this,p.apvts,ArcaneEclipseProcessor::idDelayTime,      "TIME",     &laf);
    kDFeedback.setup(this,p.apvts,ArcaneEclipseProcessor::idDelayFeedback,  "FEEDBACK", &laf);
    kDMix     .setup(this,p.apvts,ArcaneEclipseProcessor::idDelayMix,       "MIX",      &laf);
    kRDecay   .setup(this,p.apvts,ArcaneEclipseProcessor::idReverbDecay,    "DECAY",    &laf);
    kRSize    .setup(this,p.apvts,ArcaneEclipseProcessor::idReverbSize,     "SIZE",     &laf);
    kRMix     .setup(this,p.apvts,ArcaneEclipseProcessor::idReverbMix,      "MIX",      &laf);

    // Gate and Comp strip toggles
    addAndMakeVisible(tbGate); addAndMakeVisible(tbComp);
    attGate  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                   p.apvts, ArcaneEclipseProcessor::idGateOn, tbGate);
    attComp2 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                   p.apvts, ArcaneEclipseProcessor::idCompOn, tbComp);

    // X clear buttons
    addAndMakeVisible(btnClearModel);
    btnClearModel.setColour(juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
    btnClearModel.setColour(juce::TextButton::textColourOffId, kMuted);
    btnClearModel.onClick = [this]{ proc.unloadNAMModel(); repaint(); };

    addAndMakeVisible(btnClearIR);
    btnClearIR.setColour(juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
    btnClearIR.setColour(juce::TextButton::textColourOffId, kMuted);
    btnClearIR.onClick = [this]{ proc.unloadIR(); repaint(); };
    attOD    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idODOn,    stompOD);
    attMod   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idModOn,   stompMod);
    attDelay = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idDelayOn, stompDelay);
    attReverb= std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idReverbOn,stompReverb);
    addAndMakeVisible(tbCab);
    attCab   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idCabBypass,tbCab);

    // Dropdowns
    comboModType.addItem("Analog Chorus",1); comboModType.addItem("Flanger",2); comboModType.addItem("Tremolo",3);
    comboModType.setSelectedId(1); addAndMakeVisible(comboModType);
    attModType = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts,ArcaneEclipseProcessor::idModType,comboModType);

    comboDelayType.addItem("1/4",1); comboDelayType.addItem("1/4D",2); comboDelayType.addItem("1/8",3); comboDelayType.addItem("1/2",4);
    comboDelayType.setSelectedId(1); addAndMakeVisible(comboDelayType);
    attDelayType = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts,ArcaneEclipseProcessor::idDelayType,comboDelayType);

    comboReverbType.addItem("Plate",1); comboReverbType.addItem("Hall",2); comboReverbType.addItem("Room",3); comboReverbType.addItem("Spring",4);
    comboReverbType.setSelectedId(1); addAndMakeVisible(comboReverbType);
    attReverbType = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts,ArcaneEclipseProcessor::idReverbType,comboReverbType);

    comboCab.addItem("4x12  British V30",1); comboCab.addItem("2x12  Vintage 30",2); comboCab.addItem("1x12  AlNiCo",3);
    comboCab.setSelectedId(1); addAndMakeVisible(comboCab);

    // Buttons
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
    addAndMakeVisible(btnBrowse);
    btnBrowse.setButtonText("BROWSE NAM");
    btnBrowse.onClick = [this]{
        chooserModel = std::make_unique<juce::FileChooser>("Load NAM Model",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.nam");
        chooserModel->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc){ auto r=fc.getResults(); if(!r.isEmpty()){proc.loadNAMModel(r[0]);repaint();} });
    };

    sliderDist.setRange(0,30,.1); sliderDist.setValue(2.5);
    addAndMakeVisible(sliderDist);

    tabCab.setClickingTogglesState(false); tabIR.setClickingTogglesState(false);
    tabCab.onClick=[this]{activeCabTab=0;repaint();}; tabIR.onClick=[this]{activeCabTab=1;repaint();};
    addAndMakeVisible(tabCab); addAndMakeVisible(tabIR);

    startTimerHz(15);
}

ArcaneEclipseEditor::~ArcaneEclipseEditor() { stopTimer(); setLookAndFeel(nullptr); }
void ArcaneEclipseEditor::timerCallback() { vuIn*=.92f; vuOut*=.92f; repaint(); }

// ── Layout ────────────────────────────────────────────────────────────────────
juce::Rectangle<int> ArcaneEclipseEditor::chainNodeBounds(int i) const
{
    // 9 nodes: GATE COMP DRIVE AMP CAB EQ MOD DELAY REVERB
    int nodeW = 52, nodeH = 46;
    int totalW = 9*nodeW + 8*6;
    int startX = (W - totalW) / 2;
    int x = startX + i*(nodeW+6);
    int y = kTopH + (kStripH - nodeH)/2;
    return {x, y, nodeW, nodeH};
}

void ArcaneEclipseEditor::resized()
{
    int stripY = kTopH;
    int ampY   = kTopH + kStripH;
    int fxY    = ampY + kAmpH;
    int cabY   = fxY + kFXH;
    int footY  = H - kFootH;

    // ── Strip gate/comp power toggles ─────────────────────────────────────────
    // Small invisible toggle buttons positioned over the power dot icons
    tbGate.setBounds(182, kTopH+6, 14, 14);
    tbComp.setBounds(W-112, kTopH+6, 14, 14);

    // ── Strip knobs — moved inward away from VU meters ───────────────────────
    int kSz = 58;
    kInput .place(80,  stripY+60, kSz);
    kGate  .place(154, stripY+60, kSz);
    kComp  .place(W-154, stripY+60, kSz);
    kOutput.place(W-80,  stripY+60, kSz);

    // ── Amp knobs ─────────────────────────────────────────────────────────────
    int aSz=72, aCy=ampY+kAmpH*3/4;  // pushed down into faceplate area
    int ampSpan = W-200;
    int aSpacing = ampSpan/6;
    for(int i=0;i<6;++i){
        AKnob* ks[]={&kGain,&kBass,&kMid,&kTreble,&kPresence,&kMaster};
        ks[i]->place(100+aSpacing/2+i*aSpacing, aCy, aSz);
    }

    // ── FX section ────────────────────────────────────────────────────────────
    // 4 cards equally spaced, cabinet takes remaining width on right
    int nCards=4, cabW=320;
    int fxAreaW = W-cabW-16;
    int cardW = fxAreaW/nCards - 6;
    int fxKSz=48, fxTopRow=fxY+60, fxBotRow=fxY+118;

    // OD card knobs
    int odX = 8;
    kODDrive.place(odX+cardW/4,     fxTopRow, fxKSz);
    kODTone .place(odX+cardW*3/4,   fxTopRow, fxKSz);
    kODLevel.place(odX+cardW/2,     fxBotRow, fxKSz);
    stompOD.setBounds(odX+cardW/2-16, fxY+kFXH-56, 32, 32);
    btnBrowse.setBounds(odX+6, fxY+kFXH-30, cardW-12, 22);

    // Mod card
    int modX = odX+cardW+6;
    kModRate .place(modX+cardW/4,   fxTopRow, fxKSz);
    kModDepth.place(modX+cardW*3/4, fxTopRow, fxKSz);
    kModMix  .place(modX+cardW/2,   fxBotRow, fxKSz);
    stompMod.setBounds(modX+cardW/2-16, fxY+kFXH-56, 32, 32);
    comboModType.setBounds(modX+6, fxY+kFXH-30, cardW-12, 22);

    // Delay card
    int dlX = modX+cardW+6;
    kDTime    .place(dlX+cardW/4,   fxTopRow, fxKSz);
    kDFeedback.place(dlX+cardW*3/4, fxTopRow, fxKSz);
    kDMix     .place(dlX+cardW/2,   fxBotRow, fxKSz);
    stompDelay.setBounds(dlX+cardW/2-16, fxY+kFXH-56, 32, 32);
    comboDelayType.setBounds(dlX+6, fxY+kFXH-30, cardW-12, 22);

    // Reverb card
    int rvX = dlX+cardW+6;
    kRDecay.place(rvX+cardW/4,   fxTopRow, fxKSz);
    kRSize .place(rvX+cardW*3/4, fxTopRow, fxKSz);
    kRMix  .place(rvX+cardW/2,   fxBotRow, fxKSz);
    stompReverb.setBounds(rvX+cardW/2-16, fxY+kFXH-56, 32, 32);
    comboReverbType.setBounds(rvX+6, fxY+kFXH-30, cardW-12, 22);

    // ── Cabinet section ───────────────────────────────────────────────────────
    int cX = W-cabW-4;
    // No tabs — single "AMP & IR LOADER" panel
    int cabH = footY - fxY;
    // No distance slider
    sliderDist.setBounds(-200,-200,1,1);
    btnLoadModel.setBounds(cX+4,   fxY+cabH-62, (cabW-16)/2, 32);
    btnLoadIR   .setBounds(cX+4+(cabW-16)/2+8, fxY+cabH-62, (cabW-16)/2, 32);
    tbCab.setBounds(W-20, fxY+2, 16, 16);
    // Hide unused controls
    tabCab.setBounds(-200,-200,1,1);
    tabIR .setBounds(-200,-200,1,1);
    comboCab.setBounds(-200,-200,1,1);
    // X clear buttons — positioned beside MODEL and IR labels
    btnClearModel.setBounds(cX+cabW-28, fxY+50, 18, 18);
    btnClearIR   .setBounds(cX+cabW-28, fxY+88, 18, 18);
    // BROWSE NAM hidden
    btnBrowse.setBounds(-200,-200,1,1);
}

// ── paint ─────────────────────────────────────────────────────────────────────
void ArcaneEclipseEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBg);
    paintTopBar(g);
    paintStrip(g);
    paintChain(g);
    paintAmpHead(g);
    paintFXSection(g);
    paintCabSection(g);
    paintFooter(g);
}

void ArcaneEclipseEditor::paintTopBar(juce::Graphics& g)
{
    // Gradient bar
    juce::ColourGradient tg(juce::Colour(0xff13131e),0,0,kBg,0,(float)kTopH,false);
    g.setGradientFill(tg); g.fillRect(0,0,W,kTopH);
    g.setColour(kCardBd); g.drawHorizontalLine(kTopH,0.f,(float)W);

    // Logo star mark
    g.setColour(kPurple);
    float lx=22.f, ly=25.f, lr=12.f;
    for(int i=0;i<4;++i){
        float a=i*juce::MathConstants<float>::pi*.5f;
        g.drawLine(lx,ly, lx+lr*std::cos(a), ly+lr*std::sin(a), 1.5f);
    }
    g.fillEllipse(lx-3,ly-3,6,6);

    // Brand text
    g.setFont(juce::Font(20.f, juce::Font::bold));
    g.setColour(kText);
    g.drawText("ARCANE", 42, 13, 90, 24, juce::Justification::centredLeft);
    g.setFont(juce::Font(12.f, juce::Font::bold));
    g.setColour(kPurple);
    g.drawText("ECLIPSE", 136, 16, 74, 18, juce::Justification::centredLeft);
    g.setFont(juce::Font(9.f)); g.setColour(kMuted);
    g.drawText("v1.0.0", 214, 20, 44, 12, juce::Justification::centredLeft);

    // Preset box
    int px=(W-280)/2, py=9, pw=280, ph=32;
    g.setColour(juce::Colour(0xff0a0a14));
    g.fillRoundedRectangle((float)px,(float)py,(float)pw,(float)ph,5.f);
    g.setColour(kPurple);
    g.drawRoundedRectangle((float)px,(float)py,(float)pw,(float)ph,5.f,1.5f);
    g.setFont(juce::Font(12.f,juce::Font::bold)); g.setColour(kPurple);
    g.drawText("01A", px+10,py+1,36,ph-2, juce::Justification::centredLeft);
    g.setFont(juce::Font(12.f)); g.setColour(kText);
    juce::String nm = proc.isNAMLoaded() ? proc.getLoadedNAMName() : "Mystic Drive";
    juce::String srStr = " [" + juce::String((int)proc.getSampleRate()) + "Hz]";
    g.drawText(nm + srStr, px+52,py+1,pw-60,ph-2, juce::Justification::centredLeft);

    // Right icons
    auto drawIcon=[&](int x, bool orange){
        g.setColour(juce::Colour(0xff1e1e2e));
        g.fillRoundedRectangle((float)x,10.f,28.f,28.f,4.f);
        g.setColour(orange?kPurple:kMuted);
        g.drawRoundedRectangle((float)x,10.f,28.f,28.f,4.f,1.f);
    };
    drawIcon(W-92,false); g.setFont(juce::Font(11.f)); g.setColour(kMuted); g.drawText("⚙",W-92,10,28,28,juce::Justification::centred);
    drawIcon(W-58,false); g.setColour(kMuted); g.drawText("?",W-58,10,28,28,juce::Justification::centred);
    drawIcon(W-24,true);
    g.setColour(kPurple);
    g.drawEllipse((float)(W-18),15.f,10.f,10.f,1.5f);
    g.fillRect((float)(W-14),10.f,2.f,6.f);
}

void ArcaneEclipseEditor::paintStrip(juce::Graphics& g)
{
    int Y=kTopH;
    g.setColour(kStrip); g.fillRect(0,Y,W,kStripH);
    g.setColour(kCardBd); g.drawHorizontalLine(Y+kStripH,0.f,(float)W);

    // VU meters
    paintVU(g, {14.f,(float)(Y+16),16.f,88.f}, vuIn);
    paintVU(g, {(float)(W-30),(float)(Y+16),16.f,88.f}, vuOut);

    // Labels + power toggle icons
    g.setFont(juce::Font(8.f,juce::Font::bold)); g.setColour(kMuted);
    g.drawText("INPUT",  52,Y+4,58,12,juce::Justification::centred);
    g.drawText("GATE",  126,Y+4,58,12,juce::Justification::centred);

    // Gate power icon — highlights when on
    bool gateOn = tbGate.getToggleState();
    float gpx=182.f, gpy=(float)(Y+6);
    g.setColour(gateOn ? kPurple : kMuted);
    g.drawEllipse(gpx,gpy+2,10.f,10.f,1.5f);
    g.fillRect(gpx+4,gpy-1,2.f,5.f);

    g.setColour(kMuted);
    g.drawText("COMPRESSOR", W-196,Y+4,84,12,juce::Justification::centred);

    // Comp power icon — highlights when on
    bool compOn = tbComp.getToggleState();
    float cpx=(float)(W-114), cpy=(float)(Y+6);
    g.setColour(compOn ? kPurple : kMuted);
    g.drawEllipse(cpx,cpy+2,10.f,10.f,1.5f);
    g.fillRect(cpx+4,cpy-1,2.f,5.f);

    g.setColour(kMuted);
    g.drawText("OUTPUT", W-109,Y+4,58,12,juce::Justification::centred);
}

void ArcaneEclipseEditor::paintVU(juce::Graphics& g, juce::Rectangle<float> b, float lvl)
{
    g.setColour(juce::Colour(0xff0a0a14)); g.fillRoundedRectangle(b,2.f);
    int nSegs=16; float segH=b.getHeight()/nSegs;
    int lit=(int)(nSegs*juce::jlimit(0.f,1.f,lvl));
    for(int s=0;s<lit;++s){
        float sy=b.getBottom()-(s+1)*segH+1.f;
        if(s>=14)      g.setColour(juce::Colour(0xffcc2266));
        else if(s>=11) g.setColour(kPurDim);
        else           g.setColour(kPurple);
        g.fillRoundedRectangle(b.getX()+1,sy,b.getWidth()-2,segH-1.5f,1.f);
    }
    g.setColour(kCardBd); g.drawRoundedRectangle(b,2.f,.5f);
}

void ArcaneEclipseEditor::paintChain(juce::Graphics& g)
{
    // Determine active state for each of 9 nodes
    // Order: GATE(0) COMP(1) DRIVE(2) AMP(3) CAB(4) EQ(5) MOD(6) DELAY(7) REVERB(8)
    bool nodeActive[9] = {
        tbGate .getToggleState(),           // GATE
        tbComp .getToggleState(),           // COMP
        stompOD.getToggleState(),           // DRIVE
        proc.isNAMLoaded(),                 // AMP
        proc.isIRLoaded(),                  // CAB
        proc.isNAMLoaded(),                 // EQ — active whenever amp running
        stompMod.getToggleState(),          // MOD
        stompDelay.getToggleState(),        // DELAY
        stompReverb.getToggleState()        // REVERB
    };

    for (int i = 0; i < 9; ++i) {
        auto nb = chainNodeBounds(i);
        paintChainNode(g, i, nb, nodeActive[i]);
        if (i < 8) {
            auto nb2 = chainNodeBounds(i+1);
            float ax=(float)nb.getRight()+2, ay=(float)nb.getCentreY();
            float ax2=(float)nb2.getX()-2;
            // Arrow colour — purple if both connected nodes active
            g.setColour((nodeActive[i] && nodeActive[i+1])
                        ? kPurple.withAlpha(.8f) : kMuted.withAlpha(.3f));
            g.drawLine(ax,ay,ax2,ay,1.5f);
            g.drawLine(ax2-5,ay-4,ax2,ay,1.5f);
            g.drawLine(ax2-5,ay+4,ax2,ay,1.5f);
        }
    }
}

void ArcaneEclipseEditor::paintChainNode(juce::Graphics& g,int idx,
                                          juce::Rectangle<int> b,bool active)
{
    // Node box
    g.setColour(active ? kPurple.withAlpha(.2f) : kCard);
    g.fillRoundedRectangle(b.toFloat(),6.f);
    g.setColour(active ? kPurple : kCardBd);
    g.drawRoundedRectangle(b.toFloat(),6.f,active ? 2.f : 1.f);

    // Icon
    auto ib=b.toFloat().reduced(8.f,7.f);
    float cx=ib.getCentreX(), cy=ib.getCentreY();
    g.setColour(active ? kPurple : kMuted);
    juce::Path p;

    // 9 nodes: GATE(0) COMP(1) DRIVE(2) AMP(3) CAB(4) EQ(5) MOD(6) DELAY(7) REVERB(8)
    switch(idx){
        case 0: // GATE — slash line
            g.drawLine(ib.getX(),ib.getBottom(),ib.getRight(),ib.getY(),2.f); break;
        case 1: // COMP — compression curve
            p.startNewSubPath(ib.getX(),ib.getBottom()); p.lineTo(cx,cy+3);
            p.cubicTo(cx,cy+3,cx,cy-3,ib.getRight(),ib.getY());
            g.strokePath(p,juce::PathStrokeType(1.8f)); break;
        case 2: // DRIVE — pedal shape
            g.fillEllipse(ib.getX(),cy-5,7,7); g.fillEllipse(ib.getX(),cy+1,7,7);
            g.drawLine(ib.getX()+8,cy-1,ib.getRight(),cy-1,1.5f);
            g.drawLine(ib.getX()+8,cy+4,ib.getRight(),cy+4,1.5f); break;
        case 3: // AMP — amp head
            g.drawRoundedRectangle(ib,2.f,1.8f);
            g.drawLine(ib.getX(),ib.getY()+5,ib.getRight(),ib.getY()+5,1.f);
            g.fillEllipse(cx-4,cy-3,8,8); break;
        case 4: // CAB — speaker
            g.drawRoundedRectangle(ib.reduced(0,1),3.f,1.8f);
            g.drawEllipse(cx-5,cy-4,10,10,1.5f); g.drawEllipse(cx-2,cy-1,5,5,1.f); break;
        case 5: // EQ — faders
            { float pos[]={.4f,.7f,.25f};
            for(int f=0;f<3;++f){ float fx=ib.getX()+f*(ib.getWidth()/2.5f);
                g.drawLine(fx,ib.getY(),fx,ib.getBottom(),1.2f);
                g.fillEllipse(fx-2.5f,ib.getY()+pos[f]*ib.getHeight()-2.5f,5,5); } break; }
        case 6: // MOD — sine wave
            p.startNewSubPath(ib.getX(), cy);
            p.cubicTo(ib.getX()+6,cy-7, ib.getX()+12,cy+7, cx,cy);
            p.cubicTo(cx+6,cy-7, ib.getRight()-5,cy+7, ib.getRight(),cy);
            g.strokePath(p,juce::PathStrokeType(1.8f)); break;
        case 7: // DELAY — clock
            g.drawEllipse(ib.reduced(1),1.8f);
            g.drawLine(cx,cy,cx,ib.getY()+5,1.8f); g.drawLine(cx,cy,cx+5,cy+4,1.8f); break;
        case 8: // REVERB — ripples
            for(int wave=0;wave<2;++wave){ float wy=cy-2+wave*7.f; p.clear();
            p.startNewSubPath(ib.getX(),wy);
            p.cubicTo(ib.getX()+5,wy-4,ib.getX()+10,wy+4,cx,wy);
            p.cubicTo(cx+5,wy-4,ib.getRight()-5,wy+4,ib.getRight(),wy);
            g.strokePath(p,juce::PathStrokeType(1.4f-wave*.3f)); } break;
    }

    // Label
    g.setFont(juce::Font(8.f,juce::Font::bold));
    g.setColour(active ? kPurple : kMuted);
    g.drawText(kChainLabels[idx],
               juce::Rectangle<int>(b.getX()-4,b.getBottom()+2,b.getWidth()+8,12),
               juce::Justification::centred);
}

void ArcaneEclipseEditor::paintAmpHead(juce::Graphics& g)
{
    int Y=kTopH+kStripH, H2=kAmpH;
    // Chassis
    juce::ColourGradient cg(juce::Colour(0xff18182a),(float)0,(float)Y,
                             juce::Colour(0xff0d0d14),(float)0,(float)(Y+H2),false);
    g.setGradientFill(cg); g.fillRect(0,Y,W,H2);

    // Chassis border with purple glow
    g.setColour(kPurple.withAlpha(.15f));
    g.fillRect(0,Y,W,2);
    g.fillRect(0,Y+H2-2,W,2);

    // Grille area (top portion)
    int gH=H2*55/100;
    g.setColour(juce::Colour(0xff08080f));
    g.fillRect(12,Y+8,W-24,gH-8);
    g.setColour(juce::Colour(0xff111118));
    g.drawRect(12,Y+8,W-24,gH-8,1);
    // Mesh pattern
    g.setColour(juce::Colour(0xff131320));
    for(int mx=16;mx<W-16;mx+=8) g.drawVerticalLine(mx,(float)(Y+10),(float)(Y+gH-2));
    for(int my=Y+10;my<Y+gH-2;my+=7) g.drawHorizontalLine(my,16.f,(float)(W-16));

    // Handle
    juce::ColourGradient hg(juce::Colour(0xff3a3a5a),(float)(W/2),0,
                             juce::Colour(0xff1a1a2e),(float)(W/2),12,false);
    g.setGradientFill(hg);
    g.fillRoundedRectangle((float)(W/2-60),(float)(Y+4),120.f,12.f,6.f);

    // Nameplate
    int npW=280,npH=60,npX=(W-npW)/2,npY=Y+gH/2-npH/2;
    juce::ColourGradient npg(juce::Colour(0xff1e1e30),(float)npX,(float)npY,
                              juce::Colour(0xff0e0e1e),(float)npX,(float)(npY+npH),false);
    g.setGradientFill(npg); g.fillRoundedRectangle((float)npX,(float)npY,(float)npW,(float)npH,4.f);
    g.setColour(kPurple); g.drawRoundedRectangle((float)npX,(float)npY,(float)npW,(float)npH,4.f,1.5f);
    // Screw dots
    for(auto pt:{std::pair<int,int>{npX+8,npY+8},{npX+npW-8,npY+8},{npX+8,npY+npH-8},{npX+npW-8,npY+npH-8}}){
        g.setColour(kPurDim); g.fillEllipse((float)pt.first-3,(float)pt.second-3,6.f,6.f);
    }
    g.setFont(juce::Font("Georgia",28.f,juce::Font::bold)); g.setColour(kText);
    g.drawText("ARCANE", juce::Rectangle<int>(npX,npY+6,npW,28), juce::Justification::centred);
    g.setFont(juce::Font(10.f,juce::Font::bold)); g.setColour(kPurple);
    g.drawText("ECLIPSE", juce::Rectangle<int>(npX,npY+36,npW,16), juce::Justification::centred);

    // Faceplate
    int fY=Y+gH, fH=H2-gH;
    g.setColour(juce::Colour(0xff111120)); g.fillRect(0,fY,W,fH);
    g.setColour(kPurple.withAlpha(.4f)); g.fillRect(0,fY,W,2);
    // Jack
    g.setColour(juce::Colour(0xff0a0a14)); g.fillEllipse(20.f,(float)(fY+fH/2-10),20.f,20.f);
    g.setColour(kCardBd); g.drawEllipse(20.f,(float)(fY+fH/2-10),20.f,20.f,1.5f);
    g.setFont(juce::Font(7.f,juce::Font::bold)); g.setColour(kMuted);
    g.drawText("INPUT",8,fY+4,36,10,juce::Justification::centred);
    // Power LED
    g.setColour(kPurple); g.fillEllipse((float)(W-42),(float)(fY+fH/2-14),28.f,28.f);
    g.setColour(kPurple.brighter(.5f)); g.fillEllipse((float)(W-38),(float)(fY+fH/2-10),14.f,14.f);
    g.setFont(juce::Font(7.f,juce::Font::bold)); g.setColour(kMuted);
    g.drawText("POWER",(float)(W-52),(float)(fY+fH/2+16),52,10,juce::Justification::centred);
}

void ArcaneEclipseEditor::paintFXSection(juce::Graphics& g)
{
    int Y=kTopH+kStripH+kAmpH;
    g.setColour(kBg); g.fillRect(0,Y,W,kFXH);
    g.setColour(kCardBd); g.drawHorizontalLine(Y,0.f,(float)W);

    int nCards=4, cabW=320;
    int fxAreaW=W-cabW-16;
    int cardW=fxAreaW/nCards-6;

    const char* titles[]={"OVERDRIVE","MODULATION","DELAY","REVERB"};
    juce::ToggleButton* tbs[]={&stompOD,&stompMod,&stompDelay,&stompReverb};
    int cardX0=8;

    for(int i=0;i<4;++i){
        int bX=cardX0+i*(cardW+6);
        bool on=tbs[i]->getToggleState();

        // Card background
        g.setColour(kCard); g.fillRoundedRectangle((float)bX+1,(float)(Y+4),(float)(cardW-2),(float)(kFXH-8),8.f);
        g.setColour(on?kPurple.withAlpha(.5f):kCardBd);
        g.drawRoundedRectangle((float)bX+1,(float)(Y+4),(float)(cardW-2),(float)(kFXH-8),8.f,on?1.5f:1.f);

        // Title
        g.setFont(juce::Font(10.f,juce::Font::bold));
        g.setColour(on?kPurple:kMuted);
        g.drawText(titles[i], bX+10, Y+14, cardW-50, 14, juce::Justification::centredLeft);

        // Power circle icon
        float pix=(float)(bX+cardW-26), piy=(float)(Y+13);
        g.setColour(on?kPurple:kMuted);
        g.drawEllipse(pix,piy+2,12.f,12.f,1.5f);
        g.fillRect(pix+5,piy-1,2.f,6.f);
    }

    // Divider between FX and cab
    g.setColour(kCardBd);
    g.drawVerticalLine(W-cabW-8, (float)(Y+4), (float)(Y+kFXH-4));
}

void ArcaneEclipseEditor::paintCabSection(juce::Graphics& g)
{
    int fxY=kTopH+kStripH+kAmpH;
    int footY=H-kFootH;
    int cabH=footY-fxY;
    int cabW=320;
    int cX=W-cabW-4;

    // Cabinet card background
    g.setColour(kCard); g.fillRoundedRectangle((float)cX,(float)(fxY+4),316.f,(float)(cabH-8),8.f);
    g.setColour(kCardBd); g.drawRoundedRectangle((float)cX,(float)(fxY+4),316.f,(float)(cabH-8),8.f,1.f);

    // Single header — AMP & IR LOADER
    g.setColour(kCardBd); g.drawHorizontalLine(fxY+30,(float)cX,(float)(cX+316));
    g.setFont(juce::Font(11.f,juce::Font::bold));
    g.setColour(kPurple);
    g.drawText("AMP & IR LOADER", cX+2, fxY+6, 312, 20, juce::Justification::centred);

    // Cabinet photo
    int phX=cX+6, phY=fxY+36, phW=120, phH=cabH-100;
    g.setColour(juce::Colour(0xff08080f)); g.fillRoundedRectangle((float)phX,(float)phY,(float)phW,(float)phH,5.f);
    g.setColour(kCardBd); g.drawRoundedRectangle((float)phX,(float)phY,(float)phW,(float)phH,5.f,1.f);
    // Mesh
    g.setColour(juce::Colour(0xff111120));
    for(int mx=phX+4;mx<phX+phW-2;mx+=4) for(int my=phY+4;my<phY+phH-2;my+=4) g.fillEllipse((float)mx,(float)my,2.f,2.f);
    // Speaker rings
    float sx=(float)(phX+phW/2), sy=(float)(phY+phH/2);
    g.setColour(juce::Colour(0xff1a1a2e));
    for(float rr=20.f;rr<45.f;rr+=10.f) g.drawEllipse(sx-rr,sy-rr,rr*2,rr*2,2.f);
    g.fillEllipse(sx-6,sy-6,12.f,12.f);
    g.setFont(juce::Font(8.f,juce::Font::bold)); g.setColour(juce::Colour(0xff3a3a5a));
    g.drawText("ARCANE", phX, phY+phH-18, phW, 14, juce::Justification::centred);

    // Fields — no distance slider
    int rx=cX+132, ry=fxY+38;

    // MODEL section
    g.setFont(juce::Font(8.f,juce::Font::bold)); g.setColour(kMuted);
    g.drawText("MODEL", rx, ry, 50, 12, juce::Justification::centredLeft);
    g.setFont(juce::Font(10.f));
    g.setColour(proc.isNAMLoaded() ? kText : kMuted.withAlpha(.5f));
    g.drawFittedText(proc.isNAMLoaded() ? proc.getLoadedNAMName() : "No model loaded",
                     rx, ry+14, 148, 13, juce::Justification::centredLeft, 1);
    // X clear model — only shown when loaded
    if (proc.isNAMLoaded()) {
        g.setFont(juce::Font(12.f, juce::Font::bold));
        g.setColour(kMuted.brighter(.3f));
        g.drawText("x", cX+cabW-26, ry+11, 18, 18, juce::Justification::centred);
    }

    // IR section
    int irY = ry+38;
    g.setFont(juce::Font(8.f,juce::Font::bold)); g.setColour(kMuted);
    g.drawText("IR", rx, irY, 30, 12, juce::Justification::centredLeft);
    g.setFont(juce::Font(10.f));
    g.setColour(proc.isIRLoaded() ? kText : kMuted.withAlpha(.5f));
    g.drawFittedText(proc.isIRLoaded() ? proc.getLoadedIRName() : "No IR loaded",
                     rx, irY+14, 148, 13, juce::Justification::centredLeft, 1);
    // X clear IR — only shown when loaded
    if (proc.isIRLoaded()) {
        g.setFont(juce::Font(12.f, juce::Font::bold));
        g.setColour(kMuted.brighter(.3f));
        g.drawText("x", cX+cabW-26, irY+11, 18, 18, juce::Justification::centred);
    }
}

void ArcaneEclipseEditor::paintFooter(juce::Graphics& g)
{
    int Y=H-kFootH;
    juce::ColourGradient fg(juce::Colour(0xff0f0f18),0,(float)Y,kBg,0,(float)H,false);
    g.setGradientFill(fg); g.fillRect(0,Y,W,kFootH);
    g.setColour(kCardBd); g.drawHorizontalLine(Y,0.f,(float)W);

    // Headphone icon
    g.setColour(kMuted);
    juce::Path hp; hp.addCentredArc(24.f,(float)(Y+17),9.f,8.f,0.f,3.3f,6.22f,true);
    g.strokePath(hp,juce::PathStrokeType(2.f));
    g.fillEllipse(13.f,(float)(Y+21),5.f,8.f); g.fillEllipse(26.f,(float)(Y+21),5.f,8.f);
    g.setFont(juce::Font(9.f,juce::Font::bold)); g.setColour(kMuted);
    g.drawText("INPUT MONITOR",38,Y+10,105,18,juce::Justification::centredLeft);
    g.setColour(kPurple); g.fillRoundedRectangle(146.f,(float)(Y+11),30.f,15.f,3.f);
    g.setFont(juce::Font(8.f,juce::Font::bold)); g.setColour(juce::Colours::white);
    g.drawText("ON",146,Y+11,30,15,juce::Justification::centred);

    // Tabs
    g.setColour(kPurple); g.fillRoundedRectangle((float)(W/2-34),(float)(Y+7),60.f,22.f,4.f);
    g.setFont(juce::Font(10.f,juce::Font::bold)); g.setColour(juce::Colours::white);
    g.drawText("RIG",W/2-34,Y+7,60,22,juce::Justification::centred);
    g.setColour(kMuted); g.drawText("FX",W/2+34,Y+11,36,16,juce::Justification::centred);

    // Status
    auto dotDot=[&](int x,const juce::String& lbl,bool on){
        g.setColour(on?kPurple:kMuted.withAlpha(.4f));
        g.fillEllipse((float)x,(float)(Y+15),8.f,8.f);
        g.setFont(juce::Font(9.f,juce::Font::bold)); g.setColour(kMuted);
        g.drawText(lbl,x+11,Y+13,28,14,juce::Justification::centredLeft);
    };
    dotDot(W-90,"AMP",proc.isNAMLoaded());
    dotDot(W-52,"CAB",proc.isIRLoaded());
}

void ArcaneEclipseEditor::mouseDown(const juce::MouseEvent& e)
{
    for(int i=0;i<8;++i)
        if(chainNodeBounds(i).contains(e.getPosition()))
            {activeNode=i; repaint(); return;}
    AudioProcessorEditor::mouseDown(e);
}
