#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

/*
    TubeScreamerDrive — a simplified Tube Screamer-inspired overdrive.
    Signal path:
        input → pre-gain → soft clipper (tanh) → tone filter → output level
    The tone control is a one-pole shelving filter that blends between
    a darker and brighter character, matching the real pedal's tone stack.
*/
class TubeScreamerDrive
{
public:
    void prepare (double sr, int /*block*/)
    {
        sampleRate = sr;
        toneState[0] = toneState[1] = 0.0f;
    }

    void reset() { toneState[0] = toneState[1] = 0.0f; }

    // drive: 0..1  tone: 0..1 (dark..bright)  level: 0..1 output level
    void setParameters (float drive, float tone, float level)
    {
        // Drive maps to pre-gain: 1x .. 50x (approx TS range)
        preGain = 1.0f + drive * 49.0f;

        // Tone is a one-pole shelving crossfade: 0=dark, 1=bright
        // We model it as blending a lowpassed signal with the direct signal
        float freq = 500.0f + tone * 3000.0f;      // 500Hz..3500Hz cutoff
        float rc = 1.0f / (2.0f * juce::MathConstants<float>::pi * (float) sampleRate * freq);
        toneCoeff = rc / (rc + 1.0f / (float) sampleRate);

        this->level = level * 0.5f; // scale output to unity-ish range
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        int numSamples  = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();

        for (int ch = 0; ch < juce::jmin (numChannels, 2); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int n = 0; n < numSamples; ++n)
            {
                // Pre-gain + soft clip
                float x = data[n] * preGain;
                x = std::tanh (x);             // smooth, asymptote at ±1

                // One-pole tone filter: blend bright (direct) and dark (LP)
                toneState[ch] += toneCoeff * (x - toneState[ch]);
                float bright = x - toneState[ch];      // high shelf contribution
                float warm   = toneState[ch];           // low shelf contribution
                float toneMix = juce::jmap (toneCoeff, 0.0f, 1.0f, 0.0f, 1.0f);
                x = warm * (1.0f - toneMix) + bright * toneMix;

                data[n] = x * level;
            }
        }
    }

private:
    double sampleRate = 44100.0;
    float  preGain    = 1.0f;
    float  toneCoeff  = 0.5f;
    float  level      = 0.5f;
    float  toneState[2] = { 0.0f, 0.0f };
};
