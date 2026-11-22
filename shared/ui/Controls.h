#pragma once

#include "Style.h"

namespace na::ui
{

/** Simple line icons, 2 px stroke, round caps (UI-REFERENCE §7). No emoji. */
enum class Icon { check, headphones, swap };
void drawIcon (juce::Graphics&, Icon, juce::Rectangle<float> area, juce::Colour, float strokeWidth = 2.0f);

/** Button styles (UI-REFERENCE §7). Set with setStyle(). */
enum class ButtonStyle { primary, secondary, text };
void setStyle (juce::Button&, ButtonStyle, std::optional<Icon> icon = std::nullopt);

/** A toggle you can also hold: a quick click toggles it; holding it longer
    than 350 ms makes it momentary (on while held, off on release). Used for
    "Compare at equal level" (UI-REFERENCE §6.2). */
class HoldToggleButton final : public juce::TextButton
{
public:
    HoldToggleButton() { setClickingTogglesState (false); }
    std::function<void (bool on)> onToggle;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void set (bool on);
    bool wasOn = false;
    juce::uint32 downAt = 0;
};

/** The book's LookAndFeel: buttons, dropdowns, menus, the knob. */
class BookLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit BookLookAndFeel (juce::Colour cloth);
    void setCloth (juce::Colour c) { cloth = c; }

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcut,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

private:
    juce::Colour cloth;
};

/** The knob, drawn on its own (also used for the small "Set so far" knobs).
    angle: radians from 12 o'clock, clockwise. active: gold ring + shadow. */
void drawKnob (juce::Graphics&, juce::Rectangle<float> area, float angle, bool active);

/** Rotary range shared by every knob: −135° to +135°. */
inline constexpr float knobStartAngle = juce::MathConstants<float>::pi * 1.25f;
inline constexpr float knobEndAngle   = juce::MathConstants<float>::pi * 2.75f;

/** The main knob: vertical drag, Shift for fine, double-click to type, wheel. */
class BookKnob final : public juce::Slider
{
public:
    BookKnob();
    void setActive (bool a) { getProperties().set ("naActive", a); repaint(); }

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void closeEditor (bool apply);
    std::unique_ptr<juce::TextEditor> editor;
};

/** A meter in The Reading (UI-REFERENCE §7). */
struct MeterSpec
{
    juce::String label, valueText, caption;
    float value = std::numeric_limits<float>::quiet_NaN();
    float rangeMin = 0.0f, rangeMax = 1.0f;
    float targetMin = std::numeric_limits<float>::quiet_NaN(), targetMax = std::numeric_limits<float>::quiet_NaN();
    juce::Colour fill;
    float alpha = 1.0f;
};
/** Label row 22, track 12, caption: returns the height used. */
int drawMeter (juce::Graphics&, juce::Rectangle<int> area, const MeterSpec&);
inline constexpr int meterHeight = 22 + 5 + 12 + 5 + 18;

} // namespace na::ui
