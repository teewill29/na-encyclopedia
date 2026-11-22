// Unit tests for na::dsp::ListeningTools (FEEDBACK-ENGINE.md §6).

#include "dsp/ListeningTools.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using na::dsp::ListeningTools;
using Mode = ListeningTools::Mode;

namespace
{
int failures = 0;
std::string currentTest;
void check (bool ok, const std::string& what) { if (! ok) { ++failures; std::printf ("  FAIL [%s] %s\n", currentTest.c_str(), what.c_str()); } }
void checkNear (double a, double e, double tol, const std::string& what)
{
    char buf[200]; std::snprintf (buf, sizeof (buf), "%s: got %.5f, expected %.5f", what.c_str(), a, e);
    check (std::abs (a - e) <= tol, buf);
}
void test (const std::string& name, const std::function<void()>& body)
{
    currentTest = name; const auto before = failures; body();
    std::printf ("%s %s\n", failures == before ? "ok  " : "FAIL", name.c_str());
}

constexpr double sr = 48000.0;

/** Runs n samples: processed = 0.5, dry = 1.0, removed factor = 0.25. Returns left output. */
std::vector<float> run (ListeningTools& t, int n)
{
    std::vector<float> out;
    float l[64], r[64], dl[64], dr[64], f[64];
    for (int done = 0; done < n; done += 64)
    {
        for (int i = 0; i < 64; ++i) { l[i] = r[i] = 0.5f; dl[i] = dr[i] = 1.0f; f[i] = 0.25f; }
        float* o[] = { l, r };
        const float* d[] = { dl, dr };
        t.process (o, d, f, 2, 64);
        out.insert (out.end(), l, l + 64);
    }
    return out;
}
} // namespace

int main()
{
    test ("normal passes the processed signal untouched", [] {
        ListeningTools t; t.prepare (sr);
        auto out = run (t, 4800);
        check (out.front() == 0.5f && out.back() == 0.5f, "exactly the processed signal");
    });

    test ("compare plays the dry input at the level-match gain", [] {
        ListeningTools t; t.prepare (sr);
        t.setCompareGainDb (-6.0f);
        t.reset();                      // start the gain glide at its target
        t.setMode (Mode::compare);
        auto out = run (t, 9600);
        checkNear (out.back(), std::pow (10.0, -6.0 / 20.0), 1e-4, "dry x -6 dB");
    });

    test ("hear what's removed plays dry x removed factor", [] {
        ListeningTools t; t.prepare (sr);
        t.setMode (Mode::removed);
        auto out = run (t, 4800);
        checkNear (out.back(), 0.25, 1e-6, "dry x 0.25");
    });

    test ("switching crossfades over 20 ms, no jumps", [] {
        ListeningTools t; t.prepare (sr);
        run (t, 960);
        t.setMode (Mode::removed);
        auto out = run (t, 4800);
        double worst = 0;
        for (size_t i = 1; i < out.size(); ++i) worst = std::max (worst, (double) std::abs (out[i] - out[i - 1]));
        check (worst < 0.001, "largest step " + std::to_string (worst));
        checkNear (out[959], 0.25, 1e-3, "arrived after 20 ms");
        checkNear (out[400], 0.5 + (0.25 - 0.5) * 401.0 / 960.0, 2e-3, "halfway through the fade");
    });

    test ("level-match gain glides instead of jumping", [] {
        ListeningTools t; t.prepare (sr);
        t.setMode (Mode::compare);
        run (t, 4800);
        t.setCompareGainDb (12.0f);
        auto out = run (t, 48000);
        double worst = 0;
        for (size_t i = 1; i < out.size(); ++i) worst = std::max (worst, (double) std::abs (out[i] - out[i - 1]));
        check (worst < 0.002, "largest step " + std::to_string (worst));
        checkNear (out.back(), std::pow (10.0, 12.0 / 20.0), 1e-3, "arrives at +12 dB");
    });

    test ("back to normal returns exactly the processed signal", [] {
        ListeningTools t; t.prepare (sr);
        t.setMode (Mode::compare);
        run (t, 4800);
        t.setMode (Mode::normal);
        auto out = run (t, 4800);
        check (out.back() == 0.5f, "exact once the fade is done");
    });

    std::printf ("\n%s: %d failure(s)\n", failures == 0 ? "PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
