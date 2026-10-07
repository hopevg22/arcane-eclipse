#include "PluginProcessor.h"
#include <BinaryData.h>
#include <cmath>
#include "PluginEditor.h"
#include "AecapLoader.h"

ArcaneEclipseProcessor::ArcaneEclipseProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    learnParamIDs = {
        idInputGain, idNoiseGate, idCompThresh, idOutputGain,
        idAmpGain, idAmpBass, idAmpMid, idAmpTreble, idAmpPresence, idAmpMaster,
        idODDrive, idODTone, idODLevel,
        idModRate, idModDepth, idModMix,
        idDelayTime, idDelayFeedback, idDelayMix,
        idReverbDecay, idReverbSize, idReverbMix,
        // on/off toggles (for footswitch MIDI learn)
        idGateOn, idCompOn, idODOn, idModOn, idDelayOn, idReverbOn,
        // v1.1 dual amp/IR (appended so existing mappings keep their slots)
        idDualMix, idDualOn
    };
    for (auto& id : learnParamIDs) learnParamPtrs.push_back(apvts.getParameter(id));
    learnIsToggle.assign(learnParamIDs.size(), false);
    for (int i = 0; i < (int) learnParamIDs.size(); ++i) {
        auto& id = learnParamIDs[i];
        if (id == idGateOn || id == idCompOn || id == idODOn ||
            id == idModOn  || id == idDelayOn || id == idReverbOn || id == idDualOn)
            learnIsToggle[i] = true;
    }
    for (auto& c : ccMap) c.store(-1);
    for (auto& c : actionCC) c.store(-1);
    for (auto& v : prevCCVal) v = 0;

    // Load the license / trial state here, not only when the window opens, so a
    // DAW project reopened with the plugin window closed still plays.
    AELicenseManager::getInstance().refresh();

    // v1.1: a fresh instance plays straight away on a built-in amp and cabinet
    loadDefaultRig();
}

juce::AudioProcessorValueTreeState::ParameterLayout ArcaneEclipseProcessor::createParameterLayout()
{
    using Range = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back(std::make_unique<juce::AudioParameterFloat>(idInputGain,  "Input Gain",  Range(-20.f,20.f,.1f), 0.f, "dB"));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idOutputGain, "Output Gain", Range(-20.f,20.f,.1f), 0.f, "dB"));
    p.push_back(std::make_unique<juce::AudioParameterBool> (idStereoMode, "Stereo", true));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idNoiseGate,  "Noise Gate",  Range(-80.f,-40.f,.5f),-60.f,"dB"));
    p.push_back(std::make_unique<juce::AudioParameterBool> (idGateOn,     "Gate On",     false));
    p.push_back(std::make_unique<juce::AudioParameterBool> (idCabBypass,  "Cab Bypass",  false));

    p.push_back(std::make_unique<juce::AudioParameterFloat>(idAmpGain,     "Gain",     Range(0.f,10.f,.1f), 4.2f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idAmpBass,     "Bass",     Range(0.f,10.f,.1f), 5.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idAmpMid,      "Mid",      Range(0.f,10.f,.1f), 5.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idAmpTreble,   "Treble",   Range(0.f,10.f,.1f), 6.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idAmpPresence, "Presence", Range(0.f,10.f,.1f), 4.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idAmpMaster,   "Master",   Range(0.f,10.f,.1f), 6.5f));

    p.push_back(std::make_unique<juce::AudioParameterBool> (idCompOn,      "Comp On",  false)); // default OFF
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idCompThresh,  "Threshold",Range(-40.f,0.f,.5f),-18.f,"dB"));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idCompRatio,   "Ratio",    Range(1.f,20.f,.1f),4.f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idCompAttack,  "Attack",   Range(.1f,100.f,.1f),10.f,"ms"));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idCompRelease, "Release",  Range(10.f,500.f,1.f),100.f,"ms"));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idCompMakeup,  "Makeup",   Range(0.f,24.f,.1f),0.f,"dB"));

    p.push_back(std::make_unique<juce::AudioParameterBool> (idODOn,    "OD On",    false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idODDrive, "Drive",    Range(0.f,1.f,.01f),.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idODTone,  "OD Tone",  Range(0.f,1.f,.01f),.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idODLevel, "OD Level", Range(0.f,1.f,.01f),.7f));

    p.push_back(std::make_unique<juce::AudioParameterBool> (idModOn,    "Mod On",   false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idModRate,  "Mod Rate", Range(.1f,4.f,.05f),1.5f,"Hz"));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idModDepth, "Mod Depth",Range(0.f,1.f,.01f),.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idModMix,   "Mod Mix",  Range(0.f,1.f,.01f),.7f));
    p.push_back(std::make_unique<juce::AudioParameterInt>  (idModType,  "Mod Type", 0, 2, 0));

    p.push_back(std::make_unique<juce::AudioParameterBool> (idDelayOn,       "Delay On",  false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idDelayTime,     "Delay Time",
                 juce::NormalisableRange<float>(20.f,2000.f,1.f,.4f),350.f,"ms"));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idDelayFeedback, "Feedback",  Range(0.f,.97f,.01f),.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idDelayMix,      "Delay Mix", Range(0.f,1.f,.01f),.35f));
    p.push_back(std::make_unique<juce::AudioParameterInt>  (idDelayType,     "Delay Type",0, 3, 0));
    p.push_back(std::make_unique<juce::AudioParameterBool> (idDelayTapMode,  "Delay Tap Mode", false));
    p.push_back(std::make_unique<juce::AudioParameterInt>  (idDelayTapDiv,   "Delay Tap Division", 0, 3, 0));

    p.push_back(std::make_unique<juce::AudioParameterBool> (idReverbOn,    "Reverb On",  false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idReverbHighCut,"Reverb High Cut",Range(0.f,1.f,.01f),.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idReverbDecay, "Reverb Decay",Range(0.f,1.f,.01f),.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idReverbSize,  "Reverb Size", Range(0.f,1.f,.01f),.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idReverbMix,   "Reverb Mix",  Range(0.f,1.f,.01f),.5f));
    p.push_back(std::make_unique<juce::AudioParameterInt>  (idReverbType,  "Reverb Type", 0, 5, 0));   // + ambient, swell
    p.push_back(std::make_unique<juce::AudioParameterBool> (idReverbShimmer, "Shimmer", false));

    // Dual Amp/IR (v1.1): blend amp 1 (0) <-> amp 2 (1); shown as "70 / 30"
    p.push_back(std::make_unique<juce::AudioParameterBool> (idDualOn, "Dual Amp/IR", false));
    auto dbTxt = juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int){
        return (v > 0.f ? "+" : "") + juce::String(v, 1) + " dB"; });
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idAmp1Trim, "Amp 1 Level", Range(-24.f,24.f,.1f), 0.f, dbTxt));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idAmp2Trim, "Amp 2 Level", Range(-24.f,24.f,.1f), 0.f, dbTxt));
    p.push_back(std::make_unique<juce::AudioParameterBool> (idDoubler, "Doubler", false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idDoublerWidth, "Doubler Width", Range(0.f,1.f,.01f), .6f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int){ return juce::String(juce::roundToInt(v*100)) + "%"; })));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idDualMix, "Dual Mix", Range(0.f,1.f,.01f), .5f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int){
            int b = juce::roundToInt(v * 100.f); return juce::String(100 - b) + " / " + juce::String(b); })));

    return { p.begin(), p.end() };
}

