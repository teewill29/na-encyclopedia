#include "PluginProcessor.h"
#include "content/Template.h"
#include "BinaryData.h"

#include <cmath>

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

// ── Rules (skeleton: knob and target captions; zones and meters come next) ───
const char* GainStagingProcessor::knobParamFor (const juce::String& chapterId) const
{
    return usesTrim (chapterId) ? trimId : nullptr;
}

juce::String GainStagingProcessor::targetCaptionFor (const juce::String& chapterId) const
{
    const auto goal = session.getGoal();
    const char* key = chapterId == "peaks" ? "peakDb" : chapterId == "average" ? "averageDb" : nullptr;
    if (key == nullptr || ! goal[key].isObject())
        return {};
    return na::Template::fill (content.getString ("book.chapterPage.target"),
                               na::Template::forValues ({ { "min", na::Template::formatNumber ((double) goal[key]["min"]) },
                                                          { "max", na::Template::formatNumber ((double) goal[key]["max"]) },
                                                          { "unit", content.getString (juce::String ("book.units.") + key) } }));
}

std::string GainStagingProcessor::zoneFor (const juce::String&) const          { return {}; }
std::string GainStagingProcessor::doneZonesFor (const juce::String&) const     { return "onTarget"; }
na::book::Tone GainStagingProcessor::toneFor (const juce::String&, const std::string&) const { return na::book::Tone::neutral; }
std::vector<na::book::Meter> GainStagingProcessor::metersFor (const juce::String&) const   { return {}; }

// Called by the plugin wrappers to create a new instance.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GainStagingProcessor();
}
