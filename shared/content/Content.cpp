#include "Content.h"

namespace na
{

Content Content::fromJson (const char* utf8Data, size_t numBytes)
{
    Content c;
    c.root = juce::JSON::parse (juce::String::fromUTF8 (utf8Data, (int) numBytes));
    jassert (c.isValid()); // the embedded content file failed to parse
    return c;
}

juce::var Content::get (juce::StringRef dottedPath) const
{
    juce::var node = root;

    for (const auto& key : juce::StringArray::fromTokens (juce::String (dottedPath), ".", {}))
    {
        if (! node.isObject())
            return {};

        node = node[juce::Identifier (key)];
    }

    return node;
}

juce::String Content::getString (juce::StringRef dottedPath, const juce::String& fallback) const
{
    const auto v = get (dottedPath);
    return v.isString() ? v.toString() : fallback;
}

namespace
{
    juce::var findById (const juce::Array<juce::var>* list, const juce::String& id)
    {
        if (list != nullptr)
            for (const auto& item : *list)
                if (item["id"].toString() == id)
                    return item;
        return {};
    }
}

const juce::Array<juce::var>* Content::instruments() const { return root["instruments"].getArray(); }
const juce::Array<juce::var>* Content::chapters() const    { return root["chapters"].getArray(); }

juce::var Content::findInstrument (const juce::String& instrumentId) const
{
    return findById (instruments(), instrumentId);
}

juce::var Content::findGoal (const juce::String& instrumentId, const juce::String& goalId) const
{
    return findById (findInstrument (instrumentId)["goals"].getArray(), goalId);
}

juce::var Content::findChapter (const juce::String& chapterId) const
{
    return findById (chapters(), chapterId);
}

juce::StringArray Content::chapterIdsFor (const juce::var& goal) const
{
    juce::StringArray extras;
    if (auto* list = goal["extraChapters"].getArray())
        for (const auto& e : *list)
            extras.add (e.toString());

    juce::StringArray ids;
    if (auto* list = chapters())
        for (const auto& chapter : *list)
            if (! static_cast<bool> (chapter["optional"]) || extras.contains (chapter["id"].toString()))
                ids.add (chapter["id"].toString());
    return ids;
}

} // namespace na
