#include "PluginProcessor.h"
#include "content/Template.h"
#include "BinaryData.h"

#include <cmath>
#include <limits>

namespace
{
    constexpr const char* trimId = "trim";

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout (const na::Content& content)
    {
        // Trim: −24 to +24 dB, default 0 (docs/GAIN-STAGING.md §2). Name and unit from the content file.
        const auto unit = content.getString ("parameters.trim.unit");
        auto attributes = juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([unit] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + unit; })
            .withValueFromStringFunction ([] (const juce::String& t) { return t.retainCharacters ("0123456789.-").getFloatValue(); });

        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { trimId, 1 },
                                                                 content.getString ("parameters.trim.name", trimId),
                                                                 juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f),
                                                                 0.0f, attributes));
        return layout;
    }

    bool usesTrim (const juce::String& chapterId)
    {
        return chapterId == "peaks" || chapterId == "average" || chapterId == "headroom";
    }
}

GainStagingProcessor::GainStagingProcessor()
    : BookProcessor (BinaryData::gainstaging_json, (size_t) BinaryData::gainstaging_jsonSize, createLayout)
{
    trim = parameters.getRawParameterValue (trimId);
}

// ── DSP: a smoothed gain ─────────────────────────────────────────────────────
void GainStagingProcessor::prepareDsp (double sampleRate)
{
    rampCoeff = (float) std::exp (-1.0 / (0.02 * sampleRate));   // ~20 ms glide, never clicks
    currentGain = std::pow (10.0f, trim->load() * 0.05f);
}

void GainStagingProcessor::processChunk (float* const* channels, int numChannels, int numSamples,
                                         const float* const*, const na::book::Taps& t) noexcept
{
    const float target = std::pow (10.0f, trim->load (std::memory_order_relaxed) * 0.05f);
    for (int i = 0; i < numSamples; ++i)
    {
        currentGain = target + rampCoeff * (currentGain - target);
        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][i] *= currentGain;
        t.out[i] = t.in[i] * currentGain;
    }
}

// ── Rules (docs/GAIN-STAGING.md §5) ─────────────────────────────────────────
namespace
{
    constexpr float none = std::numeric_limits<float>::quiet_NaN();
    float num (const juce::var& v) { return v.isVoid() || v.isUndefined() ? none : (float) (double) v; }

    std::string band (float value, const juce::var& range)
    {
        if (std::isnan (value) || ! range.isObject()) return {};
        if (value < num (range["min"])) return "low";
        if (value > num (range["max"])) return "high";
        return "onTarget";
    }

    constexpr float hotAboveDb = -1.0f;       // Chapter I "too hot"
    constexpr float quietBelowMinBy = 10.0f;  // Chapter I "too quiet": 10 dB under the peak target
    constexpr float clipDb = -0.1f;           // Chapter IV "clipping"
    constexpr float headroomListenSeconds = 10.0f;
}

void GainStagingProcessor::readingReset()
{
    metrics.resetPeakHold();
    trimAtHoldReset = trim->load();
}

void GainStagingProcessor::analysisTick()
{
    // Chapter IV: a real trim move (> 1 dB) starts the "loudest peak so far" over.
    if (session.getCurrentChapterId() == "headroom" && std::abs (trim->load() - trimAtHoldReset) > 1.0f)
        readingReset();
}

const char* GainStagingProcessor::knobParamFor (const juce::String& chapterId) const
{
    return usesTrim (chapterId) ? trimId : nullptr;
}

juce::String GainStagingProcessor::targetCaptionFor (const juce::String& chapterId) const
{
    const auto goal = session.getGoal();
    if (chapterId == "headroom" && goal.hasProperty ("headroomMinDb"))
        return na::Template::fill (content.getString ("book.chapterPage.targetHeadroom"),
                                   na::Template::forValues ({ { "min", na::Template::formatNumber ((double) goal["headroomMinDb"]) } }));
    const char* key = chapterId == "peaks" ? "peakDb" : chapterId == "average" ? "averageDb" : nullptr;
    if (key == nullptr || ! goal[key].isObject())
        return {};
    return na::Template::fill (content.getString ("book.chapterPage.target"),
                               na::Template::forValues ({ { "min", na::Template::formatNumber ((double) goal[key]["min"]) },
                                                          { "max", na::Template::formatNumber ((double) goal[key]["max"]) },
                                                          { "unit", content.getString (juce::String ("book.units.") + key) } }));
}

