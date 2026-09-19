#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>

/*
    RoomReverb — a lush, modulated feedback-delay-network (FDN) reverb.

    Aimed at big, smooth, evolving ambiences in the spirit of Valhalla
    Supermassive. The smoothness (no metallic ring) comes from:
      - a chain of input all-pass diffusers that smear transients into a cloud,
      - per-line LFO modulation of the delay read positions,
      - an energy-preserving Householder feedback matrix,
      - damping in the feedback path.
    DECAY sets the tail length (up to very long), SIZE scales the space,
    MIX is a constant-sum wet/dry (so it never boosts the overall level).
*/
class RoomReverb
{
public:
    void prepare (double sr, int /*maxBlock*/)
    {
        sampleRate = (sr > 0.0 ? sr : 48000.0);

        // Input diffusers (short all-passes) — lengths in ms, scaled to SR
        const double apMs[kNumAP] = { 13.6, 21.3, 31.1, 43.7 };
        for (int i = 0; i < kNumAP; ++i) {
            ap[i].assign ((size_t) (apMs[i] * 0.001 * sampleRate) + 4, 0.f);
            apIdx[i] = 0;
        }

        // FDN delay lines — base lengths (ms), mutually incommensurate
        for (int i = 0; i < kNumLines; ++i) {
            lineBaseMs[i] = kBaseMs[i];
            size_t maxLen = (size_t) (kBaseMs[i] * 3.0 * 0.001 * sampleRate) + 64; // room for SIZE + mod
            line[i].assign (maxLen, 0.f);
            writeIdx[i] = 0;
            damp[i] = 0.f;
            lfoPhase[i] = (float) i / (float) kNumLines; // spread phases
        }
        reset();
    }

    void reset()
    {
        for (int i = 0; i < kNumAP; ++i)   std::fill (ap[i].begin(),   ap[i].end(),   0.f);
        for (int i = 0; i < kNumLines; ++i){ std::fill (line[i].begin(), line[i].end(), 0.f); damp[i] = 0.f; }
    }

    // (decay, preDelay, tone, size, diffusion, mod, mix) — same signature as before
    void setParameters (float decay01, float /*preDelayMs*/, float tone01,
                        float size01, float diffusion01, float mod01, float mix01)
    {
        decay01 = juce::jlimit (0.f, 1.f, decay01);
        size01  = juce::jlimit (0.f, 1.f, size01);
        mix01   = juce::jlimit (0.f, 1.f, mix01);

        // Feedback gain -> tail length. Up to 0.93 for long, ambient decays.
        feedback = 0.45f + decay01 * 0.48f;

        // SIZE scales the delay-line lengths (bigger = larger, longer space)
        sizeScale = 0.6f + size01 * 1.8f;      // 0.6x .. 2.4x

        // Damping (tone): higher tone = brighter tail
        float dampCut = 1500.f + juce::jlimit(0.f,1.f,tone01) * 6000.f;
        dampCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * dampCut / (float) sampleRate);

        // Diffusion amount for the input all-passes
        apGain = 0.55f + juce::jlimit(0.f,1.f,diffusion01) * 0.2f;   // ~0.55 .. 0.75

        // Modulation depth (samples) and rate — lush movement
        modDepth = 2.0f + juce::jlimit(0.f,1.f,mod01) * 7.0f;        // 2 .. 9 samples
        modInc   = 0.35f / (float) sampleRate;                       // ~0.35 Hz base

        // Constant-sum wet/dry (never boosts level)
        wet = mix01 * 0.6f;
        dry = 1.0f - mix01 * 0.6f;
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh = buffer.getNumChannels();
        if (numCh == 0) return;
        float* chL = buffer.getWritePointer (0);
        float* chR = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int n = 0; n < numSamples; ++n)
        {
            float inL = chL[n];
            float inR = chR ? chR[n] : inL;
            float mono = 0.5f * (inL + inR);

            // ---- input diffusion (series all-pass) ----
            float x = mono;
            for (int i = 0; i < kNumAP; ++i)
                x = processAllpass (i, x, apGain);

            // ---- read modulated delay lines ----
            float r[kNumLines];
            for (int i = 0; i < kNumLines; ++i)
            {
                lfoPhase[i] += modInc * (1.0f + 0.1f * i);
                if (lfoPhase[i] >= 1.0f) lfoPhase[i] -= 1.0f;
                float lfo = std::sin (lfoPhase[i] * juce::MathConstants<float>::twoPi);
                float delaySamps = lineBaseMs[i] * 0.001f * (float) sampleRate * sizeScale
                                   + lfo * modDepth;
                r[i] = readLine (i, delaySamps);
                // damping lowpass on the tail
                damp[i] += (1.0f - dampCoeff) * (r[i] - damp[i]);
                r[i] = damp[i];
            }

            // ---- Householder feedback matrix (energy-preserving) ----
            float sum = 0.f;
            for (int i = 0; i < kNumLines; ++i) sum += r[i];
            sum *= (2.0f / kNumLines);
            for (int i = 0; i < kNumLines; ++i)
            {
                float fb = (r[i] - sum) * feedback;
                writeLine (i, x + fb);   // inject diffused input + feedback
            }

            // ---- stereo output taps (decorrelated) ----
            float wetL = r[0] - r[3] + 0.5f * r[2];
            float wetR = r[1] - r[2] + 0.5f * r[3];

            chL[n] = dry * inL + wet * wetL;
            if (chR) chR[n] = dry * inR + wet * wetR;
        }
    }

private:
    static constexpr int kNumAP    = 4;
    static constexpr int kNumLines = 4;
    static constexpr double kBaseMs[kNumLines] = { 41.7, 56.3, 73.9, 89.1 };

    float processAllpass (int i, float in, float g)
    {
        auto& buf = ap[i];
        int   sz  = (int) buf.size();
        float bufOut = buf[(size_t) apIdx[i]];
        float y = -g * in + bufOut;
        buf[(size_t) apIdx[i]] = in + g * bufOut;
        if (++apIdx[i] >= sz) apIdx[i] = 0;
        return y;
    }

    float readLine (int i, float delaySamps)
    {
        auto& buf = line[i];
        int sz = (int) buf.size();
        float readPos = (float) writeIdx[i] - delaySamps;
        while (readPos < 0.f) readPos += sz;
        int i0 = (int) readPos;
        float frac = readPos - i0;
        int i1 = (i0 + 1) % sz;
        return buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);
    }

    void writeLine (int i, float v)
    {
        auto& buf = line[i];
        int sz = (int) buf.size();
        buf[(size_t) writeIdx[i]] = v;
        if (++writeIdx[i] >= sz) writeIdx[i] = 0;
    }

    double sampleRate = 48000.0;
    float feedback = 0.7f, sizeScale = 1.f, dampCoeff = 0.5f, apGain = 0.65f;
    float modDepth = 5.f, modInc = 0.0f, wet = 0.3f, dry = 0.7f;

    std::vector<float> ap[kNumAP];   int apIdx[kNumAP]   = { 0 };
    std::vector<float> line[kNumLines]; int writeIdx[kNumLines] = { 0 };
    float lineBaseMs[kNumLines] = { 0 }, damp[kNumLines] = { 0 }, lfoPhase[kNumLines] = { 0 };
};
