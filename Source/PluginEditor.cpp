#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

static const juce::Colour kPurple{0xff9B59FF};
static const juce::Colour kPurDim{0xff7040cc};
static const juce::Colour kBg    {0xff141418};
static const juce::Colour kSurf  {0xff1c1c22};
static const juce::Colour kCard  {0xff1e1e26};
static const juce::Colour kCardBd{0xff2e2e3e};
static const juce::Colour kText  {0xfff0f0ff};
static const juce::Colour kMuted {0xffbcbce2};
static const juce::Colour kGreen {0xff44cc88};
static const juce::Colour kRed   {0xffcc4444};

const juce::String ArcaneEclipseEditor::kChainLabels[9]=
    {"GATE","COMP","DRIVE","AMP","CAB","EQ","MOD","DELAY","REVERB"};

// Dark-outlined text so labels read on the busy background and dark art
static void haloText(juce::Graphics& g,const juce::String& s,juce::Font f,
                     juce::Colour col,juce::Rectangle<int> r,juce::Justification j)
{
    g.setFont(f);
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    for(int dx=-1;dx<=1;++dx)for(int dy=-1;dy<=1;++dy) if(dx||dy)
        g.drawText(s,r.translated(dx,dy),j,false);
    g.setColour(col);
    g.drawText(s,r,j,false);
}

// Bank+slot label: 0->"1A", 5->"2B", 19->"5D"
static juce::String slotCode(int idx){
    juce::juce_wchar c=(juce::juce_wchar)('A'+idx%4);
    return juce::String(idx/4+1)+juce::String::charToString(c);
}

// ── AELAF ─────────────────────────────────────────────────────────────────────
AELAF::AELAF(){
    setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff252532));
    setColour(juce::TextButton::textColourOffId,  kText);
    setColour(juce::TextButton::buttonOnColourId, kPurple);
    setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff12121a));
    setColour(juce::ComboBox::textColourId,       kText);
    setColour(juce::ComboBox::outlineColourId,    kCardBd);
    setColour(juce::ComboBox::arrowColourId,      kPurple);
    setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(0xff1a1a24));
    setColour(juce::PopupMenu::textColourId,                  kText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, kPurDim);
    setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
}

void AELAF::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,
                              float pos,float startA,float endA,juce::Slider&)
{
    // Unified knob image, rotated to the value; transparent corners let the art show
    static juce::Image src = juce::ImageCache::getFromMemory(
        BinaryData::knob_unified_png, BinaryData::knob_unified_pngSize);
    static juce::Image knob = src.rescaled(160,160,juce::Graphics::highResamplingQuality);

    auto b=juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h);
    auto c=b.getCentre();
    float sz=juce::jmin(b.getWidth(),b.getHeight());
    float ang=startA+pos*(endA-startA);
    float s = sz/(float)knob.getWidth();
    auto tr = juce::AffineTransform::scale(s)
                .translated(c.x - knob.getWidth()*s*0.5f, c.y - knob.getHeight()*s*0.5f)
                .rotated(ang, c.x, c.y);
    g.drawImageTransformed(knob, tr, false);

    // Purple glow at the pointer tip
    float pr=sz*0.5f*0.85f;
    float gx=c.x+std::sin(ang)*pr, gy=c.y-std::cos(ang)*pr;
    g.setColour(kPurple.withAlpha(0.30f)); g.fillEllipse(gx-sz*0.16f,gy-sz*0.16f,sz*0.32f,sz*0.32f);
    g.setColour(kPurple);                  g.fillEllipse(gx-sz*0.075f,gy-sz*0.075f,sz*0.15f,sz*0.15f);
    g.setColour(juce::Colour(0xffefe0ff)); g.fillEllipse(gx-sz*0.035f,gy-sz*0.035f,sz*0.07f,sz*0.07f);
}
void AELAF::drawLabel(juce::Graphics& g,juce::Label& l){
    auto txt=l.getText(); auto f=l.getFont(); auto j=l.getJustificationType();
    auto r=l.getLocalBounds();
    g.setFont(f);
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    for(int dx=-1;dx<=1;++dx)for(int dy=-1;dy<=1;++dy) if(dx||dy)
        g.drawText(txt,r.translated(dx,dy),j,false);
    g.setColour(l.findColour(juce::Label::textColourId));
    g.drawText(txt,r,j,false);
}
void AELAF::drawButtonBackground(juce::Graphics& g,juce::Button& btn,
                                  const juce::Colour&,bool,bool isDown){
    auto b=btn.getLocalBounds().toFloat().reduced(.5f);
    bool on=btn.getToggleState();
    g.setColour(isDown?kPurple.withAlpha(.35f):(on?kPurple.withAlpha(.22f):
                btn.findColour(juce::TextButton::buttonColourId)));
    g.fillRoundedRectangle(b,6.f);
    g.setColour(on||isDown?kPurple:kCardBd);
    g.drawRoundedRectangle(b,6.f,on?1.8f:1.f);
}
void AELAF::drawComboBox(juce::Graphics& g,int w,int h,bool,
                          int,int,int,int,juce::ComboBox&){
    g.setColour(juce::Colour(0xff12121a));
    g.fillRoundedRectangle(0,0,(float)w,(float)h,4.f);
    g.setColour(kCardBd);
    g.drawRoundedRectangle(.5f,.5f,(float)w-1,(float)h-1,4.f,1.f);
    juce::Path arr; float ax=(float)w-13,ay=(float)h*.5f;
    arr.addTriangle(ax,ay-3,ax+7,ay-3,ax+3.5f,ay+3);
    g.setColour(kPurple); g.fillPath(arr);
}
void AELAF::positionComboBoxText(juce::ComboBox& cb,juce::Label& l){
    l.setBounds(8,1,cb.getWidth()-22,cb.getHeight()-2);
    l.setFont(juce::Font(10.f));
}
void AELAF::drawPopupMenuItem(juce::Graphics& g,const juce::Rectangle<int>& area,
    bool,bool,bool hi,bool,bool,const juce::String& text,
    const juce::String&,const juce::Drawable*,const juce::Colour*){
    if(hi){g.setColour(kPurDim.withAlpha(.6f));g.fillRect(area);}
    g.setColour(hi?juce::Colours::white:kText);
    g.setFont(juce::Font(11.f));
    g.drawText(text,area.reduced(8,0),juce::Justification::centredLeft);
}


// ── CreditsPanel ─────────────────────────────────────────────────────────────
static const char* kCreditsText =
    "ARCANE ECLIPSE v1.0.0\n"
    "Developed by [Your Name]\n"
    "Philippines\n\n"
    "Built with open-source components:\n\n"
    "JUCE Framework\n"
    "  Raw Material Software Limited\n"
    "  juce.com/legal/juce-8-licence\n\n"
    "NeuralAudio\n"
    "  Copyright (c) 2024 Mike Oliphant\n"
    "  MIT License\n\n"
    "Neural Amp Modeler Core\n"
    "  Copyright (c) 2023 Steven Atkinson\n"
    "  MIT License\n\n"
    "RTNeural\n"
    "  Copyright (c) 2020 jatinchowdhury18\n"
    "  BSD 3-Clause License\n\n"
    "math_approx\n"
    "  Copyright (c) 2024 jatinchowdhury18\n"
    "  BSD 3-Clause License\n\n"
    "Eigen\n"
    "  Eigen Contributors\n"
    "  Mozilla Public License 2.0\n\n"
    "nlohmann/json\n"
    "  Copyright (c) 2013-2025 Niels Lohmann\n"
    "  MIT License\n\n"
    "VST is a registered trademark of\n"
    "Steinberg Media Technologies GmbH.\n"
    "VST3 SDK used under MIT License.\n\n"
    "Full license texts bundled in\n"
    "THIRD-PARTY-LICENSES.txt\n\n"
    "Thank you for supporting indie\n"
    "plugin development!\n\n"
    "(Click anywhere to close)";

