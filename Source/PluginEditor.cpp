#include "PluginEditor.h"
#include <BinaryData.h>

static const juce::Colour kPurple{0xff9B59FF};
static const juce::Colour kPurDim{0xff7040cc};
static const juce::Colour kBg    {0xff141418};
static const juce::Colour kSurf  {0xff1c1c22};
static const juce::Colour kCard  {0xff1e1e26};
static const juce::Colour kCardBd{0xff2e2e3e};
static const juce::Colour kText  {0xfff0f0ff};
static const juce::Colour kMuted {0xff7878aa};
static const juce::Colour kGreen {0xff44cc88};
static const juce::Colour kRed   {0xffcc4444};

const juce::String ArcaneEclipseEditor::kChainLabels[9]=
    {"GATE","COMP","DRIVE","AMP","CAB","EQ","MOD","DELAY","REVERB"};

// ── AELAF ─────────────────────────────────────────────────────────────────────
AELAF::AELAF(){
    setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff252532));
    setColour(juce::TextButton::textColourOffId,  kText);
    setColour(juce::TextButton::buttonOnColourId, kPurple);
    setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1a1a24));
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
    // Fill component background with card colour to hide JUCE default grey
    g.setColour(kCard);
    g.fillRect(x, y, w, h);

    auto b=juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h).reduced(4.f);
    auto c=b.getCentre(); float r=juce::jmin(b.getWidth(),b.getHeight())*.5f;
    // Track
    juce::Path track;
    track.addCentredArc(c.x,c.y,r*.88f,r*.88f,0,startA,endA,true);
    g.setColour(juce::Colour(0xff252535));
    g.strokePath(track,juce::PathStrokeType(3.5f,juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
    // Arc
    if(pos>0.005f){
        juce::Path arc;
        arc.addCentredArc(c.x,c.y,r*.88f,r*.88f,0,startA,startA+(endA-startA)*pos,true);
        g.setColour(kPurple);
        g.strokePath(arc,juce::PathStrokeType(3.5f,juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
    }
    // Body
    juce::ColourGradient kg(juce::Colour(0xff2a2a38),c.x-r*.3f,c.y-r*.3f,
                             juce::Colour(0xff0e0e16),c.x+r*.4f,c.y+r*.4f,true);
    g.setGradientFill(kg); g.fillEllipse(c.x-r*.72f,c.y-r*.72f,r*1.44f,r*1.44f);
    g.setColour(kCardBd); g.drawEllipse(c.x-r*.72f,c.y-r*.72f,r*1.44f,r*1.44f,1.f);
    // Pointer
    float ang=startA+pos*(endA-startA);
    float pr=r*.56f;
    float px=c.x+pr*std::sin(ang),py=c.y-pr*std::cos(ang);
    g.setColour(kPurple); g.fillEllipse(px-2.5f,py-2.5f,5.f,5.f);
    g.drawLine(c.x,c.y,px,py,2.f);
}
void AELAF::drawLabel(juce::Graphics& g,juce::Label& l){
    g.setColour(l.findColour(juce::Label::textColourId));
    g.setFont(l.getFont());
    g.drawFittedText(l.getText(),l.getLocalBounds(),l.getJustificationType(),1);
}
void AELAF::drawButtonBackground(juce::Graphics& g,juce::Button& btn,
                                  const juce::Colour&,bool,bool isDown){
    auto b=btn.getLocalBounds().toFloat().reduced(.5f);
    bool on=btn.getToggleState();
    g.setColour(isDown?kPurple.withAlpha(.3f):(on?kPurple.withAlpha(.18f):
                btn.findColour(juce::TextButton::buttonColourId)));
    g.fillRoundedRectangle(b,5.f);
    g.setColour(on||isDown?kPurple:kCardBd);
    g.drawRoundedRectangle(b,5.f,1.f);
}
void AELAF::drawComboBox(juce::Graphics& g,int w,int h,bool,
                          int,int,int,int,juce::ComboBox& cb){
    g.setColour(juce::Colour(0xff1a1a24));
    g.fillRoundedRectangle(0,0,(float)w,(float)h,4.f);
    g.setColour(kCardBd);
    g.drawRoundedRectangle(.5f,.5f,(float)w-1,(float)h-1,4.f,1.f);
    juce::Path arr; float ax=(float)w-13,ay=(float)h*.5f;
    arr.addTriangle(ax,ay-3,ax+7,ay-3,ax+3.5f,ay+3);
    g.setColour(kPurple); g.fillPath(arr);
}
void AELAF::positionComboBoxText(juce::ComboBox& cb,juce::Label& l){
    l.setBounds(6,1,cb.getWidth()-20,cb.getHeight()-2);
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

// ── AEKnob ────────────────────────────────────────────────────────────────────
void AEKnob::setup(juce::Component* p,juce::AudioProcessorValueTreeState& ap,
                    const juce::String& id,const juce::String& nm,AELAF* laf){
    slider.setLookAndFeel(laf);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour(juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::trackColourId, juce::Colours::transparentBlack);
    slider.setOpaque(false);
    p->addAndMakeVisible(slider);
    att=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(ap,id,slider);
    nameLabel.setText(nm,juce::dontSendNotification);
    nameLabel.setJustificationType(juce::Justification::centred);
    nameLabel.setFont(juce::Font(8.f,juce::Font::bold));
    nameLabel.setColour(juce::Label::textColourId,kMuted);
    nameLabel.setColour(juce::Label::backgroundColourId,juce::Colours::transparentBlack);
    nameLabel.setOpaque(false);
    p->addAndMakeVisible(nameLabel);
    valLabel.setJustificationType(juce::Justification::centred);
    valLabel.setFont(juce::Font(8.f));
    valLabel.setColour(juce::Label::textColourId,kPurple);
    valLabel.setColour(juce::Label::backgroundColourId,juce::Colours::transparentBlack);
    valLabel.setOpaque(false);
    p->addAndMakeVisible(valLabel);
    slider.onValueChange=[this]{
        valLabel.setText(juce::String(slider.getValue(),1),juce::dontSendNotification);
    };
    slider.onValueChange();
}
void AEKnob::place(int cx,int cy,int sz,bool showVal){
    // Slider bounds exactly match knob visual size — prevents background bleeding
    slider.setBounds(cx-sz/2, cy-sz/2, sz, sz);
    nameLabel.setBounds(cx-32, cy+sz/2+2,  64, 12);
    valLabel .setBounds(cx-24, cy+sz/2+14, 48, 11);
    valLabel.setVisible(showVal);
}

// ── Constructor ───────────────────────────────────────────────────────────────
ArcaneEclipseEditor::ArcaneEclipseEditor(ArcaneEclipseProcessor& p)
    :AudioProcessorEditor(&p),proc(p)
{
    setLookAndFeel(&laf);
    setSize(W,H);

    // Strip
    kInput .setup(this,p.apvts,ArcaneEclipseProcessor::idInputGain, "INPUT", &laf);
    kGate  .setup(this,p.apvts,ArcaneEclipseProcessor::idNoiseGate, "GATE",  &laf);
    kComp  .setup(this,p.apvts,ArcaneEclipseProcessor::idCompThresh,"COMP",  &laf);
    kOutput.setup(this,p.apvts,ArcaneEclipseProcessor::idOutputGain,"OUTPUT",&laf);
    // Amp
    kGain    .setup(this,p.apvts,ArcaneEclipseProcessor::idAmpGain,    "GAIN",    &laf);
    kBass    .setup(this,p.apvts,ArcaneEclipseProcessor::idAmpBass,    "BASS",    &laf);
    kMid     .setup(this,p.apvts,ArcaneEclipseProcessor::idAmpMid,     "MID",     &laf);
    kTreble  .setup(this,p.apvts,ArcaneEclipseProcessor::idAmpTreble,  "TREBLE",  &laf);
    kPresence.setup(this,p.apvts,ArcaneEclipseProcessor::idAmpPresence,"PRESENCE",&laf);
    kMaster  .setup(this,p.apvts,ArcaneEclipseProcessor::idAmpMaster,  "MASTER",  &laf);
    // FX
    kODDrive  .setup(this,p.apvts,ArcaneEclipseProcessor::idODDrive,        "DRIVE",   &laf);
    kODTone   .setup(this,p.apvts,ArcaneEclipseProcessor::idODTone,         "TONE",    &laf);
    kODLevel  .setup(this,p.apvts,ArcaneEclipseProcessor::idODLevel,        "LEVEL",   &laf);
    kModRate  .setup(this,p.apvts,ArcaneEclipseProcessor::idModRate,        "RATE",    &laf);
    kModDepth .setup(this,p.apvts,ArcaneEclipseProcessor::idModDepth,       "DEPTH",   &laf);
    kModMix   .setup(this,p.apvts,ArcaneEclipseProcessor::idModMix,         "MIX",     &laf);
    kDTime    .setup(this,p.apvts,ArcaneEclipseProcessor::idDelayTime,      "TIME",    &laf);
    kDFeedback.setup(this,p.apvts,ArcaneEclipseProcessor::idDelayFeedback,  "FEEDBACK",&laf);
    kDMix     .setup(this,p.apvts,ArcaneEclipseProcessor::idDelayMix,       "MIX",     &laf);
    kRDecay   .setup(this,p.apvts,ArcaneEclipseProcessor::idReverbDecay,    "DECAY",   &laf);
    kRSize    .setup(this,p.apvts,ArcaneEclipseProcessor::idReverbSize,     "SIZE",    &laf);
    kRMix     .setup(this,p.apvts,ArcaneEclipseProcessor::idReverbMix,      "MIX",     &laf);

    // Toggles — invisible, sit on chain nodes
    for(auto* t:{&tbGate,&tbComp,&stompOD,&stompMod,&stompDelay,&stompReverb,&tbCab})
        addAndMakeVisible(*t);
    attGate  =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idGateOn,   tbGate);
    attComp  =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idCompOn,   tbComp);
    attOD    =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idODOn,     stompOD);
    attMod   =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idModOn,   stompMod);
    attDelay =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idDelayOn, stompDelay);
    attReverb=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idReverbOn,stompReverb);
    attCab   =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idCabBypass,tbCab);

    // Dropdowns
    comboMod.addItem("Analog Chorus",1); comboMod.addItem("Flanger",2); comboMod.addItem("Tremolo",3);
    comboMod.setSelectedId(1); addAndMakeVisible(comboMod);
    attModType=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts,ArcaneEclipseProcessor::idModType,comboMod);
    comboDly.addItem("1/4",1); comboDly.addItem("1/4D",2); comboDly.addItem("1/8",3); comboDly.addItem("1/2",4);
    comboDly.setSelectedId(1); addAndMakeVisible(comboDly);
    attDlyType=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts,ArcaneEclipseProcessor::idDelayType,comboDly);
    comboRvb.addItem("Plate",1); comboRvb.addItem("Hall",2); comboRvb.addItem("Room",3); comboRvb.addItem("Spring",4);
    comboRvb.setSelectedId(1); addAndMakeVisible(comboRvb);
    attRvbType=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts,ArcaneEclipseProcessor::idReverbType,comboRvb);

    // Load buttons
    addAndMakeVisible(btnLoadModel);
    btnLoadModel.onClick=[this]{
        chooserModel=std::make_unique<juce::FileChooser>("Load NAM Model",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.nam");
        chooserModel->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc){auto r=fc.getResults();if(!r.isEmpty()){proc.loadNAMModel(r[0]);repaint();}});
    };
    addAndMakeVisible(btnLoadIR);
    btnLoadIR.onClick=[this]{
        chooserIR=std::make_unique<juce::FileChooser>("Load IR",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.wav");
        chooserIR->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc){auto r=fc.getResults();if(!r.isEmpty()){proc.loadIR(r[0]);repaint();}});
    };
    btnClearModel.setColour(juce::TextButton::buttonColourId,juce::Colours::transparentBlack);
    btnClearModel.setColour(juce::TextButton::textColourOffId,kMuted);
    btnClearModel.onClick=[this]{proc.unloadNAMModel();repaint();};
    addAndMakeVisible(btnClearModel);
    btnClearIR.setColour(juce::TextButton::buttonColourId,juce::Colours::transparentBlack);
    btnClearIR.setColour(juce::TextButton::textColourOffId,kMuted);
    btnClearIR.onClick=[this]{proc.unloadIR();repaint();};
    addAndMakeVisible(btnClearIR);

    // Scene buttons
    for(int i=0;i<5;++i){
        sceneBtn[i].setButtonText("SCENE "+juce::String(i+1));
        sceneBtn[i].setClickingTogglesState(false);
        addAndMakeVisible(sceneBtn[i]);
        sceneBtn[i].onClick=[this,i]{
            if(juce::ModifierKeys::getCurrentModifiers().isShiftDown())
                saveScene(i);
            else
                loadScene(i);
            repaint();
        };
    }

    // Tuner button
    btnTuner.setClickingTogglesState(true);
    btnTuner.onClick=[this]{tunerVisible=btnTuner.getToggleState();repaint();};
    addAndMakeVisible(btnTuner);

    startTimerHz(15);
}

