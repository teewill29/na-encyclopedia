#pragma once

#include <juce_core/juce_core.h>
#include "analysis/Metrics.h"
#include "dsp/Compressor.h"

#include <string>
#include <vector>

namespace na::vol4
{

/** Compression-specific feedback rules (FEEDBACK-ENGINE.md §2, §4). */
namespace Feedback
{
    static constexpr float rippleMaxDb       = 3.0f;  // §2.6 "too fast" ripple
    static constexpr float levelToleranceDb  = 1.0f;  // §2.7

    /** The zone a chapter's reading falls in right now, "" if there's no
        reading yet (e.g. fewer than one counted onset). */
    std::string zoneFor (const juce::String& chapterId, const juce::var& instrument, const juce::var& goal,
                         const analysis::Metrics& metrics, const CompressorSettings& settings);

    /** Zones that earn the chapter its check mark (space-separated). */
    std::string doneZonesFor (const juce::String& chapterId);

    /** Group in content "messages" for a chapter (Chapter I uses "needCheck"). */
    juce::String messageGroupFor (const juce::String& chapterId);

    /** How a zone is coloured (UI-REFERENCE §3): never the only signal,
        the status label always says it in words too. */
    enum class Tone { neutral, good, tooMuch, tooLittle };   // tooMuch = warnLow, tooLittle = warnHigh
    Tone toneFor (const juce::String& chapterId, const std::string& zone);

    /** A meter in The Reading (UI-REFERENCE §6.2, §7). */
    struct Meter
    {
        const char* labelKey;          // content "book.reading.meters.<key>"
        float value, rangeMin, rangeMax;
        float targetMin, targetMax;    // NaN = no target zone
        const char* unitKey;           // content "debug.units.<key>"
        int decimals;
        bool showsStatus;              // fill coloured by the chapter's status (else neutral)
    };
    std::vector<Meter> metersFor (const juce::String& chapterId, const juce::var& goal,
                                  const analysis::Metrics& metrics, const CompressorSettings& settings);
}

} // namespace na::vol4