CreditsPanel::CreditsPanel()
{
    setOpaque(false);
    closeBtn.onClick=[this]{ setVisible(false); };
    addAndMakeVisible(closeBtn);
}
void CreditsPanel::paint(juce::Graphics& g)
{
    auto b=getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xf0101018)); g.fillRoundedRectangle(b,12.f);
    g.setColour(kPurple); g.drawRoundedRectangle(b.reduced(0.5f),12.f,1.8f);
    g.setColour(kPurple.withAlpha(0.15f)); g.fillRoundedRectangle(b.withHeight(46),12.f);
    g.setFont(juce::Font(15.f).boldened()); g.setColour(kText);
    g.drawText("CREDITS & LICENSES",getLocalBounds().withHeight(46),juce::Justification::centred);
    g.setColour(kCardBd); g.drawHorizontalLine(46,16.f,(float)getWidth()-16);
    juce::Rectangle<int> textArea(20,54,getWidth()-40,getHeight()-100);
    g.setFont(juce::Font(10.5f)); g.setColour(juce::Colour(0xffcccce0));
    juce::AttributedString as; as.setWordWrap(juce::AttributedString::byWord);
    as.setJustification(juce::Justification::topLeft);
    as.setColour(juce::Colour(0xffcccce0)); as.setFont(juce::Font(10.5f));
    as.append(juce::String(kCreditsText));
    juce::TextLayout tl; tl.createLayout(as,(float)textArea.getWidth());
    tl.draw(g,textArea.toFloat());
    closeBtn.setBounds(getWidth()/2-40,getHeight()-38,80,26);
}

// ── AEKnob ────────────────────────────────────────────────────────────────────
void AEKnob::setup(juce::Component* p,juce::AudioProcessorValueTreeState& ap,
                    const juce::String& id,const juce::String& nm,AELAF* laf){
    paramID=id;
    slider.setLookAndFeel(laf);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,
                               juce::MathConstants<float>::pi*2.75f, true);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour(juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::trackColourId, juce::Colours::transparentBlack);
    slider.setOpaque(false);
    p->addAndMakeVisible(slider);
    att=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(ap,id,slider);
    nameLabel.setText(nm,juce::dontSendNotification);
    nameLabel.setJustificationType(juce::Justification::centred);
    nameLabel.setFont(juce::Font(8.5f).boldened());
    nameLabel.setColour(juce::Label::textColourId,juce::Colours::white);
    nameLabel.setColour(juce::Label::backgroundColourId,juce::Colours::transparentBlack);
    nameLabel.setInterceptsMouseClicks(false,false);
    nameLabel.setOpaque(false);
    p->addAndMakeVisible(nameLabel);
    valLabel.setJustificationType(juce::Justification::centred);
    valLabel.setFont(juce::Font(9.f));
    valLabel.setColour(juce::Label::textColourId,juce::Colours::white);
    valLabel.setColour(juce::Label::backgroundColourId,juce::Colours::transparentBlack);
    valLabel.setInterceptsMouseClicks(false,false);
    valLabel.setOpaque(false);
    p->addAndMakeVisible(valLabel);
    slider.onValueChange=[this]{
        valLabel.setText(juce::String(slider.getValue(),1),juce::dontSendNotification);
    };
    slider.onValueChange();
}
void AEKnob::place(int cx,int cy,int sz,bool showVal){
    slider.setBounds(cx-sz/2, cy-sz/2, sz, sz);
    nameLabel.setBounds(cx-40, cy+sz/2+6,  80, 13);
    valLabel .setBounds(cx-32, cy+sz/2+18, 64, 12);
    valLabel.setVisible(showVal);
}