ArcaneEclipseEditor::~ArcaneEclipseEditor(){stopTimer();setLookAndFeel(nullptr);}
void ArcaneEclipseEditor::timerCallback(){vuIn*=.92f;vuOut*=.92f;repaint();}

// ── Scene save/load ───────────────────────────────────────────────────────────
void ArcaneEclipseEditor::saveScene(int slot){
    scenes[slot].namPath = proc.isNAMLoaded()?proc.getLoadedNAMName():"";
    scenes[slot].irPath  = proc.isIRLoaded() ?proc.getLoadedIRName() :"";
    scenes[slot].params  = proc.apvts.copyState();
    scenes[slot].name    = "Scene "+juce::String(slot+1);
    sceneBtn[slot].setButtonText(scenes[slot].name.substring(0,8));
    activeScene=slot;
}
void ArcaneEclipseEditor::loadScene(int slot){
    if(scenes[slot].isEmpty()) return;
    if(scenes[slot].params.isValid())
        proc.apvts.replaceState(scenes[slot].params);
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
    int stripY=kTopH, ampY=kTopH+kStripH, fxY=ampY+kAmpH;
    int sceneY=fxY+kFXH, footY=sceneY+kSceneH;

    // Strip knobs
    int kSz=56;
    kInput .place(66, stripY+59,kSz);
    kGate  .place(138,stripY+59,kSz);
    kComp  .place(W-138,stripY+59,kSz);
    kOutput.place(W-66, stripY+59,kSz);

    // Chain node toggles — invisible buttons on nodes
    for(int i=0;i<9;++i){
        auto nb=chainNodeBounds(i);
        if(i==0) tbGate .setBounds(nb);
        else if(i==1) tbComp .setBounds(nb);
        else if(i==2) stompOD.setBounds(nb);
        else if(i==6) stompMod.setBounds(nb);
        else if(i==7) stompDelay.setBounds(nb);
        else if(i==8) stompReverb.setBounds(nb);
    }

    // Amp knobs
    int aSz=72, aCy=ampY+kAmpH*3/4;
    int aSpan=W-240; int aStep=aSpan/6;
    for(int i=0;i<6;++i){
        AEKnob* ks[]={&kGain,&kBass,&kMid,&kTreble,&kPresence,&kMaster};
        ks[i]->place(120+aStep/2+i*aStep,aCy,aSz,true);
    }

    // FX cards — 4 cards + cab panel
    int nCards=4, cabW=kCabW;
    int fxArea=W-cabW-8;
    int cardW=fxArea/nCards-5;
    int fxKSz=46, fxTopCy=fxY+64, fxBotCy=fxY+128;

    // OD
    int odX=4;
    kODDrive.place(odX+cardW/4,  fxTopCy,fxKSz,false);
    kODTone .place(odX+cardW*3/4,fxTopCy,fxKSz,false);
    kODLevel.place(odX+cardW/2,  fxBotCy,fxKSz,false);
    // MOD
    int modX=odX+cardW+5;
    kModRate .place(modX+cardW/4,  fxTopCy,fxKSz,false);
    kModDepth.place(modX+cardW*3/4,fxTopCy,fxKSz,false);
    kModMix  .place(modX+cardW/2,  fxBotCy,fxKSz,false);
    comboMod.setBounds(modX+4,fxY+kFXH-28,cardW-8,22);
    // DELAY
    int dlX=modX+cardW+5;
    kDTime    .place(dlX+cardW/4,  fxTopCy,fxKSz,false);
    kDFeedback.place(dlX+cardW*3/4,fxTopCy,fxKSz,false);
    kDMix     .place(dlX+cardW/2,  fxBotCy,fxKSz,false);
    comboDly.setBounds(dlX+4,fxY+kFXH-28,cardW-8,22);
    // REVERB
    int rvX=dlX+cardW+5;
    kRDecay.place(rvX+cardW/4,  fxTopCy,fxKSz,false);
    kRSize .place(rvX+cardW*3/4,fxTopCy,fxKSz,false);
    kRMix  .place(rvX+cardW/2,  fxBotCy,fxKSz,false);
    comboRvb.setBounds(rvX+4,fxY+kFXH-28,cardW-8,22);

    // Cabinet
    int cX=W-cabW;
    int cabH=kFXH;
    btnLoadModel.setBounds(cX+8,  fxY+cabH-60,(cabW-20)/2,26);
    btnLoadIR   .setBounds(cX+8+(cabW-20)/2+4,fxY+cabH-60,(cabW-20)/2,26);
    btnClearModel.setBounds(cX+cabW-22,fxY+82,16,16);
    btnClearIR   .setBounds(cX+cabW-22,fxY+116,16,16);

    // Scene bar — 5 equally spaced footswitches
    int sW=(W-16)/5-4;
    for(int i=0;i<5;++i)
        sceneBtn[i].setBounds(8+i*(sW+4),sceneY+6,sW,kSceneH-12);

    // Tuner button in footer
    btnTuner.setBounds(W/2+62, footY+7, 80, 22);

    // Hidden
    tbCab.setBounds(-200,-200,1,1);
}

