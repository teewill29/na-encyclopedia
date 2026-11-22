#include "Controls.h"

#include <cmath>

namespace na::ui
{

// ── Icons ────────────────────────────────────────────────────────────────────
void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> r, juce::Colour colour, float strokeWidth)
{
    // Drawn on a 24-unit grid, scaled into `r`.
    juce::Path p;
    switch (icon)
    {
        case Icon::check:
            p.startNewSubPath (4.5f, 12.75f); p.lineTo (9.0f, 17.25f); p.lineTo (19.5f, 6.75f);
            break;
        case Icon::headphones:
            p.startNewSubPath (4.0f, 15.0f); p.lineTo (4.0f, 12.0f);
            p.addCentredArc (12.0f, 12.0f, 8.0f, 8.0f, 0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi);
            p.lineTo (20.0f, 15.0f);
            p.addRoundedRectangle (3.0f, 14.0f, 5.0f, 7.0f, 1.5f);
            p.addRoundedRectangle (16.0f, 14.0f, 5.0f, 7.0f, 1.5f);
            break;
        case Icon::swap:
            p.startNewSubPath (7.0f, 7.0f); p.lineTo (18.0f, 7.0f); p.lineTo (15.0f, 4.0f);
            p.startNewSubPath (17.0f, 17.0f); p.lineTo (6.0f, 17.0f); p.lineTo (9.0f, 20.0f);
            break;
    }
    const float s = juce::jmin (r.getWidth(), r.getHeight()) / 24.0f;
    p.applyTransform (juce::AffineTransform::scale (s).translated (r.getCentreX() - 12.0f * s, r.getCentreY() - 12.0f * s));
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (strokeWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void setStyle (juce::Button& b, ButtonStyle style, std::optional<Icon> icon)
{
    b.getProperties().set ("naStyle", (int) style);
    b.getProperties().set ("naIcon", icon ? (int) *icon : -1);
    b.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    b.repaint();
}

namespace
{
    ButtonStyle styleOf (const juce::Button& b)
    {
        const auto v = b.getProperties()["naStyle"];
        return v.isVoid() ? ButtonStyle::secondary : (ButtonStyle) (int) v;
    }
}

// ── HoldToggleButton ─────────────────────────────────────────────────────────
void HoldToggleButton::set (bool on)
{
    setToggleState (on, juce::dontSendNotification);
    if (onToggle) onToggle (on);
}

void HoldToggleButton::mouseDown (const juce::MouseEvent& e)
{
    juce::TextButton::mouseDown (e);
    wasOn = getToggleState();
    downAt = juce::Time::getMillisecondCounter();
    if (! wasOn) set (true);     // on at once, so holding is instant
}

void HoldToggleButton::mouseUp (const juce::MouseEvent& e)
{
    juce::TextButton::mouseUp (e);
    const bool held = juce::Time::getMillisecondCounter() - downAt > 350;
    if (held)        set (false);      // momentary
    else if (wasOn)  set (false);      // a click on an "on" button turns it off
}

// ── LookAndFeel ──────────────────────────────────────────────────────────────
BookLookAndFeel::BookLookAndFeel (juce::Colour c) : cloth (c)
{
    setColour (juce::PopupMenu::backgroundColourId, Palette::paperField);
    setColour (juce::PopupMenu::textColourId, Palette::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::rowCurrent);
    setColour (juce::PopupMenu::highlightedTextColourId, Palette::ink);
    setColour (juce::ComboBox::textColourId, Palette::ink);
    setColour (juce::TextEditor::backgroundColourId, Palette::paperField);
    setColour (juce::TextEditor::textColourId, Palette::ink);
    setColour (juce::TextEditor::outlineColourId, Palette::fieldBorder);
    setColour (juce::TextEditor::focusedOutlineColourId, Palette::goldLine);
    setColour (juce::CaretComponent::caretColourId, Palette::ink);
}

void BookLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat();
    const float alpha = b.isEnabled() ? 1.0f : 0.45f;
    switch (styleOf (b))
    {
        case ButtonStyle::primary:
            g.setColour ((down ? cloth.darker (0.15f) : highlighted ? cloth.brighter (0.08f) : cloth).withMultipliedAlpha (alpha));
            g.fillRoundedRectangle (r, 6.0f);
            break;
        case ButtonStyle::secondary:
        {
            const bool on = b.getToggleState();
            g.setColour ((on ? Palette::cardOn : down ? Palette::rowCurrent : highlighted ? Palette::paperField.darker (0.03f) : Palette::paperField)
                            .withMultipliedAlpha (alpha));
            g.fillRoundedRectangle (r.reduced (0.75f), 6.0f);
            g.setColour ((on ? cloth : Palette::fieldBorder).withMultipliedAlpha (alpha));
            g.drawRoundedRectangle (r.reduced (0.75f), 6.0f, on ? 2.0f : 1.5f);
            break;
        }
        case ButtonStyle::text:
            break;
    }
}

juce::Font BookLookAndFeel::getTextButtonFont (juce::TextButton& b, int)
{
    switch (styleOf (b))
    {
        case ButtonStyle::primary:   return Fonts::cinzel (b.getHeight() >= 52 ? 17.0f : 14.0f, 600, 0.14f);
        case ButtonStyle::secondary: return Fonts::garamond (16.0f, 600);
        case ButtonStyle::text:      return Fonts::garamond (16.0f, 400, true);
    }
    return Fonts::garamond (16.0f);
}

void BookLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool highlighted, bool)
{
    const auto style = styleOf (b);
    const float alpha = b.isEnabled() ? 1.0f : 0.5f;
    const auto colour = style == ButtonStyle::primary ? Palette::gold : style == ButtonStyle::text ? Palette::inkSoft : Palette::ink;
    const auto font = getTextButtonFont (b, b.getHeight());
    auto area = b.getLocalBounds().toFloat();

    const auto iconVar = b.getProperties()["naIcon"];
    const int icon = iconVar.isVoid() ? -1 : (int) iconVar;
    const float textW = Text::width (b.getButtonText(), font);
    const float iconW = icon >= 0 ? 18.0f + 8.0f : 0.0f;
    float x = area.getCentreX() - (textW + iconW) * 0.5f;
    if (icon >= 0)
    {
        drawIcon (g, (Icon) icon, { x, area.getCentreY() - 9.0f, 18.0f, 18.0f }, colour.withMultipliedAlpha (alpha));
        x += iconW;
    }
    g.setColour (colour.withMultipliedAlpha (alpha));
    g.setFont (font);
    g.drawText (b.getButtonText(), juce::Rectangle<float> (x, area.getY(), textW + 2.0f, area.getHeight()),
                juce::Justification::centredLeft, false);
    if (style == ButtonStyle::text && highlighted)
        g.fillRect (x, area.getCentreY() + font.getHeight() * 0.38f, textW, 1.0f);
}

void BookLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.75f);
    g.setColour (Palette::paperField);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (box.hasKeyboardFocus (true) ? Palette::goldLine : Palette::fieldBorder);
    g.drawRoundedRectangle (r, 6.0f, 1.5f);
    // Chevron
    juce::Path arrow;
    const float cx = (float) w - 22.0f, cy = (float) h * 0.5f;
    arrow.startNewSubPath (cx - 5.0f, cy - 2.5f);
    arrow.lineTo (cx, cy + 2.5f);
    arrow.lineTo (cx + 5.0f, cy - 2.5f);
    g.setColour (Palette::inkSoft);
    g.strokePath (arrow, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font BookLookAndFeel::getComboBoxFont (juce::ComboBox&)  { return Fonts::garamond (18.0f); }
juce::Font BookLookAndFeel::getPopupMenuFont()                 { return Fonts::garamond (17.0f); }

void BookLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 40, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void BookLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (Palette::paperField);
    g.setColour (Palette::fieldBorder);
    g.drawRect (0, 0, w, h, 1);
}

void BookLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                         bool isHighlighted, bool isTicked, bool, const juce::String& text,
                                         const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour (Palette::rule);
        g.fillRect (area.reduced (10, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    if (isHighlighted && isActive)
    {
        g.setColour (Palette::rowCurrent);
        g.fillRect (area);
    }
    auto r = area.reduced (12, 0);
    if (isTicked)
        drawIcon (g, Icon::check, r.removeFromLeft (18).withSizeKeepingCentre (16, 16).toFloat(), Palette::good);
    else
        r.removeFromLeft (18);
    r.removeFromLeft (8);
    g.setColour (Palette::ink.withMultipliedAlpha (isActive ? 1.0f : 0.5f));
    g.setFont (getPopupMenuFont());
    g.drawText (text, r, juce::Justification::centredLeft);
}

// ── Knob ─────────────────────────────────────────────────────────────────────
void drawKnob (juce::Graphics& g, juce::Rectangle<float> area, float angle, bool active)
{
    const float d = juce::jmin (area.getWidth(), area.getHeight());
    const auto r = area.withSizeKeepingCentre (d, d);
    const auto c = r.getCentre();
    const bool small = d < 80.0f;
    const float u = d / (small ? 48.0f : 132.0f);   // mockup units

    if (active)
    {
        juce::Path circle;
        circle.addEllipse (r.expanded (3.0f));
        juce::DropShadow (juce::Colours::black.withAlpha (0.35f), (int) (14 * u), { 0, (int) (6 * u) }).drawForPath (g, circle);
        g.setColour (Palette::gold);
        g.fillEllipse (r.expanded (3.0f));
    }

    // Knurled ring: alternating #1C1C1C / #3A3A3A wedges (4° each; 5° on the small knob)
    const float step = juce::degreesToRadians (small ? 5.0f : 4.0f);
    const int wedges = (int) std::round (juce::MathConstants<float>::twoPi / step);
    for (int i = 0; i < wedges; ++i)
    {
        juce::Path wedge;
        wedge.addPieSegment (r, i * step, (i + 1) * step + 0.002f, 0.0f);
        g.setColour (i % 2 == 0 ? juce::Colour (0xFF1C1C1C) : juce::Colour (0xFF3A3A3A));
        g.fillPath (wedge);
    }

    // Cap: radial gradient, highlight top-left
    const auto cap = r.reduced ((small ? 5.0f : 11.0f) * u);
    juce::ColourGradient capFill (juce::Colour (0xFF474747), cap.getX() + cap.getWidth() * 0.35f, cap.getY() + cap.getHeight() * 0.30f,
                                  juce::Colour (0xFF121212), cap.getX() + cap.getWidth() * 0.35f + cap.getWidth() * 0.7f * 0.7071f,
                                  cap.getY() + cap.getHeight() * 0.30f + cap.getHeight() * 0.7f * 0.7071f, true);
    g.setGradientFill (capFill);
    g.fillEllipse (cap);

    // Gold indicator bar: 5 × 28 at 132 px (3 × 12 on the small knob)
    const float barW = (small ? 3.0f : 5.0f) * u, barH = (small ? 12.0f : 28.0f) * u, barTop = (small ? 6.0f : 14.0f) * u;
    juce::Path bar;
    bar.addRoundedRectangle (c.x - barW * 0.5f, r.getY() + barTop, barW, barH, barW * 0.5f);
    bar.applyTransform (juce::AffineTransform::rotation (angle, c.x, c.y));
    g.setColour (Palette::gold);
    g.fillPath (bar);
}

void BookLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    // Active knobs sit inside a padded slider: room for the gold ring and its shadow.
    const float pad = static_cast<bool> (s.getProperties()["naActive"]) ? 18.0f : 0.0f;
    drawKnob (g, juce::Rectangle<int> (x, y, w, h).toFloat().reduced (pad),
              startAngle + pos * (endAngle - startAngle), static_cast<bool> (s.getProperties()["naActive"]));
}

