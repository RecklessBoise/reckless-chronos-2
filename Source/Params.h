#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>

namespace rc
{
constexpr int kNumTimbres = 4;
constexpr int kNumEngines = 10;
constexpr int kNumMacros  = 8;

enum Engine
{
    engPiano = 0, engEPiano, engWave, engDrums, engAnalog,
    engOrgan, engPlucked, engFM, engTwin, engPoly
};

enum FilterType { fltOff = 0, fltLP12, fltLP24, fltLadder, fltMS, fltBP, fltHP };

enum FxType
{
    fxOff = 0, fxChorus, fxEnsemble, fxFlanger, fxPhaser, fxTremolo, fxAutoPan, fxRotary,
    fxOverdrive, fxAmp, fxCompressor, fxEQ, fxAutoWah, fxLoFi, fxDelay, fxNumTypes
};

// Per-timbre parameters. The order here is the index order used by the DSP.
enum TP
{
    tpEngine = 0,
    tpE1, tpE2, tpE3, tpE4, tpE5, tpE6, tpE7, tpE8,
    tpTranspose, tpFine, tpVoiceMode, tpGlide, tpUnison, tpDetune, tpPbRange,
    tpFltType, tpCutoff, tpReso, tpFltEnv, tpFltKey, tpFltVel, tpDrive,
    tpFA, tpFD, tpFS, tpFR,
    tpAA, tpAD, tpAS, tpAR, tpAmpVel,
    tpLfoWave, tpLfoRate, tpLfoDelay, tpLfoPitch, tpLfoFilter, tpLfoAmp,
    tpWheelVib, tpWheelCut, tpAtVib,
    tpIfx1Type, tpIfx1A, tpIfx1B, tpIfx1Mix,
    tpIfx2Type, tpIfx2A, tpIfx2B, tpIfx2Mix,
    tpLevel, tpPan, tpSend1, tpSend2,
    tpOn, tpKeyLo, tpKeyHi, tpVelLo, tpVelHi, tpVector,
    tpCount
};

enum GP
{
    gpMaster = 0, gpVecX, gpVecY,
    gpRt1, gpRt2, gpRt3, gpRt4, gpRt5, gpRt6, gpRt7, gpRt8,
    gpDlySync, gpDlyTime, gpDlyFb, gpDlyTone, gpDlyPing, gpDlyRet,
    gpRevSize, gpRevDamp, gpRevPre, gpRevRet,
    gpArpOn, gpArpMode, gpArpRate, gpArpOct, gpArpGate, gpArpSwing, gpArpLatch,
    gpCount
};

enum class PKind { Float, Int, Choice, Bool };

struct ParamSpec
{
    const char* id;      // suffix for timbre params ("cutoff"), full id for globals
    const char* name;
    PKind kind;
    float min, max, def;
    float centre;        // skew centre for Float (0 = linear)
    juce::StringArray choices;
};

const std::vector<ParamSpec>& timbreSpecs();   // size tpCount
const std::vector<ParamSpec>& globalSpecs();   // size gpCount

juce::String timbreParamId (int timbre, int tp);   // "t1_cutoff"
juce::String globalParamId (int gp);

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

const juce::StringArray& engineNames();
const juce::StringArray& engineShortNames();
const juce::StringArray& fxNames();

// Labels for the 8 engine macros, depending on engine
const std::array<const char*, kNumMacros>& macroLabels (int engine);
// Labels for the A/B knobs of an insert effect
std::pair<const char*, const char*> fxParamLabels (int fxType);

const std::array<const char*, 8>& realtimeLabels();
} // namespace rc