// ── paint ─────────────────────────────────────────────────────────────────────
void ArcaneEclipseEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBg);
    if(tunerVisible){ paintTuner(g); return; }
    paintTopBar(g);
    paintStrip(g);
    paintChain(g);
    paintAmpHead(g);
    paintFXSection(g);
    paintCabSection(g);
    paintSceneBar(g);
    paintFooter(g);
}

void ArcaneEclipseEditor::paintOverChildren(juce::Graphics& g)
{
    if(tunerVisible) return;
    int Y=kTopH+kStripH+kAmpH;
    int nCards=4, cabW=kCabW;
    int fxArea=W-cabW-8;
    int cardW=fxArea/nCards-5;
    int cardX0=4;
    int fxKSz=46;
    int fxTopCy=Y+64;
    juce::ToggleButton* tbs[]={&stompOD,&stompMod,&stompDelay,&stompReverb};
    const char* titles[]={"OVERDRIVE","MODULATION","DELAY","REVERB"};

    for(int i=0;i<4;++i){
        int bX=cardX0+i*(cardW+5);
        bool on=tbs[i]->getToggleState();
        juce::Colour fillCol=on?juce::Colour(0xff1e1e2c):kCard;

        // Paint gap strip between the two top-row knobs
        int k1cx=bX+cardW/4, k2cx=bX+cardW*3/4;
        int gapLeft  = k1cx + fxKSz/2 + 1;
        int gapRight = k2cx - fxKSz/2 - 1;
        // Cover full slider area height (slider bounds = sz+28 for name+val labels)
        int gapTop   = fxTopCy - fxKSz/2 - 2;
        int gapH     = fxKSz + 4; // full slider area
        if(gapRight>gapLeft){
            g.setColour(fillCol);
            g.fillRect(gapLeft,gapTop,gapRight-gapLeft,gapH);
        }
        // Also fill left of k1 and right of k2 within card bounds
        // (covers any slider bleed at card edges)
        g.fillRect(bX+2, gapTop, k1cx-fxKSz/2-bX-3, gapH);
        g.fillRect(k2cx+fxKSz/2+1, gapTop, (bX+cardW-3)-(k2cx+fxKSz/2+1), gapH);

        // Repaint card header area (title + power icon)
        g.setColour(fillCol);
        g.fillRect(bX+2,Y+5,cardW-4,26);
        g.setColour(on?kPurple.withAlpha(.35f):juce::Colours::transparentBlack);
        g.fillRect(bX+2,Y+5,cardW-4,2);

        // Title text
        g.setFont(juce::Font(10.f,juce::Font::bold));
        g.setColour(on?juce::Colours::white:kMuted.withAlpha(.5f));
        g.drawText(titles[i],bX+4,Y+12,cardW-8,15,juce::Justification::centred);

        // Power circle
        float pix=(float)(bX+cardW-22),piy=(float)(Y+11);
        g.setColour(on?kPurple:kMuted.withAlpha(.4f));
        g.drawEllipse(pix,piy+1,11.f,11.f,1.5f);
        g.fillRect(pix+4.5f,piy-1.f,2.f,5.f);

        // Card border on top
        g.setColour(on?kPurple.withAlpha(.6f):kCardBd);
        g.drawRoundedRectangle((float)bX+1,(float)(Y+4),(float)(cardW-2),(float)(kFXH-8),8.f,on?1.5f:1.f);
    }
}