// ── Constructor ───────────────────────────────────────────────────────────────
ArcaneEclipseEditor::ArcaneEclipseEditor(ArcaneEclipseProcessor& p)
    :AudioProcessorEditor(&p),proc(p)
{
    setLookAndFeel(&laf);
    setSize(W,H);

    kInput .setup(this,p.apvts,ArcaneEclipseProcessor::idInputGain, "INPUT", &laf);
    kGate  .setup(this,p.apvts,ArcaneEclipseProcessor::idNoiseGate, "GATE",  &laf);
    kComp  .setup(this,p.apvts,ArcaneEclipseProcessor::idCompThresh,"COMP",  &laf);
    kOutput.setup(this,p.apvts,ArcaneEclipseProcessor::idOutputGain,"OUTPUT",&laf);
    kGain    .setup(this,p.apvts,ArcaneEclipseProcessor::idAmpGain,    "GAIN",    &laf);
    kBass    .setup(this,p.apvts,ArcaneEclipseProcessor::idAmpBass,    "BASS",    &laf);
    kMid     .setup(this,p.apvts,ArcaneEclipseProcessor::idAmpMid,     "MID",     &laf);
    kTreble  .setup(this,p.apvts,ArcaneEclipseProcessor::idAmpTreble,  "TREBLE",  &laf);
    kPresence.setup(this,p.apvts,ArcaneEclipseProcessor::idAmpPresence,"PRESENCE",&laf);
    kMaster  .setup(this,p.apvts,ArcaneEclipseProcessor::idAmpMaster,  "MASTER",  &laf);
    kODDrive  .setup(this,p.apvts,ArcaneEclipseProcessor::idODDrive,      "DRIVE",   &laf);
    kODTone   .setup(this,p.apvts,ArcaneEclipseProcessor::idODTone,       "TONE",    &laf);
    kODLevel  .setup(this,p.apvts,ArcaneEclipseProcessor::idODLevel,      "LEVEL",   &laf);
    kModRate  .setup(this,p.apvts,ArcaneEclipseProcessor::idModRate,      "RATE",    &laf);
    kModDepth .setup(this,p.apvts,ArcaneEclipseProcessor::idModDepth,     "DEPTH",   &laf);
    kModMix   .setup(this,p.apvts,ArcaneEclipseProcessor::idModMix,       "MIX",     &laf);
    kDTime    .setup(this,p.apvts,ArcaneEclipseProcessor::idDelayTime,    "TIME",    &laf);
    kDFeedback.setup(this,p.apvts,ArcaneEclipseProcessor::idDelayFeedback,"FEEDBACK",&laf);
    kDMix     .setup(this,p.apvts,ArcaneEclipseProcessor::idDelayMix,     "MIX",     &laf);
    kRDecay   .setup(this,p.apvts,ArcaneEclipseProcessor::idReverbDecay,  "DECAY",   &laf);
    kRSize    .setup(this,p.apvts,ArcaneEclipseProcessor::idReverbSize,   "SIZE",    &laf);
    kRMix     .setup(this,p.apvts,ArcaneEclipseProcessor::idReverbMix,    "MIX",     &laf);

    allKnobs = { &kInput,&kGate,&kComp,&kOutput,
        &kGain,&kBass,&kMid,&kTreble,&kPresence,&kMaster,
        &kODDrive,&kODTone,&kODLevel,&kModRate,&kModDepth,&kModMix,
        &kDTime,&kDFeedback,&kDMix,&kRDecay,&kRSize,&kRMix };
    for (auto* k : allKnobs) k->slider.addMouseListener(this, false);
    for (int i=0;i<4;++i) sceneBtn[i].addMouseListener(this, false);

    nodeLearns = {
        {&tbGate,      ArcaneEclipseProcessor::idGateOn,   0},
        {&tbComp,      ArcaneEclipseProcessor::idCompOn,   1},
        {&stompOD,     ArcaneEclipseProcessor::idODOn,     2},
        {&stompMod,    ArcaneEclipseProcessor::idModOn,    6},
        {&stompDelay,  ArcaneEclipseProcessor::idDelayOn,  7},
        {&stompReverb, ArcaneEclipseProcessor::idReverbOn, 8},
    };
    for (auto& nl : nodeLearns) nl.comp->addMouseListener(this, false);

    for(auto* t:{&tbGate,&tbComp,&stompOD,&stompMod,&stompDelay,&stompReverb,&tbCab})
        addAndMakeVisible(*t);
    attGate  =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idGateOn,   tbGate);
    attComp  =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idCompOn,   tbComp);
    attOD    =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idODOn,     stompOD);
    attMod   =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idModOn,   stompMod);
    attDelay =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idDelayOn, stompDelay);
    attReverb=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idReverbOn,stompReverb);
    attCab   =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idCabBypass,tbCab);

    comboMod.addItem("Analog Chorus",1); comboMod.addItem("Flanger",2); comboMod.addItem("Tremolo",3);
    comboMod.setSelectedId(1); addAndMakeVisible(comboMod);
    attModType=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts,ArcaneEclipseProcessor::idModType,comboMod);
    comboDly.addItem("1/4",1); comboDly.addItem("1/4D",2); comboDly.addItem("1/8",3); comboDly.addItem("1/2",4);
    comboDly.setSelectedId(1); addAndMakeVisible(comboDly);
    attDlyType=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts,ArcaneEclipseProcessor::idDelayType,comboDly);
    comboRvb.addItem("Plate",1); comboRvb.addItem("Hall",2); comboRvb.addItem("Room",3); comboRvb.addItem("Spring",4);
    comboRvb.setSelectedId(1); addAndMakeVisible(comboRvb);
    attRvbType=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts,ArcaneEclipseProcessor::idReverbType,comboRvb);

    addAndMakeVisible(btnLoadModel);
    btnLoadModel.onClick=[this]{
        chooserModel=std::make_unique<juce::FileChooser>("Load NAM Model",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.nam");
        chooserModel->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc){auto r=fc.getResults();if(!r.isEmpty()){proc.loadNAMModel(r[0]);curNAMPath=r[0].getFullPathName();repaint();}});
    };
    addAndMakeVisible(btnLoadIR);
    btnLoadIR.onClick=[this]{
        chooserIR=std::make_unique<juce::FileChooser>("Load IR",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.wav");
        chooserIR->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc){auto r=fc.getResults();if(!r.isEmpty()){proc.loadIR(r[0]);curIRPath=r[0].getFullPathName();repaint();}});
    };
    btnClearModel.setColour(juce::TextButton::buttonColourId,juce::Colours::transparentBlack);
    btnClearModel.setColour(juce::TextButton::textColourOffId,kMuted);
    btnClearModel.onClick=[this]{proc.unloadNAMModel();curNAMPath.clear();repaint();};
    addAndMakeVisible(btnClearModel);
    btnClearIR.setColour(juce::TextButton::buttonColourId,juce::Colours::transparentBlack);
    btnClearIR.setColour(juce::TextButton::textColourOffId,kMuted);
    btnClearIR.onClick=[this]{proc.unloadIR();curIRPath.clear();repaint();};
    addAndMakeVisible(btnClearIR);

    // Scene slots 1-4
    for(int i=0;i<4;++i){
        sceneBtn[i].setButtonText(juce::String(i+1));
        sceneBtn[i].setClickingTogglesState(false);
        addAndMakeVisible(sceneBtn[i]);
        sceneBtn[i].onClick=[this,i]{
            int idx=currentBank*4+i;
            if(juce::ModifierKeys::getCurrentModifiers().isShiftDown()) saveScene(idx);
            else                                                        loadScene(idx);
            refreshSceneButtons(); repaint();
        };
    }
    bankPrev.setButtonText("< BANK"); bankNext.setButtonText("BANK >");
    addAndMakeVisible(bankPrev); addAndMakeVisible(bankNext);
    bankPrev.onClick=[this]{ currentBank=(currentBank+4)%5; refreshSceneButtons(); repaint(); };
    bankNext.onClick=[this]{ currentBank=(currentBank+1)%5; refreshSceneButtons(); repaint(); };

    // Header preset nav (invisible over painted arrows) + save
    presetPrev.setClickingTogglesState(false); presetNext.setClickingTogglesState(false);
    addAndMakeVisible(presetPrev); addAndMakeVisible(presetNext);
    auto step=[this](int d){
        int n=activeScene<0?0:activeScene; n=(n+d+20)%20; loadScene(n);
        currentBank=n/4; refreshSceneButtons(); repaint();
    };
    presetPrev.onClick=[step]{ step(-1); };
    presetNext.onClick=[step]{ step(+1); };
    headerSave.setColour(juce::TextButton::buttonColourId,kPurple);
    headerSave.setColour(juce::TextButton::textColourOffId,juce::Colours::white);
    addAndMakeVisible(headerSave);
    headerSave.onClick=[this]{
        juce::PopupMenu m;
        m.addSectionHeader("Save current settings to:");
        for (int bank=0; bank<5; ++bank) {
            juce::PopupMenu bm;
            for (int slot=0; slot<4; ++slot) {
                int idx = bank*4+slot;
                juce::String lbl = slotCode(idx);
                if (!scenes[idx].isEmpty()) lbl += "  (" + scenes[idx].name + ")";
                bm.addItem(idx+1, lbl);
            }
            m.addSubMenu("Bank " + juce::String(bank+1), bm);
        }
        m.addSeparator();
        bool haveActive = (activeScene>=0 && !scenes[activeScene].isEmpty());
        m.addItem(1001, "Rename current preset...", haveActive);
        m.addItem(1002, "Delete current preset",    haveActive);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&headerSave),
            [this](int r){
                if (r>=1 && r<=20) { saveScene(r-1); currentBank=(r-1)/4; refreshSceneButtons(); repaint(); }
                else if (r==1001) { renameScene(activeScene); }
                else if (r==1002) { deleteScene(activeScene); }
            });
    };

    // ? icon -> credits panel
    addAndMakeVisible(creditsPanel);
    creditsPanel.setVisible(false);
    // Invisible clickable region over the ? icon
    helpBtn=std::make_unique<juce::TextButton>();
    helpBtn->setButtonText("");
    helpBtn->setColour(juce::TextButton::buttonColourId,juce::Colours::transparentBlack);
    helpBtn->setColour(juce::TextButton::buttonOnColourId,juce::Colours::transparentBlack);
    helpBtn->onClick=[this]{ showCredits(); };
    addAndMakeVisible(*helpBtn);
    helpBtn->setBounds(W-78,10,30,30);

    // Tuner toggle sits on the header tuning-fork icon
    tbTuner.setClickingTogglesState(true);
    tbTuner.onClick=[this]{ setTunerVisible(tbTuner.getToggleState()); };
    addAndMakeVisible(tbTuner);

    refreshSceneButtons();
    startTimerHz(15);

    // License check — show activation dialog if not licensed
    if (!AELicenseManager::getInstance().loadFromDisk())
    {
        activationDialog = std::make_unique<AEActivationDialog>();
        activationDialog->setBounds(0, 0, W, H);
        activationDialog->onActivated = [this]{
            activationDialog->setVisible(false);
            activationDialog.reset();
            repaint();
        };
        addAndMakeVisible(*activationDialog);
        activationDialog->toFront(true);
    }
}

