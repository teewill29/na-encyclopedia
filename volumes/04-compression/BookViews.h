#pragma once

#include "PluginProcessor.h"

// Milestone 4: the book's two screens, functional and unstyled.
// Layout follows docs/UI-REFERENCE.md §6 loosely; Milestone 6 replaces these
// with the real book UI built from shared components.

namespace na::vol4
{

/** A clickable card: goal name over its tagline (UI-REFERENCE §6.1). */
class GoalCard final : public juce::Button
{
public:
    GoalCard (const juce::String& goalId, const juce::String& name, const juce::String& tagline);
    const juce::String goalId;

private:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    juce::String name, tagline;
};

/** Title Page: pick instrument and goal, then begin. */
class TitleView final : public juce::Component
{
public:
    explicit TitleView (CompressionAudioProcessor&);

    /** Re-reads the session (selected instrument/goal). */
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void showGoalsFor (const juce::String& instrumentId, const juce::String& selectGoal);
    juce::String text (const char* key) const;

    CompressionAudioProcessor& processor;
    const na::Content& content;

    juce::ComboBox instrumentBox;
    juce::OwnedArray<GoalCard> goalCards;
    juce::TextButton beginButton;
    juce::StringArray instrumentIds;
    juce::String selectedGoal, lesson;
};

/** One row in Contents: numeral, title, status (UI-REFERENCE §6.2). */
class ChapterRow final : public juce::Button
{
public:
    ChapterRow (const juce::String& chapterId, const juce::String& numeral, const juce::String& title, const juce::String& readingNow);
    void setStatus (bool isCurrent, bool isDone);
    const juce::String chapterId;

private:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    juce::String numeral, title, readingNow;
    bool current = false, done = false;
};

/** A chapter: Contents on the left, the chapter on the right. */
class ChapterView final : public juce::Component,
                          private juce::Timer
{
public:
    explicit ChapterView (CompressionAudioProcessor&);
    ~ChapterView() override;

    /** Rebuilds for the session's current chapter. */
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { repaint(); }
    juce::String text (const char* key) const;
    juce::String fill (const juce::String& templ, std::initializer_list<std::pair<juce::String, juce::String>> values) const;
    juce::String chapterText (const char* field) const;

    CompressionAudioProcessor& processor;
    const na::Content& content;

    juce::TextButton changeButton, previousButton, nextButton;
    juce::OwnedArray<ChapterRow> rows;

    juce::Slider knob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAttachment;

    juce::var chapter, goal;
    juce::String knobName, targetCaption;

    juce::Rectangle<int> leftPage, rightPage, knobBlock, textBlock, setSoFarArea;
};

} // namespace na::vol4
