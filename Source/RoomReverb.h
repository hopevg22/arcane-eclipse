#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

/*
    RoomReverb — selectable reverb on JUCE's stable Freeverb, with a pre-delay,
    a tone (HIGH CUT) control, a reverb-input DC/subsonic blocker, and an
    optional SHIMMER (octave-up) layer.

    Types (setType):
      0 = ROOM    — the v1.0 voice (medium room).
      1 = HALL    — longer, larger tail.
      2 = PLATE   — bright, dense, short pre-delay.
      3 = SPRING  — shorter, darker tail + light all-pass dispersion.
      4 = AMBIENT — long, smeared wash: input diffusion + long tail + slow
                    modulation of the tail (removes Freeverb's metallic ring).
      5 = SWELL   — AMBIENT plus an auto-swell: every pick attack is removed and
                    the note (dry and reverb) fades in like a volume pedal.
                    SIZE sets the swell time (~0.1 .. 1.5 s). Uses a 6 ms
                    look-ahead so the pick click never leaks through.

    SHIMMER (v1.1.1 rebuild, still a single layer — no feedback loop, can't run
    away): the reverb TAIL (already diffuse) is band-limited, pitch-shifted up an
    octave by a phase-vocoder shifter (OctaveUpSTFT: no grains, so no grain
    beating), band-limited again, then diffused through its own long reverb and
    added to the wet signal. (v1.1 shifted the raw send with 2 overlapping
    grains: partials beat against themselves, 6-54 % ripple depending on note.)

    Controls: DECAY (tail), SIZE (pre-delay; swell time in SWELL), HIGH CUT, MIX.
*/
/*
    OctaveUpSTFT — in-tune +12 semitone shifter for the shimmer (v1.1.1 r2).
    Peak-locked phase vocoder (Laroche-Dolson region shifting): each spectral
    peak's TRUE frequency is measured from its phase advance, the whole region
    around the peak (its main lobe) is moved to exactly twice that frequency and
    phase-rotated as one unit, so every harmonic of every note stays coherent
    and lands on the exact octave. 8192-pt FFT (hop 2048, Hann/Hann): fine
    enough to keep the notes of a chord apart.
    Measured on synthetic guitar notes/chords: 100 % of the octave energy within
    +-15 cents of the correct pitches (the v1.1.1 r1 bin-mapping shifter: 84-90 %
    on chords - the audible "out of tune" shimmer).
    Latency 8192 samples (~170 ms) - acts as a shimmer pre-delay.
*/
class OctaveUpSTFT
{
public:
    static constexpr int kOrder = 13, kN = 1 << kOrder, kOsamp = 4, kHop = kN / kOsamp, kBins = kN / 2 + 1;
    float ratio = 2.0f;

