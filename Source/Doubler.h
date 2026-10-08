#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>

/*
    StereoDoubler (v1.1.1) — turns the (mono) amp signal into a wide stereo
    "double-tracked" guitar, placed right after the cab.

    Each side gets its own "second take": a copy of the guitar through a short
    delay whose time drifts slowly and irregularly (sum of three incommensurate
    slow sines per side, different on L and R). That gives the small, ever-
    changing timing and pitch differences (a few ms, a few cents) of a real
    double-track, so the two sides decorrelate instead of sounding like a static
    Haas delay. The right take is a touch brighter and the left a touch darker,
    like two slightly different passes.

      L = c*dry + s*takeL,   R = c*dry + s*takeR      (WIDTH sets c / s)

    The dry stays in the centre so the mono sum remains solid; level is
    normalised so engaging it does not jump in volume.
*/
class StereoDoubler
{
public:
    void prepare (double sampleRate, int /*maxBlock*/)
    {
        sr = sampleRate > 0 ? sampleRate : 48000.0;
        len = (int) (0.040 * sr) + 8;
        for (auto& b : buf) b.assign ((size_t) len, 0.f);
        reset();
    }

    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.f);
        w = 0; t = 0.0; lpL = hsR = hsZ = 0.f;
        widthSm.reset (sr, 0.05); widthSm.setCurrentAndTargetValue (width);
    }

    void setWidth (float w01) { width = juce::jlimit (0.f, 1.f, w01); widthSm.setTargetValue (width); }

    void processBlock (juce::AudioBuffer<float>& b)
    {
        if (b.getNumChannels() < 2) return;
        auto* L = b.getWritePointer (0); auto* R = b.getWritePointer (1);
        const int n = b.getNumSamples();
        const double twoPi = juce::MathConstants<double>::twoPi, dt = 1.0 / sr;
        const float fs = (float) sr;
        const float aL = std::exp (-2.f * juce::MathConstants<float>::pi * 6500.f / fs);   // darker left take
        const float aH = std::exp (-2.f * juce::MathConstants<float>::pi * 2500.f / fs);   // brighter right take (shelf)

        for (int i = 0; i < n; ++i)
        {
            t += dt;
            // irregular slow drift, normalised to about [-1, 1]
            const float mL = (float) ((std::sin (twoPi * 0.131 * t) + 0.6 * std::sin (twoPi * 0.293 * t + 1.1)
                                       + 0.35 * std::sin (twoPi * 0.471 * t + 2.3)) / 1.95);
            const float mR = (float) ((std::sin (twoPi * 0.157 * t + 0.7) + 0.6 * std::sin (twoPi * 0.337 * t + 2.9)
                                       + 0.35 * std::sin (twoPi * 0.523 * t + 0.4)) / 1.95);
            const float dL = (float) ((0.0110 + 0.0012 * mL) * sr);     // ~11 ms +-1.2 ms
            const float dR = (float) ((0.0170 + 0.0015 * mR) * sr);     // ~17 ms +-1.5 ms

            const float x = 0.5f * (L[i] + R[i]);                    // amp signal is mono here
            buf[0][(size_t) w] = x; buf[1][(size_t) w] = x;
            float tL = read (0, dL), tR = read (1, dR);
            if (++w >= len) w = 0;

            lpL = aL * lpL + (1.f - aL) * tL; tL = lpL;               // left take: gentle top roll-off
            hsZ = aH * hsZ + (1.f - aH) * tR; tR = tR + 0.25f * (tR - hsZ);   // right take: +~2 dB air

            const float wd = widthSm.getNextValue();
            const float c = 1.0f - 0.45f * wd, s = 0.80f * wd;
            const float g = 1.0f / std::sqrt (c * c + 0.5f * s * s + c * s * 0.6f);   // keep loudness steady
            L[i] = g * (c * x + s * tL);
            R[i] = g * (c * x + s * tR);
        }
    }

private:
    float read (int ch, float d) const
    {
        float rp = (float) w - d; while (rp < 0.f) rp += (float) len;
        int i0 = (int) rp; float fr = rp - (float) i0; int i1 = (i0 + 1) % len;
        return buf[ch][(size_t) i0] + fr * (buf[ch][(size_t) i1] - buf[ch][(size_t) i0]);
    }

    double sr = 48000.0, t = 0.0;
    std::vector<float> buf[2];
    int len = 2048, w = 0;
    float width = 0.6f, lpL = 0.f, hsR = 0.f, hsZ = 0.f;
    juce::SmoothedValue<float> widthSm { 0.6f };
};
