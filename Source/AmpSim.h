/*
    AmpSim.h  --  Amari Eclipse built-in algorithmic amp engine.
    One engine, three voicings (Clean / Crunch / High Gain). Mono, real-time,
    dependency-free C++ (no JUCE types) so it drops into the plugin's amp block.

    Signal path per sample (run at 2x oversampling internally):
      DC block -> input HPF -> bright shelf -> tube stage(s) (+interstage HPF)
      -> tone stack (Bass/Mid/Treble) -> presence -> program comp -> power clip
      -> punch/air EQ -> out trim * master

    The cabinet is NOT in here: AmpSim outputs a raw amp signal that feeds the
    plugin's existing cabinet IR loader, exactly like the NAM block does.

    Usage:
      AmpSim amp;
      amp.prepare(sampleRate);
      amp.setVoicing(AmpSim::HighGain);
      amp.setParams(gain,bass,mid,treble,presence,master);   // all 0..1
      amp.processBlock(monoBuffer, numSamples);               // in place

    Call setParams whenever a faceplate knob changes (cheap: just recomputes
    coefficients, no allocation). Ported from the tuned Python prototype.
*/

#pragma once
#include <cmath>
#include <vector>
#include <algorithm>

class AmpSim
{
    static constexpr double kPi = 3.14159265358979323846;   // M_PI is not standard (MSVC)
public:
    enum Voicing { Clean = 0, Crunch, HighGain };

