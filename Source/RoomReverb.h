#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>

/*
    RoomReverb — selectable reverb (v1.1) on JUCE's stable Freeverb, with a
    pre-delay, a tone (HIGH CUT) control, a reverb-input DC/subsonic blocker,
    and an optional SHIMMER (octave-up) layer.

    Types (setType):
      0 = ROOM   — the v1.0 voice (medium room).
      1 = HALL   — longer, larger tail, brighter decay.
      2 = PLATE  — bright, dense, short pre-delay.
      3 = SPRING — shorter, darker tail + light all-pass dispersion ("sproing").

    SHIMMER (setShimmer true): a single octave-up layer. The reverb SEND is
    pitch-shifted up one octave (per-sample two-grain crossfade) and added to the
    send before it enters the reverb — a feed-FORWARD layer, NOT a regenerative
    output feedback loop. This is the "single layer" shimmer (Valhalla feedback=0)
    and it cannot run away (validated stable even at full amount with a long
    tail), which is why it's safe to ship after v1.0 pulled the feedback version.

    Controls: DECAY (tail), SIZE (pre-delay), HIGH CUT (brightness), MIX.
*/
class RoomReverb
{
public:
    void prepare (double sr, int maxBlock)
    {
        sampleRate = (sr > 0.0 ? sr : 48000.0);
        maxBlockSize = (maxBlock > 0 ? maxBlock : 2048);
        reverb.setSampleRate (sampleRate);
        int pdMax = (int) (0.20 * sampleRate) + 8;
        for (int ch = 0; ch < 2; ++ch) { pd[ch].assign ((size_t) pdMax, 0.f); pdw[ch] = 0; }
        wetBuf.setSize (2, maxBlockSize);

        shMax = (int) (0.12 * sampleRate) + 8;      // grain buffer
        grain = (float) (0.050 * sampleRate);       // 50 ms grains
        for (int ch = 0; ch < 2; ++ch) { sh[ch].assign ((size_t) shMax, 0.f); shw[ch] = 0; }
        updateType();
        reset();
    }

    void reset()
    {
        reverb.reset();
        for (int ch = 0; ch < 2; ++ch) { std::fill (pd[ch].begin(), pd[ch].end(), 0.f);
                                         std::fill (sh[ch].begin(), sh[ch].end(), 0.f);
                                         for (int k=0;k<kSpringAP;++k) springZ[ch][k]=0.f; }
        wetBuf.clear();
        rvDcX[0]=rvDcX[1]=rvDcY[0]=rvDcY[1]=0.f;
        shHpX[0]=shHpX[1]=shHpY[0]=shHpY[1]=0.f;
        shPhase = 0.f;
    }

    void setType (int type) { fxType = juce::jlimit (0, 3, type); updateType(); applyParams(); }
    void setShimmer (bool on) { shimmerOn = on; }

