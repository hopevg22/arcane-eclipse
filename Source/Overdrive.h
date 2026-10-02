#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

/*
    TubeScreamerDrive — a Tube Screamer-style overdrive.

    Real overdrive that ADDS drive/saturation to push the amp, with the
    classic TS voicing: the drive is focused on the mids/highs while the
    low end stays tight and relatively clean, and the clipping is soft and
    slightly asymmetric (even-harmonic warmth). Use it in front of the amp
    to tighten and push it into more gain.

    v1.1 recalibration (toward a Plini-style OD): gentler drive scaling and
    less internal compression for a smoother, more dynamic feel, a lower
    clip bias, and a post-clip roll-off that tames the harsh upper-mid fizz.
*/
class TubeScreamerDrive
{
public:
    void prepare (double sr, int /*block*/) { sampleRate = sr; reset(); }
    void reset()
    {
        hpState[0]=hpState[1]=0.f;
        toneState[0]=toneState[1]=0.f;
        postState[0]=postState[1]=0.f;
    }

    // drive/tone/level all 0..1
    void setParameters (float drive, float tone, float level)
    {
        drive = juce::jlimit(0.f,1.f,drive);
        // Overdrive gain: 2x .. ~24x into a soft clipper (was ..34x — gentler now)
        driveGain = 2.0f + drive * 22.0f;

        // Pre-split lowpass ~720Hz — TS clips the band ABOVE this, keeping lows tight
        hpCoeff = std::exp(-2.0f*juce::MathConstants<float>::pi*720.0f/(float)sampleRate);

        // Tone: post lowpass 600Hz(dark) .. 4500Hz(bright)
        float f = 600.0f + juce::jlimit(0.f,1.f,tone)*3900.0f;
        toneCoeff = std::exp(-2.0f*juce::MathConstants<float>::pi*f/(float)sampleRate);
        toneParam = juce::jlimit(0.f,1.f,tone);

        // Post-clip roll-off ~6500Hz — removes the fizz the clipper generates
        postCoeff = std::exp(-2.0f*juce::MathConstants<float>::pi*6500.0f/(float)sampleRate);

        outLevel = juce::jlimit(0.f,1.f,level);
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        int n  = buffer.getNumSamples();
        int nc = juce::jmin(buffer.getNumChannels(),2);
        // level compensation so high drive stays usable (lighter now -> more dynamic)
        float comp = 1.0f / (1.0f + 0.025f * (driveGain - 2.0f));

        for (int ch=0; ch<nc; ++ch)
        {
            auto* d = buffer.getWritePointer(ch);
            for (int i=0;i<n;++i)
            {
                float x = d[i];

                // Split lows (kept clean/tight) from the mid/high band that gets driven
                hpState[ch] += (1.0f - hpCoeff) * (x - hpState[ch]);   // low band
                float high = x - hpState[ch];                          // mid/high band

                // Drive + asymmetric soft clip (even-harmonic TS warmth)
                float clipped = asymClip(high * driveGain);

                // Post-clip roll-off to tame fizz
                postState[ch] += (1.0f - postCoeff) * (clipped - postState[ch]);
                clipped = postState[ch];

                // Recombine with some clean low end for body & tightness
                float mixed = clipped + hpState[ch] * 0.7f;

                // Tone tilt (post lowpass blend)
                toneState[ch] += (1.0f - toneCoeff) * (mixed - toneState[ch]);
                float shaped = toneState[ch] + toneParam * (mixed - toneState[ch]);

                d[i] = shaped * comp * (0.5f + outLevel);
            }
        }
    }

private:
    static float asymClip (float x)
    {
        // Bias then remove DC -> asymmetric soft clip (even + odd harmonics)
        return std::tanh(x + 0.08f) - std::tanh(0.08f);
    }

    double sampleRate = 48000.0;
    float driveGain=8.f, hpCoeff=0.9f, toneCoeff=0.5f, toneParam=0.5f, outLevel=0.7f;
    float postCoeff=0.5f;
    float hpState[2]={0.f,0.f}, toneState[2]={0.f,0.f}, postState[2]={0.f,0.f};
};
