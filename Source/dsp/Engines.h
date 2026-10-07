#pragma once

#include "Common.h"
#include "Tables.h"
#include "../Params.h"

// Synthesis engines. Each engine renders a mono signal for one voice.
// p[] is the (effective) timbre parameter array indexed by rc::TP.
namespace rc
{
struct StartInfo
{
    int note = 60;
    float vel = 0.8f;      // 0..1
    float sr = 48000.0f;
    bool firstNote = true; // no other key held in this timbre (organ percussion, legato)
    uint32_t seed = 1;
};

inline float unisonOffset (int u, int n)
{
    if (n <= 1) return 0.0f;
    return -1.0f + 2.0f * (float) u / (float) (n - 1);
}

//==============================================================================
struct VaOsc
{
    float ph = 0.0f;
    inline void adv (float inc) { ph += inc; if (ph >= 1.0f) ph -= 1.0f; }
    inline float saw (float inc) const { return 2.0f * ph - 1.0f - polyBlep (ph, inc); }
    inline float pulse (float inc, float pw) const
    {
        float v = ph < pw ? 1.0f : -1.0f;
        v += polyBlep (ph, inc);
        float t2 = ph - pw;
        if (t2 < 0.0f) t2 += 1.0f;
        v -= polyBlep (t2, inc);
        return v;
    }
    inline float tri() const { return 1.0f - 4.0f * std::abs (ph - 0.5f); }
    inline float sine() const { return fastSin01 (ph); }
    // morph 0..1 : saw -> square -> triangle -> sine
    inline float morph (float m, float inc, float pw) const
    {
        const float x = clamp01 (m) * 2.999f;
        const int i = (int) x;
        const float f = x - (float) i;
        auto shape = [&] (int s)
        {
            switch (s) { case 0: return saw (inc); case 1: return pulse (inc, pw); case 2: return tri(); default: return sine(); }
        };
        return lerp (shape (i), shape (i + 1), f);
    }
};

//==============================================================================
struct AnalogEngine
{
    VaOsc o1[4], o2[4], sub;
    Rng rng;
    float sr = 48000.0f;

    void start (const StartInfo& s)
    {
        sr = s.sr;
        rng.seed (s.seed);
        for (int i = 0; i < 4; ++i) { o1[i].ph = rng.next01(); o2[i].ph = rng.next01(); }
        sub.ph = 0.0f;
    }

    void render (float* out, int n, float freq, const float* p)
    {
        static const float intervals[] = { -24, -12, -7, -5, 0, 3, 4, 5, 7, 12, 19, 24 };
        const int uni = std::clamp ((int) p[tpUnison], 1, 4);
        const float det = p[tpDetune] * 30.0f;
        const float semi2 = pick (intervals, p[tpE4]) + p[tpE3] * 0.5f; // up to +50 cents
        const float ratio2 = std::exp2 (semi2 / 12.0f);
        const float mix = p[tpE5], subLvl = p[tpE6], noise = p[tpE7] * 0.6f;
        const float pw = 0.5f + 0.45f * p[tpE8];
        const float norm = 1.0f / std::sqrt ((float) uni);

        float inc1[4], inc2[4];
        for (int u = 0; u < uni; ++u)
        {
            const float d = std::exp2 (unisonOffset (u, uni) * det / 1200.0f);
            inc1[u] = std::min (0.45f, freq * d / sr);
            inc2[u] = std::min (0.45f, freq * ratio2 * d / sr);
        }
        const float incSub = freq * 0.5f / sr;

        for (int i = 0; i < n; ++i)
        {
            float a = 0.0f, b = 0.0f;
            for (int u = 0; u < uni; ++u)
            {
                a += o1[u].morph (p[tpE1], inc1[u], pw);
                b += o2[u].morph (p[tpE2], inc2[u], pw);
                o1[u].adv (inc1[u]);
                o2[u].adv (inc2[u]);
            }
            float s = ((1.0f - mix) * a + mix * b) * norm;
            s += subLvl * sub.pulse (incSub, 0.5f);
            sub.adv (incSub);
            s += noise * rng.next();
            out[i] = s * 0.5f;
        }
    }
};

//==============================================================================
struct TwinEngine
{
    VaOsc o1, o2;
    Svf hpf;
    Rng rng;
    float sr = 48000.0f;

    void start (const StartInfo& s)
    {
        sr = s.sr;
        rng.seed (s.seed);
        o1.ph = 0.0f; o2.ph = rng.next01();
        hpf.reset();
    }

