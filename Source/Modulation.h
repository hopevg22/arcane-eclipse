#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>

/*
    ModulationFX — selectable modulation engine (v1.1).

    One engine, three modes selected by `type`:
      0 = CHORUS  — the tuned 3-voice studio chorus (unchanged from v1.0; LFOs
                    spread 120 deg, L/R offset for width). This is the user's
                    sweet-spot voice, kept byte-for-byte.
      1 = FLANGER — a short modulated delay (~1..4.5 ms) with feedback for the
                    classic jet-sweep comb.
      2 = PHASER  — 6 cascaded modulated first-order all-pass stages with
                    feedback, log sweep across the guitar range (~200 Hz..3.5 kHz).

    Shared knobs: RATE, DEPTH, MIX. Feedback for flanger/phaser is set
    internally (no extra knob) to musical fixed values.

    v1.1.1 phaser fix: the all-pass stages had a flipped sign, which mirrored
    the sweep to ~22-24 kHz (inaudible). Now a standard first-order all-pass;
    phaser/flanger also get their own exponential RATE range (~0.15..6 Hz)
    instead of the chorus' slow remap, and the phaser MIX reaches full-depth
    notches (equal dry/all-pass) at 100%.
*/
class ModulationFX
{
public:
    void prepare (double sampleRate, int /*block*/)
    {
        sr = sampleRate;
        size_t len = (size_t) (0.060 * sr) + 8;   // up to 60 ms
        for (int ch = 0; ch < 2; ++ch) { buf[ch].assign (len, 0.f); widx[ch] = 0; }
        phase = 0.0;
        reset();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            std::fill (buf[ch].begin(), buf[ch].end(), 0.f);
            fbState[ch] = 0.f;
            for (int k = 0; k < kStages; ++k) apState[ch][k] = 0.f;
        }
        phase = 0.0;
    }

    // (rate, depth, mix, type) — type: 0 chorus, 1 flanger, 2 phaser
    void setParameters (float rate, float depth, float mix, int type)
    {
        // Rate remap kept from v1.0 so the chorus sweet spot (old dial 0.2)
        // sits at the new default dial 1.5; shared across all three modes.
        lfoRate = juce::jlimit (0.01f, 8.0f, rate * 0.133f);
        // Flanger/phaser: exponential map of the RATE knob (0.1..4) -> ~0.15..6 Hz
        float norm = juce::jlimit (0.f, 1.f, (rate - 0.1f) / 3.9f);
        lfoRateFx = 0.15f * std::pow (40.0f, norm);
        depth01 = juce::jlimit (0.f, 1.f, depth);
        depthMs = 1.0f + depth01 * 9.0f;            // chorus sweep +/- 1..10 ms
        wetMix  = juce::jlimit (0.f, 1.f, mix);
        fxType  = juce::jlimit (0, 2, type);
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        switch (fxType)
        {
            case 1:  processFlanger (buffer); break;
            case 2:  processPhaser  (buffer); break;
            default: processChorus  (buffer); break;
        }
    }

