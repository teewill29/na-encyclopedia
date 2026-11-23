#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "content/Content.h"
#include "analysis/Metrics.h"
#include "analysis/TapFifo.h"
#include "analysis/SpectrumAnalyser.h"
#include "chapters/Session.h"
#include "chapters/FeedbackTracker.h"
#include "dsp/ListeningTools.h"
#include "Rules.h"
#include "content/Template.h"

#include <array>
#include <functional>
#include <vector>

namespace na::book
{

/** Aligned analysis taps for one chunk (FEEDBACK-ENGINE.md §1). `in` is filled
    by the base; the volume fills the rest in processChunk(). */
struct Taps
{
    const float* in;
    float* out;          // mono output the volume wants measured
    float* gr;           // gain reduction, dB (0 if the volume has none)
    float* grTarget;     // instant-attack reduction, dB (0 if none)
};

/** Everything a volume of the book has in common: the session, the analysis
    taps and metrics, the feedback state machine, stopped-playback detection,
    channel layouts and saved state. A volume supplies its DSP and its rules.

    Rules and book calls are message-thread only; processChunk() is the audio thread.
*/
class BookProcessor : public juce::AudioProcessor,
                      private juce::Timer
{
public:
    using LayoutFactory = std::function<juce::AudioProcessorValueTreeState::ParameterLayout (const Content&)>;
    using ListenMode = dsp::ListeningTools::Mode;

    BookProcessor (const char* contentJson, size_t contentSize, const LayoutFactory&);
    ~BookProcessor() override;

    // ── AudioProcessor ──────────────────────────────────────────────────────
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ── Shared state ────────────────────────────────────────────────────────
    const Content& getContent() const noexcept { return content; }
    juce::AudioProcessorValueTreeState& getValueTree() noexcept { return parameters; }
    Session& getSession() noexcept { return session; }

    void beginBook (const juce::String& instrumentId, const juce::String& goalId);
    void openChapter (const juce::String& chapterId);
    void nextChapter();
    void previousChapter();

    const FeedbackTracker::Display& getFeedback() const noexcept { return feedback.getDisplay(); }
    bool isListening() const noexcept { return isAudioArriving() && metrics.get().inputActive; }
    bool isAudioArriving() const noexcept;
    const analysis::Metrics& getMetrics() const noexcept { return metrics.get(); }
    /** The averaged spectrum of `in` and `out` (only runs for volumes that call enableSpectrum()). */
    const analysis::SpectrumAnalyser& getSpectrum() const noexcept { return spectrum; }
    /** Extra rows for the debug overlay (label, value), formatted by the volume. */
    virtual std::vector<std::pair<juce::String, juce::String>> debugRows() const { return {}; }
    unsigned getDroppedAnalysisBlocks() const noexcept { return taps.getDroppedBlocks(); }

    // ── Volume rules (message thread) ───────────────────────────────────────
    /** The chapter's main knob (parameter ID), or nullptr. */
    virtual const char* knobParamFor (const juce::String& chapterId) const = 0;
    /** Up to two smaller knobs shown beside the main one (e.g. an EQ band's Gain and Width). */
    virtual juce::StringArray extraKnobsFor (const juce::String& chapterId) const { juce::ignoreUnused (chapterId); return {}; }
    /** A spectrum for The Reading instead of meters (docs/EQ.md §6); false for none. */
    virtual bool spectrumFor (const juce::String& chapterId, SpectrumData& out) const { juce::ignoreUnused (chapterId, out); return false; }
    /** Fills {placeholders} in chapter text: the goal's fields, then the instrument's,
        plus {instrumentName} and {goalName}. */
    virtual Template::Lookup templateLookup() const;
    /** Caption under the knob ("Target: 15–30 ms"), or empty. */
    virtual juce::String targetCaptionFor (const juce::String& chapterId) const = 0;
    /** Text in place of a knob for chapters without one ("9.4 dB of movement"); empty while waiting. */
    virtual juce::String readoutFor (const juce::String& chapterId) const { juce::ignoreUnused (chapterId); return {}; }
    /** The zone the current reading falls in, "" if none yet. */
    virtual std::string zoneFor (const juce::String& chapterId) const = 0;
    /** Zones that earn the tick (space-separated). */
    virtual std::string doneZonesFor (const juce::String& chapterId) const { juce::ignoreUnused (chapterId); return "onTarget"; }
    /** Group in content "messages" and "book.reading.status" for a chapter. */
    virtual juce::String messageGroupFor (const juce::String& chapterId) const { return chapterId; }
    virtual Tone toneFor (const juce::String& chapterId, const std::string& zone) const = 0;
    virtual std::vector<Meter> metersFor (const juce::String& chapterId) const = 0;
    /** Parameters shown in "Set so far" for a finished chapter (default: its knob). */
    virtual juce::StringArray setSoFarParamsFor (const juce::String& chapterId) const;
    /** An extra line in The Reading (e.g. Vol. IV: "gain stage it first"); empty for none.
        mayHint() reserves the space so the page doesn't jump when a hint appears. */
    virtual bool mayHint (const juce::String& chapterId) const { juce::ignoreUnused (chapterId); return false; }
    virtual juce::String hintFor (const juce::String& chapterId) const { juce::ignoreUnused (chapterId); return {}; }
    /** Listening tools (FEEDBACK-ENGINE.md §6), if the volume has them. */
    virtual bool hasListeningTools() const { return false; }
    void setListenMode (ListenMode m) noexcept { listenMode.store ((int) m); }
    ListenMode getListenMode() const noexcept { return (ListenMode) listenMode.load(); }

protected:
    // ── Volume hooks ────────────────────────────────────────────────────────
    virtual void prepareDsp (double sampleRate) = 0;
    virtual void resetDsp() {}
    /** Audio thread. Process `channels` in place (1 or 2), and fill taps.out/gr/grTarget.
        `dry` holds the input as it was before processing. */
    virtual void processChunk (float* const* channels, int numChannels, int numSamples,
                               const float* const* dry, const Taps&) noexcept = 0;
    /** A chapter was opened (firstVisit: apply its start values). */
    virtual void chapterOpened (const juce::String& chapterId, bool firstVisit) { juce::ignoreUnused (chapterId, firstVisit); }
    /** Session or chapter changed (any cause, including a project reload). */
    virtual void sessionChanged() {}
    /** After each analysis tick (30 Hz), message thread. */
    virtual void analysisTick() {}
    /** The current chapter's reading was reset (new chapter, new sample rate). */
    virtual void readingReset() {}

    const Content content;
    juce::AudioProcessorValueTreeState parameters;
    Session session { content };
    analysis::MetricsEngine metrics;           // message thread only
    analysis::SpectrumAnalyser spectrum;       // message thread only
    /** Call from the volume's constructor to run the spectrum analysis (docs/EQ.md §4). */
    void enableSpectrum() noexcept { spectrumOn = true; }
    std::atomic<int> listenMode { 0 };

private:
    void timerCallback() override;
    void updateFeedback();
    void restoreState (const juce::ValueTree&);

    std::shared_ptr<int> aliveToken = std::make_shared<int> (0);

    static constexpr int tapChunk = 1024;
    std::array<float, tapChunk> tapIn {}, tapOut {}, tapGr {}, tapTarget {};
    std::array<std::array<float, tapChunk>, 2> dry {};
    analysis::TapFifo<4> taps { 1u << 17 };

    FeedbackTracker feedback;
    juce::String feedbackChapter;
    float lastKnobValue = std::numeric_limits<float>::quiet_NaN();
    std::vector<float> drainIn, drainOut, drainGr, drainTarget;
    std::atomic<double> pendingSampleRate { 0.0 };
    double lastAudioArrived = -1.0;
    bool spectrumOn = false;
    static constexpr double audioStoppedAfter = 0.3;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BookProcessor)
};

} // namespace na::book