    void render (float* out, int n, float freq, const float* p)
    {
        const int w1 = pickIndex (p[tpE1], 4), w2 = pickIndex (p[tpE2], 4);
        const float semi = (p[tpE3] - 0.5f) * 24.0f;
        const float inc1 = std::min (0.45f, freq / sr), inc2 = std::min (0.45f, freq * std::exp2 (semi / 12.0f) / sr);
        const float mix = p[tpE4];
        hpf.set (expMap (p[tpE5], 20.0f, 4000.0f), p[tpE6] * 0.92f, sr);
        const float scream = 1.0f + p[tpE7] * 8.0f;
        const float pw = 0.5f - 0.42f * p[tpE8];

        for (int i = 0; i < n; ++i)
        {
            float a;
            switch (w1) { case 0: a = o1.tri(); break; case 1: a = o1.saw (inc1); break;
                          case 2: a = o1.pulse (inc1, pw); break; default: a = rng.next(); break; }
            float b;
            switch (w2) { case 0: b = o2.saw (inc2); break; case 1: b = o2.pulse (inc2, 0.5f); break;
                          case 2: b = o2.pulse (inc2, 0.1f); break; default: b = o2.pulse (inc2, 0.5f) * o1.pulse (inc1, 0.5f); break; }
            o1.adv (inc1); o2.adv (inc2);
            float s = (1.0f - mix) * a + mix * b;
            s = fastTanh (s * scream) / std::sqrt (scream);
            out[i] = hpf.hp (s) * 0.42f;
        }
    }
};

//==============================================================================
struct PolyEngine
{
    VaOsc o[4], sub;
    float pwmPh = 0.0f, drift = 0.0f, driftTarget = 0.0f;
    int driftCount = 0;
    Rng rng;
    float sr = 48000.0f;

    void start (const StartInfo& s)
    {
        sr = s.sr;
        rng.seed (s.seed);
        for (auto& x : o) x.ph = rng.next01();
        sub.ph = 0.0f;
        pwmPh = rng.next01();
        drift = driftTarget = rng.next() * 0.5f;
    }

    void render (float* out, int n, float freq, const float* p)
    {
        const int wave = pickIndex (p[tpE1], 3);
        const int uni = std::clamp ((int) p[tpUnison], 1, 4);
        const float det = p[tpDetune] * 30.0f;
        const float pwmRate = expMap (p[tpE3], 0.1f, 10.0f), pwmDepth = p[tpE4] * 0.4f;
        const float basePw = 0.5f + 0.4f * p[tpE2];
        const float subLvl = p[tpE5], noise = p[tpE6] * 0.5f;

        // slow analog drift (random walk), in cents
        if (++driftCount > 8) { driftCount = 0; driftTarget = rng.next(); }
        drift += (driftTarget - drift) * 0.02f;
        const float f = freq * std::exp2 (drift * p[tpE7] * 15.0f / 1200.0f);

        float inc[4];
        for (int u = 0; u < uni; ++u)
            inc[u] = std::min (0.45f, f * std::exp2 (unisonOffset (u, uni) * det / 1200.0f) / sr);
        const float incSub = f * 0.5f / sr;
        const float norm = 1.0f / std::sqrt ((float) uni);

        for (int i = 0; i < n; ++i)
        {
            pwmPh += pwmRate / sr;
            if (pwmPh >= 1.0f) pwmPh -= 1.0f;
            float pw = basePw;
            if (wave == 2) pw = std::clamp (basePw - pwmDepth * (0.5f + 0.5f * fastSin01 (pwmPh)), 0.05f, 0.95f);
            float s = 0.0f;
            for (int u = 0; u < uni; ++u)
            {
                s += wave == 0 ? o[u].saw (inc[u]) : o[u].pulse (inc[u], pw);
                o[u].adv (inc[u]);
            }
            s *= norm;
            s += subLvl * sub.pulse (incSub, 0.5f);
            sub.adv (incSub);
            s += noise * rng.next();
            out[i] = s * 0.35f;
        }
    }
};

//==============================================================================
struct FmEngine
{
    float ph[4] {}, out1[4] {}, fbHist[2] {};
    float t = 0.0f, sr = 48000.0f, vel = 0.8f;

    void start (const StartInfo& s)
    {
        sr = s.sr; vel = s.vel; t = 0.0f;
        for (int i = 0; i < 4; ++i) { ph[i] = 0.0f; out1[i] = 0.0f; }
        fbHist[0] = fbHist[1] = 0.0f;
    }

