#include "Screen.h"
#include "../PluginProcessor.h"
#include "../dsp/Tables.h"

namespace rc::ui
{
namespace
{
juce::String noteName (double v)
{
    return juce::MidiMessage::getMidiNoteName ((int) v, true, true, 4);
}

juce::String macroText (int engine, int macro, double v)
{
    const float x = (float) v;
    switch (engine)
    {
        case engWave:
            if (macro == 0 || macro == 1) return WaveTables::waveName (pickIndex (x, WaveTables::kWaves));
            if (macro == 7) { const char* o[] = { "+0", "+12", "+19", "+24" }; return o[pickIndex (x, 4)]; }
            break;
        case engAnalog:
            if (macro == 0 || macro == 1)
            {
                const char* n[] = { "SAW", "SQR", "TRI", "SINE" };
                const float p = x * 3.0f;
                const int i = (int) std::round (p);
                return std::abs (p - (float) i) < 0.12f ? juce::String (n[i]) : juce::String (n[(int) p]) + ">" + n[std::min (3, (int) p + 1)];
            }
            if (macro == 2) return "+" + juce::String ((int) std::round (x * 50.0f)) + "ct";
            if (macro == 3)
            {
                const int iv[] = { -24, -12, -7, -5, 0, 3, 4, 5, 7, 12, 19, 24 };
                const int s = iv[pickIndex (x, 12)];
                return (s > 0 ? "+" : "") + juce::String (s);
            }
            break;
        case engTwin:
            if (macro == 0) { const char* n[] = { "TRI", "SAW", "PULSE", "NOISE" }; return n[pickIndex (x, 4)]; }
            if (macro == 1) { const char* n[] = { "SAW", "SQR", "NARROW", "RING" }; return n[pickIndex (x, 4)]; }
            if (macro == 2) return juce::String ((x - 0.5f) * 24.0f, 1);
            break;
        case engPoly:
            if (macro == 0) { const char* n[] = { "SAW", "PULSE", "PWM" }; return n[pickIndex (x, 3)]; }
            break;
        case engFM:
        {
            static const float ratios[] = { 0.5f, 1, 1, 2, 2, 3, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14 };
            if (macro == 0) return "ALGO " + juce::String (pickIndex (x, 8) + 1);
            if (macro == 1 || macro == 3) return "x" + juce::String (ratios[pickIndex (x, 17)], 1);
            break;
        }
        case engOrgan:
            if (macro < 7) return juce::String ((int) std::round (x * 8.0f));
            break;
        default: break;
    }
    return juce::String ((int) std::round (x * 100.0f));
}

void styleScreenButton (juce::Button& b)
{
    b.getProperties().set ("screen", true);
}

template <typename T>
void layoutGrid (juce::Rectangle<int> area, std::vector<T*> items, int cols, int rowH)
{
    const int w = area.getWidth() / cols;
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (items[i] == nullptr) continue;
        const int c = (int) i % cols, r = (int) i / cols;
        items[i]->setBounds (area.getX() + c * w, area.getY() + r * rowH, w, rowH);
    }
}
} // namespace