BookKnob::BookKnob() : juce::Slider (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox)
{
    setRotaryParameters (knobStartAngle, knobEndAngle, true);
    setMouseDragSensitivity (250);
    setScrollWheelEnabled (true);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void BookKnob::mouseDown (const juce::MouseEvent& e)
{
    setMouseDragSensitivity (e.mods.isShiftDown() ? 1500 : 250);   // Shift: fine adjust
    juce::Slider::mouseDown (e);
}

void BookKnob::mouseDrag (const juce::MouseEvent& e)
{
    setMouseDragSensitivity (e.mods.isShiftDown() ? 1500 : 250);
    juce::Slider::mouseDrag (e);
}

void BookKnob::mouseDoubleClick (const juce::MouseEvent&)
{
    // Double-click to type a value
    editor = std::make_unique<juce::TextEditor>();
    editor->setFont (Fonts::cinzel (18.0f, 600));
    editor->setJustification (juce::Justification::centred);
    editor->setText (getTextFromValue (getValue()), false);
    editor->selectAll();
    editor->onReturnKey = [this] { closeEditor (true); };
    editor->onEscapeKey = [this] { closeEditor (false); };
    editor->onFocusLost = [this] { closeEditor (true); };
    addAndMakeVisible (*editor);
    editor->setBounds (getLocalBounds().withSizeKeepingCentre (juce::jmin (getWidth(), 110), 34));
    editor->grabKeyboardFocus();
}

void BookKnob::closeEditor (bool apply)
{
    if (editor == nullptr) return;
    if (apply)
        setValue (getValueFromText (editor->getText()), juce::sendNotificationSync);
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<BookKnob> (this)]
                                     { if (safe != nullptr) safe->editor.reset(); });
}

