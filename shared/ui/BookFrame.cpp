#include "BookFrame.h"

#include <cmath>

namespace na::ui
{

BookFrame::BookFrame()
{
    setBufferedToImage (true);     // static between page changes
    setInterceptsMouseClicks (false, false);
}

void BookFrame::setCloth (juce::Colour c)        { cloth = c; repaint(); }
void BookFrame::setSpine (const SpineText& s)    { spine = s; repaint(); }
void BookFrame::setLeftPageWidth (int w)         { if (w != leftPageWidth) { leftPageWidth = w; repaint(); } }

juce::Rectangle<int> BookFrame::getPagesArea() const
{
    return getLocalBounds().reduced (margin).withTrimmedLeft (spineWidth);
}

juce::Rectangle<int> BookFrame::getLeftPage() const
{
    auto pages = getPagesArea();
    return pages.removeFromLeft (leftPageWidth > 0 ? leftPageWidth : pages.getWidth() / 2);
}

juce::Rectangle<int> BookFrame::getRightPage() const
{
    return getPagesArea().withTrimmedLeft (getLeftPage().getWidth());
}

void BookFrame::paintCloth (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour clothColour)
{
    g.setColour (clothColour);
    g.fillRect (area);

    // Two faint diagonal weaves: white 3.5% at +45°, black 9% at −45°; 2 px lines every 5 px.
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (area.toNearestInt());
    const float step = 5.0f * juce::MathConstants<float>::sqrt2;   // 5 px measured across the lines
    const float span = area.getWidth() + area.getHeight();
    for (int pass = 0; pass < 2; ++pass)
    {
        g.setColour (pass == 0 ? juce::Colours::white.withAlpha (0.035f) : juce::Colours::black.withAlpha (0.09f));
        juce::Path lines;
        for (float d = -area.getHeight(); d < span; d += step)
        {
            const float x0 = area.getX() + d;
            if (pass == 0) { lines.startNewSubPath (x0, area.getBottom()); lines.lineTo (x0 + area.getHeight(), area.getY()); }
            else           { lines.startNewSubPath (x0, area.getY());      lines.lineTo (x0 + area.getHeight(), area.getBottom()); }
        }
        g.strokePath (lines, juce::PathStrokeType (2.0f * 0.7071f * 1.4f));
    }
}

namespace
{
    void bands (juce::Graphics& g, float centreX, float y)
    {
        // gold 2 px, gap 3, dark ridge 4, gap 3, gold 2 — 72 px wide (UI-REFERENCE §5)
        const float x = centreX - 36.0f;
        g.setColour (Palette::gold);              g.fillRect (x, y, 72.0f, 2.0f);
        g.setColour (juce::Colour (0xFF0E0805));  g.fillRect (x, y + 5.0f, 72.0f, 4.0f);
        g.setColour (Palette::gold);              g.fillRect (x, y + 12.0f, 72.0f, 2.0f);
    }
    constexpr float bandsHeight = 14.0f;

    /** Text running top-to-bottom (CSS writing-mode: vertical-rl), centred on cx. */
    void verticalText (juce::Graphics& g, const juce::String& text, const juce::Font& font, juce::Colour colour,
                       float cx, float top, float length, bool centred)
    {
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi, cx, top));
        g.setColour (colour);
        g.setFont (font);
        // After rotating, "x" runs down the spine from `top`.
        g.drawText (text, juce::Rectangle<float> (cx, top - font.getHeight() * 0.5f, length, font.getHeight()),
                    centred ? juce::Justification::centred : juce::Justification::centredLeft, false);
    }

    void stackedLines (juce::Graphics& g, const juce::StringArray& lines, const juce::Font& font, juce::Colour colour,
                       float cx, float top, float lineHeight)
    {
        g.setColour (colour);
        g.setFont (font);
        for (int i = 0; i < lines.size(); ++i)
            g.drawText (lines[i], juce::Rectangle<float> (cx - 45.0f, top + i * lineHeight, 90.0f, lineHeight),
                        juce::Justification::centred, false);
    }
}

