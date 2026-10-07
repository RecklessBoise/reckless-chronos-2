#include "PresetManager.h"
#include "BinaryData.h"

namespace rc
{
namespace
{
int timbreIndexForId (const juce::String& id)
{
    const auto& s = timbreSpecs();
    for (int i = 0; i < (int) s.size(); ++i)
        if (id == s[(size_t) i].id) return i;
    return -1;
}

int globalIndexForId (const juce::String& id)
{
    const auto& s = globalSpecs();
    for (int i = 0; i < (int) s.size(); ++i)
        if (id == s[(size_t) i].id) return i;
    return -1;
}

bool isPresetGlobal (int gp)
{
    return gp != gpMaster;
}
} // namespace

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state) : apvts (state)
{
    loadFactory();
    scanUserPresets();
}

const juce::StringArray& PresetManager::programCategories()
{
    static const juce::StringArray c { "Keyboard", "Organ", "Bell/Mallet", "Strings", "Vocal/Airy", "Brass",
                                       "Woodwind/Reed", "Guitar/Plucked", "Bass", "Slow Synth", "Fast Synth",
                                       "Lead Synth", "Motion Synth", "SE", "Hit/Arpg", "Drums" };
    return c;
}

const juce::StringArray& PresetManager::combiCategories()
{
    static const juce::StringArray c { "Keyboard", "Organ", "Bell/Mallet", "Strings", "Pads", "Brass/Reed",
                                       "Orchestral", "World", "Guitar", "Bass Splits", "Synth", "Lead",
                                       "Motion", "SE/Hits", "Arpeggio", "Drums/Splits" };
    return c;
}

juce::Colour PresetManager::categoryColour (int index)
{
    static const juce::uint32 cols[16] = {
        0xffc0392b, 0xff8e44ad, 0xff2e86c1, 0xff27ae60, 0xff5d6d7e, 0xffd68910, 0xff16a085, 0xffa04000,
        0xff1f3a93, 0xff7d3c98, 0xffc0398b, 0xffb03a2e, 0xff117a65, 0xff6e2c00, 0xffb7950b, 0xff515a5a };
    return juce::Colour (cols[(size_t) (index & 15)]);
}

bool PresetManager::parseValue (const juce::var& v, const ParamSpec& spec, float& out) const
{
    if (spec.kind == PKind::Choice && v.isString())
    {
        const int idx = spec.choices.indexOf (v.toString(), true);
        if (idx < 0) return false;
        out = (float) idx;
        return true;
    }
    if (v.isBool()) { out = (bool) v ? 1.0f : 0.0f; return true; }
    if (v.isInt() || v.isDouble() || v.isInt64())
    {
        out = juce::jlimit (spec.min, spec.max, (float) (double) v);
        return true;
    }
    return false;
}

void PresetManager::parseParamObject (const juce::var& obj, std::vector<std::pair<int, float>>& t,
                                      std::vector<std::pair<int, float>>* g, const juce::String& context)
{
    if (auto* o = obj.getDynamicObject())
    {
        for (auto& prop : o->getProperties())
        {
            const juce::String id = prop.name.toString();
            float val = 0.0f;
            const int ti = timbreIndexForId (id);
            if (ti >= 0)
            {
                if (parseValue (prop.value, timbreSpecs()[(size_t) ti], val)) t.push_back ({ ti, val });
                else errors.add (context + ": bad value for " + id);
                continue;
            }
            const int gi = globalIndexForId (id);
            if (gi >= 0 && g != nullptr)
            {
                if (parseValue (prop.value, globalSpecs()[(size_t) gi], val)) g->push_back ({ gi, val });
                else errors.add (context + ": bad value for " + id);
                continue;
            }
            errors.add (context + ": unknown parameter " + id);
        }
    }
}

