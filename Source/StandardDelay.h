/*
    StandardDelay.h

    A clean, transparent digital delay — the "Standard" voice on a Boss
    DD-8 style pedal. Dual-mono delay lines (same repeats on both
    channels), a damping filter in the feedback path for the TONE
    control, and smoothed delay-time changes so turning the TIME knob
    doesn't click or zipper.

    Signal path per channel:
        input -> [+ feedback] -> delay line -> lowpass (tone)
                                             -> highpass (DC / sub block)
                                             -> feedback tap
                                             \-> wet output (added on top of dry)

    v1.0.2 fix — periodic low "kick" on the repeats:
        The feedback loop previously had only a low-pass (tone) filter,
        which PASSES DC and sub-bass. DC offset and subsonic energy from
        the amp + cab were recirculated with every repeat and rode on the
        echoes as a low-frequency thump at the delay interval.
        Fix: a DC blocker + gentle ~85 Hz high-pass now sits in the
        feedback path (and on the wet output), so DC/sub can no longer
        recirculate. A denormal flush is kept as light insurance.
        (Buffer flush on re-enable is handled in PluginProcessor, mirroring
        the reverb, so a stale tail doesn't burst back when the delay is
        toggled on.)
*/

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

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

        // One-pole high-pass coefficient for the feedback DC/sub blocker (~85 Hz).
        hpCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * 85.0f / (float) sampleRate);

        reset();
    }

    void reset()
    {
        for (auto& line : delayLines) line.reset();
        for (auto& s : damperState) s = 0.0f;
        for (auto& s : hpState)     s = 0.0f;
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

                // 1) Tone: one-pole low-pass (darker/brighter repeats).
                damperState[(size_t) ch] = delayed * (1.0f - dampCoeff)
                                         + damperState[(size_t) ch] * dampCoeff;
                float toned = flush (damperState[(size_t) ch]);

                // 2) DC / sub blocker: one-pole high-pass (~85 Hz) in the loop.
                //    HP = signal - lowpass(signal). Stops DC and subsonic
                //    energy from recirculating and building into a thump.
                hpState[(size_t) ch] = toned * (1.0f - hpCoeff)
                                     + hpState[(size_t) ch] * hpCoeff;
                float cleaned = flush (toned - hpState[(size_t) ch]);

                // Recirculate and output the cleaned (thump-free) signal.
                float toWrite = dry + cleaned * feedback;
                delayLines[(size_t) ch].pushSample (0, toWrite);

                float wet = bypassed ? 0.0f : cleaned * level;
                data[n] = dry + wet;
            }
        }
    }

private:
    // Flush denormals / non-finite values to zero (belt-and-suspenders; the
    // processor already wraps processBlock in juce::ScopedNoDenormals).
    static inline float flush (float x) noexcept
    {
        if (! std::isfinite (x) || std::fabs (x) < 1.0e-15f) return 0.0f;
        return x;
    }

    double sampleRate = 44100.0;

    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>, 2> delayLines
        { juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> { 1 << 18 },
          juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> { 1 << 18 } };

    std::array<float, 2> damperState { 0.0f, 0.0f };
    std::array<float, 2> hpState     { 0.0f, 0.0f };
    juce::SmoothedValue<float> smoothedDelaySamples;

    float feedback = 0.35f, level = 0.5f, dampCoeff = 0.3f;
    float hpCoeff = 0.99f;
    bool bypassed = true;
};