//==============================================================================
class PlayPage : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PlayPage (RecklessChronosProcessor& p) : proc (p)
    {
        for (int i = 0; i < 16; ++i)
        {
            auto* b = tiles.add (new juce::TextButton());
            styleScreenButton (*b);
            b->setColour (juce::TextButton::buttonColourId, PresetManager::categoryColour (i).withSaturation (0.55f).darker (0.15f));
            b->onClick = [this, i] { openCategory (i); };
            addAndMakeVisible (b);
        }
        for (auto* b : { &userBtn, &saveBtn, &initBtn, &backBtn })
        {
            styleScreenButton (*b);
            addAndMakeVisible (b);
        }
        userBtn.onClick = [this] { openCategory (16); };
        initBtn.onClick = [this] { proc.presets.initProgram(); };
        backBtn.onClick = [this] { showGrid(); };
        search.setTextToShowWhenEmpty ("Search...", Colours::screenText.withAlpha (0.4f));
        search.setFont (font (13.0f));
        search.onTextChange = [this] { rebuild(); };
        addAndMakeVisible (search);
        list.setModel (this);
        list.setRowHeight (21);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff0e131c));
        addChildComponent (list);
        catLabel.setFont (font (14.0f, true));
        catLabel.setColour (juce::Label::textColourId, Colours::screenText);
        addChildComponent (catLabel);
        showGrid();
    }

    std::function<void()> onSave;

    void refresh()
    {
        const bool combi = proc.mode.load() == (int) Mode::Combi;
        const auto& cats = combi ? PresetManager::combiCategories() : PresetManager::programCategories();
        for (int i = 0; i < 16; ++i)
        {
            const int n = (int) (combi ? proc.presets.combisInCategory (cats[i]) : proc.presets.programsInCategory (cats[i])).size();
            tiles[i]->setButtonText (cats[i] + "\n" + juce::String (n));
        }
        if (list.isVisible()) rebuild();
    }

    void openCategory (int cat)
    {
        category = cat;
        for (auto* t : tiles) t->setVisible (false);
        userBtn.setVisible (false); initBtn.setVisible (false);
        list.setVisible (true); backBtn.setVisible (true); catLabel.setVisible (true);
        rebuild();
        resized();
    }

    void showGrid()
    {
        category = -1;
        search.clear();
        for (auto* t : tiles) t->setVisible (true);
        userBtn.setVisible (true); initBtn.setVisible (true);
        list.setVisible (false); backBtn.setVisible (false); catLabel.setVisible (false);
        refresh();
        resized();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6);
        auto bottom = r.removeFromBottom (26);
        r.removeFromBottom (4);
        saveBtn.setBounds (bottom.removeFromRight (70));
        bottom.removeFromRight (4);
        search.setBounds (bottom.removeFromRight (160));
        if (category < 0)
        {
            initBtn.setBounds (bottom.removeFromLeft (70));
            bottom.removeFromLeft (4);
            userBtn.setBounds (bottom.removeFromLeft (70));
            const int w = r.getWidth() / 4, h = r.getHeight() / 4;
            for (int i = 0; i < 16; ++i)
                tiles[i]->setBounds (r.getX() + (i % 4) * w, r.getY() + (i / 4) * h, w - 3, h - 3);
        }
        else
        {
            backBtn.setBounds (bottom.removeFromLeft (90));
            bottom.removeFromLeft (6);
            catLabel.setBounds (bottom);
            list.setBounds (r);
        }
    }

    // ListBoxModel
    int getNumRows() override { return (int) items.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (row < 0 || row >= (int) items.size()) return;
        const bool combi = proc.mode.load() == (int) Mode::Combi;
        const int idx = items[(size_t) row];
        const auto name = combi ? proc.presets.combis()[(size_t) idx].name : proc.presets.programs()[(size_t) idx].name;
        const auto cat = combi ? proc.presets.combis()[(size_t) idx].category : proc.presets.programs()[(size_t) idx].category;
        const bool current = name == proc.currentPresetName;
        g.setColour (current ? Colours::accent.withAlpha (0.9f) : selected ? Colours::screenAcc.withAlpha (0.5f)
                     : (row % 2 ? juce::Colour (0xff131a26) : juce::Colour (0xff0f151f)));
        g.fillRect (0, 0, w, h);
        g.setColour (current ? juce::Colours::black : Colours::screenText);
        g.setFont (font (13.0f, current));
        g.drawText (juce::String (idx).paddedLeft ('0', 3) + "  " + name, 8, 0, w - 140, h, juce::Justification::centredLeft);
        g.setFont (font (10.5f));
        g.setColour (current ? juce::Colours::black.withAlpha (0.7f) : Colours::screenText.withAlpha (0.45f));
        g.drawText (cat, w - 136, 0, 128, h, juce::Justification::centredRight);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        if (row < 0 || row >= (int) items.size()) return;
        const int idx = items[(size_t) row];
        if (proc.mode.load() == (int) Mode::Combi) proc.loadCombi (idx);
        else proc.loadProgram (idx);
        list.repaint();
    }

private:
    void rebuild()
    {
        items.clear();
        const bool combi = proc.mode.load() == (int) Mode::Combi;
        const auto& cats = combi ? PresetManager::combiCategories() : PresetManager::programCategories();
        const auto q = search.getText().trim();
        if (q.isNotEmpty())
        {
            const int n = combi ? (int) proc.presets.combis().size() : (int) proc.presets.programs().size();
            for (int i = 0; i < n; ++i)
            {
                const auto& name = combi ? proc.presets.combis()[(size_t) i].name : proc.presets.programs()[(size_t) i].name;
                if (name.containsIgnoreCase (q)) items.push_back (i);
            }
            catLabel.setText ("Search: " + q, juce::dontSendNotification);
            if (category < 0) { for (auto* t : tiles) t->setVisible (false); list.setVisible (true); backBtn.setVisible (true); catLabel.setVisible (true); category = 99; resized(); }
        }
        else if (category >= 0 && category < 16)
        {
            items = combi ? proc.presets.combisInCategory (cats[category]) : proc.presets.programsInCategory (cats[category]);
            catLabel.setText (cats[category] + "  (" + juce::String ((int) items.size()) + ")", juce::dontSendNotification);
        }
        else if (category == 16)
        {
            items = combi ? proc.presets.combisInCategory ("User") : proc.presets.programsInCategory ("User");
            catLabel.setText ("User (" + juce::String ((int) items.size()) + ")", juce::dontSendNotification);
        }
        list.updateContent();
        list.repaint();
    }

    RecklessChronosProcessor& proc;
    juce::OwnedArray<juce::TextButton> tiles;
    juce::TextButton userBtn { "USER" }, saveBtn { "SAVE" }, initBtn { "INIT" }, backBtn { "< BANKS" };
    juce::TextEditor search;
    juce::ListBox list;
    juce::Label catLabel;
    std::vector<int> items;
    int category = -1;

    friend class Screen;
};

