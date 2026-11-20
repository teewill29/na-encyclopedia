// Unit tests for na::FeedbackTracker (FEEDBACK-ENGINE.md §3).

#include "chapters/FeedbackTracker.h"

#include <cstdio>
#include <functional>
#include <string>

using na::FeedbackTracker;
using Phase = FeedbackTracker::Phase;

namespace
{
int failures = 0;
std::string currentTest;

void check (bool ok, const std::string& what)
{
    if (! ok) { ++failures; std::printf ("  FAIL [%s] %s\n", currentTest.c_str(), what.c_str()); }
}

void test (const std::string& name, const std::function<void()>& body)
{
    currentTest = name;
    const auto before = failures;
    body();
    std::printf ("%s %s\n", failures == before ? "ok  " : "FAIL", name.c_str());
}

constexpr double tick = 1.0 / 30.0;

/** Runs the tracker at 30 Hz for `seconds` with a fixed reading. */
void run (FeedbackTracker& t, double& now, double seconds, Phase phase, const std::string& zone, bool moveKnobFirst = false)
{
    for (double end = now + seconds; now < end - 1e-9; now += tick)
    {
        t.update (now, phase, zone, moveKnobFirst);
        moveKnobFirst = false;
    }
}
} // namespace

int main()
{
    test ("phases pass straight through", [] {
        FeedbackTracker t; double now = 0;
        run (t, now, 0.5, Phase::silence, "");
        check (t.getDisplay().phase == Phase::silence, "silence");
        run (t, now, 0.5, Phase::listening, "");
        check (t.getDisplay().phase == Phase::listening, "listening");
    });

    test ("the first zone shows at once", [] {
        FeedbackTracker t; double now = 0;
        run (t, now, tick, Phase::evaluating, "low");
        check (t.getDisplay().zone == "low", "low shown");
    });

    test ("hysteresis: a new zone must hold 1.5 s", [] {
        FeedbackTracker t; double now = 0;
        run (t, now, 1.0, Phase::evaluating, "low");
        run (t, now, 1.4, Phase::evaluating, "onTarget");
        check (t.getDisplay().zone == "low", "still low after 1.4 s");
        run (t, now, 0.2, Phase::evaluating, "onTarget");
        check (t.getDisplay().zone == "onTarget", "onTarget after 1.6 s");
    });

    test ("flicker never reaches the screen", [] {
        FeedbackTracker t; double now = 0;
        run (t, now, 1.0, Phase::evaluating, "low");
        for (int i = 0; i < 10; ++i)
        {
            run (t, now, 1.0, Phase::evaluating, "high");
            run (t, now, 1.0, Phase::evaluating, "low");
        }
        check (t.getDisplay().zone == "low", "stays on low");
        check (! t.isDone(), "not done");
    });

    test ("done after onTarget holds 2 s, and it stays done", [] {
        FeedbackTracker t; double now = 0;
        run (t, now, 1.9, Phase::evaluating, "onTarget");
        check (! t.isDone(), "not yet at 1.9 s");
        run (t, now, 0.2, Phase::evaluating, "onTarget");
        check (t.isDone(), "done at 2.1 s");
        run (t, now, 3.0, Phase::evaluating, "high");
        check (t.isDone(), "still done after drifting off");
    });

    test ("other zones never count as done", [] {
        FeedbackTracker t; double now = 0;
        run (t, now, 10.0, Phase::evaluating, "high");
        check (! t.isDone(), "high for 10 s");
    });

    test ("custom done zones (Chapter I)", [] {
        FeedbackTracker t; double now = 0;
        for (int i = 0; i < 90; ++i, now += tick) t.update (now, Phase::evaluating, "notNeeded", false, "needed notNeeded skipped");
        check (t.isDone(), "notNeeded completes Chapter I");
        FeedbackTracker u; now = 0;
        for (int i = 0; i < 90; ++i, now += tick) u.update (now, Phase::evaluating, "need", false, "needed notNeeded");
        check (! u.isDone(), "partial names don't match");
    });

    test ("silence pauses the clock toward done", [] {
        FeedbackTracker t; double now = 0;
        run (t, now, 1.5, Phase::evaluating, "onTarget");
        run (t, now, 3.0, Phase::silence, "");
        check (! t.isDone(), "not done through silence");
        check (t.getDisplay().zone == "onTarget", "last zone kept for when audio returns");
        run (t, now, 1.5, Phase::evaluating, "onTarget");
        check (! t.isDone(), "clock restarted");
        run (t, now, 0.6, Phase::evaluating, "onTarget");
        check (t.isDone(), "done after 2 s of audio");
    });

    test ("knob moved: message held and dimmed for 300 ms, then resumes", [] {
        FeedbackTracker t; double now = 0;
        run (t, now, 1.0, Phase::evaluating, "low");
        run (t, now, 0.2, Phase::evaluating, "onTarget", true);
        check (t.getDisplay().dimmed, "dimmed");
        check (t.getDisplay().zone == "low", "message held, not wiped");
        run (t, now, 0.2, Phase::evaluating, "onTarget");
        check (! t.getDisplay().dimmed, "undimmed after 300 ms");
        run (t, now, 1.6, Phase::evaluating, "onTarget");
        check (t.getDisplay().zone == "onTarget", "then the new zone shows after hysteresis");
    });

    test ("turning the knob continuously keeps it dimmed and not done", [] {
        FeedbackTracker t; double now = 0;
        for (int i = 0; i < 150; ++i, now += tick) t.update (now, Phase::evaluating, "onTarget", true);
        check (t.getDisplay().dimmed && ! t.isDone(), "dimmed, not done");
    });

    test ("reset starts over", [] {
        FeedbackTracker t; double now = 0;
        run (t, now, 3.0, Phase::evaluating, "onTarget");
        t.reset();
        check (! t.isDone() && t.getDisplay().zone.empty() && t.getDisplay().phase == Phase::silence, "fresh");
    });

    std::printf ("\n%s: %d failure(s)\n", failures == 0 ? "PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