bool ArcaneEclipseProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    auto out = l.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void ArcaneEclipseProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, 2 };
    for (auto& a : amps) {
        a.conv.prepare(spec); a.conv.reset();
        a.upIn .assign((size_t)(samplesPerBlock * 3 + 32), 0.f);
        a.upOut.assign((size_t)(samplesPerBlock * 3 + 32), 0.f);
        a.rsIn.reset(); a.rsOut.reset();
        a.sim.prepare(sampleRate);
        { std::vector<float> warm((size_t) juce::jmax(1, samplesPerBlock), 0.f);   // size AmpSim's buffer here, not on the audio thread
          a.sim.processBlock(warm.data(), (int) warm.size()); a.sim.reset(); }
    }
    for (auto& b : dualBuf) b.setSize(2, samplesPerBlock + 32, false, true, false);
    dryBuf.setSize(2, samplesPerBlock + 32, false, true, false);
    doubler.prepare(sampleRate, samplesPerBlock);
    bypassSm.reset(sampleRate, 0.025); bypassSm.setCurrentAndTargetValue(globalBypass.load() ? 1.f : 0.f);
    for (int i = 0; i < 2; ++i) {
        trimSm[i].reset(sampleRate, 0.03);
        trimSm[i].setCurrentAndTargetValue(juce::Decibels::decibelsToGain(apvts.getRawParameterValue(i ? idAmp2Trim : idAmp1Trim)->load()));
    }
    dualMixSm.reset(sampleRate, 0.03);
    dualMixSm.setCurrentAndTargetValue(apvts.getRawParameterValue(idDualMix)->load());
    monoBuf       .assign((size_t)(samplesPerBlock + 32),     0.f);
    namOutBuf     .assign((size_t)(samplesPerBlock + 32),     0.f);
    odSlot.upIn .assign((size_t)(samplesPerBlock * 3 + 32), 0.f);
    odSlot.upOut.assign((size_t)(samplesPerBlock * 3 + 32), 0.f);
    odSlot.rsIn.reset(); odSlot.rsOut.reset();
    odMono.assign((size_t)(samplesPerBlock + 32), 0.f);
    odOut .assign((size_t)(samplesPerBlock + 32), 0.f);
    odTiltZ[0] = odTiltZ[1] = 0.f;
    tunerBuf.assign(2048, 0.f); tunerFill = 0; tunerFreq.store(0.f);
    { int maxLag = (int)(sampleRate / 55.0) + 2;             // YIN scratch (RT-safe: prealloc)
      tunerD.assign((size_t) maxLag + 1, 0.0); tunerDP.assign((size_t) maxLag + 1, 1.0); }
    tunerStable = 0.f; tunerHistCount = 0;
    compressor.prepare(sampleRate, samplesPerBlock);
    overdrive.prepare(sampleRate, samplesPerBlock);
    modulation.prepare(sampleRate, samplesPerBlock);
    delay.prepare(sampleRate, samplesPerBlock);
    reverb.prepare(sampleRate, samplesPerBlock);
    updateEQ();
    for (int ch = 0; ch < 2; ++ch) {
        bassFilter[ch].reset(); midFilter[ch].reset();
        trebleFilter[ch].reset(); presenceFilter[ch].reset();
    }
    gateEnvelope = 0.f;
}

void ArcaneEclipseProcessor::releaseResources() {}

void ArcaneEclipseProcessor::updateEQ()
{
    double sr = currentSampleRate > 0 ? currentSampleRate : 44100.0;
    auto toDb = [](float v){ return juce::jmap(v,0.f,10.f,-12.f,12.f); };
    float bDb = toDb(apvts.getRawParameterValue(idAmpBass)->load());
    float mDb = toDb(apvts.getRawParameterValue(idAmpMid)->load());
    float tDb = toDb(apvts.getRawParameterValue(idAmpTreble)->load());
    float pDb = toDb(apvts.getRawParameterValue(idAmpPresence)->load());

    // Only recalculate if values have actually changed — avoids mid-stream
    // coefficient updates that cause IIR state inconsistency (robotic artifacts)
    // (per-instance cache: these were function-statics, shared by every plugin
    //  instance, so a 2nd instance in the same DAW never got its EQ set up)
    if (std::abs(bDb-eqLastB)<0.01f && std::abs(mDb-eqLastM)<0.01f &&
        std::abs(tDb-eqLastT)<0.01f && std::abs(pDb-eqLastP)<0.01f &&
        std::abs(sr-eqLastSR)<1.0)
        return; // nothing changed — keep existing coefficients

    eqLastB=bDb; eqLastM=mDb; eqLastT=tDb; eqLastP=pDb; eqLastSR=sr;

    // Reset filter state before applying new coefficients to avoid transients
    for (int ch = 0; ch < 2; ++ch) {
        bassFilter[ch].reset();
        midFilter[ch].reset();
        trebleFilter[ch].reset();
        presenceFilter[ch].reset();
        *bassFilter[ch].coefficients     = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(
                                               sr,150.,0.71,juce::Decibels::decibelsToGain(bDb));
        *midFilter[ch].coefficients      = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                                               sr,600.,0.71,juce::Decibels::decibelsToGain(mDb));
        *trebleFilter[ch].coefficients   = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
                                               sr,3000.,0.71,juce::Decibels::decibelsToGain(tDb));
        *presenceFilter[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
                                               sr,6000.,0.71,juce::Decibels::decibelsToGain(pDb));
    }
}

void ArcaneEclipseProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // License gate: silence all audio unless licensed or inside the free trial
    if (!AELicenseManager::getInstance().isAudioEnabled())
    { buffer.clear(); return; }
    juce::ScopedNoDenormals noDenormals;
    int numSamples = buffer.getNumSamples(), numCh = buffer.getNumChannels();

    // MIDI CC learn / control
    for (const auto meta : midi) {
        const auto msg = meta.getMessage();
        // Accept CC and Note messages so most footswitches work (CC 0-127,
        // Notes mapped to 128-255). Set your pedal to CC or Note mode.
        int idx = -1, val = 0;
        if (msg.isController())   { idx = msg.getControllerNumber();      val = msg.getControllerValue(); }
        else if (msg.isNoteOn())  { idx = 128 + msg.getNoteNumber();      val = 127; }
        else if (msg.isNoteOff()) { idx = 128 + msg.getNoteNumber();      val = 0;   }
        if (idx >= 0) {
            const int al = actionLearn.load();
            const int lt = learnTarget.load();
            if (al >= 0) { actionCC[idx].store(al); actionLearn.store(-1); }
            else if (lt >= 0) { ccMap[idx].store(lt); learnTarget.store(-1); }
            else {
                const int pi = ccMap[idx].load();
                if (pi >= 0 && pi < (int) learnParamPtrs.size() && learnParamPtrs[pi] != nullptr) {
                    if (pi < (int) learnIsToggle.size() && learnIsToggle[pi]) {
                        if (prevCCVal[idx] < 64 && val >= 64
                            && learnParamIDs[(size_t) pi] == idDelayOn
                            && apvts.getRawParameterValue(idDelayTapMode)->load() > .5f) {
                            tapTempo();                          // tap mode: footswitch = TAP
                        }
                        else if (prevCCVal[idx] < 64 && val >= 64) {
                            float cur = learnParamPtrs[pi]->getValue();
                            learnParamPtrs[pi]->setValueNotifyingHost(cur < 0.5f ? 1.0f : 0.0f);
                        }
                    } else {
                        learnParamPtrs[pi]->setValueNotifyingHost(val / 127.0f);
                    }
                }
                const int a = actionCC[idx].load();
                if (a >= 0 && prevCCVal[idx] < 64 && val >= 64) actionPending.store(a);
            }
            prevCCVal[idx] = val;
        }
    }

    // keep the raw input for the global bypass crossfade
    if (dryBuf.getNumSamples() < numSamples) dryBuf.setSize(2, numSamples, false, false, true);
    for (int ch = 0; ch < 2; ++ch) dryBuf.copyFrom(ch, 0, buffer, juce::jmin(ch, numCh - 1), 0, numSamples);

    // 0. TUNER — feed the pitch detector from the dry input (only when open)
    if (tunerActive.load() && ! tunerBuf.empty()) {
        auto* in = buffer.getReadPointer(0);
        for (int n = 0; n < numSamples; ++n) {
            float smp = numCh > 1 ? 0.5f * (in[n] + buffer.getReadPointer(1)[n]) : in[n];
            tunerBuf[(size_t) tunerFill++] = smp;
            if (tunerFill >= (int) tunerBuf.size()) {
                float raw = detectPitch(tunerBuf.data(), (int) tunerBuf.size(), currentSampleRate);
                tunerFreq.store(smoothTunerPitch(raw));
                tunerFill = 0;
            }
        }
    }

    // 1. INPUT GAIN
    buffer.applyGain(juce::Decibels::decibelsToGain(apvts.getRawParameterValue(idInputGain)->load()));
    inLevel.store(buffer.getMagnitude(0, numSamples));   // input meter

    // 2. NOISE GATE — only when enabled
    if (apvts.getRawParameterValue(idGateOn)->load() > .5f)
    {
        float thresh = juce::Decibels::decibelsToGain(apvts.getRawParameterValue(idNoiseGate)->load());
        for (int n = 0; n < numSamples; ++n) {
            float rms = std::fabs(buffer.getReadPointer(0)[n]);
            float coeff = rms > gateEnvelope ? 0.9995f : 0.9980f;
            gateEnvelope = rms + coeff * (gateEnvelope - rms);
            float gateGain = gateEnvelope > thresh ? 1.f :
                             juce::jlimit(0.f, 1.f, gateEnvelope / juce::jmax(thresh, 1e-6f));
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer(ch)[n] *= gateGain;
        }
    }

    // 3. COMPRESSOR — optional pre-amp compression
    if (apvts.getRawParameterValue(idCompOn)->load() > .5f) {
        compressor.setParameters(
            apvts.getRawParameterValue(idCompThresh)->load(),
            apvts.getRawParameterValue(idCompRatio)->load(),
            apvts.getRawParameterValue(idCompAttack)->load(),
            apvts.getRawParameterValue(idCompRelease)->load(),
            apvts.getRawParameterValue(idCompMakeup)->load());
        compressor.processBlock(buffer);
    }

    // 4. OVERDRIVE — pre-amp drive pedal
    if (apvts.getRawParameterValue(idODOn)->load() > .5f) {
        const float drv = apvts.getRawParameterValue(idODDrive)->load();
        const float ton = apvts.getRawParameterValue(idODTone)->load();
        const float lvl = apvts.getRawParameterValue(idODLevel)->load();
        if (odSlot.model != nullptr) {
            // Pedal capture replaces the built-in circuit (mono in -> capture -> both channels)
            if ((int) odMono.size() < numSamples + 8) odMono.assign((size_t)(numSamples + 16), 0.f);
            if ((int) odOut .size() < numSamples + 8) odOut .assign((size_t)(numSamples + 16), 0.f);
            makeMono(buffer, numSamples, odMono.data());
            const float inG  = juce::Decibels::decibelsToGain((drv - 0.5f) * 24.f);                  // +-12 dB
            const float outDb = lvl >= 0.7f ? (lvl - 0.7f) / 0.3f * 6.f : (lvl - 0.7f) / 0.7f * 24.f;
            const float outG = juce::Decibels::decibelsToGain(outDb);                                // -24..+6 dB
            const float hiG  = juce::Decibels::decibelsToGain((ton - 0.5f) * 12.f);                  // +-6 dB tilt
            const float a = std::exp(-2.f * juce::MathConstants<float>::pi * 800.f / (float) currentSampleRate);
            for (int n = 0; n < numSamples; ++n) odMono[(size_t) n] *= inG;
            runNAM(odSlot, odMono.data(), odOut.data(), numSamples);
            for (int n = 0; n < numSamples; ++n) {
                float x = odOut[(size_t) n];
                odTiltZ[0] = a * odTiltZ[0] + (1.f - a) * x;                  // lows below ~800 Hz
                odOut[(size_t) n] = outG * (odTiltZ[0] + hiG * (x - odTiltZ[0]));
            }
            for (int ch = 0; ch < numCh; ++ch)
                std::copy(odOut.begin(), odOut.begin() + numSamples, buffer.getWritePointer(ch));
        } else {
            overdrive.setParameters(drv, ton, lvl);
            overdrive.processBlock(buffer);
        }
    }

    // 5. AMP GAIN — pre-NAM input level
    float ampGainDb = juce::jmap(apvts.getRawParameterValue(idAmpGain)->load(), 0.f,10.f,-6.f,18.f);
    const float ampGainLin = juce::Decibels::decibelsToGain(ampGainDb);
    buffer.applyGain(ampGainLin);

    // 6. AMP(S) — NAM model(s). Mono amp input: consistent level whether the
    //    host feeds a mono guitar duplicated on both channels (DAW) or on a
    //    single input channel (audio interface). Average only when both
    //    channels carry comparable signal; otherwise sum.
    const bool dual = isDualActive();
    if ((int)monoBuf.size()   < numSamples + 8) monoBuf  .assign((size_t)(numSamples + 16), 0.f);
    if ((int)namOutBuf.size() < numSamples + 8) namOutBuf.assign((size_t)(numSamples + 16), 0.f);
    auto buildMono = [&]{ makeMono(buffer, numSamples, monoBuf.data()); };

    if (! dual)
    {
        // Single amp (unchanged v1.0 behaviour): no model = signal passes through
        if (hasAmp(amps[0])) {
            buildMono();
            runAmp(amps[0], monoBuf.data(), namOutBuf.data(), numSamples, ampGainLin);
            for (int ch = 0; ch < numCh; ++ch)
                std::copy(namOutBuf.begin(), namOutBuf.begin() + numSamples, buffer.getWritePointer(ch));
        }
    }
    else
    {
        // Dual: each amp runs its own model AND its own cab IR on the same input,
        // then the two are blended. Everything after the model (DC block, EQ,
        // master) is linear, so applying the shared EQ/master after the blend is
        // equivalent to applying it inside each path - one faceplate, two amps.
        buildMono();
        for (int s = 0; s < kNumAmpSlots; ++s) {
            auto& a = amps[s]; auto& db = dualBuf[s];
            if (db.getNumSamples() < numSamples) db.setSize(2, numSamples, false, false, true);
            const float* src = monoBuf.data();
            if (hasAmp(a)) { runAmp(a, monoBuf.data(), namOutBuf.data(), numSamples, ampGainLin); src = namOutBuf.data(); }
            for (int ch = 0; ch < 2; ++ch) std::copy(src, src + numSamples, db.getWritePointer(ch));
            if (a.irLoaded) {
                auto blk = juce::dsp::AudioBlock<float>(db).getSubBlock(0, (size_t) numSamples);
                a.conv.process(juce::dsp::ProcessContextReplacing<float>(blk));
            }
        }
        // running loudness of each amp before its trim (for MATCH); only while playing
        for (int s = 0; s < kNumAmpSlots; ++s) {
            float ms = 0.f; auto* d = dualBuf[s].getReadPointer(0);
            for (int n = 0; n < numSamples; ++n) ms += d[n] * d[n];
            ms /= (float) juce::jmax(1, numSamples);
            if (ms > 1.0e-7f) {                                   // > -70 dBFS: someone is playing
                float a = std::exp(-(float) numSamples / (float) (2.0 * currentSampleRate));   // ~2 s
                ampMs[s].store(a * ampMs[s].load() + (1.f - a) * ms);
            }
        }
        trimSm[0].setTargetValue(juce::Decibels::decibelsToGain(apvts.getRawParameterValue(idAmp1Trim)->load()));
        trimSm[1].setTargetValue(juce::Decibels::decibelsToGain(apvts.getRawParameterValue(idAmp2Trim)->load()));
        // Linear crossfade: keeps the level steady for two amps fed the same guitar
        dualMixSm.setTargetValue(apvts.getRawParameterValue(idDualMix)->load());
        auto* a0L = dualBuf[0].getReadPointer(0); auto* a0R = dualBuf[0].getReadPointer(1);
        auto* a1L = dualBuf[1].getReadPointer(0); auto* a1R = dualBuf[1].getReadPointer(1);
        for (int n = 0; n < numSamples; ++n) {
            float m = dualMixSm.getNextValue(), g0 = (1.f - m) * trimSm[0].getNextValue(), g1 = m * trimSm[1].getNextValue();
            buffer.getWritePointer(0)[n] = g0 * a0L[n] + g1 * a1L[n];
            if (numCh > 1) buffer.getWritePointer(1)[n] = g0 * a0R[n] + g1 * a1R[n];
        }
    }

    // 6b. DC BLOCKER — strip any DC the amp model introduces. Without this,
    //     DC slowly accumulates in the delay/reverb feedback loops and runs
    //     the level away into loud clipping after a few minutes.
    for (int ch = 0; ch < juce::jmin(numCh,2); ++ch) {
        auto* d = buffer.getWritePointer(ch);
        for (int n = 0; n < numSamples; ++n) {
            float x = d[n];
            float y = x - dcX1[ch] + 0.9975f * dcY1[ch];
            dcX1[ch] = x; dcY1[ch] = y;
            d[n] = y;
        }
    }

    // 7. AMP EQ — tone shaping post-NAM
    updateEQ();
    for (int n = 0; n < numSamples; ++n)
        for (int ch = 0; ch < juce::jmin(numCh,2); ++ch) {
            auto* d = buffer.getWritePointer(ch);
            d[n] = bassFilter[ch].processSample(d[n]);
            d[n] = midFilter[ch].processSample(d[n]);
            d[n] = trebleFilter[ch].processSample(d[n]);
            d[n] = presenceFilter[ch].processSample(d[n]);
        }

    // 8. MASTER VOLUME
    float masterDb = juce::jmap(apvts.getRawParameterValue(idAmpMaster)->load(), 0.f,10.f,-20.f,6.f);
    buffer.applyGain(juce::Decibels::decibelsToGain(masterDb));

    // 9. CABINET IR — speaker simulation (in dual mode each amp's IR was
    //    already applied inside its own path above)
    if (! dual && amps[0].irLoaded) {
        juce::dsp::AudioBlock<float> block(buffer);
        amps[0].conv.process(juce::dsp::ProcessContextReplacing<float>(block));
    }

    // 9b. STEREO DOUBLER — double-tracked width (needs stereo mode + 2 channels)
    if (numCh > 1 && apvts.getRawParameterValue(idDoubler)->load() > .5f
                  && apvts.getRawParameterValue(idStereoMode)->load() > .5f) {
        doubler.setWidth(apvts.getRawParameterValue(idDoublerWidth)->load());
        doubler.processBlock(buffer);
    }

    // 10. MODULATION — post-cab chorus/flanger (keep mix low)
    if (apvts.getRawParameterValue(idModOn)->load() > .5f) {
        modulation.setParameters(
            apvts.getRawParameterValue(idModRate)->load(),
            apvts.getRawParameterValue(idModDepth)->load(),
            apvts.getRawParameterValue(idModMix)->load(),
            apvts.getRawParameterValue(idModType)->load());
        modulation.processBlock(buffer);
    }

    // 11. DELAY
    {
        bool dlOn = apvts.getRawParameterValue(idDelayOn)->load() > .5f;
        if (dlOn && ! prevDelayOn) delay.reset();   // clear stale tail on enable (v1.0.2)
        prevDelayOn = dlOn;
    }
    if (apvts.getRawParameterValue(idDelayOn)->load() > .5f) {
        delay.setType((int) apvts.getRawParameterValue(idDelayType)->load());   // v1.1 digital/analog/tape/echo
        delay.setParameters(
            apvts.getRawParameterValue(idDelayTime)->load(),
            apvts.getRawParameterValue(idDelayFeedback)->load(),
            0.7f,
            apvts.getRawParameterValue(idDelayMix)->load(), false);
        delay.processBlock(buffer);
    }

    // 12. REVERB — always last
    {
        bool rvOn = apvts.getRawParameterValue(idReverbOn)->load() > .5f;
        if (rvOn && ! prevReverbOn) reverb.reset();   // clear any stale tail on enable
        prevReverbOn = rvOn;
    }
    if (apvts.getRawParameterValue(idReverbOn)->load() > .5f) {
        reverb.setType((int) apvts.getRawParameterValue(idReverbType)->load());        // v1.1 room/hall/plate/spring
        reverb.setShimmer(apvts.getRawParameterValue(idReverbShimmer)->load() > .5f);  // v1.1 octave-up layer
        reverb.setParameters(
            apvts.getRawParameterValue(idReverbDecay)->load(),
            0.f,
            apvts.getRawParameterValue(idReverbHighCut)->load(),
            apvts.getRawParameterValue(idReverbSize)->load(),
            .72f, .45f,
            apvts.getRawParameterValue(idReverbMix)->load());
        reverb.processBlock(buffer);
    }

    // 12b. MONO / STEREO — collapse to mono when stereo mode is off
    if (numCh > 1 && apvts.getRawParameterValue(idStereoMode)->load() < 0.5f) {
        auto* l = buffer.getWritePointer(0);
        auto* r = buffer.getWritePointer(1);
        for (int n = 0; n < numSamples; ++n) { float m = 0.5f*(l[n]+r[n]); l[n]=m; r[n]=m; }
    }

    // 13. OUTPUT GAIN
    buffer.applyGain(juce::Decibels::decibelsToGain(apvts.getRawParameterValue(idOutputGain)->load()));

    // 14. SAFETY — sanitise NaN/Inf and hard-limit runaway levels so a bad
    //     value can never blast the speakers.
    for (int ch = 0; ch < numCh; ++ch) {
        auto* d = buffer.getWritePointer(ch);
        for (int n = 0; n < numSamples; ++n) {
            float v = d[n];
            if (!std::isfinite(v)) v = 0.f;
            // Transparent below unity; smoothly soft-limit anything above so a
            // runaway can never clip harshly or blast the speakers.
            if      (v >  1.0f) v =  1.0f + std::tanh(v - 1.0f);
            else if (v < -1.0f) v = -1.0f - std::tanh(-v - 1.0f);
            d[n] = v;
        }
    }
    // 15. GLOBAL BYPASS — crossfade to the raw input (A/B against the dry guitar)
    bypassSm.setTargetValue(globalBypass.load() ? 1.f : 0.f);
    if (bypassSm.isSmoothing() || bypassSm.getTargetValue() > 0.5f) {
        for (int n = 0; n < numSamples; ++n) {
            const float b = bypassSm.getNextValue();
            for (int ch = 0; ch < numCh; ++ch) {
                float* d = buffer.getWritePointer(ch);
                d[n] = (1.f - b) * d[n] + b * dryBuf.getSample(juce::jmin(ch, 1), n);
            }
        }
    }
    outLevel.store(buffer.getMagnitude(0, numSamples));   // output meter
}

