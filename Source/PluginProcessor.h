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
#include "Doubler.h"
#include "AmpSim.h"

class ArcaneEclipseProcessor : public juce::AudioProcessor
{
public:
    // v1.1: startup screen shows once per plugin load (not on every window open)
    bool splashShown = false;

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

    // ── Amp slots (v1.1 Dual Amp/IR) ─────────────────────────────────────────
    // Slot 0 = amp 1 (always used), slot 1 = amp 2 (used when Dual is on).
    static constexpr int kNumAmpSlots = 2;
    bool loadNAMModel(const juce::File& f, int slot = 0);
    bool loadIR(const juce::File& f, int slot = 0);
    // .nam or .aecap (an .aecap may also carry its own IR). Returns false + error on failure.
    bool loadModelAny(const juce::File& f, int slot, juce::String& error);
    void unloadNAMModel(int slot = 0);
    void unloadIR(int slot = 0);
    juce::String getLoadedNAMName(int slot = 0) const { return amps[slotIdx(slot)].namName; }
    juce::String getLoadedIRName (int slot = 0) const { return amps[slotIdx(slot)].irName; }
    juce::String getNAMPath(int slot = 0) const { return amps[slotIdx(slot)].namPath; }
    juce::String getIRPath (int slot = 0) const { return amps[slotIdx(slot)].irPath; }
    // "an amp is loaded": a NAM/.aecap model OR a built-in amp
    bool isNAMLoaded(int slot = 0) const { return hasAmp(amps[slotIdx(slot)]); }
    bool isIRLoaded (int slot = 0) const { return amps[slotIdx(slot)].irLoaded; }
    bool isDualActive() const;          // Dual on AND amp 2 has a model or an IR

    // ── Built-in amps + cabinets (v1.1) and the Amari Library ────────────────
    // A model/IR is identified by a "ref": "builtin:amp/clean", "builtin:cab/4x12",
    // "library:Models/Some Amp.nam" (relative to the library folder, so presets
    // work on Windows and Mac alike) or an absolute file path.
    static constexpr int kNumBuiltinAmps = 3, kNumBuiltinCabs = 3;
    static juce::String builtinAmpName(int i);
    static juce::String builtinCabName(int i);
    static juce::String builtinAmpRef(int i);
    static juce::String builtinCabRef(int i);
    static int  builtinAmpFromRef(const juce::String& ref);   // -1 if not a built-in amp
    static int  builtinCabFromRef(const juce::String& ref);
    static bool isBuiltinRef(const juce::String& ref) { return ref.startsWith("builtin:"); }
    static juce::File libraryRoot();                          // .../Amari Labs/Arcane Eclipse
    static juce::String refForFile(const juce::File& f);      // library-relative when inside the library
    static juce::File resolveRef(const juce::String& ref);    // file for a non-built-in ref ({} if missing)
    bool loadBuiltinAmp(int v, int slot = 0);
    bool loadBuiltinCab(int c, int slot = 0);
    int  getBuiltinAmp(int slot = 0) const { return amps[slotIdx(slot)].builtin; }
    int  getBuiltinCab(int slot = 0) const { return amps[slotIdx(slot)].cab; }
    bool loadModelRef(const juce::String& ref, int slot, juce::String& error);
    bool loadIRRef(const juce::String& ref, int slot);
    void loadDefaultRig();                                    // Eclipse Clean + Eclipse 1x12 Open, amp 2 empty

    // ── Overdrive pedal capture (v1.1.1) ─────────────────────────────────────
    // A NAM pedal capture loaded here replaces the built-in drive circuit.
    // DRIVE = input level into the capture (+-12 dB), TONE = treble tilt
    // (+-6 dB, flat at noon), LEVEL = output (-24..+6 dB, 0 dB at the default).
    bool loadODModel(const juce::File& f, juce::String& error);   // .nam or .aecap
    void unloadODModel();
    bool isODModelLoaded() const { return odSlot.model != nullptr; }
    juce::String getODModelName() const { return odSlot.namName; }
    juce::String getODModelPath() const { return odSlot.namPath; }
    static juce::File aecapCacheDir();

