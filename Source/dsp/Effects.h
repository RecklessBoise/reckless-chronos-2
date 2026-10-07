#pragma once

#include "Common.h"
#include "../Params.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace rc
{
// Stereo insert effect, one instance per slot. Type can change at any time.
class InsertFx
{
public:
    void prepare (float sampleRate);
    void reset();
    void process (float* L, float* R, int n, int type, float a, float b, float mix, float modWheel);

private:
    void setType (int t);
    float sr = 48000.0f;
    int curType = -1;

    DelayLine dl, dr;
    float lfoPh = 0.0f;
    // phaser
    float apL[6] {}, apR[6] {}, phFbL = 0.0f, phFbR = 0.0f;
    // rotary
    float hornPh = 0.0f, drumPh = 0.0f, hornSpeed = 0.8f, drumSpeed = 0.67f;
    OnePole xoverL, xoverR;
    // drive / amp / eq
    Biquad bq[6];
    OnePole toneL, toneR, preL, preR;
    // compressor / wah
    float env = 0.0f, gainSm = 1.0f;
    Svf wahL, wahR;
    // lofi
    float holdL = 0.0f, holdR = 0.0f, holdCnt = 0.0f;
    float lastA = -1.0f, lastB = -1.0f;
};

class MasterDelay
{
public:
    void prepare (float sampleRate);
    void reset();
    void process (const float* inL, const float* inR, float* outL, float* outR, int n,
                  float timeMs, float fb, float tone, float ping);
private:
    float sr = 48000.0f;
    DelayLine dl, dr;
    OnePole tl, tr;
    float smTime = 375.0f;
};

class MasterReverb
{
public:
    void prepare (float sampleRate);
    void reset();
    void process (const float* inL, const float* inR, float* outL, float* outR, int n,
                  float size, float damp, float preMs);
private:
    float sr = 48000.0f;
    juce::Reverb rev;
    DelayLine pl, pr;
    Biquad hpL, hpR;
};

class Limiter
{
public:
    void prepare (float sampleRate) { sr = sampleRate; gain = 1.0f; }
    void process (float* L, float* R, int n);
private:
    float sr = 48000.0f, gain = 1.0f;
};
} // namespace rc
