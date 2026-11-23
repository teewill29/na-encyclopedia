#include "SpectrumAnalyser.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace na::analysis
{

namespace
{
    constexpr double pi = 3.14159265358979323846;

    float toDb (double power) noexcept
    {
        return power > 0.0 ? std::max (SpectrumAnalyser::floorDb, (float) (10.0 * std::log10 (power)))
                           : SpectrumAnalyser::floorDb;
    }
}

float SpectrumAnalyser::bandCentreHz (int band) noexcept
{
    return lowestHz * std::pow (2.0f, (float) band / (float) bandsPerOctave);
}

void SpectrumAnalyser::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    const double frameSeconds = hopSize / sampleRate;
    maxFrames = std::max (1, (int) std::lround (averageSeconds / frameSeconds));
    minFrames = std::max (1, (int) std::lround (minSeconds / frameSeconds));

    histIn.assign (fftSize, 0.0f);
    histOut.assign (fftSize, 0.0f);
    frame.assign (fftSize, 0.0f);

    // Periodic Hann window
    window.resize (fftSize);
    windowPower = 0.0;
    for (int i = 0; i < fftSize; ++i)
    {
        window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * pi * i / fftSize));
        windowPower += (double) window[(size_t) i] * window[(size_t) i];
    }

    buffer.assign (fftSize, {});
    twiddles.resize (fftSize / 2);
    for (int i = 0; i < fftSize / 2; ++i)
        twiddles[(size_t) i] = std::polar (1.0f, (float) (-2.0 * pi * i / fftSize));
    bitReverse.resize (fftSize);
    for (int i = 0; i < fftSize; ++i)
    {
        int r = 0;
        for (int b = 0; b < fftOrder; ++b)
            r |= ((i >> b) & 1) << (fftOrder - 1 - b);
        bitReverse[(size_t) i] = r;
    }

    // Each band takes the share of every FFT bin that overlaps it (bins are
    // treated as rectangles one bin wide). Narrow low bands take a fraction of a bin.
    const double binHz = sampleRate / fftSize;
    bandStart.assign (numBands + 1, 0);
    weightBin.clear();
    weightValue.clear();
    for (int b = 0; b < numBands; ++b)
    {
        bandStart[(size_t) b] = (int) weightBin.size();
        const double centre = bandCentreHz (b);
        const double lo = centre * std::pow (2.0, -0.5 / bandsPerOctave);
        const double hi = centre * std::pow (2.0,  0.5 / bandsPerOctave);
        for (int k = 1; k < fftSize / 2; ++k)
        {
            const double binLo = (k - 0.5) * binHz, binHi = (k + 0.5) * binHz;
            const double overlap = std::min (hi, binHi) - std::max (lo, binLo);
            if (overlap > 0.0)
            {
                weightBin.push_back (k);
                weightValue.push_back ((float) (overlap / binHz));
            }
        }
    }
    bandStart[(size_t) numBands] = (int) weightBin.size();

    ringIn.assign ((size_t) maxFrames * numBands, 0.0f);
    ringOut.assign ((size_t) maxFrames * numBands, 0.0f);
    sumIn.assign (numBands, 0.0);
    sumOut.assign (numBands, 0.0);
    inDb.assign (numBands, floorDb);
    outDb.assign (numBands, floorDb);
    reset();
}

void SpectrumAnalyser::reset() noexcept
{
    std::fill (histIn.begin(), histIn.end(), 0.0f);
    std::fill (histOut.begin(), histOut.end(), 0.0f);
    writePos = sinceFrame = filled = 0;
    ringHead = framesInAverage = 0;
    std::fill (sumIn.begin(), sumIn.end(), 0.0);
    std::fill (sumOut.begin(), sumOut.end(), 0.0);
    std::fill (inDb.begin(), inDb.end(), floorDb);
    std::fill (outDb.begin(), outDb.end(), floorDb);
    ++version;
}

void SpectrumAnalyser::process (const float* in, const float* out, int numSamples) noexcept
{
    if (histIn.empty()) return;
    for (int i = 0; i < numSamples; ++i)
    {
        histIn[(size_t) writePos] = in[i];
        histOut[(size_t) writePos] = out[i];
        writePos = (writePos + 1) % fftSize;
        filled = std::min (filled + 1, fftSize);
        if (++sinceFrame >= hopSize && filled == fftSize)
        {
            sinceFrame = 0;
            analyseFrame();
        }
    }
}

