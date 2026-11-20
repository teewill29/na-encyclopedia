#include "BookViews.h"
#include "Chapters.h"
#include "Feedback.h"
#include "content/Template.h"

#include <cmath>

namespace na::vol4
{

namespace
{
    // docs/UI-REFERENCE.md §3
    const juce::Colour paper      { 0xFFF4ECDB };
    const juce::Colour paperField { 0xFFFBF7EE };
    const juce::Colour paperShade { 0xFFDDCFB3 };
    const juce::Colour rule       { 0xFFC2B08A };
    const juce::Colour ink        { 0xFF2A2118 };
    const juce::Colour inkSoft    { 0xFF4A3C2C };
    const juce::Colour inkMuted   { 0xFF5E4E3B };
    const juce::Colour labelInk   { 0xFF6B5A45 };
    const juce::Colour good       { 0xFF2F5E3A };
    const juce::Colour warnLow    { 0xFF9A2E1F };
    const juce::Colour warnHigh   { 0xFF8A5410 };
    const juce::Colour paperPanel { 0xFFEFE5CF };
    const juce::Colour meterTrack { 0xFFDCCFB3 };
    const juce::Colour cloth      { 0xFF213B2F };
    const juce::Colour gold       { 0xFFCDAE5E };

    juce::Font font (float size, bool bold = false, bool italic = false)
    {
        return juce::FontOptions (size).withStyle (bold ? (italic ? "Bold Italic" : "Bold") : (italic ? "Italic" : "Regular"));
    }

    /** Draws wrapped text at the top of `area`; returns the height used. */
    int paragraph (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                   const juce::Font& f, juce::Colour colour, juce::Justification just = juce::Justification::topLeft)
    {
        if (text.isEmpty()) return 0;
        juce::AttributedString s;
        s.append (text, f, colour);
        s.setJustification (just);
        s.setLineSpacing (f.getHeight() * 0.3f);
        juce::TextLayout layout;
        layout.createLayout (s, (float) area.getWidth());
        layout.draw (g, area.toFloat().withHeight (layout.getHeight()));
        return (int) std::ceil (layout.getHeight());
    }

    /** Height `text` needs when wrapped to `width`. */
    int measure (const juce::String& text, const juce::Font& f, int width)
    {
        if (text.isEmpty()) return 0;
        juce::AttributedString s;
        s.append (text, f);
        s.setLineSpacing (f.getHeight() * 0.3f);
        juce::TextLayout layout;
        layout.createLayout (s, (float) width);
        return (int) std::ceil (layout.getHeight());
    }