ArcaneEclipseEditor::~ArcaneEclipseEditor(){stopTimer();setLookAndFeel(nullptr);}
void ArcaneEclipseEditor::timerCallback()
{
    learningID = proc.midiLearningParamID();
    if (tunerVisible) {
        float hz = proc.tunerFreq.load();
        if (hz > 20.f) {
            double midi = 69.0 + 12.0 * std::log2((double) hz / 440.0);
            int nearest = (int) std::lround(midi);
            tunerCents = (float) ((midi - nearest) * 100.0);
            static const char* nm[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
            int nn = ((nearest % 12) + 12) % 12, oct = nearest / 12 - 1;
            tunerNote = juce::String(nm[nn]) + juce::String(oct);
            tunerHz = hz;
        } else { tunerHz = 0.f; tunerNote = {}; tunerCents = 0.f; }
    }
    vuIn *= .92f; vuOut *= .92f; repaint();
}

void ArcaneEclipseEditor::setTunerVisible(bool v)
{
    tunerVisible=v;
    proc.tunerActive.store(v);
    if(!v){ proc.tunerFreq.store(0.f); tunerHz=0.f; tunerNote={}; tunerCents=0.f; }
    tbTuner.setToggleState(v,juce::dontSendNotification);
    for(auto* c:getChildren())
        if(c!=&tbTuner && c!=&creditsPanel)
            c->setVisible(!v);
    if(v) creditsPanel.setVisible(false);
    repaint();
}
void ArcaneEclipseEditor::mouseDown(const juce::MouseEvent& e)
{
    if(tunerVisible){
        auto pos=e.getEventRelativeTo(this).getPosition();
        if(juce::Rectangle<int>(W-46,12,30,30).contains(pos)) setTunerVisible(false);
        return;
    }
    if(!e.mods.isPopupMenu()) return;
    // Scene slot context menu (save / load / rename / delete)
    for(int i=0;i<4;++i) if(e.eventComponent==&sceneBtn[i]){
        int idx=currentBank*4+i; bool empty=scenes[idx].isEmpty();
        juce::PopupMenu m;
        m.addItem(1,"Save here");
        m.addItem(2,"Load",!empty);
        m.addItem(3,"Rename...",!empty);
        m.addItem(4,"Delete",!empty);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&sceneBtn[i]),
            [this,idx](int r){
                if(r==1){ saveScene(idx); refreshSceneButtons(); repaint(); }
                else if(r==2){ loadScene(idx); refreshSceneButtons(); repaint(); }
                else if(r==3){ renameScene(idx); }
                else if(r==4){ deleteScene(idx); }
            });
        return;
    }
    // Chain-node footswitch MIDI learn
    for(auto& nl:nodeLearns) if(e.eventComponent==nl.comp){
        juce::String pid=nl.pid; int cc=proc.ccForParam(pid);
        juce::PopupMenu m;
        m.addItem(1, cc<0 ? "MIDI Learn (footswitch)" : "MIDI Learn (re-assign)");
        if(cc>=0) m.addItem(2, "Clear MIDI (CC "+juce::String(cc)+")");
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(nl.comp),
            [this,pid](int r){
                if(r==1){ proc.midiLearnStart(pid); learningID=pid; }
                else if(r==2){ proc.midiLearnClear(pid); }
                repaint();
            });
        return;
    }
    AEKnob* hit=nullptr;
    for(auto* k:allKnobs) if(&k->slider==e.eventComponent){ hit=k; break; }
    if(hit==nullptr) return;
    juce::String pid=hit->paramID;
    int cc=proc.ccForParam(pid);
    juce::PopupMenu m;
    m.addItem(1, cc<0 ? "MIDI Learn" : "MIDI Learn (re-assign)");
    if(cc>=0) m.addItem(2, "Clear MIDI (CC "+juce::String(cc)+")");
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&hit->slider),
        [this,pid](int r){
            if(r==1){ proc.midiLearnStart(pid); learningID=pid; }
            else if(r==2){ proc.midiLearnClear(pid); }
            repaint();
        });
}

void ArcaneEclipseEditor::refreshSceneButtons(){
    for(int i=0;i<4;++i){
        int idx=currentBank*4+i;
        sceneBtn[i].setButtonText(slotCode(idx));
        sceneBtn[i].setToggleState(activeScene==idx,juce::dontSendNotification);
    }
}
void ArcaneEclipseEditor::showCredits()
{
    int pw=440, ph=540;
    creditsPanel.setBounds((W-pw)/2,(H-ph)/2,pw,ph);
    creditsPanel.setVisible(true);
    creditsPanel.toFront(false);
}
void ArcaneEclipseEditor::deleteScene(int slot){
    scenes[slot]=SceneData();
    if(activeScene==slot) activeScene=-1;
    refreshSceneButtons(); repaint();
}
void ArcaneEclipseEditor::renameScene(int slot){
    if(scenes[slot].isEmpty()) return;
    renameWindow=std::make_unique<juce::AlertWindow>("Rename Preset",
        "New name for slot "+slotCode(slot)+":", juce::MessageBoxIconType::NoIcon);
    renameWindow->addTextEditor("nm", scenes[slot].name);
    renameWindow->addButton("OK",1,juce::KeyPress(juce::KeyPress::returnKey));
    renameWindow->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    renameWindow->enterModalState(true,
        juce::ModalCallbackFunction::create([this,slot](int r){
            if(r==1 && renameWindow!=nullptr){
                auto n=renameWindow->getTextEditorContents("nm").trim();
                if(n.isNotEmpty() && n!="Empty") scenes[slot].name=n;
            }
            renameWindow.reset();
            refreshSceneButtons(); repaint();
        }), false);
}

// ── Scene save/load ───────────────────────────────────────────────────────────
void ArcaneEclipseEditor::saveScene(int slot){
    scenes[slot].namPath = curNAMPath;
    scenes[slot].irPath  = curIRPath;
    scenes[slot].params  = proc.apvts.copyState();
    if(scenes[slot].name=="Empty") scenes[slot].name = slotCode(slot);
    activeScene=slot;
}
void ArcaneEclipseEditor::loadScene(int slot){
    if(scenes[slot].isEmpty()) return;
    if(scenes[slot].params.isValid())
        proc.apvts.replaceState(scenes[slot].params);
    // Recall the scene's NAM model (reload only when it changes)
    if(scenes[slot].namPath != curNAMPath){
        if(scenes[slot].namPath.isNotEmpty() && juce::File(scenes[slot].namPath).existsAsFile())
            proc.loadNAMModel(juce::File(scenes[slot].namPath));
        else
            proc.unloadNAMModel();
        curNAMPath = scenes[slot].namPath;
    }
    // Recall the scene's IR (reload only when it changes)
    if(scenes[slot].irPath != curIRPath){
        if(scenes[slot].irPath.isNotEmpty() && juce::File(scenes[slot].irPath).existsAsFile())
            proc.loadIR(juce::File(scenes[slot].irPath));
        else
            proc.unloadIR();
        curIRPath = scenes[slot].irPath;
    }
    activeScene=slot;
}

