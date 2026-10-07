#pragma once

#include "Common.h"
#include <array>

namespace rc
{
// Band-limited single-cycle wavetables used by the Wave ROM engine.
struct WaveTables
{
    static constexpr int kSize   = 2048;
    static constexpr int kLevels = 10;     // mip levels: level L holds up to 1024 >> L harmonics
    static constexpr int kWaves  = 24;

    // [wave][level][sample]
    std::vector<float> data;

    static const WaveTables& get();
    static const char* waveName (int w);

    const float* table (int wave, int level) const
    {
        return data.data() + ((size_t) wave * kLevels + (size_t) level) * kSize;
    }

    // choose mip level for a given phase increment (cycles / sample)
    static int levelFor (float inc)
    {
        const float maxH = 0.5f / std::max (inc, 1.0e-6f);
        int level = 0;
        while (level < kLevels - 1 && (float) (1024 >> level) > maxH) ++level;
        return level;
    }

    static inline float read (const float* t, float phase01)
    {
        const float p = phase01 * (float) kSize;
        const int i = (int) p;
        const float f = p - (float) i;
        const float a = t[i & (kSize - 1)], b = t[(i + 1) & (kSize - 1)];
        return a + (b - a) * f;
    }
};

void initTables(); // force construction off the audio thread
} // namespace rc
