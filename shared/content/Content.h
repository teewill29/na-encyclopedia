#pragma once

#include <juce_core/juce_core.h>

namespace na
{

/** Read-only view of a volume's content JSON (all copy, targets and goals).

    Load once on the message thread; never touch it from the audio thread.
    Milestone 2 only needs string lookup. Templating and goal lookup arrive
    in Milestone 4.
*/
class Content
{
public:
    Content() = default;

    /** Parses UTF-8 JSON, e.g. straight from BinaryData. */
    static Content fromJson (const char* utf8Data, size_t numBytes);

    bool isValid() const noexcept { return root.isObject(); }

    /** Looks up a dotted path such as "parameters.threshold.name".
        Returns an empty var if any part of the path is missing. */
    juce::var get (juce::StringRef dottedPath) const;

    /** Like get(), as a string. Returns fallback if the path is missing. */
    juce::String getString (juce::StringRef dottedPath, const juce::String& fallback = {}) const;

    // ── Instruments, goals and chapters (in file order) ────────────────────
    const juce::Array<juce::var>* instruments() const;
    const juce::Array<juce::var>* chapters() const;

    /** The instrument / goal / chapter object with this id, or a void var. */
    juce::var findInstrument (const juce::String& instrumentId) const;
    juce::var findGoal (const juce::String& instrumentId, const juce::String& goalId) const;
    juce::var findChapter (const juce::String& chapterId) const;

    /** Chapter ids for a goal, in book order: every chapter not marked
        "optional", plus optional ones the goal lists in "extraChapters". */
    juce::StringArray chapterIdsFor (const juce::var& goal) const;

private:
    juce::var root;
};

} // namespace na
