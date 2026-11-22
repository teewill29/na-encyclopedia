#include "PluginProcessor.h"
#include "Parameters.h"
#include "Chapters.h"
#include "Feedback.h"
#include "content/Template.h"
#include "BinaryData.h"

using namespace na::vol4;

CompressionAudioProcessor::CompressionAudioProcessor()
    : BookProcessor (BinaryData::compression_json, (size_t) BinaryData::compression_jsonSize,
                     [] (const na::Content& c) { return createParameterLayout (c); })
{
    threshold = parameters.getRawParameterValue (ParamID::threshold);
    ratio     = parameters.getRawParameterValue (ParamID::ratio);
    attack    = parameters.getRawParameterValue (ParamID::attack);
    release   = parameters.getRawParameterValue (ParamID::release);
    makeup    = parameters.getRawParameterValue (ParamID::makeup);
    mix       = parameters.getRawParameterValue (ParamID::mix);
}

// ── DSP ──────────────────────────────────────────────────────────────────────
void CompressionAudioProcessor::prepareDsp (double sampleRate)
{
    compressor.prepare (sampleRate);
    listening.prepare (sampleRate);
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

void CompressionAudioProcessor::processChunk (float* const* channels, int numChannels, int numSamples,
                                              const float* const* dryIn, const na::book::Taps& t) noexcept
{
    compressor.setSettings (readSettings());
    listening.setMode (getListenMode());
    listening.setCompareGainDb (compareGainDb.load (std::memory_order_relaxed));

    compressor.process (channels, numChannels, numSamples, t.gr, wetGain.data(), t.grTarget, removed.data());

    // Compare / Hear what's removed (after the taps' sources are captured)
    listening.process (channels, dryIn, removed.data(), numChannels, numSamples);

    // `out` tap: after makeup, before Mix (the gain is linked, so in × wet gain)
    for (int i = 0; i < numSamples; ++i)
        t.out[i] = t.in[i] * wetGain[(size_t) i];
}

// ── Book hooks ───────────────────────────────────────────────────────────────
void CompressionAudioProcessor::chapterOpened (const juce::String& chapterId, bool firstVisit)
{
    if (firstVisit)
        applyStartValues (chapterId, session.getGoal(), parameters);
}

void CompressionAudioProcessor::sessionChanged()
{
    mixForcedToFull.store (na::vol4::isMixForcedToFull (session));
}

void CompressionAudioProcessor::analysisTick()
{
    // Compare plays the dry input turned up/down by the measured out − in loudness.
    if (const auto diff = metrics.get().levelDiffDb; std::isfinite (diff))
        compareGainDb.store (diff);
}

// ── Rules ────────────────────────────────────────────────────────────────────
const char* CompressionAudioProcessor::knobParamFor (const juce::String& chapterId) const
{
    const auto* rule = ruleFor (chapterId);
    return rule != nullptr ? rule->paramId : nullptr;
}

juce::String CompressionAudioProcessor::targetCaptionFor (const juce::String& chapterId) const
{
    const auto* rule = ruleFor (chapterId);
    const auto goal = session.getGoal();
    if (rule == nullptr || rule->targetKey == nullptr || ! goal[rule->targetKey].isObject())
        return {};

    const auto range = goal[rule->targetKey];
    const juce::String key (rule->targetKey);
    const auto values = na::Template::forValues ({ { "min", na::Template::formatNumber ((double) range["min"]) },
                                                   { "max", na::Template::formatNumber ((double) range["max"]) },
                                                   { "unit", content.getString ("book.units." + key) } });
    return na::Template::fill (content.getString (key == "grDb" ? "book.chapterPage.targetGr" : "book.chapterPage.target"), values);
}

juce::String CompressionAudioProcessor::readoutFor (const juce::String& chapterId) const
{
    // Chapter I: dynamic range of the input (FEEDBACK-ENGINE.md §2.2)
    const auto dr = metrics.get().dynamicRangeDb;
    if (chapterId != "goal" || std::isnan (dr))
        return {};
    return na::Template::fill (content.getString ("book.chapterPage.dynamicRange"),
                               na::Template::forValues ({ { "value", juce::String (dr, 1) } }));
}

std::string CompressionAudioProcessor::zoneFor (const juce::String& chapterId) const
{
    return Feedback::zoneFor (chapterId, session.getInstrument(), session.getGoal(), metrics.get(), readSettings());
}

std::string CompressionAudioProcessor::doneZonesFor (const juce::String& chapterId) const
{
    return Feedback::doneZonesFor (chapterId);
}

juce::String CompressionAudioProcessor::messageGroupFor (const juce::String& chapterId) const
{
    return Feedback::messageGroupFor (chapterId);
}

na::book::Tone CompressionAudioProcessor::toneFor (const juce::String& chapterId, const std::string& zone) const
{
    return Feedback::toneFor (chapterId, zone);
}

std::vector<na::book::Meter> CompressionAudioProcessor::metersFor (const juce::String& chapterId) const
{
    return Feedback::metersFor (chapterId, session.getGoal(), metrics.get(), readSettings());
}

juce::String CompressionAudioProcessor::hintFor (const juce::String& chapterId) const
{
    // Chapter I level check (docs/GAIN-STAGING.md §6): a hint, never a status.
    const auto peak = metrics.get().inPeakDb;
    if (chapterId != "goal" || std::isnan (peak))
        return {};
    if (peak > (float) (double) content.get ("levelCheck.hotAbovePeakDb"))
        return content.getString ("messages.levelCheck.hot");
    if (peak < (float) (double) content.get ("levelCheck.quietBelowPeakDb"))
        return content.getString ("messages.levelCheck.quiet");
    return {};
}

juce::StringArray CompressionAudioProcessor::setSoFarParamsFor (const juce::String& chapterId) const
{
    auto params = BookProcessor::setSoFarParamsFor (chapterId);
    if (chapterId == "ratioThreshold")
        params.add (ParamID::ratio);   // set by the goal in Chapter II
    return params;
}

// Called by the plugin wrappers to create a new instance.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CompressionAudioProcessor();
}
