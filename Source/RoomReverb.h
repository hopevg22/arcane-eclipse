/*
    RoomReverb.h

    A Feedback Delay Network (FDN) based "Room" reverb algorithm,
    in the spirit of the first algorithm on a Strymon Big Sky:
    short, natural-sounding, with clear early reflections and a
    smooth diffuse tail.

    Signal path:
        input -> pre-delay -> input diffusion (4x series allpass)
              -> 8-line FDN (Householder feedback, per-line damping
                 + subtle modulation) -> stereo output taps
        wet/dry mixed at the end.
*/

#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

class RoomReverb
{
public:
    static constexpr int kNumLines = 8;

    void prepare (double sampleRateIn, int /*maxBlockSize*/)
    {
        sampleRate = sampleRateIn;

        preDelay.prepare ({ sampleRate, (juce::uint32) 512, 2 });
        preDelay.setMaximumDelayInSamples ((int) (sampleRate * 0.5)); // up to 500ms

        for (auto& ap : inputDiffusers)
        {
            ap.resize ((int) (sampleRate * 0.02)); // up to 20ms allpass
            ap.clear();
        }

        // Base delay lengths (samples @44.1k), mutually prime-ish so the
        // network doesn't build up periodic resonances.
        static const double baseLengthsMs[kNumLines] =
        { 29.7, 37.1, 41.3, 47.9, 53.3, 59.7, 61.1, 67.7 };

        for (int i = 0; i < kNumLines; ++i)
        {
            baseLengthSamples[i] = baseLengthsMs[i] * 0.001 * sampleRate;
            int maxLen = (int) (baseLengthSamples[i] * 2.0) + 16;
            delayLines[i].assign ((size_t) maxLen, 0.0f);
            writePos[i] = 0;
            damperState[i] = 0.0f;
            lfoPhase[i] = (float) i / (float) kNumLines; // spread phases
        }

        reset();
    }

    void reset()
    {
        preDelay.reset();
        for (auto& ap : inputDiffusers) ap.clear();
        for (int i = 0; i < kNumLines; ++i)
        {
            std::fill (delayLines[i].begin(), delayLines[i].end(), 0.0f);
            damperState[i] = 0.0f;
        }
    }

    // Parameters, all 0..1 unless noted
    void setParameters (float decay01, float preDelayMs, float tone01,
                         float size01, float diffusion01, float mod01, float mix01)
    {
        decay      = decay01;
        preDelayMsParam = preDelayMs;
        tone       = tone01;
        size       = 0.5f + size01;      // 0.5x .. 1.5x room size
        diffusion  = diffusion01;
        modDepth   = mod01;
        mix        = mix01;
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        auto* left  = buffer.getWritePointer (0);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : left;
        int numSamples = buffer.getNumSamples();

        // Derived coefficients
        float rt60 = 0.15f + decay * 3.85f;             // 0.15s .. 4.0s
        float dampCoeff = juce::jmap (tone, 0.0f, 1.0f, 0.85f, 0.15f); // dark->bright
        float apGain = juce::jmap (diffusion, 0.0f, 1.0f, 0.2f, 0.7f);
        float modDepthSamples = modDepth * 6.0f;         // up to 6 samples of wobble
        float modRateHz = 0.3f;

        preDelay.setDelay ((float) (preDelayMsParam * 0.001 * sampleRate));

        for (int n = 0; n < numSamples; ++n)
        {
            float dryL = left[n];
            float dryR = right[n];
            float monoIn = 0.5f * (dryL + dryR);

            // pre-delay
            preDelay.pushSample (0, monoIn);
            float delayed = preDelay.popSample (0);

            // input diffusion: series allpass chain
            float diffused = delayed;
            for (auto& ap : inputDiffusers)
                diffused = ap.process (diffused, apGain);

            // Read each FDN line (with modulation), apply damping, sum via
            // Householder matrix, feed diffused input back in, write.
            std::array<float, kNumLines> readVals {};
            for (int i = 0; i < kNumLines; ++i)
            {
                float len = (float) (baseLengthSamples[i] * size);
                lfoPhase[i] += modRateHz / (float) sampleRate;
                if (lfoPhase[i] > 1.0f) lfoPhase[i] -= 1.0f;
                float wobble = std::sin (2.0f * juce::MathConstants<float>::pi * lfoPhase[i]) * modDepthSamples;

                float readPos = (float) writePos[i] - (len + wobble);
                readVals[i] = readInterpolated (i, readPos);
            }

            // Householder feedback mixing: y = x - (2/N) * sum(x) * ones
            float sum = 0.0f;
            for (int i = 0; i < kNumLines; ++i) sum += readVals[i];
            float factor = (2.0f / (float) kNumLines) * sum;

            float lineLenSecAvg = 0.0f;
            float wetL = 0.0f, wetR = 0.0f;
            for (int i = 0; i < kNumLines; ++i)
            {
                float mixed = readVals[i] - factor;

                // per-line damping (one-pole lowpass) in the feedback path
                damperState[i] = mixed * (1.0f - dampCoeff) + damperState[i] * dampCoeff;

                // per-line decay gain from desired RT60
                float lenSec = (float) (baseLengthSamples[i] * size / sampleRate);
                float g = std::pow (10.0f, -3.0f * lenSec / rt60);
                g = juce::jlimit (0.0f, 0.985f, g);

                float toWrite = diffused + damperState[i] * g;
                delayLines[i][(size_t) writePos[i]] = toWrite;

                // alternate lines feed L/R for a stereo spread
                if ((i & 1) == 0) wetL += readVals[i];
                else              wetR += readVals[i];

                writePos[i] = (writePos[i] + 1) % (int) delayLines[i].size();
                juce::ignoreUnused (lineLenSecAvg);
            }

            wetL *= 0.5f;
            wetR *= 0.5f;

            left[n]  = dryL * (1.0f - mix) + wetL * mix;
            right[n] = dryR * (1.0f - mix) + wetR * mix;
        }
    }

private:
    // Simple allpass diffuser with its own small delay buffer
    struct Allpass
    {
        std::vector<float> buf;
        int pos = 0;

        void resize (int len) { buf.assign ((size_t) juce::jmax (4, len), 0.0f); pos = 0; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }

        float process (float x, float g)
        {
            float bufOut = buf[(size_t) pos];
            float y = -g * x + bufOut;
            buf[(size_t) pos] = x + g * bufOut;
            pos = (pos + 1) % (int) buf.size();
            return y;
        }
    };

    float readInterpolated (int line, float readPos)
    {
        int size = (int) delayLines[line].size();
        while (readPos < 0) readPos += size;
        int i0 = (int) readPos;
        int i1 = (i0 + 1) % size;
        float frac = readPos - (float) i0;
        return delayLines[line][(size_t) i0] * (1.0f - frac) + delayLines[line][(size_t) i1] * frac;
    }

    double sampleRate = 44100.0;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> preDelay { 1 << 18 };
    std::array<Allpass, 4> inputDiffusers;

    std::array<std::vector<float>, kNumLines> delayLines;
    std::array<double, kNumLines> baseLengthSamples {};
    std::array<int, kNumLines> writePos {};
    std::array<float, kNumLines> damperState {};
    std::array<float, kNumLines> lfoPhase {};

    // parameters (0..1 normalized unless noted)
    float decay = 0.5f, preDelayMsParam = 20.0f, tone = 0.5f;
    float size = 1.0f, diffusion = 0.5f, modDepth = 0.2f, mix = 0.35f;
};