    void render (float* out, int n, float freq, const float* p)
    {
        // modulation sources per destination (bitmask of ops), carriers mask
        static const uint8_t mods[8][4] = {
            { 0b0010, 0b0100, 0b1000, 0 },     // 4>3>2>1
            { 0b0010, 0b1100, 0, 0 },          // (3+4)>2>1
            { 0b0110, 0, 0b1000, 0 },          // 4>3>1 , 2>1
            { 0b0010, 0, 0b1000, 0 },          // 2>1 , 4>3
            { 0b1000, 0b1000, 0b1000, 0 },     // 4>(1,2,3)
            { 0b0010, 0, 0, 0 },               // 2>1 , 3 , 4
            { 0b0010, 0b0100, 0, 0 },          // 3>2>1 , 4
            { 0, 0, 0, 0 },                    // additive
        };
        static const uint8_t carriers[8] = { 0b0001, 0b0001, 0b0001, 0b0101, 0b0111, 0b1101, 0b1001, 0b1111 };
        static const float ratios[] = { 0.5f, 1.0f, 1.41f, 2.0f, 2.76f, 3.0f, 3.5f, 4.0f, 5.0f, 5.4f, 6.0f, 7.0f, 7.13f, 8.0f, 9.0f, 11.0f, 14.0f };

        const int algo = pickIndex (p[tpE1], 8);
        const float r[4] = { 1.0f, pick (ratios, p[tpE2]), pick (ratios, p[tpE4]), 1.0f };
        const float velScale = 1.0f - p[tpE8] + p[tpE8] * vel * 1.3f;
        const float decay = expMap (p[tpE7], 0.03f, 6.0f);
        const float env = 0.2f + 0.8f * std::exp (-t / decay);
        const float idx[4] = { 0.0f, p[tpE3] * 8.0f * env * velScale, p[tpE5] * 8.0f * env * velScale,
                               p[tpE5] * 6.0f * env * velScale };
        const float fb = p[tpE6] * 1.3f;
        const uint8_t cmask = carriers[algo];
        int nc = 0;
        for (int i = 0; i < 4; ++i) nc += (cmask >> i) & 1;
        const float cgain = 1.0f / std::pow ((float) nc, 0.7f);
        const float detune[4] = { 1.0f, 1.0f, 1.0017f, 0.9985f };
        float inc[4];
        for (int i = 0; i < 4; ++i) inc[i] = freq * r[i] * detune[i] / sr;
        constexpr float inv2pi = 1.0f / kTwoPi;

        for (int s = 0; s < n; ++s)
        {
            float o[4];
            for (int op = 3; op >= 0; --op)
            {
                float pm = 0.0f;
                const uint8_t m = mods[algo][op];
                for (int src = op + 1; src < 4; ++src)
                    if (m & (1 << src)) pm += o[src] * idx[src];
                if (op == 3) pm += (fbHist[0] + fbHist[1]) * 0.5f * fb;
                o[op] = fastSin01 (ph[op] + pm * inv2pi);
                if (op == 3) { fbHist[1] = fbHist[0]; fbHist[0] = o[op]; }
                ph[op] += inc[op];
                if (ph[op] >= 1.0f) ph[op] -= 1.0f;
            }
            float sum = 0.0f;
            for (int op = 0; op < 4; ++op)
                if (cmask & (1 << op)) sum += o[op];
            out[s] = sum * cgain * 0.42f;
        }
        t += (float) n / sr;
    }
};

//==============================================================================
struct WaveEngine
{
    float phA[4] {}, phB[4] {};
    float t = 0.0f, sr = 48000.0f, vel = 0.8f, chiffEnv = 0.0f;
    Svf chiffFlt, breathFlt;
    Rng rng;

    void start (const StartInfo& s)
    {
        sr = s.sr; vel = s.vel; t = 0.0f;
        rng.seed (s.seed);
        for (int i = 0; i < 4; ++i) { phA[i] = rng.next01(); phB[i] = rng.next01(); }
        chiffEnv = 1.0f;
        chiffFlt.reset(); breathFlt.reset();
    }

    void render (float* out, int n, float freq, const float* p, float morphEnv)
    {
        static const float octs[] = { 1.0f, 2.0f, 2.9966f, 4.0f };
        const auto& wt = WaveTables::get();
        const int wa = pickIndex (p[tpE1], WaveTables::kWaves), wb = pickIndex (p[tpE2], WaveTables::kWaves);
        const float m = clamp01 (p[tpE3] + p[tpE4] * morphEnv + p[tpE5] * (vel - 0.6f));
        const float bRatio = pick (octs, p[tpE8]);
        const int uni = std::clamp ((int) p[tpUnison], 1, 4);
        const float det = p[tpDetune] * 30.0f;
        const float norm = 1.0f / std::sqrt ((float) uni);
        float incA[4], incB[4];
        for (int u = 0; u < uni; ++u)
        {
            const float d = std::exp2 (unisonOffset (u, uni) * det / 1200.0f);
            incA[u] = freq * d / sr;
            incB[u] = freq * bRatio * d / sr;
        }
        const float* ta = wt.table (wa, WaveTables::levelFor (incA[0]));
        const float* tb = wt.table (wb, WaveTables::levelFor (incB[0]));
        chiffFlt.set (std::min (freq * 3.0f, 9000.0f), 0.6f, sr);
        breathFlt.set (std::min (freq * 2.0f, 8000.0f), 0.3f, sr);
        const float chiffDec = std::exp (-1.0f / (0.03f * sr));
        const float chiffLvl = p[tpE6] * (0.4f + vel), breath = p[tpE7] * 0.35f;

        for (int i = 0; i < n; ++i)
        {
            float a = 0.0f, b = 0.0f;
            for (int u = 0; u < uni; ++u)
            {
                a += WaveTables::read (ta, phA[u]);
                b += WaveTables::read (tb, phB[u]);
                phA[u] += incA[u]; if (phA[u] >= 1.0f) phA[u] -= 1.0f;
                phB[u] += incB[u]; if (phB[u] >= 1.0f) phB[u] -= 1.0f;
            }
            float s = ((1.0f - m) * a + m * b) * norm;
            const float nz = rng.next();
            s += chiffFlt.bp (nz) * chiffEnv * chiffLvl;
            s += breathFlt.bp (nz) * breath;
            chiffEnv *= chiffDec;
            out[i] = s * 0.6f;
        }
        t += (float) n / sr;
    }
};

//==============================================================================
struct DrumEngine
{
    enum Inst { Kick, Snare, Clap, ClosedHat, PedalHat, OpenHat, Crash, Ride, RideBell, Tom, Rim,
                Cowbell, Clave, Shaker, Tamb, Conga };
    Inst inst = Kick;
    float sr = 48000.0f, vel = 0.8f, t = 0.0f;
    float ph[6] {}, ph0 = 0.0f, toneF = 100.0f, amp = 1.0f, ampDec = 0.999f, noiseAmp = 0.0f, noiseDec = 0.999f;
    float pitchEnv = 0.0f, pitchDec = 0.99f, clickEnv = 0.0f;
    Biquad nf1, nf2, mf;
    Rng rng;
    bool finished = false;

