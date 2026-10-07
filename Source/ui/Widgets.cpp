#include "Widgets.h"

namespace rc::ui
{
//==============================================================================
Knob::Knob (const juce::String& c, bool screenStyle) : screen (screenStyle)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (screen ? juce::Slider::TextBoxBelow : juce::Slider::NoTextBox, false, 64, 14);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, Colours::screenText);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.getProperties().set ("screen", screen);
    slider.setPopupDisplayEnabled (! screen, false, nullptr);
    addAndMakeVisible (slider);
    caption.setText (c, juce::dontSendNotification);
    caption.setJustificationType (juce::Justification::centred);
    caption.setFont (font (screen ? 10.5f : 9.5f, true));
    caption.setColour (juce::Label::textColourId, screen ? Colours::screenText.withAlpha (0.75f) : Colours::text);
    caption.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (caption);
}

void Knob::attach (APVTS& state, const juce::String& id)
{
    att.reset();
    att = std::make_unique<APVTS::SliderAttachment> (state, id, slider);
}

void Knob::detach() { att.reset(); }

void Knob::setValueFormatter (std::function<juce::String (double)> f)
{
    slider.textFromValueFunction = std::move (f);
    slider.updateText();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    caption.setBounds (r.removeFromTop (14));
    slider.setBounds (r);
}

//==============================================================================
Fader::Fader (const juce::String& c)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled (true, false, nullptr);
    addAndMakeVisible (slider);
    caption.setText (c, juce::dontSendNotification);
    caption.setJustificationType (juce::Justification::centredTop);
    caption.setFont (font (9.0f, true));
    caption.setMinimumHorizontalScale (0.6f);
    caption.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (caption);
}

void Fader::attach (APVTS& state, const juce::String& id)
{
    att.reset();
    att = std::make_unique<APVTS::SliderAttachment> (state, id, slider);
}

void Fader::resized()
{
    auto r = getLocalBounds();
    caption.setBounds (r.removeFromBottom (24));
    slider.setBounds (r);
}

//==============================================================================
Choice::Choice (const juce::String& c)
{
    box.getProperties().set ("screen", true);
    box.onChange = [this] { if (onChange) onChange(); };
    addAndMakeVisible (box);
    caption.setText (c, juce::dontSendNotification);
    caption.setFont (font (10.5f, true));
    caption.setColour (juce::Label::textColourId, Colours::screenText.withAlpha (0.75f));
    caption.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (caption);
}

void Choice::attach (APVTS& state, const juce::String& id)
{
    att.reset();
    box.clear (juce::dontSendNotification);
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id)))
        box.addItemList (p->choices, 1);
    att = std::make_unique<APVTS::ComboBoxAttachment> (state, id, box);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    if (caption.getText().isNotEmpty())
        caption.setBounds (r.removeFromTop (14));
    box.setBounds (r);
}

//==============================================================================
Toggle::Toggle (const juce::String& text, bool screenStyle)
{
    button.setButtonText (text);
    button.setClickingTogglesState (true);
    button.getProperties().set ("screen", screenStyle);
    addAndMakeVisible (button);
}

void Toggle::attach (APVTS& state, const juce::String& id)
{
    att.reset();
    att = std::make_unique<APVTS::ButtonAttachment> (state, id, button);
}

//==============================================================================
VectorPad::VectorPad (APVTS& state, const juce::String& xId, const juce::String& yId)
    : xAtt (*state.getParameter (xId), [this] (float v) { x = v; repaint(); }),
      yAtt (*state.getParameter (yId), [this] (float v) { y = v; repaint(); })
{
    xAtt.sendInitialUpdate();
    yAtt.sendInitialUpdate();
}

