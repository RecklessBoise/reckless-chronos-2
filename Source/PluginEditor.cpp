#include "PluginEditor.h"

using namespace rc;
using namespace rc::ui;

namespace
{
const char* programBankShort[16] = { "KEYS", "ORGAN", "BELL", "STRINGS", "VOCAL", "BRASS", "WIND", "GUITAR",
                                     "BASS", "SLOW", "FAST", "LEAD", "MOTION", "SE", "ARPG", "DRUMS" };
const char* combiBankShort[16] = { "KEYS", "ORGAN", "BELL", "STRINGS", "PADS", "BRASS", "ORCH", "WORLD",
                                   "GUITAR", "SPLITS", "SYNTH", "LEAD", "MOTION", "SE/HIT", "ARPEG", "DRUMS" };

void panelButton (juce::Button& b, bool led = true)
{
    b.getProperties().set ("screen", false);
    if (led) b.getProperties().set ("led", true);
}
} // namespace

//==============================================================================
InstrumentBody::InstrumentBody (RecklessChronosProcessor& p)
    : proc (p), screen (p),
      vector (p.apvts, globalParamId (gpVecX), globalParamId (gpVecY)),
      stick (p.uiBend, p.uiMod),
      keyboard (p.keyboardState)
{
    addAndMakeVisible (screen);

    for (int i = 0; i < 8; ++i)
    {
        auto* k = rtKnobs.add (new Knob (realtimeLabels()[(size_t) i], false));
        k->attach (proc.apvts, globalParamId (gpRt1 + i));
        k->slider.setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (k);
    }
    for (int i = 0; i < 9; ++i)
    {
        auto* f = faders.add (new Fader (i == 8 ? "MASTER" : juce::String (i + 1)));
        addAndMakeVisible (f);
    }
    faders[8]->attach (proc.apvts, globalParamId (gpMaster));

    for (int t = 0; t < kNumTimbres; ++t)
    {
        auto* on = onButtons.add (new Toggle (juce::String (t + 1), false));
        on->button.getProperties().set ("ledGreen", true);
        on->attach (proc.apvts, timbreParamId (t, tpOn));
        addAndMakeVisible (on);
        auto* sel = selButtons.add (new juce::TextButton ("T" + juce::String (t + 1)));
        panelButton (*sel);
        sel->onClick = [this, t] { screen.setEditTimbre (t); };
        addAndMakeVisible (sel);
    }
    screen.onEditTimbreChanged = [this]
    {
        for (int t = 0; t < kNumTimbres; ++t) selButtons[t]->setToggleState (screen.getEditTimbre() == t, juce::dontSendNotification);
        bindFaders();
    };

    for (int i = 0; i < 16; ++i)
    {
        auto* b = bankButtons.add (new juce::TextButton());
        panelButton (*b, false);
        b->onClick = [this, i] { screen.showCategory (i); };
        addAndMakeVisible (b);
    }

    addAndMakeVisible (vector);
    for (auto* t : { &arpOn, &arpLatch }) { t->button.getProperties().set ("screen", false); addAndMakeVisible (*t); }
    arpOn.attach (proc.apvts, globalParamId (gpArpOn));
    arpLatch.attach (proc.apvts, globalParamId (gpArpLatch));

    for (auto* b : { &combiBtn, &progBtn }) { panelButton (*b); addAndMakeVisible (b); }
    for (auto* b : { &saveBtn, &panicBtn, &incBtn, &decBtn, &sizeBtn, &octDown, &octUp }) { panelButton (*b, false); addAndMakeVisible (b); }
    combiBtn.onClick = [this]
    {
        if (proc.mode.load() == (int) Mode::Combi) return;
        if (! proc.presets.combis().empty()) proc.loadCombi (0);
        else proc.setMode (Mode::Combi);
    };
    progBtn.onClick = [this]
    {
        if (proc.mode.load() == (int) Mode::Program) return;
        proc.loadProgram (std::max (0, proc.currentProgramIndex));
    };
    saveBtn.onClick = [this] { screen.openSaveDialog(); };
    panicBtn.onClick = [this] { proc.panic(); proc.keyboardState.allNotesOff (0); };
    incBtn.onClick = [this] { screen.stepPreset (1); };
    decBtn.onClick = [this] { screen.stepPreset (-1); };
    sizeBtn.onClick = [this] { showSizeMenu(); };
    octDown.onClick = [this] { octave = std::max (-2, octave - 1); keyboard.setAvailableRange (36 + octave * 12, 96 + octave * 12); repaint(); };
    octUp.onClick = [this] { octave = std::min (2, octave + 1); keyboard.setAvailableRange (36 + octave * 12, 96 + octave * 12); repaint(); };
    dial.onStep = [this] (int s) { screen.stepPreset (s); };
    addAndMakeVisible (dial);
    addAndMakeVisible (stick);
    addAndMakeVisible (keyboard);

    proc.onPresetChanged = [this] { refreshFromProcessor(); };
    bindFaders();
    refreshFromProcessor();
    startTimerHz (15);
}

