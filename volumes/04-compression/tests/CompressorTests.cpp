// Unit tests for na::vol4::Compressor. Plain C++, run by ctest (CI fails if any check fails).

#include "dsp/Compressor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using na::vol4::Compressor;
using na::vol4::CompressorSettings;

namespace
{
int failures = 0;
std::string currentTest;

void check (bool ok, const std::string& what)
{
    if (! ok)
    {
        ++failures;
        std::printf ("  FAIL [%s] %s\n", currentTest.c_str(), what.c_str());
    }
}

void checkNear (double actual, double expected, double tolerance, const std::string& what)
{
    char buf[256];
    std::snprintf (buf, sizeof (buf), "%s: got %.4f, expected %.4f (+/- %.4f)", what.c_str(), actual, expected, tolerance);
    check (std::abs (actual - expected) <= tolerance, buf);
}

void test (const std::string& name, const std::function<void()>& body)
{
    currentTest = name;
    const auto before = failures;
    body();
    std::printf ("%s %s\n", failures == before ? "ok  " : "FAIL", name.c_str());
}

constexpr double sr = 48000.0;
double toDb (double g) { return 20.0 * std::log10 (std::max (g, 1.0e-12)); }

/** Runs `numSamples` of a constant-magnitude signal (alternating sign, so |x| is
    constant like a full-scale square wave) through the compressor, 64-sample blocks.
    Returns the per-sample gain applied (out / in) and per-sample GR. */
struct Run { std::vector<float> gain, gr, left, right; };

Run runConstant (Compressor& c, const CompressorSettings& s, float amplitudeL, float amplitudeR, int numSamples, int channels = 2)
{
    Run r;
    std::vector<float> l (64), rr (64), gr (64);
    for (int start = 0; start < numSamples; start += 64)
    {
        const int n = std::min (64, numSamples - start);
        for (int i = 0; i < n; ++i)
        {
            const float sign = ((start + i) % 2 == 0) ? 1.0f : -1.0f;
            l[(size_t) i] = sign * amplitudeL;
            rr[(size_t) i] = sign * amplitudeR;
        }
        float* chans[] = { l.data(), rr.data() };
        c.setSettings (s);
        c.process (chans, channels, n, gr.data());
        for (int i = 0; i < n; ++i)
        {
            const float sign = ((start + i) % 2 == 0) ? 1.0f : -1.0f;
            r.gain.push_back (amplitudeL != 0.0f ? l[(size_t) i] / (sign * amplitudeL) : 1.0f);
            r.gr.push_back (gr[(size_t) i]);
            r.left.push_back (l[(size_t) i]);
            r.right.push_back (rr[(size_t) i]);
        }
    }
    return r;
}

int samplesUntil (const std::vector<float>& v, int from, const std::function<bool (float)>& pred)
{
    for (size_t i = (size_t) from; i < v.size(); ++i)
        if (pred (v[i])) return (int) i - from;
    return -1;
}
} // namespace

