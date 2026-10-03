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

// ── v1.1 re-skin geometry ─────────────────────────────────────────────────────
// background.png is the approved render (1514 x 1039). Every coordinate below is
// written in RENDER pixels so it can be read straight off the artwork, then mapped
// to the 1200 x 823 window by these helpers.
static constexpr float kRW = 1514.f, kRH = 1039.f;
static inline float SX(float x){ return x * 1200.f / kRW; }
static inline float SY(float y){ return y *  823.f / kRH; }
static inline juce::Rectangle<float> RF(float x0,float y0,float x1,float y1)
    { return { SX(x0), SY(y0), SX(x1)-SX(x0), SY(y1)-SY(y0) }; }
static inline juce::Rectangle<int>   RR(float x0,float y0,float x1,float y1)
    { return RF(x0,y0,x1,y1).getSmallestIntegerContainer(); }

// Flat fills sampled from the render, used to paint over baked text cleanly
static const juce::Colour kPresetFill{0xff110d17};
static const juce::Colour kFooterFill{0xff14101a};

// Mark a button as an invisible hotspot over baked art (hover tint only)
static void makeHotspot(juce::Button& b, float radius = 6.f){
    b.getProperties().set("hs", radius);
    b.setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

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
    // v1.1 knob: one 256px image (rim fills the square, pointer + glow baked in at
    // 12 o'clock), rotated to the value. Same art at all three sizes.
    static juce::Image knob = juce::ImageCache::getFromMemory(
        BinaryData::knob_v11_png, BinaryData::knob_v11_pngSize);
    if (! knob.isValid()) return;

    auto b=juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h);
    auto c=b.getCentre();
    float sz=juce::jmin(b.getWidth(),b.getHeight());
    float ang=startA+pos*(endA-startA);
    float s = sz/(float)knob.getWidth();
    auto tr = juce::AffineTransform::scale(s)
                .translated(c.x - sz*0.5f, c.y - sz*0.5f)
                .rotated(ang, c.x, c.y);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImageTransformed(knob, tr, false);
}
void AELAF::drawLinearSlider(juce::Graphics& g,int x,int y,int w,int h,
                             float pos,float,float,juce::Slider::SliderStyle,juce::Slider&)
{
    // Dual Amp/IR MIX: amp 1 (left, lilac) <-> amp 2 (right, purple)
    float cy=y+h*0.68f, th=3.f;
    auto track=juce::Rectangle<float>((float)x+4.f,cy-th*0.5f,(float)w-8.f,th);
    g.setColour(juce::Colour(0xcc0c0912)); g.fillRoundedRectangle(track.expanded(2.f),3.f);
    juce::ColourGradient cg(juce::Colour(0xffc9a3ff),track.getX(),cy,kPurple,track.getRight(),cy,false);
    g.setGradientFill(cg); g.fillRoundedRectangle(track,1.5f);
    float r=juce::jmin(6.f,h*0.26f);
    g.setColour(kPurple.withAlpha(0.28f)); g.fillEllipse(pos-r*1.35f,cy-r*1.35f,r*2.7f,r*2.7f);
    g.setColour(juce::Colour(0xff1a1622)); g.fillEllipse(pos-r,cy-r,2*r,2*r);
    g.setColour(kPurple);                  g.drawEllipse(pos-r,cy-r,2*r,2*r,1.6f);
    g.setColour(juce::Colour(0xffefe0ff)); g.fillEllipse(pos-r*0.35f,cy-r*0.35f,r*0.7f,r*0.7f);
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
                                  const juce::Colour&,bool isOver,bool isDown){
    auto b=btn.getLocalBounds().toFloat().reduced(.5f);
    if (btn.getProperties().contains("hs")) {          // hotspot over baked art
        if (isOver || isDown) {
            float r = (float) btn.getProperties()["hs"];
            g.setColour(juce::Colours::white.withAlpha(isDown ? 0.10f : 0.05f));
            g.fillRoundedRectangle(b, r);
        }
        return;
    }
    bool on=btn.getToggleState();
    g.setColour(isDown?kPurple.withAlpha(.35f):(on?kPurple.withAlpha(.22f):
                btn.findColour(juce::TextButton::buttonColourId)));
    g.fillRoundedRectangle(b,6.f);
    g.setColour(on||isDown?kPurple:kCardBd);
    g.drawRoundedRectangle(b,6.f,on?1.8f:1.f);
}
void AELAF::drawButtonText(juce::Graphics& g,juce::TextButton& btn,bool over,bool down){
    if (btn.getProperties().contains("hs")) return;     // label is part of the render
    juce::LookAndFeel_V4::drawButtonText(g,btn,over,down);
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
    "ARCANE ECLIPSE v1.1.0\n"
    "by AMARI LABS\n"
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
void AEKnob::place(int cx,int cy,int sz,bool){
    slider.setBounds(cx-sz/2, cy-sz/2, sz, sz);
    // v1.1: knob names + scales are baked into the render; the value is shown
    // on the knob face while hovering/dragging (see paintOverChildren).
    nameLabel.setVisible(false);
    valLabel .setVisible(false);
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
    kRHiCut   .setup(this,p.apvts,ArcaneEclipseProcessor::idReverbHighCut,"HIGH CUT",&laf);

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

    addAndMakeVisible(stereoBtn);
    attStereo = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        proc.apvts, ArcaneEclipseProcessor::idStereoMode, stereoBtn);
    stereoBtn.onClick = [this]{ repaint(); };

    setWantsKeyboardFocus(true);
    actLearns = {                 // direct MIDI-learn buttons
        {&bankPrev, 4}, {&bankNext, 5}, {&presetPrev, 6}, {&presetNext, 7}
    };
    for (auto& a : actLearns) a.comp->addMouseListener(this, false);

    for(auto* t:{&tbGate,&tbComp,&stompOD,&stompMod,&stompDelay,&stompReverb,&tbCab})
        addAndMakeVisible(*t);
    attGate  =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idGateOn,   tbGate);
    attComp  =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idCompOn,   tbComp);
    attOD    =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idODOn,     stompOD);
    attMod   =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idModOn,   stompMod);
    attDelay =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idDelayOn, stompDelay);
    attReverb=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idReverbOn,stompReverb);
    attCab   =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idCabBypass,tbCab);

    // v1.1: pedal footswitches drive the same on/off params as the chain nodes
    for(auto* t:{&fsOD,&fsMod,&fsDelay,&fsReverb}){
        addAndMakeVisible(*t); t->setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    for(auto* t:{&tbGate,&tbComp,&stompOD,&stompMod,&stompDelay,&stompReverb,&tbTuner,&presetPrev,&presetNext,&stereoBtn})
        t->setMouseCursor(juce::MouseCursor::PointingHandCursor);
    attFsOD    =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idODOn,    fsOD);
    attFsMod   =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idModOn,   fsMod);
    attFsDelay =std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idDelayOn, fsDelay);
    attFsReverb=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idReverbOn,fsReverb);

    // v1.1: effect TYPE selection — the small pill under MIX on MOD / DELAY / REVERB
    for(int i=0;i<3;++i){
        makeHotspot(typeBtn[i],6.f);
        typeBtn[i].setTooltip("Click to choose the effect type");
        typeBtn[i].onClick=[this,i]{ showTypeMenu(i); };
        addAndMakeVisible(typeBtn[i]);
    }
    // MODEL / IR fields act as dropdowns (load / clear)
    makeHotspot(fieldModel,5.f); fieldModel.onClick=[this]{ showFieldMenu(false); }; addAndMakeVisible(fieldModel);
    makeHotspot(fieldIR,5.f);    fieldIR.onClick   =[this]{ showFieldMenu(true);  }; addAndMakeVisible(fieldIR);
    // DUAL AMP/IR: run two amp+IR sets in parallel and blend them (v1.1)
    makeHotspot(dualBtn,6.f);
    dualBtn.setClickingTogglesState(true);
    dualBtn.setTooltip("Dual Amp/IR: run two amp + IR sets side by side and blend them with MIX");
    attDual=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.apvts,ArcaneEclipseProcessor::idDualOn,dualBtn);
    dualBtn.onClick=[this]{ updateDualUI(); repaint(); };
    addAndMakeVisible(dualBtn);
    for(int i=0;i<2;++i){
        makeHotspot(ampSelBtn[i],5.f);
        ampSelBtn[i].setTooltip(i==0?"Edit amp 1 (model + IR)":"Edit amp 2 (model + IR)");
        ampSelBtn[i].onClick=[this,i]{ editAmp=i; repaint(); };
        addChildComponent(ampSelBtn[i]);
    }
    dualMix.setLookAndFeel(&laf);
    dualMix.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    dualMix.setTooltip("Blend between amp 1 and amp 2");
    dualMix.setDoubleClickReturnValue(true,0.5);
    attDualMix=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.apvts,ArcaneEclipseProcessor::idDualMix,dualMix);
    addChildComponent(dualMix);

    addAndMakeVisible(btnLoadModel);
    btnLoadModel.onClick=[this]{
        chooserModel=std::make_unique<juce::FileChooser>("Load Model (.nam / .aecap)",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.nam;*.aecap");
        chooserModel->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc){auto r=fc.getResults();if(!r.isEmpty()) loadModelFile(r[0]);});
    };
    addAndMakeVisible(btnLoadIR);
    btnLoadIR.onClick=[this]{
        chooserIR=std::make_unique<juce::FileChooser>("Load IR",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.wav");
        chooserIR->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc){auto r=fc.getResults();if(!r.isEmpty()){proc.loadIR(r[0],editSlot());repaint();}});
    };
    makeHotspot(btnLoadModel,6.f); makeHotspot(btnLoadIR,6.f);

    // Scene slots 1-4
    for(int i=0;i<4;++i){
        makeHotspot(sceneBtn[i],6.f);
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
    makeHotspot(bankPrev,6.f); makeHotspot(bankNext,6.f);
    bankPrev.setButtonText("< BANK"); bankNext.setButtonText("BANK >");
    addAndMakeVisible(bankPrev); addAndMakeVisible(bankNext);
    bankPrev.onClick=[this]{ stepBank(-1); };
    bankNext.onClick=[this]{ stepBank(+1); };

    // Header preset nav (invisible over painted arrows) + save
    presetPrev.setClickingTogglesState(false); presetNext.setClickingTogglesState(false);
    addAndMakeVisible(presetPrev); addAndMakeVisible(presetNext);
    presetPrev.onClick=[this]{ stepPreset(-1); };
    presetNext.onClick=[this]{ stepPreset(+1); };
    makeHotspot(headerSave,5.f);
    addAndMakeVisible(headerSave);
    headerSave.onClick=[this]{
        juce::PopupMenu m;
        m.addSectionHeader("Save current settings to:");
        for (int bank=0; bank<kNumBanks; ++bank) {
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
                if (r>=1 && r<=kNumScenes) { saveScene(r-1); currentBank=(r-1)/kSlotsPerBank; refreshSceneButtons(); repaint(); }
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
    makeHotspot(*helpBtn,5.f);
    helpBtn->onClick=[this]{ showCredits(); };
    addAndMakeVisible(*helpBtn);
    helpBtn->setBounds(RR(1418,13,1454,50));

    // Tuner toggle sits on the header tuning-fork icon
    tbTuner.setClickingTogglesState(true);
    tbTuner.onClick=[this]{ setTunerVisible(tbTuner.getToggleState()); };
    addAndMakeVisible(tbTuner);

    loadPresets();
    refreshSceneButtons();
    updateDualUI();
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
    learningAction = proc.actionLearningNow();
    {
        int pa = proc.takePendingAction();
        if      (pa >= 0 && pa <= 3) { loadScene(currentBank*4+pa); refreshSceneButtons(); repaint(); }
        else if (pa == 4) stepBank(-1);
        else if (pa == 5) stepBank(+1);
        else if (pa == 6) stepPreset(-1);
        else if (pa == 7) stepPreset(+1);
    }
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
    vuIn  = juce::jmax(vuIn  * 0.80f, juce::jlimit(0.f,1.f, proc.inLevel.load()));
    vuOut = juce::jmax(vuOut * 0.80f, juce::jlimit(0.f,1.f, proc.outLevel.load()));
    updateDualUI();
    repaint();
}

void ArcaneEclipseEditor::setTunerVisible(bool v)
{
    tunerVisible=v;
    proc.tunerActive.store(v);
    if(!v){ proc.tunerFreq.store(0.f); tunerHz=0.f; tunerNote={}; tunerCents=0.f; }
    tbTuner.setToggleState(v,juce::dontSendNotification);
    // Hide only what is currently visible, and restore exactly that set — the
    // re-skin keeps several components hidden on purpose (baked labels etc.).
    if(v){
        hiddenByTuner.clear();
        for(auto* c:getChildren())
            if(c!=&tbTuner && c->isVisible()){ hiddenByTuner.push_back(c); c->setVisible(false); }
    } else {
        for(auto* c:hiddenByTuner) if(c!=&creditsPanel) c->setVisible(true);
        hiddenByTuner.clear();
    }
    repaint();
}
void ArcaneEclipseEditor::stepPreset(int d){
    int n=activeScene<0?0:activeScene; n=(n+d+kNumScenes)%kNumScenes; loadScene(n);
    currentBank=n/4; refreshSceneButtons(); repaint();
}
void ArcaneEclipseEditor::stepBank(int d){
    currentBank=(currentBank+d+kNumBanks)%kNumBanks; refreshSceneButtons(); repaint();
}
void ArcaneEclipseEditor::cancelMidiLearn(){
    proc.cancelLearn(); learningID={}; learningAction=-1; repaint();
}
bool ArcaneEclipseEditor::keyPressed(const juce::KeyPress& k){
    if((!learningID.isEmpty() || learningAction>=0) &&
       (k==juce::KeyPress::escapeKey || k==juce::KeyPress::returnKey)){
        cancelMidiLearn(); return true;
    }
    return false;
}
void ArcaneEclipseEditor::mouseDown(const juce::MouseEvent& e)
{
    if(tunerVisible){
        auto pos=e.getEventRelativeTo(this).getPosition();
        if(juce::Rectangle<int>(W-46,12,30,30).contains(pos)) setTunerVisible(false);
        return;
    }
    // A left click anywhere cancels an in-progress MIDI learn
    if((!learningID.isEmpty() || learningAction>=0) && !e.mods.isPopupMenu()){
        cancelMidiLearn(); return;
    }
    if(!e.mods.isPopupMenu()) return;
    // Patch/bank selector MIDI learn (preset < >, bank < >)
    for(auto& a:actLearns) if(e.eventComponent==a.comp){
        int cc=proc.ccForAction(a.action);
        juce::PopupMenu m;
        m.addItem(1, cc<0 ? "MIDI Learn (footswitch)" : "MIDI Learn (re-assign)");
        if(cc>=0) m.addItem(2, "Clear MIDI (CC "+juce::String(cc)+")");
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(a.comp),
            [this,act=a.action](int r){
                if(r==1){ proc.actionLearnStart(act); learningAction=act; }
                else if(r==2){ proc.actionLearnClear(act); }
                repaint();
            });
        return;
    }
    // Scene slot context menu (save / load / rename / delete)
    for(int i=0;i<4;++i) if(e.eventComponent==&sceneBtn[i]){
        int idx=currentBank*4+i; bool empty=scenes[idx].isEmpty();
        int acc=proc.ccForAction(i);
        juce::PopupMenu m;
        m.addItem(1,"Save here");
        m.addItem(2,"Load",!empty);
        m.addItem(3,"Rename...",!empty);
        m.addItem(4,"Delete",!empty);
        m.addSeparator();
        m.addItem(7,"Export tone...",!empty);
        m.addItem(8,"Import tone...");
        m.addSeparator();
        m.addItem(5, acc<0 ? "MIDI Learn (footswitch)" : "MIDI Learn (re-assign)");
        if(acc>=0) m.addItem(6, "Clear MIDI (CC "+juce::String(acc)+")");
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&sceneBtn[i]),
            [this,idx,i](int r){
                if(r==1){ saveScene(idx); refreshSceneButtons(); repaint(); }
                else if(r==2){ loadScene(idx); refreshSceneButtons(); repaint(); }
                else if(r==3){ renameScene(idx); }
                else if(r==4){ deleteScene(idx); }
                else if(r==7){ exportScene(idx); }
                else if(r==8){ importScene(idx); }
                else if(r==5){ proc.actionLearnStart(i); learningAction=i; repaint(); }
                else if(r==6){ proc.actionLearnClear(i); repaint(); }
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
juce::File ArcaneEclipseEditor::getPresetsFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
           .getChildFile("ArcaneEclipse").getChildFile("presets.xml");
}
void ArcaneEclipseEditor::savePresets()
{
    juce::XmlElement root("AEPresets");
    for (int i=0;i<kNumScenes;++i){
        if (scenes[i].isEmpty()) continue;
        auto* e = root.createNewChildElement("Scene");
        e->setAttribute("idx", i);
        e->setAttribute("name", scenes[i].name);
        e->setAttribute("nam",  scenes[i].namPath);
        e->setAttribute("ir",   scenes[i].irPath);
        e->setAttribute("nam2", scenes[i].namPath2);
        e->setAttribute("ir2",  scenes[i].irPath2);
        if (scenes[i].params.isValid())
            if (auto px = scenes[i].params.createXml())
                e->addChildElement(px.release());
    }
    auto f = getPresetsFile();
    f.getParentDirectory().createDirectory();
    root.writeTo(f);
}
void ArcaneEclipseEditor::loadPresets()
{
    auto f = getPresetsFile();
    if (!f.existsAsFile()) return;
    auto xml = juce::parseXML(f);
    if (xml == nullptr || xml->getTagName() != "AEPresets") return;
    for (auto* e : xml->getChildIterator()){
        if (e->getTagName() != "Scene") continue;
        int i = e->getIntAttribute("idx", -1);
        if (i < 0 || i >= kNumScenes) continue;
        scenes[i].name    = e->getStringAttribute("name", "Empty");
        scenes[i].namPath = e->getStringAttribute("nam");
        scenes[i].irPath  = e->getStringAttribute("ir");
        scenes[i].namPath2= e->getStringAttribute("nam2");
        scenes[i].irPath2 = e->getStringAttribute("ir2");
        if (auto* px = e->getFirstChildElement())
            scenes[i].params = juce::ValueTree::fromXml(*px);
    }
}
void ArcaneEclipseEditor::deleteScene(int slot){
    scenes[slot]=SceneData();
    if(activeScene==slot) activeScene=-1;
    savePresets();
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
                savePresets();
            }
            renameWindow.reset();
            refreshSceneButtons(); repaint();
        }), false);
}

