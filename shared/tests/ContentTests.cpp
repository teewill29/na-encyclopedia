// Unit tests for shared/content (lookup, templating) and shared/chapters (Session).

#include "content/Content.h"
#include "content/Template.h"
#include "chapters/Session.h"

#include <cstdio>
#include <cstring>
#include <functional>
#include <string>

namespace
{
int failures = 0;
std::string currentTest;

void check (bool ok, const std::string& what)
{
    if (! ok) { ++failures; std::printf ("  FAIL [%s] %s\n", currentTest.c_str(), what.c_str()); }
}

void checkEq (const juce::String& actual, const juce::String& expected, const std::string& what)
{
    check (actual == expected, what + ": got \"" + actual.toStdString() + "\", expected \"" + expected.toStdString() + "\"");
}

void test (const std::string& name, const std::function<void()>& body)
{
    currentTest = name;
    const auto before = failures;
    body();
    std::printf ("%s %s\n", failures == before ? "ok  " : "FAIL", name.c_str());
}

const char* json = R"({
  "parameters": { "attack": { "name": "Attack" } },
  "chapters": [
    { "id": "goal", "numeral": "I" },
    { "id": "ratioThreshold", "numeral": "II" },
    { "id": "attack", "numeral": "III" },
    { "id": "blend", "numeral": "VI", "optional": true }
  ],
  "instruments": [
    { "id": "vox", "name": "Lead Vocal", "goals": [
      { "id": "punch", "name": "More Punch",
        "ratio": { "start": 4 }, "attackMs": { "start": 2.5, "min": 15, "max": 30 },
        "releaseMs": { "start": 250 }, "listenFor": { "attack": "T's and P's" } } ] },
    { "id": "drums", "name": "Drum Bus", "goals": [
      { "id": "smash", "name": "Parallel Smash", "extraChapters": [ "blend" ], "mixPct": { "start": 0 } } ] }
  ]
})";
} // namespace

int main()
{
    juce::MessageManager::getInstance(); // Session broadcasts changes asynchronously
    const auto content = na::Content::fromJson (json, std::strlen (json));

    test ("dotted lookup", [&] {
        checkEq (content.getString ("parameters.attack.name"), "Attack", "found");
        checkEq (content.getString ("parameters.nope.name", "fallback"), "fallback", "missing");
    });

    test ("instrument, goal and chapter lookup", [&] {
        checkEq (content.findInstrument ("vox")["name"].toString(), "Lead Vocal", "instrument");
        checkEq (content.findGoal ("drums", "smash")["name"].toString(), "Parallel Smash", "goal");
        check (! content.findGoal ("vox", "smash").isObject(), "goal of another instrument isn't found");
        checkEq (content.findChapter ("attack")["numeral"].toString(), "III", "chapter");
    });

    test ("chapter list: optional chapters only for goals that ask", [&] {
        checkEq (content.chapterIdsFor (content.findGoal ("vox", "punch")).joinIntoString (","), "goal,ratioThreshold,attack", "normal goal");
        checkEq (content.chapterIdsFor (content.findGoal ("drums", "smash")).joinIntoString (","), "goal,ratioThreshold,attack,blend", "parallel goal");
    });

    test ("templating against a goal", [&] {
        const auto lookup = na::Template::forGoal (content.findGoal ("vox", "punch"));
        checkEq (na::Template::fill ("Ratio {ratio.start}:1", lookup), "Ratio 4:1", "ratio.start");
        checkEq (na::Template::fill ("Start at {attack.start} ms", lookup), "Start at 2.5 ms", "attack -> attackMs");
        checkEq (na::Template::fill ("{release.start} ms", lookup), "250 ms", "release -> releaseMs");
        checkEq (na::Template::fill ("{goal.listenFor.attack}", lookup), "T's and P's", "goal.listenFor");
        checkEq (na::Template::fill ("{attackMs.min}-{attackMs.max}", lookup), "15-30", "direct keys");
        checkEq (na::Template::fill ("keep {unknown.thing} visible", lookup), "keep {unknown.thing} visible", "unknown left as-is");
        checkEq (na::Template::fill ("no braces", lookup), "no braces", "plain text");
    });

    test ("templating with fixed values, chained", [&] {
        const auto lookup = na::Template::chain (na::Template::forValues ({ { "numeral", "III" } }),
                                                 na::Template::forGoal (content.findGoal ("vox", "punch")));
        checkEq (na::Template::fill ("Chapter {numeral}, {ratio.start}:1", lookup), "Chapter III, 4:1", "both");
    });

    test ("number formatting", [] {
        checkEq (na::Template::formatNumber (4.0), "4", "whole");
        checkEq (na::Template::formatNumber (2.5), "2.5", "half");
        checkEq (na::Template::formatNumber (0.25), "0.25", "quarter");
        checkEq (na::Template::formatNumber (-60.0), "-60", "negative");
    });

    test ("session: setup, navigation and first visits", [&] {
        na::Session s (content);
        check (s.isOnTitlePage(), "starts on the Title Page");
        s.setup ("vox", "punch");
        check (s.isOnTitlePage(), "no chapter open yet");
        check (s.goTo ("goal"), "first visit to Chapter I");
        check (! s.isOnTitlePage(), "chapter open");
        check (s.next() && s.getCurrentChapterId() == "ratioThreshold", "next");
        check (s.next() && s.getCurrentChapterId() == "attack", "next again");
        check (! s.next(), "no chapter after the last");
        check (s.previous() && s.getCurrentChapterId() == "ratioThreshold", "previous");
        check (! s.goTo ("attack"), "second visit is not a first visit");
        check (! s.goTo ("blend"), "chapters outside this goal can't be opened");
        check (s.getCurrentIndex() == 2, "index");
    });

    test ("session: a new setup starts the book over", [&] {
        na::Session s (content);
        s.setup ("vox", "punch");
        s.goTo ("goal"); s.goTo ("attack"); s.setDone ("goal", true);
        s.setup ("drums", "smash");
        check (! s.isDone ("goal") && ! s.wasVisited ("attack"), "progress cleared");
        check (s.getChapterIds().size() == 4, "parallel chapter list");
    });

    test ("session: save and restore", [&] {
        na::Session a (content);
        a.setup ("drums", "smash");
        a.goTo ("goal"); a.goTo ("attack"); a.setDone ("goal", true);
        na::Session b (content);
        b.fromValueTree (a.toValueTree());
        checkEq (b.getInstrumentId(), "drums", "instrument");
        checkEq (b.getGoalId(), "smash", "goal");
        checkEq (b.getCurrentChapterId(), "attack", "chapter");
        check (b.isDone ("goal") && ! b.isDone ("attack"), "done");
        check (b.wasVisited ("attack") && ! b.wasVisited ("blend"), "visited");
        check (! b.isOnTitlePage(), "reopens on the chapter");
    });

    test ("session: unknown saved setup starts fresh", [&] {
        na::Session a (content);
        a.setup ("vox", "punch");
        a.goTo ("goal");
        auto tree = a.toValueTree();
        tree.setProperty ("goal", "gone", nullptr);
        na::Session b (content);
        b.fromValueTree (tree);
        check (b.isOnTitlePage() && ! b.hasSetup(), "back to the Title Page");
    });

    std::printf ("\n%s: %d failure(s)\n", failures == 0 ? "PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