int main()
{
    test ("static curve: below threshold does nothing", [] {
        checkNear (Compressor::gainReductionDb (-30.0f, -20.0f, 4.0f), 0.0, 1e-6, "10 dB under");
        checkNear (Compressor::gainReductionDb (-23.1f, -20.0f, 4.0f), 0.0, 1e-6, "just under the knee");
    });

    test ("static curve: above the knee follows the ratio", [] {
        // 10 dB over at 4:1 -> output 2.5 dB over -> 7.5 dB of reduction
        checkNear (Compressor::gainReductionDb (-10.0f, -20.0f, 4.0f), 7.5, 1e-4, "4:1, 10 dB over");
        checkNear (Compressor::gainReductionDb (0.0f, -20.0f, 2.0f), 10.0, 1e-4, "2:1, 20 dB over");
        checkNear (Compressor::gainReductionDb (0.0f, -40.0f, 20.0f), 38.0, 1e-4, "20:1, 40 dB over");
    });

    test ("static curve: soft knee is smooth and continuous", [] {
        const float t = -20.0f, r = 4.0f, w = Compressor::kneeDb;
        checkNear (Compressor::gainReductionDb (t - w / 2, t, r), 0.0, 1e-5, "knee start");
        checkNear (Compressor::gainReductionDb (t + w / 2, t, r), (1 - 1 / r) * w / 2, 1e-4, "knee end");
        checkNear (Compressor::gainReductionDb (t, t, r), (1 - 1 / r) * w / 8, 1e-4, "at threshold");
        float prev = 0.0f;
        bool monotonic = true, smooth = true;
        for (float x = t - 10; x <= t + 10; x += 0.01f)
        {
            const auto g = Compressor::gainReductionDb (x, t, r);
            monotonic &= g >= prev - 1e-6f;
            smooth &= (g - prev) <= 0.01f * (1 - 1 / r) + 1e-4f;
            prev = g;
        }
        check (monotonic, "never decreases as level rises");
        check (smooth, "no jumps");
    });

    test ("ratio 1:1 never compresses", [] {
        Compressor c; c.prepare (sr);
        CompressorSettings s; s.thresholdDb = -60; s.ratio = 1; s.makeupDb = 0;
        auto r = runConstant (c, s, 0.9f, 0.9f, 4800);
        float maxGr = *std::max_element (r.gr.begin(), r.gr.end());
        checkNear (maxGr, 0.0, 1e-6, "max gain reduction");
        checkNear (r.gain.back(), 1.0, 1e-6, "gain");
    });

    test ("steady signal settles on the static curve", [] {
        Compressor c; c.prepare (sr);
        CompressorSettings s; s.thresholdDb = -20; s.ratio = 4; s.attackMs = 5; s.releaseMs = 50;
        auto r = runConstant (c, s, 0.5f, 0.5f, (int) sr); // 0.5 = -6.02 dBFS
        const double expected = Compressor::gainReductionDb ((float) toDb (0.5), -20.0f, 4.0f);
        checkNear (r.gr.back(), expected, 0.01, "settled GR (dB)");
        checkNear (toDb (r.gain.back()), -expected, 0.01, "applied gain (dB)");
    });

    test ("attack time: ~63% of the reduction after attackMs", [] {
        for (float attackMs : { 1.0f, 10.0f, 50.0f })
        {
            Compressor c; c.prepare (sr);
            CompressorSettings s; s.thresholdDb = -30; s.ratio = 10; s.attackMs = attackMs; s.releaseMs = 500;
            auto r = runConstant (c, s, 1.0f, 1.0f, (int) sr);
            const float final = r.gr.back();
            const int reached = samplesUntil (r.gr, 0, [&] (float g) { return g >= 0.632f * final; });
            checkNear (reached / sr * 1000.0, attackMs, attackMs * 0.05 + 0.05, "attack " + std::to_string (attackMs) + " ms");
        }
    });

    test ("release time: ~63% recovered after releaseMs", [] {
        for (float releaseMs : { 20.0f, 100.0f, 500.0f })
        {
            Compressor c; c.prepare (sr);
            CompressorSettings s; s.thresholdDb = -30; s.ratio = 10; s.attackMs = 1; s.releaseMs = releaseMs;
            auto loud = runConstant (c, s, 1.0f, 1.0f, (int) sr / 2);
            const float start = loud.gr.back();
            auto quiet = runConstant (c, s, 0.001f, 0.001f, (int) sr * 3); // -60 dBFS, under threshold
            const int reached = samplesUntil (quiet.gr, 0, [&] (float g) { return g <= start * (1.0f - 0.632f); });
            checkNear (reached / sr * 1000.0, releaseMs, releaseMs * 0.05, "release " + std::to_string (releaseMs) + " ms");
        }
    });

    test ("makeup gain", [] {
        Compressor c; c.prepare (sr);
        CompressorSettings s; s.ratio = 1; s.makeupDb = 6;
        auto r = runConstant (c, s, 0.25f, 0.25f, 4800);
        checkNear (toDb (r.gain.back()), 6.0, 0.01, "+6 dB");
    });

    test ("mix 0% is exactly the dry signal", [] {
        Compressor c; c.prepare (sr);
        CompressorSettings s; s.thresholdDb = -60; s.ratio = 20; s.attackMs = 0.1f; s.makeupDb = 12; s.mixPct = 0;
        auto r = runConstant (c, s, 0.8f, 0.8f, 4800);
        bool exact = true;
        for (float g : r.gain) exact &= g == 1.0f;
        check (exact, "every sample unchanged");
    });

    test ("mix 50% blends dry and compressed", [] {
        Compressor c; c.prepare (sr);
        CompressorSettings s; s.thresholdDb = -20; s.ratio = 4; s.attackMs = 1; s.mixPct = 50;
        auto r = runConstant (c, s, 1.0f, 1.0f, (int) sr / 2);
        const double wet = std::pow (10.0, -Compressor::gainReductionDb (0.0f, -20.0f, 4.0f) / 20.0);
        checkNear (r.gain.back(), 0.5 + 0.5 * wet, 1e-3, "gain");
    });

    test ("stereo link: both sides get the same gain", [] {
        Compressor c; c.prepare (sr);
        CompressorSettings s; s.thresholdDb = -20; s.ratio = 4; s.attackMs = 1;
        auto r = runConstant (c, s, 1.0f, 0.01f, (int) sr / 2);
        checkNear (r.right.back() / (r.right.back() > 0 ? 0.01 : -0.01), r.gain.back(), 1e-4, "quiet side follows loud side");
    });

    test ("mono works", [] {
        Compressor c; c.prepare (sr);
        CompressorSettings s; s.thresholdDb = -20; s.ratio = 4; s.attackMs = 1;
        auto r = runConstant (c, s, 1.0f, 0.0f, (int) sr / 2, 1);
        checkNear (r.gr.back(), Compressor::gainReductionDb (0.0f, -20.0f, 4.0f), 0.01, "GR");
    });

    test ("automation: a big threshold jump doesn't click", [] {
        Compressor c; c.prepare (sr);
        CompressorSettings s; s.thresholdDb = 0; s.ratio = 20; s.attackMs = 0.1f; s.releaseMs = 10;
        runConstant (c, s, 1.0f, 1.0f, 4800);
        s.thresholdDb = -60; // slam it
        auto r = runConstant (c, s, 1.0f, 1.0f, 4800);
        double worst = 0;
        for (size_t i = 1; i < r.gain.size(); ++i)
            worst = std::max (worst, std::abs (toDb (r.gain[i]) - toDb (r.gain[i - 1])));
        check (worst < 0.2, "largest per-sample gain step " + std::to_string (worst) + " dB should be < 0.2 dB");
    });

    test ("automation: makeup and mix jumps are ramped", [] {
        Compressor c; c.prepare (sr);
        CompressorSettings s; s.ratio = 1; s.makeupDb = -12; s.mixPct = 0;
        runConstant (c, s, 0.5f, 0.5f, 4800);
        s.makeupDb = 24; s.mixPct = 100;
        auto r = runConstant (c, s, 0.5f, 0.5f, 4800);
        double worst = 0;
        for (size_t i = 1; i < r.gain.size(); ++i)
            worst = std::max (worst, (double) std::abs (r.gain[i] - r.gain[i - 1]));
        check (worst < 0.05, "largest per-sample gain step " + std::to_string (worst));
        checkNear (toDb (r.gain.back()), 24.0, 0.01, "arrives at +24 dB");
    });

    test ("silence, huge input and tiny input stay finite", [] {
        Compressor c; c.prepare (sr);
        CompressorSettings s; s.thresholdDb = -60; s.ratio = 20; s.attackMs = 0.1f; s.makeupDb = 24;
        bool finite = true;
        for (float amp : { 0.0f, 1.0e-30f, 1.0e-8f, 1000.0f, 0.0f })
        {
            auto r = runConstant (c, s, amp, amp, 4800);
            for (float v : r.left) finite &= std::isfinite (v);
            for (float v : r.gr)   finite &= std::isfinite (v);
        }
        check (finite, "no NaN or infinity");
    });

    test ("before the first setSettings, audio passes untouched", [] {
        Compressor c; c.prepare (sr);
        float l[4] = { 0.5f, -0.5f, 0.25f, 1.0f }, r[4] = { 0.1f, 0.2f, 0.3f, 0.4f };
        float* chans[] = { l, r };
        c.process (chans, 2, 4);
        check (l[3] == 1.0f && r[2] == 0.3f, "unchanged");
    });

    test ("other sample rates: timing scales correctly", [] {
        for (double rate : { 44100.0, 96000.0, 192000.0 })
        {
            Compressor c; c.prepare (rate);
            CompressorSettings s; s.thresholdDb = -30; s.ratio = 10; s.attackMs = 10; s.releaseMs = 500;
            auto r = runConstant (c, s, 1.0f, 1.0f, (int) rate / 2);
            const int reached = samplesUntil (r.gr, 0, [&] (float g) { return g >= 0.632f * r.gr.back(); });
            checkNear (reached / rate * 1000.0, 10.0, 0.6, "attack at " + std::to_string ((int) rate));
        }
    });

    std::printf ("\n%s: %d failure(s)\n", failures == 0 ? "PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
