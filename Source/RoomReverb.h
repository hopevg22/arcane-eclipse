#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>

/*
    RoomReverb — lush modulated FDN reverb with an optional octave-up SHIMMER.

    Tuned so all controls at noon (DECAY 0.5, SIZE 0.5, MIX 0.5) give a smooth
    "medium room / hall" ~3.4s tail with gentle modulation and an 8 kHz high-cut
    (Valhalla Room "Medium Room" flavour). SHIMMER adds an octave-up pitch-shifted
    layer fed back through the tail for an ethereal rising wash (Valhalla Shimmer).
*/
class RoomReverb
{
public:
    void prepare (double sr, int /*maxBlock*/)
    {
        sampleRate = (sr > 0.0 ? sr : 48000.0);

        const double apMs[kNumAP] = { 13.6, 21.3, 31.1, 43.7 };
        for (int i = 0; i < kNumAP; ++i) { ap[i].assign ((size_t)(apMs[i]*0.001*sampleRate)+4, 0.f); apIdx[i]=0; }

        for (int i = 0; i < kNumLines; ++i) {
            size_t maxLen = (size_t)(kBaseMs[i]*3.0*0.001*sampleRate)+64;
            line[i].assign (maxLen, 0.f); writeIdx[i]=0; damp[i]=0.f; lfoPhase[i]=(float)i/kNumLines;
        }
        // Octave-up shimmer pitch shifter
        pitchWin = (int)(0.045 * sampleRate);            // ~45 ms window
        pitchBuf.assign ((size_t)pitchWin + 8, 0.f);
        pitchWrite = 0; pitchPhase = 0.f; shimmerLP = 0.f;
        reset();
    }

    void reset()
    {
        for (int i=0;i<kNumAP;++i)   std::fill(ap[i].begin(),ap[i].end(),0.f);
        for (int i=0;i<kNumLines;++i){ std::fill(line[i].begin(),line[i].end(),0.f); damp[i]=0.f; }
        std::fill(pitchBuf.begin(),pitchBuf.end(),0.f); shimmerLP=0.f;
    }

    void setShimmer (bool on) { shimmerOn = on; }

    void setParameters (float decay01, float /*preDelayMs*/, float tone01,
                        float size01, float diffusion01, float mod01, float mix01)
    {
        decay01 = juce::jlimit(0.f,1.f,decay01);
        size01  = juce::jlimit(0.f,1.f,size01);
        mix01   = juce::jlimit(0.f,1.f,mix01);

        // Feedback tuned so DECAY 0.5 ~= 3.4 s RT60; up to very long at 1.0
        feedback  = 0.66f + decay01 * 0.31f;             // 0.66 .. 0.97
        sizeScale = 0.6f + size01 * 1.8f;                // 0.6x .. 2.4x

        float dampCut = 3000.f + juce::jlimit(0.f,1.f,tone01) * 10000.f;  // ~8 kHz at noon
        dampCoeff = std::exp(-2.0f*juce::MathConstants<float>::pi*dampCut/(float)sampleRate);

        apGain   = 0.55f + juce::jlimit(0.f,1.f,diffusion01)*0.2f;
        modDepth = 2.0f + juce::jlimit(0.f,1.f,mod01)*7.0f;
        modInc   = 0.5f / (float)sampleRate;             // ~0.5 Hz base modulation

        wet = mix01 * 0.6f;
        dry = 1.0f - mix01 * 0.6f;
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh = buffer.getNumChannels();
        if (numCh == 0) return;
        float* chL = buffer.getWritePointer(0);
        float* chR = numCh>1 ? buffer.getWritePointer(1) : nullptr;

        for (int n = 0; n < numSamples; ++n)
        {
            float inL = chL[n];
            float inR = chR ? chR[n] : inL;
            float mono = 0.5f*(inL+inR);

            // input diffusion
            float x = mono;
            for (int i=0;i<kNumAP;++i) x = processAllpass(i, x, apGain);

            // read modulated lines
            float r[kNumLines];
            for (int i=0;i<kNumLines;++i){
                lfoPhase[i] += modInc*(1.0f+0.1f*i);
                if (lfoPhase[i]>=1.0f) lfoPhase[i]-=1.0f;
                float lfo = std::sin(lfoPhase[i]*juce::MathConstants<float>::twoPi);
                float dS = kBaseMs[i]*0.001f*(float)sampleRate*sizeScale + lfo*modDepth;
                r[i] = readLine(i, dS);
                damp[i] += (1.0f-dampCoeff)*(r[i]-damp[i]);
                r[i] = damp[i];
            }

            // SHIMMER: pitch the tail up an octave and re-inject (ethereal rise)
            float inject = x;
            if (shimmerOn) {
                float tail = 0.25f*(r[0]+r[1]+r[2]+r[3]);
                float up = octaveUp(tail);
                // gentle high-pass-ish smoothing + level, then feed back into the FDN
                shimmerLP += 0.35f*(up - shimmerLP);
                inject += 0.6f * shimmerLP;
            }

            // Householder feedback (energy-preserving)
            float sum = 0.f; for (int i=0;i<kNumLines;++i) sum += r[i];
            sum *= (2.0f/kNumLines);
            for (int i=0;i<kNumLines;++i) writeLine(i, inject + (r[i]-sum)*feedback);

            float wetL = r[0]-r[3]+0.5f*r[2];
            float wetR = r[1]-r[2]+0.5f*r[3];
            chL[n] = dry*inL + wet*wetL;
            if (chR) chR[n] = dry*inR + wet*wetR;
        }
    }

private:
    static constexpr int kNumAP=4, kNumLines=4;
    static constexpr double kBaseMs[kNumLines] = { 41.7, 56.3, 73.9, 89.1 };

