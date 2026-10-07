/*
    AmpSim.h  --  Amari Eclipse built-in algorithmic amp engine.
    One engine, four voicings: Clean, Crunch (British-style), High Gain / Lead
    (US high-gain style) and Clean V2 (bright, scooped US-style clean).
    Mono, real-time, dependency-free C++ (no JUCE types).

    Signal path per sample (run at 2x oversampling internally):
      DC block -> input HPF -> pre-EQ (bass cut BEFORE clipping keeps high gain
      tight, mid push, bright cap, pre-LPF) -> 1..3 tube stages (interstage HPF/LPF)
      -> post-EQ (thump back AFTER clipping, mid scoop, upper-mid peaks)
      -> presence -> knob tone stack (neutral at 0.5) -> optional comp
      -> power clip -> hi cut -> speaker HPF -> out trim * master

    v1.1 voicing pass (Oct 7 2026): Crunch and Lead re-voiced for clarity and
    chug (fitted against Amari's own amp captures on the same DI), Clean gets a
    small clarity lift, Clean V2 added. Python prototype: scratchpad ampsim2.py.

    The cabinet is NOT in here: AmpSim outputs a raw amp signal that feeds the
    plugin's cabinet IR loader, exactly like the NAM block does.

    Usage:
      AmpSim amp;
      amp.prepare(sampleRate);
      amp.setVoicing(AmpSim::HighGain);
      amp.setParams(gain,bass,mid,treble,presence,master);   // all 0..1
      amp.processBlock(monoBuffer, numSamples);               // in place
*/

#pragma once
#include <cmath>
#include <vector>
#include <algorithm>

class AmpSim
{
    static constexpr double kPi = 3.14159265358979323846;   // M_PI is not standard (MSVC)
public:
    enum Voicing { Clean = 0, Crunch, HighGain, CleanV2 };
    static constexpr int kNumVoicings = 4;

    void prepare (double sampleRate)
    {
        fsBase = sampleRate;
        fs = sampleRate * 2.0;             // internal 2x oversampled rate
        upA.setLowpass (fs, fsBase * 0.45, 0.7071);
        upB.setLowpass (fs, fsBase * 0.45, 0.5412);
        dnA.setLowpass (fs, fsBase * 0.45, 0.7071);
        dnB.setLowpass (fs, fsBase * 0.45, 0.5412);
        envCoef = std::exp (-2.0 * kPi * 30.0 / fs);
        reset();
        recalc();
    }

    void reset()
    {
        upA.reset(); upB.reset(); dnA.reset(); dnB.reset();
        dcIn.reset();
        inHPF.reset(); preLo.reset(); preMid.reset(); bright.reset(); preLPF.reset();
        for (int i = 0; i < 3; ++i) { stHPF[i].reset(); stLPF[i].reset(); dcSt[i].reset(); }
        for (auto& b : post) b.reset();
        pres.reset(); loSh.reset(); midPk.reset(); hiSh.reset(); presK.reset();
        hiCut.reset(); spkHPF.reset();
        env = 0.0;
    }

    void setVoicing (Voicing v) { voicing = v; recalc(); }

    void setParams (float gain, float bass, float mid,
                    float treble, float presence, float master)
    {
        pGain = clamp01 (gain);  pBass = clamp01 (bass);   pMid = clamp01 (mid);
        pTreb = clamp01 (treble); pPres = clamp01 (presence); pMast = clamp01 (master);
        recalc();
    }

