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
                             float pos,float,float,juce::Slider::SliderStyle style,juce::Slider& sl)
{
    if(style==juce::Slider::LinearVertical){
        // Dual amp level fader: slot + 0 dB mark + cap
        float cx=x+w*0.5f;
        auto slot=juce::Rectangle<float>(cx-2.5f,(float)y+5.f,5.f,(float)h-10.f);
        for(double v : {-24.0,-12.0,12.0,24.0}){                         // scale ticks
            float ty=(float)sl.getPositionOfValue(v);
            g.setColour(kMuted.withAlpha(0.35f)); g.drawLine(cx-8.f,ty,cx-5.f,ty,1.f); g.drawLine(cx+5.f,ty,cx+8.f,ty,1.f);
        }
        g.setColour(juce::Colour(0xff3a3346)); g.fillRoundedRectangle(slot,2.5f);
        g.setColour(juce::Colour(0xff050408)); g.fillRoundedRectangle(slot.reduced(1.2f),2.f);
        float zeroY=(float)sl.getPositionOfValue(0.0);
        g.setColour(kMuted.withAlpha(0.8f)); g.drawLine(cx-11.f,zeroY,cx+11.f,zeroY,1.2f);
        juce::ColourGradient cg(kPurple,cx,zeroY,juce::Colour(0xffc9a3ff),cx,pos,false);
        g.setGradientFill(cg); g.fillRect(juce::Rectangle<float>(cx-1.5f,std::min(zeroY,pos),3.f,std::abs(pos-zeroY)));
        auto cap=juce::Rectangle<float>(cx-11.f,pos-5.f,22.f,10.f);
        g.setColour(kPurple.withAlpha(0.30f)); g.fillRoundedRectangle(cap.expanded(2.f),4.f);
        g.setColour(juce::Colour(0xff2a2433)); g.fillRoundedRectangle(cap,3.f);
        g.setColour(kPurple); g.drawRoundedRectangle(cap,3.f,1.2f);
        g.setColour(juce::Colour(0xffefe0ff)); g.drawLine(cap.getX()+4.f,pos,cap.getRight()-4.f,pos,1.2f);
        return;
    }
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
    "\f"                                   // column break
    "math_approx\n"
    "  Copyright (c) 2024 jatinchowdhury18\n"
    "  BSD 3-Clause License\n\n"
    "Eigen\n"
    "  Eigen Contributors\n"
    "  Mozilla Public License 2.0\n\n"
    "nlohmann/json\n"
    "  Copyright (c) 2013-2025 Niels Lohmann\n"
    "  MIT License\n\n"
    "Cinzel font\n"
    "  Copyright (c) 2020 The Cinzel Project Authors\n"
    "  SIL Open Font License 1.1\n\n"
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
    g.setColour(juce::Colour(0xff100d17)); g.fillRoundedRectangle(b,12.f);
    g.setColour(kPurple); g.drawRoundedRectangle(b.reduced(0.5f),12.f,1.8f);
    g.setColour(kPurple.withAlpha(0.15f)); g.fillRoundedRectangle(b.withHeight(46),12.f);
    g.setFont(juce::Font(15.f).boldened()); g.setColour(kText);
    g.drawText("CREDITS & LICENSES",getLocalBounds().withHeight(46),juce::Justification::centred);
    g.setColour(kCardBd); g.drawHorizontalLine(46,16.f,(float)getWidth()-16);
    // two columns so the whole list fits without scrolling
    const auto cols = juce::StringArray::fromTokens(juce::String(kCreditsText), "\f", "");
    const int colW = (getWidth() - 60) / 2;
    for (int i = 0; i < cols.size() && i < 2; ++i) {
        juce::Rectangle<int> area(22 + i * (colW + 16), 58, colW, getHeight() - 104);
        juce::AttributedString as; as.setWordWrap(juce::AttributedString::byWord);
        as.setJustification(juce::Justification::topLeft);
        as.setLineSpacing(1.5f);
        as.append(cols[i], juce::Font(juce::FontOptions(12.f)), juce::Colour(0xffe2deef));
        juce::TextLayout tl; tl.createLayout(as,(float)area.getWidth());
        tl.draw(g,area.toFloat());
    }
    g.setColour(kCardBd); g.fillRect(getWidth()/2, 64, 1, getHeight()-120);
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
    stereoBtn.setClickingTogglesState(false);                  // v1.1.1: opens Mono/Stereo/Doubler menu
    stereoBtn.setTooltip("Output: mono, stereo, or stereo with the doubler (double-tracked width)");
    stereoBtn.onClick = [this]{ showStereoMenu(); };

    // Header power icon = global bypass; [ ] icon = A/B compare (v1.1.1)
    makeHotspot(powerBtn,6.f); powerBtn.setTooltip("Bypass the whole rig (hear your dry guitar)");
    powerBtn.onClick=[this]{ proc.setGlobalBypass(! proc.isGlobalBypassed()); repaint(); };
    addAndMakeVisible(powerBtn);
    makeHotspot(abBtn,6.f); abBtn.setTooltip("A/B compare: click to flip between two versions of this patch (right-click for options)");
    abBtn.addMouseListener(this,false);            // left = flip, right = menu (handled in mouseDown)
    addAndMakeVisible(abBtn);

    // Dual: per-amp level faders + MATCH (shown in the speaker grille)
    for(int i=0;i<2;++i){
        trimSl[i].setLookAndFeel(&laf);
        trimSl[i].setMouseCursor(juce::MouseCursor::PointingHandCursor);
        trimSl[i].setDoubleClickReturnValue(true,0.0);
        trimSl[i].setSliderSnapsToMousePosition(false);
        trimSl[i].setTooltip(i==0?"Amp 1 level (double-click = 0 dB)":"Amp 2 level (double-click = 0 dB)");
        attTrim[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            p.apvts, i==0?ArcaneEclipseProcessor::idAmp1Trim:ArcaneEclipseProcessor::idAmp2Trim, trimSl[i]);
        addChildComponent(trimSl[i]);
    }
    makeHotspot(matchBtn,4.f);
    matchBtn.setTooltip("Level-match the two amps (play for a few seconds first)");
    matchBtn.onClick=[this]{ matchAmpLevels(); };
    addChildComponent(matchBtn);

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
    // DELAY footswitch is handled by hand (v1.1.1): normally on/off; in tap mode a
    // press = TAP and a long hold = on/off (see mouseDown / mouseUp)
    fsDelay.setClickingTogglesState(false);
    fsDelay.addMouseListener(this,false);
    attFsReverb=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,ArcaneEclipseProcessor::idReverbOn,fsReverb);

    // v1.1: effect TYPE selection — the small pill under MIX on MOD / DELAY / REVERB
    for(int i=0;i<3;++i){
        makeHotspot(typeBtn[i],6.f);
        typeBtn[i].setTooltip("Click to choose the effect type");
        typeBtn[i].onClick=[this,i]{ showTypeMenu(i); };
        addAndMakeVisible(typeBtn[i]);
    }
    // OVERDRIVE: pedal-capture pill (load a NAM pedal capture / back to built-in)
    makeHotspot(odBtn,6.f);
    odBtn.setTooltip("Load a NAM pedal capture into the Overdrive (replaces the built-in drive)");
    odBtn.onClick=[this]{ showODMenu(); };
    addAndMakeVisible(odBtn);

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
    dualMix.setDoubleClickReturnValue(true,0.5);       // double-click = 50/50
    dualMix.setSliderSnapsToMousePosition(false);      // drag is relative: clicks never jump the blend
    attDualMix=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.apvts,ArcaneEclipseProcessor::idDualMix,dualMix);
    addChildComponent(dualMix);
    dualMix.addMouseListener(this,false);          // right-click -> MIDI learn (expression pedal)

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

    // License gate: licensed users see nothing; first run offers the 7-day trial;
    // during the trial a reminder shows once per session; after it, the gate stays.
    makeHotspot(trialBtn,6.f);
    trialBtn.setTooltip("Free trial: click to buy or activate a license");
    trialBtn.onClick=[this]{ showLicenseGate(AEActivationDialog::Mode::TrialActive); };
    addChildComponent(trialBtn);
    AELicenseManager::getInstance().refresh();
    licAccess = AELicenseManager::getInstance().getAccess();
    trialBtn.setVisible(licAccess == AEAccess::TrialActive);

    // Startup screen first (once per plugin load), then any license / trial screen.
    if (!proc.splashShown) { proc.splashShown = true; showSplash(); }
    else                   maybeShowLicenseGate();
}

