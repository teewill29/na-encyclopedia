#include "BookViews.h"
#include "Chapters.h"
#include "Feedback.h"
#include "content/Template.h"

#include <cmath>

namespace na::vol4
{

using namespace na::ui;
namespace P = na::ui::Palette;
namespace F = na::ui::Fonts;

namespace
{
    constexpr float bodyLine = 1.45f;

    /** Ornament rule: line, open diamond, line (180 px wide). */
    void ornament (juce::Graphics& g, float cx, float cy)
    {
        g.setColour (P::goldLine);
        g.fillRect (cx - 90.0f, cy - 0.5f, 78.0f, 1.0f);
        g.fillRect (cx + 12.0f, cy - 0.5f, 78.0f, 1.0f);
        juce::Path diamond;
        diamond.addRectangle (-5.0f, -5.0f, 10.0f, 10.0f);
        diamond.applyTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::pi * 0.25f).translated (cx, cy));
        g.strokePath (diamond, juce::PathStrokeType (1.2f));
    }

    void eyebrow (juce::Graphics& g, const juce::String& s, juce::Rectangle<int> r, float size = 13.0f, float tracking = 0.24f,
                  juce::Colour c = P::labelInk, juce::Justification j = juce::Justification::centredLeft)
    {
        g.setColour (c);
        g.setFont (F::cinzel (size, 500, tracking));
        g.drawText (s, r, j, false);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
GoalCard::GoalCard (const juce::String& id, const juce::String& n, const juce::String& t, juce::Colour c)
    : juce::Button (n), goalId (id), name (n), tagline (t), cloth (c)
{
    setClickingTogglesState (true);
    setRadioGroupId (1);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void GoalCard::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat();
    const bool on = getToggleState();
    g.setColour (on ? P::cardOn : (highlighted ? P::paperField.darker (0.025f) : P::paperField));
    g.fillRoundedRectangle (r.reduced (1.0f), 6.0f);
    g.setColour (on ? cloth : P::cardBorder);
    g.drawRoundedRectangle (r.reduced (on ? 1.0f : 0.75f), 6.0f, on ? 2.0f : 1.5f);

    auto inner = getLocalBounds().reduced (14, 12);
    auto nameRow = inner.removeFromTop (24);
    const auto nameFont = F::garamond (18.0f, 600);
    g.setColour (P::ink);
    g.setFont (nameFont);
    g.drawText (name, nameRow, juce::Justification::centredLeft);
    if (on)
        drawIcon (g, Icon::check, { nameRow.getX() + Text::width (name, nameFont) + 6.0f, nameRow.getCentreY() - 8.0f, 16.0f, 16.0f }, P::good);
    inner.removeFromTop (2);
    Text::draw (g, tagline, inner, F::garamond (15.0f), P::inkSoft, juce::Justification::topLeft, 1.3f);
}

// ═════════════════════════════════════════════════════════════════════════════
TitleView::TitleView (CompressionAudioProcessor& p)
    : processor (p), content (p.getContent()),
      cloth (colourFromHex (p.getContent().getString ("book.cloth"), juce::Colour (0xFF213B2F)))
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
    setStyle (beginButton, ButtonStyle::primary);
    beginButton.onClick = [this]
    {
        if (selectedGoal.isNotEmpty())
            processor.beginBook (instrumentIds[instrumentBox.getSelectedId() - 1], selectedGoal);
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
            auto* card = goalCards.add (new GoalCard (goalVar["id"].toString(), goalVar["name"].toString(),
                                                      goalVar["tagline"].toString(), cloth));
            card->onClick = [this, card] { if (card->getToggleState()) { selectedGoal = card->goalId; beginButton.setEnabled (true); } };
            addAndMakeVisible (card);
        }

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

void TitleView::resized()
{
    // Right page: padding 56 / 56 / 48 (mockup)
    auto r = rightPage().withTrimmedTop (56).withTrimmedBottom (48).reduced (56, 0);
    int y = r.getY();
    y += 20 + 6 + 40 + 4;                                                          // eyebrow, heading
    y += Text::measure (text ("intro"), F::garamond (18.0f), r.getWidth(), bodyLine) + 22;
    questionY = y;
    y += 26 + 8;
    instrumentBox.setBounds (r.getX(), y, r.getWidth(), 48);
    y = instrumentBox.getBottom() + 22;
    goalQuestionY = y;
    y += 26 + 10;
    const int cardW = (r.getWidth() - 10) / 2, cardH = 84;
    for (int i = 0; i < goalCards.size(); ++i)
        goalCards[i]->setBounds (r.getX() + (i % 2) * (cardW + 10), y + (i / 2) * (cardH + 10), cardW, cardH);
    lessonY = goalCards.isEmpty() ? y : goalCards.getLast()->getBottom() + 16;

    beginButton.setBounds (r.getX(), r.getBottom() - 54 - 10 - 22, r.getWidth(), 54);
}

void TitleView::paint (juce::Graphics& g)
{
    // ── Left page: the title, centred (UI-REFERENCE §6.1) ───────────────────
    {
        const auto l = leftPage();
        const float cx = (float) l.getCentreX();
        const int w = l.getWidth() - 112;
        // Column: 13 + 18 + 14 + 18 + 32 + 18 + 64 + 18 + 18 + 18 + 14 + 18 + 24 = 307
        int y = l.getCentreY() - 307 / 2;
        auto row = [&] (int h) { auto rr = juce::Rectangle<int> (l.getX() + 56, y, w, h); y += h + 18; return rr; };
        eyebrow (g, text ("brand"), row (13), 13.0f, 0.32f, P::labelInk, juce::Justification::centred);
        ornament (g, cx, (float) y + 7.0f); y += 14 + 18;
        g.setColour (P::inkSoft); g.setFont (F::garamond (26.0f, 400, true));
        g.drawText (text ("series"), row (32), juce::Justification::centred);
        g.setColour (P::ink); g.setFont (F::cinzel (58.0f, 600, 0.04f));
        g.drawFittedText (text ("topic"), row (64).expanded (40, 0), juce::Justification::centred, 1, 0.7f);
        eyebrow (g, text ("volume"), row (18), 15.0f, 0.24f, P::inkSoft, juce::Justification::centred);
        ornament (g, cx, (float) y + 7.0f); y += 14 + 18;
        g.setColour (P::inkMuted); g.setFont (F::garamond (19.0f, 400, true));
        g.drawText (text ("edition"), row (24), juce::Justification::centred);
    }

    // ── Right page ──────────────────────────────────────────────────────────
    auto r = rightPage().withTrimmedTop (56).withTrimmedBottom (48).reduced (56, 0);
    eyebrow (g, text ("eyebrow"), r.withHeight (20));
    g.setColour (P::ink); g.setFont (F::garamond (34.0f, 500));
    g.drawText (text ("heading"), r.withY (r.getY() + 26).withHeight (40), juce::Justification::centredLeft);
    Text::draw (g, text ("intro"), r.withY (r.getY() + 26 + 44).withHeight (80), F::garamond (18.0f), P::inkSoft,
                juce::Justification::topLeft, bodyLine);

    g.setColour (P::ink); g.setFont (F::garamond (17.0f, 600));
    g.drawText (text ("instrumentQuestion"), r.withY (questionY).withHeight (26), juce::Justification::centredLeft);
    g.drawText (text ("goalQuestion"), r.withY (goalQuestionY).withHeight (26), juce::Justification::centredLeft);

    if (lesson.isNotEmpty())
        Text::draw (g, lesson, r.withY (lessonY).withBottom (beginButton.getY() - 10), F::garamond (16.0f, 400, true),
                    P::inkSoft, juce::Justification::topLeft, 1.4f);

    g.setColour (P::inkMuted); g.setFont (F::garamond (15.0f, 400, true));
    g.drawText (text ("footnote"), r.withY (beginButton.getBottom() + 10).withHeight (22), juce::Justification::centred);
}

// ═════════════════════════════════════════════════════════════════════════════
ChapterRow::ChapterRow (const juce::String& id, const juce::String& n, const juce::String& t, const juce::String& rn, juce::Colour c)
    : juce::Button (t), chapterId (id), numeral (n), title (t), readingNow (rn), cloth (c)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ChapterRow::setStatus (bool isCurrent, bool isDone)
{
    current = isCurrent;
    done = isDone;
    repaint();
}

void ChapterRow::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat();
    if (current)
    {
        g.setColour (P::rowCurrent);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (cloth);
        g.fillRect (r.removeFromLeft (3.0f));
    }
    else if (highlighted)
    {
        g.setColour (P::rowCurrent.withAlpha (0.45f));
        g.fillRoundedRectangle (r, 4.0f);
    }

    auto inner = getLocalBounds().reduced (10, 0);
    const bool notYet = ! current && ! done;
    g.setColour (current ? P::ink : P::labelInk);
    g.setFont (F::cinzel (15.0f, current ? 700 : 500));
    g.drawText (numeral, inner.removeFromLeft (30), juce::Justification::centredLeft);
    inner.removeFromLeft (12);

    const auto readingFont = F::garamond (14.0f, 400, true);
    auto status = inner.removeFromRight ((current ? (int) Text::width (readingNow, readingFont) + 8 : 0) + (done ? 24 : 0));
    if (done)
    {
        drawIcon (g, Icon::check, status.removeFromRight (18).withSizeKeepingCentre (18, 18).toFloat(), P::good);
        status.removeFromRight (6);
    }
    if (current)
    {
        g.setColour (P::inkSoft);
        g.setFont (readingFont);
        g.drawText (readingNow, status, juce::Justification::centredRight);
    }

    g.setColour (notYet ? P::labelInk : P::ink);
    g.setFont (F::garamond (18.0f, current ? 600 : 400));
    g.drawFittedText (title, inner, juce::Justification::centredLeft, 1, 0.8f);
}

