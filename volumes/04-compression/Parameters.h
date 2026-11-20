#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "content/Content.h"

namespace na::vol4
{

/** Parameter IDs. Stable forever: hosts store automation and sessions by ID.
    Names and units shown to the user come from the content file
    ("parameters.<id>.name" / ".unit"). */
namespace ParamID
{
    inline constexpr const char* threshold = "threshold";
    inline constexpr const char* ratio     = "ratio";
    inline constexpr const char* attack    = "attack";
    inline constexpr const char* release   = "release";
    inline constexpr const char* makeup    = "makeup";
    inline constexpr const char* mix       = "mix";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (const na::Content& content);

} // namespace na::vol4