// ── Amp slots ────────────────────────────────────────────────────────────────
// One amp on a mono block: a built-in amp or a NAM model. The block arrives
// already scaled by the GAIN knob (a level into the NAM capture); a built-in
// amp undoes that and uses GAIN as its own drive control instead, so the knob
// sweeps the voicing's designed range from edge-of-breakup to full saturation.
void ArcaneEclipseProcessor::runAmp(AmpSlot& a, const float* in, float* out, int numSamples, float preGain)
{
    if (a.builtin >= 0) {
        const float undo = 1.f / juce::jmax(1.0e-4f, preGain);
        for (int n = 0; n < numSamples; ++n) out[n] = in[n] * undo;
        const float g = apvts.getRawParameterValue(idAmpGain)->load() / 10.f;
        if (g != a.simGain) {                                  // tone is shaped by the shared amp EQ after
            a.sim.setParams(g, 0.5f, 0.5f, 0.5f, 0.5f, 0.7f);
            a.simGain = g;
        }
        a.sim.processBlock(out, numSamples);
        return;
    }
    runNAM(a, in, out, numSamples);
}

// Runs one NAM model on a mono block, resampling to/from 48 kHz when needed.
void ArcaneEclipseProcessor::runNAM(AmpSlot& a, const float* in, float* out, int numSamples)
{
    const double namSR = 48000.0;
    if (std::abs(currentSampleRate - namSR) < 1.0) {           // host already 48 kHz
        a.model->Process(const_cast<float*>(in), out, (size_t) numSamples);
        return;
    }
    const double ratio = namSR / currentSampleRate;
    int namSamples = (int) std::ceil((double) numSamples * ratio) + 4;
    if ((int) a.upIn.size()  < namSamples + 8) a.upIn .assign((size_t)(namSamples + 16), 0.f);
    if ((int) a.upOut.size() < namSamples + 8) a.upOut.assign((size_t)(namSamples + 16), 0.f);

    int actualUp = a.rsIn.process(ratio, in, a.upIn.data(), namSamples, numSamples, 0);   // host -> 48k
    actualUp = juce::jlimit(1, namSamples, actualUp);
    std::fill(a.upOut.begin(), a.upOut.begin() + actualUp + 4, 0.f);
    a.model->Process(a.upIn.data(), a.upOut.data(), (size_t) actualUp);
    int actualDown = a.rsOut.process(1.0 / ratio, a.upOut.data(), out, numSamples, actualUp, 0); // 48k -> host
    actualDown = juce::jlimit(0, numSamples, actualDown);
    for (int n = actualDown; n < numSamples; ++n) out[n] = 0.f;
}

