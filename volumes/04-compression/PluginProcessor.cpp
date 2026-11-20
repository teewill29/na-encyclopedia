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
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void CompressionAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Pass-through: input channels are already in the buffer. Only silence
    // any output channels that have no matching input.
    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
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
