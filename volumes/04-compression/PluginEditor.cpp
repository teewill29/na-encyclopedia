#include "PluginEditor.h"

namespace
{
    constexpr int baseWidth  = 1120; // docs/UI-REFERENCE.md §2
    constexpr int baseHeight = 760;
    constexpr int headerHeight = 90;
    constexpr int footerHeight = 120;

    const juce::Colour paper      { 0xFFF4ECDB }; // docs/UI-REFERENCE.md §3
    const juce::Colour ink        { 0xFF2A2118 };
    const juce::Colour inkMuted   { 0xFF5E4E3B };
    const juce::Colour meterTrack { 0xFFDCCFB3 };
    const juce::Colour good       { 0xFF2F5E3A };

    constexpr float meterMaxDb = 24.0f;
}

CompressionAudioProcessorEditor::CompressionAudioProcessorEditor (CompressionAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      generic (p),
      title (p.getContent().getString ("ui.skeletonTitle")),
      grLabel (p.getContent().getString ("ui.debugGainReduction")),
      dbUnit (p.getContent().getString ("ui.unitDb"))
{
    addAndMakeVisible (generic);
    setSize (baseWidth, baseHeight);
    startTimerHz (30);
}

CompressionAudioProcessorEditor::~CompressionAudioProcessorEditor()
{
    stopTimer();
}

void CompressionAudioProcessorEditor::timerCallback()
{
    // Fast rise, gentle fall so the number is readable.
    const auto peak = processor.takeGainReductionPeakDb();
    grDisplayDb = peak > grDisplayDb ? peak : juce::jmax (peak, grDisplayDb - 0.5f);
    repaint (getLocalBounds().removeFromBottom (footerHeight));
}

void CompressionAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (paper);

    auto area = getLocalBounds().reduced (40, 0);

    g.setColour (ink);
    g.setFont (juce::FontOptions (32.0f));
    g.drawFittedText (title, area.removeFromTop (headerHeight), juce::Justification::centred, 1);

    // Gain reduction meter
    auto footer = area.removeFromBottom (footerHeight).reduced (0, 30);
    auto labelRow = footer.removeFromTop (24);

    g.setColour (ink);
    g.setFont (juce::FontOptions (17.0f));
    g.drawText (grLabel, labelRow, juce::Justification::centredLeft);
    g.drawText (juce::String (grDisplayDb, 1) + dbUnit, labelRow, juce::Justification::centredRight);

    auto track = footer.removeFromTop (12).toFloat();
    g.setColour (meterTrack);
    g.fillRoundedRectangle (track, 6.0f);
    g.setColour (good);
    g.fillRoundedRectangle (track.reduced (0, 3).withWidth (track.getWidth() * juce::jlimit (0.0f, 1.0f, grDisplayDb / meterMaxDb)), 3.0f);

    // Version and build ID (data, not copy), so it's clear which build is loaded.
    g.setColour (inkMuted);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (juce::String (JucePlugin_VersionString) + " (" + NA_BUILD_ID + ")",
                getLocalBounds().reduced (16, 12), juce::Justification::bottomRight);
}

void CompressionAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (40, 0);
    area.removeFromTop (headerHeight);
    area.removeFromBottom (footerHeight);
    generic.setBounds (area);
}