    static Inst instFor (int note)
    {
        switch (note)
        {
            case 35: case 36: return Kick;      case 37: return Rim;
            case 38: case 40: return Snare;     case 39: return Clap;
            case 41: case 43: case 45: case 47: case 48: case 50: return Tom;
            case 42: return ClosedHat;          case 44: return PedalHat;    case 46: return OpenHat;
            case 49: case 52: case 55: case 57: return Crash;
            case 51: case 59: return Ride;      case 53: return RideBell;    case 54: return Tamb;
            case 56: return Cowbell;
            case 60: case 61: case 62: case 63: case 64: return Conga;
            case 69: case 70: case 82: return Shaker;
            case 75: case 76: case 77: return Clave;
            default: break;
        }
        static const Inst m[12] = { Kick, Rim, Snare, Clap, Snare, Tom, ClosedHat, Tom, PedalHat, Tom, OpenHat, Crash };
        return m[note % 12];
    }

    bool done() const { return finished; }

    float decayCoef (float sec) const { return std::exp (-1.0f / (std::max (0.001f, sec) * sr)); }

    void start (const StartInfo& s, const float* p)
    {
        sr = s.sr; vel = s.vel; t = 0.0f; finished = false;
        rng.seed (s.seed);
        inst = instFor (s.note);
        for (auto& x : ph) x = rng.next01();
        ph0 = 0.0f; clickEnv = 1.0f; pitchEnv = 0.0f;
        nf1.reset(); nf2.reset(); mf.reset();
        const float acoustic = p[tpE7];
        const float tomTune = 0.7f + 0.7f * p[tpE6];
        amp = 1.0f; noiseAmp = 0.0f;

        switch (inst)
        {
            case Kick:
                toneF = expMap (p[tpE1], 35.0f, 80.0f);
                pitchEnv = 2.5f + 1.5f * (1.0f - acoustic); pitchDec = decayCoef (0.03f);
                ampDec = decayCoef (expMap (p[tpE2], 0.12f, 1.2f) * (1.0f - 0.4f * acoustic));
                noiseAmp = 0.15f + 0.4f * acoustic; noiseDec = decayCoef (0.006f);
                nf1.lowpass (3000.0f, 0.7f, sr);
                break;
            case Snare:
                toneF = 180.0f * (0.7f + 0.7f * p[tpE3]);
                pitchEnv = 0.4f; pitchDec = decayCoef (0.02f);
                ampDec = decayCoef (0.12f);
                noiseAmp = 0.4f + 0.9f * p[tpE4]; noiseDec = decayCoef (0.15f + 0.1f * acoustic);
                nf1.highpass (1200.0f, 0.7f, sr); nf2.lowpass (9000.0f + 3000.0f * p[tpE8], 0.7f, sr);
                break;
            case Clap:
                amp = 0.0f; noiseAmp = 1.0f; noiseDec = decayCoef (0.18f);
                nf1.bandpass (1100.0f + 600.0f * p[tpE8], 1.6f, sr);
                break;
            case ClosedHat: case PedalHat: case OpenHat:
            {
                amp = 0.0f;
                const float d = inst == OpenHat ? expMap (p[tpE5], 0.25f, 1.2f)
                              : inst == PedalHat ? 0.07f : expMap (p[tpE5], 0.03f, 0.15f);
                noiseAmp = 1.0f; noiseDec = decayCoef (d);
                nf1.highpass (7000.0f, 0.8f, sr); nf2.highpass (9000.0f, 0.5f, sr);
                break;
            }
            case Crash: case Ride: case RideBell:
            {
                amp = 0.0f;
                const float d = inst == Crash ? 1.8f : inst == Ride ? 2.4f : 1.0f;
                noiseAmp = 1.0f; noiseDec = decayCoef (d);
                nf1.highpass (inst == Ride ? 3500.0f : 4500.0f, 0.6f, sr);
                nf2.bandpass (inst == RideBell ? 2600.0f : 6000.0f, inst == RideBell ? 3.0f : 0.5f, sr);
                break;
            }
            case Tom: case Conga:
            {
                static const std::pair<int, float> tomF[] = { { 41, 75 }, { 43, 90 }, { 45, 110 }, { 47, 130 },
                                                              { 48, 160 }, { 50, 190 }, { 60, 380 }, { 61, 330 },
                                                              { 62, 300 }, { 63, 250 }, { 64, 200 } };
                float f = 100.0f + 6.0f * (float) (s.note % 12);
                for (auto& tf : tomF) if (tf.first == s.note) f = tf.second;
                toneF = f * tomTune;
                pitchEnv = inst == Conga ? 0.3f : 0.8f; pitchDec = decayCoef (0.06f);
                ampDec = decayCoef (inst == Conga ? 0.18f : 0.35f + 0.2f * acoustic);
                noiseAmp = 0.1f + 0.2f * acoustic; noiseDec = decayCoef (0.04f);
                nf1.bandpass (toneF * 4.0f, 1.0f, sr);
                break;
            }
            case Rim:
                toneF = 1700.0f; ampDec = decayCoef (0.018f); noiseAmp = 0.6f; noiseDec = decayCoef (0.01f);
                nf1.bandpass (3500.0f, 1.5f, sr);
                break;
            case Cowbell:
                amp = 0.0f; noiseAmp = 0.0f; ampDec = decayCoef (0.25f);
                mf.bandpass (800.0f, 2.0f, sr);
                amp = 1.0f;
                break;
            case Clave:
                toneF = 2500.0f; ampDec = decayCoef (0.04f); noiseAmp = 0.0f;
                break;
            case Shaker:
                amp = 0.0f; noiseAmp = 0.8f; noiseDec = decayCoef (0.07f);
                nf1.highpass (6000.0f, 0.7f, sr);
                break;
            case Tamb:
                amp = 0.0f; noiseAmp = 1.0f; noiseDec = decayCoef (0.22f);
                nf1.highpass (7500.0f, 0.7f, sr); nf2.bandpass (9000.0f, 1.0f, sr);
                break;
        }
    }