    void prepare()
    {
        inF.assign ((size_t) kN, 0.f); outF.assign ((size_t) kN, 0.f); frame.assign ((size_t) (2 * kN), 0.f); win.resize ((size_t) kN);
        for (auto* v : { &mag, &ph, &lastPh, &outPrev, &outNew, &yr, &yi }) v->assign ((size_t) kBins, 0.f);
        peaks.reserve ((size_t) kBins);
        for (int i = 0; i < kN; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.f * juce::MathConstants<float>::pi * (float) i / (float) kN);
        reset();
    }
    void reset()
    {
        std::fill (inF.begin(), inF.end(), 0.f); std::fill (outF.begin(), outF.end(), 0.f);
        std::fill (lastPh.begin(), lastPh.end(), 0.f); std::fill (outPrev.begin(), outPrev.end(), 0.f);
        pos = 0; hop = 0;
    }
    inline float process (float x)
    {
        inF[(size_t) pos] = x; float y = outF[(size_t) pos]; outF[(size_t) pos] = 0.f;
        if (++pos >= kN) pos = 0;
        if (++hop >= kHop) { hop = 0; doFrame(); }
        return y;
    }

private:
    void doFrame()
    {
        const float twoPi = juce::MathConstants<float>::twoPi, expct = twoPi / (float) kOsamp;
        for (int i = 0; i < kN; ++i) frame[(size_t) i] = inF[(size_t) ((pos + i) % kN)] * win[(size_t) i];
        std::fill (frame.begin() + kN, frame.end(), 0.f);
        fft.performRealOnlyForwardTransform (frame.data(), true);

        float mx = 0.f;
        for (int k = 0; k < kBins; ++k) {
            float re = frame[(size_t) (2 * k)], im = frame[(size_t) (2 * k + 1)];
            mag[(size_t) k] = std::sqrt (re * re + im * im); ph[(size_t) k] = std::atan2 (im, re);
            mx = std::max (mx, mag[(size_t) k]);
        }
        // spectral peaks (local maxima over +-2 bins)
        peaks.clear();
        const float thr = std::max (mx * 1e-5f, 1e-9f);
        for (int k = 2; k < kBins - 2; ++k) {
            const float m = mag[(size_t) k];
            if (m > thr && m > mag[(size_t) k - 1] && m >= mag[(size_t) k + 1] && m > mag[(size_t) k - 2] && m >= mag[(size_t) k + 2])
                peaks.push_back (k);
        }
        std::fill (yr.begin(), yr.end(), 0.f); std::fill (yi.begin(), yi.end(), 0.f);
        for (int k = 0; k < kBins; ++k) { float v = outPrev[(size_t) k] + expct * (float) k; outNew[(size_t) k] = v - twoPi * std::round (v / twoPi); }

        const int np = (int) peaks.size();
        for (int i = 0; i < np; ++i) {
            const int p = peaks[(size_t) i];
            float d = ph[(size_t) p] - lastPh[(size_t) p] - (float) p * expct;
            d -= twoPi * std::round (d / twoPi);
            const float fT = ((float) p + d * (float) kOsamp / twoPi) * ratio;   // target frequency (bins)
            const int q = (int) std::lround (fT);
            if (q >= kBins - 1) break;
            const int shift = q - p;
            const float rot = (outPrev[(size_t) q] + expct * fT) - ph[(size_t) p];   // phase-continuous at q
            const int lo = (i == 0) ? 0 : (peaks[(size_t) i - 1] + p) / 2 + 1;
            const int hi = (i == np - 1) ? kBins - 1 : (p + peaks[(size_t) i + 1]) / 2;
            for (int k = lo; k <= hi; ++k) {
                const int t = k + shift; if (t < 0 || t >= kBins) continue;
                const float nph = ph[(size_t) k] + rot;
                yr[(size_t) t] += mag[(size_t) k] * std::cos (nph);
                yi[(size_t) t] += mag[(size_t) k] * std::sin (nph);
                outNew[(size_t) t] = nph - twoPi * std::round (nph / twoPi);
            }
        }
        lastPh = ph; outPrev = outNew;
        for (int k = 0; k < kBins; ++k) { frame[(size_t) (2 * k)] = yr[(size_t) k]; frame[(size_t) (2 * k + 1)] = yi[(size_t) k]; }
        for (int i = 2 * kBins; i < 2 * kN; ++i) frame[(size_t) i] = 0.f;
        fft.performRealOnlyInverseTransform (frame.data());
        const float norm = 1.0f / 1.5f;                                      // Hann x Hann @ 75 % overlap
        for (int i = 0; i < kN; ++i) outF[(size_t) ((pos + i) % kN)] += frame[(size_t) i] * win[(size_t) i] * norm;
    }

    juce::dsp::FFT fft { kOrder };
    std::vector<float> inF, outF, frame, win, mag, ph, lastPh, outPrev, outNew, yr, yi;
    std::vector<int> peaks;
    int pos = 0, hop = 0;
};