void ArcaneEclipseEditor::maybeShowLicenseGate()
{
    licAccess = AELicenseManager::getInstance().getAccess();
    static bool trialReminderShown = false;              // once per host session, not every window open
    if (licAccess == AEAccess::TrialAvailable)      showLicenseGate(AEActivationDialog::Mode::Welcome);
    else if (licAccess == AEAccess::TrialExpired)   showLicenseGate(AEActivationDialog::Mode::Expired);
    else if (licAccess == AEAccess::TrialActive && !trialReminderShown) {
        trialReminderShown = true;
        showLicenseGate(AEActivationDialog::Mode::TrialActive);
    }
    trialBtn.setVisible(licAccess == AEAccess::TrialActive && !tunerVisible);
}

void ArcaneEclipseEditor::showSplash()
{
    splash = std::make_unique<AESplash>();
    splash->setBounds(getLocalBounds());
    splash->onDismissed = [this]{
        juce::Component::SafePointer<ArcaneEclipseEditor> safe(this);
        juce::MessageManager::callAsync([safe]{
            if (safe == nullptr) return;
            safe->splash.reset();
            safe->maybeShowLicenseGate();
            safe->repaint();
        });
    };
    addAndMakeVisible(*splash);
    splash->toFront(true);
}

void ArcaneEclipseEditor::showLicenseGate(AEActivationDialog::Mode m)
{
    if (activationDialog) { activationDialog->setMode(m); activationDialog->toFront(true); return; }
    activationDialog = std::make_unique<AEActivationDialog>(m);
    activationDialog->setBounds(getLocalBounds());
    activationDialog->onActivated  = [this]{ closeLicenseGate(); };
    activationDialog->onContinue   = [this]{ closeLicenseGate(); };
    activationDialog->onStartTrial = [this]{ AELicenseManager::getInstance().startTrial(); closeLicenseGate(); };
    addAndMakeVisible(*activationDialog);
    activationDialog->toFront(true);
}

void ArcaneEclipseEditor::closeLicenseGate()
{
    // deferred: this runs from inside one of the dialog's own button callbacks
    juce::Component::SafePointer<ArcaneEclipseEditor> safe(this);
    juce::MessageManager::callAsync([safe]{
        if (safe == nullptr) return;
        safe->activationDialog.reset();
        safe->updateLicenseState();
    });
}

void ArcaneEclipseEditor::updateLicenseState()
{
    auto prev = licAccess;
    licAccess = AELicenseManager::getInstance().getAccess();
    trialBtn.setVisible(licAccess == AEAccess::TrialActive && !tunerVisible);
    if (licAccess == AEAccess::TrialExpired && prev != AEAccess::TrialExpired)
        showLicenseGate(AEActivationDialog::Mode::Expired);   // trial ran out while open
    repaint();
}