void ArcaneEclipseEditor::paintTopBar(juce::Graphics& g)
{
    g.setColour(kSurf); g.fillRect(0,0,W,kTopH);
    g.setColour(kCardBd); g.drawHorizontalLine(kTopH,0.f,(float)W);
    // Star logo
    g.setColour(kPurple);
    float lx=22.f,ly=24.f,lr=11.f;
    for(int i=0;i<4;++i){float a=i*juce::MathConstants<float>::pi*.5f;
        g.drawLine(lx,ly,lx+lr*std::cos(a),ly+lr*std::sin(a),1.5f);}
    g.fillEllipse(lx-2.5f,ly-2.5f,5.f,5.f);
    // Brand
    g.setFont(juce::Font(18.f,juce::Font::bold)); g.setColour(kText);
    g.drawText("ARCANE",40,10,90,28,juce::Justification::centredLeft);
    g.setFont(juce::Font(11.f,juce::Font::bold)); g.setColour(kPurple);
    g.drawText("ECLIPSE",132,14,70,18,juce::Justification::centredLeft);
    g.setFont(juce::Font(8.f)); g.setColour(kMuted);
    g.drawText("v1.0.0",205,18,44,12,juce::Justification::centredLeft);
    // Preset box
    int px=(W-320)/2,py=8,pw=320,ph=32;
    g.setColour(juce::Colour(0xff0c0c14));
    g.fillRoundedRectangle((float)px,(float)py,(float)pw,(float)ph,5.f);
    g.setColour(kPurple);
    g.drawRoundedRectangle((float)px,(float)py,(float)pw,(float)ph,5.f,1.5f);
    g.setFont(juce::Font(11.f)); g.setColour(kText);
    juce::String nm=proc.isNAMLoaded()?proc.getLoadedNAMName():"Mystic Drive";
    g.drawText(nm,px+12,py+1,pw-20,ph-2,juce::Justification::centredLeft);
    // Icons
    for(int xi:{W-98,W-62,W-26}){
        g.setColour(juce::Colour(0xff252535));
        g.fillRoundedRectangle((float)xi,8.f,30.f,30.f,4.f);
        g.setColour(kCardBd);
        g.drawRoundedRectangle((float)xi,8.f,30.f,30.f,4.f,1.f);
    }
    g.setFont(juce::Font(11.f)); g.setColour(kMuted);
    g.drawText("\u2630",W-98,8,30,30,juce::Justification::centred);
    g.drawText("?",W-62,8,30,30,juce::Justification::centred);
    g.setColour(kPurple);
    g.drawEllipse((float)(W-20),13.f,10.f,10.f,1.5f);
    g.fillRect((float)(W-16),8.f,2.f,6.f);
}

void ArcaneEclipseEditor::paintStrip(juce::Graphics& g)
{
    int Y=kTopH;
    g.setColour(juce::Colour(0xff111116)); g.fillRect(0,Y,W,kStripH);
    g.setColour(kCardBd); g.drawHorizontalLine(Y+kStripH,0.f,(float)W);
    paintVU(g,{14.f,(float)(Y+14),14.f,90.f},vuIn);
    paintVU(g,{(float)(W-28),(float)(Y+14),14.f,90.f},vuOut);
    // Labels
    g.setFont(juce::Font(7.f,juce::Font::bold)); g.setColour(kMuted);
    g.drawText("INPUT",  46,Y+4,80,10,juce::Justification::centred);
    g.drawText("GATE",  118,Y+4,80,10,juce::Justification::centred);
    g.drawText("COMPRESSOR",W-198,Y+4,120,10,juce::Justification::centred);
    g.drawText("OUTPUT",W-86,Y+4,80,10,juce::Justification::centred);
    // Gate on dot
    g.setColour(tbGate.getToggleState()?kPurple:kMuted.withAlpha(.3f));
    g.fillEllipse(174.f,(float)(Y+6),6.f,6.f);
    // Comp on dot
    g.setColour(tbComp.getToggleState()?kPurple:kMuted.withAlpha(.3f));
    g.fillEllipse((float)(W-82),(float)(Y+6),6.f,6.f);
}