class RoomReverb
{
public:
    bool tailModEnabled = true;          // slow L/R pitch drift on ambient/swell/shimmer tails
    void prepare (double sr, int maxBlock)
    {
        sampleRate = (sr > 0.0 ? sr : 48000.0);
        maxBlockSize = (maxBlock > 0 ? maxBlock : 2048);
        reverb.setSampleRate (sampleRate);
        shReverb.setSampleRate (sampleRate);
        int pdMax = (int) (0.25 * sampleRate) + 8;
        for (int ch = 0; ch < 2; ++ch) { pd[ch].assign ((size_t) pdMax, 0.f); pdw[ch] = 0; }
        wetBuf.setSize (2, maxBlockSize);
        shBuf.setSize  (2, maxBlockSize);

        octUp.prepare();                                    // shimmer pitch shifter (peak-locked PV)
        shSrc.assign ((size_t) maxBlockSize, 0.f);

        // input diffusers (ambient/swell): 4 all-passes per channel, prime-ish ms
        const float dms[kDiff] = { 4.77f, 3.59f, 12.73f, 9.31f };
        for (int ch = 0; ch < 2; ++ch)
            for (int k = 0; k < kDiff; ++k) {
                int len = (int) (dms[k] * (ch ? 1.07f : 1.0f) * 0.001 * sampleRate) + 1;
                diff[ch][k].assign ((size_t) len, 0.f); diffW[ch][k] = 0;
            }
        // tail modulation (chorus-like modulated delay on the wet)
        modMax = (int) (0.030 * sampleRate) + 8;
        for (int st = 0; st < 2; ++st) for (int ch = 0; ch < 2; ++ch) { md[st][ch].assign ((size_t) modMax, 0.f); mdw[st][ch] = 0; }
        // swell look-ahead
        laLen = (int) (0.006 * sampleRate) + 1;
        for (int ch = 0; ch < 2; ++ch) laDry[ch].assign ((size_t) laLen, 0.f);
        laW = 0;

        updateType();
        applyParams();
        reset();
    }

    void reset()
    {
        reverb.reset(); shReverb.reset();
        for (int ch = 0; ch < 2; ++ch) {
            std::fill (pd[ch].begin(), pd[ch].end(), 0.f);
            for (int st = 0; st < 2; ++st) std::fill (md[st][ch].begin(), md[st][ch].end(), 0.f);
            std::fill (laDry[ch].begin(), laDry[ch].end(), 0.f);
            for (int k = 0; k < kDiff; ++k) std::fill (diff[ch][k].begin(), diff[ch][k].end(), 0.f);
            for (int k = 0; k < kSpringAP; ++k) springZ[ch][k] = 0.f;
            shHpZ[ch] = shLpZ[ch] = shHp2Z[ch] = shLp2Z[ch] = 0.f;
            rvDcX[ch] = rvDcY[ch] = 0.f;
        }
        wetBuf.clear(); shBuf.clear(); octUp.reset();
        modPh[0][0] = modPh[1][0] = 0.0; modPh[0][1] = modPh[1][1] = 1.9;
        envFast = envSlow = 0.f; swellGain = 0.f; swellDucking = false; swellArmed = true;   // start closed: first note fades in too
    }

    void setType (int type) { int t = juce::jlimit (0, 5, type); if (t != fxType) { fxType = t; updateType(); applyParams(); } }
    void setShimmer (bool on) { if (on && ! shimmerOn) { shReverb.reset(); octUp.reset(); } shimmerOn = on; }

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
        if (wetBuf.getNumSamples() < n || wetBuf.getNumChannels() < 2) wetBuf.setSize (2, n, false, false, true);
        if (shBuf .getNumSamples() < n || shBuf .getNumChannels() < 2) shBuf .setSize (2, n, false, false, true);
        const int chans = juce::jmin (nc, 2);
        const bool ambientLike = (fxType == 4 || fxType == 5);

        // 0) SWELL: detect pick attacks on the undelayed input, then apply the
        //    swell gain to look-ahead-delayed dry + send (so the attack is removed)
        if (fxType == 5) processSwell (buffer, chans, n);

