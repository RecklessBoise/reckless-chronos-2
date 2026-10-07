#pragma once

#include "Voice.h"
#include "Effects.h"
#include <array>
#include <atomic>

namespace rc
{
// One sound layer: a voice pool + 2 insert effects. Program mode uses timbre 1,
// Combi mode layers/splits up to 4 timbres.
class Timbre
{
public:
    static constexpr int kMaxVoices = 32;

    void prepare (float sampleRate, int maxBlock);
    void reset();

    // Copies the raw parameter values and applies realtime offsets
    void updateParams (const std::array<std::atomic<float>*, tpCount>& raw, const float* rtOffsets);

    void noteOn (int note, float vel);
    void noteOff (int note);
    void setSustain (bool down);
    void allNotesOff (bool hard);

    // Renders into the timbre's internal buffers (post insert FX)
    void render (int n, const RenderCtx& baseCtx);

    bool acceptsNote (int note, int vel127) const;
    bool isOn() const { return params[tpOn] > 0.5f; }
    bool hasActiveVoices() const;
    int activeVoiceCount() const;

    const float* getL() const { return bufL.data(); }
    const float* getR() const { return bufR.data(); }
    const float* params_() const { return params; }

    float params[tpCount] {};

private:
    Voice* findVoice();
    void startVoice (int note, float vel, bool legatoAllowed);

    float sr = 48000.0f;
    std::vector<Voice> voices = std::vector<Voice> (kMaxVoices); // heap: ~17 KB per voice
    InsertFx ifx1, ifx2, ensemble;
    std::vector<float> bufL, bufR;
    uint64_t ageCounter = 0;
    uint32_t seedCounter = 1;
    int curEngine = -1;
    bool sustainDown = false;
    float lastNote = -1.0f;

    // mono note stack
    int stack[16] {};
    float stackVel[16] {};
    int stackSize = 0;

    RenderCtx ctx;
};
} // namespace rc