    // Global bypass (v1.1.1): output = raw input, 25 ms crossfade. Not a saved
    // parameter on purpose, so patches never load "bypassed".
    void setGlobalBypass(bool b) { globalBypass.store(b); }
    bool isGlobalBypassed() const { return globalBypass.load(); }
    // Dual: running loudness of each amp (before its trim), for MATCH
    float getAmpLevel(int i) const { return ampMs[juce::jlimit(0,1,i)].load(); }

    // MIDI learn
    void midiLearnStart(const juce::String& paramID);
    void midiLearnClear(const juce::String& paramID);
    juce::String midiLearningParamID() const;
    int  ccForParam(const juce::String& paramID) const;

    // Tuner — the editor sets tunerActive when open; processor fills tunerFreq
    std::atomic<bool>  tunerActive { false };
    std::atomic<float> tunerFreq   { 0.f };
    std::atomic<float> inLevel { 0.f }, outLevel { 0.f };  // VU meters

    // Patch/bank navigation via MIDI (0=prevPreset,1=nextPreset,2=prevBank,3=nextBank)
    void actionLearnStart(int action);
    void actionLearnClear(int action);
    int  ccForAction(int action) const;
    int  actionLearningNow() const;
    int  takePendingAction();          // returns a pending action then clears it (-1 = none)

    // Tap tempo (v1.1.1): each call is one tap; sets the delay TIME from the
    // averaged tap interval x the chosen division. Safe from UI or audio thread.
    void tapTempo();
    void undoLastTap();                // a footswitch HOLD (on/off) shouldn't count as a tap
    void applyTapDivision();           // re-derive TIME from the last tapped beat
    std::atomic<double> lastTapMs { -1.0 };            // for the UI tempo LED
    std::atomic<float>  tapBeatMs { 0.f };             // last tapped beat (quarter note), 0 = none
    void cancelLearn();                // cancel any in-progress learn (knob/node/action)

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ── Parameter IDs ──────────────────────────────────────────────────────────
    static constexpr auto idInputGain   = "inputGain";
    static constexpr auto idOutputGain  = "outputGain";
    static constexpr auto idStereoMode  = "stereoMode";
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
    static constexpr auto idDelayTapMode  = "delayTapMode";   // v1.1.1 footswitch = tap tempo
    static constexpr auto idDelayTapDiv   = "delayTapDiv";    // 0 1/4, 1 dotted 1/8, 2 1/8, 3 1/8 triplet
    // Reverb
    static constexpr auto idReverbOn    = "reverbOn";
    static constexpr auto idReverbDecay = "reverbDecay";
    static constexpr auto idReverbSize  = "reverbSize";
    static constexpr auto idReverbMix   = "reverbMix";
    static constexpr auto idReverbHighCut = "reverbHighCut";
    static constexpr auto idReverbType  = "reverbType";
    static constexpr auto idReverbShimmer = "reverbShimmer";   // v1.1 octave-up layer
    // Dual Amp/IR (v1.1)
    static constexpr auto idDualOn  = "dualOn";
    static constexpr auto idDualMix = "dualMix";      // 0 = amp 1 only, 1 = amp 2 only
    static constexpr auto idAmp1Trim = "amp1Trim";    // dual: per-amp level trim (dB)
    static constexpr auto idAmp2Trim = "amp2Trim";
    // Stereo doubler (v1.1.1)
    static constexpr auto idDoubler      = "doubler";
    static constexpr auto idDoublerWidth = "doublerWidth";

private:
    // One amp = NAM model (+ its own 48 kHz resamplers) + cabinet IR.
    struct AmpSlot {
        std::unique_ptr<NeuralAudio::NeuralModel> model;
        juce::CatmullRomInterpolator rsIn, rsOut;
        std::vector<float> upIn, upOut;          // 48 kHz work buffers
        juce::dsp::Convolution conv;
        bool irLoaded = false;
        juce::String namName, irName, namPath, irPath;
        int builtin = -1;                         // built-in amp voicing (0..2), -1 = NAM / none
        int cab = -1;                             // built-in cabinet (0..2), -1 = file IR / none
        AmpSim sim;
        float simGain = -1.f;                     // last GAIN sent to the built-in amp
    };
    static bool hasAmp(const AmpSlot& a) { return a.model != nullptr || a.builtin >= 0; }
    void runAmp(AmpSlot& a, const float* in, float* out, int numSamples, float preGain);
    AmpSlot amps[kNumAmpSlots];
    AmpSlot odSlot;                                          // overdrive pedal capture (no IR)
    StereoDoubler doubler;
    juce::AudioBuffer<float> dryBuf;                         // raw input for global bypass
    std::atomic<bool> globalBypass { false };
    juce::SmoothedValue<float> bypassSm { 0.f }, trimSm[2];
    std::atomic<float> ampMs[2] { {0.f}, {0.f} };
    std::vector<float> odMono, odOut;
    float odTiltZ[2] = { 0.f, 0.f };
    void makeMono(const juce::AudioBuffer<float>& b, int numSamples, float* dst) const;
    static int slotIdx(int s) { return juce::jlimit(0, kNumAmpSlots - 1, s); }
    void runNAM(AmpSlot& a, const float* in, float* out, int numSamples);
    NeuralAudio::NeuralModelLoader namLoader;