ArcaneEclipseEditor::~ArcaneEclipseEditor(){stopTimer();setLookAndFeel(nullptr);}
void ArcaneEclipseEditor::timerCallback()
{
    if (++licCheckTick >= 30) { licCheckTick = 0; if (!activationDialog && !splash) updateLicenseState(); }
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
        // animation: eased needle, strobe drift proportional to the detune, signal fade
        double now = juce::Time::getMillisecondCounterHiRes();
        float dt = tunerLastTick > 0.0 ? (float) juce::jlimit(0.0, 0.1, (now - tunerLastTick) / 1000.0) : 1.f/30.f;
        tunerLastTick = now;
        bool sig = tunerHz > 0.f;
        tunerSignal    += ((sig ? 1.f : 0.f) - tunerSignal) * 0.25f;
        tunerDispCents += ((sig ? tunerCents : 0.f) - tunerDispCents) * 0.35f;
        if (sig && std::fabs(tunerCents) >= 1.f) tunerStrobe += tunerCents * 3.0f * dt;   // px/s per cent
        tunerStrobe = std::fmod(tunerStrobe, 1000.f);
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
    startTimerHz(v ? 30 : 15);                       // smooth needle + strobe while tuning
    tunerLastTick = 0.0;
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
bool ArcaneEclipseEditor::delayTapMode() const {
    return proc.apvts.getRawParameterValue(ArcaneEclipseProcessor::idDelayTapMode)->load() > .5f;
}
void ArcaneEclipseEditor::mouseUp(const juce::MouseEvent& e)
{
    if(e.eventComponent!=&fsDelay || e.mods.isPopupMenu() || tunerVisible) return;
    auto* on=proc.apvts.getParameter(ArcaneEclipseProcessor::idDelayOn);
    bool held = juce::Time::getMillisecondCounterHiRes()-fsDelayDownMs >= 600.0;
    if(! delayTapMode() || held){
        if(held && fsDelayTapped) proc.undoLastTap();       // the hold wasn't a tap
        on->beginChangeGesture(); on->setValueNotifyingHost(on->getValue()<.5f?1.f:0.f); on->endChangeGesture();
    }
    fsDelayTapped=false; repaint();
}
void ArcaneEclipseEditor::mouseDown(const juce::MouseEvent& e)
{
    if(e.eventComponent==&abBtn && !tunerVisible){ if(e.mods.isPopupMenu()) showABMenu(); else abToggle(); return; }
    if(e.eventComponent==&fsDelay && !e.mods.isPopupMenu() && !tunerVisible){
        fsDelayDownMs = juce::Time::getMillisecondCounterHiRes();
        fsDelayTapped = false;
        if(delayTapMode()){ proc.tapTempo(); fsDelayTapped=true; }   // tap on press = tight timing
        return;
    }
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
    juce::Slider* hitSlider=nullptr; juce::String pid;
    for(auto* k:allKnobs) if(&k->slider==e.eventComponent){ hitSlider=&k->slider; pid=k->paramID; break; }
    if(hitSlider==nullptr && e.eventComponent==&dualMix){ hitSlider=&dualMix; pid=ArcaneEclipseProcessor::idDualMix; }
    if(hitSlider==nullptr) return;
    int cc=proc.ccForParam(pid);
    juce::PopupMenu m;
    m.addItem(1, cc<0 ? "MIDI Learn" : "MIDI Learn (re-assign)");
    if(cc>=0) m.addItem(2, "Clear MIDI (CC "+juce::String(cc)+")");
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(hitSlider),
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
    int pw=640, ph=600;
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
        e->setAttribute("od",   scenes[i].odPath);
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
        scenes[i].odPath  = e->getStringAttribute("od");
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
    scenes[slot].odPath   = proc.getODModelPath();
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
    if(proc.isODModelLoaded()) proc.unloadODModel();
    editAmp=0; updateDualUI();
    repaint();
}
void ArcaneEclipseEditor::loadScene(int slot){
    abReset();                                                    // a new patch starts a fresh A/B
    if(scenes[slot].isEmpty()){ resetToDefault(); activeScene=slot; return; }
    applySnapshot(scenes[slot]);
    activeScene=slot;
}
void ArcaneEclipseEditor::applySnapshot(const SceneData& sc){
    if(sc.params.isValid()){
        // Load a COPY: replaceState() adopts the tree it is given, so passing the
        // saved one directly made every later tweak silently edit the saved patch.
        proc.apvts.replaceState(sc.params.createCopy());
        // Scenes saved before v1.1 carry no Dual settings: start them in single-amp mode
        if(! sc.params.getChildWithProperty("id",ArcaneEclipseProcessor::idDualOn).isValid())
            if(auto* d=proc.apvts.getParameter(ArcaneEclipseProcessor::idDualOn)) d->setValueNotifyingHost(0.f);
    }
    // Recall each amp's model (.nam or .aecap - an .aecap may carry its own IR,
    // in which case its IR path equals its model path) and its separate IR.
    const juce::String namP[2]={sc.namPath, sc.namPath2};
    const juce::String irP [2]={sc.irPath,  sc.irPath2};
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
    // overdrive pedal capture (empty = built-in drive)
    if(sc.odPath!=proc.getODModelPath()){
        juce::String err;
        if(sc.odPath.isNotEmpty() && juce::File(sc.odPath).existsAsFile())
            proc.loadODModel(juce::File(sc.odPath),err);
        else proc.unloadODModel();
    }
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
    for(auto& t:trimSl) if(t.isVisible()!=on) t.setVisible(on);
    if(matchBtn.isVisible()!=on) matchBtn.setVisible(on);
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
    t.setProperty("od",   scenes[slot].odPath, nullptr);
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
            scenes[slot].odPath  = t.getProperty("od","").toString();
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
    for(int i=0;i<3;++i) typeBtn[i].setBounds(RR(kFootX[i+1]-60,795,kFootX[i+1]+60,810));   // type pill
    odBtn.setBounds(RR(kFootX[0]-60,795,kFootX[0]+60,810));                                   // OD capture pill

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

    // Footer STEREO/MONO/DOUBLER menu
    stereoBtn.setBounds(RR(1250,996,1342,1020));
    // header icons: power (bypass) and [ ] (A/B)
    powerBtn.setBounds(RR(1464,13,1500,50));
    abBtn   .setBounds(RR(1372,13,1408,50));
    trialBtn.setBounds(RR(1108,17,1306,46));
    if (activationDialog) activationDialog->setBounds(getLocalBounds());
    if (splash) splash->setBounds(getLocalBounds());
    // dual level faders + MATCH inside the speaker grille
    trimSl[0].setBounds(RR(1114,694,1150,774));
    trimSl[1].setBounds(RR(1203,694,1239,774));
    matchBtn .setBounds(RR(1153,777,1200,792));

    tbCab.setBounds(-200,-200,1,1);
    creditsPanel.setBounds((W-640)/2,(H-600)/2,640,600);
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
    if(proc.isGlobalBypassed()){                                     // whole-rig bypass overlay
        g.setColour(juce::Colours::black.withAlpha(0.50f)); g.fillRect(RF(0,64,1514,980));
        auto pill=RF(532,334,982,404);
        g.setColour(juce::Colour(0xee14101c)); g.fillRoundedRectangle(pill,pill.getHeight()*0.5f);
        g.setColour(juce::Colour(0xffe0566b)); g.drawRoundedRectangle(pill.reduced(0.5f),pill.getHeight()*0.5f,1.6f);
        g.setFont(juce::Font(SY(19.f)).boldened().withExtraKerningFactor(0.18f)); g.setColour(juce::Colours::white);
        g.drawText("RIG BYPASSED",pill.withTrimmedBottom(pill.getHeight()*0.38f),juce::Justification::centredBottom,false);
        g.setFont(juce::Font(SY(14.5f))); g.setColour(juce::Colour(0xffd8c8ec));
        g.drawText("you are hearing your dry guitar - click the power icon to return",pill.withTrimmedTop(pill.getHeight()*0.58f),juce::Justification::centredTop,false);
    }
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
    if(learningID==ArcaneEclipseProcessor::idDualMix && dualMix.isVisible()){
        auto b=dualMix.getBounds().toFloat().expanded(3.f);
        g.setColour(kPurple.withAlpha(0.35f+0.55f*pulse)); g.drawRoundedRectangle(b,6.f,2.5f);
        haloText(g,"LEARN",juce::Font(8.f).boldened(),kPurple,
                 {dualMix.getX(),dualMix.getY()-13,dualMix.getWidth(),12},juce::Justification::centred);
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
    if(abHas[0]||abHas[1]) pn << "   [" << (abSide==0?"A":"B") << "]";
    haloText(g,pn,juce::Font(SY(17.f)).boldened(),kText,RR(600,14,862,40),juce::Justification::centred);

    // [ ] icon = A/B: light the square of the side you're hearing
    if(abHas[0]||abHas[1]){
        auto sq = abSide==0 ? RF(1381,26,1388,37) : RF(1391,26,1398,37);
        g.setColour(kPurple.withAlpha(0.35f)); g.fillRoundedRectangle(sq.expanded(2.f),2.f);
        g.setColour(kPurple); g.fillRoundedRectangle(sq,1.5f);
    }
    // power icon: purple glow = rig on; grey + red ring = bypassed
    {
        auto ic=RF(1464,13,1500,50);
        if(proc.isGlobalBypassed()){
            g.setColour(juce::Colour(0xcc120f18)); g.fillRoundedRectangle(ic.reduced(2.f),SX(6.f));
            g.setColour(juce::Colour(0xffe0566b)); g.drawRoundedRectangle(ic.reduced(1.5f),SX(6.f),1.6f);
            auto c=ic.getCentre(); float r=SX(7.f);
            juce::Path pw; pw.addCentredArc(c.x,c.y+1.f,r,r,0.f,0.75f,juce::MathConstants<float>::twoPi-0.75f,true);
            g.setColour(juce::Colour(0xff8a8496)); g.strokePath(pw,juce::PathStrokeType(1.8f)); g.drawLine(c.x,c.y-r-1.f,c.x,c.y+1.f,1.8f);
        } else {
            g.setColour(kPurple.withAlpha(0.18f)); g.fillRoundedRectangle(ic.expanded(2.f),SX(8.f));
        }
    }

    // Free-trial pill: "TRIAL · N DAYS LEFT" (click opens buy / activate)
    if (licAccess == AEAccess::TrialActive)
    {
        auto pill = RF(1108,17,1306,46);
        const int left = AELicenseManager::getInstance().trialDaysLeft();
        const bool urgent = left <= 2;
        auto acc = urgent ? juce::Colour(0xffff8a5c) : kPurple;
        g.setColour(acc.withAlpha(0.16f)); g.fillRoundedRectangle(pill, pill.getHeight()*0.5f);
        g.setColour(acc.withAlpha(0.9f));  g.drawRoundedRectangle(pill.reduced(0.6f), pill.getHeight()*0.5f, 1.3f);
        const juce::String dot = juce::String::fromUTF8("\xc2\xb7");
        juce::String t = left <= 1 ? "TRIAL " + dot + " LAST DAY"
                                   : "TRIAL " + dot + " " + juce::String(left) + " DAYS LEFT";
        haloText(g, t, juce::Font(SY(13.f)).boldened(),
                 kText, pill.toNearestInt(), juce::Justification::centred);
    }

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
    static const char* rvb[6]={"ROOM","HALL","PLATE","SPRING","AMBIENT","SWELL"};
    auto get=[this](const char* id){ return (int) proc.apvts.getRawParameterValue(id)->load(); };
    if(fx==0) return mod[juce::jlimit(0,2,get(ArcaneEclipseProcessor::idModType))];
    if(fx==1){
        juce::String d=dly[juce::jlimit(0,3,get(ArcaneEclipseProcessor::idDelayType))];
        if(delayTapMode()) d+=juce::String::fromUTF8(" \xc2\xb7 TAP");
        return d;
    }
    juce::String s=rvb[juce::jlimit(0,5,get(ArcaneEclipseProcessor::idReverbType))];
    if(proc.apvts.getRawParameterValue(ArcaneEclipseProcessor::idReverbShimmer)->load()>0.5f) s+=" + SHIMMER";
    return s;
}

void ArcaneEclipseEditor::showTypeMenu(int fx)
{
    static const char* names[3][6]={{"Chorus","Flanger","Phaser",nullptr,nullptr,nullptr},
                                    {"Digital","Analog","Tape","Echo",nullptr,nullptr},
                                    {"Room","Hall","Plate","Spring","Ambient","Swell (auto volume swell)"}};
    const char* ids[3]={ArcaneEclipseProcessor::idModType,ArcaneEclipseProcessor::idDelayType,
                        ArcaneEclipseProcessor::idReverbType};
    auto val=[this](const char* id){ return proc.apvts.getRawParameterValue(id)->load(); };
    int cur=(int) val(ids[fx]);
    juce::PopupMenu m;
    m.addSectionHeader(fx==0?"Modulation type":fx==1?"Delay type":"Reverb type");
    for(int i=0;i<6 && names[fx][i]!=nullptr;++i) m.addItem(i+1,names[fx][i],true,i==cur);
    if(fx==1){
        static const char* divs[4]={"1/4","Dotted 1/8","1/8","1/8 triplet"};
        int dv=(int) val(ArcaneEclipseProcessor::idDelayTapDiv);
        m.addSeparator();
        m.addItem(200,"Footswitch = Tap tempo (hold = on/off)",true,delayTapMode());
        juce::PopupMenu dm;
        for(int i=0;i<4;++i) dm.addItem(210+i,divs[i],true,i==dv);
        m.addSubMenu("Tap division",dm);
    }
    if(fx==2){
        m.addSeparator();
        m.addItem(100,"Shimmer (octave-up)",true,val(ArcaneEclipseProcessor::idReverbShimmer)>0.5f);
    }
    auto setParam=[this](const char* id,float v){
        if(auto* p=proc.apvts.getParameter(id)){
            p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(v)); p->endChangeGesture();
        }
    };
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&typeBtn[fx]),
        [this,setParam,val,id=ids[fx]](int r){
            if(r>=1 && r<=6) setParam(id,(float)(r-1));
            else if(r==100) setParam(ArcaneEclipseProcessor::idReverbShimmer,val(ArcaneEclipseProcessor::idReverbShimmer)>.5f?0.f:1.f);
            else if(r==200) setParam(ArcaneEclipseProcessor::idDelayTapMode,delayTapMode()?0.f:1.f);
            else if(r>=210 && r<=213){ setParam(ArcaneEclipseProcessor::idDelayTapDiv,(float)(r-210)); proc.applyTapDivision(); }
            repaint();
        });
}

