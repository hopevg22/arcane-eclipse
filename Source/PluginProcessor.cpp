#include "PluginProcessor.h"
#include <cmath>
#include "PluginEditor.h"

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
        idGateOn, idCompOn, idODOn, idModOn, idDelayOn, idReverbOn
    };
    for (auto& id : learnParamIDs) learnParamPtrs.push_back(apvts.getParameter(id));
    learnIsToggle.assign(learnParamIDs.size(), false);
    for (int i = 0; i < (int) learnParamIDs.size(); ++i) {
        auto& id = learnParamIDs[i];
        if (id == idGateOn || id == idCompOn || id == idODOn ||
            id == idModOn  || id == idDelayOn || id == idReverbOn)
            learnIsToggle[i] = true;
    }
    for (auto& c : ccMap) c.store(-1);
    for (auto& c : actionCC) c.store(-1);
    for (auto& v : prevCCVal) v = 0;
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

    p.push_back(std::make_unique<juce::AudioParameterBool> (idReverbOn,    "Reverb On",  false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idReverbHighCut,"Reverb High Cut",Range(0.f,1.f,.01f),.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idReverbDecay, "Reverb Decay",Range(0.f,1.f,.01f),.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idReverbSize,  "Reverb Size", Range(0.f,1.f,.01f),.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idReverbMix,   "Reverb Mix",  Range(0.f,1.f,.01f),.5f));
    p.push_back(std::make_unique<juce::AudioParameterInt>  (idReverbType,  "Reverb Type", 0, 3, 0));
    p.push_back(std::make_unique<juce::AudioParameterBool> (idReverbShimmer, "Shimmer", false));

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
    convolution.prepare(spec); convolution.reset();
    resampleBufIn .assign((size_t)(samplesPerBlock * 3 + 32), 0.f);
    resampleBufOut.assign((size_t)(samplesPerBlock * 3 + 32), 0.f);
    monoBuf       .assign((size_t)(samplesPerBlock + 32),     0.f);
    namOutBuf     .assign((size_t)(samplesPerBlock + 32),     0.f);
    resamplerIn.reset(); resamplerOut.reset();
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
    static float lastB=-999,lastM=-999,lastT=-999,lastP=-999;
    static double lastSR = 0;
    if (std::abs(bDb-lastB)<0.01f && std::abs(mDb-lastM)<0.01f &&
        std::abs(tDb-lastT)<0.01f && std::abs(pDb-lastP)<0.01f &&
        std::abs(sr-lastSR)<1.0)
        return; // nothing changed — keep existing coefficients

    lastB=bDb; lastM=mDb; lastT=tDb; lastP=pDb; lastSR=sr;

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
    // License gate: silence all audio if not activated
    if (!AELicenseManager::getInstance().isActivated())
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
                        if (prevCCVal[idx] < 64 && val >= 64) {
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
        overdrive.setParameters(
            apvts.getRawParameterValue(idODDrive)->load(),
            apvts.getRawParameterValue(idODTone)->load(),
            apvts.getRawParameterValue(idODLevel)->load());
        overdrive.processBlock(buffer);
    }

    // 5. AMP GAIN — pre-NAM input level
    float ampGainDb = juce::jmap(apvts.getRawParameterValue(idAmpGain)->load(), 0.f,10.f,-6.f,18.f);
    buffer.applyGain(juce::Decibels::decibelsToGain(ampGainDb));

    // 6. NAM MODEL — the amp simulation
    if (namModel != nullptr)
    {
        const double namSR = 48000.0;

        if (std::abs(currentSampleRate - namSR) < 1.0)
        {
            // Host is already 48kHz — no resampling needed, direct processing
            // Mix to mono in-place
            if ((int)monoBuf.size() < numSamples + 8)
                monoBuf.assign((size_t)(numSamples + 16), 0.f);
            if ((int)namOutBuf.size() < numSamples + 8)
                namOutBuf.assign((size_t)(numSamples + 16), 0.f);

            // Mono input for the amp - consistent level whether the host feeds a
            // mono guitar duplicated on both channels (DAW) or on a single input
            // channel (audio interface, either input). Average only when both
            // channels carry comparable signal; otherwise sum, so a single-channel
            // guitar always reaches the amp at full level regardless of which input.
            float magL = buffer.getMagnitude(0, 0, numSamples);
            float magR = (numCh > 1) ? buffer.getMagnitude(1, 0, numSamples) : 0.f;
            bool  dualMono = (magL > 1.0e-3f && magR > 1.0e-3f
                              && magL < magR * 4.0f && magR < magL * 4.0f);
            float mScale = dualMono ? 0.5f : 1.0f;
            auto* L = buffer.getReadPointer(0);
            for (int n = 0; n < numSamples; ++n)
                monoBuf[(size_t)n] = (numCh > 1)
                    ? mScale * (L[n] + buffer.getReadPointer(1)[n]) : L[n];

            namModel->Process(monoBuf.data(), namOutBuf.data(), (size_t)numSamples);

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* dst = buffer.getWritePointer(ch);
                for (int n = 0; n < numSamples; ++n)
                    dst[n] = namOutBuf[(size_t)n];
            }
        }
        else
        {
            // Resample using JUCE's interpolators with proper block sizing
            const double ratio = namSR / currentSampleRate;
            int namSamples = (int)std::ceil((double)numSamples * ratio) + 4;

            if ((int)resampleBufIn.size()  < namSamples + 8)
                resampleBufIn.assign((size_t)(namSamples + 16), 0.f);
            if ((int)resampleBufOut.size() < namSamples + 8)
                resampleBufOut.assign((size_t)(namSamples + 16), 0.f);
            if ((int)monoBuf.size()   < numSamples + 8)
                monoBuf.assign((size_t)(numSamples + 16), 0.f);
            if ((int)namOutBuf.size() < numSamples + 8)
                namOutBuf.assign((size_t)(numSamples + 16), 0.f);

            // Mix to mono
            // Mono input for the amp - consistent level whether the host feeds a
            // mono guitar duplicated on both channels (DAW) or on a single input
            // channel (audio interface, either input). Average only when both
            // channels carry comparable signal; otherwise sum, so a single-channel
            // guitar always reaches the amp at full level regardless of which input.
            float magL = buffer.getMagnitude(0, 0, numSamples);
            float magR = (numCh > 1) ? buffer.getMagnitude(1, 0, numSamples) : 0.f;
            bool  dualMono = (magL > 1.0e-3f && magR > 1.0e-3f
                              && magL < magR * 4.0f && magR < magL * 4.0f);
            float mScale = dualMono ? 0.5f : 1.0f;
            auto* L = buffer.getReadPointer(0);
            for (int n = 0; n < numSamples; ++n)
                monoBuf[(size_t)n] = (numCh > 1)
                    ? mScale * (L[n] + buffer.getReadPointer(1)[n]) : L[n];

            // Upsample host -> 48kHz
            int actualUp = resamplerIn.process(
                ratio, monoBuf.data(), resampleBufIn.data(),
                namSamples, numSamples, 0);
            actualUp = juce::jlimit(1, namSamples, actualUp);

            // NAM inference
            std::fill(resampleBufOut.begin(),
                      resampleBufOut.begin() + actualUp + 4, 0.f);
            namModel->Process(resampleBufIn.data(),
                              resampleBufOut.data(), (size_t)actualUp);

            // Downsample 48kHz -> host
            int actualDown = resamplerOut.process(
                1.0 / ratio, resampleBufOut.data(),
                namOutBuf.data(), numSamples, actualUp, 0);
            actualDown = juce::jlimit(0, numSamples, actualDown);

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* dst = buffer.getWritePointer(ch);
                for (int n = 0; n < actualDown; ++n)
                    dst[n] = namOutBuf[(size_t)n];
                for (int n = actualDown; n < numSamples; ++n)
                    dst[n] = 0.f;
            }
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

    // 9. CABINET IR — speaker simulation
    if (irLoaded) {
        juce::dsp::AudioBlock<float> block(buffer);
        convolution.process(juce::dsp::ProcessContextReplacing<float>(block));
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
    outLevel.store(buffer.getMagnitude(0, numSamples));   // output meter
}