    double currentSampleRate = 44100.0;
    std::vector<float> monoBuf, namOutBuf;
    juce::AudioBuffer<float> dualBuf[kNumAmpSlots];          // per-amp stereo scratch (dual mode)
    juce::SmoothedValue<float> dualMixSm { 0.5f };
    OpticalCompressor compressor;
    TubeScreamerDrive overdrive;
    ModulationFX      modulation;
    StandardDelay     delay;
    RoomReverb        reverb;

    juce::dsp::IIR::Filter<float> bassFilter[2], midFilter[2], trebleFilter[2], presenceFilter[2];
    void updateEQ();
    float  eqLastB=-999.f, eqLastM=-999.f, eqLastT=-999.f, eqLastP=-999.f;   // updateEQ change cache
    double eqLastSR=0.0;
    float gateEnvelope = 0.f;
    float dcX1[2] = {0.f,0.f}, dcY1[2] = {0.f,0.f};  // DC blocker state
    bool  prevReverbOn = false;                     // reset reverb tail on enable
    bool  prevDelayOn  = false;                     // reset delay buffer on enable (v1.0.2)
    double tapTimes[4] = { 0, 0, 0, 0 }; int tapCount = 0;   // tap-tempo history (ms)
    float  timeBeforeTap = -1.f;  float beatBeforeTap = 0.f;
    juce::SpinLock tapLock;

    // MIDI learn state
    std::vector<juce::String> learnParamIDs;
    std::vector<juce::RangedAudioParameter*> learnParamPtrs;
    std::atomic<int> ccMap[256];      // 0-127 = CC number, 128-255 = Note number
    std::atomic<int> learnTarget { -1 };
    std::vector<bool> learnIsToggle;        // parallel to learnParamIDs
    int prevCCVal[256] = { 0 };             // for footswitch rising-edge detection
    std::atomic<int> actionCC[256];         // MIDI (CC/Note) -> patch/bank action, or -1
    std::atomic<int> actionLearn   { -1 };
    std::atomic<int> actionPending { -1 };
    int indexOfParam(const juce::String& id) const;
    // Tuner — YIN pitch detection (v1.1) + octave-snap smoothing
    std::vector<float> tunerBuf;
    int tunerFill = 0;
    float detectPitch(const float* buf, int n, double sr);   // YIN (uses scratch below)
    float smoothTunerPitch(float raw);                       // octave-snap + median
    std::vector<double> tunerD, tunerDP;                     // preallocated YIN scratch
    float tunerStable = 0.f;
    float tunerHist[5] = {0,0,0,0,0};
    int   tunerHistCount = 0;
    int   tunerOctRun = 0;                                   // consecutive octave-jump frames

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArcaneEclipseProcessor)
};
