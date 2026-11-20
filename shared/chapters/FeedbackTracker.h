#pragma once

#include <string>

namespace na
{

/** The per-chapter feedback state machine (FEEDBACK-ENGINE.md §3).

        silence ──audio──▶ listening ──2 s active──▶ evaluating ─▶ zone

    - Hysteresis: a new zone must hold for 1.5 s before it is shown, so the
      status doesn't flicker while the user turns a knob.
    - Done: the "done" zone (normally onTarget) must hold for 2 s.
    - Knob moved: keep the current message, dim it for 300 ms, then resume.

    Zones are plain names ("low", "onTarget", "tooSlow", ...). Plain C++,
    no JUCE; time is passed in, in seconds, so it is easy to test.
*/
class FeedbackTracker
{
public:
    static constexpr double hysteresisSeconds = 1.5;
    static constexpr double doneHoldSeconds   = 2.0;
    static constexpr double dimSeconds        = 0.3;

    enum class Phase { silence, listening, evaluating };

    struct Display
    {
        Phase phase = Phase::silence;
        std::string zone;    // shown zone while evaluating ("" until one has held 1.5 s)
        bool dimmed = false; // knob just moved
    };

    /** Starts over (new chapter or new setup). */
    void reset();

    /** Feed the latest reading. `zone` is ignored unless phase == evaluating.
        `doneZones` lists the zones that count toward the check mark, separated by spaces. */
    void update (double now, Phase phase, const std::string& zone, bool knobMoved,
                 const std::string& doneZones = "onTarget");

    const Display& getDisplay() const noexcept { return display; }

    /** True once a done zone has held for 2 s since the last reset. Stays true. */
    bool isDone() const noexcept { return done; }

private:
    Display display;
    std::string candidate;
    double candidateSince = 0.0, dimUntil = -1.0;
    bool done = false;
};

} // namespace na
