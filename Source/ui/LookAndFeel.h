#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rc::ui
{
namespace Colours
{
    const juce::Colour panel      { 0xff1d1d20 };
    const juce::Colour panelLight { 0xff2a2a2e };
    const juce::Colour text       { 0xffd9d9d9 };
    const juce::Colour textDim    { 0xff8a8a8f };
    const juce::Colour accent     { 0xffe8b830 };   // amber highlights (name bar)
    const juce::Colour led        { 0xffff5a1f };
    const juce::Colour ledGreen   { 0xff3ce06a };
    const juce::Colour screenBg   { 0xff10141c };
    const juce::Colour screenBg2  { 0xff1a2130 };
    const juce::Colour screenText { 0xffe6ecf5 };
    const juce::Colour screenAcc  { 0xff3d8bff };
}

juce::Font font (float size, bool bold = false);

// Components flagged with getProperties().set ("screen", true) are drawn in the touch-screen style
class RcLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RcLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override;
};
} // namespace rc::ui