void VectorPad::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (juce::Colour (0xff0b0b0d));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    // diamond guide with A/B/C/D corners
    const auto c = r.getCentre();
    const float hw = r.getWidth() * 0.4f, hh = r.getHeight() * 0.4f;
    juce::Path d;
    d.startNewSubPath (c.x - hw, c.y); d.lineTo (c.x, c.y - hh); d.lineTo (c.x + hw, c.y); d.lineTo (c.x, c.y + hh);
    d.closeSubPath();
    g.setColour (Colours::textDim.withAlpha (0.4f));
    g.strokePath (d, juce::PathStrokeType (1.0f));
    g.setFont (font (9.0f, true));
    g.drawText ("A", juce::Rectangle<float> (r.getX() + 2, c.y - 6, 10, 12), juce::Justification::centred);
    g.drawText ("B", juce::Rectangle<float> (c.x - 5, r.getY() + 1, 10, 12), juce::Justification::centred);
    g.drawText ("C", juce::Rectangle<float> (r.getRight() - 12, c.y - 6, 10, 12), juce::Justification::centred);
    g.drawText ("D", juce::Rectangle<float> (c.x - 5, r.getBottom() - 13, 10, 12), juce::Justification::centred);
    // stick
    const float px = c.x + x * hw, py = c.y - y * hh;
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawLine (c.x, c.y, px, py, 5.0f);
    juce::ColourGradient knob (juce::Colour (0xff4a4a50), px - 6, py - 8, juce::Colour (0xff101012), px + 6, py + 8, false);
    g.setGradientFill (knob);
    g.fillEllipse (px - 9.0f, py - 9.0f, 18.0f, 18.0f);
    g.setColour (Colours::led.withAlpha (0.8f));
    g.fillEllipse (px - 2.0f, py - 2.0f, 4.0f, 4.0f);
}

void VectorPad::setFromMouse (juce::Point<float> p)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const auto c = r.getCentre();
    const float nx = juce::jlimit (-1.0f, 1.0f, (p.x - c.x) / (r.getWidth() * 0.4f));
    const float ny = juce::jlimit (-1.0f, 1.0f, -(p.y - c.y) / (r.getHeight() * 0.4f));
    xAtt.setValueAsPartOfGesture (nx);
    yAtt.setValueAsPartOfGesture (ny);
}

void VectorPad::mouseDown (const juce::MouseEvent& e)
{
    xAtt.beginGesture(); yAtt.beginGesture();
    setFromMouse (e.position);
}

void VectorPad::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e.position); }

void VectorPad::mouseDoubleClick (const juce::MouseEvent&)
{
    xAtt.setValueAsCompleteGesture (0.0f);
    yAtt.setValueAsCompleteGesture (0.0f);
}

//==============================================================================
void PitchModStick::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const auto c = r.getCentre();
    const float rad = std::min (r.getWidth(), r.getHeight()) * 0.5f - 2.0f;
    juce::ColourGradient base (juce::Colour (0xff2b2b30), c.x, c.y - rad, juce::Colour (0xff050506), c.x, c.y + rad, false);
    g.setGradientFill (base);
    g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
    g.setColour (juce::Colours::black);
    g.fillEllipse (c.x - rad * 0.55f, c.y - rad * 0.55f, rad * 1.1f, rad * 1.1f);
    const float kx = c.x + px * rad * 0.45f, ky = c.y - py * rad * 0.45f;
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (kx - rad * 0.38f + 2, ky - rad * 0.38f + 4, rad * 0.76f, rad * 0.76f);
    juce::ColourGradient knob (juce::Colour (0xff55555c), kx - rad * 0.2f, ky - rad * 0.3f, juce::Colour (0xff0e0e10), kx + rad * 0.2f, ky + rad * 0.35f, false);
    g.setGradientFill (knob);
    g.fillEllipse (kx - rad * 0.38f, ky - rad * 0.38f, rad * 0.76f, rad * 0.76f);
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawEllipse (kx - rad * 0.38f, ky - rad * 0.38f, rad * 0.76f, rad * 0.76f, 1.0f);
}

void PitchModStick::mouseDrag (const juce::MouseEvent& e)
{
    auto r = getLocalBounds().toFloat();
    const auto c = r.getCentre();
    const float rad = std::min (r.getWidth(), r.getHeight()) * 0.5f;
    px = juce::jlimit (-1.0f, 1.0f, (e.position.x - c.x) / (rad * 0.7f));
    py = juce::jlimit (0.0f, 1.0f, -(e.position.y - c.y) / (rad * 0.7f));
    bendRef.store (px);
    modRef.store (py);
    repaint();
}

void PitchModStick::mouseUp (const juce::MouseEvent&)
{
    px = 0.0f; // pitch springs back, modulation latches
    bendRef.store (0.0f);
    repaint();
}