        // 1) pre-delayed reverb send
        for (int ch = 0; ch < 2; ++ch) {
            auto* src = buffer.getReadPointer (juce::jmin (ch, chans - 1));
            auto* w = wetBuf.getWritePointer (ch);
            int sz = (int) pd[ch].size();
            for (int i = 0; i < n; ++i) {
                pd[ch][(size_t) pdw[ch]] = src[i];
                int rp = pdw[ch] - predelaySamps; if (rp < 0) rp += sz;
                w[i] = pd[ch][(size_t) rp];
                if (++pdw[ch] >= sz) pdw[ch] = 0;
            }
        }
        // 2) DC/subsonic blocker on the send (~23 Hz) — v1.0 stability fix
        for (int ch = 0; ch < 2; ++ch) {
            auto* w = wetBuf.getWritePointer (ch);
            for (int i = 0; i < n; ++i) {
                float x = w[i], y = x - rvDcX[ch] + 0.997f * rvDcY[ch];
                rvDcX[ch] = x; rvDcY[ch] = y; w[i] = y;
            }
        }
        // 2a) shimmer source = the dry send (before diffusion / reverb): shifting
        //     the notes themselves keeps the octave on pitch; the reverb tail
        //     carries the reverb's own resonances (measured less in tune)
        if (shimmerOn) {
            if ((int) shSrc.size() < n) shSrc.assign ((size_t) n, 0.f);
            auto* l = wetBuf.getReadPointer (0); auto* r = wetBuf.getReadPointer (1);
            for (int i = 0; i < n; ++i) shSrc[(size_t) i] = 0.5f * (l[i] + r[i]);
        }
        // 2b) input diffusion (ambient / swell): smears attacks into a wash
        if (ambientLike) {
            for (int ch = 0; ch < 2; ++ch) {
                auto* w = wetBuf.getWritePointer (ch);
                for (int i = 0; i < n; ++i) {
                    float s = w[i];
                    for (int k = 0; k < kDiff; ++k) {
                        auto& b = diff[ch][k]; int& wi = diffW[ch][k];
                        float z = b[(size_t) wi];
                        float y = -0.65f * s + z;          // Schroeder all-pass, g = 0.65
                        b[(size_t) wi] = s + 0.65f * y;
                        if (++wi >= (int) b.size()) wi = 0;
                        s = y;
                    }
                    w[i] = s;
                }
            }
        }

        // 3) reverberate
        reverb.processStereo (wetBuf.getWritePointer (0), wetBuf.getWritePointer (1), n);

