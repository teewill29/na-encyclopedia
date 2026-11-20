#include "PluginEditor.h"

namespace
{
    constexpr int baseWidth  = 1120; // docs/UI-REFERENCE.md §2
    constexpr int baseHeight = 760;
    constexpr int overlayWidth = 420;

    const juce::Colour paper    { 0xFFF4ECDB }; // docs/UI-REFERENCE.md §3
    const juce::Colour inkMuted { 0xFF5E4E3B };
}

CompressionAudioProcessorEditor::CompressionAudioProcessorEditor (CompressionAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      titleView (p),
      chapterView (p)
     #if NA_DEBUG_OVERLAY
      , overlay (p.getContent(),
                 [&p]() -> const na::analysis::Metrics& { return p.getMetrics(); },
                 [&p] { return p.getDroppedAnalysisBlocks(); })
     #endif
{
    addChildComponent (titleView);
    addChildComponent (chapterView);

   #if NA_DEBUG_OVERLAY
    addChildComponent (overlay);
    debugButton.setButtonText (p.getContent().getString ("book.chapterPage.debugToggle"));
    debugButton.setClickingTogglesState (true);
    debugButton.onClick = [this] { overlay.setVisible (debugButton.getToggleState()); overlay.toFront (false); };
    addAndMakeVisible (debugButton);
   #endif

    processor.getSession().addChangeListener (this);
    setSize (baseWidth, baseHeight);
    showCurrentPage();
}

CompressionAudioProcessorEditor::~CompressionAudioProcessorEditor()
{
    processor.getSession().removeChangeListener (this);
}

void CompressionAudioProcessorEditor::showCurrentPage()
{
    const bool title = processor.getSession().isOnTitlePage();
    if (title) titleView.refresh();
    else       chapterView.refresh();
    titleView.setVisible (title);
    chapterView.setVisible (! title);
}

void CompressionAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (paper);
}

void CompressionAudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    // Version and build ID (data, not copy), so it's clear which build is loaded.
    g.setColour (inkMuted.withAlpha (0.7f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (juce::String (JucePlugin_VersionString) + " (" + NA_BUILD_ID + ")",
                getLocalBounds().reduced (8, 4), juce::Justification::bottomLeft);
}

void CompressionAudioProcessorEditor::resized()
{
    titleView.setBounds (getLocalBounds());
    chapterView.setBounds (getLocalBounds());
   #if NA_DEBUG_OVERLAY
    debugButton.setBounds (6, getHeight() - 42, 64, 22);
    overlay.setBounds (getLocalBounds().removeFromRight (overlayWidth).reduced (8, 30));
   #endif
}