bool ArcaneEclipseProcessor::loadNAMModel(const juce::File& file)
{
    if (!file.existsAsFile()) return false;
    try {
        auto* raw = namLoader.CreateFromFile(file.getFullPathName().toStdString());
        if (!raw) return false;
        std::unique_ptr<NeuralAudio::NeuralModel> model(raw);
        { const juce::ScopedLock lock(getCallbackLock()); namModel = std::move(model); loadedNAMName = file.getFileNameWithoutExtension(); }
        resamplerIn.reset(); resamplerOut.reset();
    tunerBuf.assign(2048, 0.f); tunerFill = 0; tunerFreq.store(0.f);
    tunerStable = 0.f; tunerHistCount = 0;
        std::fill(resampleBufIn.begin(),  resampleBufIn.end(),  0.f);
        std::fill(resampleBufOut.begin(), resampleBufOut.end(), 0.f);
        std::fill(monoBuf.begin(),        monoBuf.end(),        0.f);
        std::fill(namOutBuf.begin(),      namOutBuf.end(),      0.f);
        return true;
    } catch(...) { namModel = nullptr; loadedNAMName = "(failed)"; return false; }
}

bool ArcaneEclipseProcessor::loadIR(const juce::File& file)
{
    if (!file.existsAsFile()) return false;
    convolution.loadImpulseResponse(file, juce::dsp::Convolution::Stereo::yes,
        juce::dsp::Convolution::Trim::yes, 0, juce::dsp::Convolution::Normalise::yes);
    loadedIRName = file.getFileNameWithoutExtension();
    irLoaded = true;
    return true;
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
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
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

// Octave-snap + 5-frame median smoothing: kills residual octave jumps and
// jitter so the readout holds steady.
float ArcaneEclipseProcessor::smoothTunerPitch(float raw)
{
    if (raw <= 0.0f) { tunerHistCount = 0; return 0.0f; }     // reset on silence
    if (tunerStable > 0.0f) {
        while (raw > 1.5f  * tunerStable) raw *= 0.5f;        // snap toward the running estimate
        while (raw < 0.67f * tunerStable) raw *= 2.0f;
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