    void drawCheck (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
    {
        juce::Path p;
        p.startNewSubPath (r.getX() + r.getWidth() * 0.15f, r.getCentreY());
        p.lineTo (r.getX() + r.getWidth() * 0.42f, r.getBottom() - r.getHeight() * 0.18f);
        p.lineTo (r.getRight() - r.getWidth() * 0.1f, r.getY() + r.getHeight() * 0.2f);
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void styleButton (juce::TextButton& b, bool primary)
    {
        b.setColour (juce::TextButton::buttonColourId, primary ? cloth : paperField);
        b.setColour (juce::TextButton::textColourOffId, primary ? gold : ink);
        b.setColour (juce::ComboBox::outlineColourId, rule);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
GoalCard::GoalCard (const juce::String& id, const juce::String& n, const juce::String& t)
    : juce::Button (n), goalId (id), name (n), tagline (t)
{
    setClickingTogglesState (true);
    setRadioGroupId (1);
}

void GoalCard::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();
    g.setColour (on ? paperShade : (highlighted ? paperField.darker (0.03f) : paperField));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? cloth : rule);
    g.drawRoundedRectangle (r, 6.0f, on ? 2.0f : 1.0f);

    auto inner = getLocalBounds().reduced (14, 10);
    if (on)
        drawCheck (g, inner.removeFromRight (20).removeFromTop (20).toFloat(), cloth);
    g.setColour (ink);
    g.setFont (font (17.0f, true));
    g.drawText (name, inner.removeFromTop (24), juce::Justification::centredLeft);
    g.setColour (inkMuted);
    g.setFont (font (14.0f, false, true));
    g.drawFittedText (tagline, inner, juce::Justification::topLeft, 2);
}

// ═════════════════════════════════════════════════════════════════════════════
TitleView::TitleView (CompressionAudioProcessor& p)
    : processor (p), content (p.getContent())
{
    if (auto* list = content.instruments())
        for (int i = 0; i < list->size(); ++i)
        {
            instrumentIds.add ((*list)[i]["id"].toString());
            instrumentBox.addItem ((*list)[i]["name"].toString(), i + 1);
        }
    instrumentBox.onChange = [this] { showGoalsFor (instrumentIds[instrumentBox.getSelectedId() - 1], {}); };
    addAndMakeVisible (instrumentBox);

    beginButton.setButtonText (text ("begin"));
    styleButton (beginButton, true);
    beginButton.onClick = [this]
    {
        const auto instrument = instrumentIds[instrumentBox.getSelectedId() - 1];
        if (selectedGoal.isNotEmpty())
            processor.beginBook (instrument, selectedGoal);
    };
    addAndMakeVisible (beginButton);

    refresh();
}

juce::String TitleView::text (const char* key) const
{
    return content.getString (juce::String ("book.titlePage.") + key);
}

void TitleView::refresh()
{
    const auto& session = processor.getSession();
    auto index = instrumentIds.indexOf (session.getInstrumentId());
    if (index < 0) index = 0;
    instrumentBox.setSelectedId (index + 1, juce::dontSendNotification);
    showGoalsFor (instrumentIds[index], session.getGoalId());
}

void TitleView::showGoalsFor (const juce::String& instrumentId, const juce::String& selectGoal)
{
    goalCards.clear();
    selectedGoal = {};
    const auto instrument = content.findInstrument (instrumentId);
    lesson = instrument["lesson"].toString();

    if (auto* goals = instrument["goals"].getArray())
        for (const auto& goalVar : *goals)
        {
            auto* card = goalCards.add (new GoalCard (goalVar["id"].toString(), goalVar["name"].toString(), goalVar["tagline"].toString()));
            card->onClick = [this, card] { if (card->getToggleState()) { selectedGoal = card->goalId; beginButton.setEnabled (true); } };
            addAndMakeVisible (card);
        }

    // Preselect: the saved goal, or the only goal when there's just one.
    for (auto* card : goalCards)
        if (card->goalId == selectGoal || goalCards.size() == 1)
        {
            card->setToggleState (true, juce::dontSendNotification);
            selectedGoal = card->goalId;
        }

    beginButton.setEnabled (selectedGoal.isNotEmpty());
    resized();
    repaint();
}

void TitleView::paint (juce::Graphics& g)
{
    g.fillAll (paper);
    auto bounds = getLocalBounds();
    auto left = bounds.removeFromLeft (bounds.getWidth() / 2).reduced (40);
    auto right = bounds.reduced (40, 40);

    g.setColour (paperShade);
    g.fillRect (getWidth() / 2 - 1, 0, 2, getHeight());

    // Left page: the book's title (UI-REFERENCE §6.1)
    auto y = left.getY() + 150;
    auto line = [&] (const juce::String& s, const juce::Font& f, juce::Colour c, int h)
    {
        g.setColour (c);
        g.setFont (f);
        g.drawText (s, left.getX(), y, left.getWidth(), h, juce::Justification::centred);
        y += h;
    };
    line (text ("brand"), font (13.0f, true), labelInk, 30);
    g.setColour (rule); g.fillRect (left.getCentreX() - 60, y + 6, 120, 1); y += 20;
    line (text ("series"), font (24.0f, false, true), inkSoft, 40);
    line (text ("topic"), font (56.0f, true), ink, 76);
    line (text ("volume"), font (13.0f, true), labelInk, 30);
    g.setColour (rule); g.fillRect (left.getCentreX() - 60, y + 6, 120, 1); y += 20;
    line (text ("edition"), font (20.0f, false, true), inkSoft, 34);

    // Right page
    y = right.getY();
    g.setColour (labelInk); g.setFont (font (13.0f, true));
    g.drawText (text ("eyebrow"), right.getX(), y, right.getWidth(), 20, juce::Justification::centredLeft); y += 26;
    g.setColour (ink); g.setFont (font (34.0f, true));
    g.drawText (text ("heading"), right.getX(), y, right.getWidth(), 44, juce::Justification::centredLeft); y += 50;
    paragraph (g, text ("intro"), { right.getX(), y, right.getWidth(), 80 }, font (17.0f), inkSoft);
    g.setColour (ink); g.setFont (font (16.0f, true));
    g.drawText (text ("instrumentQuestion"), right.getX(), instrumentBox.getY() - 28, right.getWidth(), 22, juce::Justification::centredLeft);
    g.drawText (text ("goalQuestion"), right.getX(), instrumentBox.getBottom() + 22, right.getWidth(), 22, juce::Justification::centredLeft);

    if (lesson.isNotEmpty() && ! goalCards.isEmpty())
        paragraph (g, lesson, { right.getX(), goalCards.getLast()->getBottom() + 16, right.getWidth(), 100 },
                   font (15.0f, false, true), inkSoft);
}

void TitleView::resized()
{
    auto right = getLocalBounds().removeFromRight (getWidth() / 2).reduced (40, 40);
    // Matches paint(): eyebrow 26, heading 50, intro, 24 gap, question 28
    const auto introH = measure (text ("intro"), font (17.0f), right.getWidth());
    instrumentBox.setBounds (right.getX(), right.getY() + 26 + 50 + introH + 24 + 28, right.getWidth(), 34);

    const int cardTop = instrumentBox.getBottom() + 50;
    const int cardW = (right.getWidth() - 12) / 2, cardH = 76;
    for (int i = 0; i < goalCards.size(); ++i)
        goalCards[i]->setBounds (right.getX() + (i % 2) * (cardW + 12), cardTop + (i / 2) * (cardH + 12), cardW, cardH);

    beginButton.setBounds (right.getX(), right.getBottom() - 52, right.getWidth(), 52);
}

// ═════════════════════════════════════════════════════════════════════════════
ChapterRow::ChapterRow (const juce::String& id, const juce::String& n, const juce::String& t, const juce::String& rn)
    : juce::Button (t), chapterId (id), numeral (n), title (t), readingNow (rn) {}

void ChapterRow::setStatus (bool isCurrent, bool isDone)
{
    current = isCurrent;
    done = isDone;
    repaint();
}

void ChapterRow::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds();
    if (current)
    {
        g.setColour (paperShade);
        g.fillRect (r);
        g.setColour (cloth);
        g.fillRect (r.removeFromLeft (3));
    }
    else if (highlighted)
    {
        g.setColour (paperShade.withAlpha (0.4f));
        g.fillRect (r);
    }

    auto inner = getLocalBounds().reduced (10, 0);
    const auto c = current || done ? ink : inkMuted;
    g.setColour (c);
    g.setFont (font (14.0f, true));
    g.drawText (numeral, inner.removeFromLeft (30), juce::Justification::centredLeft);

    auto status = inner.removeFromRight (90);
    if (done)
        drawCheck (g, status.removeFromRight (18).withSizeKeepingCentre (18, 18).toFloat(), good);
    else if (current)
    {
        g.setColour (inkMuted);
        g.setFont (font (13.0f, false, true));
        g.drawText (readingNow, status, juce::Justification::centredRight);
    }

    g.setColour (c);
    g.setFont (font (16.0f, current));
    g.drawText (title, inner, juce::Justification::centredLeft);
}

// ═════════════════════════════════════════════════════════════════════════════
ChapterView::ChapterView (CompressionAudioProcessor& p)
    : processor (p), content (p.getContent())
{
    changeButton.setButtonText (text ("change"));
    changeButton.onClick = [this] { processor.getSession().showTitlePage(); };
    changeButton.setColour (juce::TextButton::buttonColourId, paper);
    changeButton.setColour (juce::TextButton::textColourOffId, inkSoft);
    addAndMakeVisible (changeButton);

    styleButton (previousButton, false);
    previousButton.onClick = [this] { processor.previousChapter(); };
    addAndMakeVisible (previousButton);

    styleButton (nextButton, true);
    nextButton.onClick = [this] { processor.nextChapter(); };
    addAndMakeVisible (nextButton);

    knob.setColour (juce::Slider::rotarySliderFillColourId, gold);
    knob.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xFF3A3A3A));
    knob.setColour (juce::Slider::thumbColourId, gold);
    knob.setColour (juce::Slider::textBoxTextColourId, ink);
    knob.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 140, 26);
    addChildComponent (knob);

    startTimerHz (10); // live values: dynamic range, "set so far"
}