void ArcaneEclipseProcessor::makeMono(const juce::AudioBuffer<float>& b, int numSamples, float* dst) const
{
    const int numCh = b.getNumChannels();
    float magL = b.getMagnitude(0, 0, numSamples);
    float magR = (numCh > 1) ? b.getMagnitude(1, 0, numSamples) : 0.f;
    bool  dualMono = (magL > 1.0e-3f && magR > 1.0e-3f && magL < magR * 4.0f && magR < magL * 4.0f);
    float mScale = dualMono ? 0.5f : 1.0f;
    auto* L = b.getReadPointer(0);
    for (int n = 0; n < numSamples; ++n)
        dst[n] = (numCh > 1) ? mScale * (L[n] + b.getReadPointer(1)[n]) : L[n];
}

// ── Overdrive pedal capture ──────────────────────────────────────────────────
bool ArcaneEclipseProcessor::loadODModel(const juce::File& f, juce::String& error)
{
    juce::File namFile = f;
    if (f.getFileExtension().equalsIgnoreCase(".aecap")) {
        auto c = AecapLoader::loadFile(f);
        if (! c.ok) { error = c.error; return false; }
        namFile = c.materializeModelTo(aecapCacheDir());          // any IR in the pack is ignored for a pedal
    }
    if (! namFile.existsAsFile()) { error = "File not found."; return false; }
    try {
        auto* raw = namLoader.CreateFromFile(namFile.getFullPathName().toStdString());
        if (! raw) { error = "Not a valid NAM model."; return false; }
        std::unique_ptr<NeuralAudio::NeuralModel> model(raw);
        const juce::ScopedLock lock(getCallbackLock());
        odSlot.model = std::move(model);
        odSlot.namName = f.getFileNameWithoutExtension();
        odSlot.namPath = refForFile(f);
        odSlot.rsIn.reset(); odSlot.rsOut.reset();
        std::fill(odSlot.upIn.begin(),  odSlot.upIn.end(),  0.f);
        std::fill(odSlot.upOut.begin(), odSlot.upOut.end(), 0.f);
        odTiltZ[0] = odTiltZ[1] = 0.f;
        return true;
    } catch (...) { error = "The model could not be loaded."; return false; }
}

void ArcaneEclipseProcessor::unloadODModel()
{
    const juce::ScopedLock lock(getCallbackLock());
    odSlot.model.reset(); odSlot.namName = {}; odSlot.namPath = {};
}

bool ArcaneEclipseProcessor::isDualActive() const
{
    return apvts.getRawParameterValue(idDualOn)->load() > .5f
        && (hasAmp(amps[1]) || amps[1].irLoaded);
}