void SpectrumAnalyser::analyseFrame() noexcept
{
    // Gate on the input's level over the frame: silence doesn't enter the average.
    double meanSquare = 0.0;
    for (const auto v : histIn) meanSquare += (double) v * v;
    meanSquare /= fftSize;
    if (meanSquare < std::pow (10.0, gateDb / 10.0))
        return;

    auto unroll = [this] (const std::vector<float>& hist)
    {
        for (int i = 0; i < fftSize; ++i)
            frame[(size_t) i] = hist[(size_t) ((writePos + i) % fftSize)];
    };
    unroll (histIn);
    bandPowers (frame.data(), &ringIn[(size_t) ringHead * numBands]);
    unroll (histOut);
    bandPowers (frame.data(), &ringOut[(size_t) ringHead * numBands]);

    ringHead = (ringHead + 1) % maxFrames;
    framesInAverage = std::min (framesInAverage + 1, maxFrames);

    // Mean power per band over the frames in the average (recomputed, so no drift)
    for (int b = 0; b < numBands; ++b)
    {
        double a = 0.0, c = 0.0;
        for (int f = 0; f < framesInAverage; ++f)
        {
            a += ringIn[(size_t) f * numBands + (size_t) b];
            c += ringOut[(size_t) f * numBands + (size_t) b];
        }
        sumIn[(size_t) b]  = a / framesInAverage;
        sumOut[(size_t) b] = c / framesInAverage;
        inDb[(size_t) b]   = toDb (sumIn[(size_t) b]);
        outDb[(size_t) b]  = toDb (sumOut[(size_t) b]);
    }
    ++version;
}

void SpectrumAnalyser::bandPowers (const float* x, float* dest) noexcept
{
    for (int i = 0; i < fftSize; ++i)
        buffer[(size_t) bitReverse[(size_t) i]] = { x[i] * window[(size_t) i], 0.0f };
    fft (buffer.data());

    // One-sided power, scaled so the bins add up to the signal's mean square
    const double scale = 2.0 / (fftSize * windowPower);
    for (int b = 0; b < numBands; ++b)
    {
        double p = 0.0;
        for (int w = bandStart[(size_t) b]; w < bandStart[(size_t) b + 1]; ++w)
            p += weightValue[(size_t) w] * std::norm (buffer[(size_t) weightBin[(size_t) w]]);
        dest[b] = (float) (p * scale);
    }
}

void SpectrumAnalyser::fft (std::complex<float>* data) const noexcept
{
    // Iterative radix-2; the input is already in bit-reversed order.
    for (int size = 2; size <= fftSize; size <<= 1)
    {
        const int half = size / 2, step = fftSize / size;
        for (int start = 0; start < fftSize; start += size)
            for (int k = 0; k < half; ++k)
            {
                const auto t = twiddles[(size_t) (k * step)] * data[start + k + half];
                data[start + k + half] = data[start + k] - t;
                data[start + k] += t;
            }
    }
}

float SpectrumAnalyser::energyDb (Stream s, float loHz, float hiHz) const noexcept
{
    if (! hasReading()) return std::numeric_limits<float>::quiet_NaN();
    const auto& power = s == Stream::in ? sumIn : sumOut;
    double total = 0.0;
    for (int b = 0; b < numBands; ++b)
        if (const auto f = bandCentreHz (b); f >= loHz && f <= hiHz)
            total += power[(size_t) b];
    return toDb (total);
}

std::vector<float> SpectrumAnalyser::typicalFromPoints (const std::vector<std::pair<float, float>>& points)
{
    std::vector<float> db ((size_t) numBands, 0.0f);
    if (points.empty()) return db;
    for (int b = 0; b < numBands; ++b)
    {
        const float x = std::log2 (bandCentreHz (b));
        if (x <= std::log2 (points.front().first)) { db[(size_t) b] = points.front().second; continue; }
        if (x >= std::log2 (points.back().first))  { db[(size_t) b] = points.back().second;  continue; }
        for (size_t i = 1; i < points.size(); ++i)
        {
            const float x0 = std::log2 (points[i - 1].first), x1 = std::log2 (points[i].first);
            if (x <= x1)
            {
                const float t = x1 > x0 ? (x - x0) / (x1 - x0) : 0.0f;
                db[(size_t) b] = points[i - 1].second + t * (points[i].second - points[i - 1].second);
                break;
            }
        }
    }
    return db;
}

