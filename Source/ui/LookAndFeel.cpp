#include "LookAndFeel.h"

namespace rc::ui
{
juce::Font font (float size, bool bold)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

static bool isScreen (const juce::Component& c)
{
    return (bool) c.getProperties().getWithDefault ("screen", false);
}

RcLookAndFeel::RcLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Colours::screenText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Colours::text);
    setColour (juce::ComboBox::textColourId, Colours::screenText);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1b2230));
    setColour (juce::PopupMenu::textColourId, Colours::screenText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::screenAcc);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, Colours::accent);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0b0f16));
    setColour (juce::TextEditor::textColourId, Colours::screenText);
    setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff34405a));
    setColour (juce::TextEditor::focusedOutlineColourId, Colours::screenAcc);
    setColour (juce::CaretComponent::caretColourId, Colours::screenText);
    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff1b2230));
    setColour (juce::AlertWindow::textColourId, Colours::screenText);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b3448));
    setColour (juce::TextButton::textColourOffId, Colours::screenText);
}

void RcLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                      float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = start + pos * (end - start);

    if (isScreen (s))
    {
        // flat touch-screen knob: arc + value pointer
        const float arcR = r - 2.0f;
        juce::Path bg, val;
        bg.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, start, end, true);
        g.setColour (juce::Colour (0xff2c3648));
        g.strokePath (bg, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float from = bipolar ? (start + end) * 0.5f : start;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, std::min (from, angle), std::max (from, angle), true);
        g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId, true).isTransparent()
                         ? Colours::screenAcc : s.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xff222b3b));
        g.fillEllipse (c.x - arcR * 0.62f, c.y - arcR * 0.62f, arcR * 1.24f, arcR * 1.24f);
        juce::Path ptr;
        ptr.addRoundedRectangle (-1.2f, -arcR * 0.62f, 2.4f, arcR * 0.4f, 1.0f);
        g.setColour (Colours::screenText);
        g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c));
        return;
    }

    // hardware knob: knurled skirt + dark cap + white line
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (c.x - r + 1.5f, c.y - r + 3.0f, r * 2.0f, r * 2.0f);
    juce::ColourGradient skirt (juce::Colour (0xff3a3a3e), c.x, c.y - r, juce::Colour (0xff0d0d0f), c.x, c.y + r, false);
    g.setGradientFill (skirt);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colour (0xff050506));
    for (int i = 0; i < 36; ++i)
    {
        const float a = (float) i / 36.0f * juce::MathConstants<float>::twoPi;
        g.drawLine (c.x + std::sin (a) * r * 0.86f, c.y - std::cos (a) * r * 0.86f,
                    c.x + std::sin (a) * r, c.y - std::cos (a) * r, 1.0f);
    }
    const float cr = r * 0.72f;
    juce::ColourGradient cap (juce::Colour (0xff3c3c41), c.x - cr * 0.5f, c.y - cr, juce::Colour (0xff141416), c.x + cr * 0.4f, c.y + cr, false);
    g.setGradientFill (cap);
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f, 1.0f);
    juce::Path ptr;
    ptr.addRoundedRectangle (-1.3f, -r * 0.95f, 2.6f, r * 0.45f, 1.0f);
    g.setColour (juce::Colour (0xfff2f2f2));
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c));
    // scale ticks
    g.setColour (Colours::textDim);
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (float) i / 10.0f * (end - start);
        const float r1 = r + 2.0f, r2 = r + (i % 5 == 0 ? 5.0f : 3.5f);
        g.drawLine (c.x + std::sin (a) * r1, c.y - std::cos (a) * r1, c.x + std::sin (a) * r2, c.y - std::cos (a) * r2, 0.8f);
    }
}

void RcLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                      juce::Slider::SliderStyle style, juce::Slider& s)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    if (isScreen (s))
    {
        const bool vert = style == juce::Slider::LinearVertical;
        auto track = vert ? b.withSizeKeepingCentre (4.0f, b.getHeight()) : b.withSizeKeepingCentre (b.getWidth(), 4.0f);
        g.setColour (juce::Colour (0xff2c3648));
        g.fillRoundedRectangle (track, 2.0f);
        g.setColour (Colours::screenAcc);
        if (vert) g.fillRoundedRectangle (track.withTop (pos), 2.0f);
        else g.fillRoundedRectangle (track.withRight (pos), 2.0f);
        g.setColour (Colours::screenText);
        if (vert) g.fillRoundedRectangle (b.getCentreX() - 9.0f, pos - 4.0f, 18.0f, 8.0f, 2.0f);
        else g.fillRoundedRectangle (pos - 4.0f, b.getCentreY() - 8.0f, 8.0f, 16.0f, 2.0f);
        return;
    }

    // hardware fader: slot + cap
    const float cx = b.getCentreX();
    g.setColour (juce::Colour (0xff050506));
    g.fillRoundedRectangle (cx - 2.5f, b.getY(), 5.0f, b.getHeight(), 2.5f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (cx - 2.5f, b.getY(), 5.0f, b.getHeight(), 2.5f, 1.0f);
    // scale marks
    g.setColour (Colours::textDim.withAlpha (0.6f));
    for (int i = 0; i <= 10; ++i)
    {
        const float yy = b.getY() + b.getHeight() * (float) i / 10.0f;
        g.drawLine (cx - 12.0f, yy, cx - 7.0f, yy, 0.7f);
    }
    const float capW = std::min (b.getWidth() - 4.0f, 26.0f), capH = 34.0f;
    auto cap = juce::Rectangle<float> (cx - capW * 0.5f, pos - capH * 0.5f, capW, capH);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (cap.translated (1.0f, 3.0f), 3.0f);
    juce::ColourGradient grad (juce::Colour (0xff3b3b40), cap.getX(), cap.getY(), juce::Colour (0xff111113), cap.getX(), cap.getBottom(), false);
    grad.addColour (0.5, juce::Colour (0xff1c1c1f));
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.fillRect (cap.getX() + 3.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 6.0f, 2.0f);
}

void RcLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    if (isScreen (b))
    {
        auto base = b.findColour (juce::TextButton::buttonColourId);
        if (on) base = b.findColour (juce::TextButton::buttonOnColourId).isOpaque()
                           ? b.findColour (juce::TextButton::buttonOnColourId) : Colours::screenAcc;
        if (over) base = base.brighter (0.12f);
        if (down) base = base.darker (0.2f);
        g.setColour (base);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (on ? 0.35f : 0.1f));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        return;
    }
    // hardware push button with LED strip
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 3.0f);
    juce::ColourGradient grad (juce::Colour (down ? 0xff151517 : 0xff34343a), r.getX(), r.getY(),
                               juce::Colour (0xff0f0f11), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colours::white.withAlpha (over ? 0.25f : 0.1f));
    g.drawRoundedRectangle (r, 3.0f, 0.8f);
    if (b.getClickingTogglesState() || b.getProperties().contains ("led"))
    {
        const auto ledCol = b.getProperties().contains ("ledGreen") ? Colours::ledGreen : Colours::led;
        auto ledR = juce::Rectangle<float> (r.getCentreX() - 6.0f, r.getY() + 3.0f, 12.0f, 3.0f);
        g.setColour (on ? ledCol : juce::Colour (0xff3a2a22));
        g.fillRoundedRectangle (ledR, 1.5f);
        if (on)
        {
            g.setColour (ledCol.withAlpha (0.3f));
            g.fillRoundedRectangle (ledR.expanded (3.0f, 2.0f), 3.0f);
        }
    }
}

void RcLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool screen = isScreen (b);
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (screen ? (b.getToggleState() ? juce::Colours::white : Colours::screenText) : Colours::text);
    auto r = b.getLocalBounds().reduced (3, 1);
    if (! screen && (b.getClickingTogglesState() || b.getProperties().contains ("led")))
        r = r.withTrimmedTop (6);
    g.drawFittedText (b.getButtonText(), r, juce::Justification::centred, 2, 0.8f);
}

juce::Font RcLookAndFeel::getTextButtonFont (juce::TextButton& b, int h)
{
    return font (std::min (12.5f, (float) h * 0.45f), ! isScreen (b));
}

void RcLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    auto base = on ? Colours::screenAcc : juce::Colour (0xff2b3448);
    g.setColour (over ? base.brighter (0.1f) : base);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.4f : 0.1f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setColour (on ? juce::Colours::white : Colours::screenText);
    g.setFont (font (std::min (12.0f, r.getHeight() * 0.5f), true));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (2), juce::Justification::centred, 1);
}

void RcLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (juce::Colour (0xff222b3b));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.hasKeyboardFocus (false) ? Colours::screenAcc : juce::Colour (0xff3a4660));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (Colours::screenText.withAlpha (0.8f));
    g.fillPath (arrow);
}

juce::Font RcLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return font (std::min (13.0f, (float) box.getHeight() * 0.55f));
}

void RcLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font RcLookAndFeel::getPopupMenuFont() { return font (14.0f); }

void RcLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (juce::Colour (0xff3a4660));
    g.drawRect (0, 0, w, h);
}

juce::Label* RcLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (font (11.0f));
    l->setColour (juce::Label::textColourId, Colours::screenText);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

void RcLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                   int thumbStart, int thumbSize, bool over, bool)
{
    g.setColour (juce::Colour (0xff1a2130));
    g.fillRect (x, y, w, h);
    g.setColour (over ? Colours::screenAcc : juce::Colour (0xff3a4660));
    if (vertical) g.fillRoundedRectangle ((float) x + 2.0f, (float) thumbStart, (float) w - 4.0f, (float) thumbSize, 3.0f);
    else g.fillRoundedRectangle ((float) thumbStart, (float) y + 2.0f, (float) thumbSize, (float) h - 4.0f, 3.0f);
}
} // namespace rc::ui