//==============================================================================
class EditPage : public juce::Component
{
public:
    explicit EditPage (RecklessChronosProcessor& p) : proc (p)
    {
        const char* subs[] = { "OSC / ENGINE", "FILTER / AMP", "LFO / PITCH" };
        for (int i = 0; i < 3; ++i)
        {
            auto* b = subTabs.add (new juce::TextButton (subs[i]));
            styleScreenButton (*b);
            b->setRadioGroupId (77);
            b->setClickingTogglesState (true);
            b->onClick = [this, i] { setSub (i); };
            addAndMakeVisible (b);
        }
        subTabs[0]->setToggleState (true, juce::dontSendNotification);

        engine = std::make_unique<Choice> ("ENGINE");
        engine->onChange = [this] { updateMacroLabels(); };
        addAndMakeVisible (*engine);
        for (int i = 0; i < 8; ++i) addKnob (macros, "E" + juce::String (i + 1));
        for (auto* nm : { "UNISON", "DETUNE", "LEVEL", "PAN" }) addKnob (engineExtra, nm);

        fltType = std::make_unique<Choice> ("FILTER");
        addChildComponent (*fltType);
        for (auto* nm : { "CUTOFF", "RESO", "EG INT", "KEY TRK", "VEL", "DRIVE" }) addKnob (filterKnobs, nm);
        for (auto* nm : { "F.ATK", "F.DEC", "F.SUS", "F.REL", "A.ATK", "A.DEC", "A.SUS", "A.REL", "AMP VEL" }) addKnob (envKnobs, nm);

        lfoWave = std::make_unique<Choice> ("LFO WAVE");
        voiceMode = std::make_unique<Choice> ("VOICE");
        addChildComponent (*lfoWave);
        addChildComponent (*voiceMode);
        for (auto* nm : { "RATE", "DELAY", ">PITCH", ">FILTER", ">AMP", "WHL VIB", "WHL CUT", "AT VIB" }) addKnob (lfoKnobs, nm);
        for (auto* nm : { "GLIDE", "TRANSP", "FINE", "BEND" }) addKnob (pitchKnobs, nm);

        setSub (0);
    }

    void setTimbre (int t)
    {
        timbre = t;
        auto id = [t] (int tp) { return timbreParamId (t, tp); };
        auto& s = proc.apvts;
        engine->attach (s, id (tpEngine));
        for (int i = 0; i < 8; ++i) macros[i]->attach (s, id (tpE1 + i));
        const int ex[] = { tpUnison, tpDetune, tpLevel, tpPan };
        for (int i = 0; i < 4; ++i) engineExtra[i]->attach (s, id (ex[i]));
        fltType->attach (s, id (tpFltType));
        const int fk[] = { tpCutoff, tpReso, tpFltEnv, tpFltKey, tpFltVel, tpDrive };
        for (int i = 0; i < 6; ++i) filterKnobs[i]->attach (s, id (fk[i]));
        const int ek[] = { tpFA, tpFD, tpFS, tpFR, tpAA, tpAD, tpAS, tpAR, tpAmpVel };
        for (int i = 0; i < 9; ++i) envKnobs[i]->attach (s, id (ek[i]));
        lfoWave->attach (s, id (tpLfoWave));
        voiceMode->attach (s, id (tpVoiceMode));
        const int lk[] = { tpLfoRate, tpLfoDelay, tpLfoPitch, tpLfoFilter, tpLfoAmp, tpWheelVib, tpWheelCut, tpAtVib };
        for (int i = 0; i < 8; ++i) lfoKnobs[i]->attach (s, id (lk[i]));
        const int pk[] = { tpGlide, tpTranspose, tpFine, tpPbRange };
        for (int i = 0; i < 4; ++i) pitchKnobs[i]->attach (s, id (pk[i]));
        updateMacroLabels();
    }

    void updateMacroLabels()
    {
        const int eng = (int) proc.apvts.getRawParameterValue (timbreParamId (timbre, tpEngine))->load();
        const auto& labels = macroLabels (eng);
        for (int i = 0; i < 8; ++i)
        {
            macros[i]->setCaption (labels[(size_t) i]);
            macros[i]->setValueFormatter ([eng, i] (double v) { return macroText (eng, i, v); });
        }
        shownEngine = eng;
    }

    void poll()
    {
        const int eng = (int) proc.apvts.getRawParameterValue (timbreParamId (timbre, tpEngine))->load();
        if (eng != shownEngine) updateMacroLabels();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 4);
        auto tabsRow = r.removeFromTop (22);
        const int tw = tabsRow.getWidth() / 3;
        for (int i = 0; i < 3; ++i) subTabs[i]->setBounds (tabsRow.getX() + i * tw, tabsRow.getY(), tw - 3, 22);
        r.removeFromTop (6);
        const int rowH = 62;

