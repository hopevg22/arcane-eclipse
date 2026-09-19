#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>

/*
    RoomReverb — lush modulated FDN with in-loop diffusion + optional SHIMMER.

    Smoothness (no metallic ring / graininess) comes from a dense diffusion
    strategy: a 6-stage input all-pass chain builds an initial echo cloud, and
    an all-pass diffuser INSIDE each feedback line multiplies echo density every
    pass (the technique behind lush plate/hall reverbs). Gentle per-line
    modulation breaks up resonant modes. Tuned so noon (DECAY/SIZE/MIX = 0.5)
    gives a smooth ~3.4 s "medium room/hall". SHIMMER adds an octave-up layer.
*/
class RoomReverb
{
public:
    void prepare (double sr, int)
    {
        sampleRate = (sr > 0.0 ? sr : 48000.0);

        // input diffusers (6 stages, short, incommensurate)
        const double apMs[kNumAP] = { 7.9, 11.3, 15.7, 21.1, 27.3, 35.9 };
        for (int i=0;i<kNumAP;++i){ ap[i].assign((size_t)(apMs[i]*0.001*sampleRate)+4,0.f); apIdx[i]=0; }

        // FDN delay lines
        for (int i=0;i<kNumLines;++i){
            size_t maxLen=(size_t)(kBaseMs[i]*3.0*0.001*sampleRate)+64;
            line[i].assign(maxLen,0.f); writeIdx[i]=0; damp[i]=0.f; lfoPhase[i]=(float)i/kNumLines;
        }
        // in-loop diffuser all-pass (one per line)
        const double lapMs[kNumLines] = { 6.7, 8.9, 11.3, 13.9 };
        for (int i=0;i<kNumLines;++i){ lap[i].assign((size_t)(lapMs[i]*0.001*sampleRate)+4,0.f); lapIdx[i]=0; }

        pitchWin=(int)(0.045*sampleRate); pitchBuf.assign((size_t)pitchWin+8,0.f);
        pitchWrite=0; pitchPhase=0.f; shimmerLP=0.f;
        reset();
    }

    void reset()
    {
        for (int i=0;i<kNumAP;++i)   std::fill(ap[i].begin(),ap[i].end(),0.f);
        for (int i=0;i<kNumLines;++i){ std::fill(line[i].begin(),line[i].end(),0.f); std::fill(lap[i].begin(),lap[i].end(),0.f); damp[i]=0.f; }
        std::fill(pitchBuf.begin(),pitchBuf.end(),0.f); shimmerLP=0.f;
    }

    void setShimmer (bool on) { shimmerOn = on; }