void ArcaneEclipseEditor::paintPedalsLive(juce::Graphics& g)
{
    bool on[4]={ stompOD.getToggleState(), stompMod.getToggleState(),
                 stompDelay.getToggleState(), stompReverb.getToggleState() };
    for(int i=0;i<4;++i){
        if(i==2 && delayTapMode()){
            // tap mode: LED flashes on every repeat (at the TIME setting); dim when the delay is off
            float tms=proc.apvts.getRawParameterValue(ArcaneEclipseProcessor::idDelayTime)->load();
            double t0=proc.lastTapMs.load(), now=juce::Time::getMillisecondCounterHiRes();
            double ph=std::fmod(now-(t0>0?t0:0.0),(double)juce::jmax(20.f,tms));
            bool flash = ph < juce::jmin(120.0, tms*0.45);
            if(flash){
                if(on[i]) ledGlow(g,kFootX[i],kLedY,5.f);
                else { g.setColour(kPurple.withAlpha(0.45f)); g.fillEllipse(RF(kFootX[i]-4.5f,kLedY-4.5f,kFootX[i]+4.5f,kLedY+4.5f)); }
            } else if(on[i]) { g.setColour(kPurple.withAlpha(0.35f)); g.fillEllipse(RF(kFootX[i]-4.f,kLedY-4.f,kFootX[i]+4.f,kLedY+4.f)); }
        }
        else if(on[i]) ledGlow(g,kFootX[i],kLedY,5.f);
        {  // pill between the MIX/LEVEL label and the LED: effect type, or the OD capture
            auto r=RF(kFootX[i]-60,795,kFootX[i]+60,810);
            g.setColour(juce::Colour(0xcc0c0912)); g.fillRoundedRectangle(r,r.getHeight()*0.5f);
            g.setColour(kPurple.withAlpha(0.55f)); g.drawRoundedRectangle(r.reduced(0.5f),r.getHeight()*0.5f,1.f);
            g.setFont(juce::Font(SY(10.f)).boldened()); g.setColour(juce::Colour(0xffe6d2ff));
            juce::String label = (i>0) ? typeName(i-1)
                               : (proc.isODModelLoaded() ? proc.getODModelName().toUpperCase() : juce::String("BUILT-IN DRIVE"));
            g.drawFittedText(label,r.withTrimmedRight(SX(13.f)).withTrimmedLeft(SX(4.f)).toNearestInt(),
                             juce::Justification::centred,1,0.6f);
            juce::Path c; float ax=r.getRight()-SX(11.f), ay=r.getCentreY();
            c.addTriangle(ax-2.6f,ay-1.4f,ax+2.6f,ay-1.4f,ax,ay+1.8f);
            g.setColour(kPurple); g.fillPath(c);
        }
    }
}

