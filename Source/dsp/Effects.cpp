#include "Effects.h"

namespace rc
{
//==============================================================================
void InsertFx::prepare (float sampleRate)
{
    sr = sampleRate;
    dl.init ((int) (sr * 1.0f) + 8);
    dr.init ((int) (sr * 1.0f) + 8);
    reset();
}

void InsertFx::reset()
{
    dl.clear(); dr.clear();
    for (int i = 0; i < 6; ++i) { apL[i] = apR[i] = 0.0f; bq[i].reset(); }
    phFbL = phFbR = 0.0f;
    env = 0.0f; gainSm = 1.0f;
    wahL.reset(); wahR.reset();
    toneL.reset(); toneR.reset(); preL.reset(); preR.reset(); xoverL.reset(); xoverR.reset();
    holdL = holdR = holdCnt = 0.0f;
    curType = -1; lastA = lastB = -1.0f;
}

void InsertFx::setType (int t)
{
    if (t == curType) return;
    reset();
    curType = t;
}

void InsertFx::process (float* L, float* R, int n, int type, float a, float b, float mix, float modWheel)
{
    if (type == fxOff || mix <= 0.0001f)
    {
        if (curType != fxOff) setType (fxOff);
        return;
    }
    setType (type);
    const bool coefChanged = (a != lastA || b != lastB);
    lastA = a; lastB = b;
    const float dry = 1.0f - mix, wet = mix;

    switch (type)
    {
        case fxChorus:
        case fxEnsemble:
        case fxFlanger:
        {
            const bool ens = type == fxEnsemble, fl = type == fxFlanger;
            const float rate = ens ? expMap (a, 0.3f, 6.0f) : expMap (a, 0.05f, 5.0f);
            const float depthMs = fl ? 0.3f + 2.5f * (ens ? 0.5f : 1.0f) : (ens ? 0.4f + 2.5f * b : 0.5f + 5.0f * b);
            const float baseMs = fl ? 0.8f : (ens ? 6.0f : 9.0f);
            const float fb = fl ? (b * 1.8f - 0.9f) : 0.0f;
            const float inc = rate / sr;
            for (int i = 0; i < n; ++i)
            {
                lfoPh += inc; if (lfoPh >= 1.0f) lfoPh -= 1.0f;
                const float inL = L[i], inR = R[i];
                float wl, wr;
                if (ens)
                {
                    // three-phase ensemble + fast vibrato component
                    float sumL = 0.0f, sumR = 0.0f;
                    for (int k = 0; k < 3; ++k)
                    {
                        const float ph = lfoPh + (float) k / 3.0f;
                        const float m = fastSin01 (ph) + 0.25f * fastSin01 (ph * 7.3f);
                        const float dlen = (baseMs + depthMs * (1.0f + m) * 0.5f) * 0.001f * sr;
                        sumL += dl.read (dlen);
                        sumR += dr.read (dlen * 1.03f);
                    }
                    wl = sumL / 3.0f; wr = sumR / 3.0f;
                }
                else
                {
                    const float ml = fastSin01 (lfoPh), mr = fastSin01 (lfoPh + 0.25f);
                    wl = dl.read ((baseMs + depthMs * (1.0f + ml) * 0.5f) * 0.001f * sr);
                    wr = dr.read ((baseMs + depthMs * (1.0f + mr) * 0.5f) * 0.001f * sr);
                }
                dl.push (inL + wl * fb);
                dr.push (inR + wr * fb);
                L[i] = inL * dry + wl * wet * 1.2f;
                R[i] = inR * dry + wr * wet * 1.2f;
            }
            break;
        }
        case fxPhaser:
        {
            const float rate = expMap (a, 0.05f, 5.0f), fb = b * 0.85f, inc = rate / sr;
            for (int i = 0; i < n; ++i)
            {
                lfoPh += inc; if (lfoPh >= 1.0f) lfoPh -= 1.0f;
                auto coefFor = [&] (float ph)
                {
                    const float fc = expMap (0.5f + 0.5f * fastSin01 (ph), 200.0f, 4000.0f);
                    const float tn = std::tan (kPi * fc / sr);
                    return (tn - 1.0f) / (tn + 1.0f);
                };
                const float cl = coefFor (lfoPh), cr = coefFor (lfoPh + 0.25f);
                float xl = L[i] + phFbL * fb, xr = R[i] + phFbR * fb;
                for (int k = 0; k < 6; ++k)
                {
                    const float yl = cl * xl + apL[k]; apL[k] = xl - cl * yl; xl = yl;
                    const float yr = cr * xr + apR[k]; apR[k] = xr - cr * yr; xr = yr;
                }
                phFbL = xl; phFbR = xr;
                L[i] = L[i] * dry + (L[i] + xl) * 0.5f * wet * 1.4f;
                R[i] = R[i] * dry + (R[i] + xr) * 0.5f * wet * 1.4f;
            }
            break;
        }
        case fxTremolo:
        case fxAutoPan:
        {
            const float inc = expMap (a, 0.3f, 12.0f) / sr;
            for (int i = 0; i < n; ++i)
            {
                lfoPh += inc; if (lfoPh >= 1.0f) lfoPh -= 1.0f;
                const float m = fastSin01 (lfoPh);
                if (type == fxTremolo)
                {
                    const float g = 1.0f - b * 0.5f * (1.0f + m);
                    L[i] *= dry + wet * g; R[i] *= dry + wet * g;
                }
                else
                {
                    const float pan = m * b;
                    const float gl = 1.0f - std::max (0.0f, pan), gr = 1.0f + std::min (0.0f, pan);
                    L[i] *= dry + wet * gl; R[i] *= dry + wet * gr;
                }
            }
            break;
        }
        case fxRotary:
        {
            const bool fast = (a > 0.5f) != (modWheel > 0.5f);
            const float hornT = fast ? 6.8f : 0.8f, drumT = fast ? 5.9f : 0.67f;
            const float drive = 1.0f + b * 6.0f;
            xoverL.setLP (800.0f, sr);
            const float accel = 1.0f - std::exp (-1.0f / (0.7f * sr)), accelD = 1.0f - std::exp (-1.0f / (2.5f * sr));
            for (int i = 0; i < n; ++i)
            {
                hornSpeed += (hornT - hornSpeed) * accel;
                drumSpeed += (drumT - drumSpeed) * accelD;
                hornPh += hornSpeed / sr; if (hornPh >= 1.0f) hornPh -= 1.0f;
                drumPh += drumSpeed / sr; if (drumPh >= 1.0f) drumPh -= 1.0f;
                float in = fastTanh ((L[i] + R[i]) * 0.5f * drive) / std::sqrt (drive) * 1.3f;
                const float lo = xoverL.lp (in), hi = in - lo;
                dl.push (hi);
                const float hs = fastSin01 (hornPh), hc = fastSin01 (hornPh + 0.25f);
                const float ds = fastSin01 (drumPh);
                const float hornL = dl.read ((1.2f + 0.9f * hs) * 0.001f * sr) * (0.7f + 0.3f * hc);
                const float hornR = dl.read ((1.2f - 0.9f * hs) * 0.001f * sr) * (0.7f - 0.3f * hc);
                const float drumL = lo * (0.75f + 0.25f * ds), drumR = lo * (0.75f - 0.25f * ds);
                const float wl = hornL + drumL, wr = hornR + drumR;
                L[i] = L[i] * dry + wl * wet;
                R[i] = R[i] * dry + wr * wet;
            }
            break;
        }
        case fxOverdrive:
        case fxAmp:
        {
            const bool amp = type == fxAmp;
            const float drive = amp ? 1.0f + a * 30.0f : 1.0f + a * 14.0f;
            if (coefChanged)
            {
                toneL.setLP (expMap (b, 1200.0f, 12000.0f), sr);
                toneR.setLP (expMap (b, 1200.0f, 12000.0f), sr);
                preL.setLP (amp ? 120.0f : 60.0f, sr); preR.setLP (amp ? 120.0f : 60.0f, sr);
                bq[0].peak (amp ? 1600.0f : 900.0f, 0.8f, amp ? 4.0f : 2.0f, sr);
                bq[1].peak (amp ? 1600.0f : 900.0f, 0.8f, amp ? 4.0f : 2.0f, sr);
                bq[2].lowpass (amp ? 4500.0f : 9000.0f, 0.7f, sr);
                bq[3].lowpass (amp ? 4500.0f : 9000.0f, 0.7f, sr);
            }
            const float makeup = 1.0f / std::pow (drive, amp ? 0.55f : 0.5f);
            for (int i = 0; i < n; ++i)
            {
                float xl = preL.hp (L[i]) * drive, xr = preR.hp (R[i]) * drive;
                if (amp)
                {
                    xl = fastTanh (fastTanh (xl + 0.2f) - 0.197f) ; xr = fastTanh (fastTanh (xr + 0.2f) - 0.197f);
                    xl = bq[2].tick (bq[0].tick (xl)); xr = bq[3].tick (bq[1].tick (xr));
                    xl *= 2.0f; xr *= 2.0f;
                }
                else
                {
                    xl = bq[2].tick (bq[0].tick (fastTanh (xl) * makeup * 2.0f));
                    xr = bq[3].tick (bq[1].tick (fastTanh (xr) * makeup * 2.0f));
                }
                xl = toneL.lp (xl); xr = toneR.lp (xr);
                L[i] = L[i] * dry + xl * wet;
                R[i] = R[i] * dry + xr * wet;
            }
            break;
        }
        case fxCompressor:
        {
            const float thr = dbToGain (-6.0f - 30.0f * a), ratio = 4.0f;
            const float att = std::exp (-1.0f / (expMap (1.0f - b, 0.001f, 0.03f) * sr));
            const float rel = std::exp (-1.0f / (expMap (1.0f - b, 0.05f, 0.5f) * sr));
            const float makeup = 1.0f + a * 2.5f;
            for (int i = 0; i < n; ++i)
            {
                const float x = std::max (std::abs (L[i]), std::abs (R[i]));
                env = x > env ? x + (env - x) * att : x + (env - x) * rel;
                float g = 1.0f;
                if (env > thr) g = std::pow (env / thr, 1.0f / ratio - 1.0f);
                L[i] = L[i] * dry + L[i] * g * makeup * wet;
                R[i] = R[i] * dry + R[i] * g * makeup * wet;
            }
            break;
        }
        case fxEQ:
        {
            if (coefChanged)
            {
                bq[0].lowShelf (200.0f, (a - 0.5f) * 24.0f, sr); bq[1].lowShelf (200.0f, (a - 0.5f) * 24.0f, sr);
                bq[2].highShelf (4000.0f, (b - 0.5f) * 24.0f, sr); bq[3].highShelf (4000.0f, (b - 0.5f) * 24.0f, sr);
            }
            for (int i = 0; i < n; ++i)
            {
                const float xl = bq[2].tick (bq[0].tick (L[i])), xr = bq[3].tick (bq[1].tick (R[i]));
                L[i] = L[i] * dry + xl * wet;
                R[i] = R[i] * dry + xr * wet;
            }
            break;
        }
        case fxAutoWah:
        {
            const float sens = 2.0f + a * 20.0f, res = 0.5f + b * 0.45f;
            const float att = std::exp (-1.0f / (0.004f * sr)), rel = std::exp (-1.0f / (0.12f * sr));
            for (int i = 0; i < n; i += 8)
            {
                const int m = std::min (8, n - i);
                for (int j = 0; j < m; ++j)
                {
                    const float x = std::abs (L[i + j]) + std::abs (R[i + j]);
                    env = x > env ? x + (env - x) * att : x + (env - x) * rel;
                }
                const float fc = expMap (clamp01 (env * sens * 0.5f), 250.0f, 3500.0f);
                wahL.set (fc, res, sr); wahR.set (fc, res, sr);
                for (int j = 0; j < m; ++j)
                {
                    const float yl = wahL.bp (L[i + j]) * 2.0f, yr = wahR.bp (R[i + j]) * 2.0f;
                    L[i + j] = L[i + j] * dry + yl * wet;
                    R[i + j] = R[i + j] * dry + yr * wet;
                }
            }
            break;
        }
        case fxLoFi:
        {
            const float bits = 3.0f + (1.0f - a) * 13.0f, q = std::exp2 (bits - 1.0f);
            const float hold = 1.0f + b * 24.0f;
            for (int i = 0; i < n; ++i)
            {
                holdCnt += 1.0f;
                if (holdCnt >= hold)
                {
                    holdCnt -= hold;
                    holdL = std::round (L[i] * q) / q;
                    holdR = std::round (R[i] * q) / q;
                }
                L[i] = L[i] * dry + holdL * wet;
                R[i] = R[i] * dry + holdR * wet;
            }
            break;
        }
        case fxDelay:
        {
            const float tm = expMap (a, 20.0f, 900.0f) * 0.001f * sr, fb = b * 0.85f;
            for (int i = 0; i < n; ++i)
            {
                const float yl = dl.read (tm), yr = dr.read (tm * 1.07f);
                dl.push (L[i] + yr * fb);
                dr.push (R[i] + yl * fb);
                L[i] = L[i] * dry + yl * wet;
                R[i] = R[i] * dry + yr * wet;
            }
            break;
        }
        default: break;
    }
}

//==============================================================================
void MasterDelay::prepare (float sampleRate)
{
    sr = sampleRate;
    dl.init ((int) (sr * 2.6f));
    dr.init ((int) (sr * 2.6f));
    reset();
}

void MasterDelay::reset() { dl.clear(); dr.clear(); tl.reset(); tr.reset(); }

void MasterDelay::process (const float* inL, const float* inR, float* outL, float* outR, int n,
                           float timeMs, float fb, float tone, float ping)
{
    tl.setLP (expMap (tone, 800.0f, 16000.0f), sr);
    tr.setLP (expMap (tone, 800.0f, 16000.0f), sr);
    const float k = 1.0f - std::exp (-1.0f / (0.05f * sr));
    for (int i = 0; i < n; ++i)
    {
        smTime += (timeMs - smTime) * k;
        const float d = std::min (smTime * 0.001f * sr, sr * 2.5f);
        const float yl = dl.read (d), yr = dr.read (d);
        const float inMono = (inL[i] + inR[i]) * 0.5f;
        // ping-pong: blend between straight stereo and cross-fed mono input
        const float xl = lerp (inL[i], inMono, ping) + tl.lp (lerp (yl, yr, ping)) * fb;
        const float xr = lerp (inR[i], 0.0f, ping) + tr.lp (lerp (yr, yl, ping)) * fb;
        dl.push (xl);
        dr.push (xr);
        outL[i] = yl;
        outR[i] = yr;
    }
}

//==============================================================================
void MasterReverb::prepare (float sampleRate)
{
    sr = sampleRate;
    rev.setSampleRate (sr);
    pl.init ((int) (sr * 0.25f));
    pr.init ((int) (sr * 0.25f));
    hpL.highpass (120.0f, 0.7f, sr);
    hpR.highpass (120.0f, 0.7f, sr);
    reset();
}

void MasterReverb::reset() { rev.reset(); pl.clear(); pr.clear(); hpL.reset(); hpR.reset(); }

void MasterReverb::process (const float* inL, const float* inR, float* outL, float* outR, int n,
                            float size, float damp, float preMs)
{
    juce::Reverb::Parameters prm;
    prm.roomSize = 0.3f + 0.69f * size;
    prm.damping = damp;
    prm.wetLevel = 1.0f;
    prm.dryLevel = 0.0f;
    prm.width = 1.0f;
    prm.freezeMode = 0.0f;
    rev.setParameters (prm);
    const float pd = preMs * 0.001f * sr;
    for (int i = 0; i < n; ++i)
    {
        const float hl = hpL.tick (inL[i]), hr = hpR.tick (inR[i]);
        pl.push (hl);
        pr.push (hr);
        outL[i] = pd < 1.0f ? hl : pl.read (pd);
        outR[i] = pd < 1.0f ? hr : pr.read (pd);
    }
    rev.processStereo (outL, outR, n);
}

//==============================================================================
void Limiter::process (float* L, float* R, int n)
{
    const float rel = std::exp (-1.0f / (0.15f * sr));
    for (int i = 0; i < n; ++i)
    {
        const float pk = std::max (std::abs (L[i]), std::abs (R[i]));
        const float target = pk > 0.95f ? 0.95f / pk : 1.0f;
        gain = target < gain ? target : target + (gain - target) * rel;
        L[i] = fastTanh (L[i] * gain * 1.05f) / 1.05f;
        R[i] = fastTanh (R[i] * gain * 1.05f) / 1.05f;
    }
}
} // namespace rc