ChapterView::~ChapterView()
{
    stopTimer();
}

juce::String ChapterView::text (const char* key) const
{
    return content.getString (juce::String ("book.chapterPage.") + key);
}

juce::String ChapterView::fill (const juce::String& templ, std::initializer_list<std::pair<juce::String, juce::String>> values) const
{
    return na::Template::fill (templ, na::Template::forValues (values));
}

juce::String ChapterView::chapterText (const char* field) const
{
    return na::Template::fill (chapter[field].toString(), na::Template::forGoal (goal));
}

void ChapterView::refresh()
{
    const auto& session = processor.getSession();
    const auto& ids = session.getChapterIds();
    const auto current = session.getCurrentChapterId();
    goal = session.getGoal();
    chapter = content.findChapter (current);

    // Contents rows
    if (rows.size() != ids.size() || (rows.size() > 0 && rows[0]->chapterId != ids[0]))
    {
        rows.clear();
        for (const auto& id : ids)
        {
            const auto c = content.findChapter (id);
            auto* row = rows.add (new ChapterRow (id, c["numeral"].toString(), c["title"].toString(), text ("readingNow")));
            row->onClick = [this, id] { processor.openChapter (id); };
            addAndMakeVisible (row);
        }
    }
    for (auto* row : rows)
        row->setStatus (row->chapterId == current, session.isDone (row->chapterId));

    // The chapter's knob (rebuilt only when the chapter changes, so a refresh
    // for a new check mark doesn't interrupt a drag)
    const auto* chapterRule = ruleFor (current);
    if (current != attachedChapter)
    {
        attachedChapter = current;
        knobAttachment.reset();
        knob.setVisible (chapterRule != nullptr && chapterRule->paramId != nullptr);
        if (knob.isVisible())
            knobAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                processor.getValueTree(), chapterRule->paramId, knob);
    }
    knobName = {};
    targetCaption = {};
    if (knob.isVisible())
    {
        knobName = content.getString (juce::String ("parameters.") + chapterRule->paramId + ".name").toUpperCase();
        if (chapterRule->targetKey != nullptr && goal[chapterRule->targetKey].isObject())
        {
            const auto range = goal[chapterRule->targetKey];
            const auto min = na::Template::formatNumber ((double) range["min"]);
            const auto max = na::Template::formatNumber ((double) range["max"]);
            const juce::String key (chapterRule->targetKey);
            targetCaption = key == "grDb"
                ? fill (text ("targetGr"), { { "min", min }, { "max", max } })
                : fill (text ("target"), { { "min", min }, { "max", max },
                                           { "unit", content.getString ("book.units." + key) } });
        }
    }

    // Previous / next
    const auto index = session.getCurrentIndex();
    previousButton.setVisible (index > 0);
    nextButton.setVisible (index >= 0 && index + 1 < ids.size());
    if (index > 0)
        previousButton.setButtonText (fill (text ("previous"), { { "numeral", content.findChapter (ids[index - 1])["numeral"].toString() } }));
    if (index + 1 < ids.size())
        nextButton.setButtonText (fill (text ("next"), { { "numeral", content.findChapter (ids[index + 1])["numeral"].toString() } }));

    resized();
    repaint();
}

