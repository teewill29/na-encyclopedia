#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "chapters/Session.h"

namespace na::vol4
{

/** Compression-specific chapter rules (FEEDBACK-ENGINE.md §4). */
struct ChapterRule
{
    const char* chapterId;
    const char* paramId;     // the chapter's main knob, or nullptr (Chapter I)
    const char* startKey;    // goal key whose "start" the knob jumps to on first visit, or nullptr
    const char* targetKey;   // goal key with the target range shown under the knob, or nullptr
};

/** The rule for a chapter, or nullptr for an unknown id. */
const ChapterRule* ruleFor (const juce::String& chapterId);

/** Parallel goals add the Blend chapter. */
bool isParallel (const juce::var& goal);

/** During Chapters II-IV of a parallel goal, Mix is held at 100% so the
    user hears the squashed copy alone (FEEDBACK-ENGINE.md §2.8). */
bool isMixForcedToFull (const Session& session);

/** Applies a chapter's first-visit start values to the parameters
    (e.g. Chapter III sets Attack to attackMs.start). Message thread. */
void applyStartValues (const juce::String& chapterId, const juce::var& goal,
                       juce::AudioProcessorValueTreeState& parameters);

} // namespace na::vol4
