#pragma once

#include "Engines.h"

namespace rc
{
struct RenderCtx
{
    const float* p = nullptr;   // effective timbre params
    float sr = 48000.0f;
    float bendSemis = 0.0f;
    float modWheel = 0.0f;      // 0..1
    float aftertouch = 0.0f;    // 0..1
    double timeSec = 0.0;       // block start time
};

class Voice
{
public:
    void prepare (float sampleRate);
    void start (int note, float vel, int engine, const RenderCtx& ctx, bool firstNote,
                float glideFrom, bool legato, uint32_t seed);
    void release();
    void kill();                // fast fade-out (voice stealing / engine change)
    void render (float* L, float* R, int n, const RenderCtx& ctx);

    bool isActive() const { return active; }
    bool isGateOn() const { return gate; }

    int note = -1;
    uint64_t age = 0;
    bool sustained = false;     // released while sustain pedal down

private:
    void renderBlock (float* L, float* R, int n, const RenderCtx& ctx);

    bool active = false, gate = false;
    int engine = engAnalog;
    float sr = 48000.0f, vel = 0.8f;
    float curNote = 60.0f, targetNote = 60.0f;
    float voiceTime = 0.0f;
    float killGain = 1.0f, killStep = 0.0f;
    bool killing = false;

    Adsr ampEnv, fltEnv;
    Lfo lfo;
    Svf svf1, svf2;
    Ladder ladder;

    AnalogEngine analog;
    TwinEngine twin;
    PolyEngine poly;
    FmEngine fm;
    WaveEngine wave;
    DrumEngine drums;
    OrganEngine organ;
    PluckEngine pluck;
    EPianoEngine epiano;
    PianoEngine piano;

    float tmp[64] {};
};
} // namespace rc
