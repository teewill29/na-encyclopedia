// Unit tests for na::vol3::Equalizer. Plain C++, run by ctest.

#include "dsp/Equalizer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using namespace na::vol3;
using Shape = BandSettings::Shape;

namespace
{
int failures = 0;
std::string currentTest;
void check (bool ok, const std::string& what) { if (! ok) { ++failures; std::printf ("  FAIL [%s] %s\n", currentTest.c_str(), what.c_str()); } }
void checkNear (double a, double e, double tol, const std::string& what)
{
    char buf[200]; std::snprintf (buf, sizeof (buf), "%s: got %.3f, expected %.3f (+/- %.3f)", what.c_str(), a, e, tol);
    check (std::abs (a - e) <= tol, buf);
}
void test (const std::string& name, const std::function<void()>& body)
{
    currentTest = name; const auto before = failures; body();
    std::printf ("%s %s\n", failures == before ? "ok  " : "FAIL", name.c_str());
}

constexpr double sr = 48000.0, pi = 3.14159265358979323846;

/** A flat EQ: every band transparent, the low cut off. */
EqSettings flat()
{
    EqSettings s;
    s.bands[0] = { Shape::lowCut, 20.0f, 0.0f, 0.707f, false };
    s.bands[1] = { Shape::bell, 300.0f, 0.0f, 1.0f, true };
    s.bands[2] = { Shape::bell, 1000.0f, 0.0f, 1.0f, true };
    s.bands[3] = { Shape::bell, 3500.0f, 0.0f, 2.0f, true };
    s.bands[4] = { Shape::highShelf, 10000.0f, 0.0f, 0.707f, true };
    return s;
}

/** Gain (dB) the EQ applies to a sine at `hz`, measured after it settles. */
double gainAt (const EqSettings& s, double hz)
{
    Equalizer eq; eq.prepare (sr); eq.setSettings (s);
    const int n = (int) sr;
    std::vector<float> l ((size_t) n), r ((size_t) n);
    for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = (float) std::sin (2.0 * pi * hz * i / sr);
    float* ch[] = { l.data(), r.data() };
    for (int start = 0; start < n; start += 256)
    {
        float* c[] = { ch[0] + start, ch[1] + start };
        eq.setSettings (s);
        eq.process (c, 2, std::min (256, n - start));
    }
    double sumIn = 0, sumOut = 0;
    for (int i = n / 2; i < n; ++i) { const double x = std::sin (2.0 * pi * hz * i / sr); sumIn += x * x; sumOut += (double) l[(size_t) i] * l[(size_t) i]; }
    return 10.0 * std::log10 (sumOut / sumIn);
}
} // namespace

