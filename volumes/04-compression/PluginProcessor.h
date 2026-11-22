#pragma once

#include "book/BookProcessor.h"
#include "dsp/Compressor.h"
#include "dsp/ListeningTools.h"

/** Vol. IV, Compression: the compressor, the listening tools and the
    compression chapters' rules. Everything else is the shared book. */
class CompressionAudioProcessor final : public na::book::BookProcessor
{
public:
    CompressionAudioProcessor();

    bool isMixForcedToFull() const noexcept { return mixForcedToFull.load(); }
    na::vol4::CompressorSettings getSettings() const noexcept { return readSettings(); }

    // ── Rules ───────────────────────────────────────────────────────────────
    const char* knobParamFor (const juce::String& chapterId) const override;
    juce::String targetCaptionFor (const juce::String& chapterId) const override;
    juce::String readoutFor (const juce::String& chapterId) const override;
    std::string zoneFor (const juce::String& chapterId) const override;
    std::string doneZonesFor (const juce::String& chapterId) const override;
    juce::String messageGroupFor (const juce::String& chapterId) const override;
    na::book::Tone toneFor (const juce::String& chapterId, const std::string& zone) const override;
    std::vector<na::book::Meter> metersFor (const juce::String& chapterId) const override;
    juce::StringArray setSoFarParamsFor (const juce::String& chapterId) const override;
    bool hasListeningTools() const override { return true; }
    bool mayHint (const juce::String& chapterId) const override { return chapterId == "goal"; }
    juce::String hintFor (const juce::String& chapterId) const override;

protected:
    void prepareDsp (double sampleRate) override;
    void resetDsp() override { compressor.reset(); }
    void processChunk (float* const* channels, int numChannels, int numSamples,
                       const float* const* dry, const na::book::Taps&) noexcept override;
    void chapterOpened (const juce::String& chapterId, bool firstVisit) override;
    void sessionChanged() override;
    void analysisTick() override;

private:
    na::vol4::CompressorSettings readSettings() const noexcept;

    std::atomic<float>* threshold = nullptr;
    std::atomic<float>* ratio     = nullptr;
    std::atomic<float>* attack    = nullptr;
    std::atomic<float>* release   = nullptr;
    std::atomic<float>* makeup    = nullptr;
    std::atomic<float>* mix       = nullptr;

    std::atomic<bool> mixForcedToFull { false };   // parallel goals, Chapters II-IV
    std::atomic<float> compareGainDb { 0.0f };     // level match for Compare, from the analysis

    na::vol4::Compressor compressor;
    na::dsp::ListeningTools listening;
    std::array<float, 1024> wetGain {}, removed {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompressionAudioProcessor)
};
