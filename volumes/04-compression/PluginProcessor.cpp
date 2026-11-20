#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"
#include "BinaryData.h"

using namespace na::vol4;

CompressionAudioProcessor::CompressionAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      content (na::Content::fromJson (BinaryData::compression_json, (size_t) BinaryData::compression_jsonSize)),
      parameters (*this, nullptr, "PARAMETERS", createParameterLayout (content))
{
    threshold = parameters.getRawParameterValue (ParamID::threshold);
    ratio     = parameters.getRawParameterValue (ParamID::ratio);
    attack    = parameters.getRawParameterValue (ParamID::attack);
    release   = parameters.getRawParameterValue (ParamID::release);
    makeup    = parameters.getRawParameterValue (ParamID::makeup);
    mix       = parameters.getRawParameterValue (ParamID::mix);
}

void CompressionAudioProcessor::prepareToPlay (double sampleRate, int)
{
    compressor.prepare (sampleRate);
    setLatencySamples (0);
}

void CompressionAudioProcessor::releaseResources()
{
    compressor.reset();
}

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

CompressorSettings CompressionAudioProcessor::readSettings() const noexcept
{
    CompressorSettings s;
    s.thresholdDb = threshold->load (std::memory_order_relaxed);
    s.ratio       = ratio->load (std::memory_order_relaxed);
    s.attackMs    = attack->load (std::memory_order_relaxed);
    s.releaseMs   = release->load (std::memory_order_relaxed);
    s.makeupDb    = makeup->load (std::memory_order_relaxed);
    s.mixPct      = mix->load (std::memory_order_relaxed);
    return s;
}

void CompressionAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Use the channel layout as it is now, not as it was in prepareToPlay:
    // the AU can switch layouts without preparing again (Front of House lesson).
    const auto numIn  = getTotalNumInputChannels();
    const auto numOut = getTotalNumOutputChannels();
    const auto n      = buffer.getNumSamples();

    if (numIn < 1 || numOut < 1 || n == 0 || buffer.getNumChannels() < juce::jmax (numIn, numOut))
        return;

    // Mono→stereo: copy the mono input to both sides, then compress both (linked).
    if (numIn == 1 && numOut == 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);
    else
        for (auto ch = numIn; ch < numOut; ++ch)
            buffer.clear (ch, 0, n);

    compressor.setSettings (readSettings());
    compressor.process (buffer.getArrayOfWritePointers(), juce::jmin (numOut, Compressor::maxChannels), n);

    const auto gr = compressor.getBlockMaxGainReductionDb();
    if (gr > grPeakForUi.load (std::memory_order_relaxed))
        grPeakForUi.store (gr, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* CompressionAudioProcessor::createEditor()
{
    return new CompressionAudioProcessorEditor (*this);
}

void CompressionAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CompressionAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

// Called by the plugin wrappers to create a new instance.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CompressionAudioProcessor();
}