        // ENGINE
        {
            auto a = r;
            auto top = a.removeFromTop (36);
            engine->setBounds (top.removeFromLeft (190));
            a.removeFromTop (4);
            std::vector<Knob*> m (macros.begin(), macros.end());
            layoutGrid (a.removeFromTop (rowH * 2), m, 4, rowH);
            std::vector<Knob*> e (engineExtra.begin(), engineExtra.end());
            layoutGrid (a.removeFromTop (rowH), e, 4, rowH);
        }
        // FILTER / AMP
        {
            auto a = r;
            auto top = a.removeFromTop (36);
            fltType->setBounds (top.removeFromLeft (150));
            std::vector<Knob*> f (filterKnobs.begin(), filterKnobs.end());
            layoutGrid (a.removeFromTop (rowH), f, 6, rowH);
            std::vector<Knob*> row1 (envKnobs.begin(), envKnobs.begin() + 4), row2 (envKnobs.begin() + 4, envKnobs.end());
            layoutGrid (a.removeFromTop (rowH), row1, 5, rowH);
            layoutGrid (a.removeFromTop (rowH), row2, 5, rowH);
        }
        // LFO / PITCH
        {
            auto a = r;
            auto top = a.removeFromTop (36);
            lfoWave->setBounds (top.removeFromLeft (130));
            top.removeFromLeft (16);
            voiceMode->setBounds (top.removeFromLeft (110));
            std::vector<Knob*> l (lfoKnobs.begin(), lfoKnobs.end());
            layoutGrid (a.removeFromTop (rowH * 2), l, 5, rowH);
            std::vector<Knob*> pk (pitchKnobs.begin(), pitchKnobs.end());
            layoutGrid (a.removeFromTop (rowH), pk, 5, rowH);
        }
    }

private:
    void addKnob (juce::OwnedArray<Knob>& arr, const juce::String& name)
    {
        auto* k = arr.add (new Knob (name, true));
        addChildComponent (k);
    }

    void setSub (int s)
    {
        sub = s;
        engine->setVisible (s == 0);
        for (auto* k : macros) k->setVisible (s == 0);
        for (auto* k : engineExtra) k->setVisible (s == 0);
        fltType->setVisible (s == 1);
        for (auto* k : filterKnobs) k->setVisible (s == 1);
        for (auto* k : envKnobs) k->setVisible (s == 1);
        lfoWave->setVisible (s == 2);
        voiceMode->setVisible (s == 2);
        for (auto* k : lfoKnobs) k->setVisible (s == 2);
        for (auto* k : pitchKnobs) k->setVisible (s == 2);
        subTabs[s]->setToggleState (true, juce::dontSendNotification);
    }

    RecklessChronosProcessor& proc;
    int timbre = 0, sub = 0, shownEngine = -1;
    juce::OwnedArray<juce::TextButton> subTabs;
    std::unique_ptr<Choice> engine, fltType, lfoWave, voiceMode;
    juce::OwnedArray<Knob> macros, engineExtra, filterKnobs, envKnobs, lfoKnobs, pitchKnobs;
};

//==============================================================================
class MixerPage : public juce::Component
{
public:
    explicit MixerPage (RecklessChronosProcessor& p) : proc (p)
    {
        for (int t = 0; t < kNumTimbres; ++t)
        {
            auto& c = strips[(size_t) t];
            c.on = std::make_unique<Toggle> ("T" + juce::String (t + 1) + " ON");
            c.vec = std::make_unique<Toggle> ("VEC");
            c.on->attach (proc.apvts, timbreParamId (t, tpOn));
            c.vec->attach (proc.apvts, timbreParamId (t, tpVector));
            c.prog.getProperties().set ("screen", true);
            c.prog.onClick = [this, t] { chooseProgram (t); };
            addAndMakeVisible (*c.on); addAndMakeVisible (*c.vec); addAndMakeVisible (c.prog);
            const char* names[] = { "LEVEL", "PAN", "TRANSP", "DLY SND", "REV SND", "KEY LO", "KEY HI", "VEL LO" };
            const int ids[] = { tpLevel, tpPan, tpTranspose, tpSend1, tpSend2, tpKeyLo, tpKeyHi, tpVelLo };
            for (int i = 0; i < 8; ++i)
            {
                auto* k = c.knobs.add (new Knob (names[i], true));
                k->attach (proc.apvts, timbreParamId (t, ids[i]));
                if (ids[i] == tpKeyLo || ids[i] == tpKeyHi) k->setValueFormatter (noteName);
                addAndMakeVisible (k);
            }
        }
        refresh();
    }