//==============================================================================
void ValueDial::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    const auto c = r.getCentre();
    const float rad = std::min (r.getWidth(), r.getHeight()) * 0.5f;
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (c.x - rad + 2, c.y - rad + 4, rad * 2, rad * 2);
    juce::ColourGradient grad (juce::Colour (0xff3a3a40), c.x, c.y - rad, juce::Colour (0xff0a0a0b), c.x, c.y + rad, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
    g.setColour (juce::Colour (0xff060607));
    for (int i = 0; i < 60; ++i)
    {
        const float a = angle + (float) i / 60.0f * juce::MathConstants<float>::twoPi;
        g.drawLine (c.x + std::sin (a) * rad * 0.9f, c.y - std::cos (a) * rad * 0.9f,
                    c.x + std::sin (a) * rad, c.y - std::cos (a) * rad, 1.2f);
    }
    juce::ColourGradient cap (juce::Colour (0xff2e2e33), c.x - rad * 0.3f, c.y - rad * 0.6f, juce::Colour (0xff111113), c.x, c.y + rad * 0.7f, false);
    g.setGradientFill (cap);
    g.fillEllipse (c.x - rad * 0.8f, c.y - rad * 0.8f, rad * 1.6f, rad * 1.6f);
    // finger dimple
    const float dx = c.x + std::sin (angle) * rad * 0.5f, dy = c.y - std::cos (angle) * rad * 0.5f;
    g.setColour (juce::Colour (0xff070708));
    g.fillEllipse (dx - rad * 0.14f, dy - rad * 0.14f, rad * 0.28f, rad * 0.28f);
}

void ValueDial::mouseDown (const juce::MouseEvent& e) { lastY = e.getPosition().y; accum = 0.0f; }

void ValueDial::mouseDrag (const juce::MouseEvent& e)
{
    const int dy = lastY - e.getPosition().y;
    lastY = e.getPosition().y;
    accum += (float) dy;
    angle += (float) dy * 0.04f;
    while (std::abs (accum) >= 14.0f)
    {
        const int s = accum > 0 ? 1 : -1;
        accum -= 14.0f * (float) s;
        if (onStep) onStep (s);
    }
    repaint();
}

void ValueDial::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    accum += w.deltaY * 60.0f;
    angle += w.deltaY * 1.5f;
    while (std::abs (accum) >= 14.0f)
    {
        const int s = accum > 0 ? 1 : -1;
        accum -= 14.0f * (float) s;
        if (onStep) onStep (s);
    }
    repaint();
}

//==============================================================================
Keyboard::Keyboard (juce::MidiKeyboardState& s)
    : juce::MidiKeyboardComponent (s, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setAvailableRange (36, 96);
    setScrollButtonsVisible (false);
    setOctaveForMiddleC (4);
    setBlackNoteLengthProportion (0.62f);
    setBlackNoteWidthProportion (0.6f);
    setMidiChannel (1);
    setVelocity (0.8f, true);
    setWantsKeyboardFocus (false);
}

void Keyboard::drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                              juce::Colour, juce::Colour)
{
    auto r = area.reduced (0.6f, 0.0f);
    juce::ColourGradient grad (juce::Colour (isDown ? 0xffd8d4cc : 0xfff8f6f0), r.getX(), r.getY(),
                               juce::Colour (isDown ? 0xffc4c0b8 : 0xffe6e2da), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r.withTrimmedTop (-4.0f), 3.0f);
    if (isOver) { g.setColour (juce::Colours::black.withAlpha (0.04f)); g.fillRect (r); }
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawLine (area.getRight(), area.getY(), area.getRight(), area.getBottom(), 1.0f);
    // front lip shadow
    g.setColour (juce::Colours::black.withAlpha (isDown ? 0.18f : 0.08f));
    g.fillRect (r.withTop (r.getBottom() - 6.0f));
    if (note % 12 == 0)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.setFont (font (9.0f));
        g.drawText ("C" + juce::String (note / 12 - 1), r.withTop (r.getBottom() - 20.0f), juce::Justification::centred);
    }
}