InstrumentBody::~InstrumentBody()
{
    proc.onPresetChanged = nullptr;
}

void InstrumentBody::refreshFromProcessor()
{
    screen.refreshAll();
    bindFaders();
    updateBankLabels();
    repaint();
}

void InstrumentBody::bindFaders()
{
    const int t = screen.getEditTimbre();
    for (int i = 0; i < 8; ++i)
        faders[i]->attach (proc.apvts, timbreParamId (t, tpE1 + i));
    const int eng = (int) proc.apvts.getRawParameterValue (timbreParamId (t, tpEngine))->load();
    const auto& labels = macroLabels (eng);
    for (int i = 0; i < 8; ++i) faders[i]->setCaption (labels[(size_t) i]);
    shownEngine = eng;
}

void InstrumentBody::updateBankLabels()
{
    const bool combi = proc.mode.load() == (int) Mode::Combi;
    for (int i = 0; i < 16; ++i)
        bankButtons[i]->setButtonText (combi ? combiBankShort[i] : programBankShort[i]);
    combiBtn.setToggleState (combi, juce::dontSendNotification);
    progBtn.setToggleState (! combi, juce::dontSendNotification);
    shownMode = proc.mode.load();
}

void InstrumentBody::showSizeMenu()
{
    juce::PopupMenu m;
    const int sizes[] = { 50, 60, 70, 80, 90, 100, 125, 150, 175, 200 };
    for (int s : sizes)
        m.addItem (s, juce::String (s) + " %", true, std::abs (proc.uiScale * 100.0f - (float) s) < 1.0f);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (sizeBtn), [this] (int r)
    {
        if (r > 0 && onScaleChosen) onScaleChosen ((float) r / 100.0f);
    });
}

void InstrumentBody::timerCallback()
{
    const int t = screen.getEditTimbre();
    const int eng = (int) proc.apvts.getRawParameterValue (timbreParamId (t, tpEngine))->load();
    if (eng != shownEngine) bindFaders();
    if (proc.mode.load() != shownMode) updateBankLabels();
    const bool combi = proc.mode.load() == (int) Mode::Combi;
    for (int i = 0; i < kNumTimbres; ++i)
    {
        onButtons[i]->setEnabled (combi);
        selButtons[i]->setEnabled (combi || i == 0);
    }
    const float l = proc.meterL.load(), r = proc.meterR.load();
    if (std::abs (l - meterL) > 0.01f || std::abs (r - meterR) > 0.01f)
    {
        meterL = l; meterR = r;
        repaint (1222, 372, 160, 30);
    }
    sizeBtn.setButtonText ("SIZE " + juce::String ((int) std::round (proc.uiScale * 100.0f)) + "%");
}