    void refresh()
    {
        const bool combi = proc.mode.load() == (int) Mode::Combi;
        for (int t = 0; t < kNumTimbres; ++t)
        {
            auto& c = strips[(size_t) t];
            c.prog.setButtonText (proc.presets.timbreNames[(size_t) t]);
            const bool enabled = combi || t == 0;
            c.on->setEnabled (combi);
            c.vec->setEnabled (combi);
            for (auto* k : c.knobs) k->setAlpha (enabled ? 1.0f : 0.35f);
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const int w = getWidth() / kNumTimbres;
        for (int t = 1; t < kNumTimbres; ++t)
        {
            g.setColour (juce::Colour (0xff2a3446));
            g.drawVerticalLine (t * w, 4.0f, (float) getHeight() - 4.0f);
        }
        if (proc.mode.load() != (int) Mode::Combi)
        {
            g.setColour (Colours::accent.withAlpha (0.85f));
            g.setFont (font (11.0f, true));
            g.drawText ("PROGRAM mode: assign a program to T2-T4 to build a Combi", getLocalBounds().removeFromBottom (14),
                        juce::Justification::centred);
        }
    }

    void resized() override
    {
        const int w = getWidth() / kNumTimbres;
        for (int t = 0; t < kNumTimbres; ++t)
        {
            auto r = juce::Rectangle<int> (t * w, 0, w, getHeight()).reduced (4, 4);
            auto& c = strips[(size_t) t];
            auto row = r.removeFromTop (20);
            c.on->setBounds (row.removeFromLeft (row.getWidth() * 3 / 5).reduced (1, 0));
            c.vec->setBounds (row.reduced (1, 0));
            r.removeFromTop (3);
            c.prog.setBounds (r.removeFromTop (22));
            r.removeFromTop (2);
            std::vector<Knob*> k (c.knobs.begin(), c.knobs.end());
            const int rowH = std::min (56, (r.getHeight() - 14) / 4);
            layoutGrid (r.removeFromTop (rowH * 4), k, 2, rowH);
        }
    }

private:
    void chooseProgram (int t)
    {
        juce::PopupMenu menu;
        const auto& cats = PresetManager::programCategories();
        for (int c = 0; c < cats.size(); ++c)
        {
            juce::PopupMenu sub;
            for (int i : proc.presets.programsInCategory (cats[c]))
                sub.addItem (i + 1, proc.presets.programs()[(size_t) i].name);
            menu.addSubMenu (cats[c], sub);
        }
        juce::PopupMenu user;
        for (int i : proc.presets.programsInCategory ("User"))
            user.addItem (i + 1, proc.presets.programs()[(size_t) i].name);
        menu.addSubMenu ("User", user);
        if (t > 0)
        {
            menu.addSeparator();
            menu.addItem (-1, "Off");
        }
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (strips[(size_t) t].prog),
                            [this, t] (int result)
                            {
                                if (result == 0) return;
                                if (result == -1)
                                {
                                    if (auto* prm = proc.apvts.getParameter (timbreParamId (t, tpOn))) prm->setValueNotifyingHost (0.0f);
                                    proc.presets.timbreNames[(size_t) t] = "---";
                                }
                                else
                                {
                                    if (t > 0 && proc.mode.load() != (int) Mode::Combi)
                                    {
                                        proc.setMode (Mode::Combi);
                                        proc.currentPresetName = "New Combi";
                                    }
                                    proc.presets.applyProgramToTimbre (result - 1, t, false);
                                    if (t == 0 && proc.mode.load() != (int) Mode::Combi)
                                        proc.currentPresetName = proc.presets.timbreNames[0];
                                }
                                if (proc.onPresetChanged) proc.onPresetChanged();
                            });
    }

    struct Strip
    {
        std::unique_ptr<Toggle> on, vec;
        juce::TextButton prog;
        juce::OwnedArray<Knob> knobs;
    };
    RecklessChronosProcessor& proc;
    std::array<Strip, kNumTimbres> strips;
};

//==============================================================================
class FxPage : public juce::Component
{
public:
    explicit FxPage (RecklessChronosProcessor& p) : proc (p)
    {
        for (int s = 0; s < 2; ++s)
        {
            type[s] = std::make_unique<Choice> ("IFX " + juce::String (s + 1));
            type[s]->onChange = [this] { updateLabels(); };
            addAndMakeVisible (*type[s]);
            for (auto* nm : { "A", "B", "MIX" })
            {
                auto* k = ifxKnobs[s].add (new Knob (nm, true));
                addAndMakeVisible (k);
            }
        }
        dlySync = std::make_unique<Choice> ("DELAY SYNC");
        dlySync->attach (proc.apvts, globalParamId (gpDlySync));
        addAndMakeVisible (*dlySync);
        const char* dn[] = { "TIME", "FDBK", "TONE", "PING", "RETURN" };
        const int di[] = { gpDlyTime, gpDlyFb, gpDlyTone, gpDlyPing, gpDlyRet };
        for (int i = 0; i < 5; ++i)
        {
            auto* k = dlyKnobs.add (new Knob (dn[i], true));
            k->attach (proc.apvts, globalParamId (di[i]));
            addAndMakeVisible (k);
        }
        const char* rn[] = { "SIZE", "DAMP", "PRE-DLY", "RETURN" };
        const int ri[] = { gpRevSize, gpRevDamp, gpRevPre, gpRevRet };
        for (int i = 0; i < 4; ++i)
        {
            auto* k = revKnobs.add (new Knob (rn[i], true));
            k->attach (proc.apvts, globalParamId (ri[i]));
            addAndMakeVisible (k);
        }
    }