private:
    static constexpr int kVoices = 3;
    static constexpr int kStages = 6;

    // ---- CHORUS (unchanged v1.0 voice) ----
    void processChorus (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
        const double inc = lfoRate * juce::MathConstants<double>::twoPi / sr;
        const double baseMs = 18.0;
        const double voiceOff[kVoices] = { 0.0,
                                           juce::MathConstants<double>::twoPi / 3.0,
                                           2.0 * juce::MathConstants<double>::twoPi / 3.0 };
        for (int n = 0; n < numSamples; ++n)
        {
            phase += inc;
            if (phase > juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* data = buffer.getWritePointer (ch);
                float  dry  = data[n];
                buf[ch][(size_t) widx[ch]] = dry;

                double chOff = (ch == 1) ? juce::MathConstants<double>::halfPi : 0.0;
                float wet = 0.f;
                for (int v = 0; v < kVoices; ++v)
                {
                    double lfo = std::sin (phase + voiceOff[v] + chOff);
                    double delMs = baseMs + depthMs * lfo;
                    wet += readInterp (ch, (float) (delMs * 0.001 * sr));
                }
                wet *= (1.0f / std::sqrt ((float) kVoices));
                data[n] = dry + wetMix * (wet - dry);
            }
            for (int ch = 0; ch < numChannels; ++ch)
                if (++widx[ch] >= (int) buf[ch].size()) widx[ch] = 0;
        }
    }

    // ---- FLANGER (short modulated delay + feedback) ----
    void processFlanger (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
        const float baseMs = 1.0f;
        const float excMs  = 3.5f * depth01;        // up to +3.5 ms
        const double incFx = lfoRateFx * juce::MathConstants<double>::twoPi / sr;
        const float fb     = 0.6f;
        const float makeup = 1.0f / std::sqrt ((1.0f - wetMix) * (1.0f - wetMix) + wetMix * wetMix);  // keep level steady

        for (int n = 0; n < numSamples; ++n)
        {
            phase += incFx;
            if (phase > juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* data = buffer.getWritePointer (ch);
                float  dry  = data[n];
                double chOff = (ch == 1) ? juce::MathConstants<double>::halfPi : 0.0;
                float  lfo01 = 0.5f + 0.5f * (float) std::sin (phase + chOff);   // 0..1
                float  delMs = baseMs + excMs * lfo01;
                float  dl    = readInterp (ch, delMs * 0.001f * (float) sr);

                buf[ch][(size_t) widx[ch]] = dry + fb * dl;   // feedback into the line
                data[n] = makeup * (dry + wetMix * (dl - dry));
            }
            for (int ch = 0; ch < numChannels; ++ch)
                if (++widx[ch] >= (int) buf[ch].size()) widx[ch] = 0;
        }
    }

    // ---- PHASER (cascaded modulated all-pass + feedback) ----
    void processPhaser (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
        const double inc = lfoRateFx * juce::MathConstants<double>::twoPi / sr;
        const float fb = 0.5f;
        const float lo = std::log (200.0f), hi = std::log (3500.0f);
        const float ctr = (lo + hi) * 0.5f, halfR = (hi - lo) * 0.5f;
        // MIX: 4th-root curve so the default (0.7) already gives deep notches; 100% = equal mix.
        // Make-up gain keeps the perceived level steady when the phaser is engaged.
        const float wetW = 0.5f * std::pow (wetMix, 0.25f), dryW = 1.0f - wetW;
        const float makeup = 1.0f + 0.6f * (2.0f * wetW);

        for (int n = 0; n < numSamples; ++n)
        {
            phase += inc;
            if (phase > juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* data = buffer.getWritePointer (ch);
                float  dry  = data[n];
                double chOff = (ch == 1) ? juce::MathConstants<double>::halfPi : 0.0;
                float  sweep = std::exp (ctr + halfR * (0.25f + 0.75f * depth01) * (float) std::sin (phase + chOff));
                float  tw = std::tan (juce::MathConstants<float>::pi * sweep / (float) sr);
                float  a  = (tw - 1.0f) / (tw + 1.0f);              // first-order all-pass coefficient

                float s = dry + fb * fbState[ch];
                for (int k = 0; k < kStages; ++k)
                {
                    float y = a * s + apState[ch][k];               // y = a*x + z
                    apState[ch][k] = s - a * y;                     // z = x - a*y
                    s = y;
                }
                fbState[ch] = juce::jlimit (-4.0f, 4.0f, s);
                data[n] = makeup * (dryW * dry + wetW * s);
            }
        }
    }

    float readInterp (int ch, float delaySamps)
    {
        auto& b = buf[ch];
        int sz = (int) b.size();
        float readPos = (float) widx[ch] - delaySamps;
        while (readPos < 0.f) readPos += sz;
        int i0 = (int) readPos;
        float frac = readPos - i0;
        int i1 = (i0 + 1) % sz;
        return b[(size_t) i0] + frac * (b[(size_t) i1] - b[(size_t) i0]);
    }

    double sr = 48000.0, phase = 0.0;
    float  lfoRate = 1.0f, lfoRateFx = 0.6f, depthMs = 5.0f, depth01 = 0.5f, wetMix = 0.5f;
    int    fxType = 0;
    std::vector<float> buf[2];
    int    widx[2] = { 0, 0 };
    float  fbState[2] = { 0.f, 0.f };
    float  apState[2][kStages] = {{0.f}};
};
