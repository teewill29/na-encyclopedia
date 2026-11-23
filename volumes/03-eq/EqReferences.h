#pragma once

#include <juce_core/juce_core.h>
#include "analysis/SpectrumAnalyser.h"

#include <algorithm>
#include <cmath>

namespace na::vol3
{

/** What "normal" looks like for an instrument's mud and harsh measurements
    (docs/EQ.md §4): its typical spectrum from the content file, and the span the
    comparison line is fitted over (half an octave… two octaves around the range,
    never below the instrument's low cut or above 16 kHz). */
struct EqReferences
{
    analysis::SpectrumAnalyser::Reference mud, harsh;

    static EqReferences forInstrument (const juce::var& instrument)
    {
        auto num = [] (const juce::var& v) { return (float) (double) v; };

        std::vector<std::pair<float, float>> points;
        if (const auto* arr = instrument["typicalSpectrum"].getArray())
            for (const auto& p : *arr)
                if (p.isArray() && p.size() >= 2)
                    points.emplace_back (num (p[0]), num (p[1]));
        const auto typical = points.empty() ? std::vector<float>() : analysis::SpectrumAnalyser::typicalFromPoints (points);

        const float lowCut = instrument["lowCutHz"].isObject() ? num (instrument["lowCutHz"]["max"]) : 20.0f;
        auto reference = [&] (const juce::var& range)
        {
            analysis::SpectrumAnalyser::Reference r;
            r.typicalDb = typical;
            if (range.isObject())
            {
                r.fitLoHz = std::max (num (range["min"]) * 0.5f, lowCut);
                r.fitHiHz = std::min (num (range["max"]) * 4.0f, 16000.0f);
            }
            return r;
        };
        return { reference (instrument["mudHz"]), reference (instrument["harshHz"]) };
    }
};

} // namespace na::vol3
