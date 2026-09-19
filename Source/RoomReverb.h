#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>

/*
    RoomReverb — Dattorro plate reverb (Jon Dattorro, "Effect Design Part 1").

    A proven, lush plate topology: input diffusion -> a cross-coupled "tank" of
    modulated all-passes, delays and damping, with output taken from many taps
    spread across the tank for a dense, smooth, decorrelated stereo tail. This
    is the classic structure behind high-quality plate/hall reverbs.

    Controls: DECAY = tail length, SIZE = space scale, MIX = wet/dry (constant
    sum). Optional SHIMMER injects an octave-up copy of the tail.
*/
class RoomReverb
{
public:
    void prepare (double sr, int)
    {
        sampleRate = (sr > 0.0 ? sr : 48000.0);
        srScale = (float) (sampleRate / 29761.0);        // Dattorro base rate

        auto mk = [this](DBuf& d, float baseLen){ d.buf.assign((size_t)(baseLen*srScale*1.6f)+64, 0.f); d.w=0; };
        mk(id1,142); mk(id2,107); mk(id3,379); mk(id4,277);
        mk(mApL,672); mk(mApR,908);
        mk(preL,4453); mk(preR,4217);
        mk(apL2,1800); mk(apR2,2656);
        mk(postL,3720); mk(postR,3163);

        pitchWin=(int)(0.045*sampleRate); pitchBuf.assign((size_t)pitchWin+8,0.f); pitchWrite=0; pitchPhase=0.f; shimmerLP=0.f;
        reset();
    }

    void reset()
    {
        for (DBuf* d : all()) { std::fill(d->buf.begin(), d->buf.end(), 0.f); d->w=0; }
        bwState=dampL=dampR=0.f; lastPostL=lastPostR=0.f; lfo1=0.f; lfo2=0.25f;
        std::fill(pitchBuf.begin(),pitchBuf.end(),0.f); shimmerLP=0.f;
    }

    void setShimmer (bool on) { shimmerOn = on; }

    void setParameters (float decay01, float, float tone01,
                        float size01, float /*diffusion*/, float mod01, float mix01)
    {
        decay01=juce::jlimit(0.f,1.f,decay01); size01=juce::jlimit(0.f,1.f,size01); mix01=juce::jlimit(0.f,1.f,mix01);
        decay   = 0.40f + decay01*0.58f;                 // 0.40 .. 0.98 (noon ~0.69, long tail)
        sizeMul = 0.7f + size01*0.9f;                    // 0.7x .. 1.6x tank size
        // damping (tone -> high cut): higher tone = brighter
        float dCut = 2500.f + juce::jlimit(0.f,1.f,tone01)*9000.f;
        dampCoeff = std::exp(-2.0f*juce::MathConstants<float>::pi*dCut/(float)sampleRate);
        bw = 0.9995f;                                    // input bandwidth (bright)
        modExc = (2.0f + juce::jlimit(0.f,1.f,mod01)*8.0f) * srScale;   // allpass excursion (samples)
        wet = mix01*0.6f; dry = 1.0f - mix01*0.6f;
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples=buffer.getNumSamples(); const int numCh=buffer.getNumChannels();
        if (numCh==0) return;
        float* chL=buffer.getWritePointer(0); float* chR=numCh>1?buffer.getWritePointer(1):nullptr;
        const float S = srScale * sizeMul;
        const float lfoInc1 = 0.9f/(float)sampleRate, lfoInc2 = 1.3f/(float)sampleRate;

        for (int n=0;n<numSamples;++n)
        {
            float inL=chL[n], inR=chR?chR[n]:inL, mono=0.5f*(inL+inR);

            // input bandwidth low-pass, then 4 diffusion all-passes
            bwState += (1.0f-bw)*(mono - bwState);
            float x=bwState;
            x=apProcess(id1,142*srScale,0.75f,x);
            x=apProcess(id2,107*srScale,0.75f,x);
            x=apProcess(id3,379*srScale,0.625f,x);
            x=apProcess(id4,277*srScale,0.625f,x);

            // shimmer injection (octave-up tail)
            float inject=x;
            if (shimmerOn){
                float up=octaveUp(0.5f*(lastPostL+lastPostR));
                shimmerLP+=0.35f*(up-shimmerLP);
                inject += 0.55f*shimmerLP;
            }

            // LFOs for the modulated tank all-passes
            lfo1+=lfoInc1; if(lfo1>=1.f) lfo1-=1.f;
            lfo2+=lfoInc2; if(lfo2>=1.f) lfo2-=1.f;
            float m1=std::sin(lfo1*juce::MathConstants<float>::twoPi)*modExc;
            float m2=std::sin(lfo2*juce::MathConstants<float>::twoPi)*modExc;

            // ---- tank (figure-8, cross-coupled via last outputs) ----
            float leftIn  = inject + lastPostR*decay;
            float rightIn = inject + lastPostL*decay;

            // left half
            float aL = apProcessMod(mApL, 672*S + m1, 0.70f, leftIn);
            float dL = delayProcess(preL, 4453*S, aL);
            dampL += (1.0f-dampCoeff)*(dL-dampL); dL = dampL*decay;
            float bL = apProcess(apL2, 1800*S, 0.50f, dL);
            float pL = delayProcess(postL, 3720*S, bL);

            // right half
            float aR = apProcessMod(mApR, 908*S + m2, 0.70f, rightIn);
            float dR = delayProcess(preR, 4217*S, aR);
            dampR += (1.0f-dampCoeff)*(dR-dampR); dR = dampR*decay;
            float bR = apProcess(apR2, 2656*S, 0.50f, dR);
            float pR = delayProcess(postR, 3163*S, bR);

            lastPostL=pL; lastPostR=pR;

            // ---- output taps (Dattorro), scaled by S ----
            float yL = tap(preR,266*S)+tap(preR,2974*S)-tap(apR2,1913*S)+tap(postR,1996*S)
                       -tap(preL,1990*S)-tap(apL2,187*S)-tap(postL,1066*S);
            float yR = tap(preL,353*S)+tap(preL,3627*S)-tap(apL2,1228*S)+tap(postL,2673*S)
                       -tap(preR,2111*S)-tap(apR2,335*S)-tap(postR,121*S);
            yL*=0.6f; yR*=0.6f;

            chL[n]=dry*inL+wet*yL; if(chR) chR[n]=dry*inR+wet*yR;
        }
    }

private:
    struct DBuf { std::vector<float> buf; int w=0; };
    DBuf id1,id2,id3,id4, mApL,mApR, preL,preR, apL2,apR2, postL,postR;
    std::vector<DBuf*> all(){ return {&id1,&id2,&id3,&id4,&mApL,&mApR,&preL,&preR,&apL2,&apR2,&postL,&postR}; }

