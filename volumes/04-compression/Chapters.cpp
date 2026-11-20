#include "Chapters.h"
#include "Parameters.h"

namespace na::vol4
{

namespace
{
    // Chapter II's knob is Threshold, but on entry it's the *ratio* that is set
    // from the goal ("ratio auto-set to ratio.start"). Its target is the
    // amount of gain reduction (grDb), not a threshold setting.
    const ChapterRule rules[] = {
        { "goal",           nullptr,             nullptr,     nullptr },
        { "ratioThreshold", ParamID::threshold,  "ratio",     "grDb" },
        { "attack",         ParamID::attack,     "attackMs",  "attackMs" },
        { "release",        ParamID::release,    "releaseMs", "releaseMs" },
        { "level",          ParamID::makeup,     nullptr,     nullptr },
        { "blend",          ParamID::mix,        "mixPct",    "mixPct" },
    };

    // Which parameter a goal's start key sets.
    const char* paramForStartKey (const juce::String& key)
    {
        if (key == "ratio")     return ParamID::ratio;
        if (key == "attackMs")  return ParamID::attack;
        if (key == "releaseMs") return ParamID::release;
        if (key == "mixPct")    return ParamID::mix;
        return nullptr;
    }
}

const ChapterRule* ruleFor (const juce::String& chapterId)
{
    for (const auto& r : rules)
        if (chapterId == r.chapterId)
            return &r;
    return nullptr;
}

bool isParallel (const juce::var& goal)
{
    if (auto* extras = goal["extraChapters"].getArray())
        return extras->contains ("blend");
    return false;
}

bool isMixForcedToFull (const Session& session)
{
    if (session.isOnTitlePage() || ! isParallel (session.getGoal()))
        return false;
    const auto& id = session.getCurrentChapterId();
    return id == "ratioThreshold" || id == "attack" || id == "release";
}

void applyStartValues (const juce::String& chapterId, const juce::var& goal,
                       juce::AudioProcessorValueTreeState& parameters)
{
    const auto* rule = ruleFor (chapterId);
    if (rule == nullptr || rule->startKey == nullptr)
        return;

    const auto start = goal[rule->startKey]["start"];
    auto* param = parameters.getParameter (paramForStartKey (rule->startKey));
    if (param == nullptr || ! (start.isInt() || start.isDouble() || start.isInt64()))
        return;

    param->beginChangeGesture();
    param->setValueNotifyingHost (param->convertTo0to1 ((float) (double) start));
    param->endChangeGesture();
}

} // namespace na::vol4