void Keyboard::drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    auto r = area;
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.translated (1.5f, 2.0f), 2.0f);
    juce::ColourGradient grad (juce::Colour (isDown ? 0xff101012 : 0xff2a2a2e), r.getX(), r.getY(),
                               juce::Colour (0xff050506), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 2.0f);
    // top bevel
    g.setColour (juce::Colours::white.withAlpha (isOver ? 0.25f : 0.14f));
    g.fillRoundedRectangle (r.reduced (r.getWidth() * 0.18f, 0.0f).withTrimmedBottom (isDown ? 6.0f : 10.0f).withTrimmedTop (2.0f), 1.5f);
}

//==============================================================================
juce::Path heartPath (juce::Rectangle<float> r)
{
    const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
    juce::Path p;
    p.startNewSubPath (x + w * 0.5f, y + h);
    p.cubicTo (x - w * 0.15f, y + h * 0.55f, x + w * 0.05f, y - h * 0.1f, x + w * 0.5f, y + h * 0.25f);
    p.cubicTo (x + w * 0.95f, y - h * 0.1f, x + w * 1.15f, y + h * 0.55f, x + w * 0.5f, y + h);
    p.closeSubPath();
    return p;
}

void paintHeart (juce::Graphics& g, juce::Rectangle<float> r, bool filled, juce::Colour colour)
{
    const auto p = heartPath (r);
    g.setColour (colour);
    if (filled) g.fillPath (p);
    else g.strokePath (p, juce::PathStrokeType (1.4f));
}

void paintWood (juce::Graphics& g, juce::Rectangle<float> r, bool leftSide)
{
    juce::Path shape;
    shape.addRoundedRectangle (r, 8.0f);
    g.saveState();
    g.reduceClipRegion (shape);
    juce::ColourGradient grad (juce::Colour (0xff8a5426), r.getX(), r.getY(), juce::Colour (0xff4f2c10), r.getRight(), r.getBottom(), false);
    grad.addColour (0.5, juce::Colour (0xff74431b));
    g.setGradientFill (grad);
    g.fillRect (r);
    // grain
    juce::Random rnd (leftSide ? 17 : 29);
    for (int i = 0; i < 26; ++i)
    {
        const float x0 = r.getX() + rnd.nextFloat() * r.getWidth();
        juce::Path line;
        line.startNewSubPath (x0, r.getY());
        for (float yy = r.getY(); yy < r.getBottom(); yy += 12.0f)
            line.lineTo (x0 + std::sin (yy * 0.02f + (float) i) * 2.5f + std::sin (yy * 0.007f) * 4.0f, yy);
        g.setColour (juce::Colour (0xff2e1806).withAlpha (0.08f + rnd.nextFloat() * 0.18f));
        g.strokePath (line, juce::PathStrokeType (0.6f + rnd.nextFloat() * 1.4f));
    }
    // edge light
    juce::ColourGradient edge (juce::Colours::white.withAlpha (0.18f), leftSide ? r.getX() : r.getRight(), r.getY(),
                               juce::Colours::transparentWhite, leftSide ? r.getX() + 10.0f : r.getRight() - 10.0f, r.getY(), false);
    g.setGradientFill (edge);
    g.fillRect (r);
    g.restoreState();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.strokePath (shape, juce::PathStrokeType (1.0f));
}

void paintBrushedPanel (juce::Graphics& g, juce::Rectangle<float> r)
{
    juce::ColourGradient grad (juce::Colour (0xff26262a), r.getX(), r.getY(), juce::Colour (0xff141416), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRect (r);
    juce::Random rnd (7);
    for (float yy = r.getY(); yy < r.getBottom(); yy += 2.0f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.008f + rnd.nextFloat() * 0.018f));
        g.drawHorizontalLine ((int) yy, r.getX(), r.getRight());
    }
}

void paintGrille (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colour (0xff111113));
    g.fillRect (r);
    g.setColour (juce::Colour (0xff2c2c30));
    for (float yy = r.getY() + 3.0f; yy < r.getBottom() - 1.0f; yy += 4.0f)
        for (float xx = r.getX() + (((int) yy / 4) % 2 ? 2.0f : 4.0f); xx < r.getRight(); xx += 4.0f)
            g.fillRect (xx, yy, 1.4f, 1.4f);
}

void paintPanelLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, float size, juce::Justification j)
{
    g.setColour (Colours::text.withAlpha (0.85f));
    g.setFont (font (size, true));
    g.drawText (text, r, j, false);
}
} // namespace rc::ui