// ── Scene save/load ───────────────────────────────────────────────────────────
void ArcaneEclipseEditor::saveScene(int slot){
    scenes[slot].namPath  = proc.getNAMPath(0);
    scenes[slot].irPath   = proc.getIRPath(0);
    scenes[slot].namPath2 = proc.getNAMPath(1);
    scenes[slot].irPath2  = proc.getIRPath(1);
    scenes[slot].params   = proc.apvts.copyState();
    if(scenes[slot].name=="Empty") scenes[slot].name = slotCode(slot);
    activeScene=slot;
    savePresets();
}
void ArcaneEclipseEditor::resetToDefault(){
    for (auto* prm : proc.getParameters())
        prm->setValueNotifyingHost(prm->getDefaultValue());
    for(int a=0;a<ArcaneEclipseProcessor::kNumAmpSlots;++a){
        if(proc.isNAMLoaded(a)) proc.unloadNAMModel(a);
        if(proc.isIRLoaded(a))  proc.unloadIR(a);
    }
    editAmp=0; updateDualUI();
    repaint();
}
void ArcaneEclipseEditor::loadScene(int slot){
    if(scenes[slot].isEmpty()){ resetToDefault(); activeScene=slot; return; }
    if(scenes[slot].params.isValid()){
        proc.apvts.replaceState(scenes[slot].params);
        // Scenes saved before v1.1 carry no Dual settings: start them in single-amp mode
        if(! scenes[slot].params.getChildWithProperty("id",ArcaneEclipseProcessor::idDualOn).isValid())
            if(auto* d=proc.apvts.getParameter(ArcaneEclipseProcessor::idDualOn)) d->setValueNotifyingHost(0.f);
    }
    // Recall each amp's model (.nam or .aecap - an .aecap may carry its own IR,
    // in which case its IR path equals its model path) and its separate IR.
    const juce::String namP[2]={scenes[slot].namPath, scenes[slot].namPath2};
    const juce::String irP [2]={scenes[slot].irPath,  scenes[slot].irPath2};
    for(int a=0;a<ArcaneEclipseProcessor::kNumAmpSlots;++a){
        if(namP[a]!=proc.getNAMPath(a)){
            juce::String err;
            if(namP[a].isNotEmpty() && juce::File(namP[a]).existsAsFile()) proc.loadModelAny(juce::File(namP[a]),a,err);
            else proc.unloadNAMModel(a);
        }
        if(irP[a]!=proc.getIRPath(a)){
            if(irP[a].isNotEmpty() && irP[a]!=namP[a] && juce::File(irP[a]).existsAsFile()) proc.loadIR(juce::File(irP[a]),a);
            else if(irP[a].isEmpty()) proc.unloadIR(a);
        }
    }
    activeScene=slot;
    updateDualUI();
}

