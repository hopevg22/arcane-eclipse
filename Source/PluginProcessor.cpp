#include "PluginProcessor.h"
#include "PluginEditor.h"

ArcaneEclipseProcessor::ArcaneEclipseProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{}

juce::AudioProcessorValueTreeState::ParameterLayout ArcaneEclipseProcessor::createParameterLayout()
{
    using Range = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back(std::make_unique<juce::AudioParameterFloat>(idInputGain,  "Input Gain",  Range(-20.f,20.f,.1f), 0.f, "dB"));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idOutputGain, "Output Gain", Range(-20.f,20.f,.1f), 0.f, "dB"));
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
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idModRate,  "Mod Rate", Range(.1f,10.f,.1f),1.f,"Hz"));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idModDepth, "Mod Depth",Range(0.f,1.f,.01f),.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idModMix,   "Mod Mix",  Range(0.f,1.f,.01f),.5f));
    p.push_back(std::make_unique<juce::AudioParameterInt>  (idModType,  "Mod Type", 0, 2, 0));

    p.push_back(std::make_unique<juce::AudioParameterBool> (idDelayOn,       "Delay On",  false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idDelayTime,     "Delay Time",
                 juce::NormalisableRange<float>(20.f,2000.f,1.f,.4f),350.f,"ms"));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idDelayFeedback, "Feedback",  Range(0.f,.97f,.01f),.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idDelayMix,      "Delay Mix", Range(0.f,1.f,.01f),.35f));
    p.push_back(std::make_unique<juce::AudioParameterInt>  (idDelayType,     "Delay Type",0, 3, 0));

    p.push_back(std::make_unique<juce::AudioParameterBool> (idReverbOn,    "Reverb On",  false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idReverbDecay, "Reverb Decay",Range(0.f,1.f,.01f),.4f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idReverbSize,  "Reverb Size", Range(0.f,1.f,.01f),.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(idReverbMix,   "Reverb Mix",  Range(0.f,1.f,.01f),.3f));
    p.push_back(std::make_unique<juce::AudioParameterInt>  (idReverbType,  "Reverb Type", 0, 3, 0));

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

void ArcaneEclipseProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    int numSamples = buffer.getNumSamples(), numCh = buffer.getNumChannels();

    // 1. INPUT GAIN
    buffer.applyGain(juce::Decibels::decibelsToGain(apvts.getRawParameterValue(idInputGain)->load()));

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

            auto* L = buffer.getReadPointer(0);
            for (int n = 0; n < numSamples; ++n)
                monoBuf[(size_t)n] = numCh > 1
                    ? 0.5f * (L[n] + buffer.getReadPointer(1)[n]) : L[n];

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
            auto* L = buffer.getReadPointer(0);
            for (int n = 0; n < numSamples; ++n)
                monoBuf[(size_t)n] = numCh > 1
                    ? 0.5f * (L[n] + buffer.getReadPointer(1)[n]) : L[n];

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
    if (apvts.getRawParameterValue(idDelayOn)->load() > .5f) {
        delay.setParameters(
            apvts.getRawParameterValue(idDelayTime)->load(),
            apvts.getRawParameterValue(idDelayFeedback)->load(),
            0.7f,
            apvts.getRawParameterValue(idDelayMix)->load(), false);
        delay.processBlock(buffer);
    }

    // 12. REVERB — always last
    if (apvts.getRawParameterValue(idReverbOn)->load() > .5f) {
        reverb.setParameters(
            apvts.getRawParameterValue(idReverbDecay)->load(),
            0.f, .5f,
            apvts.getRawParameterValue(idReverbSize)->load(),
            .6f, .2f,
            apvts.getRawParameterValue(idReverbMix)->load());
        reverb.processBlock(buffer);
    }

    // 13. OUTPUT GAIN
    buffer.applyGain(juce::Decibels::decibelsToGain(apvts.getRawParameterValue(idOutputGain)->load()));
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
{ if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, d); }

void ArcaneEclipseProcessor::setStateInformation(const void* data, int sizeInBytes)
{ if (auto xml = getXmlFromBinary(data, sizeInBytes)) apvts.replaceState(juce::ValueTree::fromXml(*xml)); }

juce::AudioProcessorEditor* ArcaneEclipseProcessor::createEditor() { return new ArcaneEclipseEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ArcaneEclipseProcessor(); }
