#pragma once

#include "book/BookProcessor.h"

/** Vol. I, Gain Staging (docs/GAIN-STAGING.md): one Trim knob. Everything
    else is the shared book.

    Feedback rules: docs/GAIN-STAGING.md §5. */
class GainStagingProcessor final : public na::book::BookProcessor
{
public:
    GainStagingProcessor();

    // ── Rules ───────────────────────────────────────────────────────────────
    const char* knobParamFor (const juce::String& chapterId) const override;
    juce::String targetCaptionFor (const juce::String& chapterId) const override;
    juce::String readoutFor (const juce::String& chapterId) const override;
    std::string zoneFor (const juce::String& chapterId) const override;
    std::string doneZonesFor (const juce::String& chapterId) const override;
    na::book::Tone toneFor (const juce::String& chapterId, const std::string& zone) const override;
    std::vector<na::book::Meter> metersFor (const juce::String& chapterId) const override;

protected:
    void prepareDsp (double sampleRate) override;
    void readingReset() override;
    void analysisTick() override;
    void processChunk (float* const* channels, int numChannels, int numSamples,
                       const float* const* dry, const na::book::Taps&) noexcept override;

private:
    std::atomic<float>* trim = nullptr;
    float currentGain = 1.0f;     // audio thread: ramped toward the knob
    float rampCoeff = 0.999f;
    float trimAtHoldReset = 0.0f;   // Chapter IV: moving the trim > 1 dB restarts the peak hold

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainStagingProcessor)
};
