#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

/*
    RoomReverb — smooth, STABLE reverb built on JUCE's Freeverb, with a
    pre-delay and independent tone control.

    Controls:
      DECAY    -> tail length (room size)
      SIZE     -> pre-delay (space / distance feel)
      HIGH CUT -> brightness (damping)  [low = dark, high = bright]
      MIX      -> constant-sum wet/dry (never boosts the overall level)

    The reverb runs on a copy of the signal (pure wet); the dry is mixed in
    afterwards, so pre-delay affects only the reverb, not the direct sound, and
    there is no feedback path that can run away.
*/
class RoomReverb
{
public:
    void prepare (double sr, int maxBlock)
    {
        sampleRate = (sr > 0.0 ? sr : 48000.0);
        reverb.setSampleRate (sampleRate);
        int pdMax = (int) (0.20 * sampleRate) + 8;               // up to 200 ms pre-delay
        for (int ch = 0; ch < 2; ++ch) { pd[ch].assign ((size_t) pdMax, 0.f); pdw[ch] = 0; }
        wetBuf.setSize (2, maxBlock > 0 ? maxBlock : 2048);
        reset();
    }

    void reset()
    {
        reverb.reset();
        for (int ch = 0; ch < 2; ++ch) std::fill (pd[ch].begin(), pd[ch].end(), 0.f);
        wetBuf.clear();
        rvDcX[0]=rvDcX[1]=rvDcY[0]=rvDcY[1]=0.f;
    }

    void setShimmer (bool) {}   // shimmer removed for now (kept as no-op for API compatibility)

    // (decay, preDelay[unused], highCut, size, diffusion[unused], mod[unused], mix)
    void setParameters (float decay01, float, float highCut01,
                        float size01, float, float, float mix01)
    {
        decay01=juce::jlimit(0.f,1.f,decay01); size01=juce::jlimit(0.f,1.f,size01);
        highCut01=juce::jlimit(0.f,1.f,highCut01); mix01=juce::jlimit(0.f,1.f,mix01);

        juce::Reverb::Parameters p;
        p.roomSize   = juce::jlimit(0.f,1.f, 0.30f + decay01*0.68f);   // tail length
        p.damping    = juce::jlimit(0.f,1.f, 0.95f - highCut01*0.90f); // HIGH CUT: high=bright
        p.wetLevel   = 1.0f;   // pure wet — we blend dry ourselves
        p.dryLevel   = 0.0f;
        p.width      = 1.0f;
        p.freezeMode = 0.0f;
        reverb.setParameters (p);

        predelaySamps = (int) (size01 * 0.12 * sampleRate);           // SIZE -> up to 120 ms pre-delay
        wetGain = mix01 * 0.6f;
        dryGain = 1.0f - mix01 * 0.6f;
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples(), nc = buffer.getNumChannels();
        if (nc == 0) return;
        if (wetBuf.getNumSamples() < n || wetBuf.getNumChannels() < juce::jmax(1,nc))
            wetBuf.setSize (juce::jmax(1,nc), n, false, false, true);

        // build the (pre-delayed) reverb input in wetBuf
        for (int ch = 0; ch < juce::jmin(nc,2); ++ch) {
            auto* src = buffer.getReadPointer(ch); auto* w = wetBuf.getWritePointer(ch);
            int sz = (int) pd[ch].size();
            for (int i = 0; i < n; ++i) {
                pd[ch][(size_t)pdw[ch]] = src[i];
                int rp = pdw[ch] - predelaySamps; if (rp < 0) rp += sz;
                w[i] = pd[ch][(size_t)rp];
                if (++pdw[ch] >= sz) pdw[ch] = 0;
            }
        }
        // High-pass the reverb input to strip DC / subsonic energy. The cab IR
        // sits after the main DC blocker, so its low-frequency content reaches
        // the reverb un-filtered and accumulates in the comb filters over time,
        // eventually running the output away (loud/scratchy on hot high-gain
        // patches). This one-pole high-pass (~23 Hz) prevents that build-up.
        for (int ch = 0; ch < juce::jmin(nc,2); ++ch) {
            auto* w = wetBuf.getWritePointer(ch);
            for (int i = 0; i < n; ++i) {
                float x = w[i];
                float y = x - rvDcX[ch] + 0.997f * rvDcY[ch];
                rvDcX[ch] = x; rvDcY[ch] = y;
                w[i] = y;
            }
        }
        // reverberate the wet copy
        if (nc >= 2) reverb.processStereo (wetBuf.getWritePointer(0), wetBuf.getWritePointer(1), n);
        else         reverb.processMono   (wetBuf.getWritePointer(0), n);

        // blend dry (original) + wet
        for (int ch = 0; ch < juce::jmin(nc,2); ++ch) {
            auto* d = buffer.getWritePointer(ch); auto* w = wetBuf.getReadPointer(ch);
            for (int i = 0; i < n; ++i) d[i] = dryGain*d[i] + wetGain*juce::jlimit(-2.0f,2.0f,w[i]);
        }
    }

private:
    juce::Reverb reverb;
    double sampleRate = 48000.0;
    int    predelaySamps = 0;
    float  wetGain = 0.3f, dryGain = 0.7f;
    float  rvDcX[2] = {0.f,0.f}, rvDcY[2] = {0.f,0.f};  // reverb-input DC/subsonic blocker
    std::vector<float> pd[2]; int pdw[2] = {0,0};
    juce::AudioBuffer<float> wetBuf;
};
