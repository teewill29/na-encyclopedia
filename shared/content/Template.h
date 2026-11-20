#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <optional>

namespace na
{

/** Fills "{path}" placeholders in content strings (FEEDBACK-ENGINE.md §5). */
namespace Template
{
    using Lookup = std::function<std::optional<juce::String> (const juce::String& path)>;

    /** Replaces every {path} that `lookup` knows. Unknown placeholders are
        left as they are, so a gap in the content is visible, not hidden. */
    juce::String fill (const juce::String& text, const Lookup& lookup);

    /** Lookup for goal-based placeholders:
          {ratio.start}            → goal.ratio.start
          {attack.start}           → goal.attackMs.start   (attack → attackMs)
          {release.start}          → goal.releaseMs.start  (release → releaseMs)
          {mix.min}                → goal.mixPct.min       (mix → mixPct)
          {goal.listenFor.attack}  → goal.listenFor.attack
        Any other path is read straight from the goal object. */
    Lookup forGoal (const juce::var& goal);

    /** Lookup from a fixed set of names, e.g. {"numeral", "III"}. */
    Lookup forValues (std::initializer_list<std::pair<juce::String, juce::String>> values);

    /** Tries each lookup in turn. */
    Lookup chain (Lookup first, Lookup second);

    /** Numbers as people write them: 4 → "4", 2.5 → "2.5", 0.25 → "0.25". */
    juce::String formatNumber (double value);
}

} // namespace na
