#include "Params.h"

namespace rc
{
const juce::StringArray& engineNames()
{
    static const juce::StringArray n { "Grand Piano", "Electric Piano", "Wave ROM", "Drum Kit", "Analog",
                                       "Tonewheel Organ", "Plucked String", "FM Matrix", "Twin Filter", "Vintage Poly" };
    return n;
}

const juce::StringArray& engineShortNames()
{
    static const juce::StringArray n { "PNO", "EPN", "WAV", "DRM", "ANA", "ORG", "PLK", "FM", "TWN", "VP6" };
    return n;
}

const juce::StringArray& fxNames()
{
    static const juce::StringArray n { "Off", "Chorus", "Ensemble", "Flanger", "Phaser", "Tremolo", "Auto Pan",
                                       "Rotary", "Overdrive", "Amp Sim", "Compressor", "Tone EQ", "Auto Wah",
                                       "Lo-Fi", "Delay" };
    return n;
}

const std::vector<ParamSpec>& timbreSpecs()
{
    static const std::vector<ParamSpec> s = []
    {
        using K = PKind;
        std::vector<ParamSpec> v;
        auto F = [&] (const char* id, const char* nm, float mn, float mx, float df, float c = 0.0f)
        { v.push_back ({ id, nm, K::Float, mn, mx, df, c, {} }); };
        auto I = [&] (const char* id, const char* nm, float mn, float mx, float df)
        { v.push_back ({ id, nm, K::Int, mn, mx, df, 0.0f, {} }); };
        auto C = [&] (const char* id, const char* nm, juce::StringArray ch, float df)
        { v.push_back ({ id, nm, K::Choice, 0.0f, (float) (ch.size() - 1), df, 0.0f, ch }); };
        auto B = [&] (const char* id, const char* nm, bool df)
        { v.push_back ({ id, nm, K::Bool, 0.0f, 1.0f, df ? 1.0f : 0.0f, 0.0f, {} }); };

        C ("engine", "Engine", engineNames(), (float) engAnalog);
        F ("e1", "Macro 1", 0, 1, 0.5f); F ("e2", "Macro 2", 0, 1, 0.5f);
        F ("e3", "Macro 3", 0, 1, 0.5f); F ("e4", "Macro 4", 0, 1, 0.5f);
        F ("e5", "Macro 5", 0, 1, 0.5f); F ("e6", "Macro 6", 0, 1, 0.5f);
        F ("e7", "Macro 7", 0, 1, 0.5f); F ("e8", "Macro 8", 0, 1, 0.5f);
        I ("transpose", "Transpose", -24, 24, 0);
        F ("fine", "Fine Tune", -50, 50, 0);
        C ("voiceMode", "Voice Mode", { "Poly", "Mono", "Legato" }, 0);
        F ("glide", "Glide", 0, 2, 0, 0.25f);
        I ("unison", "Unison", 1, 4, 1);
        F ("detune", "Unison Detune", 0, 1, 0.2f);
        I ("pbRange", "Bend Range", 0, 12, 2);
        F ("pitchEg", "Pitch EG Int", -1, 1, 0);
        C ("fltType", "Filter Type", { "Off", "LP12", "LP24", "Ladder", "MS-LP", "BP", "HP" }, (float) fltLP24);
        F ("cutoff", "Cutoff", 20, 20000, 8000, 1000);
        F ("reso", "Resonance", 0, 1, 0.1f);
        F ("fltEnv", "Filter EG Int", -1, 1, 0.3f);
        F ("fltKey", "Filter Key Track", 0, 1, 0.5f);
        F ("fltVel", "Filter Velocity", 0, 1, 0.3f);
        F ("drive", "Drive", 0, 1, 0);
        F ("fA", "Filter Attack", 0.001f, 10, 0.005f, 0.5f);
        F ("fD", "Filter Decay", 0.001f, 10, 0.4f, 0.5f);
        F ("fS", "Filter Sustain", 0, 1, 0.3f);
        F ("fR", "Filter Release", 0.001f, 10, 0.3f, 0.5f);
        F ("aA", "Amp Attack", 0.001f, 10, 0.003f, 0.5f);
        F ("aD", "Amp Decay", 0.001f, 20, 0.5f, 0.8f);
        F ("aS", "Amp Sustain", 0, 1, 1);
        F ("aR", "Amp Release", 0.001f, 20, 0.3f, 0.8f);
        F ("ampVel", "Amp Velocity", 0, 1, 0.6f);
        C ("lfoWave", "LFO Wave", { "Sine", "Triangle", "Saw", "Square", "S&H" }, 0);
        F ("lfoRate", "LFO Rate", 0.05f, 20, 5, 2);
        F ("lfoDelay", "LFO Delay", 0, 3, 0.2f);
        F ("lfoPitch", "LFO > Pitch", 0, 1, 0);
        F ("lfoFilter", "LFO > Filter", 0, 1, 0);
        F ("lfoAmp", "LFO > Amp", 0, 1, 0);
        F ("wheelVib", "Wheel > Vibrato", 0, 1, 0.3f);
        F ("wheelCut", "Wheel > Cutoff", 0, 1, 0);
        F ("atVib", "AT > Vibrato", 0, 1, 0);
        C ("ifx1Type", "IFX1 Type", fxNames(), 0);
        F ("ifx1A", "IFX1 A", 0, 1, 0.5f); F ("ifx1B", "IFX1 B", 0, 1, 0.5f); F ("ifx1Mix", "IFX1 Mix", 0, 1, 0.5f);
        C ("ifx2Type", "IFX2 Type", fxNames(), 0);
        F ("ifx2A", "IFX2 A", 0, 1, 0.5f); F ("ifx2B", "IFX2 B", 0, 1, 0.5f); F ("ifx2Mix", "IFX2 Mix", 0, 1, 0.5f);
        F ("level", "Level", 0, 1, 0.75f);
        F ("pan", "Pan", -1, 1, 0);
        F ("send1", "Delay Send", 0, 1, 0.1f);
        F ("send2", "Reverb Send", 0, 1, 0.2f);
        B ("on", "On", true);
        I ("keyLo", "Key Low", 0, 127, 0);
        I ("keyHi", "Key High", 0, 127, 127);
        I ("velLo", "Vel Low", 1, 127, 1);
        I ("velHi", "Vel High", 1, 127, 127);
        B ("vector", "Vector", false);
        jassert ((int) v.size() == tpCount);
        return v;
    }();
    return s;
}

const std::vector<ParamSpec>& globalSpecs()
{
    static const std::vector<ParamSpec> s = []
    {
        using K = PKind;
        std::vector<ParamSpec> v;
        auto F = [&] (const char* id, const char* nm, float mn, float mx, float df, float c = 0.0f)
        { v.push_back ({ id, nm, K::Float, mn, mx, df, c, {} }); };
        auto I = [&] (const char* id, const char* nm, float mn, float mx, float df)
        { v.push_back ({ id, nm, K::Int, mn, mx, df, 0.0f, {} }); };
        auto C = [&] (const char* id, const char* nm, juce::StringArray ch, float df)
        { v.push_back ({ id, nm, K::Choice, 0.0f, (float) (ch.size() - 1), df, 0.0f, ch }); };
        auto B = [&] (const char* id, const char* nm, bool df)
        { v.push_back ({ id, nm, K::Bool, 0.0f, 1.0f, df ? 1.0f : 0.0f, 0.0f, {} }); };

        F ("master", "Master Volume", 0, 1, 0.8f);
        F ("vecX", "Vector X", -1, 1, 0);
        F ("vecY", "Vector Y", -1, 1, 0);
        F ("rt1", "RT Cutoff", -1, 1, 0);
        F ("rt2", "RT Resonance", -1, 1, 0);
        F ("rt3", "RT EG Int", -1, 1, 0);
        F ("rt4", "RT Attack", -1, 1, 0);
        F ("rt5", "RT Decay", -1, 1, 0);
        F ("rt6", "RT Release", -1, 1, 0);
        F ("rt7", "RT Reverb", -1, 1, 0);
        F ("rt8", "RT Delay", -1, 1, 0);
        C ("dlySync", "Delay Sync", { "Free", "1/4", "1/8", "1/8D", "1/8T", "1/16", "1/4D" }, 0);
        F ("dlyTime", "Delay Time", 10, 2000, 375, 300);
        F ("dlyFb", "Delay Feedback", 0, 0.95f, 0.35f);
        F ("dlyTone", "Delay Tone", 0, 1, 0.6f);
        F ("dlyPing", "Delay Ping-Pong", 0, 1, 0.5f);
        F ("dlyRet", "Delay Return", 0, 1, 1);
        F ("revSize", "Reverb Size", 0, 1, 0.6f);
        F ("revDamp", "Reverb Damping", 0, 1, 0.4f);
        F ("revPre", "Reverb Pre-Delay", 0, 200, 20);
        F ("revRet", "Reverb Return", 0, 1, 1);
        B ("arpOn", "Arp On", false);
        C ("arpMode", "Arp Mode", { "Up", "Down", "Up-Down", "Random", "As Played", "Chord" }, 0);
        C ("arpRate", "Arp Rate", { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 3);
        I ("arpOct", "Arp Octaves", 1, 4, 1);
        F ("arpGate", "Arp Gate", 0.05f, 1, 0.5f);
        F ("arpSwing", "Arp Swing", 0, 0.6f, 0);
        B ("arpLatch", "Arp Latch", false);
        jassert ((int) v.size() == gpCount);
        return v;
    }();
    return s;
}

juce::String timbreParamId (int timbre, int tp)
{
    return "t" + juce::String (timbre + 1) + "_" + timbreSpecs()[(size_t) tp].id;
}

juce::String globalParamId (int gp) { return globalSpecs()[(size_t) gp].id; }

static std::function<juce::String (float, int)> formatterFor (const juce::String& id)
{
    auto time = [] (float v, int) -> juce::String
    {
        if (v < 1.0f) return juce::String ((int) std::round (v * 1000.0f)) + " ms";
        return juce::String (v, v < 10.0f ? 2 : 1) + " s";
    };
    auto ms = [] (float v, int) -> juce::String { return juce::String ((int) std::round (v)) + " ms"; };
    auto hz = [] (float v, int) -> juce::String
    {
        if (v >= 1000.0f) return juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz";
        return juce::String (v, v < 10.0f ? 2 : 0) + " Hz";
    };
    auto pct = [] (float v, int) -> juce::String { return juce::String ((int) std::round (v * 100.0f)) + "%"; };
    auto bip = [] (float v, int) -> juce::String
    {
        const int p = (int) std::round (v * 100.0f);
        return (p > 0 ? "+" : "") + juce::String (p);
    };

    if (id == "cutoff" || id == "lfoRate") return hz;
    if (id == "dlyTime" || id == "revPre") return ms;
    if (id == "fA" || id == "fD" || id == "fR" || id == "aA" || id == "aD" || id == "aR" || id == "glide" || id == "lfoDelay")
        return time;
    if (id == "fine") return [] (float v, int) { return juce::String ((int) std::round (v)) + " ct"; };
    if (id == "pan")
        return [] (float v, int) -> juce::String
        {
            const int p = (int) std::round (v * 100.0f);
            return p == 0 ? juce::String ("C") : (p < 0 ? "L" + juce::String (-p) : "R" + juce::String (p));
        };
    if (id == "fltEnv" || id == "pitchEg" || id.startsWith ("rt") || id == "vecX" || id == "vecY") return bip;
    return pct;
}

static std::unique_ptr<juce::RangedAudioParameter> makeParam (const juce::String& id, const juce::String& name,
                                                             const ParamSpec& s, float defOverride)
{
    const juce::ParameterID pid { id, 1 };
    switch (s.kind)
    {
        case PKind::Float:
        {
            juce::NormalisableRange<float> r (s.min, s.max);
            if (s.centre > 0.0f)
                r.setSkewForCentre (s.centre);
            return std::make_unique<juce::AudioParameterFloat> (
                pid, name, r, defOverride,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (formatterFor (s.id)));
        }
        case PKind::Int:
            return std::make_unique<juce::AudioParameterInt> (pid, name, (int) s.min, (int) s.max, (int) defOverride);
        case PKind::Choice:
            return std::make_unique<juce::AudioParameterChoice> (pid, name, s.choices, (int) defOverride);
        case PKind::Bool:
            return std::make_unique<juce::AudioParameterBool> (pid, name, defOverride > 0.5f);
    }
    return {};
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int g = 0; g < gpCount; ++g)
    {
        const auto& s = globalSpecs()[(size_t) g];
        layout.add (makeParam (s.id, s.name, s, s.def));
    }

    for (int t = 0; t < kNumTimbres; ++t)
    {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> (
            "timbre" + juce::String (t + 1), "Timbre " + juce::String (t + 1), "|");
        for (int p = 0; p < tpCount; ++p)
        {
            const auto& s = timbreSpecs()[(size_t) p];
            float def = s.def;
            if (p == tpOn)
                def = (t == 0) ? 1.0f : 0.0f;
            group->addChild (makeParam (timbreParamId (t, p),
                                        "T" + juce::String (t + 1) + " " + s.name, s, def));
        }
        layout.add (std::move (group));
    }
    return layout;
}

const std::array<const char*, kNumMacros>& macroLabels (int engine)
{
    static const std::array<std::array<const char*, kNumMacros>, kNumEngines> labels { {
        { "HAMMER", "SUSTAIN", "UNISON", "INHARM", "NOISE", "BOARD", "VEL>TONE", "WIDTH" },  // Piano
        { "TINE/REED", "HARDNESS", "BELL", "DECAY", "BARK", "ATTACK", "TREM DEP", "TREM RATE" }, // EP
        { "WAVE A", "WAVE B", "MORPH", "MORPH EG", "VEL>MORPH", "CHIFF", "BREATH", "B OCTAVE" }, // Wave
        { "KICK TUNE", "KICK DEC", "SNR TONE", "SNAPPY", "HAT DEC", "TOM TUNE", "CHARACTER", "NOISE COL" }, // Drums
        { "OSC1 WAVE", "OSC2 WAVE", "OSC2 FINE", "OSC2 INT", "OSC MIX", "SUB", "NOISE", "PW" },  // Analog
        { "16'", "5 1/3'", "8'", "4'", "2 2/3'", "2'", "UPPER", "PERC" },                        // Organ
        { "PLUCK POS", "EXCITE", "DECAY", "BRIGHT", "STIFF", "PICK", "BODY", "BOW" },           // Plucked
        { "ALGO", "RATIO 2", "INDEX 2", "RATIO 3", "INDEX 3", "FEEDBACK", "MOD DECAY", "VEL>IDX" }, // FM
        { "VCO1 WAVE", "VCO2 WAVE", "VCO2 PITCH", "VCO MIX", "HPF CUT", "HPF PEAK", "SCREAM", "PW" }, // Twin
        { "WAVEFORM", "PW", "PWM RATE", "PWM DEPTH", "SUB OSC", "NOISE", "DRIFT", "ENSEMBLE" },   // Poly
    } };
    return labels[(size_t) juce::jlimit (0, kNumEngines - 1, engine)];
}

std::pair<const char*, const char*> fxParamLabels (int t)
{
    switch (t)
    {
        case fxChorus:     return { "RATE", "DEPTH" };
        case fxEnsemble:   return { "SPEED", "DEPTH" };
        case fxFlanger:    return { "RATE", "FEEDBACK" };
        case fxPhaser:     return { "RATE", "FEEDBACK" };
        case fxTremolo:    return { "RATE", "DEPTH" };
        case fxAutoPan:    return { "RATE", "DEPTH" };
        case fxRotary:     return { "SPEED", "DRIVE" };
        case fxOverdrive:  return { "DRIVE", "TONE" };
        case fxAmp:        return { "GAIN", "TONE" };
        case fxCompressor: return { "AMOUNT", "SPEED" };
        case fxEQ:         return { "LOW", "HIGH" };
        case fxAutoWah:    return { "SENS", "RESO" };
        case fxLoFi:       return { "BITS", "RATE" };
        case fxDelay:      return { "TIME", "FEEDBACK" };
        default:           return { "A", "B" };
    }
}

const std::array<const char*, 8>& realtimeLabels()
{
    static const std::array<const char*, 8> l { "CUTOFF", "RESONANCE", "EG INT", "ATTACK",
                                                "DECAY", "RELEASE", "REVERB", "DELAY" };
    return l;
}
} // namespace rc