// ── Meter ────────────────────────────────────────────────────────────────────
int drawMeter (juce::Graphics& g, juce::Rectangle<int> area, const MeterSpec& m)
{
    auto r = area;
    auto labelRow = r.removeFromTop (22);
    g.setColour (Palette::inkSoft);
    g.setFont (Fonts::garamond (15.0f));
    g.drawText (m.label, labelRow, juce::Justification::centredLeft);
    g.setColour (Palette::ink);
    g.setFont (Fonts::garamond (18.0f, 600));
    g.drawText (m.valueText, labelRow, juce::Justification::centredRight);
    r.removeFromTop (5);

    auto track = r.removeFromTop (12).toFloat();
    const auto toX = [&] (float v)
    {
        return track.getX() + track.getWidth() * juce::jlimit (0.0f, 1.0f, (v - m.rangeMin) / (m.rangeMax - m.rangeMin));
    };
    juce::Path trackShape;
    trackShape.addRoundedRectangle (track, 6.0f);
    g.setColour (Palette::meterTrack);
    g.fillPath (trackShape);
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (trackShape);
        if (! std::isnan (m.targetMin))
        {
            const auto zone = juce::Rectangle<float>::leftTopRightBottom (toX (m.targetMin), track.getY(), toX (m.targetMax), track.getBottom());
            g.setColour (Palette::good.withAlpha (0.22f));
            g.fillRect (zone);
            g.setColour (Palette::good);
            g.fillRect (zone.withWidth (1.0f));
            g.fillRect (zone.withX (zone.getRight() - 1.0f).withWidth (1.0f));
        }
    }
    if (! std::isnan (m.value))
    {
        // Bars grow from 0 (gain reduction, ±dB); level meters entirely below 0 (dBFS) grow from their low end.
        const auto origin = m.rangeMax <= 0.0f ? m.rangeMin : juce::jlimit (m.rangeMin, m.rangeMax, 0.0f);
        const auto zeroX = toX (origin);
        const auto valueX = toX (m.value);
        auto fill = juce::Rectangle<float>::leftTopRightBottom (juce::jmin (zeroX, valueX), track.getY() + 3.0f,
                                                              juce::jmax (zeroX, valueX), track.getBottom() - 3.0f);
        g.setColour (m.fill.withMultipliedAlpha (m.alpha));
        g.fillRoundedRectangle (fill.withWidth (juce::jmax (3.0f, fill.getWidth())), 3.0f);
    }
    r.removeFromTop (5);
    g.setColour (Palette::inkMuted);
    g.setFont (Fonts::garamond (13.0f));
    g.drawText (m.caption, r.removeFromTop (18), juce::Justification::centredLeft);
    return r.getY() - area.getY();
}

} // namespace na::ui
