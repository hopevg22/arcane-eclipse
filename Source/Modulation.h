#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>

/*
    ModulationFX — a rich multi-voice stereo chorus.

    Three modulated voices per channel (LFOs spread 120 deg apart) with a
    left/right phase offset for a wide, shimmering stereo image, in the spirit
    of classic studio choruses (e.g. Blue Cat Chorus). RATE sets the LFO speed,
    DEPTH the sweep amount, MIX the dry/wet blend.
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
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch) std::fill (buf[ch].begin(), buf[ch].end(), 0.f);
        phase = 0.0;
    }

    // Signature kept: (rate, depth, mix, type) — type ignored (always chorus)
    void setParameters (float rate, float depth, float mix, int /*type*/)
    {
        lfoRate = juce::jlimit (0.01f, 8.0f, rate);
        depthMs = 1.0f + juce::jlimit (0.f, 1.f, depth) * 9.0f;   // sweep +/- 1..10 ms
        wetMix  = juce::jlimit (0.f, 1.f, mix);
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
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

                // wide stereo: offset the right channel's LFO by 90 degrees
                double chOff = (ch == 1) ? juce::MathConstants<double>::halfPi : 0.0;

                float wet = 0.f;
                for (int v = 0; v < kVoices; ++v)
                {
                    double lfo = std::sin (phase + voiceOff[v] + chOff);
                    double delMs = baseMs + depthMs * lfo;
                    float  delSamps = (float) (delMs * 0.001 * sr);
                    wet += readInterp (ch, delSamps);
                }
                wet *= (1.0f / (float) kVoices);

                data[n] = dry + wetMix * (wet - dry);
            }

            for (int ch = 0; ch < numChannels; ++ch)
                if (++widx[ch] >= (int) buf[ch].size()) widx[ch] = 0;
        }
    }

private:
    static constexpr int kVoices = 3;

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
    float  lfoRate = 1.0f, depthMs = 5.0f, wetMix = 0.5f;
    std::vector<float> buf[2];
    int widx[2] = { 0, 0 };
};