    void setTimbre (int t)
    {
        timbre = t;
        const int ids[2][4] = { { tpIfx1Type, tpIfx1A, tpIfx1B, tpIfx1Mix }, { tpIfx2Type, tpIfx2A, tpIfx2B, tpIfx2Mix } };
        for (int s = 0; s < 2; ++s)
        {
            type[s]->attach (proc.apvts, timbreParamId (t, ids[s][0]));
            for (int k = 0; k < 3; ++k) ifxKnobs[s][k]->attach (proc.apvts, timbreParamId (t, ids[s][k + 1]));
        }
        updateLabels();
        repaint();
    }

    void updateLabels()
    {
        for (int s = 0; s < 2; ++s)
        {
            const auto l = fxParamLabels (type[s]->box.getSelectedItemIndex());
            ifxKnobs[s][0]->setCaption (l.first);
            ifxKnobs[s][1]->setCaption (l.second);
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff2a3446));
        g.drawVerticalLine (getWidth() / 2, 4.0f, (float) getHeight() - 4.0f);
        g.setColour (Colours::screenText.withAlpha (0.6f));
        g.setFont (font (11.0f, true));
        g.drawText ("INSERT FX  -  TIMBRE " + juce::String (timbre + 1), 8, 2, getWidth() / 2 - 16, 16, juce::Justification::centredLeft);
        g.drawText ("MASTER FX", getWidth() / 2 + 8, 2, getWidth() / 2 - 16, 16, juce::Justification::centredLeft);
        g.setColour (Colours::screenText.withAlpha (0.45f));
        g.drawText ("REVERB", getWidth() / 2 + 8, revTitleY, 100, 14, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 4);
        r.removeFromTop (18);
        auto left = r.removeFromLeft (r.getWidth() / 2).reduced (2, 0);
        auto right = r.reduced (6, 0);
        for (int s = 0; s < 2; ++s)
        {
            auto blk = left.removeFromTop (left.getHeight() / 2 - (s == 0 ? 0 : 0));
            type[s]->setBounds (blk.removeFromTop (36).removeFromLeft (170));
            std::vector<Knob*> k (ifxKnobs[s].begin(), ifxKnobs[s].end());
            layoutGrid (blk.removeFromTop (64), k, 3, 64);
        }
        dlySync->setBounds (right.removeFromTop (36).removeFromLeft (120));
        std::vector<Knob*> d (dlyKnobs.begin(), dlyKnobs.end());
        layoutGrid (right.removeFromTop (62), d, 5, 62);
        revTitleY = right.getY() + 4;
        right.removeFromTop (20);
        std::vector<Knob*> rv (revKnobs.begin(), revKnobs.end());
        layoutGrid (right.removeFromTop (62), rv, 4, 62);
    }

private:
    RecklessChronosProcessor& proc;
    int timbre = 0, revTitleY = 0;
    std::unique_ptr<Choice> type[2], dlySync;
    juce::OwnedArray<Knob> ifxKnobs[2], dlyKnobs, revKnobs;
};

//==============================================================================
class ArpPage : public juce::Component
{
public:
    explicit ArpPage (RecklessChronosProcessor& p) : proc (p)
    {
        on = std::make_unique<Toggle> ("ARPEGGIATOR ON");
        latch = std::make_unique<Toggle> ("LATCH");
        on->attach (proc.apvts, globalParamId (gpArpOn));
        latch->attach (proc.apvts, globalParamId (gpArpLatch));
        mode = std::make_unique<Choice> ("PATTERN");
        rate = std::make_unique<Choice> ("RATE");
        mode->attach (proc.apvts, globalParamId (gpArpMode));
        rate->attach (proc.apvts, globalParamId (gpArpRate));
        for (auto* c : { on.get(), latch.get() }) addAndMakeVisible (c);
        for (auto* c : { mode.get(), rate.get() }) addAndMakeVisible (c);
        const char* nm[] = { "OCTAVES", "GATE", "SWING" };
        const int ids[] = { gpArpOct, gpArpGate, gpArpSwing };
        for (int i = 0; i < 3; ++i)
        {
            auto* k = knobs.add (new Knob (nm[i], true));
            k->attach (proc.apvts, globalParamId (ids[i]));
            addAndMakeVisible (k);
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Colours::screenText.withAlpha (0.5f));
        g.setFont (font (11.0f));
        g.drawFittedText ("The arpeggiator follows the host tempo (120 BPM when standalone) and plays every active timbre.\n"
                          "Tip: the Hit/Arpg programs and Arpeggio combis switch it on automatically.",
                          getLocalBounds().removeFromBottom (44).reduced (10, 0), juce::Justification::centred, 3);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 8);
        auto row = r.removeFromTop (30);
        on->setBounds (row.removeFromLeft (200));
        row.removeFromLeft (10);
        latch->setBounds (row.removeFromLeft (100));
        r.removeFromTop (10);
        auto row2 = r.removeFromTop (38);
        mode->setBounds (row2.removeFromLeft (160));
        row2.removeFromLeft (16);
        rate->setBounds (row2.removeFromLeft (120));
        r.removeFromTop (8);
        std::vector<Knob*> k (knobs.begin(), knobs.end());
        layoutGrid (r.removeFromTop (70).removeFromLeft (300), k, 3, 70);
    }