void ArcaneEclipseEditor::showStereoMenu()
{
    auto val=[this](const char* id){ return proc.apvts.getRawParameterValue(id)->load(); };
    const bool st=val(ArcaneEclipseProcessor::idStereoMode)>.5f, db=val(ArcaneEclipseProcessor::idDoubler)>.5f;
    const float w=val(ArcaneEclipseProcessor::idDoublerWidth);
    juce::PopupMenu m;
    m.addSectionHeader("Output");
    m.addItem(1,"Mono",true,!st);
    m.addItem(2,"Stereo",true,st && !db);
    m.addItem(3,"Stereo + Doubler (double-tracked width)",true,st && db);
    juce::PopupMenu wm;
    wm.addItem(11,"Subtle (35%)",true,std::abs(w-.35f)<.02f);
    wm.addItem(12,"Medium (60%)",true,std::abs(w-.60f)<.02f);
    wm.addItem(13,"Wide (100%)",true,w>.98f);
    m.addSubMenu("Doubler width",wm);
    auto setP=[this](const char* id,float v){ if(auto* p=proc.apvts.getParameter(id)){ p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(v)); p->endChangeGesture(); } };
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&stereoBtn),[this,setP](int r){
        if(r==1){ setP(ArcaneEclipseProcessor::idStereoMode,0.f); setP(ArcaneEclipseProcessor::idDoubler,0.f); }
        else if(r==2){ setP(ArcaneEclipseProcessor::idStereoMode,1.f); setP(ArcaneEclipseProcessor::idDoubler,0.f); }
        else if(r==3){ setP(ArcaneEclipseProcessor::idStereoMode,1.f); setP(ArcaneEclipseProcessor::idDoubler,1.f); }
        else if(r>=11 && r<=13){ static const float ws[3]={.35f,.6f,1.f};
            setP(ArcaneEclipseProcessor::idDoublerWidth,ws[r-11]); setP(ArcaneEclipseProcessor::idStereoMode,1.f); setP(ArcaneEclipseProcessor::idDoubler,1.f); }
        repaint();
    });
}

// ── A/B compare ──────────────────────────────────────────────────────────────
SceneData ArcaneEclipseEditor::snapshotNow(){
    SceneData s; s.name="AB";
    s.params=proc.apvts.copyState();
    s.namPath=proc.getNAMPath(0); s.irPath=proc.getIRPath(0);
    s.namPath2=proc.getNAMPath(1); s.irPath2=proc.getIRPath(1);
    s.odPath=proc.getODModelPath();
    return s;
}
void ArcaneEclipseEditor::abReset(){ abHas[0]=abHas[1]=false; abSide=0; }
void ArcaneEclipseEditor::abToggle(){
    abSnap[abSide]=snapshotNow(); abHas[abSide]=true;           // remember the side we are leaving
    abSide^=1;
    if(abHas[abSide]) applySnapshot(abSnap[abSide]);
    else { abSnap[abSide]=abSnap[abSide^1]; abHas[abSide]=true; }   // first visit: start as a copy
    updateDualUI(); repaint();
}
void ArcaneEclipseEditor::showABMenu(){
    const char* cur = abSide==0 ? "A" : "B"; const char* oth = abSide==0 ? "B" : "A";
    juce::PopupMenu m;
    m.addSectionHeader(juce::String("A/B compare - now on ")+cur);
    m.addItem(1,juce::String("Switch to ")+oth);
    m.addItem(2,juce::String("Copy ")+cur+" to "+oth);
    m.addItem(3,"Reset A/B (keep what you hear now)",abHas[0]||abHas[1]);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&abBtn),[this](int r){
        if(r==1) abToggle();
        else if(r==2){ abSnap[abSide]=snapshotNow(); abHas[abSide]=true; abSnap[abSide^1]=abSnap[abSide]; abHas[abSide^1]=true; }
        else if(r==3) abReset();
        repaint();
    });
}
void ArcaneEclipseEditor::matchAmpLevels(){
    const float a=proc.getAmpLevel(0), b=proc.getAmpLevel(1);
    if(a<1.0e-7f || b<1.0e-7f || !proc.isDualActive()){
        matchMsg="PLAY A FEW SECONDS FIRST"; matchFlashMs=juce::Time::getMillisecondCounterHiRes(); repaint(); return;
    }
    const float d=juce::jlimit(-24.f,24.f,10.f*std::log10(a/b));      // mean-square ratio -> dB
    auto setP=[this](const char* id,float v){ if(auto* p=proc.apvts.getParameter(id)){ p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(v)); p->endChangeGesture(); } };
    setP(ArcaneEclipseProcessor::idAmp1Trim,0.f); setP(ArcaneEclipseProcessor::idAmp2Trim,d);
    matchMsg="MATCHED"; matchFlashMs=juce::Time::getMillisecondCounterHiRes(); repaint();
}

