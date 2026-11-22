// Unit tests for shared/analysis: TapFifo, K-weighting and every metric in
// docs/FEEDBACK-ENGINE.md §2, fed with synthetic signals whose answers are known.

#include "analysis/Metrics.h"
#include "analysis/TapFifo.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using namespace na::analysis;

namespace
{
int failures = 0;
std::string currentTest;

void check (bool ok, const std::string& what)
{
    if (! ok) { ++failures; std::printf ("  FAIL [%s] %s\n", currentTest.c_str(), what.c_str()); }
}

void checkNear (double actual, double expected, double tol, const std::string& what)
{
    char buf[256];
    std::snprintf (buf, sizeof (buf), "%s: got %.3f, expected %.3f (+/- %.3f)", what.c_str(), actual, expected, tol);
    check (std::abs (actual - expected) <= tol, buf);
}

void test (const std::string& name, const std::function<void()>& body)
{
    currentTest = name;
    const auto before = failures;
    body();
    std::printf ("%s %s\n", failures == before ? "ok  " : "FAIL", name.c_str());
}

constexpr double sr = 48000.0;
constexpr double pi = 3.14159265358979323846;

/** A signal generator: f(sampleIndex) -> {in, out, gr}. Fed in 512-sample calls. */
struct Frame { float in, out, gr; float target = 0.0f; };

void feed (MetricsEngine& m, double seconds, const std::function<Frame (int64_t)>& f, int64_t& t)
{
    std::vector<float> in (512), out (512), gr (512), target (512);
    auto total = (int64_t) (seconds * sr);
    while (total > 0)
    {
        const int n = (int) std::min<int64_t> (512, total);
        for (int i = 0; i < n; ++i) { auto fr = f (t++); in[(size_t) i] = fr.in; out[(size_t) i] = fr.out; gr[(size_t) i] = fr.gr; target[(size_t) i] = fr.target; }
        m.process (in.data(), out.data(), gr.data(), target.data(), n);
        total -= n;
    }
}

float sine (int64_t t, double hz, double amp) { return (float) (amp * std::sin (2.0 * pi * hz * (double) t / sr)); }

/** A "note" every `period` seconds: sharp 2 ms attack to `peak`, then a body at `body` amplitude, 200 Hz. */
float note (int64_t t, double period, double peak, double body)
{
    const double pos = std::fmod ((double) t / sr, period);
    double env;
    if (pos < 0.002)      env = peak * pos / 0.002;
    else if (pos < 0.012) env = peak + (body - peak) * (pos - 0.002) / 0.010;
    else if (pos < period * 0.8) env = body;
    else env = 0.0;
    return (float) (env * std::sin (2.0 * pi * 200.0 * (double) t / sr));
}
} // namespace