void PresetManager::loadFactory()
{
    progs.clear();
    combs.clear();
    errors.clear();

    const auto json = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::factory_json, BinaryData::factory_jsonSize));
    if (auto* pa = json.getProperty ("programs", {}).getArray())
    {
        for (auto& pv : *pa)
        {
            ProgramPreset p;
            p.name = pv.getProperty ("name", "Init").toString();
            p.category = pv.getProperty ("cat", "Keyboard").toString();
            parseParamObject (pv.getProperty ("p", {}), p.values.timbre, &p.values.global, p.name);
            parseParamObject (pv.getProperty ("g", {}), p.values.timbre, &p.values.global, p.name);
            progs.push_back (std::move (p));
        }
    }
    if (auto* ca = json.getProperty ("combis", {}).getArray())
    {
        for (auto& cv : *ca)
        {
            CombiPreset c;
            c.name = cv.getProperty ("name", "Init Combi").toString();
            c.category = cv.getProperty ("cat", "Keyboard").toString();
            if (auto* ta = cv.getProperty ("t", {}).getArray())
            {
                for (int i = 0; i < juce::jmin (kNumTimbres, ta->size()); ++i)
                {
                    const auto& tv = ta->getReference (i);
                    if (! tv.isObject()) continue;
                    auto& slot = c.slots[(size_t) i];
                    slot.used = true;
                    slot.program = tv.getProperty ("prog", "").toString();
                    if (slot.program.isNotEmpty() && findProgram (slot.program) < 0)
                        errors.add (c.name + ": unknown program " + slot.program);
                    parseParamObject (tv.getProperty ("p", {}), slot.p, nullptr, c.name);
                }
            }
            std::vector<std::pair<int, float>> dummy;
            parseParamObject (cv.getProperty ("g", {}), dummy, &c.global, c.name);
            combs.push_back (std::move (c));
        }
    }
}

juce::File PresetManager::userFolder() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
#if JUCE_MAC
        .getChildFile ("Audio").getChildFile ("Presets")
#endif
        .getChildFile ("Reckless").getChildFile ("Reckless Chronos 2");
}

void PresetManager::scanUserPresets()
{
    progs.erase (std::remove_if (progs.begin(), progs.end(), [] (auto& p) { return p.user; }), progs.end());
    combs.erase (std::remove_if (combs.begin(), combs.end(), [] (auto& c) { return c.user; }), combs.end());

    const auto dir = userFolder();
    if (! dir.isDirectory()) return;
    for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.rc2preset"))
    {
        const auto v = juce::JSON::parse (f);
        if (! v.isObject()) continue;
        if (v.getProperty ("type", "") == "combi")
        {
            CombiPreset c;
            c.name = v.getProperty ("name", f.getFileNameWithoutExtension()).toString();
            c.category = v.getProperty ("cat", "User").toString();
            c.user = true; c.file = f;
            if (auto* ta = v.getProperty ("t", {}).getArray())
                for (int i = 0; i < juce::jmin (kNumTimbres, ta->size()); ++i)
                {
                    auto& slot = c.slots[(size_t) i];
                    slot.used = true;
                    parseParamObject (ta->getReference (i), slot.p, nullptr, c.name);
                }
            std::vector<std::pair<int, float>> dummy;
            parseParamObject (v.getProperty ("g", {}), dummy, &c.global, c.name);
            combs.push_back (std::move (c));
        }
        else
        {
            ProgramPreset p;
            p.name = v.getProperty ("name", f.getFileNameWithoutExtension()).toString();
            p.category = v.getProperty ("cat", "User").toString();
            p.user = true; p.file = f;
            parseParamObject (v.getProperty ("p", {}), p.values.timbre, &p.values.global, p.name);
            parseParamObject (v.getProperty ("g", {}), p.values.timbre, &p.values.global, p.name);
            progs.push_back (std::move (p));
        }
    }
}

std::vector<int> PresetManager::programsInCategory (const juce::String& cat) const
{
    std::vector<int> r;
    for (int i = 0; i < (int) progs.size(); ++i)
        if ((cat == "User" && progs[(size_t) i].user) || (cat != "User" && ! progs[(size_t) i].user && progs[(size_t) i].category == cat))
            r.push_back (i);
    return r;
}

std::vector<int> PresetManager::combisInCategory (const juce::String& cat) const
{
    std::vector<int> r;
    for (int i = 0; i < (int) combs.size(); ++i)
        if ((cat == "User" && combs[(size_t) i].user) || (cat != "User" && ! combs[(size_t) i].user && combs[(size_t) i].category == cat))
            r.push_back (i);
    return r;
}

int PresetManager::findProgram (const juce::String& name) const
{
    for (int i = 0; i < (int) progs.size(); ++i)
        if (progs[(size_t) i].name == name) return i;
    return -1;
}

void PresetManager::setParam (const juce::String& id, float realValue)
{
    if (auto* prm = apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (realValue));
}

void PresetManager::resetTimbre (int t, bool on)
{
    for (int p = 0; p < tpCount; ++p)
    {
        float def = timbreSpecs()[(size_t) p].def;
        if (p == tpOn) def = on ? 1.0f : 0.0f;
        setParam (timbreParamId (t, p), def);
    }
}

void PresetManager::resetGlobalsForPreset()
{
    for (int g = 0; g < gpCount; ++g)
        if (isPresetGlobal (g))
            setParam (globalParamId (g), globalSpecs()[(size_t) g].def);
}