void BookFrame::paintSpine (juce::Graphics& g, juce::Rectangle<float> r, const SpineText& s, juce::Colour clothColour,
                            float numeralSize, bool roundLeftCorners)
{
    // Leather: leatherDark → leatherMid (40%) → #2B1B12 (70%) → #120A06
    juce::ColourGradient leather (Palette::leatherDark, r.getX(), 0, juce::Colour (0xFF120A06), r.getRight(), 0, false);
    leather.addColour (0.4, Palette::leatherMid);
    leather.addColour (0.7, juce::Colour (0xFF2B1B12));
    g.setGradientFill (leather);
    juce::Path shape;
    shape.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), 6.0f, 6.0f,
                               roundLeftCorners, false, roundLeftCorners, false);
    g.fillPath (shape);

    // Inset shadow on the page side
    juce::ColourGradient shade (juce::Colours::black.withAlpha (0.0f), r.getRight() - 14.0f, 0,
                                juce::Colours::black.withAlpha (0.45f), r.getRight(), 0, false);
    g.setGradientFill (shade);
    g.fillRect (r.withLeft (r.getRight() - 14.0f));

    // Fixed positions, top to bottom (UI-REFERENCE §5). Only the topic zone stretches.
    const float cx = r.getCentreX();
    const float top = r.getY() + 18.0f, bottom = r.getBottom() - 16.0f;
    const auto goldA = [] (float a) { return Palette::gold.withAlpha (a); };

    float y = top;
    bands (g, cx, y);                                y += bandsHeight + 14.0f;
    verticalText (g, s.series, Fonts::cinzel (10.0f, 500, 0.2f), goldA (0.8f), cx, y, 150.0f, false);
    y += 150.0f + 14.0f;
    bands (g, cx, y);                                y += bandsHeight;
    const float topicTop = y;

    // From the bottom up: maker, bands, edition, plate, bands
    float yb = bottom;
    const float makerH = 2 * 9.0f * 1.45f;           yb -= makerH;
    stackedLines (g, s.maker, Fonts::cinzel (9.0f, 500, 0.16f), goldA (0.75f), cx, yb, 9.0f * 1.45f);
    yb -= 12.0f + bandsHeight;                       bands (g, cx, yb);
    const float editionH = 2 * 8.0f * 1.4f;         yb -= 12.0f + editionH;
    stackedLines (g, s.edition, Fonts::cinzel (8.0f, 500, 0.14f), goldA (0.7f), cx, yb, 8.0f * 1.4f);
    yb -= 12.0f + 78.0f;
    {   // Label plate: 64 × 78, cloth colour, double gold border (1 px, 3 px gap)
        auto plate = juce::Rectangle<float> (cx - 32.0f, yb, 64.0f, 78.0f);
        g.setColour (clothColour);
        g.fillRect (plate);
        g.setColour (Palette::gold);
        g.drawRect (plate, 1.0f);
        g.drawRect (plate.reduced (4.0f), 1.0f);
        auto inner = plate.reduced (4.0f);
        g.setFont (Fonts::cinzel (10.0f, 500, 0.16f));
        g.drawText (s.volLabel, inner.removeFromTop (inner.getHeight() * 0.42f).withTrimmedTop (6.0f), juce::Justification::centredBottom);
        g.setFont (Fonts::cinzel (numeralSize, 700));
        g.drawText (s.numeral, inner.withTrimmedBottom (4.0f), juce::Justification::centredTop);
    }
    yb -= 16.0f + bandsHeight;                       bands (g, cx, yb);
    const float topicBottom = yb;

    // Topic word: never wraps by itself (UI-REFERENCE §5)
    const auto topic = s.topic;
    const float zone = topicBottom - topicTop;
    if (topic.length() <= 14)
    {
        const bool small = topic.length() > 12;
        verticalText (g, topic, Fonts::cinzel (small ? 19.0f : 22.0f, 600, small ? 0.2f : 0.24f), Palette::gold,
                      cx, topicTop, zone, true);
    }
    else
    {
        // Two lines, split at the space nearest the middle, centred
        int split = -1;
        for (int i = 0; i < topic.length(); ++i)
            if (topic[i] == ' ' && (split < 0 || std::abs (i - topic.length() / 2) < std::abs (split - topic.length() / 2)))
                split = i;
        const auto first = split > 0 ? topic.substring (0, split) : topic;
        const auto second = split > 0 ? topic.substring (split + 1) : juce::String();
        const auto font = Fonts::cinzel (20.0f, 600, 0.2f);
        const float lineGap = 20.0f * 1.2f;
        // vertical-rl: the first line is the rightmost column
        verticalText (g, first,  font, Palette::gold, cx + lineGap * 0.5f, topicTop, zone, true);
        verticalText (g, second, font, Palette::gold, cx - lineGap * 0.5f, topicTop, zone, true);
    }
}

void BookFrame::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Cloth boards, rounded 12
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path board;
        board.addRoundedRectangle (bounds, 12.0f);
        g.reduceClipRegion (board);
        paintCloth (g, bounds, cloth);
    }

    const auto inner = bounds.reduced ((float) margin);
    paintSpine (g, inner.withWidth ((float) spineWidth), spine, cloth);

    // Pages, with the stack of page edges showing underneath
    const auto pages = getPagesArea().toFloat();
    auto roundedRight = [] (juce::Rectangle<float> r)
    {
        juce::Path p;
        p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), 6.0f, 6.0f, false, true, false, true);
        return p;
    };
    g.setColour (juce::Colour (0xFFCDBD9A)); g.fillPath (roundedRight (pages.translated (0, 4.0f)));
    g.setColour (juce::Colour (0xFFD8CAAA)); g.fillPath (roundedRight (pages.translated (0, 2.0f)));

    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (roundedRight (pages));

    const auto left = getLeftPage().toFloat(), right = getRightPage().toFloat();
    juce::ColourGradient leftFill (juce::Colour (0xFFEFE5D0), left.getX(), 0, Palette::paperShade, left.getRight(), 0, false);
    leftFill.addColour (0.8, Palette::paper);
    g.setGradientFill (leftFill);
    g.fillRect (left);

    const double edge = leftPageWidth > 0 ? 0.08 : 0.10;
    juce::ColourGradient rightFill (Palette::paperShade, right.getX(), 0, juce::Colour (0xFFF6EFE0), right.getRight(), 0, false);
    rightFill.addColour (edge, Palette::paper);
    g.setGradientFill (rightFill);
    g.fillRect (right);
}

} // namespace na::ui
