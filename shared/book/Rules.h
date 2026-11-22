#pragma once

// Small types shared by the book and each volume's rules. No JUCE plugin
// dependencies, so tools and tests can use them on their own.

namespace na::book
{

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