void PresetManager::applyProgramToTimbre (int index, int t, bool resetGlobals)
{
    if (index < 0 || index >= (int) progs.size()) return;
    const auto& p = progs[(size_t) index];
    resetTimbre (t, true);
    for (auto& [tp, v] : p.values.timbre)
        setParam (timbreParamId (t, tp), v);
    timbreNames[(size_t) t] = p.name;
    if (resetGlobals)
    {
        resetGlobalsForPreset();
        for (auto& [gp, v] : p.values.global)
            setParam (globalParamId (gp), v);
    }
}

void PresetManager::applyProgram (int index)
{
    if (index < 0 || index >= (int) progs.size()) return;
    applyProgramToTimbre (index, 0, true);
    for (int t = 1; t < kNumTimbres; ++t)
    {
        resetTimbre (t, false);
        timbreNames[(size_t) t] = "---";
    }
    if (onPresetApplied) onPresetApplied (Mode::Program, progs[(size_t) index].name);
}

void PresetManager::initProgram()
{
    resetGlobalsForPreset();
    resetTimbre (0, true);
    for (int t = 1; t < kNumTimbres; ++t) resetTimbre (t, false);
    timbreNames = { "Init Program", "---", "---", "---" };
    if (onPresetApplied) onPresetApplied (Mode::Program, "Init Program");
}

void PresetManager::applyCombi (int index)
{
    if (index < 0 || index >= (int) combs.size()) return;
    const auto& c = combs[(size_t) index];
    resetGlobalsForPreset();
    for (int t = 0; t < kNumTimbres; ++t)
    {
        const auto& slot = c.slots[(size_t) t];
        if (! slot.used) { resetTimbre (t, false); timbreNames[(size_t) t] = "---"; continue; }
        const int pi = slot.program.isNotEmpty() ? findProgram (slot.program) : -1;
        if (pi >= 0) applyProgramToTimbre (pi, t, false);
        else { resetTimbre (t, true); timbreNames[(size_t) t] = "Custom"; }
        for (auto& [tp, v] : slot.p)
            setParam (timbreParamId (t, tp), v);
    }
    for (auto& [gp, v] : c.global)
        setParam (globalParamId (gp), v);
    if (onPresetApplied) onPresetApplied (Mode::Combi, c.name);
}

juce::var PresetManager::timbreToVar (int t, bool onlyNonDefault) const
{
    auto* o = new juce::DynamicObject();
    for (int p = 0; p < tpCount; ++p)
    {
        const auto& spec = timbreSpecs()[(size_t) p];
        const float v = apvts.getRawParameterValue (timbreParamId (t, p))->load();
        if (onlyNonDefault && std::abs (v - spec.def) < 1.0e-6f && p != tpEngine) continue;
        if (spec.kind == PKind::Choice) o->setProperty (spec.id, spec.choices[(int) v]);
        else o->setProperty (spec.id, v);
    }
    return juce::var (o);
}

juce::var PresetManager::globalsToVar() const
{
    auto* o = new juce::DynamicObject();
    for (int g = 0; g < gpCount; ++g)
    {
        if (! isPresetGlobal (g) || g == gpVecX || g == gpVecY || (g >= gpRt1 && g <= gpRt8)) continue;
        const auto& spec = globalSpecs()[(size_t) g];
        const float v = apvts.getRawParameterValue (globalParamId (g))->load();
        if (spec.kind == PKind::Choice) o->setProperty (spec.id, spec.choices[(int) v]);
        else o->setProperty (spec.id, v);
    }
    return juce::var (o);
}

bool PresetManager::saveCurrent (Mode mode, const juce::String& name, const juce::String& category, juce::String& error)
{
    const auto dir = userFolder();
    if (! dir.createDirectory())
    {
        error = "Cannot create " + dir.getFullPathName();
        return false;
    }
    auto* root = new juce::DynamicObject();
    root->setProperty ("name", name);
    root->setProperty ("cat", category);
    if (mode == Mode::Combi)
    {
        root->setProperty ("type", "combi");
        juce::Array<juce::var> ts;
        for (int t = 0; t < kNumTimbres; ++t) ts.add (timbreToVar (t, false));
        root->setProperty ("t", ts);
    }
    else
    {
        root->setProperty ("type", "program");
        root->setProperty ("p", timbreToVar (0, true));
    }
    root->setProperty ("g", globalsToVar());

    const auto file = dir.getChildFile (juce::File::createLegalFileName (name) + ".rc2preset");
    if (! file.replaceWithText (juce::JSON::toString (juce::var (root))))
    {
        error = "Cannot write " + file.getFullPathName();
        return false;
    }
    scanUserPresets();
    return true;
}
} // namespace rc