// ── Model loading: .nam or .aecap into the amp currently being edited ─────────
int ArcaneEclipseEditor::editSlot() const {
    bool dualOn = proc.apvts.getRawParameterValue(ArcaneEclipseProcessor::idDualOn)->load() > .5f;
    return dualOn ? editAmp : 0;
}
void ArcaneEclipseEditor::loadModelFile(const juce::File& f){
    juce::String err;
    if(! proc.loadModelAny(f, editSlot(), err) && err.isNotEmpty())
        juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Arcane Eclipse", "Could not open " + f.getFileName() + ":\n" + err);
    repaint();
}
void ArcaneEclipseEditor::updateDualUI(){
    if(tunerVisible) return;                      // tuner owns visibility while open
    bool on = proc.apvts.getRawParameterValue(ArcaneEclipseProcessor::idDualOn)->load() > .5f;
    if(!on) editAmp=0;
    for(auto& b:ampSelBtn) if(b.isVisible()!=on) b.setVisible(on);
    if(dualMix.isVisible()!=on) dualMix.setVisible(on);
}

// ── Per-slot export / import (.aetone = a single-slot preset file) ──────────────
void ArcaneEclipseEditor::exportScene(int slot){
    if(slot<0 || slot>=kNumScenes || scenes[slot].isEmpty()) return;
    juce::ValueTree t("AETONE");
    t.setProperty("name", scenes[slot].name, nullptr);
    t.setProperty("nam",  scenes[slot].namPath, nullptr);
    t.setProperty("ir",   scenes[slot].irPath, nullptr);
    t.setProperty("nam2", scenes[slot].namPath2, nullptr);
    t.setProperty("ir2",  scenes[slot].irPath2, nullptr);
    if(scenes[slot].params.isValid()) t.appendChild(scenes[slot].params.createCopy(), nullptr);
    auto xml = t.toXmlString();
    chooserExport=std::make_unique<juce::FileChooser>("Export Tone",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile(scenes[slot].name + ".aetone"), "*.aetone");
    chooserExport->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles,
        [xml](const juce::FileChooser& fc){ auto r=fc.getResult();
            if(r != juce::File()) r.replaceWithText(xml); });
}
void ArcaneEclipseEditor::importScene(int slot){
    if(slot<0 || slot>=kNumScenes) return;
    chooserImport=std::make_unique<juce::FileChooser>("Import Tone",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.aetone");
    chooserImport->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
        [this,slot](const juce::FileChooser& fc){ auto r=fc.getResult();
            if(r == juce::File() || !r.existsAsFile()) return;
            auto xml = juce::XmlDocument::parse(r);
            if(xml==nullptr) return;
            auto t = juce::ValueTree::fromXml(*xml);
            if(!t.isValid() || !t.hasType("AETONE")) return;
            scenes[slot].name    = t.getProperty("name", slotCode(slot)).toString();
            scenes[slot].namPath = t.getProperty("nam","").toString();
            scenes[slot].irPath  = t.getProperty("ir","").toString();
            scenes[slot].namPath2= t.getProperty("nam2","").toString();
            scenes[slot].irPath2 = t.getProperty("ir2","").toString();
            if(t.getNumChildren()>0) scenes[slot].params = t.getChild(0).createCopy();
            savePresets(); loadScene(slot); refreshSceneButtons(); repaint();
        });
}

