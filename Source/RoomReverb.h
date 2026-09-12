#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

/*
    RoomReverb — smooth Freeverb-based room reverb.

    The previous version was a custom feedback-delay-network (FDN) whose
    resonant modes rang metallically ("robotic") on clean tones. This wraps
    JUCE's well-proven juce::Reverb (a Freeverb implementation), which is
    smooth and natural, while keeping the exact same public interface the
    processor already calls — so nothing else in the chain changes.

    Knob mapping:  DECAY -> tail length (via damping),  SIZE -> room size,
                   MIX   -> wet/dry blend.
*/
class RoomReverb
{
public:
    void prepare (double sampleRateIn, int /*maxBlockSize*/)
    {
        reverb.setSampleRate (sampleRateIn > 0.0 ? sampleRateIn : 48000.0);
        reverb.reset();
    }

    void reset() { reverb.reset(); }

    // Signature kept identical to the old FDN; only decay/size/mix are used.
    void setParameters (float decay01, float /*preDelayMs*/, float /*tone01*/,
                        float size01, float /*diffusion01*/, float /*mod01*/, float mix01)
    {
        decay01 = juce::jlimit (0.f, 1.f, decay01);
        size01  = juce::jlimit (0.f, 1.f, size01);
        mix01   = juce::jlimit (0.f, 1.f, mix01);

        juce::Reverb::Parameters p;
        p.roomSize   = juce::jlimit (0.f, 1.f, 0.30f + size01 * 0.65f);
        p.damping    = juce::jlimit (0.f, 1.f, 0.90f - decay01 * 0.85f); // more decay -> longer tail
        p.wetLevel   = mix01;
        p.dryLevel   = 1.0f - mix01;
        p.width      = 1.0f;
        p.freezeMode = 0.0f;
        reverb.setParameters (p);
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        int n  = buffer.getNumSamples();
        int nc = buffer.getNumChannels();
        if (nc >= 2)
            reverb.processStereo (buffer.getWritePointer(0), buffer.getWritePointer(1), n);
        else if (nc == 1)
            reverb.processMono (buffer.getWritePointer(0), n);
    }

private:
    juce::Reverb reverb;
};
