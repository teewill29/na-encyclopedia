#include "Style.h"
#include "NAFonts.h"

namespace na::ui
{

namespace
{
    juce::Typeface::Ptr load (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }

    struct Faces
    {
        juce::Typeface::Ptr cinzel500 = load (NAFonts::CinzelMedium_ttf,       NAFonts::CinzelMedium_ttfSize);
        juce::Typeface::Ptr cinzel600 = load (NAFonts::CinzelSemiBold_ttf,     NAFonts::CinzelSemiBold_ttfSize);
        juce::Typeface::Ptr cinzel700 = load (NAFonts::CinzelBold_ttf,         NAFonts::CinzelBold_ttfSize);
        juce::Typeface::Ptr garamond400 = load (NAFonts::EBGaramondRegular_ttf,  NAFonts::EBGaramondRegular_ttfSize);
        juce::Typeface::Ptr garamond500 = load (NAFonts::EBGaramondMedium_ttf,   NAFonts::EBGaramondMedium_ttfSize);
        juce::Typeface::Ptr garamond600 = load (NAFonts::EBGaramondSemiBold_ttf, NAFonts::EBGaramondSemiBold_ttfSize);
        juce::Typeface::Ptr garamondItalic = load (NAFonts::EBGaramondItalic_ttf, NAFonts::EBGaramondItalic_ttfSize);
    };

    const Faces& faces()
    {
        static const Faces f;
        return f;
    }

    juce::Font make (juce::Typeface::Ptr face, float size)
    {
        // JUCE's plain height is the line height (ascent + descent); "point
        // height" is the em size, which is what CSS and UI-REFERENCE call font-size.
        return juce::Font (juce::FontOptions (face)).withPointHeight (size);
    }
}

juce::Font Fonts::cinzel (float size, int weight, float tracking)
{
    const auto& f = faces();
    auto face = weight >= 700 ? f.cinzel700 : weight >= 600 ? f.cinzel600 : f.cinzel500;
    return make (face, size).withExtraKerningFactor (tracking);
}

juce::Font Fonts::garamond (float size, int weight, bool italic)
{
    const auto& f = faces();
    auto face = italic ? f.garamondItalic : weight >= 600 ? f.garamond600 : weight >= 500 ? f.garamond500 : f.garamond400;
    return make (face, size);
}

namespace
{
    juce::TextLayout layoutFor (const juce::String& text, const juce::Font& font, int width,
                                float lineHeight, juce::Colour colour, juce::Justification just)
    {
        juce::AttributedString s;
        s.append (text, font, colour);
        s.setJustification (just);
        // Extra space so each line is `lineHeight` × the em size (CSS line-height),
        // on top of the font's own ascent + descent.
        s.setLineSpacing (juce::jmax (0.0f, lineHeight * font.getHeightInPoints() - font.getHeight()));
        juce::TextLayout layout;
        layout.createLayout (s, (float) width);
        return layout;
    }
}

int Text::measure (const juce::String& text, const juce::Font& font, int width, float lineHeight)
{
    if (text.isEmpty()) return 0;
    return (int) std::ceil (layoutFor (text, font, width, lineHeight, {}, juce::Justification::topLeft).getHeight());
}

int Text::draw (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area, const juce::Font& font,
                juce::Colour colour, juce::Justification just, float lineHeight)
{
    if (text.isEmpty()) return 0;
    auto layout = layoutFor (text, font, area.getWidth(), lineHeight, colour, just);
    layout.draw (g, area.toFloat().withHeight (layout.getHeight()));
    return (int) std::ceil (layout.getHeight());
}

float Text::width (const juce::String& text, const juce::Font& font)
{
    return juce::GlyphArrangement::getStringWidth (font, text);
}

juce::Colour colourFromHex (const juce::String& hex, juce::Colour fallback)
{
    const auto digits = hex.trimCharactersAtStart ("#");
    if (digits.length() != 6 || ! digits.containsOnly ("0123456789abcdefABCDEF"))
        return fallback;
    return juce::Colour ((juce::uint32) (0xFF000000u | (juce::uint32) digits.getHexValue32()));
}

} // namespace na::ui