// ── Layout ────────────────────────────────────────────────────────────────────
juce::Rectangle<int> ArcaneEclipseEditor::chainNodeBounds(int i) const
{
    int nodeW=64,nodeH=54;
    int totalW=9*nodeW+8*6;
    int startX=(W-totalW)/2;
    int x=startX+i*(nodeW+6);
    int y=kTopH+(kStripH-nodeH)/2;
    return{x,y,nodeW,nodeH};
}

void ArcaneEclipseEditor::resized()
{
    int stripY=kTopH, ampY=kTopH+kStripH, fxY=ampY+kAmpH, sceneY=fxY+kFXH;

    // Strip knobs
    int sSz=52, sCy=stripY+kStripH/2;
    kInput .place(66,   sCy,sSz);
    kGate  .place(138,  sCy,sSz);
    kComp  .place(W-138,sCy,sSz);
    kOutput.place(W-66, sCy,sSz);

    // Chain node toggles (invisible, over painted nodes)
    for(int i=0;i<9;++i){
        auto nb=chainNodeBounds(i);
        if(i==0) tbGate.setBounds(nb); else if(i==1) tbComp.setBounds(nb);
        else if(i==2) stompOD.setBounds(nb); else if(i==6) stompMod.setBounds(nb);
        else if(i==7) stompDelay.setBounds(nb); else if(i==8) stompReverb.setBounds(nb);
    }

    // Amp knobs on the brushed faceplate (below the purple divider ~0.571)
    int aSz=72;
    int faceTop=ampY+(int)(kAmpH*0.571f);
    int aCy=faceTop+(int)((fxY-faceTop)*0.42f);
    for(int i=0;i<6;++i){
        AEKnob* ks[]={&kGain,&kBass,&kMid,&kTreble,&kPresence,&kMaster};
        ks[i]->place(150+i*180,aCy,aSz,true);
    }

    // FX cards
    int cardW=196, step=208, fxSz=48;
    int topCy=fxY+(int)(kFXH*0.28f), botCy=fxY+(int)(kFXH*0.61f);
    int comboY=fxY+(int)(kFXH*0.83f);
    int odX=10, modX=10+step, dlX=10+2*step, rvX=10+3*step;
    kODDrive.place(odX+(int)(cardW*0.28f),topCy,fxSz,true);
    kODTone .place(odX+(int)(cardW*0.72f),topCy,fxSz,true);
    kODLevel.place(odX+cardW/2,           botCy,fxSz,true);
    kModRate .place(modX+(int)(cardW*0.28f),topCy,fxSz,true);
    kModDepth.place(modX+(int)(cardW*0.72f),topCy,fxSz,true);
    kModMix  .place(modX+cardW/2,           botCy,fxSz,true);
    comboMod.setBounds(modX+10,comboY,cardW-20,24);
    kDTime    .place(dlX+(int)(cardW*0.28f),topCy,fxSz,true);
    kDFeedback.place(dlX+(int)(cardW*0.72f),topCy,fxSz,true);
    kDMix     .place(dlX+cardW/2,           botCy,fxSz,true);
    comboDly.setBounds(dlX+10,comboY,cardW-20,24);
    kRDecay.place(rvX+(int)(cardW*0.28f),topCy,fxSz,true);
    kRSize .place(rvX+(int)(cardW*0.72f),topCy,fxSz,true);
    kRMix  .place(rvX+cardW/2,           botCy,fxSz,true);
    comboRvb.setBounds(rvX+10,comboY,cardW-20,24);

    // Cabinet loader buttons (mapped to the art's recessed slots)
    int cabX=844; auto px=[&](float v){return cabX+(int)(v*kCabW/477.f);};
    auto py=[&](float v){return fxY+(int)(v*kFXH/355.f);};
    btnLoadModel.setBounds(px(31),py(282),px(231)-px(31),py(331)-py(282));
    btnLoadIR   .setBounds(px(245),py(282),px(445)-px(245),py(332)-py(282));
    btnClearModel.setBounds(px(445)-20,(py(74)+py(121))/2-8,16,16);
    btnClearIR   .setBounds(px(445)-20,(py(134)+py(175))/2-8,16,16);

    // Scene bar: 4 slots + 2 bank buttons
    int bankW=96,gap=10;
    int slotW=(W-2*bankW-2*16-5*gap)/4;
    int sy=sceneY+8, sh=kSceneH-16;
    bankPrev.setBounds(16,sy,bankW,sh);
    for(int i=0;i<4;++i)
        sceneBtn[i].setBounds(16+bankW+gap+i*(slotW+gap),sy,slotW,sh);
    bankNext.setBounds(W-16-bankW,sy,bankW,sh);

    // Header preset nav + save + tuner icon
    presetPrev.setBounds(436,10,28,34);
    presetNext.setBounds(696,10,28,34);
    headerSave.setBounds(742,10,64,30);
    tbTuner.setBounds(W-150,10,30,30);

    tbCab.setBounds(-200,-200,1,1);
    creditsPanel.setBounds((W-440)/2,(H-540)/2,440,540);
}

// ── paint ─────────────────────────────────────────────────────────────────────
void ArcaneEclipseEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBg);
    if(tunerVisible){ paintTuner(g); return; }
    static juce::Image bg = juce::ImageCache::getFromMemory(BinaryData::background_png,BinaryData::background_pngSize);
    if(bg.isValid()){
        float sc=juce::jmax((float)W/bg.getWidth(),(float)H/bg.getHeight());
        int dw=(int)(bg.getWidth()*sc), dh=(int)(bg.getHeight()*sc);
        g.drawImage(bg,(W-dw)/2,(H-dh)/2,dw,dh,0,0,bg.getWidth(),bg.getHeight());
    }
    paintTopBar(g); paintStrip(g); paintChain(g); paintAmpHead(g);
    paintFXSection(g); paintCabSection(g); paintSceneBar(g); paintFooter(g);
}
void ArcaneEclipseEditor::paintOverChildren(juce::Graphics& g)
{
    if(tunerVisible || learningID.isEmpty()) return;
    float pulse=(float)std::sin(juce::Time::getMillisecondCounter()*0.006)*0.5f+0.5f;
    for(auto* k:allKnobs) if(k->paramID==learningID && k->slider.isVisible()){
        auto b=k->slider.getBounds().toFloat().expanded(3.f);
        g.setColour(kPurple.withAlpha(0.35f+0.55f*pulse));
        g.drawRoundedRectangle(b,b.getWidth()*0.5f,2.5f);
        haloText(g,"LEARN",juce::Font(8.f).boldened(),kPurple,
                 {k->slider.getX()-12,k->slider.getY()-13,k->slider.getWidth()+24,12},juce::Justification::centred);
    }
    for(auto& nl:nodeLearns) if(nl.pid==learningID){
        auto nb=chainNodeBounds(nl.nodeIdx);
        g.setColour(kPurple.withAlpha(0.35f+0.55f*pulse));
        g.drawRoundedRectangle(nb.toFloat().expanded(2.f),8.f,2.5f);
        haloText(g,"LEARN",juce::Font(8.f).boldened(),kPurple,
                 {nb.getX()-8,nb.getY()-13,nb.getWidth()+16,12},juce::Justification::centred);
    }
}

