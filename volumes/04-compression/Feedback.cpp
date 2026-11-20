#include "Feedback.h"

#include <cmath>

namespace na::vol4::Feedback
{

namespace
{
    constexpr float none = analysis::Metrics::none;

    float num (const juce::var& v) { return v.isVoid() || v.isUndefined() ? none : (float) (double) v; }

    /** low / onTarget / high for a value against a goal's {min, max}. */
    std::string band (float value, const juce::var& range)
    {
        if (std::isnan (value) || ! range.isObject()) return {};
        if (value < num (range["min"])) return "low";
        if (value > num (range["max"])) return "high";
        return "onTarget";
    }
}

std::string zoneFor (const juce::String& chapterId, const juce::var& instrument, const juce::var& goal,
                     const analysis::Metrics& m, const CompressorSettings& s)
{
    if (chapterId == "goal")                          // §2.2 need check
    {
        const auto minRange = instrument["needCheckMinRangeDb"];
        if (minRange.isVoid() || minRange.isUndefined()) return "skipped";   // drums, buses
        if (std::isnan (m.dynamicRangeDb)) return {};
        return m.dynamicRangeDb < num (minRange) ? "notNeeded" : "needed";
    }
    if (chapterId == "ratioThreshold") return band (m.grP90Db, goal["grDb"]);          // §2.3
    // §2.5 / §4: Chapter III is judged by the Attack knob against attackMs.
    // The fronts-kept meter shows the live measurement alongside (owner
    // decision in M5: the two content targets disagree on real audio until
    // they're reviewed). Still needs audio, like every chapter.
    if (chapterId == "attack")         return band (s.attackMs, goal["attackMs"]);
    if (chapterId == "release")                       // §2.6
    {
        const auto range = goal["releaseMs"];
        const bool residualKnown = ! std::isnan (m.residualGrDb);
        if (residualKnown && m.residualGrDb > num (goal["residualGrMaxDb"])) return "tooSlow";
        if (s.releaseMs < num (range["min"]) || (! std::isnan (m.grRippleDb) && m.grRippleDb > rippleMaxDb)) return "tooFast";
        if (s.releaseMs > num (range["max"])) return "tooSlow";
        return residualKnown ? "onTarget" : std::string();
    }
    if (chapterId == "level")                         // §2.7
    {
        if (std::isnan (m.levelDiffDb)) return {};
        if (m.levelDiffDb >  levelToleranceDb) return "high";
        if (m.levelDiffDb < -levelToleranceDb) return "low";
        return "onTarget";
    }
    if (chapterId == "blend") return band (s.mixPct, goal["mixPct"]);                     // §2.8
    return {};
}

std::string doneZonesFor (const juce::String& chapterId)
{
    // Chapter I is informational: any answer completes it ("continue either way").
    return chapterId == "goal" ? "needed notNeeded skipped" : "onTarget";
}

juce::String messageGroupFor (const juce::String& chapterId)
{
    return chapterId == "goal" ? juce::String ("needCheck") : chapterId;
}

Tone toneFor (const juce::String& chapterId, const std::string& zone)
{
    if (zone.empty()) return Tone::neutral;
    if (zone == "onTarget" || zone == "needed" || zone == "skipped") return Tone::good;
    if (zone == "notNeeded") return Tone::tooLittle;
    if (zone == "tooFast") return Tone::tooMuch;
    if (zone == "tooSlow") return Tone::tooLittle;
    // low/high mean different things per chapter
    const bool lowIsTooMuch = chapterId == "attack";   // low fronts kept = attack too fast
    if (zone == "low")  return lowIsTooMuch ? Tone::tooMuch : Tone::tooLittle;
    if (zone == "high") return lowIsTooMuch ? Tone::tooLittle : Tone::tooMuch;
    return Tone::neutral;
}

std::vector<Meter> metersFor (const juce::String& chapterId, const juce::var& goal,
                              const analysis::Metrics& m, const CompressorSettings& s)
{
    const auto t = [&goal] (const char* key, const char* field) { return num (goal[key][field]); };
    const Meter grNow { "grNow", m.currentGrDb, 0.0f, 24.0f, none, none, "db", 1, false };

    if (chapterId == "ratioThreshold")
        return { { "grLoud", m.grP90Db, 0.0f, 24.0f, t ("grDb", "min"), t ("grDb", "max"), "db", 1, true } };
    if (chapterId == "attack")   // informational: the status comes from the knob (see zoneFor)
        return { grNow, { "frontsKept", m.frontsKeptPct, 0.0f, 100.0f, t ("frontsKeptPct", "min"), t ("frontsKeptPct", "max"), "percent", 0, false } };
    if (chapterId == "release")
        return { grNow, { "recovery", m.residualGrDb, 0.0f, 6.0f, 0.0f, num (goal["residualGrMaxDb"]), "db", 1, true } };
    if (chapterId == "level")
        return { { "levelDiff", m.levelDiffDb, -6.0f, 6.0f, -levelToleranceDb, levelToleranceDb, "db", 1, true } };
    if (chapterId == "blend")
        return { { "mix", s.mixPct, 0.0f, 100.0f, t ("mixPct", "min"), t ("mixPct", "max"), "percent", 0, true } };
    return {};
}

} // namespace na::vol4::Feedback
