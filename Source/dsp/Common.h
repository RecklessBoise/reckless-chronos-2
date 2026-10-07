#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace rc
{
constexpr float kPi    = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float clamp01 (float x) { return std::min (1.0f, std::max (0.0f, x)); }
inline float mtof (float n)    { return 440.0f * std::exp2 ((n - 69.0f) / 12.0f); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }

inline float fastTanh (float x)
{
    x = std::clamp (x, -3.0f, 3.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

// Maps 0..1 to an exponential time / frequency range
inline float expMap (float x, float lo, float hi) { return lo * std::pow (hi / lo, clamp01 (x)); }

// Picks an entry from a list by a 0..1 control
template <typename T, size_t N>
inline T pick (const T (&arr)[N], float x)
{
    const int i = std::clamp ((int) (x * (float) N), 0, (int) N - 1);
    return arr[i];
}
inline int pickIndex (float x, int n) { return std::clamp ((int) (x * (float) n), 0, n - 1); }

struct Rng
{
    uint32_t s = 0x9E3779B9u;
    void seed (uint32_t v) { s = v ? v : 1u; }
    inline uint32_t nextU() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float next() { return (float) (nextU() >> 8) * (1.0f / 8388608.0f) - 1.0f; } // -1..1
    inline float next01() { return (float) (nextU() >> 8) * (1.0f / 16777216.0f); }
};

inline float polyBlep (float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// Global sine lookup (defined in Tables.cpp)
float fastSin01 (float phase01); // phase in cycles (any value), returns sin(2*pi*phase)

//==============================================================================
struct Adsr
{
    enum Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Idle;
    float v = 0.0f, sus = 1.0f, sr = 48000.0f;
    float aInc = 1.0f, dCoef = 0.0f, rCoef = 0.0f;

    void setSampleRate (float s) { sr = s; }
    void set (float a, float d, float s, float r)
    {
        aInc  = 1.0f / std::max (1.0f, a * sr);
        dCoef = std::exp (-4.6f / std::max (1.0f, d * sr));
        rCoef = std::exp (-4.6f / std::max (1.0f, r * sr));
        sus   = s;
    }
    void noteOn (bool retriggerFromZero)
    {
        if (retriggerFromZero) v = 0.0f;
        stage = Attack;
    }
    void noteOff() { if (stage != Idle) stage = Release; }
    void reset()   { stage = Idle; v = 0.0f; }
    bool isActive() const { return stage != Idle; }

    inline float next()
    {
        switch (stage)
        {
            case Attack:
                v += aInc;
                if (v >= 1.0f) { v = 1.0f; stage = Decay; }
                break;
            case Decay:
                v = sus + (v - sus) * dCoef;
                if (std::abs (v - sus) < 1.0e-4f) { v = sus; stage = Sustain; }
                break;
            case Sustain: v = sus; break;
            case Release:
                v *= rCoef;
                if (v < 1.0e-5f) { v = 0.0f; stage = Idle; }
                break;
            case Idle: default: v = 0.0f; break;
        }
        return v;
    }
    // Advance n samples at once (control rate), returns final value
    inline float advance (int n)
    {
        float out = v;
        for (int i = 0; i < n; ++i) out = next();
        return out;
    }
};

//==============================================================================
struct Lfo
{
    float phase = 0.0f, value = 0.0f, held = 0.0f;
    Rng rng;
    float compute (int wave, float ph)
    {
        switch (wave)
        {
            case 0: return fastSin01 (ph);
            case 1: return 1.0f - 4.0f * std::abs (ph - 0.5f);
            case 2: return 2.0f * ph - 1.0f;
            case 3: return ph < 0.5f ? 1.0f : -1.0f;
            default: return held;
        }
    }
    // advance by dt seconds
    float tick (int wave, float rate, float dt)
    {
        phase += rate * dt;
        if (phase >= 1.0f)
        {
            phase -= std::floor (phase);
            held = rng.next();
        }
        value = compute (wave, phase);
        return value;
    }
};

//==============================================================================
// Topology-preserving state variable filter
struct Svf
{
    float g = 0, k = 2, a1 = 1, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    void reset() { ic1 = ic2 = 0.0f; }
    void set (float fc, float res01, float sr)
    {
        fc = std::clamp (fc, 10.0f, sr * 0.45f);
        g  = std::tan (kPi * fc / sr);
        k  = 2.0f - 1.96f * clamp01 (res01);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    inline void tick (float v0, float& lp, float& bp, float& hp)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
    inline float lp (float x) { float l, b, h; tick (x, l, b, h); return l; }
    inline float bp (float x) { float l, b, h; tick (x, l, b, h); return b; }
    inline float hp (float x) { float l, b, h; tick (x, l, b, h); return h; }
};

// Zero-delay-feedback 4-pole ladder with input saturation
struct Ladder
{
    float s[4] { 0, 0, 0, 0 };
    float G = 0, k = 0;
    void reset() { s[0] = s[1] = s[2] = s[3] = 0.0f; }
    void set (float fc, float res01, float sr)
    {
        fc = std::clamp (fc, 10.0f, sr * 0.45f);
        const float g = std::tan (kPi * fc / sr);
        G = g / (1.0f + g);
        k = 4.0f * clamp01 (res01) * 0.98f;
    }
    inline float tick (float x, float drive)
    {
        const float G2 = G * G, G4 = G2 * G2;
        const float beta = 1.0f - G;
        // zero-delay feedback solve (linear), then saturate the input stage
        const float S = (G2 * G * s[0] + G2 * s[1] + G * s[2] + s[3]) * beta;
        const float yEst = (G4 * x + S) / (1.0f + k * G4);
        float u = fastTanh ((x - k * yEst) * drive) / drive;
        for (int i = 0; i < 4; ++i)
        {
            const float v = (u - s[i]) * G;
            const float y = v + s[i];
            s[i] = y + v;
            u = y;
        }
        return u * (1.0f + k * 0.35f);
    }
};

// One-pole lowpass/highpass helpers
struct OnePole
{
    float z = 0.0f, a = 0.0f;
    void setLP (float fc, float sr) { a = std::exp (-kTwoPi * std::clamp (fc, 1.0f, sr * 0.49f) / sr); }
    inline float lp (float x) { z = x + a * (z - x); return z; }
    inline float hp (float x) { return x - lp (x); }
    void reset() { z = 0.0f; }
};

// RBJ biquad
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0.0f; }
    inline float tick (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void setNorm (float B0, float B1, float B2, float A0, float A1, float A2)
    {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    void lowShelf (float fc, float db, float sr)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = kTwoPi * fc / sr;
        const float cs = std::cos (w), sn = std::sin (w), al = sn / 2.0f * std::sqrt (2.0f), sa = 2.0f * std::sqrt (A) * al;
        setNorm (A * ((A + 1) - (A - 1) * cs + sa), 2 * A * ((A - 1) - (A + 1) * cs), A * ((A + 1) - (A - 1) * cs - sa),
                 (A + 1) + (A - 1) * cs + sa, -2 * ((A - 1) + (A + 1) * cs), (A + 1) + (A - 1) * cs - sa);
    }
    void highShelf (float fc, float db, float sr)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = kTwoPi * fc / sr;
        const float cs = std::cos (w), sn = std::sin (w), al = sn / 2.0f * std::sqrt (2.0f), sa = 2.0f * std::sqrt (A) * al;
        setNorm (A * ((A + 1) + (A - 1) * cs + sa), -2 * A * ((A - 1) + (A + 1) * cs), A * ((A + 1) + (A - 1) * cs - sa),
                 (A + 1) - (A - 1) * cs + sa, 2 * ((A - 1) - (A + 1) * cs), (A + 1) - (A - 1) * cs - sa);
    }
    void peak (float fc, float q, float db, float sr)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = kTwoPi * fc / sr;
        const float al = std::sin (w) / (2.0f * q), cs = std::cos (w);
        setNorm (1 + al * A, -2 * cs, 1 - al * A, 1 + al / A, -2 * cs, 1 - al / A);
    }
    void bandpass (float fc, float q, float sr)
    {
        const float w = kTwoPi * std::min (fc, sr * 0.45f) / sr, al = std::sin (w) / (2.0f * q), cs = std::cos (w);
        setNorm (al, 0, -al, 1 + al, -2 * cs, 1 - al);
    }
    void highpass (float fc, float q, float sr)
    {
        const float w = kTwoPi * std::min (fc, sr * 0.45f) / sr, al = std::sin (w) / (2.0f * q), cs = std::cos (w);
        setNorm ((1 + cs) / 2, -(1 + cs), (1 + cs) / 2, 1 + al, -2 * cs, 1 - al);
    }
    void lowpass (float fc, float q, float sr)
    {
        const float w = kTwoPi * std::min (fc, sr * 0.45f) / sr, al = std::sin (w) / (2.0f * q), cs = std::cos (w);
        setNorm ((1 - cs) / 2, 1 - cs, (1 - cs) / 2, 1 + al, -2 * cs, 1 - al);
    }
};

// Simple fractional delay line
struct DelayLine
{
    std::vector<float> buf;
    int w = 0, mask = 0;
    void init (int minSize)
    {
        int n = 1;
        while (n < minSize) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    inline float read (float delaySamples) const
    {
        float rp = (float) w - delaySamples - 1.0f;
        while (rp < 0) rp += (float) (mask + 1);
        const int i0 = (int) rp;
        const float f = rp - (float) i0;
        const float a = buf[(size_t) (i0 & mask)], b = buf[(size_t) ((i0 + 1) & mask)];
        return a + (b - a) * f;
    }
};

struct DcBlock
{
    float x1 = 0, y1 = 0;
    inline float tick (float x) { const float y = x - x1 + 0.995f * y1; x1 = x; y1 = y; return y; }
    void reset() { x1 = y1 = 0.0f; }
};
} // namespace rc