bool ArcaneEclipseProcessor::loadNAMModel(const juce::File& file, int slot)
{
    if (!file.existsAsFile()) return false;
    auto& a = amps[slotIdx(slot)];
    try {
        auto* raw = namLoader.CreateFromFile(file.getFullPathName().toStdString());
        if (!raw) return false;
        std::unique_ptr<NeuralAudio::NeuralModel> model(raw);
        {
            const juce::ScopedLock lock(getCallbackLock());
            a.model = std::move(model);
            a.builtin = -1;
            a.namName = file.getFileNameWithoutExtension();
            a.namPath = refForFile(file);
            a.rsIn.reset(); a.rsOut.reset();
            std::fill(a.upIn.begin(),  a.upIn.end(),  0.f);
            std::fill(a.upOut.begin(), a.upOut.end(), 0.f);
        }
        return true;
    } catch(...) {
        const juce::ScopedLock lock(getCallbackLock());
        a.model = nullptr; a.namName = "(failed)"; return false;
    }
}

bool ArcaneEclipseProcessor::loadIR(const juce::File& file, int slot)
{
    if (!file.existsAsFile()) return false;
    auto& a = amps[slotIdx(slot)];
    a.conv.loadImpulseResponse(file, juce::dsp::Convolution::Stereo::yes,
        juce::dsp::Convolution::Trim::yes, 0, juce::dsp::Convolution::Normalise::yes);
    a.irName = file.getFileNameWithoutExtension();
    a.irPath = refForFile(file);
    a.cab = -1;
    a.irLoaded = true;
    return true;
}

void ArcaneEclipseProcessor::unloadNAMModel(int slot)
{
    auto& a = amps[slotIdx(slot)];
    const juce::ScopedLock lock(getCallbackLock());
    a.model.reset(); a.builtin = -1; a.namName = {}; a.namPath = {};
}

void ArcaneEclipseProcessor::unloadIR(int slot)
{
    auto& a = amps[slotIdx(slot)];
    a.irLoaded = false; a.cab = -1; a.irName = {}; a.irPath = {}; a.conv.reset();
}

juce::File ArcaneEclipseProcessor::aecapCacheDir()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
             .getChildFile("ArcaneEclipse").getChildFile("aecap_cache");
}

// .nam directly, or .aecap (unpack the model + optional embedded IR). For an
// .aecap with an IR, the slot's IR path is recorded as the .aecap itself.
bool ArcaneEclipseProcessor::loadModelAny(const juce::File& f, int slot, juce::String& error)
{
    slot = slotIdx(slot);
    if (! f.getFileExtension().equalsIgnoreCase(".aecap"))
        return loadNAMModel(f, slot);

    auto c = AecapLoader::loadFile(f);
    if (! c.ok) { error = c.error; return false; }
    auto nam = c.materializeModelTo(aecapCacheDir());
    bool ok = nam.existsAsFile() && loadNAMModel(nam, slot);
    if (ok) { amps[slot].namName = f.getFileNameWithoutExtension(); amps[slot].namPath = refForFile(f); }
    if (c.hasIR) {
        auto irf = aecapCacheDir().getChildFile("aecap_" + juce::String(juce::Time::getHighResolutionTicks()) + ".wav");
        if (irf.replaceWithData(c.irWav.getData(), c.irWav.getSize()) && loadIR(irf, slot)) {
            amps[slot].irName = f.getFileNameWithoutExtension();
            amps[slot].irPath = refForFile(f);
        }
    }
    if (! ok) error = "The model inside this .aecap could not be loaded.";
    return ok;
}

// ── Built-in amps + cabinets, library refs (v1.1) ────────────────────────────
static const char* const kAmpKeys[] = { "clean", "crunch", "lead" };
static const char* const kAmpNames[] = { "Eclipse Clean", "Eclipse Crunch", "Eclipse Lead" };
static const char* const kCabKeys[] = { "1x12", "2x12", "4x12" };
static const char* const kCabNames[] = { "Eclipse 1x12 Open", "Eclipse 2x12", "Eclipse 4x12" };

juce::String ArcaneEclipseProcessor::builtinAmpName(int i) { return kAmpNames[juce::jlimit(0, kNumBuiltinAmps - 1, i)]; }
juce::String ArcaneEclipseProcessor::builtinCabName(int i) { return kCabNames[juce::jlimit(0, kNumBuiltinCabs - 1, i)]; }
juce::String ArcaneEclipseProcessor::builtinAmpRef(int i)  { return juce::String("builtin:amp/") + kAmpKeys[juce::jlimit(0, kNumBuiltinAmps - 1, i)]; }
juce::String ArcaneEclipseProcessor::builtinCabRef(int i)  { return juce::String("builtin:cab/") + kCabKeys[juce::jlimit(0, kNumBuiltinCabs - 1, i)]; }
int ArcaneEclipseProcessor::builtinAmpFromRef(const juce::String& ref) {
    for (int i = 0; i < kNumBuiltinAmps; ++i) if (ref == builtinAmpRef(i)) return i;
    return -1;
}
int ArcaneEclipseProcessor::builtinCabFromRef(const juce::String& ref) {
    for (int i = 0; i < kNumBuiltinCabs; ++i) if (ref == builtinCabRef(i)) return i;
    return -1;
}

juce::File ArcaneEclipseProcessor::libraryRoot()
{
   #if JUCE_MAC
    return juce::File("/Users/Shared/Amari Labs/Arcane Eclipse");
   #elif JUCE_WINDOWS
    return juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)     // C:\ProgramData
             .getChildFile("Amari Labs").getChildFile("Arcane Eclipse");
   #else
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
             .getChildFile("Amari Labs").getChildFile("Arcane Eclipse");
   #endif
}

juce::String ArcaneEclipseProcessor::refForFile(const juce::File& f)
{
    const auto root = libraryRoot();
    if (f.isAChildOf(root))
        return "library:" + f.getRelativePathFrom(root).replaceCharacter('\\', '/');
    return f.getFullPathName();
}

// Library refs resolve against this computer's library folder. An absolute path
// that doesn't exist here (a preset made on Windows opened on a Mac, or a moved
// library) falls back to a file with the same name anywhere in the library.
juce::File ArcaneEclipseProcessor::resolveRef(const juce::String& ref)
{
    if (ref.isEmpty() || isBuiltinRef(ref)) return {};
    const auto root = libraryRoot();
    if (ref.startsWith("library:")) {
        auto f = root.getChildFile(ref.fromFirstOccurrenceOf("library:", false, false));
        if (f.existsAsFile()) return f;
    } else if (juce::File::isAbsolutePath(ref)) {
        juce::File f(ref);
        if (f.existsAsFile()) return f;
    }
    const auto name = ref.fromLastOccurrenceOf("/", false, false).fromLastOccurrenceOf("\\", false, false);
    if (name.isNotEmpty() && root.isDirectory())
        for (const auto& e : juce::RangedDirectoryIterator(root, true, "*", juce::File::findFiles))
            if (e.getFile().getFileName().equalsIgnoreCase(name)) return e.getFile();
    return {};
}

bool ArcaneEclipseProcessor::loadBuiltinAmp(int v, int slot)
{
    if (v < 0 || v >= kNumBuiltinAmps) return false;
    auto& a = amps[slotIdx(slot)];
    const juce::ScopedLock lock(getCallbackLock());
    a.model.reset();
    a.builtin = v;
    a.sim.setVoicing((AmpSim::Voicing) v);
    a.sim.reset();
    a.simGain = -1.f;
    a.namName = builtinAmpName(v);
    a.namPath = builtinAmpRef(v);
    return true;
}