void ArcaneEclipseEditor::paintVU(juce::Graphics& g,juce::Rectangle<float> b,float lvl)
{
    g.setColour(juce::Colour(0xff0a0a10)); g.fillRoundedRectangle(b,2.f);
    int n=16; float sh=b.getHeight()/n;
    int lit=(int)(n*juce::jlimit(0.f,1.f,lvl));
    for(int s=0;s<lit;++s){
        float sy=b.getBottom()-(s+1)*sh+1.f;
        if(s>=14)      g.setColour(kRed);
        else if(s>=11) g.setColour(kPurDim);
        else           g.setColour(kPurple);
        g.fillRoundedRectangle(b.getX()+1,sy,b.getWidth()-2,sh-1.5f,1.f);
    }
    g.setColour(kCardBd); g.drawRoundedRectangle(b,2.f,.5f);
}

void ArcaneEclipseEditor::paintChain(juce::Graphics& g)
{
    bool nodeActive[9]={
        tbGate .getToggleState(),
        tbComp .getToggleState(),
        stompOD.getToggleState(),
        proc.isNAMLoaded(),
        proc.isIRLoaded(),
        proc.isNAMLoaded(),
        stompMod.getToggleState(),
        stompDelay.getToggleState(),
        stompReverb.getToggleState()
    };
    for(int i=0;i<9;++i){
        auto nb=chainNodeBounds(i);
        paintChainNode(g,i,nb,nodeActive[i]);
        if(i<8){
            auto nb2=chainNodeBounds(i+1);
            float ax=(float)nb.getRight()+2,ay=(float)nb.getCentreY();
            float ax2=(float)nb2.getX()-2;
            g.setColour((nodeActive[i]&&nodeActive[i+1])?kPurple.withAlpha(.8f):kMuted.withAlpha(.25f));
            g.drawLine(ax,ay,ax2,ay,1.5f);
            g.drawLine(ax2-5,ay-4,ax2,ay,1.5f);
            g.drawLine(ax2-5,ay+4,ax2,ay,1.5f);
        }
    }
}

void ArcaneEclipseEditor::paintChainNode(juce::Graphics& g,int idx,
                                          juce::Rectangle<int> b,bool active)
{
    g.setColour(active?kPurple.withAlpha(.18f):kCard);
    g.fillRoundedRectangle(b.toFloat(),7.f);
    g.setColour(active?kPurple:kCardBd);
    g.drawRoundedRectangle(b.toFloat(),7.f,active?2.f:1.f);

    auto ib=b.toFloat().reduced(10.f,8.f);
    float cx=ib.getCentreX(),cy=ib.getCentreY();
    g.setColour(active?kPurple:kMuted);
    juce::Path p;
    switch(idx){
        case 0: g.drawLine(ib.getX(),ib.getBottom(),ib.getRight(),ib.getY(),2.f); break;
        case 1: p.startNewSubPath(ib.getX(),ib.getBottom());p.lineTo(cx,cy+2);
                p.cubicTo(cx,cy+2,cx,cy-2,ib.getRight(),ib.getY());
                g.strokePath(p,juce::PathStrokeType(1.8f)); break;
        case 2: g.fillEllipse(ib.getX(),cy-5,7,7);g.fillEllipse(ib.getX(),cy+1,7,7);
                g.drawLine(ib.getX()+8,cy-1,ib.getRight(),cy-1,1.5f);
                g.drawLine(ib.getX()+8,cy+4,ib.getRight(),cy+4,1.5f); break;
        case 3: g.drawRoundedRectangle(ib,2.f,1.8f);
                g.drawLine(ib.getX(),ib.getY()+5,ib.getRight(),ib.getY()+5,1.f);
                g.fillEllipse(cx-4,cy-2,8,8); break;
        case 4: g.drawRoundedRectangle(ib.reduced(0,1),3.f,1.8f);
                g.drawEllipse(cx-5,cy-4,10,10,1.5f);
                g.drawEllipse(cx-2,cy-1,5,5,1.f); break;
        case 5:{ float pos[]={.35f,.65f,.25f};
                for(int f=0;f<3;++f){float fx=ib.getX()+f*(ib.getWidth()/2.5f);
                    g.drawLine(fx,ib.getY(),fx,ib.getBottom(),1.2f);
                    g.fillEllipse(fx-2.5f,ib.getY()+pos[f]*ib.getHeight()-2.5f,5,5);}break;}
        case 6: p.startNewSubPath(ib.getX(),cy);
                p.cubicTo(ib.getX()+6,cy-7,ib.getX()+12,cy+7,cx,cy);
                p.cubicTo(cx+6,cy-7,ib.getRight()-5,cy+7,ib.getRight(),cy);
                g.strokePath(p,juce::PathStrokeType(1.8f)); break;
        case 7: g.drawEllipse(ib.reduced(1),1.8f);
                g.drawLine(cx,cy,cx,ib.getY()+5,1.8f);
                g.drawLine(cx,cy,cx+5,cy+4,1.8f); break;
        case 8: for(int w=0;w<2;++w){float wy=cy-2+w*7.f;p.clear();
                p.startNewSubPath(ib.getX(),wy);
                p.cubicTo(ib.getX()+5,wy-4,ib.getX()+10,wy+4,cx,wy);
                p.cubicTo(cx+5,wy-4,ib.getRight()-5,wy+4,ib.getRight(),wy);
                g.strokePath(p,juce::PathStrokeType(1.4f-w*.3f));} break;
    }
    g.setFont(juce::Font(7.f,juce::Font::bold));
    g.setColour(active?kPurple:kMuted);
    g.drawText(kChainLabels[idx],
               juce::Rectangle<int>(b.getX()-3,b.getBottom()+2,b.getWidth()+6,11),
               juce::Justification::centred);
}

