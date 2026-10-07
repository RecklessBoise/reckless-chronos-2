#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "LookAndFeel.h"

namespace rc::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

// Rotary knob + caption, re-attachable to any parameter
class Knob : public juce::Component
{
public:
    Knob (const juce::String& caption = {}, bool screenStyle = true);
    void attach (APVTS& state, const juce::String& paramId);
    void detach();
    void setCaption (const juce::String& c) { caption.setText (c, juce::dontSendNotification); }
    void setValueFormatter (std::function<juce::String (double)> f);
    void resized() override;
    juce::Slider slider;

private:
    juce::Label caption;
    std::unique_ptr<APVTS::SliderAttachment> att;
    bool screen;
};

// Hardware style vertical fader with caption underneath
class Fader : public juce::Component
{
public:
    explicit Fader (const juce::String& caption = {});
    void attach (APVTS& state, const juce::String& paramId);
    void setCaption (const juce::String& c) { caption.setText (c, juce::dontSendNotification); }
    void resized() override;
    juce::Slider slider;

private:
    juce::Label caption;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// Screen combo box bound to a choice parameter
class Choice : public juce::Component
{
public:
    explicit Choice (const juce::String& caption = {});
    void attach (APVTS& state, const juce::String& paramId);
    void resized() override;
    juce::ComboBox box;
    std::function<void()> onChange;

private:
    juce::Label caption;
    std::unique_ptr<APVTS::ComboBoxAttachment> att;
};

// Toggle bound to a bool parameter
class Toggle : public juce::Component
{
public:
    explicit Toggle (const juce::String& text, bool screenStyle = true);
    void attach (APVTS& state, const juce::String& paramId);
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::TextButton button;

private:
    std::unique_ptr<APVTS::ButtonAttachment> att;
};

// Two-parameter joystick (vector)
class VectorPad : public juce::Component
{
public:
    VectorPad (APVTS& state, const juce::String& xId, const juce::String& yId);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void setFromMouse (juce::Point<float>);
    juce::ParameterAttachment xAtt, yAtt;
    float x = 0.0f, y = 0.0f;
};

// Spring-loaded pitch (X) / mod (Y) joystick
class PitchModStick : public juce::Component
{
public:
    PitchModStick (std::atomic<float>& bend, std::atomic<float>& mod) : bendRef (bend), modRef (mod) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { mouseDrag (e); }
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    std::atomic<float>& bendRef;
    std::atomic<float>& modRef;
    float px = 0.0f, py = 0.0f;
};

// Endless encoder: dragging emits +/- steps
class ValueDial : public juce::Component
{
public:
    std::function<void (int)> onStep;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    float angle = 0.0f, accum = 0.0f;
    int lastY = 0;
};

class Keyboard : public juce::MidiKeyboardComponent
{
public:
    Keyboard (juce::MidiKeyboardState& s);

private:
    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour noteFillColour) override;
};

// Painting helpers
void paintWood (juce::Graphics&, juce::Rectangle<float> r, bool leftSide);
void paintBrushedPanel (juce::Graphics&, juce::Rectangle<float> r);
void paintGrille (juce::Graphics&, juce::Rectangle<float> r);
void paintPanelLabel (juce::Graphics&, const juce::String&, juce::Rectangle<float> r, float size = 10.0f,
                      juce::Justification j = juce::Justification::centred);
} // namespace rc::ui
