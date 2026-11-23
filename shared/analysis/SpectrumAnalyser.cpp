#include "SpectrumAnalyser.h"

#include <algorithm>
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

float SpectrumAnalyser::buildUpDb (Stream s, float loHz, float hiHz) const noexcept
{
    if (! hasReading() || ! (hiHz > loHz)) return std::numeric_limits<float>::quiet_NaN();
    const auto& db = levelsDb (s);

    // Mean level (dB per band) over [a, b]; NaN if no band falls in it
    auto meanLevel = [&] (float a, float b)
    {
        double sum = 0.0; int n = 0;
        for (int i = 0; i < numBands; ++i)
            if (const auto f = bandCentreHz (i); f >= a && f <= b) { sum += db[(size_t) i]; ++n; }
        return n > 0 ? (float) (sum / n) : std::numeric_limits<float>::quiet_NaN();
    };

    // The straight line (in log frequency) between the octave below and the octave above
    const float belowLevel = meanLevel (loHz * 0.5f, loHz * 0.999f);
    const float aboveLevel = meanLevel (hiHz * 1.001f, hiHz * 2.0f);
    const float belowAt = std::log2 (loHz / std::sqrt (2.0f)), aboveAt = std::log2 (hiHz * std::sqrt (2.0f));
    auto line = [&] (float hz)
    {
        if (std::isnan (belowLevel)) return aboveLevel;
        if (std::isnan (aboveLevel)) return belowLevel;
        const float t = (std::log2 (hz) - belowAt) / (aboveAt - belowAt);
        return belowLevel + t * (aboveLevel - belowLevel);
    };

    double sum = 0.0; int n = 0;
    for (int i = 0; i < numBands; ++i)
        if (const auto f = bandCentreHz (i); f >= loHz && f <= hiHz)
        {
            sum += db[(size_t) i] - line (f);
            ++n;
        }
    return n > 0 && ! (std::isnan (belowLevel) && std::isnan (aboveLevel)) ? (float) (sum / n)
                                                                            : std::numeric_limits<float>::quiet_NaN();
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
