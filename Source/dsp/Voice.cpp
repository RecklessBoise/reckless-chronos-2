#include "Voice.h"

namespace rc
{
void Voice::prepare (float sampleRate)
{
    sr = sampleRate;
    ampEnv.setSampleRate (sr);
    fltEnv.setSampleRate (sr);
    pluck.prepare (sr);
    active = false;
}

void Voice::start (int n, float v, int eng, const RenderCtx& ctx, bool firstNote, float glideFrom, bool legato, uint32_t seed)
{
    const float* p = ctx.p;
    const bool wasActive = active && !killing;
    note = n;
    vel = v;
    engine = eng;
    targetNote = (float) n;
    curNote = (glideFrom >= 0.0f && p[tpGlide] > 0.0005f) ? glideFrom : (float) n;
    gate = true;
    sustained = false;
    active = true;
    killing = false;
    killGain = 1.0f;

    ampEnv.set (p[tpAA], p[tpAD], p[tpAS], p[tpAR]);
    fltEnv.set (p[tpFA], p[tpFD], p[tpFS], p[tpFR]);

    if (legato && wasActive)
        return; // legato: keep envelopes and engine state running

    ampEnv.noteOn (! wasActive);
    fltEnv.noteOn (true);
    voiceTime = 0.0f;
    lfo.phase = 0.0f;
    lfo.rng.seed (seed * 31u + 7u);

    svf1.reset(); svf2.reset(); ladder.reset();

    StartInfo si;
    si.note = n; si.vel = v; si.sr = sr; si.firstNote = firstNote; si.seed = seed;
    const float freq = mtof ((float) n + p[tpTranspose] + p[tpFine] * 0.01f);

    switch (engine)
    {
        case engAnalog:  analog.start (si); break;
        case engTwin:    twin.start (si); break;
        case engPoly:    poly.start (si); break;
        case engFM:      fm.start (si); break;
        case engWave:    wave.start (si); break;
        case engDrums:   drums.start (si, p); break;
        case engOrgan:   organ.start (si, p); break;
        case engPlucked: pluck.start (si, p, freq); break;
        case engEPiano:  epiano.start (si, freq); break;
        case engPiano:   piano.start (si, p, freq); break;
        default: break;
    }
}

void Voice::release()
{
    gate = false;
    if (engine == engDrums)
        return; // one-shot
    if (engine == engPiano && note >= 89)
        return; // top octave has no dampers
    ampEnv.noteOff();
    fltEnv.noteOff();
}

void Voice::kill()
{
    if (! active) return;
    killing = true;
    killStep = 1.0f / (0.004f * sr);
}

void Voice::render (float* L, float* R, int n, const RenderCtx& ctx)
{
    int done = 0;
    while (done < n && active)
    {
        const int len = std::min (32, n - done);
        renderBlock (L + done, R + done, len, ctx);
        done += len;
    }
}

void Voice::renderBlock (float* L, float* R, int n, const RenderCtx& ctx)
{
    const float* p = ctx.p;
    const float dt = (float) n / sr;

    // glide
    if (curNote != targetNote)
    {
        const float g = p[tpGlide];
        if (g <= 0.0005f) curNote = targetNote;
        else
        {
            const float k = 1.0f - std::exp (-dt / (g * 0.4f));
            curNote += (targetNote - curNote) * k;
            if (std::abs (targetNote - curNote) < 0.001f) curNote = targetNote;
        }
    }

    // LFO with delayed fade-in
    const float lfoVal = lfo.tick ((int) p[tpLfoWave], p[tpLfoRate], dt);
    const float fade = p[tpLfoDelay] <= 0.0f ? 1.0f : clamp01 ((voiceTime - p[tpLfoDelay]) / 0.3f);
    const float lfoFaded = lfoVal * fade;
    voiceTime += dt;

    // pitch
    // envelopes (filter env at control rate, also drives the pitch EG)
    ampEnv.set (p[tpAA], p[tpAD], p[tpAS], p[tpAR]);
    fltEnv.set (p[tpFA], p[tpFD], p[tpFS], p[tpFR]);
    const float fe = fltEnv.advance (n);

    const float ctlVib = ctx.modWheel * p[tpWheelVib] * 0.6f + ctx.aftertouch * p[tpAtVib] * 0.6f;
    const float pitch = curNote + p[tpTranspose] + p[tpFine] * 0.01f + ctx.bendSemis
                      + lfoFaded * p[tpLfoPitch] * 2.0f + lfoVal * ctlVib + p[tpPitchEg] * fe * 24.0f;
    const float freq = std::clamp (mtof (pitch), 8.0f, sr * 0.45f);

    // engine
    switch (engine)
    {
        case engAnalog:  analog.render (tmp, n, freq, p); break;
        case engTwin:    twin.render (tmp, n, freq, p); break;
        case engPoly:    poly.render (tmp, n, freq, p); break;
        case engFM:      fm.render (tmp, n, freq, p); break;
        case engWave:    wave.render (tmp, n, freq, p, fe); break;
        case engDrums:   drums.render (tmp, n, p); break;
        case engOrgan:   organ.render (tmp, n, freq, p); break;
        case engPlucked: pluck.render (tmp, n, freq, p, gate); break;
        case engEPiano:  epiano.render (tmp, n, freq, p, ctx.timeSec); break;
        case engPiano:   piano.render (tmp, n); break;
        default: std::fill (tmp, tmp + n, 0.0f); break;
    }

    // filter
    const int ft = (int) p[tpFltType];
    if (ft != fltOff)
    {
        const float octs = p[tpFltEnv] * fe * 7.0f
                         + p[tpFltKey] * ((float) note - 60.0f) / 12.0f
                         + p[tpFltVel] * (vel - 0.7f) * 3.0f
                         + lfoFaded * p[tpLfoFilter] * 3.0f
                         + ctx.modWheel * p[tpWheelCut] * 4.0f;
        const float fc = std::clamp (p[tpCutoff] * std::exp2 (octs), 20.0f, std::min (20000.0f, sr * 0.45f));
        const float res = clamp01 (p[tpReso]);
        const float drive = 1.0f + p[tpDrive] * 6.0f;
        const float comp = 1.0f / std::sqrt (drive);

        switch (ft)
        {
            case fltLP12:
                svf1.set (fc, res, sr);
                for (int i = 0; i < n; ++i) tmp[i] = svf1.lp (fastTanh (tmp[i] * drive) * comp);
                break;
            case fltLP24:
                svf1.set (fc, res * 0.8f, sr); svf2.set (fc, res * 0.4f, sr);
                for (int i = 0; i < n; ++i) tmp[i] = svf2.lp (svf1.lp (fastTanh (tmp[i] * drive) * comp));
                break;
            case fltLadder:
                ladder.set (fc, res, sr);
                for (int i = 0; i < n; ++i) tmp[i] = ladder.tick (tmp[i], drive);
                break;
            case fltMS:
            {
                svf1.set (fc, std::min (1.0f, res * 1.02f), sr);
                for (int i = 0; i < n; ++i)
                {
                    const float y = svf1.lp (tmp[i] * drive);
                    tmp[i] = fastTanh (y * 1.4f) * 0.8f * comp;
                }
                break;
            }
            case fltBP:
                svf1.set (fc, res, sr);
                for (int i = 0; i < n; ++i) tmp[i] = svf1.bp (fastTanh (tmp[i] * drive) * comp) * 1.5f;
                break;
            case fltHP:
                svf1.set (fc, res, sr);
                for (int i = 0; i < n; ++i) tmp[i] = svf1.hp (fastTanh (tmp[i] * drive) * comp);
                break;
            default: break;
        }
    }

    // amplitude & pan
    const float velGain = 1.0f - p[tpAmpVel] + p[tpAmpVel] * (0.15f + 0.85f * vel * vel);
    const float lfoAmp = 1.0f - p[tpLfoAmp] * 0.5f * (1.0f + lfoFaded);
    float pan = 0.0f;
    if (engine == engPiano) pan = std::clamp (((float) note - 64.0f) / 40.0f * p[tpE8], -1.0f, 1.0f);
    if (engine == engEPiano) pan = epiano.tremPan;
    const float panAngle = (pan + 1.0f) * 0.25f * kPi;
    const float gl = std::cos (panAngle) * 1.4142f, gr = std::sin (panAngle) * 1.4142f;
    const float g = velGain * lfoAmp;

    for (int i = 0; i < n; ++i)
    {
        float a = ampEnv.next() * g;
        if (killing)
        {
            killGain -= killStep;
            if (killGain <= 0.0f) { killGain = 0.0f; }
            a *= killGain;
        }
        const float s = tmp[i] * a;
        L[i] += s * gl;
        R[i] += s * gr;
    }

    bool engineDone = false;
    switch (engine)
    {
        case engDrums:   engineDone = drums.done(); break;
        case engPlucked: engineDone = pluck.done(); break;
        case engEPiano:  engineDone = epiano.done(); break;
        case engPiano:   engineDone = piano.done(); break;
        default: break;
    }
    if (! ampEnv.isActive() || engineDone || (killing && killGain <= 0.0f))
    {
        active = false;
        killing = false;
        note = -1;
        gate = false;
    }
}
} // namespace rc