bool ArcaneEclipseProcessor::loadBuiltinCab(int c, int slot)
{
    if (c < 0 || c >= kNumBuiltinCabs) return false;
    static const void* const data[] = { BinaryData::cab_eclipse_1x12_wav, BinaryData::cab_eclipse_2x12_wav, BinaryData::cab_eclipse_4x12_wav };
    static const int sizes[] = { BinaryData::cab_eclipse_1x12_wavSize, BinaryData::cab_eclipse_2x12_wavSize, BinaryData::cab_eclipse_4x12_wavSize };
    auto& a = amps[slotIdx(slot)];
    a.conv.loadImpulseResponse(data[c], (size_t) sizes[c], juce::dsp::Convolution::Stereo::yes,
        juce::dsp::Convolution::Trim::yes, 0, juce::dsp::Convolution::Normalise::yes);
    a.irName = builtinCabName(c);
    a.irPath = builtinCabRef(c);
    a.cab = c;
    a.irLoaded = true;
    return true;
}

bool ArcaneEclipseProcessor::loadModelRef(const juce::String& ref, int slot, juce::String& error)
{
    const int b = builtinAmpFromRef(ref);
    if (b >= 0) return loadBuiltinAmp(b, slot);
    auto f = resolveRef(ref);
    if (! f.existsAsFile()) { error = "File not found."; return false; }
    return loadModelAny(f, slot, error);
}

bool ArcaneEclipseProcessor::loadIRRef(const juce::String& ref, int slot)
{
    const int c = builtinCabFromRef(ref);
    if (c >= 0) return loadBuiltinCab(c, slot);
    auto f = resolveRef(ref);
    return f.existsAsFile() && loadIR(f, slot);
}

void ArcaneEclipseProcessor::loadDefaultRig()
{
    loadBuiltinAmp(0, 0);
    loadBuiltinCab(0, 0);
    unloadNAMModel(1);
    unloadIR(1);
}

void ArcaneEclipseProcessor::getStateInformation(juce::MemoryBlock& d)
{
    if (auto xml = apvts.copyState().createXml()) {
        auto* mm = xml->createNewChildElement("MIDIMAP");
        for (int c = 0; c < 128; ++c) {
            int pi = ccMap[c].load();
            if (pi >= 0 && pi < (int) learnParamIDs.size()) {
                auto* e = mm->createNewChildElement("M");
                e->setAttribute("cc", c);
                e->setAttribute("param", learnParamIDs[pi]);
            }
        }
        auto* am = xml->createNewChildElement("AMPS");          // v1.1: recall loaded files
        for (int i = 0; i < kNumAmpSlots; ++i) {
            auto* e = am->createNewChildElement("A");
            e->setAttribute("nam", amps[i].namPath);
            e->setAttribute("ir",  amps[i].irPath);
        }
        am->setAttribute("od", odSlot.namPath);                  // overdrive pedal capture
        copyXmlToBinary(*xml, d);
    }
}

void ArcaneEclipseProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        for (auto& c : ccMap) c.store(-1);
        if (auto* mm = xml->getChildByName("MIDIMAP")) {
            for (auto* e : mm->getChildIterator()) {
                int cc = e->getIntAttribute("cc", -1);
                int idx = indexOfParam(e->getStringAttribute("param"));
                if (cc >= 0 && cc < 128 && idx >= 0) ccMap[cc].store(idx);
            }
            xml->removeChildElement(mm, true);
        }
        juce::StringArray namP, irP;
        juce::String odP; bool haveAmps = false;
        if (auto* am = xml->getChildByName("AMPS")) {
            haveAmps = true; odP = am->getStringAttribute("od");
            for (auto* e : am->getChildIterator()) {
                namP.add(e->getStringAttribute("nam")); irP.add(e->getStringAttribute("ir"));
            }
            xml->removeChildElement(am, true);
        }
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        // Reload each amp's model + IR (sessions saved before v1.1 have no AMPS block)
        for (int i = 0; i < juce::jmin(kNumAmpSlots, namP.size()); ++i) {
            juce::String err;
            if (namP[i].isEmpty()) unloadNAMModel(i);
            else if (namP[i] != amps[i].namPath && ! loadModelRef(namP[i], i, err)) unloadNAMModel(i);
            if (irP[i].isEmpty()) unloadIR(i);
            else if (irP[i] != namP[i] && irP[i] != amps[i].irPath) loadIRRef(irP[i], i);
        }
        if (haveAmps) {
            juce::String err;
            auto odf = resolveRef(odP);
            if (odP.isNotEmpty() && odf.existsAsFile()) { if (odP != odSlot.namPath) loadODModel(odf, err); }
            else unloadODModel();
        }
    }
}

// ── MIDI learn ──────────────────────────────────────────────────────────────
int ArcaneEclipseProcessor::indexOfParam(const juce::String& id) const {
    for (int i = 0; i < (int) learnParamIDs.size(); ++i) if (learnParamIDs[i] == id) return i;
    return -1;
}
void ArcaneEclipseProcessor::midiLearnStart(const juce::String& paramID) {
    learnTarget.store(indexOfParam(paramID));
}
void ArcaneEclipseProcessor::midiLearnClear(const juce::String& paramID) {
    int idx = indexOfParam(paramID);
    if (idx < 0) return;
    for (int c = 0; c < 128; ++c) if (ccMap[c].load() == idx) ccMap[c].store(-1);
    if (learnTarget.load() == idx) learnTarget.store(-1);
}
juce::String ArcaneEclipseProcessor::midiLearningParamID() const {
    int t = learnTarget.load();
    return (t >= 0 && t < (int) learnParamIDs.size()) ? learnParamIDs[t] : juce::String();
}
int ArcaneEclipseProcessor::ccForParam(const juce::String& paramID) const {
    int idx = indexOfParam(paramID);
    if (idx < 0) return -1;
    for (int c = 0; c < 128; ++c) if (ccMap[c].load() == idx) return c;
    return -1;
}

void ArcaneEclipseProcessor::actionLearnStart(int a) { actionLearn.store(a); learnTarget.store(-1); }
void ArcaneEclipseProcessor::actionLearnClear(int a) {
    for (int c = 0; c < 128; ++c) if (actionCC[c].load() == a) actionCC[c].store(-1);
    if (actionLearn.load() == a) actionLearn.store(-1);
}
int  ArcaneEclipseProcessor::ccForAction(int a) const {
    for (int c = 0; c < 128; ++c) if (actionCC[c].load() == a) return c;
    return -1;
}
int  ArcaneEclipseProcessor::actionLearningNow() const { return actionLearn.load(); }
int  ArcaneEclipseProcessor::takePendingAction() { return actionPending.exchange(-1); }
void ArcaneEclipseProcessor::cancelLearn() { learnTarget.store(-1); actionLearn.store(-1); }

