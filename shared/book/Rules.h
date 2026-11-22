#pragma once

// Small types shared by the book and each volume's rules. No JUCE plugin
// dependencies, so tools and tests can use them on their own.

#include <vector>

namespace na::book
{

/** What The Reading's spectrum view draws (docs/EQ.md §6). Frequencies in Hz,
    levels in dB. Any of the curves may be empty (e.g. before there's audio). */
struct SpectrumData
{
    std::vector<float> freqs;     // shared x positions for inDb / outDb / curveDb
    std::vector<float> inDb;      // the track before the volume's processing (faint)
    std::vector<float> outDb;     // after (filled)
    std::vector<float> curveDb;   // the volume's own response (e.g. the EQ curve), 0 dB = flat
    float shadeLowHz = 0.0f, shadeHighHz = 0.0f;   // the chapter's problem range (0 = none)
};

/** How a zone is coloured (UI-REFERENCE §3). The status label always says it in words too. */
enum class Tone { neutral, good, tooMuch, tooLittle };   // tooMuch = warnLow, tooLittle = warnHigh

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

} // namespace na::book