// ── Layout ────────────────────────────────────────────────────────────────────
// ── v1.1 re-skin: layout map (render px; see SX/SY/RR at top of file) ─────────
// Chain-node boxes, measured off the render (left edges, 77 x 64 px each)
static const float kNodeX[9] = { 358, 449, 541, 629, 720, 809, 899, 987, 1078 };
static constexpr float kNodeY0 = 109.f, kNodeY1 = 173.f, kNodeW = 77.f;
// Knob centres + diameters (fitted against the concept render)
static const float kStripXY[4][2] = { {85,144}, {183,143}, {1315,143}, {1425,144} };
static constexpr float kStripD = 58.f;
static const float kAmpX[6] = { 192, 417, 646, 870, 1096, 1319 };
static constexpr float kAmpY = 484.f, kAmpD = 72.f;
static const float kPedXY[12][2] = {          // row 1 (2 per pedal) then row 2 (MIX/LEVEL)
    {84,666},{196,666}, {345,666},{455,666}, {606,666},{720,666}, {869,666},{981,666},
    {140,750},{400,750},{663,750},{923,750} };
static constexpr float kPedD = 50.f;
// Pedals: card spans, footswitch / LED centres
static const float kCardX[4][2] = { {22,258}, {284,520}, {545,781}, {806,1042} };
static const float kFootX[4]    = { 139, 400, 661, 922 };
static constexpr float kFootY = 855.f, kFootR = 26.f, kLedY = 817.f;
// Scene bar slots
static const float kSlotX[4][2] = { {152,447}, {466,748}, {765,1048}, {1067,1360} };
static constexpr float kSlotY0 = 918.f, kSlotY1 = 964.f;

