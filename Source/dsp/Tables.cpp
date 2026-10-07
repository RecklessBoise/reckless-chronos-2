#include "Tables.h"
#include <juce_dsp/juce_dsp.h>

namespace rc
{
namespace
{
struct SineTable
{
    static constexpr int N = 4096;
    float t[N + 1];
    SineTable()
    {
        for (int i = 0; i <= N; ++i)
            t[i] = std::sin (kTwoPi * (float) i / (float) N);
    }
};

const SineTable& sineTable()
{
    static const SineTable s;
    return s;
}

float formant (float freqHz, float centre, float width)
{
    const float d = (freqHz - centre) / width;
    return std::exp (-d * d);
}

// Harmonic amplitude spectrum for each wave (h = 1..1024)
float spectrum (int wave, int h, Rng& rng)
{
    const float fh = (float) h;
    const float f0 = 220.0f, fr = f0 * fh; // nominal pitch used for formant shaping
    const bool odd = (h & 1) != 0;
    switch (wave)
    {
        case 0:  return 1.0f / fh;                                           // Saw
        case 1:  return odd ? 1.0f / fh : 0.0f;                              // Square
        case 2:  return odd ? ((((h - 1) / 2) & 1) ? -1.0f : 1.0f) / (fh * fh) : 0.0f; // Triangle
        case 3:  return h == 1 ? 1.0f : 0.0f;                                // Sine
        case 4:                                                              // Organ
        {
            switch (h) { case 1: return 1.0f; case 2: return 0.8f; case 3: return 0.6f; case 4: return 0.5f;
                         case 6: return 0.3f; case 8: return 0.25f; default: return 0.0f; }
        }
        case 5:  return (1.0f / fh) * (1.0f + 1.2f * formant (fr, 1200.0f, 700.0f)) * std::exp (-fh / 60.0f); // Strings
        case 6:  return std::pow (fh, -0.6f) * std::exp (-fh / 25.0f) * (1.0f + formant (fr, 1500.0f, 900.0f)); // Brass
        case 7:  return (formant (fr, 800, 180) + 0.7f * formant (fr, 1150, 200) + 0.25f * formant (fr, 2900, 300)) / std::sqrt (fh); // Aah
        case 8:  return (formant (fr, 350, 120) + 0.5f * formant (fr, 600, 150) + 0.1f * formant (fr, 2400, 300)) / std::sqrt (fh); // Ooh
        case 9:  return (formant (fr, 270, 100) + 0.6f * formant (fr, 2300, 250) + 0.4f * formant (fr, 3000, 300)) / std::sqrt (fh); // Eee
        case 10: return (odd ? 1.0f : 0.08f) / fh * std::exp (-fh / 40.0f);  // Clarinet
        case 11: return std::pow (fh, -0.5f) * (0.2f + formant (fr, 1300, 400)) * std::exp (-fh / 50.0f); // Oboe
        case 12: { const float a[] = { 1.0f, 0.35f, 0.12f, 0.05f, 0.02f }; return h <= 5 ? a[h - 1] : 0.0f; } // Flute
        case 13: { switch (h) { case 1: return 1.0f; case 3: return 0.6f; case 5: return 0.35f; case 8: return 0.4f;
                                case 13: return 0.25f; case 21: return 0.15f; case 34: return 0.08f; default: return 0.0f; } } // Glass
        case 14: return std::abs (rng.next()) * std::pow (fh, -0.5f) * (h < 128 ? 1.0f : 0.0f); // Digital 1
        case 15: return (h % 4 == 1 || h % 7 == 0) ? (0.5f + 0.5f * std::abs (rng.next())) / std::sqrt (fh) : 0.0f; // Digital 2
        case 16: return std::sin (kPi * fh * 0.25f) / fh;                    // Pulse 25
        case 17: return std::sin (kPi * fh * 0.10f) / fh;                    // Pulse 10
        case 18: { const float a[] = { 1.0f, 0.5f, 0.33f, 0.12f, 0.05f }; return h <= 5 ? a[h - 1] : 0.0f; } // Bass
        case 19: return (odd ? 1.0f : 0.5f) / fh * (1.0f + 0.8f * formant (fr, 2000, 800)) * (h < 60 ? 1.0f : 0.0f); // Reed
        case 20: return 1.0f / (fh * fh) * (1.0f + 0.5f * formant (fr, 900, 300)); // Harp
        case 21: return (1.0f / fh) * (0.3f + formant (fr, 300, 120) + 0.8f * formant (fr, 1200, 350)) * std::exp (-fh / 70.0f); // Cello
        case 22: return (formant (fr, 750, 200) + 0.8f * formant (fr, 1250, 250) + 0.6f * formant (fr, 2600, 400) + 0.3f * formant (fr, 3400, 300)) / std::pow (fh, 0.3f); // Bright vox
        case 23: return (1.0f / fh) * (1.0f + 3.0f * formant (fh, 5.0f, 1.8f)) * std::exp (-fh / 120.0f); // Sync
        default: return 0.0f;
    }
}
} // namespace

float fastSin01 (float phase)
{
    const auto& s = sineTable();
    phase -= std::floor (phase);
    const float p = phase * (float) SineTable::N;
    const int i = (int) p;
    const float f = p - (float) i;
    return s.t[i] + (s.t[i + 1] - s.t[i]) * f;
}

const char* WaveTables::waveName (int w)
{
    static const char* names[kWaves] = { "Saw", "Square", "Triangle", "Sine", "Organ", "Strings", "Brass", "Choir Aah",
                                         "Choir Ooh", "Vox Eee", "Clarinet", "Oboe", "Flute", "Glass", "Digital 1",
                                         "Digital 2", "Pulse 25", "Pulse 10", "Bass", "Reed", "Harp", "Cello",
                                         "Bright Vox", "Sync" };
    return names[std::clamp (w, 0, kWaves - 1)];
}

const WaveTables& WaveTables::get()
{
    static const WaveTables tables = []
    {
        WaveTables wt;
        wt.data.assign ((size_t) kWaves * kLevels * kSize, 0.0f);
        juce::dsp::FFT fft (11); // 2048
        std::vector<float> buf ((size_t) kSize * 2);

        for (int w = 0; w < kWaves; ++w)
        {
            std::array<float, 1025> amp {};
            Rng rng;
            rng.seed (1234u + (uint32_t) w * 7919u);
            for (int h = 1; h <= 1024; ++h)
                amp[(size_t) h] = spectrum (w, h, rng);

            float norm = 0.0f;
            for (int L = 0; L < kLevels; ++L)
            {
                const int maxH = 1024 >> L;
                std::fill (buf.begin(), buf.end(), 0.0f);
                for (int h = 1; h < maxH && h < kSize / 2; ++h)
                {
                    buf[(size_t) (2 * h)]     = 0.0f;
                    buf[(size_t) (2 * h + 1)] = -amp[(size_t) h];
                }
                fft.performRealOnlyInverseTransform (buf.data());
                float* dst = wt.data.data() + ((size_t) w * kLevels + (size_t) L) * kSize;
                for (int i = 0; i < kSize; ++i)
                    dst[i] = buf[(size_t) i];
                if (L == 0)
                {
                    for (int i = 0; i < kSize; ++i)
                        norm = std::max (norm, std::abs (dst[i]));
                    norm = norm > 0.0f ? 0.9f / norm : 1.0f;
                }
                for (int i = 0; i < kSize; ++i)
                    dst[i] *= norm;
            }
        }
        return wt;
    }();
    return tables;
}

void initTables()
{
    (void) sineTable();
    (void) WaveTables::get();
}
} // namespace rc
