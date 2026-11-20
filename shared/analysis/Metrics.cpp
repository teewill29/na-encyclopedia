#include "Metrics.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace na::analysis
{

namespace
{
    constexpr float nan = Metrics::none;
    constexpr int   grBins = 601;          // 0.0 … 60.0 dB in 0.1 dB steps
    constexpr float floorDb = -120.0f;

    inline float toDb (double gain) noexcept
    {
        return gain > 1.0e-6 ? (float) (20.0 * std::log10 (gain)) : floorDb;
    }

    inline float powerToDb (double meanSquare) noexcept
    {
        return meanSquare > 1.0e-12 ? (float) (10.0 * std::log10 (meanSquare)) : floorDb;
    }

    /** Linear-interpolated percentile of v[0..n), p in 0..1. Sorts v. */
    float percentile (float* v, size_t n, double p) noexcept
    {
        if (n == 0) return nan;
        std::sort (v, v + n);
        const double idx = p * (double) (n - 1);
        const auto lo = (size_t) std::floor (idx);
        const auto hi = std::min (lo + 1, n - 1);
        const double frac = idx - (double) lo;
        return (float) (v[lo] + (v[hi] - v[lo]) * frac);
    }
}

int MetricsEngine::msToSamples (double ms) const noexcept
{
    return std::max (1, (int) std::lround (ms * 0.001 * sampleRate));
}

int MetricsEngine::secondsToTicks (double s) const noexcept
{
    return std::max (1, (int) std::lround (s / tickSeconds));
}

void MetricsEngine::prepare (double newSampleRate)
{
    sampleRate  = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    tickLength  = std::max (1, (int) std::lround (sampleRate / 1000.0));
    tickSeconds = tickLength / sampleRate;

    kIn.prepare (sampleRate);
    kOut.prepare (sampleRate);

    envAttack  = std::exp (-1.0 / (onsetAttackMs * 0.001 * sampleRate));
    envRelease = std::exp (-1.0 / (onsetReleaseMs * 0.001 * sampleRate));
    onsetMinGap = msToSamples (onsetMinGapMs);
    riseWindowTicks = secondsToTicks (onsetRiseWindowMs * 0.001);

    size_t histLen = 1;
    while (histLen < (size_t) (0.3 * sampleRate)) histLen <<= 1;
    histIn.assign (histLen, 0.0f);
    histOut.assign (histLen, 0.0f);
    histGr.assign (histLen, 0.0f);
    histMask = histLen - 1;

    const auto windowTicks = (size_t) secondsToTicks (windowSeconds);
    gateRing.init ((size_t) secondsToTicks (gateRmsSeconds), 0.0);
    activeRing.init (windowTicks, 0);
    grBinRing.init (windowTicks, -1);
    grHistogram.assign (grBins, 0);
    kInRing.init ((size_t) secondsToTicks (shortTermSeconds), 0.0);
    kOutRing.init ((size_t) secondsToTicks (shortTermSeconds), 0.0);
    envMinRing.init ((size_t) riseWindowTicks, floorDb);

    hopTicks = secondsToTicks (hopSeconds);
    momentaryTicks = secondsToTicks (momentarySeconds);
    momentaryRing.init ((size_t) std::max (1, (int) std::lround (windowSeconds / hopSeconds)), nan);

    spanTicks = secondsToTicks (rippleSpanMs * 0.001);
    rippleRing.init ((size_t) std::max (1, (int) std::lround (windowSeconds * 1000.0 / rippleSpanMs)), nan);

    frontsRing.init (onsetsForMedian, nan);
    residualRing.init (onsetsForMedian, nan);

    scratch.assign (std::max ({ momentaryRing.size(), (size_t) onsetsForMedian, rippleRing.size() }), 0.0f);

    reset();
}

void MetricsEngine::reset()
{
    kIn.reset();
    kOut.reset();
    envelope = 0.0;
    position = 0;
    lastOnset = std::numeric_limits<int64_t>::min() / 2;

    std::fill (histIn.begin(), histIn.end(), 0.0f);
    std::fill (histOut.begin(), histOut.end(), 0.0f);
    std::fill (histGr.begin(), histGr.end(), 0.0f);

    tickCount = 0;
    tickSumSq = tickSumKIn = tickSumKOut = tickSumGr = 0.0;
    tickEnvMinDb = 0.0f;
    tickGrMin = tickGrMax = 0.0f;

    gateRing.init (gateRing.size(), 0.0);   gateSum = 0.0;
    activeRing.init (activeRing.size(), 0); activeCount = 0;
    grBinRing.init (grBinRing.size(), -1);
    std::fill (grHistogram.begin(), grHistogram.end(), 0);
    grHistogramCount = 0;
    kInRing.init (kInRing.size(), 0.0);
    kOutRing.init (kOutRing.size(), 0.0);
    envMinRing.init (envMinRing.size(), floorDb);
    hopCounter = 0;
    gateOpen = false;
    ticksSeen = 0;
    momentaryRing.init (momentaryRing.size(), nan);

    spanCounter = 0;
    spanAllActive = true;
    spanStart = 0;
    spanGrMin = spanGrMax = 0.0f;
    rippleRing.init (rippleRing.size(), nan);

    numPending = 0;
    frontsRing.init (frontsRing.size(), nan);
    residualRing.init (residualRing.size(), nan);

    metrics = {};
}

void MetricsEngine::process (const float* in, const float* out, const float* gr, int numSamples) noexcept
{
    if (histIn.empty())
        return; // not prepared

    for (int i = 0; i < numSamples; ++i)
        processSample (in[i], out[i], gr[i]);

    updateSummary();
}

void MetricsEngine::processSample (float in, float out, float gr) noexcept
{
    const auto h = (size_t) position & histMask;
    histIn[h] = in;
    histOut[h] = out;
    histGr[h] = gr;
    metrics.currentGrDb = gr;

    // Onset envelope follower on `in` (§2.4)
    const double mag = std::abs ((double) in);
    envelope = mag + (mag > envelope ? envAttack : envRelease) * (envelope - mag);
    const float envDb = toDb (envelope);

    float riseRef = tickCount == 0 ? envDb : tickEnvMinDb;
    for (size_t k = 0; k < envMinRing.size(); ++k)
        riseRef = std::min (riseRef, envMinRing.back (k));

    if (envDb - riseRef > (float) onsetRiseDb
        && envDb > (float) gateDb
        && position - lastOnset >= onsetMinGap)
        onOnset (position);

    // Finish any onsets whose body window has passed (§2.5)
    const int64_t needed = msToSamples (bodyEndMs);
    while (numPending > 0 && position - pending[0] >= needed)
    {
        completeFront (pending[0]);
        std::move (pending + 1, pending + numPending, pending);
        --numPending;
    }

    // Tick accumulators
    const double kiv = kIn.process (in), kov = kOut.process (out);
    if (tickCount == 0)
    {
        tickEnvMinDb = envDb;
        tickGrMin = tickGrMax = gr;
    }
    tickEnvMinDb = std::min (tickEnvMinDb, envDb);
    tickGrMin = std::min (tickGrMin, gr);
    tickGrMax = std::max (tickGrMax, gr);
    tickSumSq   += (double) in * in;
    tickSumKIn  += kiv * kiv;
    tickSumKOut += kov * kov;
    tickSumGr   += gr;

    ++position;
    if (++tickCount >= tickLength)
        finishTick();
}

void MetricsEngine::finishTick() noexcept
{
    const double n = tickCount;
    const double meanSq = tickSumSq / n;
    const float  meanGr = (float) (tickSumGr / n);
    ++ticksSeen;

    // §2.1 activity gate: 300 ms short-term RMS of `in`
    gateSum += meanSq - gateRing.push (meanSq);
    if (ticksSeen % 1000 == 0) // stop floating-point drift
    {
        gateSum = 0.0;
        for (double v : gateRing.data) gateSum += v;
    }
    gateOpen = powerToDb (std::max (0.0, gateSum) / (double) gateRing.size()) > (float) gateDb;

    activeCount += (gateOpen ? 1 : 0) - activeRing.push (gateOpen ? 1 : 0);

    // §2.3 GR distribution over active audio
    const int16_t bin = gateOpen ? (int16_t) std::clamp ((int) std::lround (meanGr * 10.0f), 0, grBins - 1) : (int16_t) -1;
    const int16_t oldBin = grBinRing.push (bin);
    if (oldBin >= 0) { --grHistogram[(size_t) oldBin]; --grHistogramCount; }
    if (bin >= 0)    { ++grHistogram[(size_t) bin];    ++grHistogramCount; }

    // Loudness energy per tick
    kInRing.push (tickSumKIn / n);
    kOutRing.push (tickSumKOut / n);

    // Onset rise reference: lowest envelope in each recent tick
    envMinRing.push (tickEnvMinDb);

    // §2.6 ripple, in 20 ms spans of sustained audio
    if (spanCounter == 0)
    {
        spanStart = position - tickCount;
        spanGrMin = tickGrMin;
        spanGrMax = tickGrMax;
        spanAllActive = true;
    }
    spanGrMin = std::min (spanGrMin, tickGrMin);
    spanGrMax = std::max (spanGrMax, tickGrMax);
    spanAllActive = spanAllActive && gateOpen;
    if (++spanCounter >= spanTicks)
    {
        const bool sustained = spanAllActive && lastOnset < spanStart - msToSamples (rippleQuietAfterOnsetMs);
        rippleRing.push (sustained ? spanGrMax - spanGrMin : nan);
        spanCounter = 0;
    }

    if (++hopCounter >= hopTicks)
    {
        finishHop();
        hopCounter = 0;
    }

    tickCount = 0;
    tickSumSq = tickSumKIn = tickSumKOut = tickSumGr = 0.0;
}

void MetricsEngine::finishHop() noexcept
{
    // §2.2 momentary loudness (400 ms) of `in`
    const auto mTicks = std::min ((size_t) momentaryTicks, kInRing.filled);
    double sum = 0.0;
    for (size_t k = 0; k < mTicks; ++k) sum += kInRing.back (k);
    const float momentary = mTicks > 0 ? (float) KWeighting::loudness (sum / (double) mTicks) : nan;
    metrics.momentaryInLufs = momentary;
    momentaryRing.push (gateOpen && mTicks == (size_t) momentaryTicks ? momentary : nan);

    // §2.7 short-term loudness (3 s), in and out
    double sumIn = 0.0, sumOut = 0.0;
    for (size_t k = 0; k < kInRing.filled; ++k) { sumIn += kInRing.back (k); sumOut += kOutRing.back (k); }
    if (kInRing.filled > 0)
    {
        metrics.loudnessInLufs  = (float) KWeighting::loudness (sumIn / (double) kInRing.filled);
        metrics.loudnessOutLufs = (float) KWeighting::loudness (sumOut / (double) kInRing.filled);
        metrics.levelDiffDb = metrics.loudnessInLufs > -70.0f ? metrics.loudnessOutLufs - metrics.loudnessInLufs : nan;
    }
}

void MetricsEngine::onOnset (int64_t at) noexcept
{
    lastOnset = at;
    ++metrics.onsetCount;

    // §2.6 residual GR: mean GR in the 5 ms just before the onset
    const int64_t end = at - msToSamples (onsetPreRollMs);
    const int64_t begin = end - msToSamples (residualWindowMs);
    if (begin >= 0)
    {
        double sum = 0.0;
        for (int64_t p = begin; p < end; ++p) sum += histGr[(size_t) p & histMask];
        residualRing.push ((float) (sum / (double) (end - begin)));
    }

    if (numPending < (int) std::size (pending))
        pending[numPending++] = at;
}

void MetricsEngine::completeFront (int64_t onset) noexcept
{
    // §2.5: punch = peak (first 10 ms) − body RMS (30-120 ms), on in and out
    const int64_t start = onset - msToSamples (onsetPreRollMs);
    const int64_t peakEnd = onset + msToSamples (peakWindowMs);
    const int64_t bodyBegin = onset + msToSamples (bodyStartMs);
    const int64_t bodyEnd = onset + msToSamples (bodyEndMs);
    if (start < 0) return;

    double peakIn = 0.0, peakOut = 0.0;
    for (int64_t p = start; p < peakEnd; ++p)
    {
        peakIn  = std::max (peakIn,  (double) std::abs (histIn[(size_t) p & histMask]));
        peakOut = std::max (peakOut, (double) std::abs (histOut[(size_t) p & histMask]));
    }
    double sqIn = 0.0, sqOut = 0.0;
    for (int64_t p = bodyBegin; p < bodyEnd; ++p)
    {
        const double a = histIn[(size_t) p & histMask], b = histOut[(size_t) p & histMask];
        sqIn += a * a;
        sqOut += b * b;
    }
    const double len = (double) (bodyEnd - bodyBegin);
    // Body RMS in dB = 10·log10(mean square).
    const float pIn  = toDb (peakIn)  - powerToDb (sqIn / len);
    const float pOut = toDb (peakOut) - powerToDb (sqOut / len);

    // Note for M5 tuning: a steady sine already measures 3.01 dB here (its own
    // peak-to-RMS), right at the 3 dB "no real hit" limit in the spec.
    if (pIn < (float) minPunchDb)
        return; // no real hit to protect

    metrics.lastPunchInDb = pIn;
    metrics.lastPunchOutDb = pOut;
    frontsRing.push (std::clamp (pOut / pIn * 100.0f, 0.0f, 110.0f));
}

void MetricsEngine::updateSummary() noexcept
{
    // §2.1 state
    const auto filled = activeRing.filled;
    metrics.activeSeconds = (float) (activeCount * tickSeconds);
    const double inactiveFraction = filled > 0 ? 1.0 - (double) activeCount / (double) filled : 1.0;
    if (filled == 0 || inactiveFraction > silenceFraction)
        metrics.state = Metrics::State::silence;
    else if (metrics.activeSeconds < minActiveSeconds)
        metrics.state = Metrics::State::listening;
    else
        metrics.state = Metrics::State::evaluating;

    auto collect = [this] (const auto& ring) -> size_t
    {
        size_t n = 0;
        for (size_t k = 0; k < ring.filled; ++k)
        {
            const float v = ring.back (k);
            if (! std::isnan (v)) scratch[n++] = v;
        }
        return n;
    };

    // §2.2 dynamic range
    {
        const auto n = collect (momentaryRing);
        if ((int) n >= minMomentaryValues)
        {
            const float p10 = percentile (scratch.data(), n, 0.10);
            const float p95 = percentile (scratch.data(), n, 0.95);
            metrics.dynamicRangeDb = p95 - p10;
        }
        else
            metrics.dynamicRangeDb = nan;
    }

    // §2.3 GR 90th percentile
    if (grHistogramCount > 0)
    {
        const int target = (int) std::ceil (0.9 * grHistogramCount);
        int cumulative = 0, bin = 0;
        for (; bin < grBins; ++bin)
            if ((cumulative += grHistogram[(size_t) bin]) >= target) break;
        metrics.grP90Db = (float) std::min (bin, grBins - 1) * 0.1f;
    }
    else
        metrics.grP90Db = nan;

    // §2.5 fronts kept, §2.6 residual GR: medians of the last 8 onsets
    {
        const auto n = collect (frontsRing);
        metrics.frontsKeptCount = (int) n;
        metrics.frontsKeptPct = n > 0 ? percentile (scratch.data(), n, 0.5) : nan;
    }
    {
        const auto n = collect (residualRing);
        metrics.residualGrDb = n > 0 ? percentile (scratch.data(), n, 0.5) : nan;
    }

    // §2.6 ripple: largest in the window
    {
        const auto n = collect (rippleRing);
        metrics.grRippleDb = n > 0 ? *std::max_element (scratch.data(), scratch.data() + n) : nan;
    }
}

} // namespace na::analysis
