/*
    StandardDelay.h

    Selectable delay engine (v1.1). Dual-mono delay lines, a tone damping
    filter, smoothed time changes, and a DC/sub blocker in the feedback path
    (the v1.0.2 fix for the periodic low "kick").

    Modes selected by `type` (setType):
      0 = DIGITAL — clean, transparent repeats (v1.0 voice).
      1 = ANALOG  — feedback low-passed (~3 kHz) + gentle saturation; the
                    repeats get darker and rounder as they decay (BBD-style).
      2 = TAPE    — darker still (~3.5 kHz) + more saturation + wow/flutter
                    modulation of the read head for subtle pitch movement.
      3 = ECHO    — a brighter vintage-digital voice (~4.8 kHz, light drive).

    Shared knobs: TIME (ms), FEEDBACK, TONE, LEVEL. Tap-tempo just sets TIME.

    Signal path per channel:
        input -> [+ conditioned feedback] -> delay line -> lowpass (tone)
                 -> highpass (DC/sub block) -> mode LP -> mode saturation
                 -> feedback tap
                 \-> wet output (added on top of dry)
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
        smoothedDelaySamples.reset (sampleRate, 0.05);
        smoothedDelaySamples.setCurrentAndTargetValue ((float) (sampleRate * 0.3));
        hpCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * 85.0f / (float) sampleRate);
        updateModeCoeffs();
        reset();
    }

    void reset()
    {
        for (auto& line : delayLines) line.reset();
        for (auto& s : damperState) s = 0.0f;
        for (auto& s : hpState)     s = 0.0f;
        for (auto& s : modeLpState) s = 0.0f;
        wowPhase = 0.0f;
    }

    // type: 0 digital, 1 analog, 2 tape, 3 echo
    void setType (int type)
    {
        fxType = juce::jlimit (0, 3, type);
        updateModeCoeffs();
    }

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
        const float wowInc   = wowRate * juce::MathConstants<float>::twoPi / (float) sampleRate;
        const float wowSamps = wowMs * 0.001f * (float) sampleRate;

        for (int n = 0; n < numSamples; ++n)
        {
            float base = smoothedDelaySamples.getNextValue();
            if (wowDepth > 0.f)
            {
                wowPhase += wowInc;
                if (wowPhase > juce::MathConstants<float>::twoPi) wowPhase -= juce::MathConstants<float>::twoPi;
            }
            float delaySamples = base + (wowDepth > 0.f ? wowSamps * std::sin (wowPhase) : 0.f);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* data = buffer.getWritePointer (ch);
                float dry = data[n];

                delayLines[(size_t) ch].setDelay (juce::jmax (1.0f, delaySamples));
                float delayed = delayLines[(size_t) ch].popSample (0);

                // Tone low-pass
                damperState[(size_t) ch] = delayed * (1.0f - dampCoeff) + damperState[(size_t) ch] * dampCoeff;
                float toned = flush (damperState[(size_t) ch]);

                // DC / sub blocker (v1.0.2)
                hpState[(size_t) ch] = toned * (1.0f - hpCoeff) + hpState[(size_t) ch] * hpCoeff;
                float cleaned = flush (toned - hpState[(size_t) ch]);

                // Mode conditioning of the FEEDBACK signal (analog/tape/echo)
                float fbSig = cleaned;
                if (modeLp > 0.f)
                {
                    modeLpState[(size_t) ch] = fbSig * (1.0f - modeLp) + modeLpState[(size_t) ch] * modeLp;
                    fbSig = flush (modeLpState[(size_t) ch]);
                }
                if (modeSat > 0.f)
                    fbSig = std::tanh (modeSat * fbSig) / satNorm;

                float toWrite = dry + fbSig * feedback;
                delayLines[(size_t) ch].pushSample (0, toWrite);

                float wet = bypassed ? 0.0f : cleaned * level;
                data[n] = dry + wet;
            }
        }
    }

private:
    void updateModeCoeffs()
    {
        auto lp = [this](float hz){ return std::exp (-2.0f * juce::MathConstants<float>::pi * hz / (float) sampleRate); };
        switch (fxType)
        {
            case 1:  modeLp = lp (3000.0f); modeSat = 1.4f; wowDepth = 0.f; break;   // analog
            case 2:  modeLp = lp (3500.0f); modeSat = 1.8f; wowDepth = 1.f; break;   // tape
            case 3:  modeLp = lp (4800.0f); modeSat = 1.2f; wowDepth = 0.f; break;   // echo
            default: modeLp = 0.0f;         modeSat = 0.0f; wowDepth = 0.f; break;   // digital
        }
        satNorm = (modeSat > 0.f) ? std::tanh (modeSat) : 1.0f;
    }

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
    std::array<float, 2> modeLpState { 0.0f, 0.0f };
    juce::SmoothedValue<float> smoothedDelaySamples;

    float feedback = 0.35f, level = 0.5f, dampCoeff = 0.3f;
    float hpCoeff = 0.99f;
    int   fxType = 0;
    float modeLp = 0.0f, modeSat = 0.0f, satNorm = 1.0f;
    float wowDepth = 0.0f, wowPhase = 0.0f;
    const float wowRate = 0.6f, wowMs = 0.25f;   // tape wow/flutter
    bool  bypassed = true;
};
