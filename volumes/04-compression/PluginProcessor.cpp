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

    drainIn.resize (8192);
    drainOut.resize (8192);
    drainGr.resize (8192);
    metrics.prepare (44100.0);
    startTimerHz (30);
}

CompressionAudioProcessor::~CompressionAudioProcessor()
{
    stopTimer();
}

void CompressionAudioProcessor::prepareToPlay (double sampleRate, int)
{
    compressor.prepare (sampleRate);
    pendingSampleRate.store (sampleRate);
    setLatencySamples (0);
}

void CompressionAudioProcessor::timerCallback()
{
    // A new sample rate: re-prepare (allocates, fine here) and drop stale audio.
    if (const auto rate = pendingSampleRate.exchange (0.0); rate > 0.0)
    {
        metrics.prepare (rate);
        taps.discardAll();
    }

    float* dest[] = { drainIn.data(), drainOut.data(), drainGr.data() };
    for (int guard = 0; guard < 64; ++guard)
    {
        const auto n = taps.pop (dest, drainIn.size());
        if (n == 0)
            break;
        metrics.process (drainIn.data(), drainOut.data(), drainGr.data(), (int) n);
    }
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

    const auto numCh = juce::jmin (numOut, Compressor::maxChannels);
    auto* const* channels = buffer.getArrayOfWritePointers();
    float blockMaxGr = 0.0f;

    // In chunks, so the fixed-size tap buffers never need to grow.
    for (int start = 0; start < n; start += tapChunk)
    {
        const auto len = juce::jmin (tapChunk, n - start);
        float* chunk[Compressor::maxChannels] = {};
        for (int ch = 0; ch < numCh; ++ch)
            chunk[ch] = channels[ch] + start;

        // `in` tap: mono sum before compression
        for (int i = 0; i < len; ++i)
            tapIn[(size_t) i] = numCh == 2 ? 0.5f * (chunk[0][i] + chunk[1][i]) : chunk[0][i];

        compressor.process (chunk, numCh, len, tapGr.data(), tapWetGain.data());
        blockMaxGr = juce::jmax (blockMaxGr, compressor.getBlockMaxGainReductionDb());

        // `out` tap: after makeup, before Mix (the gain is linked, so in × wet gain)
        for (int i = 0; i < len; ++i)
            tapOut[(size_t) i] = tapIn[(size_t) i] * tapWetGain[(size_t) i];

        const float* streams[] = { tapIn.data(), tapOut.data(), tapGr.data() };
        taps.push (streams, (size_t) len); // drops (never blocks) if analysis falls behind
    }

    if (blockMaxGr > grPeakForUi.load (std::memory_order_relaxed))
        grPeakForUi.store (blockMaxGr, std::memory_order_relaxed);
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