    void prepare (double sampleRate)
    {
        fsBase = sampleRate;
        fs = sampleRate * 2.0;             // internal 2x oversampled rate
        // anti-image / anti-alias filters at base Nyquist (cascaded Butterworth)
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
        dcb1.reset(); dcb2.reset(); dcb3.reset();
        inHPF.reset(); interHPF.reset(); bright.reset();
        loSh.reset(); midPk.reset(); hiSh.reset(); pres.reset();
        punch.reset(); air.reset();
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

        // 2x upsample (zero-stuff + anti-image LPF, x2 gain compensation)
        for (int i = 0; i < numSamples; ++i)
        {
            os[(size_t) (2 * i)]     = 2.0f * data[i];
            os[(size_t) (2 * i + 1)] = 0.0f;
        }
        for (int k = 0; k < numSamples * 2; ++k)
            os[(size_t) k] = (float) upB.process (upA.process (os[(size_t) k]));

        // amp chain at 2x
        for (int k = 0; k < numSamples * 2; ++k)
            os[(size_t) k] = chain (os[(size_t) k]);

        // 2x downsample (anti-alias LPF + decimate)
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
        void reset() { z1 = z2 = 0; }
        inline double process (double x)
        {
            double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void setBypass() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
        void norm (double B0, double B1, double B2, double A0, double A1, double A2)
        { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }

        void setHighpass (double fs, double f, double Q)
        {
            double w = 2 * kPi * f / fs, c = std::cos (w), s = std::sin (w), al = s / (2 * Q);
            norm ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
        }
        void setLowpass (double fs, double f, double Q)
        {
            double w = 2 * kPi * f / fs, c = std::cos (w), s = std::sin (w), al = s / (2 * Q);
            norm ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
        }
        void setPeaking (double fs, double f, double gainDb, double Q)
        {
            if (std::abs (gainDb) < 1e-4) { setBypass(); return; }
            double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * f / fs;
            double c = std::cos (w), s = std::sin (w), al = s / (2 * Q);
            norm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
        }
        void setShelf (double fs, double f, double gainDb, bool high, double S = 0.9)
        {
            if (std::abs (gainDb) < 1e-4) { setBypass(); return; }
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

    // ---------------- one-pole DC blocker ----------------
    struct DCBlock
    {
        double x1 = 0, y1 = 0, R = 0.9995;
        void reset() { x1 = y1 = 0; }
        inline double process (double x) { double y = x - x1 + R * y1; x1 = x; y1 = y; return y; }
    };

    // ---------------- voicing profile ----------------
    struct Prof {
        double input_hpf, bright, bright_amt, pre_min, pre_max, stages, bias, asym;
        double interstage_hpf, power_k, comp, bass_range, mid_range, mid_bias,
               mid_freq, treble_range, air_freq, air_db, punch_freq, punch_db, out_trim;
    };
    static Prof profile (Voicing v)
    {
        switch (v)
        {
            case Clean:    return { 70, 1500, 5.0, 1.0, 6.0, 1.0, 0.0, 0.05,
                                    0,   1.1, 0.0,  12, 10, 0.0, 650, 12, 7000, 3.5, 0, 0.0, 0.9 };
            case Crunch:   return { 90, 900, 1.0, 3.0, 18.0, 1.5, 0.15, 0.25,
                                    140, 1.6, 0.07, 12, 12, 2.0, 700, 12, 0, 0.0, 110, 3.5, 0.6 };
            default:       return { 110, 700, 0.5, 8.0, 55.0, 2.0, 0.10, 0.20,
                                    180, 2.2, 0.20, 10, 12, -1.0, 750, 12, 0, 0.0, 100, 4.5, 0.42 };
        }
    }

    void recalc()
    {
        p = profile (voicing);
        drive  = p.pre_min + (p.pre_max - p.pre_min) * pGain;
        drive2 = drive * (p.stages == 1.5 ? 0.5 : 0.8);
        bias2  = p.bias * 0.5;  asym2 = p.asym * 0.5;
        tb1 = std::tanh (p.bias);  tb2 = std::tanh (bias2);

        inHPF.setHighpass (fs, p.input_hpf, 0.7071);
        if (p.interstage_hpf > 0) interHPF.setHighpass (fs, p.interstage_hpf, 0.7071);
        bright.setShelf (fs, p.bright, p.bright_amt, true);

        loSh.setShelf   (fs, 120.0, (pBass - 0.5) * p.bass_range, false);
        midPk.setPeaking (fs, p.mid_freq, (pMid - 0.5) * p.mid_range + p.mid_bias, 0.7);
        hiSh.setShelf   (fs, 3000.0, (pTreb - 0.5) * p.treble_range, true);
        pres.setShelf   (fs, 4500.0, (pPres - 0.5) * 10.0, true);
        punch.setPeaking (fs, p.punch_freq > 0 ? p.punch_freq : 1000.0, p.punch_db, 1.0);
        air.setShelf    (fs, p.air_freq > 0 ? p.air_freq : 8000.0, p.air_db, true);
        outGain = p.out_trim * (0.3 + pMast);
    }

    inline float chain (float in)
    {
        double x = dcb1.process (in);
        x = inHPF.process (x);
        x = bright.process (x);
        x = std::tanh (drive * x + p.bias + p.asym * std::max (x, 0.0)) - tb1;   // stage 1
        x = dcb2.process (x);
        if (p.stages >= 1.5)
        {
            if (p.interstage_hpf > 0) x = interHPF.process (x);
            x = std::tanh (drive2 * x + bias2 + asym2 * std::max (x, 0.0)) - tb2; // stage 2
            x = dcb3.process (x);
        }
        x = loSh.process (x);
        x = midPk.process (x);
        x = hiSh.process (x);
        x = pres.process (x);
        if (p.comp > 0.0) { env = envCoef * env + (1.0 - envCoef) * std::abs (x); x /= (1.0 + p.comp * env); }
        x = std::tanh (p.power_k * x) / std::tanh (p.power_k);                    // power clip
        if (p.punch_db > 0.0) x = punch.process (x);
        if (p.air_db   > 0.0) x = air.process (x);
        return (float) (x * outGain);
    }

    static double clamp01 (double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

    // state
    double fsBase = 48000, fs = 96000, envCoef = 0, env = 0;
    double drive = 1, drive2 = 1, bias2 = 0, asym2 = 0, tb1 = 0, tb2 = 0, outGain = 0.6;
    float  pGain = 0.5f, pBass = 0.5f, pMid = 0.5f, pTreb = 0.5f, pPres = 0.5f, pMast = 0.7f;
    Voicing voicing = Clean;
    Prof p {};
    Biquad upA, upB, dnA, dnB, inHPF, interHPF, bright, loSh, midPk, hiSh, pres, punch, air;
    DCBlock dcb1, dcb2, dcb3;
    std::vector<float> os;
};