void ArcaneEclipseEditor::paintAmpHead(juce::Graphics& g)
{
    int Y=kTopH+kStripH, H2=kAmpH;
    // Chassis gradient
    juce::ColourGradient cg(juce::Colour(0xff1c1c28),0,(float)Y,
                             juce::Colour(0xff111118),0,(float)(Y+H2),false);
    g.setGradientFill(cg); g.fillRect(0,Y,W,H2);
    g.setColour(kPurple.withAlpha(.6f)); g.fillRect(0,Y,W,2);
    g.setColour(kCardBd); g.drawHorizontalLine(Y+H2,0.f,(float)W);

    // Grille (upper 55%)
    int gH=(int)(H2*.55f);
    g.setColour(juce::Colour(0xff0a0a10));
    g.fillRect(12,Y+6,W-24,gH-4);
    g.setColour(juce::Colour(0xff131320));
    for(int mx=16;mx<W-16;mx+=7) g.drawVerticalLine(mx,(float)(Y+8),(float)(Y+gH-2));
    for(int my=Y+8;my<Y+gH-2;my+=6) g.drawHorizontalLine(my,16.f,(float)(W-16));

    // Handle
    juce::ColourGradient hg(juce::Colour(0xff303048),(float)(W/2),0,
                             juce::Colour(0xff181828),(float)(W/2),14,false);
    g.setGradientFill(hg);
    g.fillRoundedRectangle((float)(W/2-70),(float)(Y+2),140.f,14.f,7.f);

    // Nameplate
    int npW=300,npH=64,npX=(W-npW)/2,npY=Y+gH/2-npH/2;
    juce::ColourGradient npg(juce::Colour(0xff252538),(float)npX,(float)npY,
                              juce::Colour(0xff141422),(float)npX,(float)(npY+npH),false);
    g.setGradientFill(npg); g.fillRoundedRectangle((float)npX,(float)npY,(float)npW,(float)npH,5.f);
    g.setColour(kPurple); g.drawRoundedRectangle((float)npX,(float)npY,(float)npW,(float)npH,5.f,1.5f);
    for(auto pt:{std::pair<int,int>{npX+7,npY+7},{npX+npW-7,npY+7},{npX+7,npY+npH-7},{npX+npW-7,npY+npH-7}}){
        g.setColour(kPurDim); g.fillEllipse((float)pt.first-3,(float)pt.second-3,6.f,6.f);
    }
    g.setFont(juce::Font("Georgia",26.f,juce::Font::bold)); g.setColour(kText);
    g.drawText("ARCANE",juce::Rectangle<int>(npX,npY+6,npW,28),juce::Justification::centred);
    g.setFont(juce::Font(9.f,juce::Font::bold)); g.setColour(kPurple);
    g.drawText("ECLIPSE",juce::Rectangle<int>(npX,npY+36,npW,16),juce::Justification::centred);
    // Purple underline
    g.setColour(kPurple); g.fillRect((float)(npX+npW/2-30),(float)(npY+33),60.f,1.5f);

    // Faceplate
    int fY=Y+gH,fH=H2-gH;
    g.setColour(juce::Colour(0xff161622)); g.fillRect(0,fY,W,fH);
    g.setColour(kPurple.withAlpha(.5f)); g.fillRect(0,fY,W,2);
    // Jack
    g.setColour(juce::Colour(0xff0a0a12)); g.fillEllipse(16.f,(float)(fY+fH/2-9),18.f,18.f);
    g.setColour(kCardBd); g.drawEllipse(16.f,(float)(fY+fH/2-9),18.f,18.f,1.5f);
    g.setFont(juce::Font(7.f)); g.setColour(kMuted);
    g.drawText("IN",10,fY+3,28,10,juce::Justification::centred);
    // Power LED
    g.setColour(kPurple); g.fillEllipse((float)(W-38),(float)(fY+fH/2-12),24.f,24.f);
    g.setColour(kPurple.brighter(.5f)); g.fillEllipse((float)(W-33),(float)(fY+fH/2-7),10.f,10.f);
    g.setFont(juce::Font(7.f)); g.setColour(kMuted);
    g.drawText("POWER",(float)(W-48),(float)(fY+fH/2+14),44,10,juce::Justification::centred);
}

void ArcaneEclipseEditor::paintFXSection(juce::Graphics& g)
{
    int Y=kTopH+kStripH+kAmpH;
    g.setColour(kBg); g.fillRect(0,Y,W,kFXH);
    g.setColour(kCardBd); g.drawHorizontalLine(Y,0.f,(float)W);

    int nCards=4,cabW=kCabW;
    int fxArea=W-cabW-8;
    int cardW=fxArea/nCards-5;

    const char* titles[]={"OVERDRIVE","MODULATION","DELAY","REVERB"};
    juce::ToggleButton* tbs[]={&stompOD,&stompMod,&stompDelay,&stompReverb};
    int cardX0=4;

    for(int i=0;i<4;++i){
        int bX=cardX0+i*(cardW+5);
        bool on=tbs[i]->getToggleState();
        // Card bg
        g.setColour(on?juce::Colour(0xff1e1e2c):kCard);
        g.fillRoundedRectangle((float)bX+1,(float)(Y+4),(float)(cardW-2),(float)(kFXH-8),8.f);
        g.setColour(on?kPurple.withAlpha(.6f):kCardBd);
        g.drawRoundedRectangle((float)bX+1,(float)(Y+4),(float)(cardW-2),(float)(kFXH-8),8.f,on?1.5f:1.f);
        if(on){g.setColour(kPurple.withAlpha(.35f));g.fillRect((float)(bX+1),(float)(Y+4),(float)(cardW-2),2.f);}
        // Title
        g.setFont(juce::Font(10.f,juce::Font::bold));
        g.setColour(on?juce::Colours::white:kMuted.withAlpha(.5f));
        g.drawText(titles[i],bX+4,Y+12,cardW-8,15,juce::Justification::centred);
        // Power icon
        float pix=(float)(bX+cardW-22),piy=(float)(Y+11);
        g.setColour(on?kPurple:kMuted.withAlpha(.4f));
        g.drawEllipse(pix,piy+1,11.f,11.f,1.5f);
        g.fillRect(pix+4.5f,piy-1.f,2.f,5.f);
    }
    // Repaint card backgrounds OVER any slider component backgrounds
    // This covers the default grey/dark bars that JUCE draws between rotary knobs
    for(int i=0;i<4;++i){
        int bX=cardX0+i*(cardW+5);
        bool on=tbs[i]->getToggleState();
        // Refill the card area except knob positions
        juce::Rectangle<float> cardR((float)bX+1,(float)(Y+4),(float)(cardW-2),(float)(kFXH-8));
        // Top section (above knobs)
        g.setColour(on?juce::Colour(0xff1e1e2c):kCard);
        g.fillRoundedRectangle(cardR,8.f);
        g.setColour(on?kPurple.withAlpha(.6f):kCardBd);
        g.drawRoundedRectangle(cardR,8.f,on?1.5f:1.f);
        if(on){g.setColour(kPurple.withAlpha(.35f));g.fillRect((float)(bX+1),(float)(Y+4),(float)(cardW-2),2.f);}
        g.setFont(juce::Font(10.f,juce::Font::bold));
        g.setColour(on?juce::Colours::white:kMuted.withAlpha(.5f));
        g.drawText(titles[i],bX+4,Y+12,cardW-8,15,juce::Justification::centred);
        float pix=(float)(bX+cardW-22),piy=(float)(Y+11);
        g.setColour(on?kPurple:kMuted.withAlpha(.4f));
        g.drawEllipse(pix,piy+1,11.f,11.f,1.5f);
        g.fillRect(pix+4.5f,piy-1.f,2.f,5.f);
    }

    // Divider before cab
    g.setColour(kCardBd);
    g.drawVerticalLine(W-cabW-4,(float)(Y+4),(float)(Y+kFXH-4));
}