// ── Tap tempo ────────────────────────────────────────────────────────────────
void ArcaneEclipseProcessor::tapTempo()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    float beat = 0.f;
    timeBeforeTap = apvts.getRawParameterValue(idDelayTime)->load();
    beatBeforeTap = tapBeatMs.load();
    {
        const juce::SpinLock::ScopedLockType sl(tapLock);
        if (tapCount > 0 && now - tapTimes[(tapCount - 1) % 4] > 2500.0) tapCount = 0;   // >2.5 s gap = start over
        tapTimes[tapCount % 4] = now; ++tapCount;
        if (tapCount < 2) { lastTapMs.store(now); return; }
        int k = juce::jmin(tapCount, 4);                      // average up to the last 3 intervals
        double first = tapTimes[(tapCount - k) % 4];
        beat = (float) ((now - first) / (double) (k - 1));
    }
    lastTapMs.store(now);
    tapBeatMs.store(beat);
    applyTapDivision();
}

void ArcaneEclipseProcessor::applyTapDivision()
{
    float beat = tapBeatMs.load();
    if (beat <= 0.f) return;
    static const float divMul[4] = { 1.0f, 0.75f, 0.5f, 1.0f / 3.0f };   // 1/4, dotted 1/8, 1/8, 1/8 triplet
    int div = juce::jlimit(0, 3, (int) apvts.getRawParameterValue(idDelayTapDiv)->load());
    float ms = juce::jlimit(20.f, 2000.f, beat * divMul[div]);
    if (auto* t = apvts.getParameter(idDelayTime)) t->setValueNotifyingHost(t->convertTo0to1(ms));
}

void ArcaneEclipseProcessor::undoLastTap()
{
    {
        const juce::SpinLock::ScopedLockType sl(tapLock);
        if (tapCount > 0) --tapCount;
    }
    tapBeatMs.store(beatBeforeTap);
    if (timeBeforeTap > 0.f)
        if (auto* t = apvts.getParameter(idDelayTime)) t->setValueNotifyingHost(t->convertTo0to1(timeBeforeTap));
}

// ── Tuner pitch detection (YIN) ───────────────────────────────────────────────
// YIN difference + cumulative-mean-normalised difference with an absolute
// threshold. Far more octave-robust than plain autocorrelation (handles a weak
// or missing fundamental, which is what caused octave flicker). Uses the
// preallocated tunerD/tunerDP scratch so it stays real-time safe.
float ArcaneEclipseProcessor::detectPitch(const float* buf, int n, double sr)
{
    double energy = 0.0;
    for (int i = 0; i < n; ++i) energy += (double) buf[i] * buf[i];
    if (energy <= 0.0) return 0.0f;
    if (std::sqrt(energy / (double) n) < 0.004) return 0.0f;   // level gate

    const int minLag = juce::jmax(2, (int) (sr / 1200.0));     // up to ~1200 Hz
    int maxLag = juce::jmin(n / 2, (int) (sr / 55.0));         // down to ~55 Hz
    if ((int) tunerD.size() <= maxLag) maxLag = (int) tunerD.size() - 1;
    if (maxLag <= minLag) return 0.0f;

    // difference function d(tau)
    for (int tau = 1; tau <= maxLag; ++tau) {
        double sum = 0.0;
        for (int i = 0; i < n - tau; ++i) { double df = (double) buf[i] - buf[i + tau]; sum += df * df; }
        tunerD[(size_t) tau] = sum;
    }
    // cumulative mean normalised difference d'(tau)
    double run = 0.0; tunerDP[0] = 1.0;
    for (int tau = 1; tau <= maxLag; ++tau) {
        run += tunerD[(size_t) tau];
        tunerDP[(size_t) tau] = (run > 0.0) ? tunerD[(size_t) tau] * (double) tau / run : 1.0;
    }
    // absolute threshold: first dip below threshold, descend to its local min
    const double thresh = 0.12;
    int best = -1;
    for (int tau = minLag; tau < maxLag; ++tau) {
        if (tunerDP[(size_t) tau] < thresh) {
            while (tau + 1 <= maxLag && tunerDP[(size_t) (tau + 1)] < tunerDP[(size_t) tau]) ++tau;
            best = tau; break;
        }
    }
    if (best < 0) {                                            // fallback: global min
        double mn = 1e9;
        for (int tau = minLag; tau <= maxLag; ++tau)
            if (tunerDP[(size_t) tau] < mn) { mn = tunerDP[(size_t) tau]; best = tau; }
        if (best < 0 || mn > 0.6) return 0.0f;                 // not periodic enough
    }
    // parabolic interpolation around the chosen lag
    double tauEst = best;
    if (best > minLag && best < maxLag) {
        double a = tunerDP[(size_t) (best - 1)], b = tunerDP[(size_t) best], c = tunerDP[(size_t) (best + 1)];
        double den = a - 2.0 * b + c;
        if (std::abs(den) > 1e-12) tauEst = best + 0.5 * (a - c) / den;
    }
    return (tauEst > 0.0) ? (float) (sr / tauEst) : 0.0f;
}

// Octave-error correction + 5-frame median smoothing.
// Only a jump of almost exactly an octave (+-3 %) away from the running estimate
// is treated as a detection error and folded back; any other jump (changing to
// another string) is accepted at once. A genuine octave change (e.g. a 12th-fret
// note) is accepted once it persists for 5 frames (~0.2 s). Silence resets.
// (v1.1 folded EVERY large jump into the previous note's octave, so moving
// G3 -> E4 read "E3".)
float ArcaneEclipseProcessor::smoothTunerPitch(float raw)
{
    if (raw <= 0.0f) { tunerHistCount = 0; tunerStable = 0.0f; tunerOctRun = 0; return 0.0f; }
    if (tunerStable > 0.0f) {
        const float r = raw / tunerStable;
        const bool octUp = std::abs(r - 2.0f) < 0.06f, octDn = std::abs(r - 0.5f) < 0.015f;
        if (octUp || octDn) {
            if (++tunerOctRun < 5) raw = octUp ? raw * 0.5f : raw * 2.0f;   // fold back a probable error
            else { tunerHistCount = 0; tunerStable = 0.0f; tunerOctRun = 0; }  // persisted: real octave change
        } else {
            tunerOctRun = 0;
            if (r > 1.06f || r < 0.94f) { tunerHistCount = 0; tunerStable = 0.0f; }  // new note: restart smoothing
        }
    }
    // rolling 5-sample median
    for (int i = juce::jmin(tunerHistCount, 4); i > 0; --i) tunerHist[i] = tunerHist[i - 1];
    tunerHist[0] = raw;
    if (tunerHistCount < 5) ++tunerHistCount;
    float tmp[5]; for (int i = 0; i < tunerHistCount; ++i) tmp[i] = tunerHist[i];
    for (int i = 0; i < tunerHistCount; ++i)
        for (int j = i + 1; j < tunerHistCount; ++j)
            if (tmp[j] < tmp[i]) std::swap(tmp[i], tmp[j]);
    float med = tmp[tunerHistCount / 2];
    tunerStable = (tunerStable > 0.0f) ? 0.8f * tunerStable + 0.2f * med : med;
    return med;
}

juce::AudioProcessorEditor* ArcaneEclipseProcessor::createEditor() { return new ArcaneEclipseEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ArcaneEclipseProcessor(); }
