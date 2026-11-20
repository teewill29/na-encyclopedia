#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    constexpr int baseWidth  = 1120; // docs/UI-REFERENCE.md §2
    constexpr int baseHeight = 760;

    const juce::Colour paper { 0xFFF4ECDB }; // docs/UI-REFERENCE.md §3
    const juce::Colour ink   { 0xFF2A2118 };

    juce::String loadSkeletonTitle()
    {
        const auto json = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::compression_json,
                                                                     BinaryData::compression_jsonSize));
        return json["ui"]["skeletonTitle"].toString();
    }
}

CompressionAudioProcessorEditor::CompressionAudioProcessorEditor (CompressionAudioProcessor& p)
    : AudioProcessorEditor (&p),
      title (loadSkeletonTitle())
{
    setSize (baseWidth, baseHeight);
}

void CompressionAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (paper);
    g.setColour (ink);
    g.setFont (juce::FontOptions (32.0f));
    g.drawFittedText (title, getLocalBounds().reduced (40), juce::Justification::centred, 2);
}
