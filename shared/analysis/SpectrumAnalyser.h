#pragma once

#include <array>
#include <complex>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace na::analysis
{

/** The averaged spectrum of `in` and `out` (docs/EQ.md §4), and the
    measurements built on it: energy in a range, build-up, rumble.

    Hann-windowed 4096-point FFT, 50 % overlap, summed into 1/6-octave bands,
    averaged over the last ~3 s of active audio (frames whose `in` level is
    under the gate are skipped, so the reading holds when the track stops).

    Band level = the signal's mean-square power that falls in the band, in dB
    (a full-scale sine reads −3 dB in its band). Pink noise reads flat.

    prepare() allocates; process() doesn't. Message/analysis thread only,
    never the audio thread (it runs FFTs).
*/
class SpectrumAnalyser
{
public:
    static constexpr int    fftOrder       = 12;
    static constexpr int    fftSize        = 1 << fftOrder;   // 4096
    static constexpr int    hopSize        = fftSize / 2;     // 50 % overlap
    static constexpr double averageSeconds = 3.0;             // rolling average of active frames
    static constexpr double minSeconds     = 1.0;             // active audio before there's a reading
    static constexpr double gateDb         = -50.0;           // frame RMS of `in` (same gate as Metrics §2.1)
    static constexpr float  lowestHz       = 10.0f;           // first band centre; then every 1/6 octave
    static constexpr int    bandsPerOctave = 6;
    static constexpr int    numBands       = 67;              // 10 Hz … 20.5 kHz
    static constexpr float  floorDb        = -150.0f;         // digital silence

    enum class Stream { in, out };

    /** What "normal" looks like for a build-up measurement (docs/EQ.md §4).
        typicalDb: the instrument's typical level per band (numBands values, any
        offset), or empty for flat (pink). The track's difference from it is
        measured against a straight line fitted over [fitLoHz, fitHiHz] outside
        the range; 0 = an octave either side of the range. */
    struct Reference
    {
        std::vector<float> typicalDb;
        float fitLoHz = 0.0f, fitHiHz = 0.0f;
    };

    /** A typical spectrum from (Hz, dB) points, interpolated in log frequency
        to every band (held flat beyond the first and last point). */
    static std::vector<float> typicalFromPoints (const std::vector<std::pair<float, float>>& points);

    void prepare (double sampleRate);
    void reset() noexcept;

    /** Feeds aligned `in` and `out` samples (any count). */
    void process (const float* in, const float* out, int numSamples) noexcept;

    /** True once there's at least minSeconds of active audio in the average. */
    bool hasReading() const noexcept { return framesInAverage >= minFrames; }
    double getActiveSeconds() const noexcept { return framesInAverage * (double) hopSize / sampleRate; }
    /** Goes up by one each time the average changes (for redraws). */
    uint64_t getVersion() const noexcept { return version; }

    static float bandCentreHz (int band) noexcept;
    /** Averaged band levels in dB (numBands values; floorDb before any reading). */
    const std::vector<float>& levelsDb (Stream s) const noexcept { return s == Stream::in ? inDb : outDb; }

    /** Total power of the bands whose centre lies in [loHz, hiHz], in dB. NaN without a reading. */
    float energyDb (Stream, float loHz, float hiHz) const noexcept;
    /** How far [loHz, hiHz] stands out compared with the reference, in dB (mean
        over its bands): the track minus the typical spectrum, against a straight
        line fitted to that difference around the range. A track shaped like the
        reference reads ~0 (whatever its overall tilt); a muddy or harsh bump reads
        positive. NaN without a reading. No allocation. */
    float buildUpDb (Stream, float loHz, float hiHz, const Reference&) const noexcept;
    /** The same against a flat (pink) reference, an octave either side. */
    float buildUpDb (Stream, float loHz, float hiHz) const noexcept;
    /** How much the processing lowered (−) or raised (+) [loHz, hiHz] compared with
        the rest of the track: the mean out − in change over the range, minus the
        median change over [spanLo, spanHi]. A broad level change (an output knob)
        reads 0; a cut in the range reads about its depth there. Use it to judge
        `out` from the `in` reading: buildUp(in) + changeDb = the build-up after
        the processing, on the same footing as before it. NaN without a reading. */
    float changeDb (float loHz, float hiHz, float spanLoHz, float spanHiHz) const noexcept;

    /** Energy below belowHz relative to the whole track, in dB (≤ 0). NaN without a reading. */
    float rumbleDb (Stream, float belowHz) const noexcept;

private:
    void analyseFrame() noexcept;
    void bandPowers (const float* frame, float* dest) noexcept;
    void fft (std::complex<float>* data) const noexcept;

    double sampleRate = 48000.0;
    int    minFrames = 1, maxFrames = 1;

    // Input frames (circular, fftSize long) and how many samples since the last frame
    std::vector<float> histIn, histOut;
    int    writePos = 0, sinceFrame = 0, filled = 0;

    // FFT
    std::vector<float> window;
    double windowPower = 1.0;                       // Σ w²
    std::vector<std::complex<float>> buffer, twiddles;
    std::vector<int> bitReverse;
    std::vector<float> frame;

    // Band weights: for band b, entries [bandStart[b], bandStart[b+1]) of (bin, weight)
    std::vector<int>   bandStart;
    std::vector<int>   weightBin;
    std::vector<float> weightValue;

    // The average: a ring of per-frame band powers for each stream
    std::vector<float> ringIn, ringOut;             // maxFrames × numBands
    int    ringHead = 0, framesInAverage = 0;
    std::vector<double> sumIn, sumOut;              // mean power per band (numBands)
    std::vector<float> inDb, outDb;                 // numBands
    uint64_t version = 0;
};

} // namespace na::analysis
