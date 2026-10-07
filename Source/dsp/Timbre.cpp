#include "Timbre.h"

namespace rc
{
void Timbre::prepare (float sampleRate, int maxBlock)
{
    sr = sampleRate;
    for (auto& v : voices) v.prepare (sr);
    ifx1.prepare (sr); ifx2.prepare (sr); ensemble.prepare (sr);
    bufL.assign ((size_t) std::max (maxBlock, 64), 0.0f);
    bufR.assign ((size_t) std::max (maxBlock, 64), 0.0f);
    reset();
}

void Timbre::reset()
{
    for (auto& v : voices) v.kill();
    ifx1.reset(); ifx2.reset(); ensemble.reset();
    stackSize = 0;
    sustainDown = false;
    lastNote = -1.0f;
}

void Timbre::updateParams (const std::array<std::atomic<float>*, tpCount>& raw, const float* rt)
{
    for (int i = 0; i < tpCount; ++i)
        params[i] = raw[(size_t) i]->load (std::memory_order_relaxed);

    // realtime control offsets (knobs on the front panel)
    params[tpCutoff] = std::clamp (params[tpCutoff] * std::exp2 (rt[0] * 4.0f), 20.0f, 20000.0f);
    params[tpReso]   = clamp01 (params[tpReso] + rt[1] * 0.5f);
    params[tpFltEnv] = std::clamp (params[tpFltEnv] + rt[2], -1.0f, 1.0f);
    const float aMul = std::exp2 (rt[3] * 4.0f), dMul = std::exp2 (rt[4] * 4.0f), rMul = std::exp2 (rt[5] * 4.0f);
    params[tpAA] = std::clamp (params[tpAA] * aMul, 0.001f, 20.0f);
    params[tpFA] = std::clamp (params[tpFA] * aMul, 0.001f, 20.0f);
    params[tpAD] = std::clamp (params[tpAD] * dMul, 0.001f, 30.0f);
    params[tpFD] = std::clamp (params[tpFD] * dMul, 0.001f, 30.0f);
    params[tpAR] = std::clamp (params[tpAR] * rMul, 0.001f, 30.0f);
    params[tpFR] = std::clamp (params[tpFR] * rMul, 0.001f, 30.0f);
    params[tpSend2] = clamp01 (params[tpSend2] + rt[6]);
    params[tpSend1] = clamp01 (params[tpSend1] + rt[7]);

    const int eng = (int) params[tpEngine];
    if (eng != curEngine)
    {
        if (curEngine >= 0)
            for (auto& v : voices) v.kill();
        curEngine = eng;
    }
}

bool Timbre::acceptsNote (int note, int vel127) const
{
    return isOn() && note >= (int) params[tpKeyLo] && note <= (int) params[tpKeyHi]
        && vel127 >= (int) params[tpVelLo] && vel127 <= (int) params[tpVelHi];
}

bool Timbre::hasActiveVoices() const
{
    for (auto& v : voices) if (v.isActive()) return true;
    return false;
}

int Timbre::activeVoiceCount() const
{
    int c = 0;
    for (auto& v : voices) c += v.isActive() ? 1 : 0;
    return c;
}

Voice* Timbre::findVoice()
{
    Voice* best = nullptr;
    // free voice
    for (auto& v : voices) if (! v.isActive()) return &v;
    // oldest released voice
    for (auto& v : voices)
        if (! v.isGateOn() && (best == nullptr || v.age < best->age)) best = &v;
    if (best) return best;
    // oldest voice
    for (auto& v : voices)
        if (best == nullptr || v.age < best->age) best = &v;
    return best;
}

void Timbre::startVoice (int note, float vel, bool legatoAllowed)
{
    const int mode = (int) params[tpVoiceMode];
    ctx.p = params;
    ctx.sr = sr;

    bool anyGate = false;
    for (auto& v : voices) if (v.isGateOn()) { anyGate = true; break; }

    if (mode == 0)
    {
        // drums: choke open hat with closed/pedal hat
        if (curEngine == engDrums && (note == 42 || note == 44))
            for (auto& v : voices) if (v.isActive() && v.note == 46) v.kill();
        // retrigger same note: release previous instance
        for (auto& v : voices)
            if (v.isActive() && v.note == note && curEngine != engDrums) v.kill();

        Voice* v = findVoice();
        const float glideFrom = lastNote;
        v->start (note, vel, curEngine, ctx, ! anyGate, glideFrom, false, seedCounter++ * 2654435761u);
        v->age = ++ageCounter;
    }
    else
    {
        // mono / legato: single voice 0
        Voice& v = voices[0];
        for (size_t i = 1; i < voices.size(); ++i) if (voices[i].isActive()) voices[i].kill();
        const bool legato = mode == 2 && legatoAllowed && v.isActive() && v.isGateOn();
        v.start (note, vel, curEngine, ctx, ! anyGate, lastNote, legato, seedCounter++ * 2654435761u);
        v.age = ++ageCounter;
    }
    lastNote = (float) note;
}

void Timbre::noteOn (int note, float vel)
{
    if ((int) params[tpVoiceMode] != 0)
    {
        // push to stack
        for (int i = 0; i < stackSize; ++i)
            if (stack[i] == note)
            {
                for (int j = i; j < stackSize - 1; ++j) { stack[j] = stack[j + 1]; stackVel[j] = stackVel[j + 1]; }
                --stackSize;
                break;
            }
        if (stackSize < 16) { stack[stackSize] = note; stackVel[stackSize] = vel; ++stackSize; }
    }
    startVoice (note, vel, true);
}

void Timbre::noteOff (int note)
{
    const int mode = (int) params[tpVoiceMode];
    if (mode != 0)
    {
        for (int i = 0; i < stackSize; ++i)
            if (stack[i] == note)
            {
                for (int j = i; j < stackSize - 1; ++j) { stack[j] = stack[j + 1]; stackVel[j] = stackVel[j + 1]; }
                --stackSize;
                break;
            }
        Voice& v = voices[0];
        if (v.isActive() && v.note == note)
        {
            if (stackSize > 0)
            {
                // return to previous held note
                ctx.p = params;
                v.start (stack[stackSize - 1], stackVel[stackSize - 1], curEngine, ctx, false, (float) note, true, seedCounter++);
                lastNote = (float) stack[stackSize - 1];
            }
            else if (sustainDown) v.sustained = true;
            else v.release();
        }
        return;
    }

    for (auto& v : voices)
        if (v.isActive() && v.isGateOn() && v.note == note)
        {
            if (sustainDown) v.sustained = true;
            else v.release();
        }
}

void Timbre::setSustain (bool down)
{
    sustainDown = down;
    if (! down)
        for (auto& v : voices)
            if (v.isActive() && v.sustained)
            {
                v.sustained = false;
                v.release();
            }
}

void Timbre::allNotesOff (bool hard)
{
    stackSize = 0;
    if (hard) lastNote = -1.0f;
    sustainDown = false;
    for (auto& v : voices)
    {
        v.sustained = false;
        if (hard) v.kill();
        else if (v.isActive()) v.release();
    }
}

void Timbre::render (int n, const RenderCtx& baseCtx)
{
    std::fill (bufL.begin(), bufL.begin() + n, 0.0f);
    std::fill (bufR.begin(), bufR.begin() + n, 0.0f);

    ctx = baseCtx;
    ctx.p = params;
    ctx.sr = sr;
    ctx.bendSemis = baseCtx.bendSemis * params[tpPbRange];

    for (auto& v : voices)
        if (v.isActive())
            v.render (bufL.data(), bufR.data(), n, ctx);

    if (curEngine == engPoly && params[tpE8] > 0.01f)
        ensemble.process (bufL.data(), bufR.data(), n, fxEnsemble, 0.45f, 0.6f, params[tpE8] * 0.7f, 0.0f);
    ifx1.process (bufL.data(), bufR.data(), n, (int) params[tpIfx1Type], params[tpIfx1A], params[tpIfx1B],
                  params[tpIfx1Mix], baseCtx.modWheel);
    ifx2.process (bufL.data(), bufR.data(), n, (int) params[tpIfx2Type], params[tpIfx2A], params[tpIfx2B],
                  params[tpIfx2Mix], baseCtx.modWheel);
}
} // namespace rc