void ArcaneEclipseEditor::paintTopBar(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff0c0c13)); g.fillRect(0,0,W,kTopH);
    g.setColour(kCardBd); g.drawHorizontalLine(kTopH,0.f,(float)W);
    // logo
    g.setColour(kPurple);
    float lx=26.f,ly=27.f,R=13.f;
    for(int i=0;i<8;++i){float a=i*juce::MathConstants<float>::pi/4.f;float rr=(i%2)?R*0.5f:R;
        g.drawLine(lx,ly,lx+std::cos(a)*rr,ly+std::sin(a)*rr,(i%2)?1.f:1.6f);}
    g.fillEllipse(lx-3,ly-3,6,6);
    g.setColour(kPurple.withAlpha(0.4f)); g.drawEllipse(lx-R,ly-R,2*R,2*R,1.f);
    // brand
    haloText(g,"ARCANE",juce::Font(19.f).boldened(),kText,{46,10,110,30},juce::Justification::centredLeft);
    haloText(g,"ECLIPSE",juce::Font(12.f).boldened(),kPurple,{150,12,80,26},juce::Justification::centredLeft);
    haloText(g,"v1.0.0",juce::Font(8.5f),kMuted,{228,14,60,24},juce::Justification::centredLeft);
    // preset box
    int pbx=430,pbw=300;
    g.setColour(juce::Colour(0xff0c0c14)); g.fillRoundedRectangle((float)pbx,8.f,(float)pbw,38.f,6.f);
    g.setColour(kPurple); g.drawRoundedRectangle(pbx+0.5f,8.5f,pbw-1.f,37.f,6.f,1.4f);
    g.setColour(kPurple);
    {juce::Path a;a.startNewSubPath(pbx+20.f,20.f);a.lineTo(pbx+13.f,25.f);a.lineTo(pbx+20.f,30.f);g.strokePath(a,juce::PathStrokeType(2.f));}
    {juce::Path a;a.startNewSubPath(pbx+pbw-20.f,20.f);a.lineTo(pbx+pbw-13.f,25.f);a.lineTo(pbx+pbw-20.f,30.f);g.strokePath(a,juce::PathStrokeType(2.f));}
    juce::String pn = (activeScene>=0 && !scenes[activeScene].isEmpty())?scenes[activeScene].name:"No Preset";
    haloText(g,pn,juce::Font(12.f).boldened(),kText,{pbx+30,12,pbw-60,20},juce::Justification::centred);
    for(int i=0;i<5;++i){ g.setColour(i==currentBank?kPurple:juce::Colour(0x66bcbce2));
        g.fillEllipse(pbx+pbw/2-24+i*12-2.f,37.f,4.f,4.f);}
    // icons
    int ico[4]={W-150,W-114,W-78,W-42};
    for(int k=0;k<4;++k){g.setColour(juce::Colour(0xff212130));g.fillRoundedRectangle((float)ico[k],10.f,30.f,30.f,5.f);
        g.setColour(kCardBd);g.drawRoundedRectangle(ico[k]+0.5f,10.5f,29.f,29.f,5.f,1.f);}
    {float cx=ico[0]+15.f; g.setColour(kMuted);
     g.drawLine(cx-4,17,cx-4,27,1.6f); g.drawLine(cx+4,17,cx+4,27,1.6f);
     juce::Path u;u.startNewSubPath(cx-4,27);u.quadraticTo(cx-4,31,cx,31);u.quadraticTo(cx+4,31,cx+4,27);
     g.strokePath(u,juce::PathStrokeType(1.6f)); g.drawLine(cx,31,cx,34,1.6f);}
    g.setColour(kMuted);
    g.drawRect(juce::Rectangle<float>((float)(ico[1]+7),20.f,7.f,10.f),1.4f);
    g.drawRect(juce::Rectangle<float>((float)(ico[1]+16),20.f,7.f,10.f),1.4f);
    haloText(g,"?",juce::Font(14.f).boldened(),kMuted,{ico[2],10,30,30},juce::Justification::centred);
    g.setColour(kPurple); g.drawEllipse(ico[3]+10.f,19.f,10.f,10.f,1.4f); g.drawLine(ico[3]+15.f,17.f,ico[3]+15.f,24.f,1.4f);
}

void ArcaneEclipseEditor::paintStrip(juce::Graphics& g)
{
    int stripY=kTopH;
    g.setColour(kCardBd); g.drawHorizontalLine(stripY+kStripH,0.f,(float)W);
    int sCy=stripY+kStripH/2;
    paintVU(g,{14.f,(float)(sCy-45),14.f,90.f},vuIn);
    paintVU(g,{(float)(W-28),(float)(sCy-45),14.f,90.f},vuOut);
}
void ArcaneEclipseEditor::paintVU(juce::Graphics& g,juce::Rectangle<float> b,float lvl)
{
    g.setColour(juce::Colour(0xff08080e)); g.fillRoundedRectangle(b,2.f);
    int n=16,lit=(int)std::round(n*juce::jlimit(0.f,1.f,lvl));
    float sh=b.getHeight()/n;
    for(int i=0;i<lit;++i){
        float sy=b.getBottom()-(i+1)*sh+1;
        g.setColour(i>=14?kRed:(i>=11?kPurDim:kPurple));
        g.fillRect(b.getX()+1,sy,b.getWidth()-2,sh-1.5f);
    }
    g.setColour(kCardBd.withAlpha(0.6f)); g.drawRoundedRectangle(b,2.f,1.f);
}

