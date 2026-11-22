#pragma once

#include "Style.h"
#include "book/Rules.h"

namespace na::ui
{

/** Draws The Reading's spectrum (docs/EQ.md §6): a calm, averaged view.
    x: 20 Hz – 20 kHz, log. Levels: −90 … −6 dB. The volume's curve (e.g. the EQ)
    is drawn on its own ±18 dB scale around the middle line. */
void drawSpectrum (juce::Graphics&, juce::Rectangle<float> area, const book::SpectrumData&,
                   juce::Colour curveColour, const juce::String& khzSuffix);

} // namespace na::ui