    void render (float* out, int n, const float* p)
    {
        static const float metal[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
        const float noiseCol = p[tpE8];
        const float gain = (0.25f + 0.85f * vel) * 0.85f;
        float peak = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float s = 0.0f;
            const float nz = rng.next();
            switch (inst)
            {
                case Kick: case Tom: case Conga: case Snare: case Rim: case Clave:
                {
                    const float f = toneF * (1.0f + pitchEnv);
                    ph0 += f / sr; if (ph0 >= 1.0f) ph0 -= 1.0f;
                    float tone = fastSin01 (ph0);
                    if (inst == Snare)
                    {
                        ph[0] += toneF * 1.84f / sr; if (ph[0] >= 1.0f) ph[0] -= 1.0f;
                        tone = 0.6f * tone + 0.4f * fastSin01 (ph[0]);
                    }
                    s = tone * amp;
                    float nn = nf1.tick (nz);
                    if (inst == Snare) nn = nf2.tick (nn);
                    s += nn * noiseAmp;
                    if (inst == Kick) s = fastTanh (s * 1.5f);
                    pitchEnv *= pitchDec;
                    amp *= ampDec; noiseAmp *= noiseDec;
                    break;
                }
                case Clap:
                {
                    const float tt = t * 1000.0f;
                    float burst = 0.0f;
                    if (tt < 30.0f)
                    {
                        const float local = std::fmod (tt, 10.0f);
                        burst = std::exp (-local / 2.5f);
                    }
                    s = nf1.tick (nz) * (burst * 1.4f + noiseAmp * 0.6f) * 2.0f;
                    noiseAmp *= noiseDec;
                    break;
                }
                case ClosedHat: case PedalHat: case OpenHat: case Crash: case Ride: case RideBell: case Tamb:
                {
                    float m = 0.0f;
                    const float scale = (inst == Ride || inst == RideBell) ? 0.8f : (inst == Crash ? 1.1f : 1.6f);
                    for (int k = 0; k < 6; ++k)
                    {
                        ph[k] += metal[k] * scale / sr; if (ph[k] >= 1.0f) ph[k] -= 1.0f;
                        m += ph[k] < 0.5f ? 1.0f : -1.0f;
                    }
                    m *= 1.0f / 6.0f;
                    const float src = lerp (m, nz, 0.25f + 0.6f * noiseCol);
                    float x = nf1.tick (src);
                    if (inst != ClosedHat && inst != PedalHat && inst != OpenHat) x = 0.6f * x + 0.6f * nf2.tick (src);
                    else x = nf2.tick (x);
                    s = x * noiseAmp * 1.6f;
                    noiseAmp *= noiseDec;
                    break;
                }
                case Cowbell:
                {
                    ph[0] += 540.0f / sr; if (ph[0] >= 1.0f) ph[0] -= 1.0f;
                    ph[1] += 800.0f / sr; if (ph[1] >= 1.0f) ph[1] -= 1.0f;
                    const float sq = (ph[0] < 0.5f ? 0.5f : -0.5f) + (ph[1] < 0.5f ? 0.5f : -0.5f);
                    const float e = amp * (0.4f + 0.6f * std::exp (-t / 0.02f));
                    s = mf.tick (sq) * e * 2.0f;
                    amp *= ampDec;
                    break;
                }
                case Shaker:
                {
                    const float att = std::min (1.0f, t / 0.01f);
                    s = nf1.tick (nz) * noiseAmp * att;
                    noiseAmp *= noiseDec;
                    break;
                }
            }
            s *= gain;
            out[i] = s;
            peak = std::max (peak, std::abs (s));
            t += 1.0f / sr;
        }
        if (t > 0.02f && amp < 1.0e-4f && noiseAmp < 1.0e-4f && peak < 1.0e-4f)
            finished = true;
    }
};

//==============================================================================
struct OrganEngine
{
    float ph[9] {};
    float percEnv = 0.0f, clickEnv = 0.0f, percPh = 0.0f, sr = 48000.0f;
    Rng rng;
    Svf clickFlt;