void ChapterView::paint (juce::Graphics& g)
{
    g.fillAll (paper);
    g.setColour (paperShade);
    g.fillRect (leftPage.getRight() + 15, 0, 2, getHeight());

    const auto& session = processor.getSession();

    // ── Left page: Contents ─────────────────────────────────────────────────
    {
        auto area = leftPage;
        g.setColour (labelInk); g.setFont (font (13.0f, true));
        g.drawText (text ("contents"), area.removeFromTop (24), juce::Justification::centredLeft);
        const auto setupLine = fill (text ("setup"), { { "instrument", session.getInstrument()["name"].toString() },
                                                       { "goal", goal["name"].toString() } });
        g.setColour (ink); g.setFont (font (16.0f));
        g.drawText (setupLine, area.removeFromTop (34).withTrimmedRight (90), juce::Justification::centredLeft);

        // Set so far: the knobs of the chapters before this one
        auto sf = setSoFarArea;
        g.setColour (labelInk); g.setFont (font (13.0f, true));
        g.drawText (text ("setSoFar"), sf.removeFromTop (24), juce::Justification::centredLeft);
        const auto& ids = session.getChapterIds();
        for (int i = 0; i < session.getCurrentIndex() && i < ids.size(); ++i)
            if (const auto* r = ruleFor (ids[i]); r != nullptr && r->paramId != nullptr)
                if (auto* param = processor.getValueTree().getParameter (r->paramId))
                {
                    auto row = sf.removeFromTop (24);
                    g.setColour (inkMuted); g.setFont (font (15.0f));
                    g.drawText (param->getName (64), row, juce::Justification::centredLeft);
                    g.setColour (ink);
                    g.drawText (param->getCurrentValueAsText(), row, juce::Justification::centredRight);
                }
    }

    // ── Right page: the chapter ─────────────────────────────────────────────
    auto area = rightPage;
    auto top = area.removeFromTop (24);
    g.setColour (labelInk); g.setFont (font (13.0f, true));
    g.drawText (fill (text ("eyebrow"), { { "numeral", chapter["numeral"].toString() } }), top, juce::Justification::centredLeft);
    g.setColour (inkMuted); g.setFont (font (15.0f, false, true));
    g.drawText (fill (text ("counter"), { { "current", juce::String (session.getCurrentIndex() + 1) },
                                          { "total", juce::String (session.getChapterIds().size()) } }),
                top, juce::Justification::centredRight);

    g.setColour (ink); g.setFont (font (38.0f, true));
    g.drawText (chapter["title"].toString(), area.removeFromTop (52), juce::Justification::centredLeft);
    if (chapter.hasProperty ("subtitle"))
    {
        g.setColour (inkSoft); g.setFont (font (17.0f, false, true));
        g.drawText (chapterText ("subtitle"), area.removeFromTop (26), juce::Justification::centredLeft);
    }
    area.removeFromTop (6);
    paragraph (g, chapterText ("intro"), area.withBottom (knobBlock.getY() - 8), font (17.0f), inkSoft);

    // Knob block: name above, target below (the knob draws its own value)
    if (knob.isVisible())
    {
        g.setColour (labelInk); g.setFont (font (12.0f, true));
        g.drawText (knobName, knobBlock.withHeight (20), juce::Justification::centred);
        paragraph (g, targetCaption, knobBlock.withTop (knob.getBottom() + 4), font (14.0f, false, true), inkMuted,
                   juce::Justification::centredTop);
    }
    else if (chapter["id"].toString() == "goal")
    {
        // Chapter I: no knob; the dynamic range readout instead (UI-REFERENCE §6.3)
        const auto dr = processor.getMetrics().dynamicRangeDb;
        auto block = knobBlock.withTrimmedTop (40);
        g.setColour (ink);
        if (std::isnan (dr))
            paragraph (g, text ("dynamicRangeWaiting"), block, font (16.0f, false, true), inkMuted, juce::Justification::centredTop);
        else
            paragraph (g, fill (text ("dynamicRange"), { { "value", juce::String (dr, 1) } }),
                       block, font (30.0f, true), ink, juce::Justification::centredTop);
    }

    // Instructions
    auto instructions = textBlock;
    auto section = [&] (const char* eyebrowKey, const juce::String& body)
    {
        if (body.isEmpty()) return;
        g.setColour (labelInk); g.setFont (font (12.0f, true));
        g.drawText (text (eyebrowKey), instructions.removeFromTop (18), juce::Justification::centredLeft);
        instructions.removeFromTop (paragraph (g, body, instructions, font (15.5f), ink) + 10);
    };
    const bool fromSlow = goal["attackMs"]["approach"].toString() == "fromSlow";
    section ("doThis", chapter.hasProperty ("doThis") ? chapterText ("doThis")
                       : chapterText (fromSlow ? "doThisFromSlow" : "doThisFromFast"));
    section ("listenFor", chapterText ("listenFor"));
    section ("tryThis", chapterText ("tryThis"));

    paintReading (g, readingArea);
}