juce::Rectangle<int> ArcaneEclipseEditor::chainNodeBounds(int i) const
{
    return RR(kNodeX[i], kNodeY0, kNodeX[i]+kNodeW, kNodeY1);
}

void ArcaneEclipseEditor::resized()
{
    auto placeR=[](AEKnob& k,float x,float y,float d){
        k.place(juce::roundToInt(SX(x)), juce::roundToInt(SY(y)), juce::roundToInt(SX(d)), false);
    };

    // Strip knobs
    AEKnob* strip[4]={&kInput,&kGate,&kComp,&kOutput};
    for(int i=0;i<4;++i) placeR(*strip[i],kStripXY[i][0],kStripXY[i][1],kStripD);

    // Amp faceplate
    AEKnob* amp[6]={&kGain,&kBass,&kMid,&kTreble,&kPresence,&kMaster};
    for(int i=0;i<6;++i) placeR(*amp[i],kAmpX[i],kAmpY,kAmpD);

    // Pedals: OD drive/tone, MOD rate/depth, DELAY time/fb, REVERB decay/size, then the 4 lower knobs
    AEKnob* ped[12]={&kODDrive,&kODTone,&kModRate,&kModDepth,&kDTime,&kDFeedback,&kRDecay,&kRSize,
                     &kODLevel,&kModMix,&kDMix,&kRMix};
    for(int i=0;i<12;++i) placeR(*ped[i],kPedXY[i][0],kPedXY[i][1],kPedD);
    // Reverb HIGH CUT has no position in the v1.1 render — hidden (param keeps its value)
    kRHiCut.slider.setBounds(-100,-100,1,1); kRHiCut.slider.setVisible(false);

    // Chain node toggles (invisible, over the baked nodes)
    tbGate.setBounds(chainNodeBounds(0));      tbComp.setBounds(chainNodeBounds(1));
    stompOD.setBounds(chainNodeBounds(2));     stompMod.setBounds(chainNodeBounds(6));
    stompDelay.setBounds(chainNodeBounds(7));  stompReverb.setBounds(chainNodeBounds(8));

    // Pedal footswitches + type hotspots (pedal titles)
    juce::ToggleButton* fs[4]={&fsOD,&fsMod,&fsDelay,&fsReverb};
    for(int i=0;i<4;++i) fs[i]->setBounds(RR(kFootX[i]-kFootR,kFootY-kFootR,kFootX[i]+kFootR,kFootY+kFootR));
    for(int i=0;i<3;++i) typeBtn[i].setBounds(RR(kFootX[i+1]-52,795,kFootX[i+1]+52,810));   // type pill

    // Cab / loader panel
    fieldModel  .setBounds(RR(1291,649,1471,687));
    fieldIR     .setBounds(RR(1291,707,1471,745));
    dualBtn     .setBounds(RR(1291,763,1470,800));
    // Dual row (shown only while Dual is on): [1] ---MIX--- [2]
    ampSelBtn[0].setBounds(RR(1291,806,1323,832));
    ampSelBtn[1].setBounds(RR(1438,806,1470,832));
    dualMix     .setBounds(RR(1330,806,1431,832));
    btnLoadModel.setBounds(RR(1087,840,1271,877));
    btnLoadIR   .setBounds(RR(1291,840,1469,877));

    // Scene bar
    bankPrev.setBounds(RR(24,kSlotY0,134,kSlotY1));
    for(int i=0;i<4;++i) sceneBtn[i].setBounds(RR(kSlotX[i][0],kSlotY0,kSlotX[i][1],kSlotY1));
    bankNext.setBounds(RR(1379,kSlotY0,1489,kSlotY1));

    // Header: preset arrows, SAVE, tuner (fork icon), help (?)
    presetPrev.setBounds(RR(545,12,592,54));
    presetNext.setBounds(RR(870,12,917,54));
    headerSave.setBounds(RR(938,13,1017,50));
    tbTuner   .setBounds(RR(1325,13,1361,50));
    if(helpBtn) helpBtn->setBounds(RR(1418,13,1454,50));   // resized() first runs from setSize(), before helpBtn exists

    // Footer STEREO/MONO toggle
    stereoBtn.setBounds(RR(1270,996,1342,1020));

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
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(bg,0,0,W,H,0,0,bg.getWidth(),bg.getHeight());
    }
    paintHeaderLive(g);
    paintVU(g,RF(18,100,34,198),vuIn);
    paintVU(g,RF(1480,100,1496,198),vuOut);
    paintChainLive(g);
    paintPedalsLive(g);
    paintCabLive(g);
    paintScenesLive(g);
    paintFooterLive(g);
}