void ArcaneEclipseEditor::showODMenu()
{
    const bool loaded = proc.isODModelLoaded();
    juce::PopupMenu m;
    m.addSectionHeader(loaded ? "Pedal capture: " + proc.getODModelName() : juce::String("Overdrive: built-in drive"));
    m.addItem(1, "Load pedal capture (.nam / .aecap)...");
    m.addItem(2, "Use built-in drive", loaded, ! loaded);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&odBtn), [this](int r){
        if(r==1){
            chooserOD=std::make_unique<juce::FileChooser>("Load pedal capture (.nam / .aecap)",
                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.nam;*.aecap");
            chooserOD->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
                [this](const juce::FileChooser& fc){
                    auto res=fc.getResults(); if(res.isEmpty()) return;
                    juce::String err;
                    if(! proc.loadODModel(res[0],err))
                        juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon,"Arcane Eclipse",
                            "Could not load "+res[0].getFileName()+":\n"+err);
                    repaint();
                });
        }
        else if(r==2){ proc.unloadODModel(); repaint(); }
    });
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

    // Dual: amp level faders + MATCH in the speaker grille
    if(dualOn){
        auto panel=RF(1100,663,1253,801);
        g.setColour(juce::Colour(0xe60c0912)); g.fillRoundedRectangle(panel,SX(6.f));
        g.setColour(kPurple.withAlpha(0.35f)); g.drawRoundedRectangle(panel.reduced(0.5f),SX(6.f),1.f);
        g.setFont(juce::Font(SY(10.f)).boldened().withExtraKerningFactor(0.18f)); g.setColour(kMuted);
        g.drawText("AMP LEVEL",RF(1100,665,1253,677),juce::Justification::centred,false);
        for(int i=0;i<2;++i){
            auto b=trimSl[i].getBounds().toFloat();
            g.setFont(juce::Font(SY(10.5f)).boldened()); g.setColour(editAmp==i?juce::Colours::white:juce::Colour(0xffd8c8ec));
            g.drawText(trimSl[i].getTextFromValue(trimSl[i].getValue()),juce::Rectangle<float>(b.getCentreX()-SX(30.f),SY(679.f),SX(60.f),SY(13.f)),juce::Justification::centred,false);
            g.setFont(juce::Font(SY(11.f)).boldened()); g.setColour(editAmp==i?kPurple:kMuted);
            g.drawText(juce::String(i+1),juce::Rectangle<float>(b.getCentreX()-SX(12.f),SY(777.f),SX(24.f),SY(15.f)),juce::Justification::centred,false);
        }
        auto mb=matchBtn.getBounds().toFloat();
        bool flash = juce::Time::getMillisecondCounterHiRes()-matchFlashMs < 1600.0;
        g.setColour(juce::Colour(0xff1a1622)); g.fillRoundedRectangle(mb,mb.getHeight()*0.5f);
        g.setColour(flash && matchMsg=="MATCHED" ? juce::Colour(0xff3fe0a0) : kPurple); g.drawRoundedRectangle(mb.reduced(0.5f),mb.getHeight()*0.5f,1.2f);
        g.setFont(juce::Font(SY(9.5f)).boldened()); g.setColour(juce::Colours::white);
        g.drawText("MATCH",mb,juce::Justification::centred,false);
        if(flash){
            g.setFont(juce::Font(SY(9.f)).boldened()); g.setColour(matchMsg=="MATCHED"?juce::Colour(0xff3fe0a0):juce::Colour(0xffe0a056));
            g.drawText(matchMsg,RF(1100,792,1253,801),juce::Justification::centred,false);
        }
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
    // v1.1 footer logo: hide the baked flat "A" + "AMARI LABS" wording under a clean
    // copy of the empty footer strip just to its right, then draw the new mark alone,
    // centred ("DEVELOPED BY AMARI LABS" already sits at the left).
    {
        static juce::Image bg   = juce::ImageCache::getFromMemory(BinaryData::background_png,BinaryData::background_pngSize);
        static juce::Image mark = juce::ImageCache::getFromMemory(BinaryData::logo_amari_mark_v2_png,BinaryData::logo_amari_mark_v2_pngSize);
        if (bg.isValid()) {
            const float x0=666.f, x1=848.f, y0=985.f, y1=1032.f, shift=202.f;   // render px
            auto dst = RF(x0,y0,x1,y1);
            g.drawImage(bg, juce::roundToInt(dst.getX()), juce::roundToInt(dst.getY()),
                        juce::roundToInt(dst.getWidth()), juce::roundToInt(dst.getHeight()),
                        (int)(x0+shift), (int)y0, (int)(x1-x0), (int)(y1-y0));
        }
        if (mark.isValid()) {
            const float h = SY(42.f), w = h * (float) mark.getWidth() / (float) mark.getHeight();
            juce::Rectangle<float> r (0.f, 0.f, w, h);
            r.setCentre((float) W * 0.5f, SY(1008.f));
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImage(mark, r, juce::RectanglePlacement::stretchToFit);
        }
    }
    // STEREO / MONO (repaint over the baked word so it can change)
    const bool st=proc.apvts.getRawParameterValue(ArcaneEclipseProcessor::idStereoMode)->load()>.5f;
    const bool db=st && proc.apvts.getRawParameterValue(ArcaneEclipseProcessor::idDoubler)->load()>.5f;
    g.setColour(kFooterFill); g.fillRect(RF(1240,998,1342,1019));
    g.setFont(juce::Font(SY(14.f)).boldened());
    g.setColour(db?juce::Colour(0xffc9a3ff):(st?kPurple:kMuted));
    g.drawText(db?"DOUBLER":(st?"STEREO":"MONO"),RR(1236,998,1336,1019),juce::Justification::centredRight,false);
    // AMP / CAB status lamps (baked rings light up when a model / IR is loaded)
    bool d2=proc.isDualActive();
    if(proc.isNAMLoaded(0) || (d2 && proc.isNAMLoaded(1))) ledGlow(g,1365.f,1008.f,4.f);
    if(proc.isIRLoaded(0)  || (d2 && proc.isIRLoaded(1)))  ledGlow(g,1443.f,1008.f,4.f);
}

