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

private:
    juce::var root;
};

} // namespace na
