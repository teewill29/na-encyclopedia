#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "content/Content.h"

namespace na
{

/** Where the user is in the book: instrument, goal, current chapter, and
    which chapters are done or have been visited. Saved with the host session.

    Message thread only. Listeners hear about every change (sendChangeMessage
    is asynchronous, so listeners may be called a moment later).
*/
class Session final : public juce::ChangeBroadcaster
{
public:
    explicit Session (const Content& content);

    bool hasSetup() const noexcept { return instrumentId.isNotEmpty() && goalId.isNotEmpty(); }

    /** Picks instrument and goal (the Title Page). Starts the book over:
        progress is cleared and the current chapter becomes the first one. */
    void setup (const juce::String& instrumentId, const juce::String& goalId);

    /** Back to the Title Page without losing the current setup. */
    void showTitlePage();
    bool isOnTitlePage() const noexcept { return onTitlePage || ! hasSetup() || currentChapterId.isEmpty(); }

    const juce::String& getInstrumentId() const noexcept { return instrumentId; }
    const juce::String& getGoalId() const noexcept { return goalId; }
    juce::var getInstrument() const { return content.findInstrument (instrumentId); }
    juce::var getGoal() const { return content.findGoal (instrumentId, goalId); }

    /** Chapter ids for the current goal, in order. */
    const juce::StringArray& getChapterIds() const noexcept { return chapterIds; }
    const juce::String& getCurrentChapterId() const noexcept { return currentChapterId; }
    int getCurrentIndex() const noexcept { return chapterIds.indexOf (currentChapterId); }

    /** Opens a chapter. Returns true the first time this chapter is opened
        for this setup (when its knob should jump to the goal's start value). */
    bool goTo (const juce::String& chapterId);
    /** Move one chapter on / back. Return false if there's nowhere to go.
        (Use goTo() when you need to know about first visits.) */
    bool next();
    bool previous();

    bool isDone (const juce::String& chapterId) const { return done.contains (chapterId); }
    void setDone (const juce::String& chapterId, bool isChapterDone);
    bool wasVisited (const juce::String& chapterId) const { return visited.contains (chapterId); }

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& tree);

    static const juce::Identifier treeType;

private:
    const Content& content;
    juce::String instrumentId, goalId, currentChapterId;
    juce::StringArray chapterIds, done, visited;
    bool onTitlePage = true;
};

} // namespace na