    void start (const StartInfo& s, const float* p)
    {
        sr = s.sr;
        rng.seed (s.seed);
        for (auto& x : ph) x = rng.next01(); // free-running tonewheels
        percEnv = (s.firstNote && p[tpE8] > 0.02f) ? 1.0f : 0.0f;
        percPh = 0.0f;
        clickEnv = 1.0f;
        clickFlt.reset();
    }

    void render (float* out, int n, float freq, const float* p)
    {
        static const float ratios[9] = { 0.5f, 1.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f };
        float lvl[9], inc[9];
        auto db = [] (float e) { return e < 0.01f ? 0.0f : dbToGain (-3.0f * (8.0f - 8.0f * e)); };
        for (int k = 0; k < 6; ++k) lvl[k] = db (p[tpE1 + k]);
        lvl[6] = lvl[7] = lvl[8] = db (p[tpE7]);
        float sum = 0.0f;
        for (int k = 0; k < 9; ++k)
        {
            float f = freq * ratios[k];
            while (f > 6000.0f) f *= 0.5f; // tonewheel foldback
            inc[k] = f / sr;
            sum += lvl[k];
        }
        const float norm = 0.9f / std::max (1.5f, sum * 0.6f);
        const float percLvl = p[tpE8] * 0.9f;
        const float percDec = std::exp (-1.0f / (0.28f * sr));
        const float clickDec = std::exp (-1.0f / (0.004f * sr));
        clickFlt.set (3000.0f, 0.3f, sr);
        const float percInc = std::min (freq * 3.0f, 6000.0f) / sr;

        for (int i = 0; i < n; ++i)
        {
            float s = 0.0f;
            for (int k = 0; k < 9; ++k)
            {
                if (lvl[k] > 0.0f) s += lvl[k] * fastSin01 (ph[k]);
                ph[k] += inc[k]; if (ph[k] >= 1.0f) ph[k] -= 1.0f;
            }
            s *= norm;
            if (percEnv > 1.0e-4f)
            {
                percPh += percInc; if (percPh >= 1.0f) percPh -= 1.0f;
                s += percLvl * percEnv * fastSin01 (percPh) * 0.6f;
                percEnv *= percDec;
            }
            s += clickFlt.bp (rng.next()) * clickEnv * 0.25f;
            clickEnv *= clickDec;
            out[i] = s * 0.36f;
        }
    }
};

//==============================================================================
struct PluckEngine
{
    DelayLine line;
    float lp = 0.0f, ap1x = 0.0f, ap1y = 0.0f, ap2x = 0.0f, ap2y = 0.0f;
    float excite[4096] {};
    int exciteLen = 0, exciteIdx = 0;
    float pickEnv = 0.0f, sr = 48000.0f, vel = 0.8f, energy = 1.0f, t = 0.0f;
    Biquad body1, body2, body3;
    Rng rng;
    bool finished = false;

    void prepare (float s) { sr = s; line.init ((int) (s / 18.0f) + 8); }

    bool done() const { return finished; }

    void start (const StartInfo& s, const float* p, float freq)
    {
        sr = s.sr; vel = s.vel; t = 0.0f; finished = false;
        rng.seed (s.seed);
        line.clear();
        lp = ap1x = ap1y = ap2x = ap2y = 0.0f;
        energy = 1.0f;

        const float period = sr / std::max (freq, 20.0f);
        exciteLen = std::clamp ((int) period, 2, 4095);
        exciteIdx = 0;
        // shaped noise burst: brightness lowpass + pluck position comb
        OnePole f;
        f.setLP (expMap (p[tpE2], 400.0f, 16000.0f) * (0.4f + 0.8f * vel), sr);
        for (int i = 0; i < exciteLen; ++i) excite[i] = f.lp (rng.next());
        const int pos = std::max (1, (int) ((0.05f + 0.45f * p[tpE1]) * (float) exciteLen));
        // pluck-position comb, computed backwards in place
        for (int i = exciteLen - 1; i >= 0; --i)
            excite[i] = (excite[i] - (i >= pos ? excite[i - pos] : 0.0f)) * (0.3f + 0.9f * vel);
        pickEnv = 1.0f;
        body1.bandpass (110.0f, 2.0f, sr); body2.bandpass (220.0f, 2.5f, sr); body3.bandpass (450.0f, 2.0f, sr);
        body1.reset(); body2.reset(); body3.reset();
    }

