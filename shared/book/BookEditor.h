#pragma once

#include "BookProcessor.h"
#include "BookViews.h"
#include "ui/BookFrame.h"
#include "ui/Controls.h"
#include "ui/MetricsOverlay.h"

namespace na::book
{

/** Small gold text on the cloth's bottom edge (window size, debug toggle). */
class ClothTextButton final : public juce::Button
{
public:
    ClothTextButton() : juce::Button ({}) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

/** The whole book at its base size (1120 × 760). The editor scales it. */
class BookRoot final : public juce::Component,
                       private juce::ChangeListener
{
public:
    BookRoot (BookProcessor&, std::function<void (float)> onScaleChosen);
    ~BookRoot() override;

    void setScaleLabel (float scale);
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { showCurrentPage(); }
    void showCurrentPage();

    BookProcessor& processor;
    na::ui::BookLookAndFeel lookAndFeel;
    na::ui::BookFrame frame;
    TitleView titleView;
    ChapterView chapterView;
    ClothTextButton sizeButton;
    std::function<void (float)> onScaleChosen;

   #if NA_DEBUG_OVERLAY
    na::MetricsOverlay overlay;
    ClothTextButton debugButton;
   #endif
};

/** The plugin window. Sizes in fixed steps (UI-REFERENCE §2), remembered per user. */
class BookEditor final : public juce::AudioProcessorEditor
{
public:
    explicit BookEditor (BookProcessor&);

    static constexpr float scales[] = { 0.8f, 1.0f, 1.25f, 1.5f };

    void setScale (float scale);
    void resized() override;

private:
    static juce::PropertiesFile::Options settingsOptions();

    BookRoot root;
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BookEditor)
};

} // namespace na::book