int main()
{
    test ("TapFifo keeps streams aligned and in order", [] {
        TapFifo<3> fifo (1000);
        check (fifo.capacity() == 1024, "capacity rounds up to a power of two");
        float a[300], b[300], c[300];
        for (int round = 0; round < 10; ++round)
        {
            for (int i = 0; i < 300; ++i) { a[i] = (float) (round * 300 + i); b[i] = -a[i]; c[i] = a[i] * 2; }
            const float* src[] = { a, b, c };
            check (fifo.push (src, 300), "push fits");
            float x[300], y[300], z[300];
            float* dst[] = { x, y, z };
            const auto n = fifo.pop (dst, 300);
            check (n == 300, "pop returns all");
            bool ok = true;
            for (int i = 0; i < 300; ++i) ok &= x[i] == a[i] && y[i] == -a[i] && z[i] == 2 * a[i];
            check (ok, "data intact across wrap-around");
        }
    });

    test ("TapFifo drops (never blocks) when full", [] {
        TapFifo<1> fifo (256);
        float a[200] = {};
        const float* src[] = { a };
        check (fifo.push (src, 200), "first fits");
        check (! fifo.push (src, 200), "second is refused");
        check (fifo.getDroppedBlocks() == 1, "drop counted");
    });

    test ("K-weighting: 997 Hz full-scale sine reads -3.01 LUFS", [] {
        for (double rate : { 44100.0, 48000.0, 96000.0 })
        {
            KWeighting k; k.prepare (rate);
            double sum = 0; int n = 0;
            for (int i = 0; i < (int) rate * 2; ++i)
            {
                const double y = k.process (std::sin (2.0 * pi * 997.0 * i / rate));
                if (i > (int) rate / 2) { sum += y * y; ++n; }
            }
            checkNear (KWeighting::loudness (sum / n), -3.01, 0.05, "at " + std::to_string ((int) rate));
        }
    });

    test ("silence reads as silence", [] {
        MetricsEngine m; m.prepare (sr); int64_t t = 0;
        feed (m, 3.0, [] (int64_t) { return Frame { 0, 0, 0 }; }, t);
        check (m.get().state == Metrics::State::silence, "state");
        check (std::isnan (m.get().grP90Db), "no GR stat");
        check (m.get().onsetCount == 0, "no onsets");
    });

    test ("audio below the -50 dBFS gate counts as silence", [] {
        MetricsEngine m; m.prepare (sr); int64_t t = 0;
        feed (m, 3.0, [] (int64_t i) { auto s = sine (i, 440, 0.001); return Frame { s, s, 0 }; }, t); // -60 dBFS
        check (m.get().state == Metrics::State::silence, "state");
    });

    test ("listening for the first 2 s of audio, then evaluating", [] {
        MetricsEngine m; m.prepare (sr); int64_t t = 0;
        auto tone = [] (int64_t i) { auto s = sine (i, 440, 0.3); return Frame { s, s, 0 }; };
        feed (m, 1.0, tone, t);
        check (m.get().state == Metrics::State::listening, "after 1 s");
        feed (m, 1.5, tone, t);
        check (m.get().state == Metrics::State::evaluating, "after 2.5 s");
    });

    test ("dynamic range: steady tone ~0 dB, loud/quiet sections ~20 dB", [] {
        {
            MetricsEngine m; m.prepare (sr); int64_t t = 0;
            feed (m, 4.0, [] (int64_t i) { auto s = sine (i, 1000, 0.3); return Frame { s, s, 0 }; }, t);
            checkNear (m.get().dynamicRangeDb, 0.0, 0.3, "steady");
        }
        {
            MetricsEngine m; m.prepare (sr); int64_t t = 0;
            // 1 s at -6 dBFS, 1 s at -26 dBFS, repeating
            feed (m, 6.0, [] (int64_t i) { const bool loud = (i / (int64_t) sr) % 2 == 0;
                                           auto s = sine (i, 1000, loud ? 0.5 : 0.05); return Frame { s, s, 0 }; }, t);
            checkNear (m.get().dynamicRangeDb, 20.0, 1.5, "20 dB swings");
        }
    });

    test ("GR 90th percentile over active audio", [] {
        MetricsEngine m; m.prepare (sr); int64_t t = 0;
        // GR is 2 dB 80% of the time, 8 dB 20% of the time -> p90 = 8
        feed (m, 4.0, [] (int64_t i) { auto s = sine (i, 300, 0.3);
                                       const bool hi = (i % 4800) < 960; return Frame { s, s, hi ? 8.0f : 2.0f }; }, t);
        checkNear (m.get().grP90Db, 8.0, 0.11, "p90");
        // GR during silence is ignored
        MetricsEngine q; q.prepare (sr); int64_t u = 0;
        feed (q, 2.0, [] (int64_t i) { auto s = sine (i, 300, 0.3); return Frame { s, s, 3.0f }; }, u);
        // After the 300 ms gate has closed, GR (e.g. a release tail) no longer counts.
        feed (q, 0.5, [] (int64_t) { return Frame { 0, 0, 0.0f }; }, u);
        feed (q, 1.0, [] (int64_t) { return Frame { 0, 0, 20.0f }; }, u);
        checkNear (q.get().grP90Db, 3.0, 0.11, "silent GR ignored");
    });

    test ("onsets: one per note, none doubled, none in steady tone", [] {
        MetricsEngine m; m.prepare (sr); int64_t t = 0;
        feed (m, 4.0, [] (int64_t i) { auto s = note (i, 0.25, 0.8, 0.3); return Frame { s, s, 0 }; }, t);
        checkNear (m.get().onsetCount, 16, 1, "16 notes in 4 s");

        MetricsEngine q; q.prepare (sr); int64_t u = 0;
        feed (q, 0.5, [] (int64_t) { return Frame { 0, 0, 0 }; }, u);
        feed (q, 3.0, [] (int64_t i) { auto s = sine (i, 300, 0.3); return Frame { s, s, 0 }; }, u);
        check (q.get().onsetCount == 1, "a held tone is a single onset (got " + std::to_string (q.get().onsetCount) + ")");
    });

    test ("fronts kept: how much of the instant-attack reduction the front escaped", [] {
        auto inFront = [] (int64_t i) { return std::fmod ((double) i / sr, 0.25) < 0.012; };
        {
            MetricsEngine m; m.prepare (sr); int64_t t = 0;
            // Slow attack: an instant attack would take 6 dB off the front, none is applied
            feed (m, 3.0, [&] (int64_t i) { auto s = note (i, 0.25, 0.8, 0.3); return Frame { s, s, 0.0f, inFront (i) ? 6.0f : 2.0f }; }, t);
            checkNear (m.get().frontsKeptPct, 100.0, 0.5, "front untouched");
            check (m.get().frontsKeptCount == 8, "uses the last 8 onsets");
            checkNear (m.get().lastFrontTargetGrDb, 6.0, 0.01, "would-be reduction reported");
        }
        {
            MetricsEngine m; m.prepare (sr); int64_t t = 0;
            // Fast attack: 4.5 of the 6 dB is applied to the front -> 25% kept
            feed (m, 3.0, [&] (int64_t i) { auto s = note (i, 0.25, 0.8, 0.3);
                return Frame { s, s, inFront (i) ? 4.5f : 0.0f, inFront (i) ? 6.0f : 0.0f }; }, t);
            checkNear (m.get().frontsKeptPct, 25.0, 0.5, "front mostly caught");
            checkNear (m.get().lastFrontGrDb, 4.5, 0.01, "applied reduction reported");
        }
        {
            MetricsEngine m; m.prepare (sr); int64_t t = 0;
            // 2 dB left over from the last note doesn't count against attack:
            // (4.5 - 2) of (6 - 2) dB added -> 37.5% kept
            feed (m, 3.0, [&] (int64_t i) { auto s = note (i, 0.25, 0.8, 0.3);
                return Frame { s, s, inFront (i) ? 4.5f : 2.0f, inFront (i) ? 6.0f : 2.0f }; }, t);
            checkNear (m.get().frontsKeptPct, 37.5, 0.5, "leftover reduction excluded");
        }
        {
            MetricsEngine m; m.prepare (sr); int64_t t = 0;
            // Fronts below the threshold (would-be < 1 dB) say nothing about attack
            feed (m, 3.0, [] (int64_t i) { auto s = note (i, 0.25, 0.8, 0.3); return Frame { s, s, 0.0f, 0.5f }; }, t);
            check (std::isnan (m.get().frontsKeptPct) && m.get().onsetCount > 0, "skipped, though onsets were found");
        }
    });

    test ("residual GR: what's left just before each note", [] {
        MetricsEngine m; m.prepare (sr); int64_t t = 0;
        // GR is 2 dB in the 20 ms before each note, 6 dB elsewhere
        feed (m, 3.0, [] (int64_t i) {
            const double pos = std::fmod ((double) i / sr, 0.25);
            return Frame { note (i, 0.25, 0.8, 0.3), note (i, 0.25, 0.8, 0.3), pos > 0.23 ? 2.0f : 6.0f }; }, t);
        checkNear (m.get().residualGrDb, 2.0, 0.01, "median residual");
    });

    test ("GR ripple in sustained audio", [] {
        MetricsEngine m; m.prepare (sr); int64_t t = 0;
        // GR wobbles 4 dB peak-to-peak at 50 Hz (fast release pumping on a held note)
        feed (m, 4.0, [] (int64_t i) { auto s = sine (i, 100, 0.4);
            return Frame { s, s, (float) (5.0 + 2.0 * std::sin (2.0 * pi * 50.0 * (double) i / sr)) }; }, t);
        checkNear (m.get().grRippleDb, 4.0, 0.1, "ripple");

        MetricsEngine q; q.prepare (sr); int64_t u = 0;
        feed (q, 4.0, [] (int64_t i) { auto s = sine (i, 100, 0.4); return Frame { s, s, 5.0f }; }, u);
        checkNear (q.get().grRippleDb, 0.0, 0.01, "steady GR");
    });

    test ("level match: out vs in short-term loudness", [] {
        MetricsEngine m; m.prepare (sr); int64_t t = 0;
        feed (m, 4.0, [] (int64_t i) { auto s = sine (i, 1000, 0.5); return Frame { s, s * 0.5f, 6.0f }; }, t);
        checkNear (m.get().levelDiffDb, -6.02, 0.05, "half the level");
        feed (m, 4.0, [] (int64_t i) { auto s = sine (i, 1000, 0.5); return Frame { s, s, 0.0f }; }, t);
        checkNear (m.get().levelDiffDb, 0.0, 0.05, "matched");
    });

    test ("gain staging: peaks, average and crest of a steady sine", [] {
        MetricsEngine m; m.prepare (sr); int64_t t = 0;
        // in: 0.5 amplitude sine (-6.02 dBFS peak, -9.03 dBFS RMS); out: half of it
        feed (m, 4.0, [] (int64_t i) { auto s = sine (i, 220, 0.5); return Frame { s, s * 0.5f, 0 }; }, t);
        checkNear (m.get().inPeakDb, -6.02, 0.05, "in peak");
        checkNear (m.get().inAverageDb, -9.03, 0.05, "in average");
        checkNear (m.get().outPeakDb, -12.04, 0.05, "out peak");
        checkNear (m.get().outAverageDb, -15.05, 0.05, "out average");
        checkNear (m.get().crestDb, 3.01, 0.05, "crest of a sine");
    });

    test ("gain staging: one stray spike doesn't set the peak level", [] {
        MetricsEngine m; m.prepare (sr); int64_t t = 0;
        // 0.25 sine (-12 dBFS) with a single full-scale click 2 s in
        feed (m, 4.0, [] (int64_t i) { float s = i == (int64_t) (2 * sr) ? 1.0f : sine (i, 220, 0.25); return Frame { s, s, 0 }; }, t);
        checkNear (m.get().inPeakDb, -12.04, 0.1, "95th percentile ignores the click");
        checkNear (m.get().maxOutPeakDb, 0.0, 0.01, "but the hold catches it");
    });

    test ("gain staging: the average ignores silence; the hold resets", [] {
        MetricsEngine m; m.prepare (sr); int64_t t = 0;
        // 1 s on, 1 s silence, repeating: the average is the level while playing
        feed (m, 6.0, [] (int64_t i) { const bool on = (i / (int64_t) sr) % 2 == 0;
            auto s = on ? sine (i, 220, 0.5) : 0.0f; return Frame { s, s, 0 }; }, t);
        checkNear (m.get().inAverageDb, -9.03, 0.6, "average while playing");
        check (m.get().holdActiveSeconds > 2.0f, "active time counted");
        m.resetPeakHold();
        check (std::isnan (m.get().maxOutPeakDb) && m.get().holdActiveSeconds == 0.0f, "hold reset");
        feed (m, 1.0, [] (int64_t i) { auto s = sine (i, 220, 0.1); return Frame { s, s, 0 }; }, t);
        checkNear (m.get().maxOutPeakDb, -20.0, 0.05, "hold restarts from the new audio");
    });

    test ("reset clears everything", [] {
        MetricsEngine m; m.prepare (sr); int64_t t = 0;
        feed (m, 3.0, [] (int64_t i) { auto s = note (i, 0.25, 0.8, 0.2); return Frame { s, s, 4.0f }; }, t);
        m.reset();
        check (m.get().onsetCount == 0 && std::isnan (m.get().frontsKeptPct) && m.get().state == Metrics::State::silence, "fresh");
    });

    std::printf ("\n%s: %d failure(s)\n", failures == 0 ? "PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