int main()
{
    test ("flat EQ changes nothing", [] {
        for (double hz : { 40.0, 300.0, 1000.0, 5000.0, 15000.0 })
            checkNear (gainAt (flat(), hz), 0.0, 0.01, std::to_string ((int) hz) + " Hz");
    });

    test ("bell: +6 dB at its frequency, nothing far away", [] {
        auto s = flat(); s.bands[2].gainDb = 6.0f;
        checkNear (gainAt (s, 1000.0), 6.0, 0.05, "1 kHz");
        checkNear (gainAt (s, 60.0), 0.0, 0.2, "60 Hz");
        checkNear (gainAt (s, 15000.0), 0.0, 0.3, "15 kHz");
        s.bands[2].gainDb = -6.0f;
        checkNear (gainAt (s, 1000.0), -6.0, 0.05, "cut -6 dB");
    });

    test ("width: a narrow bell leaves its neighbours alone", [] {
        auto wide = flat();   wide.bands[2].gainDb = 9.0f;   wide.bands[2].q = 0.5f;
        auto narrow = flat(); narrow.bands[2].gainDb = 9.0f; narrow.bands[2].q = 6.0f;
        check (gainAt (narrow, 1500.0) < gainAt (wide, 1500.0) - 3.0, "narrow affects 1.5 kHz much less");
    });

    test ("low cut: 12 dB/octave below the corner, flat above", [] {
        auto s = flat(); s.bands[0].on = true; s.bands[0].freqHz = 100.0f;
        checkNear (gainAt (s, 100.0), -3.01, 0.1, "-3 dB at the corner");
        checkNear (gainAt (s, 25.0), -24.0, 1.0, "2 octaves below");
        checkNear (gainAt (s, 1000.0), 0.0, 0.1, "1 kHz untouched");
    });

    test ("shelves: high shelf lifts the top, low shelf the bottom", [] {
        auto s = flat(); s.bands[4].gainDb = 6.0f;
        checkNear (gainAt (s, 18000.0), 6.0, 0.4, "high shelf at 18 kHz");
        checkNear (gainAt (s, 200.0), 0.0, 0.1, "high shelf at 200 Hz");
        auto t = flat(); t.bands[0] = { Shape::lowShelf, 150.0f, 6.0f, 0.707f, true };
        checkNear (gainAt (t, 40.0), 6.0, 0.4, "low shelf at 40 Hz");
        checkNear (gainAt (t, 3000.0), 0.0, 0.1, "low shelf at 3 kHz");
    });

    test ("the drawn curve matches what you hear", [] {
        auto s = flat();
        s.bands[0].on = true; s.bands[0].freqHz = 80.0f;
        s.bands[1].gainDb = -4.0f; s.bands[1].freqHz = 350.0f; s.bands[1].q = 1.4f;
        s.bands[3].gainDb = -3.0f; s.bands[3].freqHz = 3000.0f; s.bands[3].q = 4.0f;
        s.bands[4].gainDb = 3.0f;
        s.outputDb = 1.5f;
        for (double hz : { 50.0, 120.0, 350.0, 1000.0, 3000.0, 12000.0 })
            checkNear (Equalizer::responseDb (s, (float) hz, sr), gainAt (s, hz), 0.15, "curve at " + std::to_string ((int) hz) + " Hz");
    });

    test ("turning a band off glides to transparent", [] {
        auto s = flat(); s.bands[2].gainDb = 12.0f; s.bands[2].on = false;
        checkNear (gainAt (s, 1000.0), 0.0, 0.01, "off = flat");
    });

    test ("big setting jumps don't click", [] {
        Equalizer eq; eq.prepare (sr);
        auto s = flat(); s.bands[2].freqHz = 200.0f; s.bands[2].gainDb = -12.0f; s.bands[2].q = 4.0f;
        eq.setSettings (s);
        const int n = (int) sr;
        std::vector<float> l ((size_t) n), r ((size_t) n);
        for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = (float) (0.5 * std::sin (2.0 * pi * 440.0 * i / sr));
        // A click is a sudden spike in curvature (second difference), not just a level change:
        // a 440 Hz sine's own curvature is tiny, so compare the transition against it.
        auto curvature = [&] (int from, int to)
        {
            double worst = 0.0;
            for (int i = from; i < to; ++i)
                worst = std::max (worst, (double) std::abs (l[(size_t) i + 1] - 2.0f * l[(size_t) i] + l[(size_t) i - 1]));
            return worst;
        };
        const double steadyAfter = curvature (n - 4800, n - 1);           // settled at the new, louder setting
        const double transition  = curvature (n / 2 - 100, n / 2 + 4800);
        check (transition < steadyAfter * 1.5, "transition curvature " + std::to_string (transition)
               + " vs steady " + std::to_string (steadyAfter));
    });

    test ("extreme settings stay stable and finite", [] {
        for (double rate : { 44100.0, 96000.0 })
        {
            Equalizer eq; eq.prepare (rate);
            auto s = flat();
            s.bands[0] = { Shape::lowCut, 1000.0f, 0.0f, 0.707f, true };
            for (int b = 1; b < 4; ++b) { s.bands[(size_t) b].gainDb = 18.0f; s.bands[(size_t) b].q = 8.0f; s.bands[(size_t) b].freqHz = 16000.0f; }
            s.bands[4] = { Shape::bell, 20000.0f, -18.0f, 8.0f, true };
            eq.setSettings (s);
            std::vector<float> l (48000), r (48000);
            for (size_t i = 0; i < l.size(); ++i) l[i] = r[i] = (i % 2 ? 1.0f : -1.0f);   // nasty full-scale square
            float* c[] = { l.data(), r.data() };
            eq.process (c, 2, (int) l.size());
            bool finite = true;
            for (float v : l) finite &= std::isfinite (v);
            check (finite, "finite at " + std::to_string ((int) rate));
        }
    });

    test ("before the first setSettings, audio passes untouched", [] {
        Equalizer eq; eq.prepare (sr);
        float l[3] = { 0.1f, 0.2f, 0.3f }, r[3] = { 0.4f, 0.5f, 0.6f };
        float* c[] = { l, r };
        eq.process (c, 2, 3);
        check (std::abs (l[2] - 0.3f) < 1e-9f && std::abs (r[0] - 0.4f) < 1e-9f, "unchanged");
    });

    std::printf ("\n%s: %d failure(s)\n", failures == 0 ? "PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
