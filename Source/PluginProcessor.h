#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
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
    bool acceptsMidi() const override { return false; }
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
    juce::String getLoadedNAMName() const { return loadedNAMName; }
    juce::String getLoadedIRName()  const { return loadedIRName; }
    bool isNAMLoaded() const { return namModel != nullptr; }
    bool isIRLoaded()  const { return irLoaded; }

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ── Parameter IDs ──────────────────────────────────────────────────────────
    static constexpr auto idInputGain   = "inputGain";
    static constexpr auto idOutputGain  = "outputGain";
    static constexpr auto idNoiseGate   = "noiseGate";
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

    juce::LagrangeInterpolator resamplerIn, resamplerOut;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArcaneEclipseProcessor)
};