void ArcaneEclipseEditor::paintTuner(juce::Graphics& g)
{
    // ── v1.1.1 tuner redesign ────────────────────────────────────────────────
    using juce::Colour; using juce::Path; using juce::PathStrokeType; namespace Colours = juce::Colours;
    const float pi = juce::MathConstants<float>::pi;
    const Colour green (0xff3fe0a0), lilac (0xffc9a3ff), dimSeg (0xff2a2236), panel (0xff0f0c16);

    // backdrop: the plugin art, darkened, with a soft purple bloom behind the gauge
    static juce::Image bg = juce::ImageCache::getFromMemory(BinaryData::background_png,BinaryData::background_pngSize);
    if(bg.isValid()) g.drawImage(bg,0,0,W,H,0,0,bg.getWidth(),bg.getHeight());
    g.setColour(Colour(0xf309070f)); g.fillRect(0,0,W,H);
    const bool sig    = tunerHz > 0.f;
    const bool inTune = sig && std::fabs(tunerCents) < 3.f;
    const Colour accent = inTune ? green : kPurple;
    { juce::ColourGradient rg(accent.withAlpha(0.20f*(0.4f+0.6f*tunerSignal)),600.f,420.f,Colours::transparentBlack,600.f,420.f+520.f,true);
      g.setGradientFill(rg); g.fillRect(0,0,W,H); }

    // header
    g.setColour(Colour(0xff0b0911)); g.fillRect(0,0,W,kTopH);
    g.setColour(kPurple.withAlpha(.55f)); g.fillRect(0,kTopH-1,W,1);
    g.setFont(juce::Font(juce::FontOptions(15.f,juce::Font::bold)).withExtraKerningFactor(0.32f)); g.setColour(kText);
    g.drawText("CHROMATIC TUNER",0,0,W,kTopH,juce::Justification::centred);
    g.setFont(juce::Font(juce::FontOptions(11.f,juce::Font::bold)).withExtraKerningFactor(0.12f)); g.setColour(kMuted);
    g.drawText("A4 = 440 Hz",24,0,200,kTopH,juce::Justification::centredLeft);
    juce::Rectangle<float> xr((float)(W-46),12.f,30.f,30.f);
    g.setColour(Colour(0xff1b1724)); g.fillRoundedRectangle(xr,6.f);
    g.setColour(kPurple); g.drawRoundedRectangle(xr.reduced(0.5f),6.f,1.3f);
    g.setColour(kText); { float a=xr.getCentreX(),b=xr.getCentreY(); g.drawLine(a-6,b-6,a+6,b+6,2.f); g.drawLine(a-6,b+6,a+6,b-6,2.f); }

    // ── arc gauge: 51 LED segments, -50..+50 cents ───────────────────────────
    const float cx=600.f, cy=500.f, R=300.f, span=1.2217f;          // +-70 degrees
    const float c = juce::jlimit(-50.f,50.f,tunerDispCents);
    auto polar=[&](float r,float a){ return juce::Point<float>(cx+r*std::sin(a), cy-r*std::cos(a)); };
    { Path track; track.addCentredArc(cx,cy,R-20.f,R-20.f,0.f,-span-0.06f,span+0.06f,true);
      g.setColour(panel); g.strokePath(track,PathStrokeType(56.f,PathStrokeType::curved,PathStrokeType::rounded));
      g.setColour(kPurple.withAlpha(0.18f)); g.strokePath(track,PathStrokeType(57.f,PathStrokeType::curved,PathStrokeType::rounded));
      g.setColour(panel); g.strokePath(track,PathStrokeType(55.f,PathStrokeType::curved,PathStrokeType::rounded)); }
    for(int i=-25;i<=25;++i){
        float cs=(float)i*2.f, a=cs/50.f*span;
        bool major=(i%5==0);
        float r0=major?R-42.f:R-34.f, r1=R-6.f;
        bool lit = sig && ((c>=0.f && cs>=0.f && cs<=c+1.f) || (c<0.f && cs<=0.f && cs>=c-1.f));
        bool head = sig && std::fabs(cs-c)<=1.f;
        Colour col = dimSeg;
        if(lit)  col = (std::fabs(cs)<=3.f ? green : lilac.interpolatedWith(kPurple,std::fabs(cs)/50.f)).withAlpha(0.85f*tunerSignal+0.15f);
        if(head) col = inTune ? green : Colours::white;
        if(i==0 && !lit) col = green.withAlpha(0.35f);
        auto p0=polar(r0,a), p1=polar(r1,a);
        if(lit||head){ g.setColour(col.withAlpha(0.22f)); g.drawLine({p0,p1},major?13.f:11.f); }
        g.setColour(col); g.drawLine({p0,p1},major?5.f:3.6f);
    }
    // scale labels
    g.setFont(juce::Font(juce::FontOptions(11.f,juce::Font::bold)));
    for(int v : {-50,-25,0,25,50}){
        auto p=polar(R+18.f,(float)v/50.f*span);
        g.setColour(v==0?green.withAlpha(0.8f):kMuted.withAlpha(0.8f));
        g.drawText(v>0?"+"+juce::String(v):juce::String(v),juce::Rectangle<float>(p.x-22,p.y-8,44,16),juce::Justification::centred);
    }
    // pointer riding the arc (no needle crossing the note readout)
    { float na=c/50.f*span, al=0.30f+0.70f*tunerSignal;
      auto q0=polar(R-48.f,na), q1=polar(R+2.f,na);
      g.setColour(accent.withAlpha(0.20f*al)); g.drawLine({q0,q1},14.f);
      g.setColour((inTune?green:Colours::white).withAlpha(al)); g.drawLine({q0,q1},3.f);
      Path tip; auto t0=polar(R-54.f,na), tl=polar(R-70.f,na-0.035f), tr=polar(R-70.f,na+0.035f);
      tip.addTriangle(t0.x,t0.y,tl.x,tl.y,tr.x,tr.y);
      g.setColour(accent.withAlpha(al)); g.fillPath(tip); }

    // ── note readout ─────────────────────────────────────────────────────────
    {
        juce::String letter="-", sharp, octave;
        if(sig && tunerNote.isNotEmpty()){
            letter=tunerNote.substring(0,1);
            int k=1; if(tunerNote.length()>1 && tunerNote[1]=='#'){ sharp=juce::String::fromUTF8("\xe2\x99\xaf"); k=2; }
            octave=tunerNote.substring(k);
        }
        juce::Font big(juce::FontOptions(150.f,juce::Font::bold));
        juce::GlyphArrangement ga; ga.addLineOfText(big,letter,0.f,0.f);
        float lw=ga.getBoundingBox(0,-1,true).getWidth();
        juce::Rectangle<float> lr(cx-lw*0.5f-4.f,248.f,lw+8.f,150.f);
        Colour nc = !sig ? kMuted.withAlpha(0.35f) : (inTune ? green : Colours::white);
        if(sig){                                                     // soft glow
            g.setFont(big); g.setColour(accent.withAlpha(0.10f));
            for(int dx=-4;dx<=4;dx+=2) for(int dy=-4;dy<=4;dy+=2) if(dx||dy) g.drawText(letter,lr.translated((float)dx,(float)dy),juce::Justification::centred,false);
        }
        g.setFont(big); g.setColour(nc); g.drawText(letter,lr,juce::Justification::centred,false);
        g.setFont(juce::Font(juce::FontOptions(52.f,juce::Font::bold))); g.setColour(nc);
        if(sharp.isNotEmpty()) g.drawText(sharp,juce::Rectangle<float>(lr.getRight()-2.f,262.f,60.f,56.f),juce::Justification::centredLeft,false);
        g.setFont(juce::Font(juce::FontOptions(34.f,juce::Font::bold))); g.setColour(nc.withAlpha(sig?0.7f:0.3f));
        if(octave.isNotEmpty()) g.drawText(octave,juce::Rectangle<float>(lr.getRight()+2.f,346.f,50.f,40.f),juce::Justification::centredLeft,false);
        if(!sig){
            g.setFont(juce::Font(juce::FontOptions(12.f,juce::Font::bold)).withExtraKerningFactor(0.3f)); g.setColour(kMuted.withAlpha(0.8f));
            g.drawText("PLAY A SINGLE STRING",juce::Rectangle<float>(cx-160,392,320,20),juce::Justification::centred,false);
        }
    }

    // flat / sharp arrows + IN TUNE badge
    {
        bool flat=sig && tunerCents<=-3.f, sharpOn=sig && tunerCents>=3.f;
        auto tri=[&](float x,float y,bool left,bool on){
            Path t; if(left) t.addTriangle(x+12,y-14,x+12,y+14,x-12,y); else t.addTriangle(x-12,y-14,x-12,y+14,x+12,y);
            if(on){ g.setColour(kPurple.withAlpha(0.30f)); g.strokePath(t,PathStrokeType(8.f)); }
            g.setColour(on?lilac:dimSeg); g.fillPath(t);
        };
        tri(cx-150.f,441.f,true,flat); tri(cx+150.f,441.f,false,sharpOn);
        juce::Rectangle<float> badge(cx-64.f,427.f,128.f,28.f);
        g.setColour(inTune?green.withAlpha(0.18f):Colour(0xaa0c0912)); g.fillRoundedRectangle(badge,14.f);
        g.setColour(inTune?green:kPurple.withAlpha(0.45f)); g.drawRoundedRectangle(badge.reduced(0.5f),14.f,1.3f);
        g.setFont(juce::Font(juce::FontOptions(12.f,juce::Font::bold)).withExtraKerningFactor(0.25f));
        g.setColour(inTune?green:(sig?lilac:kMuted.withAlpha(0.6f)));
        g.drawText(inTune?"IN TUNE":(flat?"FLAT":(sharpOn?"SHARP":"TUNER")),badge,juce::Justification::centred,false);
    }

    // cents + Hz readout
    {
        juce::String cTxt = sig ? (tunerCents>=0.f?"+":"")+juce::String(tunerCents,1)+juce::String::fromUTF8(" \xc2\xa2") : juce::String("--");
        juce::String hTxt = sig ? juce::String(tunerHz,1)+" Hz" : juce::String("--- Hz");
        g.setFont(juce::Font(juce::FontOptions(20.f,juce::Font::bold)));
        g.setColour(sig?kText:kMuted.withAlpha(0.5f));
        g.drawText(cTxt,juce::Rectangle<float>(cx-230,cy-6,200,26),juce::Justification::centredRight,false);
        g.drawText(hTxt,juce::Rectangle<float>(cx+30,cy-6,200,26),juce::Justification::centredLeft,false);
        g.setColour(kPurple.withAlpha(0.6f)); g.fillEllipse(cx-3.f,cy+4.f,6.f,6.f);
    }

    // ── strobe band: stripes drift left (flat) / right (sharp), stand still in tune
    {
        juce::Rectangle<float> sb(330.f,585.f,540.f,38.f);
        g.setColour(panel); g.fillRoundedRectangle(sb,9.f);
        juce::Graphics::ScopedSaveState ss(g);
        Path clip; clip.addRoundedRectangle(sb.reduced(1.f),8.f); g.reduceClipRegion(clip);
        const float period=26.f, off=std::fmod(tunerStrobe,period)+(tunerStrobe<0.f?period:0.f);
        Colour sc = !sig ? dimSeg : (inTune ? green : kPurple);
        float sa = !sig ? 0.6f : 0.55f+0.35f*tunerSignal;
        for(float x=sb.getX()-2.f*period+off; x<sb.getRight()+period; x+=period){
            Path st; st.startNewSubPath(x,sb.getBottom()); st.lineTo(x+10.f,sb.getBottom()); st.lineTo(x+22.f,sb.getY()); st.lineTo(x+12.f,sb.getY()); st.closeSubPath();
            g.setColour(sc.withAlpha(sa)); g.fillPath(st);
        }
        juce::ColourGradient fl(panel,sb.getX(),0.f,panel.withAlpha(0.f),sb.getX()+90.f,0.f,false); g.setGradientFill(fl); g.fillRect(sb.withWidth(90.f));
        juce::ColourGradient fr(panel.withAlpha(0.f),sb.getRight()-90.f,0.f,panel,sb.getRight(),0.f,false); g.setGradientFill(fr); g.fillRect(sb.withTrimmedLeft(sb.getWidth()-90.f));
    }
    g.setColour((inTune?green:kPurple).withAlpha(0.5f)); g.drawRoundedRectangle(juce::Rectangle<float>(330.f,585.f,540.f,38.f),9.f,1.2f);
    g.setFont(juce::Font(juce::FontOptions(10.f,juce::Font::bold)).withExtraKerningFactor(0.25f)); g.setColour(kMuted.withAlpha(0.75f));
    g.drawText("FLAT",juce::Rectangle<float>(330.f,627.f,120.f,16.f),juce::Justification::centredLeft,false);
    g.drawText("STROBE",juce::Rectangle<float>(cx-60.f,627.f,120.f,16.f),juce::Justification::centred,false);
    g.drawText("SHARP",juce::Rectangle<float>(750.f,627.f,120.f,16.f),juce::Justification::centredRight,false);

    // ── standard-tuning string guide ─────────────────────────────────────────
    {
        static const char* nm[6]={"E","A","D","G","B","E"}; static const char* oc[6]={"2","2","3","3","3","4"};
        static const float hz[6]={82.41f,110.f,146.83f,196.f,246.94f,329.63f};
        int near=-1; float best=1e9f;
        if(sig) for(int i=0;i<6;++i){ float d=std::fabs(1200.f*std::log2(tunerHz/hz[i])); if(d<best){best=d;near=i;} }
        if(best>250.f) near=-1;                                      // not close to any open string
        const float pw=74.f, gap=12.f, x0=cx-(6*pw+5*gap)*0.5f, y=672.f;
        for(int i=0;i<6;++i){
            juce::Rectangle<float> r(x0+i*(pw+gap),y,pw,46.f);
            bool on=(i==near); bool ok=on&&inTune;
            g.setColour(ok?green.withAlpha(0.16f):(on?kPurple.withAlpha(0.22f):Colour(0xcc0e0b15))); g.fillRoundedRectangle(r,8.f);
            g.setColour(ok?green:(on?kPurple:kPurple.withAlpha(0.22f))); g.drawRoundedRectangle(r.reduced(0.5f),8.f,on?1.6f:1.f);
            g.setFont(juce::Font(juce::FontOptions(19.f,juce::Font::bold))); g.setColour(ok?green:(on?Colours::white:kMuted));
            g.drawText(juce::String(nm[i])+oc[i],r.withHeight(28.f).translated(0,4.f),juce::Justification::centred,false);
            g.setFont(juce::Font(juce::FontOptions(10.f))); g.setColour((on?lilac:kMuted).withAlpha(0.8f));
            g.drawText(juce::String(hz[i],1)+" Hz",r.withTrimmedTop(28.f).withHeight(14.f),juce::Justification::centred,false);
        }
        g.setFont(juce::Font(juce::FontOptions(10.f,juce::Font::bold)).withExtraKerningFactor(0.25f)); g.setColour(kMuted.withAlpha(0.7f));
        g.drawText("STANDARD TUNING",juce::Rectangle<float>(cx-100.f,y-20.f,200.f,14.f),juce::Justification::centred,false);
    }

    // footer
    g.setColour(kPurple.withAlpha(0.35f)); g.fillRect(0,H-kFootH,W,1);
    g.setFont(juce::Font(juce::FontOptions(10.5f))); g.setColour(kMuted.withAlpha(0.8f));
    g.drawText(juce::String::fromUTF8("Click \xe2\x9c\x95 (top-right) to return to the rig"),0,H-kFootH,W,kFootH,juce::Justification::centred);
}
