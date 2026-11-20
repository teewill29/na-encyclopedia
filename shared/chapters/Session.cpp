#include "Session.h"

namespace na
{

const juce::Identifier Session::treeType { "Session" };

namespace
{
    const juce::Identifier instrumentProp { "instrument" }, goalProp { "goal" }, chapterProp { "chapter" },
                           doneProp { "done" }, visitedProp { "visited" }, titleProp { "onTitlePage" };
}

Session::Session (const Content& c) : content (c) {}

void Session::setup (const juce::String& newInstrument, const juce::String& newGoal)
{
    const auto goal = content.findGoal (newInstrument, newGoal);
    if (! goal.isObject())
        return;

    instrumentId = newInstrument;
    goalId = newGoal;
    chapterIds = content.chapterIdsFor (goal);
    done.clear();
    visited.clear();
    currentChapterId = {};
    onTitlePage = false;
    sendChangeMessage();
}

void Session::showTitlePage()
{
    onTitlePage = true;
    sendChangeMessage();
}

bool Session::goTo (const juce::String& chapterId)
{
    if (! chapterIds.contains (chapterId))
        return false;

    const bool firstVisit = ! visited.contains (chapterId);
    visited.addIfNotAlreadyThere (chapterId);
    currentChapterId = chapterId;
    onTitlePage = false;
    sendChangeMessage();
    return firstVisit;
}

bool Session::next()
{
    const auto i = getCurrentIndex();
    if (i < 0 || i + 1 >= chapterIds.size())
        return false;
    goTo (chapterIds[i + 1]);
    return true;
}

bool Session::previous()
{
    const auto i = getCurrentIndex();
    if (i <= 0)
        return false;
    goTo (chapterIds[i - 1]);
    return true;
}

void Session::setDone (const juce::String& chapterId, bool isChapterDone)
{
    if (isChapterDone == done.contains (chapterId))
        return;
    if (isChapterDone) done.add (chapterId);
    else               done.removeString (chapterId);
    sendChangeMessage();
}

juce::ValueTree Session::toValueTree() const
{
    juce::ValueTree tree (treeType);
    tree.setProperty (instrumentProp, instrumentId, nullptr);
    tree.setProperty (goalProp, goalId, nullptr);
    tree.setProperty (chapterProp, currentChapterId, nullptr);
    tree.setProperty (doneProp, done.joinIntoString (","), nullptr);
    tree.setProperty (visitedProp, visited.joinIntoString (","), nullptr);
    tree.setProperty (titleProp, onTitlePage, nullptr);
    return tree;
}

void Session::fromValueTree (const juce::ValueTree& tree)
{
    if (! tree.hasType (treeType))
        return;

    const auto newInstrument = tree[instrumentProp].toString();
    const auto newGoal = tree[goalProp].toString();
    const auto goal = content.findGoal (newInstrument, newGoal);

    if (goal.isObject())
    {
        instrumentId = newInstrument;
        goalId = newGoal;
        chapterIds = content.chapterIdsFor (goal);
        auto keepKnown = [this] (const juce::String& list)
        {
            juce::StringArray result;
            for (const auto& id : juce::StringArray::fromTokens (list, ",", {}))
                if (chapterIds.contains (id)) result.add (id);
            return result;
        };
        done = keepKnown (tree[doneProp].toString());
        visited = keepKnown (tree[visitedProp].toString());
        currentChapterId = chapterIds.contains (tree[chapterProp].toString()) ? tree[chapterProp].toString() : juce::String();
        onTitlePage = static_cast<bool> (tree[titleProp]) || currentChapterId.isEmpty();
    }
    else
    {
        // Unknown setup (e.g. content changed between versions): start fresh.
        instrumentId = goalId = currentChapterId = {};
        chapterIds.clear(); done.clear(); visited.clear();
        onTitlePage = true;
    }
    sendChangeMessage();
}

} // namespace na