void InstrumentBody::paint (juce::Graphics& g)
{
    const float W = (float) kWidth, H = (float) kHeight;
    g.fillAll (juce::Colour (0xff050506));

    // chassis
    g.setColour (juce::Colour (0xff0d0d0f));
    g.fillRoundedRectangle (36.0f, 20.0f, W - 72.0f, H - 28.0f, 6.0f);
    paintGrille (g, { 44.0f, 26.0f, W - 88.0f, 20.0f });
    paintBrushedPanel (g, { 44.0f, 46.0f, W - 88.0f, 416.0f });
    g.setColour (juce::Colours::black);
    g.fillRect (44.0f, 462.0f, W - 88.0f, H - 474.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine (462, 44.0f, W - 44.0f);

    // wood end cheeks
    paintWood (g, { 2.0f, 14.0f, 44.0f, H - 18.0f }, true);
    paintWood (g, { W - 46.0f, 14.0f, 44.0f, H - 18.0f }, false);

    // section labels
    paintPanelLabel (g, "VECTOR", { 64.0f, 56.0f, 120.0f, 14.0f });
    paintPanelLabel (g, "ARPEGGIATOR", { 60.0f, 208.0f, 128.0f, 14.0f }, 9.0f);
    paintPanelLabel (g, "REALTIME CONTROLS", { 210.0f, 52.0f, 416.0f, 14.0f });
    paintPanelLabel (g, "TIMBRE ON", { 210.0f, 140.0f, 208.0f, 12.0f }, 9.0f);
    paintPanelLabel (g, "EDIT TIMBRE", { 418.0f, 140.0f, 208.0f, 12.0f }, 9.0f);
    paintPanelLabel (g, "TONE ADJUST  /  ENGINE MACROS", { 210.0f, 190.0f, 376.0f, 12.0f }, 9.0f);
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawHorizontalLine (184, 210.0f, 632.0f);
    g.drawVerticalLine (198, 60.0f, 450.0f);

    // help text in the left column
    g.setColour (Colours::textDim);
    g.setFont (font (9.0f));
    g.drawFittedText ("Double-click a realtime knob to reset it.\nDrag the vector stick to blend timbres A-D in a Combi, "
                      "or to sweep cutoff / EG in a Program.", juce::Rectangle<int> (58, 300, 134, 110),
                      juce::Justification::topLeft, 8);

    // screen bezel
    auto bezel = juce::Rectangle<float> (642.0f, 54.0f, 556.0f, 398.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (bezel.translated (0.0f, 3.0f), 6.0f);
    juce::ColourGradient bz (juce::Colour (0xff2e2e33), bezel.getX(), bezel.getY(), juce::Colour (0xff0a0a0c), bezel.getX(), bezel.getBottom(), false);
    g.setGradientFill (bz);
    g.fillRoundedRectangle (bezel, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (bezel, 6.0f, 1.0f);
    g.setColour (Colours::textDim);
    g.setFont (font (8.5f, true));
    g.drawText ("CHRONO-VIEW", juce::Rectangle<float> (652.0f, 440.0f, 120.0f, 10.0f), juce::Justification::centredLeft);

    // branding (top right)
    g.setColour (juce::Colour (0xffe9e9e9));
    g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold)).withExtraKerningFactor (0.08f));
    g.drawText ("CHRONOS 2", juce::Rectangle<float> (1340.0f, 58.0f, 190.0f, 34.0f), juce::Justification::centredLeft);
    g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold | juce::Font::italic)).withExtraKerningFactor (0.05f));
    g.drawText ("RECKLESS", juce::Rectangle<float> (1520.0f, 60.0f, 110.0f, 30.0f), juce::Justification::centredRight);
    g.setColour (Colours::textDim);
    g.setFont (font (8.5f, true));
    g.drawText ("MUSIC WORKSTATION  -  10 SYNTHESIS ENGINES", juce::Rectangle<float> (1340.0f, 90.0f, 290.0f, 12.0f), juce::Justification::centredLeft);

    paintPanelLabel (g, "MODE", { 1222.0f, 112.0f, 130.0f, 12.0f }, 9.0f, juce::Justification::centredLeft);
    paintPanelLabel (g, "VALUE", { 1236.0f, 166.0f, 100.0f, 12.0f }, 9.0f);
    paintPanelLabel (g, "BANK  /  CATEGORY", { 1392.0f, 166.0f, 236.0f, 12.0f }, 9.0f);
    paintPanelLabel (g, "OUTPUT", { 1222.0f, 360.0f, 160.0f, 12.0f }, 9.0f, juce::Justification::centredLeft);

    // output meter
    auto meter = [&] (float v, float y)
    {
        const int leds = 16;
        const float db = juce::Decibels::gainToDecibels (v, -60.0f);
        const int lit = (int) std::round (juce::jmap (db, -48.0f, 0.0f, 0.0f, (float) leds));
        for (int i = 0; i < leds; ++i)
        {
            const auto col = i >= 14 ? juce::Colour (0xffff3b30) : i >= 11 ? juce::Colour (0xffffc83d) : Colours::ledGreen;
            g.setColour (i < lit ? col : col.withAlpha (0.12f));
            g.fillRoundedRectangle (1222.0f + (float) i * 10.0f, y, 8.0f, 6.0f, 1.5f);
        }
    };
    meter (meterL, 376.0f);
    meter (meterR, 386.0f);

    // keybed labels
    paintPanelLabel (g, "JOYSTICK", { 70.0f, 476.0f, 120.0f, 12.0f }, 9.0f);
    paintPanelLabel (g, "OCTAVE " + juce::String (octave >= 0 ? "+" : "") + juce::String (octave), { 60.0f, 626.0f, 140.0f, 12.0f }, 9.0f);
    // headphone jack detail
    g.setColour (juce::Colour (0xff1a1a1d));
    g.fillEllipse (52.0f, 668.0f, 12.0f, 12.0f);
    g.setColour (juce::Colours::black);
    g.fillEllipse (55.0f, 671.0f, 6.0f, 6.0f);
}

