#pragma once

#include "PluginProcessor.h"
#include "ui/Controls.h"

// The book's two screens (docs/UI-REFERENCE.md §6). Both are transparent and
// sit over the pages that na::ui::BookFrame draws; they are given the pages
// area (everything right of the spine) and lay out their own two pages.

namespace na::vol4
{

/** A goal card: name over tagline, check when chosen (UI-REFERENCE §6.1). */
class GoalCard final : public juce::Button
{
public:
    GoalCard (const juce::String& goalId, const juce::String& name, const juce::String& tagline, juce::Colour cloth);
    const juce::String goalId;

private:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    juce::String name, tagline;
    juce::Colour cloth;
};

/** Title Page: pick instrument and goal, then begin. */
class TitleView final : public juce::Component
{
public:
    explicit TitleView (CompressionAudioProcessor&);

    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void showGoalsFor (const juce::String& instrumentId, const juce::String& selectGoal);
    juce::String text (const char* key) const;
    juce::Rectangle<int> leftPage() const  { return getLocalBounds().removeFromLeft (getWidth() / 2); }
    juce::Rectangle<int> rightPage() const { return getLocalBounds().withTrimmedLeft (getWidth() / 2); }

    CompressionAudioProcessor& processor;
    const na::Content& content;
    juce::Colour cloth;

    juce::ComboBox instrumentBox;
    juce::OwnedArray<GoalCard> goalCards;
    juce::TextButton beginButton;
    juce::StringArray instrumentIds;
    juce::String selectedGoal, lesson;
    int questionY = 0, goalQuestionY = 0, lessonY = 0;
};

/** One row in Contents: numeral, title, status (UI-REFERENCE §6.2). */
class ChapterRow final : public juce::Button
{
public:
    ChapterRow (const juce::String& chapterId, const juce::String& numeral, const juce::String& title,
                const juce::String& readingNow, juce::Colour cloth);
    void setStatus (bool isCurrent, bool isDone);
    const juce::String chapterId;

private:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    juce::String numeral, title, readingNow;
    juce::Colour cloth;
    bool current = false, done = false;
};

/** A chapter: Contents on the left page, the chapter on the right. */
class ChapterView final : public juce::Component,
                          private juce::Timer
{
public:
    static constexpr int leftPageWidth = 330;

    explicit ChapterView (CompressionAudioProcessor&);
    ~ChapterView() override;

    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { repaint (rightPage().withTop (reading.getY())); repaint (setSoFarArea); repaint (knobBlock); }
    juce::String text (const char* key) const;
    juce::String fill (const juce::String& templ, std::initializer_list<std::pair<juce::String, juce::String>> values) const;
    juce::String chapterText (const char* field) const;
    juce::String doThisText() const;
    juce::Rectangle<int> leftPage() const  { return getLocalBounds().removeFromLeft (leftPageWidth); }
    juce::Rectangle<int> rightPage() const { return getLocalBounds().withTrimmedLeft (leftPageWidth); }

    void layoutRightPage();
    void paintContents (juce::Graphics&);
    void paintChapter (juce::Graphics&);
    void paintReading (juce::Graphics&, juce::Rectangle<int> area);

    CompressionAudioProcessor& processor;
    const na::Content& content;
    juce::Colour cloth;

    juce::TextButton changeButton, previousButton, nextButton;
    juce::OwnedArray<ChapterRow> rows;
    na::ui::BookKnob knob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAttachment;

    juce::var chapter, goal;
    juce::String attachedChapter, knobName, targetCaption;

    // Right-page layout (computed by layoutRightPage, used by paint)
    juce::Rectangle<int> header, titleArea, subtitleArea, introArea, knobBlock, instructions, reading, setSoFarArea;
    float introSize = 18.0f, instructionSize = 17.0f, messageSize = 17.0f;
    int knobSize = 132, readingPad = 16, readingGap = 12;
};

} // namespace na::vol4