void ArcaneEclipseEditor::paintChain(juce::Graphics& g)
{
    bool act[9]={ tbGate.getToggleState(), tbComp.getToggleState(), stompOD.getToggleState(),
                  proc.isNAMLoaded(), proc.isIRLoaded(), true,
                  stompMod.getToggleState(), stompDelay.getToggleState(), stompReverb.getToggleState() };
    for(int i=0;i<9;++i){
        auto nb=chainNodeBounds(i);
        paintChainNode(g,i,nb,act[i]);
        haloText(g,kChainLabels[i],juce::Font(7.f).boldened(),juce::Colours::white,
                 {nb.getX()-3,nb.getBottom()+2,nb.getWidth()+6,10},juce::Justification::centred);
        if(i<8){
            auto nn=chainNodeBounds(i+1); bool glow=act[i]&&act[i+1];
            float ay=(float)nb.getCentreY(), ax=nb.getRight()+1.f, ax2=nn.getX()-1.f;
            g.setColour((glow?kPurple:kMuted).withAlpha(glow?0.85f:0.3f));
            g.drawLine(ax,ay,ax2,ay,1.4f);
            g.drawLine(ax2-4,ay-3,ax2,ay,1.4f); g.drawLine(ax2-4,ay+3,ax2,ay,1.4f);
        }
    }
}
void ArcaneEclipseEditor::paintChainNode(juce::Graphics& g,int idx,juce::Rectangle<int> nb,bool on)
{
    auto r=nb.toFloat();
    g.setColour(on?kPurple.withAlpha(0.16f):juce::Colour(0xb814141e));
    g.fillRoundedRectangle(r,7.f);
    g.setColour(on?kPurple:kCardBd); g.drawRoundedRectangle(r,7.f,on?1.6f:1.f);
    float cx=r.getCentreX(), cy=r.getCentreY()-2.f, R=11.f;
    g.setColour(on?kPurple:kMuted);
    switch(idx){
      case 0: g.drawLine(cx-R,cy+R*0.7f,cx+R,cy-R*0.7f,2.f); break;
      case 1:{juce::Path p;p.startNewSubPath(cx-R,cy+7);p.quadraticTo(cx,cy+7,cx,cy);p.quadraticTo(cx,cy-7,cx+R,cy-7);g.strokePath(p,juce::PathStrokeType(1.8f));}break;
      case 2: g.fillEllipse(cx-R+2-3,cy-4-3,6,6); g.fillEllipse(cx-R+2-3,cy+4-3,6,6);
              g.drawLine(cx-R+7,cy-4,cx+R,cy-4,1.6f); g.drawLine(cx-R+7,cy+4,cx+R,cy+4,1.6f); break;
      case 3: g.drawRoundedRectangle(cx-R,cy-8,2*R,16,2.f,1.6f); g.fillEllipse(cx-4,cy-4,8,8); break;
      case 4: g.drawRoundedRectangle(cx-R,cy-8,2*R,16,3.f,1.6f); g.drawEllipse(cx-5,cy-5,10,10,1.5f); g.fillEllipse(cx-2,cy-2,4,4); break;
      case 5:{float pk[3]={0.35f,0.65f,0.28f};for(int f=0;f<3;++f){float fx=cx-R+f*R;g.drawLine(fx,cy-8,fx,cy+8,1.3f);g.fillEllipse(fx-2.4f,cy-8+pk[f]*16-2.4f,4.8f,4.8f);} }break;
      case 6:{juce::Path p;p.startNewSubPath(cx-R,cy);p.quadraticTo(cx-R/2,cy-8,cx,cy);p.quadraticTo(cx+R/2,cy+8,cx+R,cy);g.strokePath(p,juce::PathStrokeType(1.8f));}break;
      case 7: g.drawEllipse(cx-(R-1),cy-(R-1),2*(R-1),2*(R-1),1.7f); g.drawLine(cx,cy,cx,cy-6,1.7f); g.drawLine(cx,cy,cx+5,cy+3,1.7f); break;
      case 8:{for(int w=0;w<3;++w){float wy=cy-6+w*6;juce::Path p;p.startNewSubPath(cx-R,wy);p.quadraticTo(cx-R/2,wy-4,cx,wy);p.quadraticTo(cx+R/2,wy+4,cx+R,wy);g.strokePath(p,juce::PathStrokeType(1.4f));}}break;
    }
}

void ArcaneEclipseEditor::paintAmpHead(juce::Graphics& g)
{
    static juce::Image amp = juce::ImageCache::getFromMemory(BinaryData::amp_png,BinaryData::amp_pngSize);
    int ampY=kTopH+kStripH;
    if(amp.isValid())
        g.drawImage(amp,10,ampY,W-20,kAmpH,0,0,amp.getWidth(),amp.getHeight());
}

void ArcaneEclipseEditor::paintFXSection(juce::Graphics& g)
{
    static juce::Image card = juce::ImageCache::getFromMemory(BinaryData::fxcard_png,BinaryData::fxcard_pngSize);
    int fxY=kTopH+kStripH+kAmpH, cardW=196, stepp=208;
    const char* titles[4]={"OVERDRIVE","MODULATION","DELAY","REVERB"};
    bool on[4]={ stompOD.getToggleState(), stompMod.getToggleState(),
                 stompDelay.getToggleState(), stompReverb.getToggleState() };
    for(int i=0;i<4;++i){
        int x=10+i*stepp;
        if(card.isValid())
            g.drawImage(card,x,fxY,cardW,kFXH,0,0,card.getWidth(),card.getHeight());
        haloText(g,titles[i],juce::Font(10.f).boldened(),juce::Colours::white,
                 {x,fxY+14,cardW,16},juce::Justification::centred);
        // power state dot — cover the baked art dot, then draw on/off state
        float dx=x+cardW*0.907f, dy=fxY+kFXH*0.068f;
        g.setColour(juce::Colour(0xff191922)); g.fillEllipse(dx-12,dy-12,24,24);
        if(on[i]){
            g.setColour(kPurple.withAlpha(0.35f)); g.fillEllipse(dx-12,dy-12,24,24);
            g.setColour(kPurple);                  g.fillEllipse(dx-7,dy-7,14,14);
            g.setColour(juce::Colour(0xffe6d2ff)); g.fillEllipse(dx-3,dy-3,6,6);
        } else {
            g.setColour(juce::Colour(0xff45455c)); g.drawEllipse(dx-6,dy-6,12,12,1.4f);
        }
    }
}

void ArcaneEclipseEditor::paintCabSection(juce::Graphics& g)
{
    static juce::Image cab = juce::ImageCache::getFromMemory(BinaryData::cabinet_png,BinaryData::cabinet_pngSize);
    int fxY=kTopH+kStripH+kAmpH, cabX=844;
    if(cab.isValid())
        g.drawImage(cab,cabX,fxY,kCabW,kFXH,0,0,cab.getWidth(),cab.getHeight());
    auto px=[&](float v){return cabX+(int)(v*kCabW/477.f);};
    auto py=[&](float v){return fxY+(int)(v*kFXH/355.f);};
    {int y0=py(74),y1=py(121);
     haloText(g,"MODEL",juce::Font(8.f).boldened(),kMuted,{px(258),y0+(y1-y0)/4-6,120,12},juce::Justification::centredLeft);
     juce::String v=proc.isNAMLoaded()?proc.getLoadedNAMName():juce::String("No model loaded");
     haloText(g,v,juce::Font(9.f),juce::Colour(0xffd4d4ee),{px(258),y0+(y1-y0)/2-2,px(430)-px(258),14},juce::Justification::centredLeft);}
    {int y0=py(134),y1=py(175);
     haloText(g,"IR",juce::Font(8.f).boldened(),kMuted,{px(258),y0+(y1-y0)/4-6,120,12},juce::Justification::centredLeft);
     juce::String v=proc.isIRLoaded()?proc.getLoadedIRName():juce::String("No IR loaded");
     haloText(g,v,juce::Font(9.f),juce::Colour(0xffd4d4ee),{px(258),y0+(y1-y0)/2-2,px(430)-px(258),14},juce::Justification::centredLeft);}
    // arcane emblem in slot 3
    {float ex=(float)px(345), ey=(float)py(214), R=8.f;
     g.setColour(kPurDim.withAlpha(0.7f));
     for(int k=0;k<3;++k){g.drawLine((float)px(258)+k*14.f,ey,(float)px(258)+k*14.f+8.f,ey,1.4f);
        g.drawLine((float)px(432)-k*14.f-8.f,ey,(float)px(432)-k*14.f,ey,1.4f);}
     g.setColour(kPurple.withAlpha(0.9f)); g.drawEllipse(ex-R,ey-R,2*R,2*R,1.3f);
     g.setColour(kPurDim.withAlpha(0.5f)); g.drawEllipse(ex-R-3.5f,ey-R-3.5f,2*(R+3.5f),2*(R+3.5f),0.8f);
     juce::Path st;st.startNewSubPath(ex,ey-R-2);st.lineTo(ex+2,ey);st.lineTo(ex,ey+R+2);st.lineTo(ex-2,ey);st.closeSubPath();
     g.setColour(kPurple); g.fillPath(st);
     juce::Path st2;st2.startNewSubPath(ex-R-2,ey);st2.lineTo(ex,ey-2);st2.lineTo(ex+R+2,ey);st2.lineTo(ex,ey+2);st2.closeSubPath();
     g.setColour(juce::Colour(0xffc9a3ff)); g.fillPath(st2);
     g.setColour(juce::Colour(0xffefe0ff)); g.fillEllipse(ex-1.7f,ey-1.7f,3.4f,3.4f);}
}