    static float readInterp(DBuf& d, float delay){
        int sz=(int)d.buf.size(); float rp=(float)d.w-delay; while(rp<0.f) rp+=sz;
        int i0=(int)rp; float f=rp-i0; int i1=(i0+1)%sz; return d.buf[(size_t)i0]+f*(d.buf[(size_t)i1]-d.buf[(size_t)i0]);
    }
    static void push(DBuf& d,float v){ d.buf[(size_t)d.w]=v; if(++d.w>=(int)d.buf.size()) d.w=0; }
    static float tap(DBuf& d,float off){ return readInterp(d,off); }

    // fixed all-pass
    static float apProcess(DBuf& d,float len,float g,float in){
        float bo=readInterp(d,len); float y=-g*in+bo; push(d,in+g*bo); return y;
    }
    // modulated all-pass (interpolated length)
    static float apProcessMod(DBuf& d,float len,float g,float in){
        float bo=readInterp(d,len); float y=-g*in+bo; push(d,in+g*bo); return y;
    }
    // pure delay
    static float delayProcess(DBuf& d,float len,float in){ float o=readInterp(d,len); push(d,in); return o; }

    float octaveUp(float in){
        int sz=(int)pitchBuf.size(); pitchBuf[(size_t)pitchWrite]=in; float out=0.f;
        for(int h=0;h<2;++h){ float p=pitchPhase+(h==1?0.5f:0.f); if(p>=1.f)p-=1.f; float dl=p*(float)pitchWin;
            float rp=(float)pitchWrite-dl; while(rp<0.f)rp+=sz; int i0=(int)rp; float f=rp-i0; int i1=(i0+1)%sz;
            out+=(pitchBuf[(size_t)i0]+f*(pitchBuf[(size_t)i1]-pitchBuf[(size_t)i0]))*(1.f-std::abs(2.f*p-1.f)); }
        pitchPhase-=1.0f/(float)pitchWin; if(pitchPhase<0.f)pitchPhase+=1.f; if(++pitchWrite>=sz)pitchWrite=0; return out;
    }

    double sampleRate=48000.0; float srScale=1.6f;
    float decay=0.7f,sizeMul=1.f,dampCoeff=0.5f,bw=0.9995f,modExc=8.f,wet=0.3f,dry=0.7f;
    float bwState=0.f,dampL=0.f,dampR=0.f,lastPostL=0.f,lastPostR=0.f,lfo1=0.f,lfo2=0.25f;
    bool shimmerOn=false;
    std::vector<float> pitchBuf; int pitchWin=2048,pitchWrite=0; float pitchPhase=0.f,shimmerLP=0.f;
};