    void render (float* out, int n, float freq, const float* p, bool gate)
    {
        const float bright = p[tpE4];
        const float b = 0.02f + (1.0f - bright) * 0.75f;          // loop lowpass coefficient
        const float c = -0.6f * p[tpE5];                           // dispersion allpass coefficient
        const float lpDelay = b / (1.0f - b);
        const float apDelay = (1.0f - c) / (1.0f + c);
        const float period = sr / std::max (freq, 20.0f);
        const float L = std::max (2.0f, period - lpDelay - 2.0f * apDelay);
        const float t60 = expMap (p[tpE3], 0.3f, 14.0f) * std::pow (220.0f / std::max (freq, 30.0f), 0.3f);
        const float g = std::pow (10.0f, -3.0f / (t60 * std::max (freq, 20.0f)));
        const float pickLvl = p[tpE6] * 0.6f, bodyLvl = p[tpE7], bow = gate ? p[tpE8] * 0.08f : 0.0f;
        const float pickDec = std::exp (-1.0f / (0.004f * sr));
        float peak = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float y = line.read (L);
            lp = (1.0f - b) * y + b * lp;
            // two first-order allpasses for stiffness
            const float a1 = c * lp + ap1x - c * ap1y; ap1x = lp; ap1y = a1;
            const float a2 = c * a1 + ap2x - c * ap2y; ap2x = a1; ap2y = a2;
            float in = a2 * g;
            if (exciteIdx < exciteLen) in += excite[exciteIdx++];
            if (bow > 0.0f) in += rng.next() * bow * (1.0f - std::min (1.0f, std::abs (y)));
            line.push (fastTanh (in));
            float s = y;
            s += rng.next() * pickEnv * pickLvl * vel;
            pickEnv *= pickDec;
            if (bodyLvl > 0.0f)
                s += bodyLvl * (body1.tick (y) * 1.2f + body2.tick (y) * 0.9f + body3.tick (y) * 0.6f);
            out[i] = s * 1.75f;
            peak = std::max (peak, std::abs (s));
        }
        t += (float) n / sr;
        if (t > 0.05f && peak < 2.0e-5f && bow == 0.0f)
            finished = true;
    }
};

//==============================================================================
struct EPianoEngine
{
    float phC = 0.0f, phM = 0.0f, phB = 0.0f, t = 0.0f, sr = 48000.0f, vel = 0.8f, freq0 = 440.0f;
    float tremPan = 0.0f;
    DcBlock dc;
    Svf thumpFlt;
    Rng rng;
    float level = 1.0f;
    bool finished = false;

    bool done() const { return finished; }

    void start (const StartInfo& s, float freq)
    {
        sr = s.sr; vel = s.vel; t = 0.0f; freq0 = freq; finished = false;
        rng.seed (s.seed);
        phC = phM = phB = 0.0f;
        dc.reset(); thumpFlt.reset();
        level = 1.0f;
    }

    void render (float* out, int n, float freq, const float* p, double timeSec)
    {
        const float type = p[tpE1];                  // 0 tine .. 1 reed
        const float hard = p[tpE2] * (0.3f + vel);
        const float bellLvl = p[tpE3] * (0.3f + 0.9f * vel);
        const float T = expMap (p[tpE4], 0.6f, 9.0f) * std::pow (261.0f / std::max (freq0, 40.0f), 0.45f);
        const float bark = p[tpE5] * (0.4f + vel);
        const float attackLvl = p[tpE6] * vel * 0.5f;
        const float bellRatio = lerp (14.0f, 7.3f, type);
        const float incC = freq / sr, incM = freq / sr, incB = std::min (0.45f, freq * bellRatio / sr);
        thumpFlt.set (std::min (freq0 * 6.0f, 5000.0f), 0.4f, sr);
        const float dt = 1.0f / sr;
        float peak = 0.0f;

        // tremolo (tine: stereo autopan, reed: amplitude)
        const float tr = expMap (p[tpE8], 0.5f, 9.0f);
        const float lfo = fastSin01 ((float) std::fmod (timeSec * tr, 1.0));
        tremPan = lfo * p[tpE7] * (1.0f - type);
        const float tremAmp = 1.0f - p[tpE7] * type * 0.5f * (1.0f + lfo);

        for (int i = 0; i < n; ++i)
        {
            const float e1 = std::exp (-t / (T * 0.25f)), e2 = std::exp (-t / T);
            const float env = 0.55f * e1 + 0.45f * e2;
            const float idx = (0.2f + 2.2f * hard) * std::exp (-t / 0.35f) * (1.0f - type)
                            + (0.9f + 1.5f * hard) * (0.4f + 0.6f * std::exp (-t / 0.8f)) * type;
            const float m = fastSin01 (phM);
            float x = fastSin01 (phC + idx * m * 0.159f) * env;
            x += bellLvl * fastSin01 (phB) * std::exp (-t / lerp (0.12f, 0.05f, type)) * 0.5f;
            // pickup non-linearity
            const float tine = x + bark * 0.7f * x * x;
            const float reed = fastTanh (x * (1.0f + bark * 3.0f));
            x = dc.tick (lerp (tine, reed, type));
            x += thumpFlt.bp (rng.next()) * attackLvl * std::exp (-t / 0.012f);
            phC += incC; if (phC >= 1.0f) phC -= 1.0f;
            phM += incM; if (phM >= 1.0f) phM -= 1.0f;
            phB += incB; if (phB >= 1.0f) phB -= 1.0f;
            t += dt;
            const float o = x * tremAmp * 0.4f;
            out[i] = o;
            peak = std::max (peak, std::abs (o));
        }
        if (t > 0.1f && peak < 2.0e-5f) finished = true;
    }
};