void ArcaneEclipseEditor::paintSceneBar(juce::Graphics& g){ juce::ignoreUnused(g); }

void ArcaneEclipseEditor::paintFooter(juce::Graphics& g)
{
    int Y=kTopH+kStripH+kAmpH+kFXH+kSceneH;
    juce::ColourGradient fg(juce::Colour(0xff0c0c12),0,(float)Y,juce::Colour(0xff08080e),0,(float)H,false);
    g.setGradientFill(fg); g.fillRect(0,Y,W,kFootH);
    g.setColour(kCardBd); g.drawHorizontalLine(Y,0.f,(float)W);
    g.setColour(kMuted); juce::Path hp;
    hp.addCentredArc(24.f,(float)(Y+17),8.f,7.f,0.f,3.3f,6.22f,true);
    g.strokePath(hp,juce::PathStrokeType(2.f));
    g.fillEllipse(14.f,(float)(Y+20),5.f,8.f); g.fillEllipse(27.f,(float)(Y+20),5.f,8.f);
    g.setFont(juce::Font(8.f).boldened()); g.setColour(kMuted);
    g.drawText("INPUT MONITOR",36,Y+9,105,16,juce::Justification::centredLeft);
    g.setColour(kPurple); g.fillRoundedRectangle(142.f,(float)(Y+10),28.f,14.f,3.f);
    g.setFont(juce::Font(7.f).boldened()); g.setColour(juce::Colours::white);
    g.drawText("ON",142,Y+10,28,14,juce::Justification::centred);
    g.setColour(kPurple); g.fillRoundedRectangle((float)(W/2-30),(float)(Y+6),58.f,22.f,4.f);
    g.setFont(juce::Font(9.f).boldened()); g.setColour(juce::Colours::white);
    g.drawText("RIG",W/2-30,Y+6,58,22,juce::Justification::centred);
    g.setColour(kMuted); g.drawText("FX",W/2+36,Y+9,24,16,juce::Justification::centred);
    g.setColour(proc.isNAMLoaded()?kPurple:kMuted.withAlpha(.3f));
    g.fillEllipse((float)(W-80),(float)(Y+13),8.f,8.f);
    g.setFont(juce::Font(8.f)); g.setColour(kMuted);
    g.drawText("AMP",W-70,Y+10,30,14,juce::Justification::centredLeft);
    g.setColour(proc.isIRLoaded()?kPurple:kMuted.withAlpha(.3f));
    g.fillEllipse((float)(W-34),(float)(Y+13),8.f,8.f);
    g.drawText("CAB",W-24,Y+10,26,14,juce::Justification::centredLeft);
}

void ArcaneEclipseEditor::paintTuner(juce::Graphics& g)
{
    // Background (same as main panel) + dark overlay for readability
    static juce::Image bg = juce::ImageCache::getFromMemory(BinaryData::background_png,BinaryData::background_pngSize);
    if(bg.isValid()){
        float sc=juce::jmax((float)W/bg.getWidth(),(float)H/bg.getHeight());
        int dw=(int)(bg.getWidth()*sc), dh=(int)(bg.getHeight()*sc);
        g.drawImage(bg,(W-dw)/2,(H-dh)/2,dw,dh,0,0,bg.getWidth(),bg.getHeight());
    } else g.fillAll(kBg);
    g.setColour(juce::Colour(0xc60a0a12)); g.fillRect(0,0,W,H);
    g.setColour(juce::Colour(0xff0c0c13)); g.fillRect(0,0,W,kTopH);
    g.setColour(kPurple.withAlpha(.6f)); g.fillRect(0,kTopH-2,W,2);
    g.setFont(juce::Font(16.f).boldened()); g.setColour(kText);
    g.drawText("CHROMATIC TUNER",0,0,W,kTopH,juce::Justification::centred);
    // X close button (top-right)
    juce::Rectangle<float> xr((float)(W-46),12.f,30.f,30.f);
    g.setColour(juce::Colour(0xff212130)); g.fillRoundedRectangle(xr,5.f);
    g.setColour(kPurple); g.drawRoundedRectangle(xr.reduced(0.5f),5.f,1.3f);
    g.setColour(kText);
    float xcx=xr.getCentreX(), xcy=xr.getCentreY();
    g.drawLine(xcx-6,xcy-6,xcx+6,xcy+6,2.f); g.drawLine(xcx-6,xcy+6,xcx+6,xcy-6,2.f);
    int cx=W/2,cy=H/2-20;
    g.setColour(kCard); g.fillEllipse((float)(cx-180),(float)(cy-180),360.f,360.f);
    g.setColour(kCardBd); g.drawEllipse((float)(cx-180),(float)(cy-180),360.f,360.f,2.f);
    bool inTune=std::fabs(tunerCents)<3.f && tunerHz>0;
    g.setColour(inTune?kGreen.withAlpha(.2f):kCard);
    g.fillEllipse((float)(cx-40),(float)(cy-40),80.f,80.f);
    if(tunerHz>0){
        float angle=juce::MathConstants<float>::pi*(tunerCents/60.f);
        float nx=cx+160.f*std::sin(angle),ny=cy-160.f*std::cos(angle);
        g.setColour(inTune?kGreen:kPurple); g.drawLine((float)cx,(float)cy,nx,ny,3.f);
    }
    g.setFont(juce::Font(juce::FontOptions().withName("Georgia").withHeight(72.f).withStyle("Bold")));
    g.setColour(inTune?kGreen:kText);
    g.drawText(tunerHz>0?tunerNote:"--",cx-80,cy-50,160,100,juce::Justification::centred);
    g.setFont(juce::Font(14.f)); g.setColour(kMuted);
    if(tunerHz>0) g.drawText(juce::String(tunerCents,1)+" cents",cx-80,cy+50,160,24,juce::Justification::centred);
    for(int c=-6;c<=6;++c){
        float a=juce::MathConstants<float>::pi*(c/10.f);
        float r1=150.f,r2=c==0?165.f:158.f;
        float x1=cx+r1*std::sin(a),y1=cy-r1*std::cos(a);
        float x2=cx+r2*std::sin(a),y2=cy-r2*std::cos(a);
        g.setColour(c==0?kPurple:kCardBd); g.drawLine(x1,y1,x2,y2,c==0?2.f:1.f);
    }
    g.setFont(juce::Font(11.f)); g.setColour(kMuted);
    g.drawText(tunerHz>0?juce::String(tunerHz,1)+" Hz":"---",cx-80,cy+78,160,20,juce::Justification::centred);
    g.setColour(kCardBd); g.drawHorizontalLine(H-kFootH,0.f,(float)W);
    g.setFont(juce::Font(9.f)); g.setColour(kMuted);
    g.drawText("Click the X (top-right) or the fork icon to close",0,H-kFootH,W,kFootH,juce::Justification::centred);
}