private:
    RecklessChronosProcessor& proc;
    std::unique_ptr<Toggle> on, latch;
    std::unique_ptr<Choice> mode, rate;
    juce::OwnedArray<Knob> knobs;
};

//==============================================================================
Screen::Screen (RecklessChronosProcessor& p) : proc (p)
{
    play = std::make_unique<PlayPage> (p);
    edit = std::make_unique<EditPage> (p);
    mixer = std::make_unique<MixerPage> (p);
    fx = std::make_unique<FxPage> (p);
    arp = std::make_unique<ArpPage> (p);
    play->saveBtn.onClick = [this] { openSaveDialog(); };
    for (juce::Component* c : { (juce::Component*) play.get(), (juce::Component*) edit.get(), (juce::Component*) mixer.get(),
                                (juce::Component*) fx.get(), (juce::Component*) arp.get() })
        addChildComponent (c);

    const char* names[] = { "PLAY", "EDIT", "MIXER", "FX", "ARP" };
    for (int i = 0; i < 5; ++i)
    {
        auto* b = tabs.add (new juce::TextButton (names[i]));
        styleScreenButton (*b);
        b->setRadioGroupId (91);
        b->setClickingTogglesState (true);
        b->onClick = [this, i] { showPage (i); };
        addAndMakeVisible (b);
    }
    for (int t = 0; t < kNumTimbres; ++t)
    {
        auto* b = timbreButtons.add (new juce::TextButton ("T" + juce::String (t + 1)));
        styleScreenButton (*b);
        b->setRadioGroupId (92);
        b->setClickingTogglesState (true);
        b->onClick = [this, t] { setEditTimbre (t); };
        addAndMakeVisible (b);
    }
    for (auto* b : { &prevBtn, &nextBtn })
    {
        styleScreenButton (*b);
        b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff8a6a12));
        addAndMakeVisible (b);
    }
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };

    setEditTimbre (0);
    showPage (0);
    startTimerHz (12);
}

Screen::~Screen() = default;

int Screen::getEditTimbre() const { return proc.editTimbre; }

void Screen::setEditTimbre (int t)
{
    if (proc.mode.load() != (int) Mode::Combi) t = 0;
    proc.editTimbre = t;
    timbreButtons[t]->setToggleState (true, juce::dontSendNotification);
    edit->setTimbre (t);
    fx->setTimbre (t);
    if (onEditTimbreChanged) onEditTimbreChanged();
}

void Screen::showPage (int index)
{
    page = index;
    play->setVisible (index == 0);
    edit->setVisible (index == 1);
    mixer->setVisible (index == 2);
    fx->setVisible (index == 3);
    arp->setVisible (index == 4);
    tabs[index]->setToggleState (true, juce::dontSendNotification);
    if (index == 2) mixer->refresh();
}

void Screen::showCategory (int cat)
{
    showPage (0);
    play->openCategory (cat);
}

void Screen::stepPreset (int delta)
{
    const bool combi = proc.mode.load() == (int) Mode::Combi;
    const int n = combi ? (int) proc.presets.combis().size() : (int) proc.presets.programs().size();
    if (n == 0) return;
    int cur = -1;
    for (int i = 0; i < n; ++i)
    {
        const auto& name = combi ? proc.presets.combis()[(size_t) i].name : proc.presets.programs()[(size_t) i].name;
        if (name == proc.currentPresetName) { cur = i; break; }
    }
    const int next = ((cur < 0 ? 0 : cur + delta) % n + n) % n;
    if (combi) proc.loadCombi (next);
    else proc.loadProgram (next);
}

void Screen::refreshAll()
{
    if (proc.mode.load() != (int) Mode::Combi && proc.editTimbre != 0)
        setEditTimbre (0);
    play->refresh();
    mixer->refresh();
    edit->updateMacroLabels();
    shownName = {};
    repaint();
}