void ArcaneEclipseEditor::paintOverChildren(juce::Graphics& g)
{
    if(tunerVisible) return;

    // Value readout on the knob face while hovering / dragging
    for(auto* k:allKnobs){
        auto& s=k->slider;
        if(!s.isVisible() || !(s.isMouseOverOrDragging())) continue;
        auto b=s.getBounds();
        float fs=juce::jlimit(8.f,11.f,b.getWidth()*0.22f);
        haloText(g,s.getTextFromValue(s.getValue()),juce::Font(fs).boldened(),juce::Colours::white,
                 b.withSizeKeepingCentre(b.getWidth()+20,(int)fs+4),juce::Justification::centred);
    }

    if(dualMix.isVisible() && dualMix.isMouseOverOrDragging()){
        auto lb=dualMix.getBounds().withHeight(juce::roundToInt(SY(11.f))).translated(0,-juce::roundToInt(SY(3.f)));
        haloText(g,dualMix.getTextFromValue(dualMix.getValue()),juce::Font(SY(11.f)).boldened(),juce::Colours::white,
                 lb.expanded(10,0),juce::Justification::centred);
    }

    if(learningID.isEmpty() && learningAction<0) return;
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
    if(learningAction>=0){
        juce::Component* ac=nullptr;
        if(learningAction>=0 && learningAction<=3) ac=&sceneBtn[learningAction];
        else if(learningAction==4) ac=&bankPrev;
        else if(learningAction==5) ac=&bankNext;
        else if(learningAction==6) ac=&presetPrev;
        else if(learningAction==7) ac=&presetNext;
        if(ac!=nullptr){
            auto nb=ac->getBounds();
            g.setColour(kPurple.withAlpha(0.35f+0.55f*pulse));
            g.drawRoundedRectangle(nb.toFloat().expanded(2.f),6.f,2.5f);
            haloText(g,"LEARN",juce::Font(8.f).boldened(),kPurple,
                     {nb.getX()-6,nb.getY()-13,nb.getWidth()+12,12},juce::Justification::centred);
        }
    }
}

// Purple LED glow (on-state) centred on a render-px point
static void ledGlow(juce::Graphics& g,float rx,float ry,float rr)
{
    float cx=SX(rx), cy=SY(ry), r=SX(rr);
    g.setColour(kPurple.withAlpha(0.28f)); g.fillEllipse(cx-r*2.2f,cy-r*2.2f,r*4.4f,r*4.4f);
    g.setColour(kPurple);                  g.fillEllipse(cx-r,cy-r,r*2.f,r*2.f);
    g.setColour(juce::Colour(0xffefe0ff)); g.fillEllipse(cx-r*0.45f,cy-r*0.45f,r*0.9f,r*0.9f);
}

void ArcaneEclipseEditor::paintHeaderLive(juce::Graphics& g)
{
    // Preset name inside the baked preset box
    juce::String pn = (activeScene>=0 && !scenes[activeScene].isEmpty())
                      ? slotCode(activeScene)+"   "+scenes[activeScene].name : juce::String("No Preset");
    haloText(g,pn,juce::Font(SY(17.f)).boldened(),kText,RR(600,14,862,40),juce::Justification::centred);

    // Bank dots: the render has 7; v1.1 has 8 banks, so repaint the row
    g.setColour(kPresetFill); g.fillRect(RF(690,40,810,55));
    for(int i=0;i<kNumBanks;++i){
        float x=SX(749.5f+(i-(kNumBanks-1)*0.5f)*15.5f), y=SY(47.f), r=SX(2.6f);
        g.setColour(i==currentBank?kPurple:juce::Colour(0x66bcbce2));
        g.fillEllipse(x-r,y-r,2*r,2*r);
    }
}

void ArcaneEclipseEditor::paintVU(juce::Graphics& g,juce::Rectangle<float> b,float lvl)
{
    // Lights the render's 14 baked meter segments from the bottom up
    constexpr int n=14;
    int lit=(int)std::round(n*juce::jlimit(0.f,1.f,lvl));
    float pitch=b.getHeight()/n, sh=pitch*0.68f;
    for(int i=0;i<lit;++i){
        float sy=b.getBottom()-(i+1)*pitch+(pitch-sh)*0.5f;
        auto c = i>=12 ? kRed : (i>=9 ? juce::Colour(0xffc9a3ff) : kPurple);
        g.setColour(c.withAlpha(0.25f)); g.fillRect(b.getX()-1.f,sy-1.f,b.getWidth()+2.f,sh+2.f);
        g.setColour(c);                  g.fillRect(b.getX()+0.5f,sy,b.getWidth()-1.f,sh);
    }
}

