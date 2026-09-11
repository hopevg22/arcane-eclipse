#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

/*
    TubeScreamerDrive — a gentle Tube Screamer-style overdrive / booster.

    Signal path:
        input → moderate pre-gain → soft clip (tanh) → tone tilt → output level

    IMPORTANT: this is a BOOSTER into the amp, not a fuzz. The pre-gain is kept
    modest (1x..9x) so it pushes the NAM amp harder while preserving pick
    dynamics and the amp's character — rather than square-waving the signal
    and erasing the amp tone.
*/
class TubeScreamerDrive
{
public:
    void prepare (double sr, int /*block*/)
    {
        sampleRate = sr;
        lpState[0] = lpState[1] = 0.0f;
    }

    void reset() { lpState[0] = lpState[1] = 0.0f; }

    // drive: 0..1   tone: 0..1 (dark..bright)   level: 0..1 output
    void setParameters (float drive, float tone, float level)
    {
        // Gentle booster range — NOT a fuzz. 1x .. 9x into a soft clipper.
        preGain = 1.0f + juce::jlimit (0.0f, 1.0f, drive) * 8.0f;

        // Tone: one-pole lowpass cutoff 700Hz(dark) .. 5000Hz(bright)
        float freq = 700.0f + juce::jlimit (0.0f, 1.0f, tone) * 4300.0f;
        lpCoeff  = std::exp (-2.0f * juce::MathConstants<float>::pi * freq / (float) sampleRate);
        lpCoeff  = juce::jlimit (0.0f, 0.999f, lpCoeff);
        toneParam = juce::jlimit (0.0f, 1.0f, tone);

        // Output ~unity at level 0.7 so it boosts, not buries or silences
        outLevel = 0.4f + juce::jlimit (0.0f, 1.0f, level) * 0.9f; // 0.4 .. 1.3
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
                // Soft clip — small signals boost linearly, loud ones round off
                float x = std::tanh (data[n] * preGain);

                // Tone tilt: blend a lowpassed (dark) copy with the direct (bright) one
                lpState[ch] += (1.0f - lpCoeff) * (x - lpState[ch]);
                float shaped = lpState[ch] + toneParam * (x - lpState[ch]);

                data[n] = shaped * outLevel;
            }
        }
    }

private:
    double sampleRate = 48000.0;
    float  preGain    = 1.0f;
    float  lpCoeff    = 0.5f;
    float  toneParam  = 0.5f;
    float  outLevel   = 1.0f;
    float  lpState[2] = { 0.0f, 0.0f };
};
