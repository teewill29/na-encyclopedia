#pragma once

#include "book/BookProcessor.h"
#include "dsp/Equalizer.h"

/** Vol. III, EQ (docs/EQ.md): a 5-band parametric EQ on the shared book.

    Build status: step 1. EQ, parameters, chapter knobs, start values and the
    spectrum view (EQ curve) work; the analysis and feedback come next. */
class EqProcessor final : public na::book::BookProcessor
{
public:
    EqProcessor();

    na::vol3::EqSettings getSettings() const noexcept { return readSettings(); }

    // ── Rules ───────────────────────────────────────────────────────────────
    const char* knobParamFor (const juce::String& chapterId) const override;
    juce::StringArray extraKnobsFor (const juce::String& chapterId) const override;
    juce::String targetCaptionFor (const juce::String& chapterId) const override;
    std::string zoneFor (const juce::String& chapterId) const override;
    na::book::Tone toneFor (const juce::String& chapterId, const std::string& zone) const override;
    std::vector<na::book::Meter> metersFor (const juce::String& chapterId) const override;
    bool spectrumFor (const juce::String& chapterId, na::book::SpectrumData&) const override;
    juce::StringArray setSoFarParamsFor (const juce::String& chapterId) const override;

    /** The band a chapter works on (1-5), or 0. */
    int bandFor (const juce::String& chapterId) const;

protected:
    void prepareDsp (double sampleRate) override;
    void resetDsp() override { equalizer.reset(); }
    void processChunk (float* const* channels, int numChannels, int numSamples,
                       const float* const* dry, const na::book::Taps&) noexcept override;
    void chapterOpened (const juce::String& chapterId, bool firstVisit) override;

private:
    na::vol3::EqSettings readSettings() const noexcept;
    void setParam (const juce::String& id, float value);

    struct BandParams { std::atomic<float>* freq; std::atomic<float>* gain; std::atomic<float>* q; std::atomic<float>* on; };
    std::array<BandParams, 5> bandParams {};
    std::atomic<float>* band1Shape = nullptr;
    std::atomic<float>* band5Shape = nullptr;
    std::atomic<float>* output = nullptr;

    na::vol3::Equalizer equalizer;
    double currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqProcessor)
};
