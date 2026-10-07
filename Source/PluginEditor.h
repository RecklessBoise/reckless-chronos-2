#pragma once

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/Widgets.h"
#include "ui/Screen.h"

// The whole instrument, drawn at a fixed design size and scaled by the editor.
class InstrumentBody : public juce::Component, private juce::Timer
{
public:
    static constexpr int kWidth = 1680;
    static constexpr int kHeight = 700;

    explicit InstrumentBody (RecklessChronosProcessor&);
    ~InstrumentBody() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (float)> onScaleChosen;
    void refreshFromProcessor();

private:
    void timerCallback() override;
    void bindFaders();
    void updateBankLabels();
    void showSizeMenu();

    RecklessChronosProcessor& proc;
    rc::ui::Screen screen;
    juce::OwnedArray<rc::ui::Knob> rtKnobs;
    juce::OwnedArray<rc::ui::Fader> faders;
    juce::OwnedArray<rc::ui::Toggle> onButtons;
    juce::OwnedArray<juce::TextButton> selButtons, bankButtons;
    rc::ui::VectorPad vector;
    rc::ui::Toggle arpOn { "ARP ON", false }, arpLatch { "LATCH", false };
    juce::TextButton combiBtn { "COMBI" }, progBtn { "PROG" }, saveBtn { "SAVE" }, panicBtn { "PANIC" },
                     incBtn { "INC" }, decBtn { "DEC" }, sizeBtn { "SIZE" }, octDown { "OCT -" }, octUp { "OCT +" };
    rc::ui::ValueDial dial;
    rc::ui::PitchModStick stick;
    rc::ui::Keyboard keyboard;
    int shownEngine = -1, shownMode = -1, octave = 0;
    float meterL = 0.0f, meterR = 0.0f;
};

class RecklessChronosEditor : public juce::AudioProcessorEditor
{
public:
    explicit RecklessChronosEditor (RecklessChronosProcessor&);
    ~RecklessChronosEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black); }

private:
    RecklessChronosProcessor& proc;
    rc::ui::RcLookAndFeel lnf;
    InstrumentBody body;
    bool sizeReady = false;
};