    // (decay, preDelay[unused], highCut, size, diffusion[unused], mod[unused], mix)
    void setParameters (float decay01, float, float highCut01,
                        float size01, float, float, float mix01)
    {
        decayP=juce::jlimit(0.f,1.f,decay01); sizeP=juce::jlimit(0.f,1.f,size01);
        highCutP=juce::jlimit(0.f,1.f,highCut01); mixP=juce::jlimit(0.f,1.f,mix01);
        applyParams();
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples(), nc = buffer.getNumChannels();
        if (nc == 0) return;
        if (wetBuf.getNumSamples() < n || wetBuf.getNumChannels() < juce::jmax(1,nc))
            wetBuf.setSize (juce::jmax(1,nc), n, false, false, true);
        const int chans = juce::jmin(nc,2);

        // 1) pre-delayed reverb send
        for (int ch = 0; ch < chans; ++ch) {
            auto* src = buffer.getReadPointer(ch); auto* w = wetBuf.getWritePointer(ch);
            int sz = (int) pd[ch].size();
            for (int i = 0; i < n; ++i) {
                pd[ch][(size_t)pdw[ch]] = src[i];
                int rp = pdw[ch] - predelaySamps; if (rp < 0) rp += sz;
                w[i] = pd[ch][(size_t)rp];
                if (++pdw[ch] >= sz) pdw[ch] = 0;
            }
        }
        // 2) DC/subsonic blocker on the send (~23 Hz) — v1.0 stability fix
        for (int ch = 0; ch < chans; ++ch) {
            auto* w = wetBuf.getWritePointer(ch);
            for (int i = 0; i < n; ++i) {
                float x = w[i];
                float y = x - rvDcX[ch] + 0.997f * rvDcY[ch];
                rvDcX[ch] = x; rvDcY[ch] = y;
                w[i] = y;
            }
        }
        // 2b) SHIMMER: add a single octave-up layer to the SEND (feed-forward, no loop)
        if (shimmerOn) {
            for (int i = 0; i < n; ++i) {
                float p1 = shPhase, p2 = shPhase + 0.5f; if (p2 >= 1.f) p2 -= 1.f;
                float d1 = (1.f - p1) * grain, d2 = (1.f - p2) * grain;
                float g1 = std::sin (juce::MathConstants<float>::pi * p1);
                float g2 = std::sin (juce::MathConstants<float>::pi * p2);
                for (int ch = 0; ch < chans; ++ch) {
                    auto* w = wetBuf.getWritePointer(ch);
                    sh[ch][(size_t)shw[ch]] = w[i];
                    float up = g1 * tap (ch, d1) + g2 * tap (ch, d2);
                    if (++shw[ch] >= shMax) shw[ch] = 0;
                    float hx = up, hy = hx - shHpX[ch] + 0.995f * shHpY[ch];
                    shHpX[ch] = hx; shHpY[ch] = hy;
                    w[i] = w[i] + shimmerAmt * hy;
                }
                shPhase += 1.0f / grain; if (shPhase >= 1.f) shPhase -= 1.f;   // octave up
            }
        }
        // 3) reverberate
        if (nc >= 2) reverb.processStereo (wetBuf.getWritePointer(0), wetBuf.getWritePointer(1), n);
        else         reverb.processMono   (wetBuf.getWritePointer(0), n);

        // 3b) spring dispersion
        if (fxType == 3) {
            for (int ch = 0; ch < chans; ++ch) {
                auto* w = wetBuf.getWritePointer(ch);
                for (int i = 0; i < n; ++i) {
                    float s = w[i];
                    for (int k = 0; k < kSpringAP; ++k) {
                        float v = -springG * s + springZ[ch][k];
                        springZ[ch][k] = s + springG * v; s = v;
                    }
                    w[i] = s;
                }
            }
        }

        // 4) blend dry + wet
        for (int ch = 0; ch < chans; ++ch) {
            auto* d = buffer.getWritePointer(ch); auto* w = wetBuf.getReadPointer(ch);
            for (int i = 0; i < n; ++i) d[i] = dryGain*d[i] + wetGain*juce::jlimit(-2.0f,2.0f,w[i]);
        }
    }

private:
    void updateType()
    {
        switch (fxType)
        {
            case 1: rsBase=0.55f; rsSpan=0.44f; dampFloor=0.05f; dampSpan=0.80f; pdScale=0.14f; break; // hall
            case 2: rsBase=0.45f; rsSpan=0.45f; dampFloor=0.02f; dampSpan=0.70f; pdScale=0.04f; break; // plate
            case 3: rsBase=0.25f; rsSpan=0.45f; dampFloor=0.25f; dampSpan=0.70f; pdScale=0.06f; break; // spring
            default:rsBase=0.30f; rsSpan=0.68f; dampFloor=0.05f; dampSpan=0.90f; pdScale=0.12f; break; // room
        }
    }

    void applyParams()
    {
        juce::Reverb::Parameters p;
        p.roomSize   = juce::jlimit(0.f,1.f, rsBase + decayP*rsSpan);
        p.damping    = juce::jlimit(0.f,1.f, (dampFloor + dampSpan) - highCutP*dampSpan);
        p.wetLevel   = 1.0f; p.dryLevel = 0.0f; p.width = 1.0f; p.freezeMode = 0.0f;
        reverb.setParameters (p);
        predelaySamps = (int) (sizeP * pdScale * sampleRate);
        wetGain = mixP * 0.6f;
        dryGain = 1.0f - mixP * 0.6f;
    }

    float tap (int ch, float delaySamps)
    {
        auto& b = sh[ch];
        float rp = (float) shw[ch] - delaySamps;
        while (rp < 0.f) rp += shMax;
        int i0 = (int) rp; float fr = rp - i0; int i1 = (i0 + 1) % shMax;
        return b[(size_t) i0] + fr * (b[(size_t) i1] - b[(size_t) i0]);
    }

    juce::Reverb reverb;
    double sampleRate = 48000.0;
    int    maxBlockSize = 2048;
    int    predelaySamps = 0;
    float  wetGain = 0.3f, dryGain = 0.7f;
    float  rvDcX[2]={0,0}, rvDcY[2]={0,0};
    std::vector<float> pd[2]; int pdw[2]={0,0};
    juce::AudioBuffer<float> wetBuf;

    int   fxType = 0;
    float decayP=0.5f, sizeP=0.5f, highCutP=0.5f, mixP=0.5f;
    float rsBase=0.30f, rsSpan=0.68f, dampFloor=0.05f, dampSpan=0.90f, pdScale=0.12f;

    static constexpr int kSpringAP = 4;
    float springG = 0.6f, springZ[2][kSpringAP] = {{0}};

    bool  shimmerOn = false;
    float shimmerAmt = 0.6f;
    std::vector<float> sh[2]; int shw[2]={0,0}; int shMax=0; float grain=2400.f, shPhase=0.f;
    float shHpX[2]={0,0}, shHpY[2]={0,0};
};
