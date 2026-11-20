#include "FeedbackTracker.h"

namespace na
{

namespace
{
    bool listed (const std::string& list, const std::string& zone)
    {
        if (zone.empty()) return false;
        size_t pos = 0;
        while ((pos = list.find (zone, pos)) != std::string::npos)
        {
            const bool startOk = pos == 0 || list[pos - 1] == ' ';
            const bool endOk = pos + zone.size() == list.size() || list[pos + zone.size()] == ' ';
            if (startOk && endOk) return true;
            pos += zone.size();
        }
        return false;
    }
}

void FeedbackTracker::reset()
{
    display = {};
    candidate.clear();
    candidateSince = 0.0;
    dimUntil = -1.0;
    done = false;
}

void FeedbackTracker::update (double now, Phase phase, const std::string& zone, bool knobMoved,
                              const std::string& doneZones)
{
    if (knobMoved)
        dimUntil = now + dimSeconds;

    display.dimmed = now < dimUntil;
    if (display.dimmed)
    {
        // Hold the current message; the clocks restart when the dim ends.
        candidateSince = dimUntil;
        return;
    }

    display.phase = phase;
    if (phase != Phase::evaluating)
    {
        // Keep the last zone so returning audio doesn't flash an empty status,
        // but nothing counts toward "done" without audio.
        candidate.clear();
        return;
    }

    if (zone != candidate)
    {
        candidate = zone;
        candidateSince = now;
    }

    // The first reading shows straight away; later changes must hold 1.5 s.
    if (candidate != display.zone && (display.zone.empty() || now - candidateSince >= hysteresisSeconds))
        display.zone = candidate;

    if (display.zone == candidate && listed (doneZones, candidate) && now - candidateSince >= doneHoldSeconds)
        done = true;
}

} // namespace na