float SpectrumAnalyser::buildUpDb (Stream s, float loHz, float hiHz, const Reference& ref) const noexcept
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    if (! hasReading() || ! (hiHz > loHz)) return nan;
    const auto& db = levelsDb (s);
    const bool typical = ref.typicalDb.size() == (size_t) numBands;
    const float fitLo = ref.fitLoHz > 0.0f ? ref.fitLoHz : loHz * 0.5f;
    const float fitHi = ref.fitHiHz > 0.0f ? ref.fitHiHz : hiHz * 2.0f;

    // The track's difference from "normal", per band
    auto diff = [&] (int b) { return db[(size_t) b] - (typical ? ref.typicalDb[(size_t) b] : 0.0f); };
    auto inRange = [&] (float f) { return f >= loHz && f <= hiHz; };

    // Least-squares line (in log frequency) through the difference around the range.
    // Bands at the digital floor (nothing there at all) are left out.
    double n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
    bool below = false, above = false;
    for (int b = 0; b < numBands; ++b)
    {
        const float f = bandCentreHz (b);
        if (f < fitLo || f > fitHi || inRange (f) || db[(size_t) b] <= floorDb + 1.0f) continue;
        const double x = std::log2 (f), y = diff (b);
        n += 1; sx += x; sy += y; sxx += x * x; sxy += x * y;
        below = below || f < loHz;
        above = above || f > hiHz;
    }
    if (n < 1) return nan;
    double slope = 0.0, intercept = sy / n;
    if (below && above && n >= 2)
    {
        const double denom = n * sxx - sx * sx;
        if (std::abs (denom) > 1e-9)
        {
            slope = (n * sxy - sx * sy) / denom;
            intercept = (sy - slope * sx) / n;
        }
    }

    double sum = 0.0; int count = 0;
    for (int b = 0; b < numBands; ++b)
        if (const auto f = bandCentreHz (b); inRange (f))
        {
            sum += diff (b) - (intercept + slope * std::log2 (f));
            ++count;
        }
    return count > 0 ? (float) (sum / count) : nan;
}

float SpectrumAnalyser::buildUpDb (Stream s, float loHz, float hiHz) const noexcept
{
    static const Reference flat {};
    return buildUpDb (s, loHz, hiHz, flat);
}

float SpectrumAnalyser::changeDb (float loHz, float hiHz, float spanLoHz, float spanHiHz) const noexcept
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    if (! hasReading()) return nan;

    std::array<float, numBands> spanChanges {};
    int spanCount = 0, rangeCount = 0;
    double rangeSum = 0.0;
    for (int b = 0; b < numBands; ++b)
    {
        if (inDb[(size_t) b] <= floorDb + 1.0f) continue;   // nothing there before: no change to speak of
        const float f = bandCentreHz (b), change = outDb[(size_t) b] - inDb[(size_t) b];
        if (f >= spanLoHz && f <= spanHiHz) spanChanges[(size_t) spanCount++] = change;
        if (f >= loHz && f <= hiHz) { rangeSum += change; ++rangeCount; }
    }
    if (spanCount == 0 || rangeCount == 0) return nan;
    const auto mid = spanChanges.begin() + spanCount / 2;
    std::nth_element (spanChanges.begin(), mid, spanChanges.begin() + spanCount);
    return (float) (rangeSum / rangeCount) - *mid;
}

float SpectrumAnalyser::rumbleDb (Stream s, float belowHz) const noexcept
{
    if (! hasReading()) return std::numeric_limits<float>::quiet_NaN();
    const auto& power = s == Stream::in ? sumIn : sumOut;
    double below = 0.0, total = 0.0;
    for (int b = 0; b < numBands; ++b)
    {
        total += power[(size_t) b];
        if (bandCentreHz (b) < belowHz) below += power[(size_t) b];
    }
    if (total <= 0.0) return std::numeric_limits<float>::quiet_NaN();
    return below > 0.0 ? std::max (floorDb, (float) (10.0 * std::log10 (below / total))) : floorDb;
}

} // namespace na::analysis