void ArcaneEclipseEditor::paintChainLive(juce::Graphics& g)
{
    bool act[9]={ tbGate.getToggleState(), tbComp.getToggleState(), stompOD.getToggleState(),
                  proc.isNAMLoaded(0) || (proc.isDualActive() && proc.isNAMLoaded(1)),
                  proc.isIRLoaded(0)  || (proc.isDualActive() && proc.isIRLoaded(1)), true,
                  stompMod.getToggleState(), stompDelay.getToggleState(), stompReverb.getToggleState() };
    // Baked indicator bulbs on GATE, COMP, DELAY, REVERB
    static const float ledX[9]={396,487,-1,-1,-1,-1,-1,1025,1117};

    for(int i=0;i<9;++i){
        auto r=chainNodeBounds(i).toFloat().reduced(1.f);
        if(act[i]){
            g.setColour(kPurple.withAlpha(0.10f)); g.fillRoundedRectangle(r,6.f);
            g.setColour(kPurple.withAlpha(0.30f)); g.drawRoundedRectangle(r.expanded(1.5f),7.f,3.f);
            g.setColour(kPurple);                  g.drawRoundedRectangle(r,6.f,1.4f);
            if(ledX[i]>0) ledGlow(g,ledX[i],117.f,3.2f);
        } else {
            g.setColour(juce::Colours::black.withAlpha(0.38f)); g.fillRoundedRectangle(r,6.f);  // dim = off
        }
        if(i<8 && act[i] && act[i+1]){
            float y=SY(141.f), x0=SX(kNodeX[i]+kNodeW), x1=SX(kNodeX[i+1]);
            g.setColour(kPurple); g.drawLine(x0,y,x1,y,1.6f);
        }
    }
}

juce::String ArcaneEclipseEditor::typeName(int fx) const
{
    static const char* mod[3]={"CHORUS","FLANGER","PHASER"};
    static const char* dly[4]={"DIGITAL","ANALOG","TAPE","ECHO"};
    static const char* rvb[4]={"ROOM","HALL","PLATE","SPRING"};
    auto get=[this](const char* id){ return (int) proc.apvts.getRawParameterValue(id)->load(); };
    if(fx==0) return mod[juce::jlimit(0,2,get(ArcaneEclipseProcessor::idModType))];
    if(fx==1) return dly[juce::jlimit(0,3,get(ArcaneEclipseProcessor::idDelayType))];
    juce::String s=rvb[juce::jlimit(0,3,get(ArcaneEclipseProcessor::idReverbType))];
    if(proc.apvts.getRawParameterValue(ArcaneEclipseProcessor::idReverbShimmer)->load()>0.5f) s+=" + SHIMMER";
    return s;
}

void ArcaneEclipseEditor::showTypeMenu(int fx)
{
    static const char* names[3][4]={{"Chorus","Flanger","Phaser",nullptr},
                                    {"Digital","Analog","Tape","Echo"},
                                    {"Room","Hall","Plate","Spring"}};
    const char* ids[3]={ArcaneEclipseProcessor::idModType,ArcaneEclipseProcessor::idDelayType,
                        ArcaneEclipseProcessor::idReverbType};
    int cur=(int) proc.apvts.getRawParameterValue(ids[fx])->load();
    juce::PopupMenu m;
    m.addSectionHeader(fx==0?"Modulation type":fx==1?"Delay type":"Reverb type");
    for(int i=0;i<4 && names[fx][i]!=nullptr;++i) m.addItem(i+1,names[fx][i],true,i==cur);
    if(fx==2){
        bool sh=proc.apvts.getRawParameterValue(ArcaneEclipseProcessor::idReverbShimmer)->load()>0.5f;
        m.addSeparator();
        m.addItem(100,"Shimmer (octave-up)",true,sh);
    }
    auto setParam=[this](const char* id,float v){
        if(auto* p=proc.apvts.getParameter(id)){
            p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(v)); p->endChangeGesture();
        }
    };
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&typeBtn[fx]),
        [this,fx,setParam,id=ids[fx]](int r){
            if(r>=1 && r<=4) setParam(id,(float)(r-1));
            else if(r==100){
                bool sh=proc.apvts.getRawParameterValue(ArcaneEclipseProcessor::idReverbShimmer)->load()>0.5f;
                setParam(ArcaneEclipseProcessor::idReverbShimmer,sh?0.f:1.f);
            }
            juce::ignoreUnused(fx);
            repaint();
        });
}

void ArcaneEclipseEditor::paintPedalsLive(juce::Graphics& g)
{
    bool on[4]={ stompOD.getToggleState(), stompMod.getToggleState(),
                 stompDelay.getToggleState(), stompReverb.getToggleState() };
    for(int i=0;i<4;++i){
        if(on[i]) ledGlow(g,kFootX[i],kLedY,5.f);
        if(i>0){  // effect-type pill (click = type menu), between the MIX label and the LED
            auto r=RF(kFootX[i]-52,795,kFootX[i]+52,810);
            g.setColour(juce::Colour(0xcc0c0912)); g.fillRoundedRectangle(r,r.getHeight()*0.5f);
            g.setColour(kPurple.withAlpha(0.55f)); g.drawRoundedRectangle(r.reduced(0.5f),r.getHeight()*0.5f,1.f);
            g.setFont(juce::Font(SY(10.f)).boldened()); g.setColour(juce::Colour(0xffe6d2ff));
            g.drawText(typeName(i-1),r.withTrimmedRight(SX(12.f)),juce::Justification::centred,false);
            juce::Path c; float ax=r.getRight()-SX(11.f), ay=r.getCentreY();
            c.addTriangle(ax-2.6f,ay-1.4f,ax+2.6f,ay-1.4f,ax,ay+1.8f);
            g.setColour(kPurple); g.fillPath(c);
        }
    }
}

