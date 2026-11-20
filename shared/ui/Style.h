#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The book's look, shared by every volume (docs/UI-REFERENCE.md §3, §4).
namespace na::ui
{

/** Colour tokens (UI-REFERENCE §3). `cloth` is per volume and comes from content. */
namespace Palette
{
    inline const juce::Colour gold        { 0xFFCDAE5E };
    inline const juce::Colour goldLine    { 0xFFA88B4A };
    inline const juce::Colour leatherDark { 0xFF1A100A };
    inline const juce::Colour leatherMid  { 0xFF3A2618 };
    inline const juce::Colour paper       { 0xFFF4ECDB };
    inline const juce::Colour paperShade  { 0xFFDDCFB3 };
    inline const juce::Colour paperPanel  { 0xFFEFE5CF };
    inline const juce::Colour paperField  { 0xFFFBF7EE };
    inline const juce::Colour rule        { 0xFFC2B08A };
    inline const juce::Colour ink         { 0xFF2A2118 };
    inline const juce::Colour inkSoft     { 0xFF4A3C2C };
    inline const juce::Colour inkMuted    { 0xFF5E4E3B };
    inline const juce::Colour labelInk    { 0xFF6B5A45 };
    inline const juce::Colour good        { 0xFF2F5E3A };
    inline const juce::Colour warnLow     { 0xFF9A2E1F };   // too fast / too much
    inline const juce::Colour warnHigh    { 0xFF8A5410 };   // too slow / too little
    inline const juce::Colour meterTrack  { 0xFFDCCFB3 };
    inline const juce::Colour fieldBorder { 0xFFA8966F };
    inline const juce::Colour rowCurrent  { 0xFFE3D5B8 };
    inline const juce::Colour cardOn      { 0xFFE9DDC3 };
    inline const juce::Colour cardBorder  { 0xFFB9A888 };
}

/** The two book fonts, embedded (UI-REFERENCE §4). Message thread only. */
namespace Fonts
{
    /** Cinzel, weight 500, 600 or 700. `tracking` is letter-spacing in em (0.24 = 0.24em). */
    juce::Font cinzel (float size, int weight = 600, float tracking = 0.0f);

    /** EB Garamond, weight 400, 500 or 600, optionally italic (400 only). */
    juce::Font garamond (float size, int weight = 400, bool italic = false);
}

/** Text helpers that measure and draw the same way, so layout and paint agree. */
namespace Text
{
    /** Height of `text` wrapped to `width` (line height = 1.45 × size by default). */
    int measure (const juce::String& text, const juce::Font& font, int width, float lineHeight = 1.45f);

    /** Draws wrapped text from the top of `area`; returns the height used. */
    int draw (juce::Graphics&, const juce::String& text, juce::Rectangle<int> area, const juce::Font&,
              juce::Colour, juce::Justification = juce::Justification::topLeft, float lineHeight = 1.45f);

    /** Width of a single line, letter-spacing included. */
    float width (const juce::String& text, const juce::Font& font);
}

/** Parses "#RRGGBB" (content colours). Falls back to `fallback`. */
juce::Colour colourFromHex (const juce::String& hex, juce::Colour fallback);

} // namespace na::ui