//==============================================================================
struct PianoEngine
{
    static constexpr int kMaxPartials = 20;
    static constexpr int kOsc = kMaxPartials * 2;
    alignas (16) float re[kOsc] {}, im[kOsc] {}, cs[kOsc] {}, sn[kOsc] {}, amp[kOsc] {};
    int nOsc = 0;
    float slowest = 1.0f, level = 1.0f, t = 0.0f, sr = 48000.0f, vel = 0.8f;
    Biquad hammerFlt;
    float hammerEnv = 0.0f, thumpPh = 0.0f, thumpEnv = 0.0f;
    Rng rng;
    bool finished = false;

    bool done() const { return finished; }

    void start (const StartInfo& s, const float* p, float freq)
    {
        sr = s.sr; vel = s.vel; t = 0.0f; finished = false;
        rng.seed (s.seed);
        const float note = (float) s.note;
        const float B = 0.00012f * std::exp2 ((note - 48.0f) / 16.0f) * (0.2f + 1.6f * p[tpE4]);
        const float velTone = 0.3f + 0.7f * p[tpE7];
        const float hard = clamp01 (p[tpE1] * (1.0f - velTone + velTone * vel * 1.25f));
        const float pw = 2.4f - 1.8f * hard;               // spectral tilt
        const float t60Base = (0.3f + 1.6f * p[tpE2]) * 14.0f * std::exp2 (-(note - 21.0f) / 22.0f);
        const float cents = p[tpE3] * 3.0f + 22.0f * std::pow (p[tpE3], 4.0f);
        const float dA = std::exp2 (cents / 1200.0f), dB = std::exp2 (-cents / 1200.0f);

        nOsc = 0;
        float sumA = 0.0f;
        slowest = 0.0f;
        for (int k = 1; k <= kMaxPartials; ++k)
        {
            const float fk = freq * (float) k * std::sqrt (1.0f + B * (float) (k * k));
            if (fk * dA > sr * 0.45f) break;
            float a = std::pow ((float) k, -pw) * (0.08f + std::abs (std::sin (kPi * (float) k * 0.118f)));
            if (k == 1) a *= 1.2f;
            const float t60 = t60Base / (1.0f + 0.12f * (float) (k - 1)) / (1.0f + fk / 4000.0f);
            for (int str = 0; str < 2; ++str)
            {
                const float f = fk * (str == 0 ? dA : dB);
                const float tt = str == 0 ? t60 : t60 * 2.6f;
                const float r = std::exp (-6.9078f / (std::max (0.05f, tt) * sr));
                const float w = kTwoPi * f / sr;
                cs[nOsc] = r * std::cos (w);
                sn[nOsc] = r * std::sin (w);
                re[nOsc] = 1.0f; im[nOsc] = 0.0f;
                amp[nOsc] = a * (str == 0 ? 0.62f : 0.38f);
                if (k == 1) slowest = std::max (slowest, r);
                ++nOsc;
            }
            sumA += a;
        }
        const float norm = 0.82f / std::max (0.3f, sumA);
        for (int i = 0; i < nOsc; ++i) amp[i] *= norm;
        level = 1.0f;

        hammerFlt.bandpass (std::min (freq * 5.0f, 7000.0f), 0.9f, sr);
        hammerFlt.reset();
        hammerEnv = p[tpE5] * (0.2f + vel) * 0.35f;
        thumpEnv = p[tpE6] * vel * 0.25f;
        thumpPh = 0.0f;
    }

    void render (float* out, int n)
    {
        const float hDec = std::exp (-1.0f / (0.012f * sr));
        const float thDec = std::exp (-1.0f / (0.07f * sr));
        const float thInc = 72.0f / sr;
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float s = 0.0f;
            for (int k = 0; k < nOsc; ++k)
            {
                const float r0 = re[k], i0 = im[k];
                re[k] = r0 * cs[k] - i0 * sn[k];
                im[k] = r0 * sn[k] + i0 * cs[k];
                s += im[k] * amp[k];
            }
            if (hammerEnv > 1.0e-5f)
            {
                s += hammerFlt.tick (rng.next()) * hammerEnv;
                hammerEnv *= hDec;
            }
            if (thumpEnv > 1.0e-5f)
            {
                thumpPh += thInc; if (thumpPh >= 1.0f) thumpPh -= 1.0f;
                s += fastSin01 (thumpPh) * thumpEnv;
                thumpEnv *= thDec;
            }
            out[i] = s;
            peak = std::max (peak, std::abs (s));
            level *= slowest;
        }
        t += (float) n / sr;
        if (t > 0.1f && (level < 1.0e-4f || peak < 1.0e-5f))
            finished = true;
    }
};
} // namespace rc