void Screen::openSaveDialog()
{
    const bool combi = proc.mode.load() == (int) Mode::Combi;
    saveWindow = std::make_unique<juce::AlertWindow> (combi ? "Save Combi" : "Save Program",
                                                      "Saved to your user preset folder.", juce::MessageBoxIconType::NoIcon, this);
    saveWindow->addTextEditor ("name", proc.currentPresetName, "Name:");
    saveWindow->addComboBox ("cat", combi ? PresetManager::combiCategories() : PresetManager::programCategories(), "Category:");
    saveWindow->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveWindow->enterModalState (true, juce::ModalCallbackFunction::create ([this, combi] (int result)
    {
        if (result == 1 && saveWindow != nullptr)
        {
            const auto name = saveWindow->getTextEditorContents ("name").trim();
            const auto cat = saveWindow->getComboBoxComponent ("cat")->getText();
            juce::String err;
            if (name.isNotEmpty() && proc.presets.saveCurrent (combi ? Mode::Combi : Mode::Program, name, cat, err))
            {
                proc.currentPresetName = name;
                play->refresh();
            }
            else if (err.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save failed", err);
        }
        saveWindow.reset();
        repaint();
    }), false);
}

void Screen::timerCallback()
{
    const int m = proc.mode.load();
    const int v = proc.voiceCount.load();
    if (proc.currentPresetName != shownName || m != shownMode || v != shownVoices)
    {
        if (m != shownMode)
        {
            shownMode = m;
            refreshAll();
        }
        shownName = proc.currentPresetName;
        shownVoices = v;
        repaint (headerArea.getUnion (nameArea));
    }
    edit->poll();
    for (int t = 0; t < kNumTimbres; ++t)
        timbreButtons[t]->setEnabled (m == (int) Mode::Combi || t == 0);
}

void Screen::mouseUp (const juce::MouseEvent&) {}

void Screen::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    juce::ColourGradient bg (Colours::screenBg2, 0, 0, Colours::screenBg, 0, r.getHeight(), false);
    g.setGradientFill (bg);
    g.fillRect (r);

    // header strip
    const bool combi = proc.mode.load() == (int) Mode::Combi;
    g.setColour (juce::Colour (0xff0a0d13));
    g.fillRect (headerArea);
    g.setColour (Colours::screenText.withAlpha (0.8f));
    g.setFont (font (11.0f, true));
    const char* pageNames[] = { "Play", "Edit", "Mixer", "Effects", "Arpeggiator" };
    g.drawText (juce::String (combi ? "COMBINATION" : "PROGRAM") + "  -  " + pageNames[page],
                headerArea.reduced (6, 0), juce::Justification::centredLeft);
    g.setColour (Colours::screenText.withAlpha (0.55f));
    g.drawText ("VOICES " + juce::String (shownVoices < 0 ? 0 : shownVoices),
                headerArea.withTrimmedRight (4 * 34 + 10), juce::Justification::centredRight);

    // name bar (amber, like the hardware's selected-program bar)
    auto nb = nameArea.toFloat().reduced (40.0f, 0.0f);
    g.setColour (Colours::accent);
    g.fillRect (nb);
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.setFont (font (10.0f, true));
    juce::String cat;
    if (combi) { for (auto& c : proc.presets.combis()) if (c.name == proc.currentPresetName) { cat = c.category; break; } }
    else { for (auto& p : proc.presets.programs()) if (p.name == proc.currentPresetName) { cat = p.category; break; } }
    g.drawText (cat.toUpperCase(), nb.reduced (8.0f, 2.0f), juce::Justification::topLeft);
    g.setColour (juce::Colours::black);
    g.setFont (font (20.0f, true));
    g.drawFittedText (proc.currentPresetName, nb.reduced (8.0f, 2.0f).withTrimmedTop (9.0f).toNearestInt(),
                      juce::Justification::centredLeft, 1);
    // tab bar background
    g.setColour (juce::Colour (0xff0a0d13));
    g.fillRect (tabArea);
}

void Screen::resized()
{
    auto r = getLocalBounds();
    headerArea = r.removeFromTop (22);
    nameArea = r.removeFromTop (38);
    tabArea = r.removeFromBottom (30);
    contentArea = r;

    auto hdr = headerArea;
    for (int t = kNumTimbres - 1; t >= 0; --t)
        timbreButtons[t]->setBounds (hdr.removeFromRight (34).reduced (2, 2));
    prevBtn.setBounds (nameArea.getX() + 4, nameArea.getY() + 3, 32, nameArea.getHeight() - 6);
    nextBtn.setBounds (nameArea.getRight() - 36, nameArea.getY() + 3, 32, nameArea.getHeight() - 6);

    auto tb = tabArea.reduced (4, 3);
    const int w = tb.getWidth() / 5;
    for (int i = 0; i < 5; ++i) tabs[i]->setBounds (tb.getX() + i * w, tb.getY(), w - 4, tb.getHeight());

    for (juce::Component* c : { (juce::Component*) play.get(), (juce::Component*) edit.get(), (juce::Component*) mixer.get(),
                                (juce::Component*) fx.get(), (juce::Component*) arp.get() })
        c->setBounds (contentArea);
}
} // namespace rc::ui