    void processBlock (float* data, int numSamples)
    {
        if ((int) os.size() < numSamples * 2) os.resize ((size_t) numSamples * 2);

        for (int i = 0; i < numSamples; ++i)
        {
            os[(size_t) (2 * i)]     = 2.0f * data[i];
            os[(size_t) (2 * i + 1)] = 0.0f;
        }
        for (int k = 0; k < numSamples * 2; ++k)
            os[(size_t) k] = (float) upB.process (upA.process (os[(size_t) k]));

        for (int k = 0; k < numSamples * 2; ++k)
            os[(size_t) k] = chain (os[(size_t) k]);

        for (int k = 0; k < numSamples * 2; ++k)
            os[(size_t) k] = (float) dnB.process (dnA.process (os[(size_t) k]));
        for (int i = 0; i < numSamples; ++i)
            data[i] = os[(size_t) (2 * i)];
    }

private:
    // ---------------- Biquad (RBJ), transposed direct form II ----------------
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        bool on = false;
        void reset() { z1 = z2 = 0; }
        inline double process (double x)
        {
            if (! on) return x;
            double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void setBypass() { b0 = 1; b1 = b2 = a1 = a2 = 0; on = false; }
        void norm (double B0, double B1, double B2, double A0, double A1, double A2)
        { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; on = true; }

        void setHighpass (double fs, double f, double Q)
        {
            if (f <= 0) { setBypass(); return; }
            double w = 2 * kPi * f / fs, c = std::cos (w), s = std::sin (w), al = s / (2 * Q);
            norm ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
        }
        void setLowpass (double fs, double f, double Q)
        {
            if (f <= 0 || f >= fs * 0.49) { setBypass(); return; }
            double w = 2 * kPi * f / fs, c = std::cos (w), s = std::sin (w), al = s / (2 * Q);
            norm ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
        }
        void setPeaking (double fs, double f, double gainDb, double Q)
        {
            if (f <= 0 || std::abs (gainDb) < 1e-4) { setBypass(); return; }
            double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * f / fs;
            double c = std::cos (w), s = std::sin (w), al = s / (2 * Q);
            norm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
        }
        void setShelf (double fs, double f, double gainDb, bool high, double S = 0.9)
        {
            if (f <= 0 || std::abs (gainDb) < 1e-4) { setBypass(); return; }
            double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * f / fs;
            double c = std::cos (w), s = std::sin (w);
            double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2);
            double t = 2 * std::sqrt (A) * al;
            if (! high)
                norm (A * ((A + 1) - (A - 1) * c + t), 2 * A * ((A - 1) - (A + 1) * c),
                      A * ((A + 1) - (A - 1) * c - t), (A + 1) + (A - 1) * c + t,
                      -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - t);
            else
                norm (A * ((A + 1) + (A - 1) * c + t), -2 * A * ((A - 1) + (A + 1) * c),
                      A * ((A + 1) + (A - 1) * c - t), (A + 1) - (A - 1) * c + t,
                      2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - t);
        }
    };

    struct DCBlock
    {
        double x1 = 0, y1 = 0, R = 0.9995;
        void reset() { x1 = y1 = 0; }
        inline double process (double x) { double y = x - x1 + R * y1; x1 = x; y1 = y; return y; }
    };

    // ---------------- voicing profile ----------------
    struct Stage { double k, bias, asym, hpf; };          // stage 1: k scales the GAIN-knob drive; later stages: fixed gain
    struct Peak  { double f, db, q; };
    struct Prof {
        double inHpf;
        double preLoF, preLoDb, preMidF, preMidDb, preMidQ, brightF, brightDb, preLpf;
        double gMin, gMax;
        int    nStages; Stage st[3]; double isLpf;
        int    nPost;   Peak post[4];
        double presF, presDb, comp, powerK, hiCut, spkHpf, trim;
    };
    static Prof profile (Voicing v)
    {
        switch (v)
        {
            case Clean:     // Eclipse Clean: original voice + clarity lift (fuller lows, less box, a little edge)
                return { 70,   0, 0,   0, 0, 0.8,   1500, 5.0,  0,
                         1.0, 6.0,
                         1, { {1, 0.00, 0.05, 0} }, 0,
                         3, { {110, 3.5, 0.7}, {480, -3.0, 0.9}, {2600, 2.0, 1.0} },
                         7000, 3.5, 0.0, 1.1, 0, 0, 4.5 };      // trims: voicings level-matched (~-27 dBFS at default GAIN)
            case Crunch:    // British-style crunch: bright cap, tight lows into the clip, thump + upper-mid bite after
                return { 100,  220, -5.0,   0, 0, 0.8,   2796, 10.0, 8000,
                         3.0, 18.0,
                         2, { {1, 0.15, 0.25, 0}, {8.0, 0.07, 0.12, 155} }, 0,
                         4, { {105, 10.0, 0.8}, {300, -6.7, 0.5}, {1500, 0.0, 0.8}, {2200, 3.0, 1.0} },
                         2500, 6.0, 0.0, 1.64, 7282, 70, 0.51 };
            case CleanV2:   // Clean V2: bright-cap sparkle, scooped mids, round lows (US blackface style)
                return { 75,   0, 0,   0, 0, 0.8,   2500, 11.0, 0,
                         0.8, 4.5,
                         1, { {1, 0.00, 0.03, 0} }, 0,
                         3, { {100, 5.0, 0.7}, {500, -9.0, 0.7}, {3200, 5.0, 0.9} },
                         5500, 6.0, 0.0, 1.0, 0, 0, 8.0 };
            default:        // High Gain / Lead (US high-gain style): bass cut + mid push before 3 cascaded stages, V-curve after
                return { 120,  273, -9.1,   961, 4.4, 0.8,   3111, 2.0, 7000,
                         6.0, 40.0,
                         3, { {1, 0.08, 0.15, 0}, {7.55, 0.05, 0.10, 209}, {4.37, 0.02, 0.05, 350} }, 6500,
                         3, { {115, 11.0, 0.9}, {728, -12.0, 0.76}, {2490, 1.8, 1.0} },
                         3921, 2.7, 0.0, 1.10, 8511, 60, 0.57 };
        }
    }

    void recalc()
    {
        p = profile (voicing);
        drive = p.gMin + (p.gMax - p.gMin) * pGain;
        for (int i = 0; i < 3; ++i) tb[i] = std::tanh (p.st[i].bias);

        inHPF.setHighpass (fs, p.inHpf, 0.7071);
        preLo.setShelf    (fs, p.preLoF, p.preLoDb, false);
        preMid.setPeaking (fs, p.preMidF, p.preMidDb, p.preMidQ);
        bright.setShelf   (fs, p.brightF, p.brightDb, true);
        preLPF.setLowpass (fs, p.preLpf, 0.7071);
        for (int i = 0; i < 3; ++i)
        {
            if (i > 0 && i < p.nStages) { stHPF[i].setHighpass (fs, p.st[i].hpf, 0.7071); stLPF[i].setLowpass (fs, p.isLpf, 0.7071); }
            else                        { stHPF[i].setBypass(); stLPF[i].setBypass(); }
        }
        for (int i = 0; i < 4; ++i)
            if (i < p.nPost) post[i].setPeaking (fs, p.post[i].f, p.post[i].db, p.post[i].q);
            else             post[i].setBypass();
        pres.setShelf (fs, p.presF, p.presDb, true);

        // knob tone stack - neutral at 0.5 (the plugin applies its shared amp EQ after)
        loSh.setShelf    (fs, 120.0, (pBass - 0.5) * 12.0, false);
        midPk.setPeaking (fs, 700.0, (pMid - 0.5) * 12.0, 0.7);
        hiSh.setShelf    (fs, 3000.0, (pTreb - 0.5) * 12.0, true);
        presK.setShelf   (fs, 4500.0, (pPres - 0.5) * 10.0, true);

        hiCut.setLowpass   (fs, p.hiCut, 0.7071);
        spkHPF.setHighpass (fs, p.spkHpf, 0.7071);
        outGain = p.trim * (0.3 + pMast);
    }

    inline float chain (float in)
    {
        double x = dcIn.process (in);
        x = inHPF.process (x);
        x = preLo.process (x);
        x = preMid.process (x);
        x = bright.process (x);
        x = preLPF.process (x);
        for (int i = 0; i < p.nStages; ++i)
        {
            const auto& s = p.st[i];
            if (i > 0) { x = stHPF[i].process (x); x = stLPF[i].process (x); }
            const double g = (i == 0) ? drive * s.k : s.k;
            x = std::tanh (g * x + s.bias + s.asym * std::max (x, 0.0)) - tb[i];
            x = dcSt[i].process (x);
        }
        for (int i = 0; i < p.nPost; ++i) x = post[i].process (x);
        x = pres.process (x);
        x = loSh.process (x); x = midPk.process (x); x = hiSh.process (x); x = presK.process (x);
        if (p.comp > 0.0) { env = envCoef * env + (1.0 - envCoef) * std::abs (x); x /= (1.0 + p.comp * env); }
        x = std::tanh (p.powerK * x) / std::tanh (p.powerK);                      // power clip
        x = hiCut.process (x);
        x = spkHPF.process (x);
        return (float) (x * outGain);
    }

    static double clamp01 (double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

    double fsBase = 48000, fs = 96000, envCoef = 0, env = 0;
    double drive = 1, tb[3] { 0, 0, 0 }, outGain = 0.6;
    float  pGain = 0.5f, pBass = 0.5f, pMid = 0.5f, pTreb = 0.5f, pPres = 0.5f, pMast = 0.7f;
    Voicing voicing = Clean;
    Prof p {};
    Biquad upA, upB, dnA, dnB, inHPF, preLo, preMid, bright, preLPF, stHPF[3], stLPF[3], post[4],
           pres, loSh, midPk, hiSh, presK, hiCut, spkHPF;
    DCBlock dcIn, dcSt[3];
    std::vector<float> os;
};
