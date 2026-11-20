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

} // namespace na