void ChapterView::paintReading (juce::Graphics& g, juce::Rectangle<int> area)
{
    using Phase = na::FeedbackTracker::Phase;
    using Tone = Feedback::Tone;

    const auto& fb = processor.getFeedback();
    const auto chapterId = chapter["id"].toString();
    const auto group = Feedback::messageGroupFor (chapterId);

    juce::String statusKey, messageKey;
    Tone tone = Tone::neutral;
    if (fb.phase == Phase::silence)
        statusKey = "silence", messageKey = "silence";
    else if (fb.phase == Phase::listening || fb.zone.empty())
        statusKey = "listening", messageKey = "listening";
    else
    {
        statusKey = group + "." + juce::String (fb.zone);
        messageKey = statusKey;
        tone = Feedback::toneFor (chapterId, fb.zone);
    }
    const auto toneColour = tone == Tone::good ? good : tone == Tone::tooMuch ? warnLow : tone == Tone::tooLittle ? warnHigh : inkMuted;
    const float alpha = fb.dimmed ? 0.45f : 1.0f;

    g.setColour (paperPanel);
    g.fillRoundedRectangle (area.toFloat(), 6.0f);
    g.setColour (rule);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 6.0f, 1.0f);

    auto inner = area.reduced (16, 10);
    auto header = inner.removeFromTop (18);
    g.setColour (labelInk); g.setFont (font (12.0f, true));
    g.drawText (content.getString ("book.reading.title"), header, juce::Justification::centredLeft);
    g.setColour (inkMuted); g.setFont (font (13.0f, false, true));
    g.drawText (content.getString ("book.reading.live"), header.withTrimmedRight (0), juce::Justification::centredRight);
    const auto liveW = juce::GlyphArrangement::getStringWidthInt (font (13.0f, false, true), content.getString ("book.reading.live"));
    g.setColour (fb.phase == Phase::silence ? inkMuted.withAlpha (0.4f) : good);
    g.fillEllipse ((float) (header.getRight() - liveW - 14), (float) header.getCentreY() - 4.0f, 8.0f, 8.0f);
    inner.removeFromTop (6);

    // Status: dot + label (in words) + coaching message
    auto status = inner.removeFromTop (48);
    g.setColour (toneColour.withMultipliedAlpha (alpha));
    g.fillEllipse ((float) status.getX(), (float) status.getY() + 5.0f, 12.0f, 12.0f);
    status.removeFromLeft (20);
    g.setFont (font (18.0f, true));
    g.drawText (content.getString ("book.reading.status." + statusKey), status.removeFromLeft (190).withHeight (22), juce::Justification::centredLeft);
    paragraph (g, content.getString ("messages." + messageKey), status, font (15.0f), ink.withMultipliedAlpha (alpha));

    // Meters, side by side
    const auto meters = Feedback::metersFor (chapterId, goal, processor.getMetrics(), processor.getSettings());
    if (meters.empty()) return;
    inner.removeFromTop (4);
    const int gap = 24;
    const int w = (inner.getWidth() - gap * ((int) meters.size() - 1)) / (int) meters.size();
    for (size_t i = 0; i < meters.size(); ++i)
    {
        const auto& m = meters[i];
        auto r = juce::Rectangle<int> (inner.getX() + (int) i * (w + gap), inner.getY(), w, 34);
        auto labelRow = r.removeFromTop (18);
        g.setColour (inkSoft); g.setFont (font (13.0f));
        g.drawText (content.getString (juce::String ("book.reading.meters.") + m.labelKey), labelRow, juce::Justification::centredLeft);
        g.setColour (ink);
        g.drawText (std::isnan (m.value) ? content.getString ("debug.none")
                                         : (m.decimals > 0 ? juce::String (m.value, m.decimals) : juce::String (juce::roundToInt (m.value))) + content.getString (juce::String ("debug.units.") + m.unitKey),
                    labelRow, juce::Justification::centredRight);

        auto track = r.removeFromTop (12).toFloat();
        const auto toX = [&] (float v) { return track.getX() + track.getWidth() * juce::jlimit (0.0f, 1.0f, (v - m.rangeMin) / (m.rangeMax - m.rangeMin)); };
        g.setColour (meterTrack);
        g.fillRoundedRectangle (track, 6.0f);
        if (! std::isnan (m.targetMin))
        {
            const auto zone = juce::Rectangle<float>::leftTopRightBottom (toX (m.targetMin), track.getY(), toX (m.targetMax), track.getBottom());
            g.setColour (good.withAlpha (0.22f));
            g.fillRect (zone);
            g.setColour (good);
            g.fillRect (zone.withWidth (1.0f));
            g.fillRect (zone.withX (zone.getRight() - 1.0f).withWidth (1.0f));
        }
        if (! std::isnan (m.value))
        {
            const auto zeroX = toX (juce::jmax (m.rangeMin, 0.0f));
            const auto valueX = toX (m.value);
            const auto fill = juce::Rectangle<float>::leftTopRightBottom (juce::jmin (zeroX, valueX), track.getY() + 3.0f,
                                                                        juce::jmax (zeroX, valueX), track.getBottom() - 3.0f);
            // Single-meter chapters colour the fill by status; the plain GR meter stays neutral.
            g.setColour ((m.showsStatus ? toneColour : inkMuted).withMultipliedAlpha (alpha));
            g.fillRoundedRectangle (fill.withWidth (juce::jmax (2.0f, fill.getWidth())), 3.0f);
        }
    }
}

