#pragma once

#include "Style.h"

namespace na::ui
{

/** What's printed on a volume's spine (UI-REFERENCE §5). */
struct SpineText
{
    juce::String series;        // "NA ENCYCLOPEDIA"
    juce::String topic;         // "COMPRESSION"
    juce::String volLabel;      // "VOL."
    juce::String numeral;       // "IV"
    juce::StringArray edition;  // { "FIRST", "EDITION" }
    juce::StringArray maker;    // { "NOVELTEE", "AUDIO" }
};

/** The open volume: cloth boards, the shared leather spine, two cream pages.
    Pages are drawn here; the screens on top of them are transparent children.
    Base size 1120 × 760 (UI-REFERENCE §2); scaling is done by the parent. */
class BookFrame final : public juce::Component
{
public:
    static constexpr int baseWidth = 1120, baseHeight = 760;
    static constexpr int margin = 18, spineWidth = 96;

    BookFrame();

    void setCloth (juce::Colour);
    void setSpine (const SpineText&);

    /** Width of the left page; 0 = two equal pages (Title Page). */
    void setLeftPageWidth (int width);

    juce::Rectangle<int> getPagesArea() const;
    juce::Rectangle<int> getLeftPage() const;
    juce::Rectangle<int> getRightPage() const;

    void paint (juce::Graphics&) override;

    /** The spine on its own, any height (the shelf view uses this too). */
    static void paintSpine (juce::Graphics&, juce::Rectangle<float> area, const SpineText&, juce::Colour cloth,
                            float numeralSize = 30.0f, bool roundLeftCorners = true);

    /** Cloth weave over `area` (UI-REFERENCE §8). */
    static void paintCloth (juce::Graphics&, juce::Rectangle<float> area, juce::Colour cloth);

private:
    juce::Colour cloth { 0xFF213B2F };
    SpineText spine;
    int leftPageWidth = 0;
};

} // namespace na::ui