void ArcaneEclipseEditor::showFieldMenu(bool ir)
{
    const int a = editSlot();
    bool loaded = ir ? proc.isIRLoaded(a) : proc.isNAMLoaded(a);
    juce::PopupMenu m;
    if(loaded) m.addSectionHeader(ir ? proc.getLoadedIRName(a) : proc.getLoadedNAMName(a));
    const bool dualOn = proc.apvts.getRawParameterValue(ArcaneEclipseProcessor::idDualOn)->load() > .5f;
    juce::String who = dualOn ? (a==0 ? " (amp 1)" : " (amp 2)") : juce::String();
    m.addItem(1, (ir ? "Load IR..." : "Load model...") + who);
    m.addItem(2, (ir ? "Clear IR" : "Clear model") + who, loaded);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(ir ? &fieldIR : &fieldModel),
        [this,ir,a](int r){
            if(r==1){ if(ir) btnLoadIR.onClick(); else btnLoadModel.onClick(); }
            else if(r==2){ if(ir) proc.unloadIR(a); else proc.unloadNAMModel(a); repaint(); }
        });
}

void ArcaneEclipseEditor::paintCabLive(juce::Graphics& g)
{
    const bool dualOn = proc.apvts.getRawParameterValue(ArcaneEclipseProcessor::idDualOn)->load() > .5f;
    const int a = editSlot();

    // MODEL / IR names for the amp being edited
    auto f=juce::Font(SY(13.f));
    juce::String mv=proc.isNAMLoaded(a)?proc.getLoadedNAMName(a):juce::String("No model loaded");
    haloText(g,mv,f,proc.isNAMLoaded(a)?kText:kMuted,RR(1302,667,1440,685),juce::Justification::centredLeft);
    juce::String iv=proc.isIRLoaded(a)?proc.getLoadedIRName(a):juce::String("No IR loaded");
    haloText(g,iv,f,proc.isIRLoaded(a)?kText:kMuted,RR(1302,726,1440,744),juce::Justification::centredLeft);

    // Amp-number chips next to the baked MODEL / IR labels (dual only)
    auto chip=[&](float rx,float ry){
        auto r=RF(rx,ry-6.5f,rx+15.f,ry+6.5f);
        g.setColour(kPurple); g.fillRoundedRectangle(r,r.getHeight()*0.5f);
        g.setColour(juce::Colours::white); g.setFont(juce::Font(SY(10.5f)).boldened());
        g.drawText(juce::String(a+1),r,juce::Justification::centred,false);
    };
    if(dualOn){ chip(1349.f,661.f); chip(1317.f,720.f); }

    // DUAL AMP/IR button: the render shows it "on"; dim it + its lamp when off
    if(dualOn) ledGlow(g,1446.f,782.f,4.2f);
    else {
        auto r=RF(1291,763,1470,800);
        g.setColour(juce::Colours::black.withAlpha(0.45f)); g.fillRoundedRectangle(r,SX(7.f));
        g.setColour(juce::Colour(0xff15121c)); g.fillEllipse(RF(1441,777,1451,787));
    }

    // [1] --MIX-- [2]
    if(dualOn){
        for(int i=0;i<2;++i){
            auto r=ampSelBtn[i].getBounds().toFloat().reduced(0.5f);
            bool sel=(editAmp==i);
            bool has=proc.isNAMLoaded(i)||proc.isIRLoaded(i);
            g.setColour(sel?kPurple.withAlpha(0.30f):juce::Colour(0xcc0c0912)); g.fillRoundedRectangle(r,5.f);
            g.setColour(sel?kPurple:kPurple.withAlpha(0.40f));                  g.drawRoundedRectangle(r,5.f,sel?1.5f:1.f);
            g.setFont(juce::Font(SY(15.f)).boldened());
            g.setColour(sel?juce::Colours::white:(has?juce::Colour(0xffd8c8ec):kMuted));
            g.drawText(juce::String(i+1),r,juce::Justification::centred,false);
        }
        if(! dualMix.isMouseOverOrDragging())      // value replaces the label while adjusting
            haloText(g,"MIX",juce::Font(SY(9.5f)).boldened(),kMuted,
                     dualMix.getBounds().withHeight(juce::roundToInt(SY(11.f))).translated(0,-juce::roundToInt(SY(3.f))),
                     juce::Justification::centred);
    }
}

void ArcaneEclipseEditor::paintScenesLive(juce::Graphics& g)
{
    for(int i=0;i<4;++i){
        auto r=RF(kSlotX[i][0],kSlotY0,kSlotX[i][1],kSlotY1).reduced(1.f);
        int idx=currentBank*kSlotsPerBank+i;
        bool active=(activeScene==idx);
        if(active){
            g.setColour(kPurple.withAlpha(0.14f)); g.fillRoundedRectangle(r,6.f);
            g.setColour(kPurple.withAlpha(0.30f)); g.drawRoundedRectangle(r.expanded(1.5f),7.f,3.f);
            g.setColour(kPurple);                  g.drawRoundedRectangle(r,6.f,1.5f);
        }
        auto ri=r.toNearestInt();
        haloText(g,slotCode(idx),juce::Font(SY(14.f)).boldened(),kPurple,
                 ri.withTrimmedLeft(12).withWidth(34),juce::Justification::centredLeft);
        bool empty=scenes[idx].isEmpty();
        haloText(g,empty?juce::String("Empty"):scenes[idx].name,juce::Font(SY(15.f)).boldened(),
                 empty?kMuted.withAlpha(0.55f):kText,ri.reduced(46,0),juce::Justification::centred);
    }
}

void ArcaneEclipseEditor::paintFooterLive(juce::Graphics& g)
{
    // STEREO / MONO (repaint over the baked word so it can change)
    bool st=stereoBtn.getToggleState();
    g.setColour(kFooterFill); g.fillRect(RF(1270,998,1342,1019));
    g.setFont(juce::Font(SY(14.f)).boldened());
    g.setColour(st?kPurple:kMuted);
    g.drawText(st?"STEREO":"MONO",RR(1262,998,1336,1019),juce::Justification::centredRight,false);
    // AMP / CAB status lamps (baked rings light up when a model / IR is loaded)
    bool d2=proc.isDualActive();
    if(proc.isNAMLoaded(0) || (d2 && proc.isNAMLoaded(1))) ledGlow(g,1365.f,1008.f,4.f);
    if(proc.isIRLoaded(0)  || (d2 && proc.isIRLoaded(1)))  ledGlow(g,1443.f,1008.f,4.f);
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