void InstrumentBody::resized()
{
    screen.setBounds (654, 66, 532, 374);
    vector.setBounds (64, 72, 120, 120);
    arpOn.setBounds (64, 224, 56, 34);
    arpLatch.setBounds (128, 224, 56, 34);

    for (int i = 0; i < 8; ++i) rtKnobs[i]->setBounds (210 + i * 52, 70, 52, 64);
    for (int t = 0; t < kNumTimbres; ++t)
    {
        onButtons[t]->setBounds (214 + t * 52, 154, 44, 26);
        selButtons[t]->setBounds (214 + (t + 4) * 52, 154, 44, 26);
    }
    for (int i = 0; i < 9; ++i) faders[i]->setBounds (208 + i * 47, 206, 46, 246);

    combiBtn.setBounds (1222, 126, 62, 32);
    progBtn.setBounds (1290, 126, 62, 32);
    saveBtn.setBounds (1392, 126, 62, 32);
    panicBtn.setBounds (1460, 126, 62, 32);
    sizeBtn.setBounds (1528, 126, 100, 32);
    dial.setBounds (1236, 182, 100, 100);
    incBtn.setBounds (1236, 292, 46, 26);
    decBtn.setBounds (1290, 292, 46, 26);
    for (int i = 0; i < 16; ++i)
        bankButtons[i]->setBounds (1392 + (i % 4) * 59, 182 + (i / 4) * 34, 56, 30);

    stick.setBounds (80, 490, 100, 100);
    octDown.setBounds (66, 642, 60, 26);
    octUp.setBounds (134, 642, 60, 26);
    keyboard.setBounds (216, 470, kWidth - 216 - 52, 214);
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 36.0f);
}

//==============================================================================
RecklessChronosEditor::RecklessChronosEditor (RecklessChronosProcessor& p)
    : AudioProcessorEditor (p), proc (p), body (p)
{
    const float initialScale = juce::jlimit (0.5f, 2.0f, proc.uiScale);
    setLookAndFeel (&lnf);
    addAndMakeVisible (body);
    body.setSize (InstrumentBody::kWidth, InstrumentBody::kHeight);
    body.onScaleChosen = [this] (float s)
    {
        setSize ((int) std::round (InstrumentBody::kWidth * s), (int) std::round (InstrumentBody::kHeight * s));
    };

    setResizable (true, true);
    setResizeLimits (InstrumentBody::kWidth / 2, InstrumentBody::kHeight / 2, InstrumentBody::kWidth * 2, InstrumentBody::kHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) InstrumentBody::kWidth / (double) InstrumentBody::kHeight);
    setSize ((int) std::round (InstrumentBody::kWidth * initialScale), (int) std::round (InstrumentBody::kHeight * initialScale));
    sizeReady = true;
    resized();
}

RecklessChronosEditor::~RecklessChronosEditor()
{
    setLookAndFeel (nullptr);
}

void RecklessChronosEditor::resized()
{
    const float s = (float) getWidth() / (float) InstrumentBody::kWidth;
    body.setTransform (juce::AffineTransform::scale (s));
    if (sizeReady)
        proc.uiScale = s;
}
