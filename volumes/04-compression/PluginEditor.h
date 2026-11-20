#pragma once

#include "PluginProcessor.h"

// Milestone 1: a plain window showing the volume title, so we can confirm
// the plugin loads. The book UI replaces this in Milestone 6.
class CompressionAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CompressionAudioProcessorEditor (CompressionAudioProcessor&);
    ~CompressionAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;

private:
    juce::String title; // read from content/compression.json, never hard-coded

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompressionAudioProcessorEditor)
};