void ArcaneEclipseEditor::paintCabSection(juce::Graphics& g)
{
    int fxY=kTopH+kStripH+kAmpH;
    int cX=W-kCabW,cabH=kFXH;

    // Card
    g.setColour(kCard);
    g.fillRoundedRectangle((float)cX,(float)(fxY+4),kCabW-4,(float)(cabH-8),8.f);
    g.setColour(kPurple.withAlpha(.5f));
    g.drawRoundedRectangle((float)cX,(float)(fxY+4),kCabW-4,(float)(cabH-8),8.f,1.5f);

    // Header
    g.setColour(kCardBd); g.drawHorizontalLine(fxY+30,(float)cX,(float)(cX+kCabW-4));
    g.setFont(juce::Font(10.f,juce::Font::bold)); g.setColour(kPurple);
    g.drawText("NAM & IR LOADER",cX+2,fxY+8,kCabW-8,18,juce::Justification::centred);

    // Cabinet photo — compact to give more field space
    int phX=cX+6,phY=fxY+38,phW=88,phH=110;
    g.setColour(juce::Colour(0xff080810)); g.fillRoundedRectangle((float)phX,(float)phY,(float)phW,(float)phH,5.f);
    g.setColour(kCardBd); g.drawRoundedRectangle((float)phX,(float)phY,(float)phW,(float)phH,5.f,1.f);
    // Speaker
    float sx=(float)(phX+phW/2),sy=(float)(phY+phH/2);
    for(float rr=20.f;rr<44.f;rr+=9.f){g.setColour(juce::Colour(0xff1a1a2e));g.drawEllipse(sx-rr,sy-rr,rr*2,rr*2,2.f);}
    g.setColour(juce::Colour(0xff222234));g.fillEllipse(sx-6,sy-6,12.f,12.f);
    g.setFont(juce::Font(7.f,juce::Font::bold)); g.setColour(juce::Colour(0xff2a2a40));
    g.drawText("ARCANE",phX,phY+phH-14,phW,12,juce::Justification::centred);

    // Fields
    int rx=cX+112,ry=fxY+38;
    auto drawField=[&](const juce::String& lbl,const juce::String& val,int y,bool loaded){
        g.setFont(juce::Font(7.f,juce::Font::bold)); g.setColour(kMuted);
        g.drawText(lbl,rx,y,80,11,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff0c0c14));
        g.fillRoundedRectangle((float)rx,(float)(y+13),(float)(kCabW-cX+cX-rx-12),22.f,4.f);
        g.setColour(loaded?kPurple.withAlpha(.4f):kCardBd);
        g.drawRoundedRectangle((float)rx,(float)(y+13),(float)(kCabW-cX+cX-rx-12),22.f,4.f,1.f);
        g.setFont(juce::Font(9.f));
        g.setColour(loaded?kText:kMuted.withAlpha(.5f));
        g.drawFittedText(val,rx+4,y+14,(int)(kCabW-cX+cX-rx-20),18,juce::Justification::centredLeft,1);
    };
    drawField("MODEL",proc.isNAMLoaded()?proc.getLoadedNAMName():"No model loaded",ry,  proc.isNAMLoaded());
    drawField("IR",   proc.isIRLoaded() ?proc.getLoadedIRName() :"No IR loaded",   ry+42,proc.isIRLoaded());

    // AMP/CAB dots
    g.setColour(proc.isNAMLoaded()?kGreen:kMuted.withAlpha(.3f));
    g.fillEllipse((float)(cX+kCabW-36),(float)(fxY+cabH-22),8.f,8.f);
    g.setFont(juce::Font(8.f)); g.setColour(kMuted);
    g.drawText("AMP",(float)(cX+kCabW-26),(float)(fxY+cabH-24),28,12,juce::Justification::centredLeft);
    g.setColour(proc.isIRLoaded()?kGreen:kMuted.withAlpha(.3f));
    g.fillEllipse((float)(cX+kCabW-36),(float)(fxY+cabH-8),8.f,8.f);
    g.drawText("CAB",(float)(cX+kCabW-26),(float)(fxY+cabH-10),28,12,juce::Justification::centredLeft);
}

