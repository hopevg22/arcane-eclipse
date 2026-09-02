/*
    StandardDelay.h

    A clean, transparent digital delay — the "Standard" voice on a Boss
    DD-8 style pedal. Dual-mono delay lines (same repeats on both
    channels), a damping filter in the feedback path for the TONE
    control, and smoothed delay-time changes so turning the TIME knob
    doesn't click or zipper.

    Signal path per channel:
        input -> [+ feedback] -> delay line -> lowpass (tone) -> feedback tap
                                             \-> wet output (added on top of dry)
*/

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>

class StandardDelay
{
public:
    static constexpr float kMinTimeMs = 20.0f;
    static constexpr float kMaxTimeMs = 2000.0f;

    void prepare (double sampleRateIn, int maxBlockSize)
    {
        sampleRate = sampleRateIn;

        for (auto& line : delayLines)
        {
            line.prepare ({ sampleRate, (juce::uint32) maxBlockSize, 1 });
            line.setMaximumDelayInSamples ((int) (sampleRate * (kMaxTimeMs / 1000.0) + 8));
        }

        smoothedDelaySamples.reset (sampleRate, 0.05); // 50ms glide on TIME changes
        smoothedDelaySamples.setCurrentAndTargetValue ((float) (sampleRate * 0.3));

        for (auto& s : damperState) s = 0.0f;

        reset();
    }

    void reset()
    {
        for (auto& line : delayLines) line.reset();
        for (auto& s : damperState) s = 0.0f;
    }

    // timeMs: delay time in ms. feedback01/tone01/level01: 0..1. bypassed: true bypass.
    void setParameters (float timeMs, float feedback01, float tone01, float level01, bool bypassedIn)
    {
        timeMs = juce::jlimit (kMinTimeMs, kMaxTimeMs, timeMs);
        smoothedDelaySamples.setTargetValue ((float) (timeMs * 0.001 * sampleRate));

        feedback = juce::jlimit (0.0f, 0.97f, feedback01);
        level    = juce::jlimit (0.0f, 1.0f, level01);
        dampCoeff = juce::jmap (juce::jlimit (0.0f, 1.0f, tone01), 0.0f, 1.0f, 0.75f, 0.05f);
        bypassed = bypassedIn;
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        int numChannels = juce::jmin (buffer.getNumChannels(), 2);
        int numSamples  = buffer.getNumSamples();

        for (int n = 0; n < numSamples; ++n)
        {
            float delaySamples = smoothedDelaySamples.getNextValue();

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* data = buffer.getWritePointer (ch);
                float dry = data[n];

                delayLines[(size_t) ch].setDelay (delaySamples);
                float delayed = delayLines[(size_t) ch].popSample (0);

                // damping (one-pole lowpass) in the feedback path — this is
                // what gives the TONE control its darker/brighter repeats
                damperState[(size_t) ch] = delayed * (1.0f - dampCoeff) + damperState[(size_t) ch] * dampCoeff;

                float toWrite = dry + damperState[(size_t) ch] * feedback;
                delayLines[(size_t) ch].pushSample (0, toWrite);

                float wet = bypassed ? 0.0f : damperState[(size_t) ch] * level;
                data[n] = dry + wet;
            }
        }
    }

private:
    double sampleRate = 44100.0;

    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>, 2> delayLines
        { juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> { 1 << 18 },
          juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> { 1 << 18 } };

    std::array<float, 2> damperState { 0.0f, 0.0f };
    juce::SmoothedValue<float> smoothedDelaySamples;

    float feedback = 0.35f, level = 0.5f, dampCoeff = 0.3f;
    bool bypassed = true;
};
