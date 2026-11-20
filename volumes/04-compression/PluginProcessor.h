#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "content/Content.h"
#include "analysis/Metrics.h"
#include "analysis/TapFifo.h"
#include "chapters/Session.h"
#include "chapters/FeedbackTracker.h"
#include "dsp/Compressor.h"

#include <array>

class CompressionAudioProcessor final : public juce::AudioProcessor,
                                        private juce::Timer
{
public:
    CompressionAudioProcessor();
    ~CompressionAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

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

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    const na::Content& getContent() const noexcept { return content; }
    juce::AudioProcessorValueTreeState& getValueTree() noexcept { return parameters; }

    /** Peak gain reduction (dB) since the last call. Message thread only. */
    float takeGainReductionPeakDb() noexcept { return grPeakForUi.exchange (0.0f); }

    // ── The book (message thread only) ──────────────────────────────────────
    na::Session& getSession() noexcept { return session; }
    /** Title Page → "Begin": picks instrument + goal and opens the first chapter. */
    void beginBook (const juce::String& instrumentId, const juce::String& goalId);
    /** Opens a chapter, applying its start values on the first visit. */
    void openChapter (const juce::String& chapterId);
    void nextChapter();
    void previousChapter();
    bool isMixForcedToFull() const noexcept { return mixForcedToFull.load(); }

    /** The Reading for the current chapter (FEEDBACK-ENGINE.md §3). Message thread only. */
    const na::FeedbackTracker::Display& getFeedback() const noexcept { return feedback.getDisplay(); }

    /** True while audio is arriving and isn't silent: "Listening live".
        Logic stops calling the plugin when playback stops, so the analysis
        never hears the silence; this notices that audio stopped arriving. */
    bool isListening() const noexcept { return isAudioArriving() && metrics.get().inputActive; }
    bool isAudioArriving() const noexcept;

    /** Current knob values (as the compressor sees them). */
    na::vol4::CompressorSettings getSettings() const noexcept { return readSettings(); }

    /** Latest analysis (FEEDBACK-ENGINE.md §2). Message thread only. */
    const na::analysis::Metrics& getMetrics() const noexcept { return metrics.get(); }
    unsigned getDroppedAnalysisBlocks() const noexcept { return taps.getDroppedBlocks(); }

private:
    na::vol4::CompressorSettings readSettings() const noexcept;
    void timerCallback() override;   // drains the taps into the metrics, 30 Hz
    void updateMixForce();
    void updateFeedback();
    void restoreState (const juce::ValueTree& state);

    const na::Content content;  // must be built before `parameters`, which reads names from it
    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float>* threshold = nullptr;
    std::atomic<float>* ratio     = nullptr;
    std::atomic<float>* attack    = nullptr;
    std::atomic<float>* release   = nullptr;
    std::atomic<float>* makeup    = nullptr;
    std::atomic<float>* mix       = nullptr;

    na::Session session { content };
    std::atomic<bool> mixForcedToFull { false };  // parallel goals, Chapters II-IV
    std::shared_ptr<int> aliveToken = std::make_shared<int> (0); // guards async callbacks

    na::vol4::Compressor compressor;
    std::atomic<float> grPeakForUi { 0.0f };

    // Analysis taps (FEEDBACK-ENGINE.md §1): in (mono), out (mono, after
    // makeup, before Mix), gr (dB), grTarget (dB, instant-attack reduction).
    // Audio thread → FIFO → message thread.
    static constexpr int tapChunk = 1024;
    std::array<float, tapChunk> tapIn {}, tapOut {}, tapGr {}, tapTarget {}, tapWetGain {};
    na::analysis::TapFifo<4> taps { 1u << 17 };   // ~2.7 s at 48 kHz

    na::analysis::MetricsEngine metrics;           // message thread only
    na::FeedbackTracker feedback;                  // message thread only
    juce::String feedbackChapter;                  // chapter the tracker belongs to
    float lastKnobValue = std::numeric_limits<float>::quiet_NaN();
    std::vector<float> drainIn, drainOut, drainGr, drainTarget; // message thread only
    std::atomic<double> pendingSampleRate { 0.0 }; // set by prepareToPlay, applied by the timer
    double lastAudioArrived = -1.0;                // wall clock, seconds (message thread)
    static constexpr double audioStoppedAfter = 0.3;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompressionAudioProcessor)
};