// ═════════════════════════════════════════════════════════════════════════════
ChapterView::ChapterView (CompressionAudioProcessor& p)
    : processor (p), content (p.getContent()),
      cloth (colourFromHex (p.getContent().getString ("book.cloth"), juce::Colour (0xFF213B2F)))
{
    changeButton.setButtonText (text ("change"));
    setStyle (changeButton, ButtonStyle::text);
    changeButton.onClick = [this] { processor.getSession().showTitlePage(); };
    addAndMakeVisible (changeButton);

    setStyle (previousButton, ButtonStyle::text);
    previousButton.onClick = [this] { processor.previousChapter(); };
    addAndMakeVisible (previousButton);

    setStyle (nextButton, ButtonStyle::primary);
    nextButton.onClick = [this] { processor.nextChapter(); };
    addAndMakeVisible (nextButton);

    knob.setActive (true);
    addChildComponent (knob);

    startTimerHz (15); // live values: The Reading, Chapter I readout, Set so far
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

juce::String ChapterView::doThisText() const
{
    if (chapter.hasProperty ("doThis")) return chapterText ("doThis");
    return chapterText (goal["attackMs"]["approach"].toString() == "fromSlow" ? "doThisFromSlow" : "doThisFromFast");
}

void ChapterView::refresh()
{
    const auto& session = processor.getSession();
    const auto& ids = session.getChapterIds();
    const auto current = session.getCurrentChapterId();
    goal = session.getGoal();
    chapter = content.findChapter (current);

    if (rows.size() != ids.size() || (rows.size() > 0 && rows[0]->chapterId != ids[0]))
    {
        rows.clear();
        for (const auto& id : ids)
        {
            const auto c = content.findChapter (id);
            auto* row = rows.add (new ChapterRow (id, c["numeral"].toString(), c["title"].toString(), text ("readingNow"), cloth));
            row->onClick = [this, id] { processor.openChapter (id); };
            addAndMakeVisible (row);
        }
    }
    for (auto* row : rows)
        row->setStatus (row->chapterId == current, session.isDone (row->chapterId));

    // The chapter's knob; rebuilt only when the chapter changes, so a new
    // check mark doesn't interrupt a drag.
    const auto* rule = ruleFor (current);
    if (current != attachedChapter)
    {
        attachedChapter = current;
        knobAttachment.reset();
        knob.setVisible (rule != nullptr && rule->paramId != nullptr);
        if (knob.isVisible())
            knobAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.getValueTree(), rule->paramId, knob);
    }
    knobName = {};
    targetCaption = {};
    if (knob.isVisible())
    {
        knobName = content.getString (juce::String ("parameters.") + rule->paramId + ".name").toUpperCase();
        if (rule->targetKey != nullptr && goal[rule->targetKey].isObject())
        {
            const auto range = goal[rule->targetKey];
            const auto min = na::Template::formatNumber ((double) range["min"]);
            const auto max = na::Template::formatNumber ((double) range["max"]);
            const juce::String key (rule->targetKey);
            targetCaption = key == "grDb" ? fill (text ("targetGr"), { { "min", min }, { "max", max } })
                                          : fill (text ("target"), { { "min", min }, { "max", max }, { "unit", content.getString ("book.units." + key) } });
        }
    }

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

void ChapterView::layoutRightPage()
{
    // Right page padding 36 / 44 / 30 (mockup). Long chapters step the text
    // down (never below 15 px) instead of overlapping.
    const auto r = rightPage().withTrimmedTop (36).withTrimmedBottom (30).reduced (44, 0);
    const int bottomRow = 46, knobW = 170, colGap = 32;
    int gap = 18;

    // Every message this chapter can show, so The Reading's height never jumps.
    juce::StringArray messages { content.getString ("messages.silence"), content.getString ("messages.listening") };
    if (auto* group = content.get ("messages." + Feedback::messageGroupFor (chapter["id"].toString())).getDynamicObject())
        for (const auto& prop : group->getProperties())
            messages.add (prop.value.toString());
    const int captionH = Text::measure (targetCaption, F::garamond (14.0f, 400, true), knobW, 1.3f);
    const int knobExtras = 16 + 8 + 4 + 16 + 4 + 32 + 4 + captionH;    // name, range, value, target
    knobSize = 132;
    const bool hasMeters = ! Feedback::metersFor (chapter["id"].toString(), goal, processor.getMetrics(), processor.getSettings()).empty();

    readingPad = 16; readingGap = 12;
    for (int attempt = 0; attempt < 24; ++attempt)
    {
        int y = r.getY();
        header = { r.getX(), y, r.getWidth(), 20 };                         y += 20 + 8;
        titleArea = { r.getX(), y, r.getWidth(), 48 };                      y += 48 + 6;
        subtitleArea = { r.getX(), y, r.getWidth(), chapter.hasProperty ("subtitle") ? 26 : 0 };
        y += subtitleArea.getHeight();
        const int introH = Text::measure (chapterText ("intro"), F::garamond (introSize), r.getWidth(), bodyLine);
        introArea = { r.getX(), y, r.getWidth(), introH };                  y += introH + gap;

        const int instrW = r.getWidth() - knobW - colGap;
        int instrH = 0;
        for (const auto& body : { doThisText(), chapterText ("listenFor"), chapterText ("tryThis") })
            if (body.isNotEmpty())
                instrH += (instrH > 0 ? 14 : 0) + 17 + 4 + Text::measure (body, F::garamond (instructionSize), instrW, bodyLine);

        const int msgW = r.getWidth() - 40 - 24;
        int msgH = 0;
        for (const auto& m : messages)
            msgH = juce::jmax (msgH, Text::measure (m, F::garamond (messageSize), msgW, 1.4f));
        const int readingH = readingPad + 18 + readingGap + 26 + msgH + (hasMeters ? readingGap + na::ui::meterHeight : 0) + readingPad;
        reading = { r.getX(), r.getBottom() - bottomRow - gap - readingH, r.getWidth(), readingH };

        const int available = reading.getY() - gap - y;
        // The knob shrinks (to 96 px) before any text does.
        knobSize = juce::jlimit (96, 132, available - knobExtras);
        const int knobNeeded = knobExtras + knobSize;
        const int needed = juce::jmax (knob.isVisible() ? knobNeeded : 120, instrH);
        knobBlock = { r.getX(), y, knobW, juce::jmax (knobNeeded, available) };
        instructions = { r.getX() + knobW + colGap, y, instrW, juce::jmax (instrH, available) };
        if (needed <= available) break;

        // Tighten step by step; body text never below 14 px (spec minimum 13).
        if (instructionSize > 15.5f)      instructionSize -= 0.5f;
        else if (gap > 12)                gap -= 3;
        else if (readingPad > 12)         { readingPad -= 4; readingGap -= 4; }
        else if (introSize > 16.0f)       introSize -= 0.5f;
        else if (instructionSize > 14.0f) instructionSize -= 0.5f;
        else if (messageSize > 15.0f)     messageSize -= 0.5f;
        else if (introSize > 15.0f)       introSize -= 0.5f;
        else break;
    }

    // The slider is larger than the knob so the gold ring's shadow isn't clipped.
    const int pad = 18;
    knob.setBounds (knobBlock.getCentreX() - knobSize / 2 - pad, knobBlock.getY() + 16 + 8 - pad, knobSize + 2 * pad, knobSize + 2 * pad);
}

void ChapterView::resized()
{
    // ── Left page: padding 40 / 32 / 32 / 36 ────────────────────────────────
    const auto l = leftPage().withTrimmedTop (40).withTrimmedBottom (32).withTrimmedLeft (36).withTrimmedRight (32);
    changeButton.setBounds (l.getRight() - 64, l.getY() + 20, 64, 34);
    int y = l.getY() + 20 + 34 + 12 + 1 + 16;
    for (auto* row : rows)
    {
        row->setBounds (l.getX(), y, l.getWidth(), 44);
        y += 44 + 2;
    }
    setSoFarArea = l.withTop (l.getBottom() - (16 + 12 + 3 * 48 + 2 * 12));

    // ── Right page ──────────────────────────────────────────────────────────
    introSize = 18.0f; instructionSize = 17.0f; messageSize = 17.0f;
    layoutRightPage();
    const auto r = rightPage().withTrimmedTop (36).withTrimmedBottom (30).reduced (44, 0);
    const auto bottom = r.withTop (r.getBottom() - 46);
    const int nextW = (int) Text::width (nextButton.getButtonText(), F::cinzel (14.0f, 600, 0.14f)) + 44;
    nextButton.setBounds (bottom.getRight() - nextW, bottom.getY(), nextW, 46);
    const int prevW = (int) Text::width (previousButton.getButtonText(), F::garamond (16.0f, 400, true)) + 28;
    previousButton.setBounds ((nextButton.isVisible() ? nextButton.getX() - 10 : bottom.getRight()) - prevW, bottom.getY(), prevW, 46);
}

void ChapterView::paint (juce::Graphics& g)
{
    paintContents (g);
    paintChapter (g);
    paintReading (g, reading);
}

void ChapterView::paintContents (juce::Graphics& g)
{
    const auto& session = processor.getSession();
    const auto l = leftPage().withTrimmedTop (40).withTrimmedBottom (32).withTrimmedLeft (36).withTrimmedRight (32);

    eyebrow (g, text ("contents"), l.withHeight (16));
    const auto setupLine = fill (text ("setup"), { { "instrument", session.getInstrument()["name"].toString() },
                                                   { "goal", goal["name"].toString() } });
    g.setColour (P::ink);
    g.setFont (F::garamond (20.0f, 600));
    g.drawFittedText (setupLine, l.withY (l.getY() + 20).withHeight (34).withTrimmedRight (70), juce::Justification::centredLeft, 1, 0.8f);
    g.setColour (P::rule);
    g.fillRect (l.getX(), l.getY() + 20 + 34 + 12, l.getWidth(), 1);

    // Set so far: small knobs (48 px) with name and value, for chapters before this one
    auto sf = setSoFarArea;
    eyebrow (g, text ("setSoFar"), sf.removeFromTop (16), 12.0f);
    sf.removeFromTop (12);
    juce::StringArray params;
    const auto& ids = session.getChapterIds();
    for (int i = 0; i < session.getCurrentIndex() && i < ids.size(); ++i)
        if (const auto* r = ruleFor (ids[i]); r != nullptr && r->paramId != nullptr)
        {
            params.add (r->paramId);
            if (ids[i] == "ratioThreshold") params.add ("ratio");   // set by the goal in Chapter II
        }
    const int colW = (sf.getWidth() - 22) / 2;
    for (int i = 0; i < params.size() && i < 6; ++i)
        if (auto* param = processor.getValueTree().getParameter (params[i]))
        {
            auto cell = juce::Rectangle<int> (sf.getX() + (i % 2) * (colW + 22), sf.getY() + (i / 2) * 60, colW, 48);
            drawKnob (g, cell.removeFromLeft (48).toFloat(),
                      knobStartAngle + param->getValue() * (knobEndAngle - knobStartAngle), false);
            cell.removeFromLeft (10);
            g.setColour (P::inkMuted); g.setFont (F::garamond (14.0f));
            g.drawFittedText (param->getName (64), cell.removeFromTop (22).withTrimmedTop (4), juce::Justification::centredLeft, 1, 0.75f);
            g.setColour (P::ink); g.setFont (F::garamond (17.0f, 600));
            g.drawText (param->getCurrentValueAsText(), cell.removeFromTop (22), juce::Justification::centredLeft);
        }
}

void ChapterView::paintChapter (juce::Graphics& g)
{
    const auto& session = processor.getSession();

    eyebrow (g, fill (text ("eyebrow"), { { "numeral", chapter["numeral"].toString() } }), header);
    g.setColour (P::inkMuted); g.setFont (F::garamond (14.0f, 400, true));
    g.drawText (fill (text ("counter"), { { "current", juce::String (session.getCurrentIndex() + 1) },
                                          { "total", juce::String (session.getChapterIds().size()) } }),
                header, juce::Justification::centredRight);

    g.setColour (P::ink); g.setFont (F::cinzel (42.0f, 600, 0.03f));
    g.drawText (chapter["title"].toString(), titleArea, juce::Justification::centredLeft);
    if (! subtitleArea.isEmpty())
    {
        g.setColour (P::inkSoft); g.setFont (F::garamond (18.0f, 400, true));
        g.drawText (chapterText ("subtitle"), subtitleArea, juce::Justification::centredLeft);
    }
    Text::draw (g, chapterText ("intro"), introArea, F::garamond (introSize), juce::Colour (0xFF3A2E22),
                juce::Justification::topLeft, bodyLine);

    // Knob block: name, knob, range labels, value, target
    if (knob.isVisible())
    {
        eyebrow (g, knobName, knobBlock.withHeight (16), 12.0f, 0.22f, P::labelInk, juce::Justification::centred);
        auto* param = processor.getValueTree().getParameter (ruleFor (session.getCurrentChapterId())->paramId);
        int y = knob.getBottom() - 18 + 4;
        if (param != nullptr)
        {
            auto range = juce::Rectangle<int> (knobBlock.getCentreX() - 75, y, 150, 16);
            g.setColour (P::inkMuted); g.setFont (F::garamond (13.0f));
            g.drawText (param->getText (0.0f, 32), range, juce::Justification::centredLeft);
            g.drawText (param->getText (1.0f, 32), range, juce::Justification::centredRight);
            y += 16 + 4;
            g.setColour (P::ink); g.setFont (F::cinzel (26.0f, 600));
            g.drawText (param->getCurrentValueAsText(), knobBlock.withY (y).withHeight (32), juce::Justification::centred);
            y += 32 + 4;
        }
        Text::draw (g, targetCaption, knobBlock.withY (y).withHeight (60), F::garamond (14.0f, 400, true), P::inkSoft,
                    juce::Justification::centredTop, 1.3f);
    }
    else if (chapter["id"].toString() == "goal")
    {
        // Chapter I: a large dynamic range readout instead of a knob (UI-REFERENCE §6.3)
        const auto dr = processor.getMetrics().dynamicRangeDb;
        auto block = knobBlock.withTrimmedTop (24);
        if (std::isnan (dr))
            Text::draw (g, text ("dynamicRangeWaiting"), block, F::garamond (17.0f, 400, true), P::inkMuted, juce::Justification::centredTop, 1.35f);
        else
            Text::draw (g, fill (text ("dynamicRange"), { { "value", juce::String (dr, 1) } }), block,
                        F::cinzel (26.0f, 600), P::ink, juce::Justification::centredTop, 1.25f);
    }

    // Instructions
    auto area = instructions;
    auto section = [&] (const char* key, const juce::String& body)
    {
        if (body.isEmpty()) return;
        eyebrow (g, text (key), area.removeFromTop (17), 13.0f, 0.2f);
        area.removeFromTop (4);
        area.removeFromTop (Text::draw (g, body, area, F::garamond (instructionSize), P::ink, juce::Justification::topLeft, bodyLine) + 14);
    };
    section ("doThis", doThisText());
    section ("listenFor", chapterText ("listenFor"));
    section ("tryThis", chapterText ("tryThis"));
}

void ChapterView::paintReading (juce::Graphics& g, juce::Rectangle<int> area)
{
    using Phase = na::FeedbackTracker::Phase;
    using Tone = Feedback::Tone;

    const auto& fb = processor.getFeedback();
    const auto chapterId = chapter["id"].toString();
    const auto group = Feedback::messageGroupFor (chapterId);
    const auto meters = Feedback::metersFor (chapterId, goal, processor.getMetrics(), processor.getSettings());

    juce::String statusKey, messageKey;
    Tone tone = Tone::neutral;
    if (fb.phase == Phase::silence)                           statusKey = messageKey = "silence";
    else if (fb.phase == Phase::listening || fb.zone.empty()) statusKey = messageKey = "listening";
    else
    {
        statusKey = messageKey = group + "." + juce::String (fb.zone);
        tone = Feedback::toneFor (chapterId, fb.zone);
    }
    const auto toneColour = tone == Tone::good ? P::good : tone == Tone::tooMuch ? P::warnLow : tone == Tone::tooLittle ? P::warnHigh : P::inkMuted;
    const float alpha = fb.dimmed ? 0.45f : 1.0f;

    g.setColour (P::paperPanel);
    g.fillRoundedRectangle (area.toFloat(), 6.0f);
    g.setColour (P::rule);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 6.0f, 1.0f);

    auto inner = area.reduced (20, readingPad);
    auto head = inner.removeFromTop (18);
    eyebrow (g, content.getString ("book.reading.title"), head, 13.0f, 0.24f, P::inkSoft);
    const bool listening = processor.isListening();
    const auto liveText = content.getString (listening ? "book.reading.live" : "book.reading.idle");
    const auto liveFont = F::garamond (14.0f, 400, true);
    g.setColour (P::inkMuted); g.setFont (liveFont);
    g.drawText (liveText, head, juce::Justification::centredRight);
    g.setColour (listening ? P::warnLow : P::inkMuted.withAlpha (0.35f));
    g.fillEllipse ((float) head.getRight() - Text::width (liveText, liveFont) - 14.0f, (float) head.getCentreY() - 4.0f, 8.0f, 8.0f);
    inner.removeFromTop (readingGap);

    // Status: dot + label (in words, never colour alone) + coaching message
    auto status = inner.removeFromTop (inner.getHeight() - (meters.empty() ? 0 : readingGap + na::ui::meterHeight));
    g.setColour (toneColour.withMultipliedAlpha (alpha));
    g.fillEllipse ((float) status.getX(), (float) status.getY() + 7.0f, 12.0f, 12.0f);
    status.removeFromLeft (24);
    g.setFont (F::garamond (20.0f, 600));
    g.drawText (content.getString ("book.reading.status." + statusKey), status.removeFromTop (26), juce::Justification::centredLeft);
    Text::draw (g, content.getString ("messages." + messageKey), status, F::garamond (messageSize), P::ink.withMultipliedAlpha (alpha),
                juce::Justification::topLeft, 1.4f);

    // Meters, side by side
    if (meters.empty()) return;
    inner.removeFromTop (readingGap);
    const int gap = 24;
    const int w = (inner.getWidth() - gap * ((int) meters.size() - 1)) / (int) meters.size();
    for (size_t i = 0; i < meters.size(); ++i)
    {
        const auto& m = meters[i];
        MeterSpec spec;
        spec.label = content.getString (juce::String ("book.reading.meters.") + m.labelKey);
        const auto unit = content.getString (juce::String ("debug.units.") + m.unitKey);
        spec.valueText = std::isnan (m.value) ? content.getString ("debug.none")
                       : (m.decimals > 0 ? juce::String (m.value, m.decimals) : juce::String (juce::roundToInt (m.value))) + unit;
        if (! std::isnan (m.targetMin))
            spec.caption = fill (content.getString ("book.reading.meterTarget"),
                                 { { "min", na::Template::formatNumber (m.targetMin) }, { "max", na::Template::formatNumber (m.targetMax) }, { "unit", unit } });
        spec.value = m.value;
        spec.rangeMin = m.rangeMin;   spec.rangeMax = m.rangeMax;
        spec.targetMin = m.targetMin; spec.targetMax = m.targetMax;
        spec.fill = m.showsStatus ? toneColour : P::inkMuted;
        spec.alpha = alpha;
        drawMeter (g, { inner.getX() + (int) i * (w + gap), inner.getY(), w, na::ui::meterHeight }, spec);
    }
}

} // namespace na::vol4
