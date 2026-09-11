#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "AELicense.h"
#include <atomic>
#include <vector>
#include "NeuralModel.h"
#include "Compressor.h"
#include "Overdrive.h"
#include "Modulation.h"
#include "RoomReverb.h"
#include "StandardDelay.h"

class ArcaneEclipseProcessor : public juce::AudioProcessor
{
public:
    ArcaneEclipseProcessor();
    ~ArcaneEclipseProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Arcane Eclipse"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    bool isBusesLayoutSupported(const BusesLayout& l) const override;
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    bool loadNAMModel(const juce::File& f);
    bool loadIR(const juce::File& f);
    void unloadNAMModel() { const juce::ScopedLock lock(getCallbackLock()); namModel.reset(); loadedNAMName = ""; }
    void unloadIR()       { irLoaded = false; loadedIRName = ""; convolution.reset(); }
    juce::String getLoadedNAMName() const { return loadedNAMName; }
    juce::String getLoadedIRName()  const { return loadedIRName; }
    bool isNAMLoaded() const { return namModel != nullptr; }
    bool isIRLoaded()  const { return irLoaded; }

    // MIDI learn
    void midiLearnStart(const juce::String& paramID);
    void midiLearnClear(const juce::String& paramID);
    juce::String midiLearningParamID() const;
    int  ccForParam(const juce::String& paramID) const;

    // Tuner — the editor sets tunerActive when open; processor fills tunerFreq
    std::atomic<bool>  tunerActive { false };
    std::atomic<float> tunerFreq   { 0.f };

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ── Parameter IDs ──────────────────────────────────────────────────────────
    static constexpr auto idInputGain   = "inputGain";
    static constexpr auto idOutputGain  = "outputGain";
    static constexpr auto idNoiseGate   = "noiseGate";
    static constexpr auto idGateOn      = "gateOn";
    static constexpr auto idCabBypass   = "cabBypass";
    // Amp
    static constexpr auto idAmpGain     = "ampGain";
    static constexpr auto idAmpBass     = "ampBass";
    static constexpr auto idAmpMid      = "ampMid";
    static constexpr auto idAmpTreble   = "ampTreble";
    static constexpr auto idAmpPresence = "ampPresence";
    static constexpr auto idAmpMaster   = "ampMaster";
    // Compressor
    static constexpr auto idCompOn      = "compOn";
    static constexpr auto idCompThresh  = "compThresh";
    static constexpr auto idCompRatio   = "compRatio";
    static constexpr auto idCompAttack  = "compAttack";
    static constexpr auto idCompRelease = "compRelease";
    static constexpr auto idCompMakeup  = "compMakeup";
    // Overdrive
    static constexpr auto idODOn        = "odOn";
    static constexpr auto idODDrive     = "odDrive";
    static constexpr auto idODTone      = "odTone";
    static constexpr auto idODLevel     = "odLevel";
    // Modulation
    static constexpr auto idModOn       = "modOn";
    static constexpr auto idModRate     = "modRate";
    static constexpr auto idModDepth    = "modDepth";
    static constexpr auto idModMix      = "modMix";
    static constexpr auto idModType     = "modType";
    // Delay
    static constexpr auto idDelayOn       = "delayOn";
    static constexpr auto idDelayTime     = "delayTime";
    static constexpr auto idDelayFeedback = "delayFeedback";
    static constexpr auto idDelayMix      = "delayMix";
    static constexpr auto idDelayType     = "delayType";
    // Reverb
    static constexpr auto idReverbOn    = "reverbOn";
    static constexpr auto idReverbDecay = "reverbDecay";
    static constexpr auto idReverbSize  = "reverbSize";
    static constexpr auto idReverbMix   = "reverbMix";
    static constexpr auto idReverbType  = "reverbType";

private:
    std::unique_ptr<NeuralAudio::NeuralModel> namModel;
    NeuralAudio::NeuralModelLoader namLoader;
    juce::String loadedNAMName, loadedIRName;
    bool irLoaded = false;

    juce::CatmullRomInterpolator resamplerIn, resamplerOut;
    double currentSampleRate = 44100.0;
    std::vector<float> resampleBufIn, resampleBufOut, monoBuf, namOutBuf;

    juce::dsp::Convolution convolution;
    OpticalCompressor compressor;
    TubeScreamerDrive overdrive;
    ModulationFX      modulation;
    StandardDelay     delay;
    RoomReverb        reverb;

    juce::dsp::IIR::Filter<float> bassFilter[2], midFilter[2], trebleFilter[2], presenceFilter[2];
    void updateEQ();
    float gateEnvelope = 0.f;
    float dcX1[2] = {0.f,0.f}, dcY1[2] = {0.f,0.f};  // DC blocker state

    // MIDI learn state
    std::vector<juce::String> learnParamIDs;
    std::vector<juce::RangedAudioParameter*> learnParamPtrs;
    std::atomic<int> ccMap[128];
    std::atomic<int> learnTarget { -1 };
    std::vector<bool> learnIsToggle;        // parallel to learnParamIDs
    int prevCCVal[128] = { 0 };             // for footswitch rising-edge detection
    int indexOfParam(const juce::String& id) const;
    // Tuner
    std::vector<float> tunerBuf;
    int tunerFill = 0;
    static float detectPitch(const float* buf, int n, double sr);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArcaneEclipseProcessor)
};
