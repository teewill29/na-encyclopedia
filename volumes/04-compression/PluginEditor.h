#pragma once

#include "PluginProcessor.h"
#include "BookViews.h"
#include "ui/MetricsOverlay.h"

// The plugin window: Title Page or a chapter, depending on the session.
// Functional and unstyled until the book UI in Milestone 6.
class CompressionAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              private juce::ChangeListener
{
public:
    explicit CompressionAudioProcessorEditor (CompressionAudioProcessor&);
    ~CompressionAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { showCurrentPage(); }
    void showCurrentPage();

    CompressionAudioProcessor& processor;
    na::vol4::TitleView titleView;
    na::vol4::ChapterView chapterView;

   #if NA_DEBUG_OVERLAY
    na::MetricsOverlay overlay;
    juce::TextButton debugButton;
   #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompressionAudioProcessorEditor)
};
