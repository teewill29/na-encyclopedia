#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "analysis/Metrics.h"
#include "content/Content.h"

#include <functional>
#include <utility>
#include <vector>

namespace na
{

/** Debug overlay that prints every metric live (FEEDBACK-ENGINE.md §7).
    Labels come from the content file's "debug" section. Repaints at 15 Hz. */
class MetricsOverlay final : public juce::Component,
                             private juce::Timer
{
public:
    using MetricsSource = std::function<const analysis::Metrics& ()>;
    using CounterSource = std::function<unsigned()>;
    /** A volume's own rows (label, value), already formatted. */
    using ExtraRows = std::function<std::vector<std::pair<juce::String, juce::String>>()>;

    MetricsOverlay (const Content& content, MetricsSource metrics, CounterSource droppedBlocks, ExtraRows extraRows = {});
    ~MetricsOverlay() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }

    juce::String label (const char* key) const;
    juce::String number (float v, int decimals, const char* unitKey) const;

    const Content& content;
    MetricsSource getMetrics;
    CounterSource getDropped;
    ExtraRows getExtraRows;
};

} // namespace na
