#pragma once

#include "PluginProcessor.h"
#include "ui/MetricsOverlay.h"

// Plain test editor: JUCE's generic sliders for every parameter, a live
// gain-reduction meter and (NA_DEBUG_OVERLAY) the analysis debug overlay.
// The book UI replaces this in Milestone 6.
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
   #if NA_DEBUG_OVERLAY
    na::MetricsOverlay overlay;
   #endif

    juce::String title;      // from the content file
    juce::String grLabel;    // from the content file
    juce::String dbUnit;     // from the content file
    float grDisplayDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompressionAudioProcessorEditor)
};
