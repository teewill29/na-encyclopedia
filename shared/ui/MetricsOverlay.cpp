#include "MetricsOverlay.h"

#include <cmath>

namespace na
{

namespace
{
    const juce::Colour panel { 0xFFEFE5CF }; // paperPanel (UI-REFERENCE §3)
    const juce::Colour rule  { 0xFFC2B08A };
    const juce::Colour ink   { 0xFF2A2118 };
    const juce::Colour muted { 0xFF5E4E3B };
}

MetricsOverlay::MetricsOverlay (const Content& c, MetricsSource metrics, CounterSource dropped, ExtraRows extraRows)
    : content (c), getMetrics (std::move (metrics)), getDropped (std::move (dropped)), getExtraRows (std::move (extraRows))
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (15);
}

MetricsOverlay::~MetricsOverlay()
{
    stopTimer();
}

juce::String MetricsOverlay::label (const char* key) const
{
    return content.getString (juce::String ("debug.labels.") + key, key);
}

juce::String MetricsOverlay::number (float v, int decimals, const char* unitKey) const
{
    if (std::isnan (v))
        return content.getString ("debug.none");
    // Note: juce::String (v, 0) means "default precision", not "no decimals".
    return (decimals > 0 ? juce::String (v, decimals) : juce::String (juce::roundToInt (v))) + content.getString (juce::String ("debug.units.") + unitKey);
}

void MetricsOverlay::paint (juce::Graphics& g)
{
    using State = analysis::Metrics::State;
    const auto& m = getMetrics();

    const char* stateKey = m.state == State::silence ? "silence" : m.state == State::listening ? "listening" : "evaluating";

    std::vector<std::pair<juce::String, juce::String>> rows {
        { label ("state"),           content.getString (juce::String ("debug.states.") + stateKey) },
        { label ("activeSeconds"),   number (m.activeSeconds, 1, "seconds") },
        { label ("dynamicRange"),    number (m.dynamicRangeDb, 1, "db") },
        { label ("momentaryIn"),     number (m.momentaryInLufs, 1, "lufs") },
        { label ("grNow"),           number (m.currentGrDb, 1, "db") },
        { label ("grP90"),           number (m.grP90Db, 1, "db") },
        { label ("onsets"),          juce::String (m.onsetCount) },
        { label ("frontsKept"),      number (m.frontsKeptPct, 0, "percent") },
        { label ("frontsKeptCount"), juce::String (m.frontsKeptCount) },
        { label ("frontGr"),         number (m.lastFrontGrDb, 1, "db") },
        { label ("frontTargetGr"),   number (m.lastFrontTargetGrDb, 1, "db") },
        { label ("residualGr"),      number (m.residualGrDb, 1, "db") },
        { label ("ripple"),          number (m.grRippleDb, 1, "db") },
        { label ("loudnessIn"),      number (m.loudnessInLufs, 1, "lufs") },
        { label ("loudnessOut"),     number (m.loudnessOutLufs, 1, "lufs") },
        { label ("levelDiff"),       number (m.levelDiffDb, 1, "db") },
        { label ("peakIn"),          number (m.inPeakDb, 1, "db") },
        { label ("peakOut"),         number (m.outPeakDb, 1, "db") },
        { label ("averageIn"),       number (m.inAverageDb, 1, "db") },
        { label ("averageOut"),      number (m.outAverageDb, 1, "db") },
        { label ("maxPeak"),         number (m.maxOutPeakDb, 1, "db") },
        { label ("dropped"),         juce::String (getDropped ? getDropped() : 0u) },
    };
    if (getExtraRows)
        for (auto& row : getExtraRows())
            rows.push_back (std::move (row));

    auto area = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (rule);
    g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.0f);

    auto inner = getLocalBounds().reduced (16, 12);
    g.setColour (muted);
    g.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
    g.drawText (content.getString ("debug.title"), inner.removeFromTop (22), juce::Justification::centredLeft);
    inner.removeFromTop (6);

    const int rowH = juce::jmin (24, inner.getHeight() / (int) rows.size());
    g.setFont (juce::FontOptions (13.0f));
    for (const auto& [name, value] : rows)
    {
        auto row = inner.removeFromTop (rowH);
        g.setColour (muted);
        g.drawText (name, row, juce::Justification::centredLeft);
        g.setColour (ink);
        g.drawText (value, row, juce::Justification::centredRight);
    }
}

} // namespace na