void ChapterView::resized()
{
    auto bounds = getLocalBounds().reduced (28, 24);
    leftPage = bounds.removeFromLeft (330);
    bounds.removeFromLeft (40);
    rightPage = bounds.withTrimmedBottom (62);

    changeButton.setBounds (leftPage.getRight() - 84, leftPage.getY() + 26, 84, 30);
    for (int i = 0; i < rows.size(); ++i)
        rows[i]->setBounds (leftPage.getX(), leftPage.getY() + 80 + i * 44, leftPage.getWidth(), 44);
    setSoFarArea = leftPage.withTop (leftPage.getY() + 80 + 6 * 44 + 30);

    // Matches paint(): eyebrow 24, title 52, subtitle 26, gap 6, intro, gap 24
    int y = rightPage.getY() + 24 + 52 + (chapter.hasProperty ("subtitle") ? 26 : 0) + 6;
    y += measure (chapterText ("intro"), font (17.0f), rightPage.getWidth()) + 24;
    // The Reading sits at the bottom of the right page, above the buttons.
    readingArea = rightPage.withTop (rightPage.getBottom() - 132);

    // Knob block: name 20, knob, value box (part of the knob), target caption 36
    knobBlock = { rightPage.getX(), y, 170, juce::jmax (120, readingArea.getY() - 12 - y) };
    const auto knobH = juce::jlimit (90, 170, knobBlock.getHeight() - 20 - 36);
    knob.setBounds (knobBlock.getX() + 10, knobBlock.getY() + 20, 150, knobH);
    textBlock = { knobBlock.getRight() + 32, knobBlock.getY(), rightPage.getRight() - knobBlock.getRight() - 32, readingArea.getY() - 12 - knobBlock.getY() };

    const auto bottom = getLocalBounds().reduced (28, 24).removeFromBottom (50);
    nextButton.setBounds (bottom.getRight() - 220, bottom.getY(), 220, 50);
    previousButton.setBounds (nextButton.getX() - 200, bottom.getY(), 180, 50);
}

} // namespace na::vol4