    float processAllpass(int i, float in, float g){
        auto& buf=ap[i]; int sz=(int)buf.size();
        float bo=buf[(size_t)apIdx[i]]; float y=-g*in+bo; buf[(size_t)apIdx[i]]=in+g*bo;
        if(++apIdx[i]>=sz) apIdx[i]=0; return y;
    }
    float readLine(int i,float d){
        auto& buf=line[i]; int sz=(int)buf.size();
        float rp=(float)writeIdx[i]-d; while(rp<0.f) rp+=sz;
        int i0=(int)rp; float f=rp-i0; int i1=(i0+1)%sz;
        return buf[(size_t)i0]+f*(buf[(size_t)i1]-buf[(size_t)i0]);
    }
    void writeLine(int i,float v){
        auto& buf=line[i]; int sz=(int)buf.size();
        buf[(size_t)writeIdx[i]]=v; if(++writeIdx[i]>=sz) writeIdx[i]=0;
    }
    // octave-up pitch shifter (two crossfaded heads reading at 2x)
    float octaveUp(float in){
        int sz=(int)pitchBuf.size();
        pitchBuf[(size_t)pitchWrite]=in;
        float out=0.f;
        for(int h=0;h<2;++h){
            float p=pitchPhase + (h==1?0.5f:0.f); if(p>=1.f) p-=1.f;
            float delay=p*(float)pitchWin;
            float rp=(float)pitchWrite-delay; while(rp<0.f) rp+=sz;
            int i0=(int)rp; float f=rp-i0; int i1=(i0+1)%sz;
            float s=pitchBuf[(size_t)i0]+f*(pitchBuf[(size_t)i1]-pitchBuf[(size_t)i0]);
            float w=1.f-std::abs(2.f*p-1.f);   // triangular crossfade window
            out+=s*w;
        }
        pitchPhase -= 1.0f/(float)pitchWin;    // read moves 1x faster => octave up
        if(pitchPhase<0.f) pitchPhase+=1.f;
        if(++pitchWrite>=sz) pitchWrite=0;
        return out;
    }

    double sampleRate=48000.0;
    float feedback=0.82f, sizeScale=1.5f, dampCoeff=0.5f, apGain=0.65f;
    float modDepth=5.f, modInc=0.f, wet=0.3f, dry=0.7f;
    bool  shimmerOn=false;
    std::vector<float> ap[kNumAP]; int apIdx[kNumAP]={0};
    std::vector<float> line[kNumLines]; int writeIdx[kNumLines]={0};
    float damp[kNumLines]={0}, lfoPhase[kNumLines]={0};
    std::vector<float> pitchBuf; int pitchWin=2048, pitchWrite=0; float pitchPhase=0.f, shimmerLP=0.f;
};
