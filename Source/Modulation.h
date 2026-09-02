#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

// Simple modulation effect: Chorus / Flanger / Phaser / Tremolo
class ModulationFX
{
public:
    void prepare(double sampleRate, int samplesPerBlock)
    {
        sr = sampleRate;
        lfoPhase = 0.0;
        juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, 2 };
        for (auto& d : delayLine) { d.prepare(spec); d.setMaximumDelayInSamples(4096); }
    }

    void setParameters(float rate, float depth, float mix, int typeIdx)
    {
        lfoRate  = (double)rate;
        lfoDepth = (double)depth;
        wetMix   = mix;
        type     = typeIdx; // 0=Chorus 1=Flanger 2=Tremolo
    }

    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        int numSamples  = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();
        double phaseInc = lfoRate * juce::MathConstants<double>::twoPi / sr;

        for (int n = 0; n < numSamples; ++n)
        {
            double lfo = std::sin(lfoPhase);
            lfoPhase  += phaseInc;
            if (lfoPhase > juce::MathConstants<double>::twoPi)
                lfoPhase -= juce::MathConstants<double>::twoPi;

            for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
            {
                float* data = buffer.getWritePointer(ch);
                float  dry  = data[n];

                float wet = 0.f;
                if (type == 2) // Tremolo — amplitude modulate
                {
                    wet = dry * (float)(1.0 - lfoDepth * 0.5 * (1.0 + lfo));
                }
                else // Chorus / Flanger — delay modulate
                {
                    float baseDelay = (type == 0) ? 20.f : 3.f; // ms
                    float modDepth  = (type == 0) ? 15.f : 2.f;
                    float delaySamples = (float)sr * 0.001f *
                        (baseDelay + (float)(lfoDepth * modDepth * lfo));
                    delaySamples = juce::jlimit(0.f, 4094.f, delaySamples);

                    delayLine[ch].pushSample(ch, dry);
                    wet = delayLine[ch].popSample(ch, delaySamples, true);
                }

                data[n] = dry + wetMix * (wet - dry);
            }
        }
    }

private:
    double sr = 44100.0;
    double lfoPhase = 0.0;
    double lfoRate  = 0.5;
    double lfoDepth = 0.5;
    float  wetMix   = 0.5f;
    int    type     = 0;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine[2];
};
