#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "content/Content.h"
#include "dsp/Compressor.h"

class CompressionAudioProcessor final : public juce::AudioProcessor
{
public:
    CompressionAudioProcessor();
    ~CompressionAudioProcessor() override = default;

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
    juce::AudioProcessorValueTreeState& getParameters() noexcept { return parameters; }

    /** Peak gain reduction (dB) since the last call. Message thread only. */
    float takeGainReductionPeakDb() noexcept { return grPeakForUi.exchange (0.0f); }

private:
    na::vol4::CompressorSettings readSettings() const noexcept;

    const na::Content content;  // must be built before `parameters`, which reads names from it
    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float>* threshold = nullptr;
    std::atomic<float>* ratio     = nullptr;
    std::atomic<float>* attack    = nullptr;
    std::atomic<float>* release   = nullptr;
    std::atomic<float>* makeup    = nullptr;
    std::atomic<float>* mix       = nullptr;

    na::vol4::Compressor compressor;
    std::atomic<float> grPeakForUi { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompressionAudioProcessor)
};
