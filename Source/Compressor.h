#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

/*
    OpticalCompressor — an LA-2A-inspired optical compressor.
    Programme-dependent attack/release via a simple envelope follower
    feeding a VCA (implemented as a gain smoother). Ratio is fixed at
    the "compress" position (~4:1) but can be overridden.
*/
class OpticalCompressor
{
public:
    void prepare (double sr, int /*block*/)
    {
        sampleRate = sr;
        envelope   = 0.0f;
        gainSmooth = 1.0f;
    }

    void reset() { envelope = 0.0f; gainSmooth = 1.0f; }

    // threshold: dBFS  ratio: e.g. 4.0  attack/release: ms  makeupGain: dB
    void setParameters (float thresholdDb, float ratio,
                        float attackMs, float releaseMs, float makeupGainDb)
    {
        threshold  = juce::Decibels::decibelsToGain (thresholdDb);
        this->ratio    = ratio;
        attackCoeff    = std::exp (-1.0f / (float) (sampleRate * attackMs  * 0.001f));
        releaseCoeff   = std::exp (-1.0f / (float) (sampleRate * releaseMs * 0.001f));
        makeupGain     = juce::Decibels::decibelsToGain (makeupGainDb);
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        int numSamples  = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();

        for (int n = 0; n < numSamples; ++n)
        {
            // Peak detection — mono sum
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                peak = std::max (peak, std::fabs (buffer.getReadPointer (ch)[n]));

            // Envelope follower
            float coeff = peak > envelope ? attackCoeff : releaseCoeff;
            envelope = peak + coeff * (envelope - peak);

            // Gain computer
            float gain = 1.0f;
            if (envelope > threshold && envelope > 1e-6f)
            {
                float excess    = envelope / threshold;          // linear excess
                float targetGain = threshold * std::pow (excess, 1.0f / ratio - 1.0f);
                gain = targetGain / envelope;
            }

            // Smooth the gain
            gainSmooth = gain + 0.9995f * (gainSmooth - gain);

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer (ch)[n] *= gainSmooth * makeupGain;
        }
    }

private:
    double sampleRate = 44100.0;
    float  threshold    = 1.0f;
    float  ratio        = 4.0f;
    float  attackCoeff  = 0.999f;
    float  releaseCoeff = 0.9999f;
    float  makeupGain   = 1.0f;
    float  envelope     = 0.0f;
    float  gainSmooth   = 1.0f;
};
