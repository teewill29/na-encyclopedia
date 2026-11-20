#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"
#include "Chapters.h"
#include "Feedback.h"
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
    drainTarget.resize (8192);
    metrics.prepare (44100.0);
    startTimerHz (30);
}

CompressionAudioProcessor::~CompressionAudioProcessor()
{
    aliveToken.reset();
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
        feedbackChapter = {};   // the audio clock restarted: start the chapter's reading over
    }

    float* dest[] = { drainIn.data(), drainOut.data(), drainGr.data(), drainTarget.data() };
    for (int guard = 0; guard < 64; ++guard)
    {
        const auto n = taps.pop (dest, drainIn.size());
        if (n == 0)
            break;
        metrics.process (drainIn.data(), drainOut.data(), drainGr.data(), drainTarget.data(), (int) n);
    }

    updateFeedback();
}

void CompressionAudioProcessor::updateFeedback()
{
    if (session.isOnTitlePage())
    {
        feedbackChapter = {};
        return;
    }

    const auto chapter = session.getCurrentChapterId();
    if (chapter != feedbackChapter)
    {
        feedback.reset();
        feedbackChapter = chapter;
        lastKnobValue = std::numeric_limits<float>::quiet_NaN();
    }

    // Did the chapter's own knob move since the last tick?
    bool knobMoved = false;
    if (const auto* rule = na::vol4::ruleFor (chapter); rule != nullptr && rule->paramId != nullptr)
    {
        const auto v = parameters.getRawParameterValue (rule->paramId)->load();
        knobMoved = ! std::isnan (lastKnobValue) && std::abs (v - lastKnobValue) > 1.0e-4f;
        lastKnobValue = v;
    }

    using Phase = na::FeedbackTracker::Phase;
    const auto& m = metrics.get();
    const auto phase = m.state == na::analysis::Metrics::State::silence   ? Phase::silence
                     : m.state == na::analysis::Metrics::State::listening ? Phase::listening
                                                                          : Phase::evaluating;
    const auto zone = na::vol4::Feedback::zoneFor (chapter, session.getInstrument(), session.getGoal(), m, readSettings());

    feedback.update (metrics.getSecondsAnalysed(), phase, zone, knobMoved,
                     na::vol4::Feedback::doneZonesFor (chapter));

    if (feedback.isDone() && ! session.isDone (chapter))
        session.setDone (chapter, true);
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
    s.mixPct      = mixForcedToFull.load (std::memory_order_relaxed) ? 100.0f
                                                                     : mix->load (std::memory_order_relaxed);
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

        compressor.process (chunk, numCh, len, tapGr.data(), tapWetGain.data(), tapTarget.data());
        blockMaxGr = juce::jmax (blockMaxGr, compressor.getBlockMaxGainReductionDb());

        // `out` tap: after makeup, before Mix (the gain is linked, so in × wet gain)
        for (int i = 0; i < len; ++i)
            tapOut[(size_t) i] = tapIn[(size_t) i] * tapWetGain[(size_t) i];

        const float* streams[] = { tapIn.data(), tapOut.data(), tapGr.data(), tapTarget.data() };
        taps.push (streams, (size_t) len); // drops (never blocks) if analysis falls behind
    }

    if (blockMaxGr > grPeakForUi.load (std::memory_order_relaxed))
        grPeakForUi.store (blockMaxGr, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* CompressionAudioProcessor::createEditor()
{
    return new CompressionAudioProcessorEditor (*this);
}

void CompressionAudioProcessor::beginBook (const juce::String& instrumentId, const juce::String& goalId)
{
    session.setup (instrumentId, goalId);
    if (! session.getChapterIds().isEmpty())
        openChapter (session.getChapterIds()[0]);
    updateMixForce();
}

void CompressionAudioProcessor::openChapter (const juce::String& chapterId)
{
    if (session.goTo (chapterId))
        na::vol4::applyStartValues (chapterId, session.getGoal(), parameters);
    updateMixForce();
}

void CompressionAudioProcessor::nextChapter()
{
    const auto i = session.getCurrentIndex();
    if (i >= 0 && i + 1 < session.getChapterIds().size())
        openChapter (session.getChapterIds()[i + 1]);
}

void CompressionAudioProcessor::previousChapter()
{
    const auto i = session.getCurrentIndex();
    if (i > 0)
        openChapter (session.getChapterIds()[i - 1]);
}

void CompressionAudioProcessor::updateMixForce()
{
    mixForcedToFull.store (na::vol4::isMixForcedToFull (session));
}

void CompressionAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Parameters plus the book session (instrument, goal, chapter, progress).
    auto state = parameters.copyState();
    state.removeChild (state.getChildWithName (na::Session::treeType), nullptr);
    state.appendChild (session.toValueTree(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CompressionAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    // The session is message-thread only; hosts usually restore there, but not always.
    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        restoreState (state);
    else
        juce::MessageManager::callAsync ([this, state, alive = std::weak_ptr<int> (aliveToken)]
                                         { if (alive.lock()) restoreState (state); });
}

void CompressionAudioProcessor::restoreState (const juce::ValueTree& stateIn)
{
    auto state = stateIn.createCopy();
    const auto sessionTree = state.getChildWithName (na::Session::treeType);
    state.removeChild (sessionTree, nullptr);
    parameters.replaceState (state);
    if (sessionTree.isValid())
        session.fromValueTree (sessionTree);
    updateMixForce();
}

// Called by the plugin wrappers to create a new instance.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CompressionAudioProcessor();
}