    void setParameters (float decay01, float, float tone01,
                        float size01, float diffusion01, float mod01, float mix01)
    {
        decay01=juce::jlimit(0.f,1.f,decay01); size01=juce::jlimit(0.f,1.f,size01); mix01=juce::jlimit(0.f,1.f,mix01);
        feedback  = 0.66f + decay01*0.31f;               // DECAY 0.5 ~ 3.4 s
        sizeScale = 0.6f + size01*1.8f;
        float dampCut = 3000.f + juce::jlimit(0.f,1.f,tone01)*10000.f;   // ~8 kHz at noon
        dampCoeff = std::exp(-2.0f*juce::MathConstants<float>::pi*dampCut/(float)sampleRate);
        apGain    = 0.62f + juce::jlimit(0.f,1.f,diffusion01)*0.16f;     // strong diffusion
        lapGain   = 0.55f;                                                // in-loop diffusion
        modDepth  = 3.0f + juce::jlimit(0.f,1.f,mod01)*6.0f;             // smooth movement
        modInc    = 0.45f/(float)sampleRate;
        wet = mix01*0.6f; dry = 1.0f - mix01*0.6f;
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples=buffer.getNumSamples(); const int numCh=buffer.getNumChannels();
        if (numCh==0) return;
        float* chL=buffer.getWritePointer(0); float* chR=numCh>1?buffer.getWritePointer(1):nullptr;

        for (int n=0;n<numSamples;++n)
        {
            float inL=chL[n], inR=chR?chR[n]:inL, mono=0.5f*(inL+inR);

            // dense input diffusion (6 stages)
            float x=mono;
            for (int i=0;i<kNumAP;++i) x=processAP(ap[i],apIdx[i],x,apGain);

            // read + damp + in-loop diffuse each line
            float r[kNumLines];
            for (int i=0;i<kNumLines;++i){
                lfoPhase[i]+=modInc*(1.0f+0.13f*i); if(lfoPhase[i]>=1.0f) lfoPhase[i]-=1.0f;
                float lfo=std::sin(lfoPhase[i]*juce::MathConstants<float>::twoPi);
                float dS=kBaseMs[i]*0.001f*(float)sampleRate*sizeScale + lfo*modDepth;
                float d=readLine(i,dS);
                damp[i]+=(1.0f-dampCoeff)*(d-damp[i]); d=damp[i];
                d=processAP(lap[i],lapIdx[i],d,lapGain);      // in-loop diffusion => density
                r[i]=d;
            }

            // shimmer: octave-up tail re-injected
            float inject=x;
            if (shimmerOn){
                float tail=0.25f*(r[0]+r[1]+r[2]+r[3]);
                float up=octaveUp(tail);
                shimmerLP+=0.35f*(up-shimmerLP);
                inject+=0.6f*shimmerLP;
            }

            // Householder feedback
            float sum=0.f; for(int i=0;i<kNumLines;++i) sum+=r[i]; sum*=(2.0f/kNumLines);
            for (int i=0;i<kNumLines;++i) writeLine(i, inject + (r[i]-sum)*feedback);

            float wetL=r[0]-r[3]+0.5f*r[2], wetR=r[1]-r[2]+0.5f*r[3];
            chL[n]=dry*inL+wet*wetL; if(chR) chR[n]=dry*inR+wet*wetR;
        }
    }

private:
    static constexpr int kNumAP=6, kNumLines=4;
    static constexpr double kBaseMs[kNumLines] = { 41.7, 56.3, 73.9, 89.1 };

    float processAP(std::vector<float>& buf,int& idx,float in,float g){
        int sz=(int)buf.size(); float bo=buf[(size_t)idx];
        float y=-g*in+bo; buf[(size_t)idx]=in+g*bo; if(++idx>=sz) idx=0; return y;
    }
    float readLine(int i,float d){
        auto& buf=line[i]; int sz=(int)buf.size(); float rp=(float)writeIdx[i]-d; while(rp<0.f) rp+=sz;
        int i0=(int)rp; float f=rp-i0; int i1=(i0+1)%sz; return buf[(size_t)i0]+f*(buf[(size_t)i1]-buf[(size_t)i0]);
    }
    void writeLine(int i,float v){ auto& buf=line[i]; int sz=(int)buf.size(); buf[(size_t)writeIdx[i]]=v; if(++writeIdx[i]>=sz) writeIdx[i]=0; }
    float octaveUp(float in){
        int sz=(int)pitchBuf.size(); pitchBuf[(size_t)pitchWrite]=in; float out=0.f;
        for(int h=0;h<2;++h){
            float p=pitchPhase+(h==1?0.5f:0.f); if(p>=1.f) p-=1.f; float delay=p*(float)pitchWin;
            float rp=(float)pitchWrite-delay; while(rp<0.f) rp+=sz; int i0=(int)rp; float f=rp-i0; int i1=(i0+1)%sz;
            float s=pitchBuf[(size_t)i0]+f*(pitchBuf[(size_t)i1]-pitchBuf[(size_t)i0]); out+=s*(1.f-std::abs(2.f*p-1.f));
        }
        pitchPhase-=1.0f/(float)pitchWin; if(pitchPhase<0.f) pitchPhase+=1.f; if(++pitchWrite>=sz) pitchWrite=0; return out;
    }

    double sampleRate=48000.0;
    float feedback=0.82f,sizeScale=1.5f,dampCoeff=0.5f,apGain=0.7f,lapGain=0.55f,modDepth=5.f,modInc=0.f,wet=0.3f,dry=0.7f;
    bool shimmerOn=false;
    std::vector<float> ap[kNumAP]; int apIdx[kNumAP]={0};
    std::vector<float> line[kNumLines]; int writeIdx[kNumLines]={0};
    std::vector<float> lap[kNumLines];  int lapIdx[kNumLines]={0};
    float damp[kNumLines]={0},lfoPhase[kNumLines]={0};
    std::vector<float> pitchBuf; int pitchWin=2048,pitchWrite=0; float pitchPhase=0.f,shimmerLP=0.f;
};