        // 3b) spring dispersion
        if (fxType == 3) {
            for (int ch = 0; ch < 2; ++ch) {
                auto* w = wetBuf.getWritePointer (ch);
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

        // 3c) SHIMMER: octave-up of the (diffuse) tail, re-diffused, added to the wet
        if (shimmerOn) processShimmer (n);

        // 3d) gentle tail drift for AMBIENT / SWELL lushness (the shimmer
        //     layer is NOT drifted - it must stay exactly on pitch)
        if (ambientLike && tailModEnabled) modulate (wetBuf, 1, n, 0.6f);

        // 4) blend dry + wet
        for (int ch = 0; ch < chans; ++ch) {
            auto* d = buffer.getWritePointer (ch); auto* w = wetBuf.getReadPointer (ch);
            for (int i = 0; i < n; ++i) d[i] = dryGain * d[i] + wetGain * juce::jlimit (-2.0f, 2.0f, w[i]);
        }
    }

private:
    // ---------------------------------------------------------------- SWELL
    void processSwell (juce::AudioBuffer<float>& buffer, int chans, int n)
    {
        const float fs = (float) sampleRate;
        const float aF = std::exp (-1.0f / (0.001f * fs)), rF = std::exp (-1.0f / (0.040f * fs));
        const float aS = std::exp (-1.0f / (0.080f * fs)), rS = std::exp (-1.0f / (0.300f * fs));
        const float duckStep = 1.0f / (0.004f * fs);                  // 4 ms fade-out on a new attack
        const float riseStep = 1.0f / (swellTime * fs);
        for (int i = 0; i < n; ++i) {
            float x = 0.f;
            for (int ch = 0; ch < chans; ++ch) x = juce::jmax (x, std::fabs (buffer.getReadPointer (ch)[i]));
            envFast = (x > envFast) ? aF * envFast + (1 - aF) * x : rF * envFast + (1 - rF) * x;
            envSlow = (x > envSlow) ? aS * envSlow + (1 - aS) * x : rS * envSlow + (1 - rS) * x;

            if (swellArmed && envFast > 0.003f && envFast > envSlow * 2.0f) { swellDucking = true; swellArmed = false; }
            if (! swellArmed && envFast < envSlow * 1.25f) swellArmed = true;

            if (swellDucking) { swellGain -= duckStep; if (swellGain <= 0.f) { swellGain = 0.f; swellDucking = false; } }
            else if (swellGain < 1.f) swellGain = juce::jmin (1.f, swellGain + riseStep);
            // equal-power-ish curve sounds more like a volume pedal than a linear ramp
            float g = swellGain * swellGain * (3.f - 2.f * swellGain);

            for (int ch = 0; ch < chans; ++ch) {
                float* d = buffer.getWritePointer (ch);
                float delayed = laDry[ch][(size_t) laW];
                laDry[ch][(size_t) laW] = d[i];
                d[i] = delayed * g;
            }
            if (++laW >= laLen) laW = 0;
        }
    }

    // -------------------------------------------------------------- SHIMMER
    void processShimmer (int n)
    {
        const float fs = (float) sampleRate;
        // band-limits: before the shifter (HP 250 / LP 4.5k) and after it (HP 600 / LP 6.5k, soft top)
        const float hp1 = std::exp (-2.f * juce::MathConstants<float>::pi * 250.f  / fs);
        const float lp1 = std::exp (-2.f * juce::MathConstants<float>::pi * 4500.f / fs);
        const float hp2 = std::exp (-2.f * juce::MathConstants<float>::pi * 600.f  / fs);
        const float lp2 = std::exp (-2.f * juce::MathConstants<float>::pi * 6500.f / fs);
        auto* oL = shBuf.getWritePointer (0); auto* oR = shBuf.getWritePointer (1);
        for (int i = 0; i < n; ++i) {
            shLpZ[0] = lp1 * shLpZ[0] + (1.f - lp1) * shSrc[(size_t) i];
            shHpZ[0] = hp1 * shHpZ[0] + (1.f - hp1) * shLpZ[0];
            float up = octUp.process (shLpZ[0] - shHpZ[0]);                    // exact +12 st
            shLp2Z[0] = lp2 * shLp2Z[0] + (1.f - lp2) * up;
            shLp2Z[1] = lp2 * shLp2Z[1] + (1.f - lp2) * shLp2Z[0];               // 2-pole top roll-off
            shHp2Z[0] = hp2 * shHp2Z[0] + (1.f - hp2) * shLp2Z[1];
            oL[i] = oR[i] = shLp2Z[1] - shHp2Z[0];
        }
        // the shimmer reverb spreads the (mono) octave layer into a wide, soft pad
        shReverb.processStereo (oL, oR, n);
        for (int ch = 0; ch < 2; ++ch) {
            auto* w = wetBuf.getWritePointer (ch); auto* s = shBuf.getReadPointer (ch);
            for (int i = 0; i < n; ++i) w[i] += shimmerAmt * s[i];
        }
    }

    // ------------------------------------------------------ TAIL MODULATION
    // Pure slow pitch drift (a modulated delay, never blended with its own
    // unmodulated copy). Set 0 drifts the SHIMMER octave layer (it has no dry
    // counterpart, so it can't beat against the guitar); set 1 gently drifts the
    // main tail for AMBIENT / SWELL lushness. L and R drift at different rates.
    void modulate (juce::AudioBuffer<float>& b, int set, int n, float depthMs)
    {
        const double rate[2] = { 0.31, 0.37 };
        const float base = (float) (0.006 * sampleRate), exc = (float) (depthMs * 0.001 * sampleRate);
        for (int ch = 0; ch < 2; ++ch) {
            float* w = b.getWritePointer (ch);
            auto& buf = md[set][ch]; int& wi = mdw[set][ch]; double& ph = modPh[set][ch];
            const double inc = juce::MathConstants<double>::twoPi * rate[ch] / sampleRate;
            for (int i = 0; i < n; ++i) {
                ph += inc; if (ph > juce::MathConstants<double>::twoPi) ph -= juce::MathConstants<double>::twoPi;
                buf[(size_t) wi] = w[i];
                float dl = base + exc * (float) std::sin (ph);
                float rp = (float) wi - dl; while (rp < 0.f) rp += (float) modMax;
                int i0 = (int) rp; float fr = rp - (float) i0; int i1 = (i0 + 1) % modMax;
                w[i] = buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
                if (++wi >= modMax) wi = 0;
            }
        }
    }

    void updateType()
    {
        switch (fxType)
        {
            case 1: rsBase=0.55f; rsSpan=0.44f; dampFloor=0.05f; dampSpan=0.80f; pdScale=0.14f; break; // hall
            case 2: rsBase=0.45f; rsSpan=0.45f; dampFloor=0.02f; dampSpan=0.70f; pdScale=0.04f; break; // plate
            case 3: rsBase=0.25f; rsSpan=0.45f; dampFloor=0.25f; dampSpan=0.70f; pdScale=0.06f; break; // spring
            case 4: rsBase=0.80f; rsSpan=0.19f; dampFloor=0.10f; dampSpan=0.65f; pdScale=0.20f; break; // ambient
            case 5: rsBase=0.78f; rsSpan=0.20f; dampFloor=0.10f; dampSpan=0.65f; pdScale=0.00f; break; // swell
            default:rsBase=0.30f; rsSpan=0.68f; dampFloor=0.05f; dampSpan=0.90f; pdScale=0.12f; break; // room
        }
    }

    void applyParams()
    {
        juce::Reverb::Parameters p;
        p.roomSize   = juce::jlimit (0.f, 1.f, rsBase + decayP * rsSpan);
        p.damping    = juce::jlimit (0.f, 1.f, (dampFloor + dampSpan) - highCutP * dampSpan);
        p.wetLevel   = 1.0f; p.dryLevel = 0.0f; p.width = 1.0f; p.freezeMode = 0.0f;
        reverb.setParameters (p);

        juce::Reverb::Parameters sp;                         // shimmer diffuser: long + soft
        sp.roomSize = juce::jlimit (0.f, 1.f, 0.80f + 0.18f * decayP);
        sp.damping  = 0.35f; sp.wetLevel = 1.0f; sp.dryLevel = 0.0f; sp.width = 1.0f; sp.freezeMode = 0.0f;
        shReverb.setParameters (sp);

        predelaySamps = (int) (sizeP * pdScale * sampleRate);
        swellTime = 0.10f + 1.40f * sizeP;                   // SWELL: SIZE = swell time
        wetGain = mixP * 0.6f;
        dryGain = 1.0f - mixP * 0.6f;
    }

    juce::Reverb reverb, shReverb;
    double sampleRate = 48000.0;
    int    maxBlockSize = 2048;
    int    predelaySamps = 0;
    float  wetGain = 0.3f, dryGain = 0.7f;
    float  rvDcX[2]={0,0}, rvDcY[2]={0,0};
    std::vector<float> pd[2]; int pdw[2]={0,0};
    juce::AudioBuffer<float> wetBuf, shBuf;

    int   fxType = 0;
    float decayP=0.5f, sizeP=0.5f, highCutP=0.5f, mixP=0.5f;
    float rsBase=0.30f, rsSpan=0.68f, dampFloor=0.05f, dampSpan=0.90f, pdScale=0.12f;

    static constexpr int kSpringAP = 4;
    float springG = 0.6f, springZ[2][kSpringAP] = {{0}};

    static constexpr int kDiff = 4;
    std::vector<float> diff[2][kDiff]; int diffW[2][kDiff] = {{0}};

    std::vector<float> md[2][2]; int mdw[2][2]={{0,0},{0,0}}; int modMax=0; double modPh[2][2]={{0.0,1.9},{0.0,1.9}};

    bool  shimmerOn = false;
    float shimmerAmt = 0.55f;                   // octave ~12 dB under the note: sits under the tone, not on top
    OctaveUpSTFT octUp;
    std::vector<float> shSrc;
    float shHpZ[2]={0,0}, shLpZ[2]={0,0}, shHp2Z[2]={0,0}, shLp2Z[2]={0,0};

    // swell
    float swellTime = 0.8f, envFast = 0.f, envSlow = 0.f, swellGain = 1.f;
    bool  swellDucking = false, swellArmed = true;
    std::vector<float> laDry[2]; int laLen = 1, laW = 0;
};
