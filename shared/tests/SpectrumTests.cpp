// Unit tests for shared/analysis/SpectrumAnalyser (docs/EQ.md §4), fed with
// synthetic signals whose answers are known.

#include "analysis/SpectrumAnalyser.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <vector>

using namespace na::analysis;
using Stream = SpectrumAnalyser::Stream;

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

void checkRange (double actual, double lo, double hi, const std::string& what)
{
    char buf[256];
    std::snprintf (buf, sizeof (buf), "%s: got %.3f, expected %.3f to %.3f", what.c_str(), actual, lo, hi);
    check (actual >= lo && actual <= hi, buf);
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

std::vector<float> sine (double hz, double amp, double seconds)
{
    std::vector<float> v ((size_t) (seconds * sr));
    for (size_t i = 0; i < v.size(); ++i) v[i] = (float) (amp * std::sin (2.0 * pi * hz * (double) i / sr));
    return v;
}

std::vector<float> white (double rms, double seconds, unsigned seed = 1)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> n (0.0f, (float) rms);
    std::vector<float> v ((size_t) (seconds * sr));
    for (auto& x : v) x = n (rng);
    return v;
}

/** Pink noise (Paul Kellet's filter): equal power per octave. */
std::vector<float> pink (double gain, double seconds, unsigned seed = 2)
{
    auto w = white (1.0, seconds, seed);
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    for (auto& x : w)
    {
        const double in = x;
        b0 = 0.99886 * b0 + in * 0.0555179;  b1 = 0.99332 * b1 + in * 0.0750759;
        b2 = 0.96900 * b2 + in * 0.1538520;  b3 = 0.86650 * b3 + in * 0.3104856;
        b4 = 0.55000 * b4 + in * 0.5329522;  b5 = -0.7616 * b5 - in * 0.0168980;
        x = (float) (gain * (b0 + b1 + b2 + b3 + b4 + b5 + b6 + in * 0.5362) * 0.11);
        b6 = in * 0.115926;
    }
    return w;
}

/** RBJ peaking EQ, applied in place. */
void peak (std::vector<float>& v, double hz, double gainDb, double q)
{
    const double A = std::pow (10.0, gainDb / 40.0), w0 = 2.0 * pi * hz / sr, alpha = std::sin (w0) / (2.0 * q);
    const double a0 = 1 + alpha / A;
    const double b0 = (1 + alpha * A) / a0, b1 = -2 * std::cos (w0) / a0, b2 = (1 - alpha * A) / a0;
    const double a1 = -2 * std::cos (w0) / a0, a2 = (1 - alpha / A) / a0;
    double z1 = 0, z2 = 0;
    for (auto& x : v)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        x = (float) y;
    }
}

/** RBJ 2nd-order high-pass, applied in place. */
void highPass (std::vector<float>& v, double hz)
{
    const double w0 = 2.0 * pi * hz / sr, alpha = std::sin (w0) / (2.0 * 0.7071), c = std::cos (w0);
    const double a0 = 1 + alpha;
    const double b0 = (1 + c) / 2 / a0, b1 = -(1 + c) / a0, b2 = b0, a1 = -2 * c / a0, a2 = (1 - alpha) / a0;
    double z1 = 0, z2 = 0;
    for (auto& x : v)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        x = (float) y;
    }
}

void feed (SpectrumAnalyser& a, const std::vector<float>& in, const std::vector<float>& out, int block = 512)
{
    for (size_t i = 0; i < in.size(); i += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, in.size() - i);
        a.process (in.data() + i, out.data() + i, n);
    }
}

void feed (SpectrumAnalyser& a, const std::vector<float>& x) { feed (a, x, x); }

int bandFor (float hz)
{
    int best = 0;
    for (int b = 0; b < SpectrumAnalyser::numBands; ++b)
        if (std::abs (std::log2 (SpectrumAnalyser::bandCentreHz (b) / hz)) < std::abs (std::log2 (SpectrumAnalyser::bandCentreHz (best) / hz)))
            best = b;
    return best;
}
} // namespace

