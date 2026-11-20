#pragma once

#include "PluginProcessor.h"

// Milestone 2: plain test editor. JUCE's generic sliders for every parameter
// plus a live gain-reduction readout. The book UI replaces this in Milestone 6.
class CompressionAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              private juce::Timer
{
public:
    explicit CompressionAudioProcessorEditor (CompressionAudioProcessor&);
    ~CompressionAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    CompressionAudioProcessor& processor;
    juce::GenericAudioProcessorEditor generic;

    juce::String title;      // from the content file
    juce::String grLabel;    // from the content file
    juce::String dbUnit;     // from the content file
    float grDisplayDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompressionAudioProcessorEditor)
};
