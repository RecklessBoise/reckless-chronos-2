#pragma once

#include "../Params.h"

namespace rc
{
enum class Mode { Program = 0, Combi = 1 };

struct ParamValues
{
    std::vector<std::pair<int, float>> timbre;   // (TP index, real value)
    std::vector<std::pair<int, float>> global;   // (GP index, real value)
};

struct ProgramPreset
{
    juce::String name, category;
    ParamValues values;
    bool user = false;
    juce::File file;
};

struct CombiSlot
{
    bool used = false;
    juce::String program;                 // referenced factory program (optional)
    std::vector<std::pair<int, float>> p; // overrides / full values
};

struct CombiPreset
{
    juce::String name, category;
    std::array<CombiSlot, kNumTimbres> slots;
    std::vector<std::pair<int, float>> global;
    bool user = false;
    juce::File file;
};

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& state);

    static const juce::StringArray& programCategories();
    static const juce::StringArray& combiCategories();
    static juce::Colour categoryColour (int index);

    void loadFactory();
    void scanUserPresets();
    juce::File userFolder() const;

    const std::vector<ProgramPreset>& programs() const { return progs; }
    const std::vector<CombiPreset>& combis() const { return combs; }

    // indices into programs()/combis() for a category ("User" = user presets)
    std::vector<int> programsInCategory (const juce::String& cat) const;
    std::vector<int> combisInCategory (const juce::String& cat) const;
    int findProgram (const juce::String& name) const;

    // Apply presets to the parameter state (message thread)
    void applyProgram (int index);
    void applyProgramToTimbre (int index, int timbre, bool resetGlobals);
    void applyCombi (int index);
    void initProgram();

    // Favourites ("liked" presets), shared by all instances through a file in the user folder
    bool isFavorite (Mode mode, const juce::String& name) const;
    void toggleFavorite (Mode mode, const juce::String& name);
    void reloadFavorites();
    int numFavorites (Mode mode) const;

    bool saveCurrent (Mode mode, const juce::String& name, const juce::String& category, juce::String& error);

    // Unknown parameter names met while parsing (should be empty)
    const juce::StringArray& parseErrors() const { return errors; }

    std::function<void (Mode, const juce::String&)> onPresetApplied;

    // Name of the program currently assigned to each timbre (for the mixer page)
    std::array<juce::String, kNumTimbres> timbreNames;

private:
    void setParam (const juce::String& id, float realValue);
    void resetTimbre (int t, bool on);
    void resetGlobalsForPreset();
    bool parseValue (const juce::var& v, const ParamSpec& spec, float& out) const;
    void parseParamObject (const juce::var& obj, std::vector<std::pair<int, float>>& t,
                           std::vector<std::pair<int, float>>* g, const juce::String& context);
    juce::var timbreToVar (int t, bool onlyNonDefault) const;
    juce::var globalsToVar() const;

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<ProgramPreset> progs;
    std::vector<CombiPreset> combs;
    juce::StringArray errors;
    juce::StringArray favorites; // "P|name" / "C|name", in the order they were liked
    juce::File favoritesFile() const { return userFolder().getChildFile ("favorites.json"); }
    void saveFavorites() const;
};
} // namespace rc
