#include "BookEditor.h"

namespace na::book
{

namespace P = na::ui::Palette;

// ── ClothTextButton ──────────────────────────────────────────────────────────
void ClothTextButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    g.setColour (P::gold.withAlpha (highlighted || getToggleState() ? 0.95f : 0.6f));
    g.setFont (na::ui::Fonts::cinzel (10.0f, 600, 0.16f));
    g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred, false);
}

// ── BookRoot ─────────────────────────────────────────────────────────────────
namespace
{
    na::ui::SpineText spineFrom (const na::Content& c)
    {
        na::ui::SpineText s;
        s.series = c.getString ("series").toUpperCase();
        s.topic = c.getString ("topic").toUpperCase();
        s.volLabel = c.getString ("book.spine.vol");
        s.numeral = c.getString ("volumeNumeral");
        s.edition = juce::StringArray::fromTokens (c.getString ("edition").toUpperCase(), " ", {});
        s.maker = juce::StringArray::fromTokens (c.getString ("brand").toUpperCase(), " ", {});
        return s;
    }
}

BookRoot::BookRoot (BookProcessor& p, std::function<void (float)> onScale)
    : processor (p),
      lookAndFeel (na::ui::colourFromHex (p.getContent().getString ("book.cloth"), juce::Colour (0xFF213B2F))),
      titleView (p),
      chapterView (p),
      onScaleChosen (std::move (onScale))
     #if NA_DEBUG_OVERLAY
      , overlay (p.getContent(),
                 [&p]() -> const na::analysis::Metrics& { return p.getMetrics(); },
                 [&p] { return p.getDroppedAnalysisBlocks(); })
     #endif
{
    setLookAndFeel (&lookAndFeel);
    frame.setCloth (na::ui::colourFromHex (p.getContent().getString ("book.cloth"), juce::Colour (0xFF213B2F)));
    frame.setSpine (spineFrom (p.getContent()));
    addAndMakeVisible (frame);
    addChildComponent (titleView);
    addChildComponent (chapterView);

    sizeButton.onClick = [this]
    {
        juce::PopupMenu menu;
        menu.setLookAndFeel (&lookAndFeel);
        const auto unit = processor.getContent().getString ("debug.units.percent");
        for (int i = 0; i < (int) std::size (BookEditor::scales); ++i)
            menu.addItem (i + 1, juce::String (juce::roundToInt (BookEditor::scales[i] * 100)) + unit);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (sizeButton),
                            [this] (int chosen) { if (chosen > 0 && onScaleChosen) onScaleChosen (BookEditor::scales[chosen - 1]); });
    };
    addAndMakeVisible (sizeButton);

   #if NA_DEBUG_OVERLAY
    addChildComponent (overlay);
    debugButton.setButtonText (p.getContent().getString ("book.chapterPage.debugToggle").toUpperCase());
    debugButton.setClickingTogglesState (true);
    debugButton.onClick = [this] { overlay.setVisible (debugButton.getToggleState()); overlay.toFront (false); };
    addAndMakeVisible (debugButton);
   #endif

    processor.getSession().addChangeListener (this);
    setSize (na::ui::BookFrame::baseWidth, na::ui::BookFrame::baseHeight);
    showCurrentPage();
}

BookRoot::~BookRoot()
{
    processor.getSession().removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void BookRoot::setScaleLabel (float scale)
{
    sizeButton.setButtonText (juce::String (juce::roundToInt (scale * 100)) + processor.getContent().getString ("debug.units.percent"));
}

void BookRoot::showCurrentPage()
{
    const bool title = processor.getSession().isOnTitlePage();
    frame.setLeftPageWidth (title ? 0 : ChapterView::leftPageWidth);
    if (title) titleView.refresh();
    else       chapterView.refresh();
    titleView.setVisible (title);
    chapterView.setVisible (! title);
}

void BookRoot::paintOverChildren (juce::Graphics& g)
{
    // Version and build ID (data, not copy) on the cloth's bottom edge.
    g.setColour (P::gold.withAlpha (0.45f));
    g.setFont (na::ui::Fonts::cinzel (9.0f, 500, 0.1f));
    g.drawText (juce::String (JucePlugin_VersionString) + " (" + NA_BUILD_ID + ")",
                getLocalBounds().removeFromBottom (na::ui::BookFrame::margin).reduced (130, 0),
                juce::Justification::centredLeft, false);
}

void BookRoot::resized()
{
    frame.setBounds (getLocalBounds());
    const auto pages = frame.getPagesArea();
    titleView.setBounds (pages);
    chapterView.setBounds (pages);

    const auto band = getLocalBounds().removeFromBottom (na::ui::BookFrame::margin);
    sizeButton.setBounds (band.withLeft (band.getRight() - 70).withTrimmedRight (14));
   #if NA_DEBUG_OVERLAY
    debugButton.setBounds (band.withWidth (60).withX (band.getX() + 60));
    overlay.setBounds (pages.withTrimmedLeft (pages.getWidth() - 430).reduced (12, 20));
   #endif
}

// ── Editor ───────────────────────────────────────────────────────────────────
juce::PropertiesFile::Options BookEditor::settingsOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "NA Encyclopedia";
    o.folderName = "Noveltee Audio";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    return o;
}

BookEditor::BookEditor (BookProcessor& p)
    : AudioProcessorEditor (&p),
      root (p, [this] (float s)
      {
          setScale (s);
          juce::PropertiesFile settings (settingsOptions());
          settings.setValue ("windowScale", s);   // remembered per user
      })
{
    addAndMakeVisible (root);
    juce::PropertiesFile settings (settingsOptions());
    const auto saved = (float) settings.getDoubleValue ("windowScale", 1.0);
    float chosen = 1.0f;
    for (auto s : scales)
        if (std::abs (s - saved) < 0.01f) chosen = s;
    setScale (chosen);
}

void BookEditor::setScale (float s)
{
    scale = s;
    root.setScaleLabel (s);
    root.setTransform (juce::AffineTransform::scale (s));
    setSize (juce::roundToInt (na::ui::BookFrame::baseWidth * s), juce::roundToInt (na::ui::BookFrame::baseHeight * s));
}

void BookEditor::resized()
{
    root.setBounds (0, 0, na::ui::BookFrame::baseWidth, na::ui::BookFrame::baseHeight);
}

} // namespace na::book
