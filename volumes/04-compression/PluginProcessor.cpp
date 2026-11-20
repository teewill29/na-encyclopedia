#include "PluginProcessor.h"
#include "PluginEditor.h"

CompressionAudioProcessor::CompressionAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void CompressionAudioProcessor::prepareToPlay (double, int) {}
void CompressionAudioProcessor::releaseResources() {}

bool CompressionAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Mono tracks are the norm for vocals, bass and kick, and Logic hides
    // plugins that can't run on them. Supported: mono→mono, mono→stereo,
    // stereo→stereo.
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    const auto isMonoOrStereo = [] (const juce::AudioChannelSet& set)
    {
        return set == juce::AudioChannelSet::mono() || set == juce::AudioChannelSet::stereo();
    };

    if (! isMonoOrStereo (in) || ! isMonoOrStereo (out))
        return false;

    return in.size() <= out.size(); // no stereo→mono
}

void CompressionAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numIn  = getTotalNumInputChannels();
    const auto numOut = getTotalNumOutputChannels();

    // Pass-through: input channels are already in the buffer.
    // Mono→stereo: copy the mono input to both sides.
    if (numIn == 1 && numOut == 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());
    else
        for (auto ch = numIn; ch < numOut; ++ch)
            buffer.clear (ch, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* CompressionAudioProcessor::createEditor()
{
    return new CompressionAudioProcessorEditor (*this);
}

void CompressionAudioProcessor::getStateInformation (juce::MemoryBlock&) {}
void CompressionAudioProcessor::setStateInformation (const void*, int) {}

// Called by the plugin wrappers to create a new instance.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CompressionAudioProcessor();
}