int main()
{
    test ("bands: 1/6 octave from 10 Hz, 20 Hz and 20 kHz are on the grid", []
    {
        checkNear (SpectrumAnalyser::bandCentreHz (0), 10.0, 1e-3, "first band");
        checkNear (SpectrumAnalyser::bandCentreHz (6), 20.0, 1e-3, "band 6");
        checkNear (SpectrumAnalyser::bandCentreHz (66), 20480.0, 0.5, "last band");
    });

    test ("no reading before a second of audio, and none from silence", []
    {
        SpectrumAnalyser a; a.prepare (sr);
        check (! a.hasReading(), "fresh analyser has no reading");
        check (std::isnan (a.energyDb (Stream::in, 20, 20000)), "energy is NaN without a reading");
        feed (a, std::vector<float> ((size_t) (5 * sr), 0.0f));
        check (! a.hasReading(), "silence gives no reading");
        feed (a, sine (1000, 0.5, 0.5));
        check (! a.hasReading(), "half a second isn't enough");
        feed (a, sine (1000, 0.5, 1.0));
        check (a.hasReading(), "a reading after 1.5 s");
    });

    test ("a sine reads its mean-square power in its band (0.5 amplitude = -9 dB)", []
    {
        SpectrumAnalyser a; a.prepare (sr);
        feed (a, sine (1000, 0.5, 4.0));
        const float expected = (float) (10.0 * std::log10 (0.5 * 0.5 / 2.0));
        checkNear (a.energyDb (Stream::in, 10, 22000), expected, 0.1, "total energy");
        checkNear (a.energyDb (Stream::in, 800, 1250), expected, 0.2, "energy around 1 kHz");
        checkNear (a.levelsDb (Stream::in)[(size_t) bandFor (1000)], expected, 1.0, "the 1 kHz band itself");
        check (a.levelsDb (Stream::in)[(size_t) bandFor (4000)] < expected - 60.0f, "nothing at 4 kHz");
    });

    test ("in and out are measured separately", []
    {
        SpectrumAnalyser a; a.prepare (sr);
        const auto in = pink (1.0, 4.0);
        auto out = in;
        for (auto& x : out) x *= 0.5f;
        feed (a, in, out);
        checkNear (a.energyDb (Stream::in, 20, 20000) - a.energyDb (Stream::out, 20, 20000), 6.02, 0.05, "out is 6 dB lower");
    });

    test ("pink noise reads flat: equal power per band from 50 Hz to 15 kHz", []
    {
        SpectrumAnalyser a; a.prepare (sr);
        feed (a, pink (1.0, 4.0));
        const auto& db = a.levelsDb (Stream::in);
        const float ref = db[(size_t) bandFor (1000)];
        for (float hz : { 50.0f, 100.0f, 300.0f, 3000.0f, 10000.0f, 15000.0f })
            checkNear (db[(size_t) bandFor (hz)], ref, 2.0, "band at " + std::to_string ((int) hz) + " Hz vs 1 kHz");
    });

    test ("build-up: flat spectra read ~0 dB", []
    {
        for (int kind = 0; kind < 2; ++kind)
        {
            SpectrumAnalyser a; a.prepare (sr);
            feed (a, kind == 0 ? pink (1.0, 4.0) : white (0.1, 4.0));
            const std::string name = kind == 0 ? "pink" : "white";
            checkNear (a.buildUpDb (Stream::in, 200, 500), 0.0, 0.75, name + ", mud range");
            checkNear (a.buildUpDb (Stream::in, 2500, 5000), 0.0, 0.75, name + ", harsh range");
        }
    });

    test ("build-up: a +6 dB bump at 300 Hz shows in the mud range, not the harsh range", []
    {
        SpectrumAnalyser a; a.prepare (sr);
        auto x = pink (1.0, 4.0);
        peak (x, 300.0, 6.0, 1.4);
        feed (a, x);
        checkRange (a.buildUpDb (Stream::in, 200, 500), 2.5, 6.0, "mud range");
        checkNear (a.buildUpDb (Stream::in, 2500, 5000), 0.0, 0.75, "harsh range");
    });

    test ("build-up: cutting the bump on `out` brings it back to ~0", []
    {
        SpectrumAnalyser a; a.prepare (sr);
        auto in = pink (1.0, 4.0);
        peak (in, 300.0, 6.0, 1.4);
        auto out = in;
        peak (out, 300.0, -6.0, 1.4);
        feed (a, in, out);
        check (a.buildUpDb (Stream::in, 200, 500) > 2.5f, "in still has the bump");
        checkNear (a.buildUpDb (Stream::out, 200, 500), 0.0, 0.75, "out is flat again");
    });

    test ("typical spectra: interpolated in log frequency, held flat past the ends", []
    {
        const auto t = SpectrumAnalyser::typicalFromPoints ({ { 100.0f, -10.0f }, { 400.0f, 0.0f }, { 1600.0f, -20.0f } });
        checkNear (t[(size_t) bandFor (100)], -10.0, 0.15, "at a point" /* band centres are ~1.6 % off round numbers */);
        checkNear (t[(size_t) bandFor (200)], -5.0, 0.15, "an octave between two points" /* band centres are ~1.6 % off round numbers */);
        checkNear (t[(size_t) bandFor (800)], -10.0, 0.15, "falling side" /* band centres are ~1.6 % off round numbers */);
        checkNear (t[(size_t) bandFor (20)], -10.0, 0.01, "below the first point");
        checkNear (t[(size_t) bandFor (10000)], -20.0, 0.01, "above the last point");
    });

    // A "voice-shaped" source: little below 150 Hz, a natural body around 350 Hz.
    auto voiceLike = [] (unsigned seed)
    {
        auto x = pink (1.0, 4.0, seed);
        highPass (x, 150.0);
        highPass (x, 150.0);
        peak (x, 350.0, 8.0, 0.7);
        return x;
    };

    test ("build-up against a typical spectrum: a normal track reads ~0, even though its body stands out", [&]
    {
        SpectrumAnalyser reference; reference.prepare (sr);
        feed (reference, voiceLike (11));
        SpectrumAnalyser::Reference ref { reference.levelsDb (Stream::in), 100.0f, 2000.0f };

        SpectrumAnalyser a; a.prepare (sr);
        feed (a, voiceLike (12));
        check (a.buildUpDb (Stream::in, 200, 500) > 3.0f, "against flat, the natural body reads high: " + std::to_string (a.buildUpDb (Stream::in, 200, 500)));
        checkNear (a.buildUpDb (Stream::in, 200, 500, ref), 0.0, 0.75, "against the typical spectrum");
    });

    test ("build-up against a typical spectrum: extra mud shows, overall tilt doesn't", [&]
    {
        SpectrumAnalyser reference; reference.prepare (sr);
        feed (reference, voiceLike (11));
        SpectrumAnalyser::Reference ref { reference.levelsDb (Stream::in), 100.0f, 2000.0f };

        SpectrumAnalyser muddy; muddy.prepare (sr);
        auto x = voiceLike (13);
        peak (x, 300.0, 6.0, 1.4);
        feed (muddy, x);
        checkRange (muddy.buildUpDb (Stream::in, 200, 500, ref), 2.5, 6.0, "a +6 dB bump at 300 Hz");

        SpectrumAnalyser dark; dark.prepare (sr);
        auto y = voiceLike (14);
        peak (y, 4000.0, -9.0, 0.3);              // a broad, darker tilt across the whole span
        feed (dark, y);
        checkNear (dark.buildUpDb (Stream::in, 200, 500, ref), 0.0, 1.0, "a darker recording isn't 'muddy'");
    });

    test ("rumble: low on high-passed noise, high with a 30 Hz hum, and a low cut removes it", []
    {
        SpectrumAnalyser clean; clean.prepare (sr);
        auto x = pink (1.0, 4.0);
        highPass (x, 100.0);
        feed (clean, x);
        check (clean.rumbleDb (Stream::in, 80) < -15.0f, "high-passed pink: rumble below 80 Hz under -15 dB");

        SpectrumAnalyser hum; hum.prepare (sr);
        auto y = x;
        const auto s = sine (30.0, 0.3, 4.0);
        for (size_t i = 0; i < y.size(); ++i) y[i] += s[i];
        auto cut = y;
        highPass (cut, 120.0);
        highPass (cut, 120.0);
        feed (hum, y, cut);
        check (hum.rumbleDb (Stream::in, 80) > -6.0f, "with a 30 Hz hum: rumble above -6 dB");
        check (hum.rumbleDb (Stream::out, 80) < hum.rumbleDb (Stream::in, 80) - 20.0f, "the low cut takes it down by 20 dB or more");
    });

    test ("the reading holds when the track stops, and averages the last 3 s", []
    {
        SpectrumAnalyser a; a.prepare (sr);
        feed (a, sine (1000, 0.5, 4.0));
        const float before = a.energyDb (Stream::in, 10, 22000);
        feed (a, std::vector<float> ((size_t) (3 * sr), 0.0f));
        checkNear (a.energyDb (Stream::in, 10, 22000), before, 0.3, "silence leaves the reading alone (only the fade-out frames count)");
        checkNear (a.getActiveSeconds(), SpectrumAnalyser::averageSeconds, 0.1, "the average spans ~3 s");

        feed (a, sine (1000, 0.25, 4.0));   // 6 dB quieter, longer than the average
        checkNear (a.energyDb (Stream::in, 10, 22000), before - 6.02, 0.1, "after 4 s the old level has left the average");
    });

    test ("other sample rates: 44.1 and 96 kHz read the same", []
    {
        for (double rate : { 44100.0, 96000.0 })
        {
            SpectrumAnalyser a; a.prepare (rate);
            std::vector<float> v ((size_t) (4 * rate));
            for (size_t i = 0; i < v.size(); ++i) v[i] = (float) (0.5 * std::sin (2.0 * pi * 1000.0 * (double) i / rate));
            feed (a, v);
            checkNear (a.energyDb (Stream::in, 800, 1250), 10.0 * std::log10 (0.125), 0.2, "1 kHz sine at " + std::to_string ((int) rate));
        }
    });

    std::printf ("\n%s: %d failure(s)\n", failures == 0 ? "PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