void ArcaneEclipseEditor::paintSceneBar(juce::Graphics& g)
{
    int Y=kTopH+kStripH+kAmpH+kFXH;
    juce::ColourGradient sg(juce::Colour(0xff1a1a26),0,(float)Y,
                             juce::Colour(0xff111118),0,(float)(Y+kSceneH),false);
    g.setGradientFill(sg); g.fillRect(0,Y,W,kSceneH);
    g.setColour(kPurple.withAlpha(.3f)); g.fillRect(0,Y,W,1);
    g.setColour(kCardBd); g.drawHorizontalLine(Y+kSceneH,0.f,(float)W);

    // Draw scene buttons appearance
    int sW=(W-16)/5-4;
    for(int i=0;i<5;++i){
        int bX=8+i*(sW+4),bY=Y+6;
        bool active=(i==activeScene);
        bool hasData=!scenes[i].isEmpty();
        g.setColour(active?kPurple.withAlpha(.25f):(hasData?juce::Colour(0xff1e1e2c):kCard));
        g.fillRoundedRectangle((float)bX,(float)bY,(float)sW,(float)(kSceneH-12),6.f);
        g.setColour(active?kPurple:(hasData?kPurDim:kCardBd));
        g.drawRoundedRectangle((float)bX,(float)bY,(float)sW,(float)(kSceneH-12),6.f,active?2.f:1.f);
        // LED dot
        g.setColour(active?kPurple:(hasData?kPurDim.withAlpha(.5f):kMuted.withAlpha(.2f)));
        g.fillEllipse((float)(bX+8),(float)(bY+kSceneH/2-14),6.f,6.f);
        // Scene label
        g.setFont(juce::Font(9.f,juce::Font::bold));
        g.setColour(active?juce::Colours::white:(hasData?kText:kMuted.withAlpha(.4f)));
        g.drawText(hasData?scenes[i].name:"SCENE "+juce::String(i+1),
                   bX+18,bY+1,sW-22,kSceneH-14,juce::Justification::centredLeft);
        // Save indicator dot (top right of slot)
        if(hasData){
            g.setColour(kPurDim.withAlpha(.6f));
            g.fillEllipse((float)(bX+sW-10),(float)(bY+4),5.f,5.f);
        }
    }
}

void ArcaneEclipseEditor::paintFooter(juce::Graphics& g)
{
    int Y=kTopH+kStripH+kAmpH+kFXH+kSceneH;
    juce::ColourGradient fg(juce::Colour(0xff111116),0,(float)Y,kBg,0,(float)H,false);
    g.setGradientFill(fg); g.fillRect(0,Y,W,kFootH);
    g.setColour(kCardBd); g.drawHorizontalLine(Y,0.f,(float)W);
    // Headphone icon
    g.setColour(kMuted); juce::Path hp;
    hp.addCentredArc(24.f,(float)(Y+17),8.f,7.f,0.f,3.3f,6.22f,true);
    g.strokePath(hp,juce::PathStrokeType(2.f));
    g.fillEllipse(14.f,(float)(Y+20),5.f,8.f); g.fillEllipse(27.f,(float)(Y+20),5.f,8.f);
    g.setFont(juce::Font(8.f,juce::Font::bold)); g.setColour(kMuted);
    g.drawText("INPUT MONITOR",36,Y+9,105,16,juce::Justification::centredLeft);
    // ON badge
    g.setColour(kPurple); g.fillRoundedRectangle(142.f,(float)(Y+10),28.f,14.f,3.f);
    g.setFont(juce::Font(7.f,juce::Font::bold)); g.setColour(juce::Colours::white);
    g.drawText("ON",142,Y+10,28,14,juce::Justification::centred);
    // RIG/FX
    g.setColour(kPurple); g.fillRoundedRectangle((float)(W/2-30),(float)(Y+6),58.f,22.f,4.f);
    g.setFont(juce::Font(9.f,juce::Font::bold)); g.setColour(juce::Colours::white);
    g.drawText("RIG",W/2-30,Y+6,58,22,juce::Justification::centred);
    g.setColour(kMuted); g.drawText("FX",W/2+36,Y+9,24,16,juce::Justification::centred);
    // Tuner button drawn by the TextButton component itself — nothing to draw here
    // AMP/CAB right
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
    g.fillAll(kBg);
    // Header
    g.setColour(kSurf); g.fillRect(0,0,W,kTopH);
    g.setColour(kPurple.withAlpha(.6f)); g.fillRect(0,kTopH-2,W,2);
    g.setFont(juce::Font(16.f,juce::Font::bold)); g.setColour(kText);
    g.drawText("CHROMATIC TUNER",0,0,W,kTopH,juce::Justification::centred);
    // Close hint
    g.setFont(juce::Font(9.f)); g.setColour(kMuted);
    g.drawText("Press TUNER to return",W-160,0,155,kTopH,juce::Justification::centredLeft);

    int cx=W/2,cy=H/2-20;
    // Outer ring
    g.setColour(kCard); g.fillEllipse((float)(cx-180),(float)(cy-180),360.f,360.f);
    g.setColour(kCardBd); g.drawEllipse((float)(cx-180),(float)(cy-180),360.f,360.f,2.f);
    // Center zone (in tune = green)
    bool inTune=std::fabs(tunerCents)<3.f && tunerHz>0;
    g.setColour(inTune?kGreen.withAlpha(.2f):kCard);
    g.fillEllipse((float)(cx-40),(float)(cy-40),80.f,80.f);
    // Cents needle
    if(tunerHz>0){
        float angle=juce::MathConstants<float>::pi*(tunerCents/60.f);
        float nx=cx+160.f*std::sin(angle),ny=cy-160.f*std::cos(angle);
        g.setColour(inTune?kGreen:kPurple);
        g.drawLine((float)cx,(float)cy,nx,ny,3.f);
    }
    // Note name
    g.setFont(juce::Font("Georgia",72.f,juce::Font::bold));
    g.setColour(inTune?kGreen:kText);
    g.drawText(tunerHz>0?tunerNote:"--",cx-80,cy-50,160,100,juce::Justification::centred);
    // Cents display
    g.setFont(juce::Font(14.f)); g.setColour(kMuted);
    if(tunerHz>0)
        g.drawText(juce::String(tunerCents,1)+" cents",cx-80,cy+50,160,24,juce::Justification::centred);
    // Cent markers
    for(int c=-6;c<=6;++c){
        float a=juce::MathConstants<float>::pi*(c/10.f);
        float r1=150.f,r2=c==0?165.f:158.f;
        float x1=cx+r1*std::sin(a),y1=cy-r1*std::cos(a);
        float x2=cx+r2*std::sin(a),y2=cy-r2*std::cos(a);
        g.setColour(c==0?kPurple:kCardBd);
        g.drawLine(x1,y1,x2,y2,c==0?2.f:1.f);
    }
    // Hz readout
    g.setFont(juce::Font(11.f)); g.setColour(kMuted);
    g.drawText(tunerHz>0?juce::String(tunerHz,1)+" Hz":"---",cx-80,cy+78,160,20,juce::Justification::centred);
    // Footer hint
    g.setColour(kCardBd); g.drawHorizontalLine(H-kFootH,0.f,(float)W);
    g.setFont(juce::Font(9.f)); g.setColour(kMuted);
    g.drawText("Click TUNER to close",0,H-kFootH,W,kFootH,juce::Justification::centred);
}
