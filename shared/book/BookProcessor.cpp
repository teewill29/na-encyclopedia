#include "BookProcessor.h"
#include "BookEditor.h"

namespace na::book
{

BookProcessor::BookProcessor (const char* contentJson, size_t contentSize, const LayoutFactory& layout)
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      content (Content::fromJson (contentJson, contentSize)),
      parameters (*this, nullptr, "PARAMETERS", layout (content))
{
    drainIn.resize (8192);
    drainOut.resize (8192);
    drainGr.resize (8192);
    drainTarget.resize (8192);
    metrics.prepare (44100.0);
    startTimerHz (30);
}

BookProcessor::~BookProcessor()
{
    aliveToken.reset();
    stopTimer();
}

void BookProcessor::prepareToPlay (double sampleRate, int)
{
    prepareDsp (sampleRate);
    pendingSampleRate.store (sampleRate);
    setLatencySamples (0);
}

void BookProcessor::releaseResources()
{
    resetDsp();
}

bool BookProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Mono tracks are the norm for vocals, bass and kick, and Logic hides
    // plugins that can't run on them. Supported: mono→mono, mono→stereo, stereo→stereo.
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const auto isMonoOrStereo = [] (const juce::AudioChannelSet& set)
    {
        return set == juce::AudioChannelSet::mono() || set == juce::AudioChannelSet::stereo();
    };
    return isMonoOrStereo (in) && isMonoOrStereo (out) && in.size() <= out.size();
}

void BookProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // The layout as it is now, not as it was in prepareToPlay (Front of House lesson).
    const auto numIn  = getTotalNumInputChannels();
    const auto numOut = getTotalNumOutputChannels();
    const auto n      = buffer.getNumSamples();
    if (numIn < 1 || numOut < 1 || n == 0 || buffer.getNumChannels() < juce::jmax (numIn, numOut))
        return;

    // Mono→stereo: copy the mono input to both sides, then process both.
    if (numIn == 1 && numOut == 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);
    else
        for (auto ch = numIn; ch < numOut; ++ch)
            buffer.clear (ch, 0, n);

    const auto numCh = juce::jmin (numOut, 2);
    auto* const* channels = buffer.getArrayOfWritePointers();

    // In chunks, so the fixed-size tap buffers never need to grow.
    for (int start = 0; start < n; start += tapChunk)
    {
        const auto len = juce::jmin (tapChunk, n - start);
        float* chunk[2] = {};
        for (int ch = 0; ch < numCh; ++ch)
            chunk[ch] = channels[ch] + start;

        // `in` tap: mono sum before processing; keep the dry input
        for (int i = 0; i < len; ++i)
            tapIn[(size_t) i] = numCh == 2 ? 0.5f * (chunk[0][i] + chunk[1][i]) : chunk[0][i];
        for (int ch = 0; ch < numCh; ++ch)
            std::copy (chunk[ch], chunk[ch] + len, dry[(size_t) ch].begin());
        std::fill (tapGr.begin(), tapGr.begin() + len, 0.0f);
        std::fill (tapTarget.begin(), tapTarget.begin() + len, 0.0f);

        const float* dryChannels[2] = { dry[0].data(), dry[1].data() };
        processChunk (chunk, numCh, len, dryChannels, { tapIn.data(), tapOut.data(), tapGr.data(), tapTarget.data() });

        const float* streams[] = { tapIn.data(), tapOut.data(), tapGr.data(), tapTarget.data() };
        taps.push (streams, (size_t) len);   // drops (never blocks) if the analysis falls behind
    }
}

juce::AudioProcessorEditor* BookProcessor::createEditor()
{
    return new BookEditor (*this);
}

void BookProcessor::timerCallback()
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
        if (n == 0) break;
        metrics.process (drainIn.data(), drainOut.data(), drainGr.data(), drainTarget.data(), (int) n);
        lastAudioArrived = juce::Time::getMillisecondCounterHiRes() * 0.001;
    }

    updateFeedback();
    analysisTick();
}

bool BookProcessor::isAudioArriving() const noexcept
{
    return lastAudioArrived >= 0.0
        && juce::Time::getMillisecondCounterHiRes() * 0.001 - lastAudioArrived < audioStoppedAfter;
}

void BookProcessor::updateFeedback()
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
        readingReset();
    }

    // Did the chapter's own knob move since the last tick?
    bool knobMoved = false;
    if (const auto* paramId = knobParamFor (chapter))
    {
        const auto v = parameters.getRawParameterValue (paramId)->load();
        knobMoved = ! std::isnan (lastKnobValue) && std::abs (v - lastKnobValue) > 1.0e-4f;
        lastKnobValue = v;
    }

    using Phase = FeedbackTracker::Phase;
    const auto& m = metrics.get();
    // No audio arriving (playback stopped) reads as silence straight away.
    const auto phase = ! isAudioArriving()                             ? Phase::silence
                     : m.state == analysis::Metrics::State::silence   ? Phase::silence
                     : m.state == analysis::Metrics::State::listening ? Phase::listening
                                                                      : Phase::evaluating;

    feedback.update (metrics.getSecondsAnalysed(), phase, zoneFor (chapter), knobMoved, doneZonesFor (chapter));

    if (feedback.isDone() && ! session.isDone (chapter))
        session.setDone (chapter, true);
}

Template::Lookup BookProcessor::templateLookup() const
{
    const auto goal = session.getGoal();
    const auto instrument = session.getInstrument();
    return Template::chain (Template::forValues ({ { "instrumentName", instrument["name"].toString() },
                                                   { "goalName", goal["name"].toString() } }),
                            Template::chain (Template::forGoal (goal), Template::forGoal (instrument)));
}

juce::StringArray BookProcessor::setSoFarParamsFor (const juce::String& chapterId) const
{
    juce::StringArray params;
    if (const auto* paramId = knobParamFor (chapterId))
        params.add (paramId);
    return params;
}

void BookProcessor::beginBook (const juce::String& instrumentId, const juce::String& goalId)
{
    session.setup (instrumentId, goalId);
    if (! session.getChapterIds().isEmpty())
        openChapter (session.getChapterIds()[0]);
    sessionChanged();
}

void BookProcessor::openChapter (const juce::String& chapterId)
{
    const bool firstVisit = session.goTo (chapterId);
    chapterOpened (chapterId, firstVisit);
    sessionChanged();
}

void BookProcessor::nextChapter()
{
    const auto i = session.getCurrentIndex();
    if (i >= 0 && i + 1 < session.getChapterIds().size())
        openChapter (session.getChapterIds()[i + 1]);
}

void BookProcessor::previousChapter()
{
    const auto i = session.getCurrentIndex();
    if (i > 0)
        openChapter (session.getChapterIds()[i - 1]);
}

void BookProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Parameters plus the book session (instrument, goal, chapter, progress).
    auto state = parameters.copyState();
    state.removeChild (state.getChildWithName (Session::treeType), nullptr);
    state.appendChild (session.toValueTree(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BookProcessor::setStateInformation (const void* data, int sizeInBytes)
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

void BookProcessor::restoreState (const juce::ValueTree& stateIn)
{
    auto state = stateIn.createCopy();
    const auto sessionTree = state.getChildWithName (Session::treeType);
    state.removeChild (sessionTree, nullptr);
    parameters.replaceState (state);
    if (sessionTree.isValid())
        session.fromValueTree (sessionTree);
    sessionChanged();
}

} // namespace na::book