juce::String GainStagingProcessor::readoutFor (const juce::String& chapterId) const
{
    // Chapter I: where the track arrives, before the trim
    const auto& m = metrics.get();
    if (chapterId != "where" || std::isnan (m.inPeakDb) || std::isnan (m.inAverageDb))
        return {};
    return na::Template::fill (content.getString ("book.chapterPage.readout"),
                               na::Template::forValues ({ { "peak", juce::String (m.inPeakDb, 1) },
                                                          { "average", juce::String (m.inAverageDb, 1) } }));
}

std::string GainStagingProcessor::zoneFor (const juce::String& chapterId) const
{
    const auto& m = metrics.get();
    const auto goal = session.getGoal();

    if (chapterId == "where")
    {
        if (std::isnan (m.inPeakDb)) return {};
        if (m.inPeakDb > hotAboveDb) return "tooHot";
        if (m.inPeakDb < num (goal["peakDb"]["min"]) - quietBelowMinBy) return "tooQuiet";
        return "aboutRight";
    }
    if (chapterId == "peaks")   return band (m.outPeakDb, goal["peakDb"]);
    if (chapterId == "average") return band (m.outAverageDb, goal["averageDb"]);
    if (chapterId == "headroom")
    {
        if (std::isnan (m.maxOutPeakDb)) return {};
        if (m.maxOutPeakDb >= clipDb) return "clipping";
        if (m.maxOutPeakDb > -num (goal["headroomMinDb"])) return "tight";
        return m.holdActiveSeconds >= headroomListenSeconds ? "onTarget" : std::string();
    }
    return {};
}

std::string GainStagingProcessor::doneZonesFor (const juce::String& chapterId) const
{
    if (chapterId == "where")
        return "tooQuiet aboutRight tooHot";   // informational: any verdict completes it
    if (chapterId == "average")
    {
        // "Very dynamic / very dense" with the peaks still in their zone is a correct
        // finish: the advice is to leave the trim and compress later (Vol. IV).
        const bool peaksOk = band (metrics.get().outPeakDb, session.getGoal()["peakDb"]) == "onTarget";
        return peaksOk ? "onTarget low high" : "onTarget";
    }
    return "onTarget";
}

na::book::Tone GainStagingProcessor::toneFor (const juce::String& chapterId, const std::string& zone) const
{
    using Tone = na::book::Tone;
    if (zone == "onTarget" || zone == "aboutRight") return Tone::good;
    if (zone == "tooHot" || zone == "clipping" || zone == "tight") return Tone::tooMuch;
    if (zone == "tooQuiet") return Tone::tooLittle;
    if (chapterId == "peaks") return zone == "high" ? Tone::tooMuch : Tone::tooLittle;
    if (chapterId == "average") return Tone::tooLittle;   // very dynamic / very dense: advice, not an error
    return Tone::neutral;
}

std::vector<na::book::Meter> GainStagingProcessor::metersFor (const juce::String& chapterId) const
{
    const auto& m = metrics.get();
    const auto goal = session.getGoal();
    const auto t = [&goal] (const char* key, const char* field) { return num (goal[key][field]); };
    constexpr float lo = -48.0f, hi = 0.0f;

    if (chapterId == "where")
        return { { "peak", m.inPeakDb, lo, hi, none, none, "dbfs", 1, false },
                 { "average", m.inAverageDb, lo, hi, none, none, "dbfs", 1, false } };
    if (chapterId == "peaks")
        return { { "peak", m.outPeakDb, lo, hi, t ("peakDb", "min"), t ("peakDb", "max"), "dbfs", 1, true } };
    if (chapterId == "average")
        return { { "average", m.outAverageDb, lo, hi, t ("averageDb", "min"), t ("averageDb", "max"), "dbfs", 1, true },
                 { "crest", m.crestDb, 0.0f, 24.0f, none, none, "db", 1, false } };
    if (chapterId == "headroom")
        return { { "maxPeak", m.maxOutPeakDb, lo, hi, lo, -num (goal["headroomMinDb"]), "dbfs", 1, true } };
    return {};
}

// Called by the plugin wrappers to create a new instance.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GainStagingProcessor();
}
