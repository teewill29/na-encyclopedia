#include "Template.h"

namespace na::Template
{

juce::String formatNumber (double value)
{
    if (std::abs (value - std::round (value)) < 1.0e-9)
        return juce::String ((juce::int64) std::llround (value));

    auto text = juce::String (value, 2);
    while (text.endsWithChar ('0')) text = text.dropLastCharacters (1);
    if (text.endsWithChar ('.'))    text = text.dropLastCharacters (1);
    return text;
}

juce::String fill (const juce::String& text, const Lookup& lookup)
{
    juce::String result;
    int pos = 0;
    while (pos < text.length())
    {
        const auto open = text.indexOfChar (pos, '{');
        if (open < 0) break;
        const auto close = text.indexOfChar (open + 1, '}');
        if (close < 0) break;

        result += text.substring (pos, open);
        const auto path = text.substring (open + 1, close);
        if (auto value = lookup ? lookup (path) : std::nullopt)
            result += *value;
        else
            result += text.substring (open, close + 1);
        pos = close + 1;
    }
    return result + text.substring (pos);
}

Lookup forGoal (const juce::var& goal)
{
    return [goal] (const juce::String& path) -> std::optional<juce::String>
    {
        static const std::pair<const char*, const char*> aliases[] = {
            { "attack", "attackMs" }, { "release", "releaseMs" }, { "mix", "mixPct" },
            { "gr", "grDb" }, { "frontsKept", "frontsKeptPct" },
        };

        auto parts = juce::StringArray::fromTokens (path, ".", {});
        if (parts.isEmpty()) return std::nullopt;
        if (parts[0] == "goal") parts.remove (0);
        else
            for (const auto& [from, to] : aliases)
                if (parts[0] == from) { parts.set (0, to); break; }

        juce::var node = goal;
        for (const auto& key : parts)
        {
            if (! node.isObject()) return std::nullopt;
            node = node[juce::Identifier (key)];
        }

        if (node.isInt() || node.isInt64() || node.isDouble()) return formatNumber ((double) node);
        if (node.isString()) return node.toString();
        return std::nullopt;
    };
}

Lookup forValues (std::initializer_list<std::pair<juce::String, juce::String>> values)
{
    std::vector<std::pair<juce::String, juce::String>> copy (values);
    return [copy] (const juce::String& path) -> std::optional<juce::String>
    {
        for (const auto& [name, value] : copy)
            if (name == path) return value;
        return std::nullopt;
    };
}

Lookup chain (Lookup first, Lookup second)
{
    return [first, second] (const juce::String& path) -> std::optional<juce::String>
    {
        if (first)  if (auto v = first (path))  return v;
        if (second) if (auto v = second (path)) return v;
        return std::nullopt;
    };
}

} // namespace na::Template
