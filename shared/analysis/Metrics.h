#pragma once

#include "KWeighting.h"

#include <cstdint>
#include <limits>
#include <vector>

namespace na::analysis
{

/** Every metric from docs/FEEDBACK-ENGINE.md §2. NaN means "not enough data yet". */
struct Metrics
{
    static constexpr float none = std::numeric_limits<float>::quiet_NaN();

    enum class State { silence, listening, evaluating };   // §2.1 / §3
    State state = State::silence;
    bool  inputActive = false;        // the 300 ms gate is open right now (§2.1)
    float activeSeconds = 0.0f;       // active audio in the analysis window

    float dynamicRangeDb = none;      // §2.2: p95 − p10 of momentary loudness (active only)
    float momentaryInLufs = none;     // latest 400 ms K-weighted loudness of `in`

    float grP90Db = none;             // §2.3: 90th percentile GR over active audio
    float currentGrDb = 0.0f;         // latest GR sample

    int   onsetCount = 0;             // §2.4: onsets since reset
    float frontsKeptPct = none;       // §2.5: median of the last 8 counted onsets
    int   frontsKeptCount = 0;        //        how many onsets that median uses (0..8)
    float lastFrontGrDb = none;       //        latest counted front: reduction applied …
    float lastFrontTargetGrDb = none; //        … and what an instant attack would have applied

    float residualGrDb = none;        // §2.6: median GR in the 5 ms before the last 8 onsets
    float grRippleDb = none;          // §2.6: largest GR peak-to-peak in a sustained 20 ms span

    float loudnessInLufs = none;      // §2.7: short-term (3 s) K-weighted loudness
    float loudnessOutLufs = none;
    float levelDiffDb = none;         //        out − in

    // Gain staging (docs/GAIN-STAGING.md §4), unweighted, in dBFS
    float inPeakDb = none;            // 95th percentile of 100 ms window peaks, active audio
    float outPeakDb = none;
    float inAverageDb = none;         // RMS of active audio in the window
    float outAverageDb = none;
    float crestDb = none;             // out peak − out average
    float maxOutPeakDb = none;        // loudest out sample since resetPeakHold()
    float holdActiveSeconds = 0.0f;   // active audio heard since resetPeakHold()
};

/** Computes Metrics from four aligned streams: `in` (mono-summed input),
    `out` (mono-summed output after makeup, before Mix), `gr` (gain
    reduction in dB, positive = reducing) and `grTarget` (the reduction an
    instant attack would apply: the static curve of the input).

    prepare() allocates; process() doesn't, so it is safe to call from a
    timer or analysis thread at any rate. Not for the audio thread (it does
    real work per sample, and sorts small arrays when it finishes a call).
*/
class MetricsEngine
{
public:
    // Tunables (FEEDBACK-ENGINE.md §2). Kept together so they're easy to adjust.
    static constexpr double windowSeconds      = 4.0;    // rolling analysis window
    static constexpr double gateDb             = -50.0;  // §2.1 activity gate (short-term RMS)
    static constexpr double gateRmsSeconds     = 0.3;
    static constexpr double silenceFraction    = 0.8;
    static constexpr double minActiveSeconds   = 2.0;
    static constexpr double momentarySeconds   = 0.4;    // §2.2
    static constexpr double hopSeconds         = 0.1;
    static constexpr int    minMomentaryValues = 5;
    static constexpr double shortTermSeconds   = 3.0;    // §2.7
    static constexpr double onsetAttackMs      = 1.0;    // §2.4
    static constexpr double onsetReleaseMs     = 50.0;
    static constexpr double onsetRiseDb        = 6.0;
    static constexpr double onsetRiseWindowMs  = 10.0;
    static constexpr double onsetMinGapMs      = 50.0;
    static constexpr double onsetPreRollMs     = 2.0;    // detection lags the true start by ~1-2 ms
    static constexpr double frontWindowMs      = 10.0;   // §2.5
    static constexpr double minFrontTargetDb   = 1.0;    // fronts the compressor barely touches don't count
    static constexpr int    onsetsForMedian    = 8;
    static constexpr double residualWindowMs   = 5.0;    // §2.6
    static constexpr double rippleSpanMs       = 20.0;
    static constexpr double rippleQuietAfterOnsetMs = 50.0;
    static constexpr double peakWindowSeconds  = 0.1;    // gain staging peaks: back-to-back 100 ms windows (GAIN-STAGING §4)
    static constexpr double peakPercentile     = 0.95;

    void prepare (double sampleRate);
    void reset();

    /** Starts the "loudest peak so far" hold over (Vol. I Chapter IV). */
    void resetPeakHold() noexcept;

    void process (const float* in, const float* out, const float* gr, const float* grTarget, int numSamples) noexcept;

    const Metrics& get() const noexcept { return metrics; }

    /** Seconds of audio analysed since prepare()/reset(). The feedback clock:
        "hold for 2 s" means 2 s of the track, and it pauses when audio stops. */
    double getSecondsAnalysed() const noexcept { return (double) position / sampleRate; }

private:
    void processSample (float in, float out, float gr, float grTarget) noexcept;
    void finishTick() noexcept;
    void finishHop() noexcept;
    void onOnset (int64_t at) noexcept;
    void completeFront (int64_t onset) noexcept;
    void updateSummary() noexcept;

    int    msToSamples (double ms) const noexcept;
    int    secondsToTicks (double s) const noexcept;

    template <typename T> struct Ring
    {
        std::vector<T> data;
        size_t head = 0, filled = 0;
        void init (size_t n, T value) { data.assign (n, value); head = 0; filled = 0; }
        /** Pushes v, returns the value it replaced. */
        T push (T v) noexcept
        {
            const T old = data[head];
            data[head] = v;
            head = (head + 1) % data.size();
            if (filled < data.size()) ++filled;
            return old;
        }
        /** k = 0 is the newest value. */
        T back (size_t k) const noexcept { return data[(head + data.size() - 1 - k) % data.size()]; }
        size_t size() const noexcept { return data.size(); }
    };

    double sampleRate = 48000.0;
    int    tickLength = 48;       // samples per ~1 ms tick
    double tickSeconds = 0.001;

    // Per-sample
    KWeighting kIn, kOut;
    double envelope = 0.0, envAttack = 0.0, envRelease = 0.0;
    int64_t position = 0;         // absolute sample index
    int64_t lastOnset = std::numeric_limits<int64_t>::min() / 2;
    int    onsetMinGap = 0, riseWindowTicks = 10;

    // History of the raw streams, for the per-onset measurements
    std::vector<float> histIn, histOut, histGr, histTarget;
    size_t histMask = 0;

    // Current tick accumulators
    int    tickCount = 0;
    double tickSumSq = 0.0, tickSumKIn = 0.0, tickSumKOut = 0.0, tickSumGr = 0.0, tickSumSqOut = 0.0;
    float  tickPeakIn = 0.0f, tickPeakOut = 0.0f;

    // Gain staging: per-tick peaks and power over the window; window peaks per hop
    Ring<float>  tickPeakInRing, tickPeakOutRing;
    Ring<double> tickPowerInRing, tickPowerOutRing;
    Ring<float>  peakWindowInRing, peakWindowOutRing;   // NaN = inactive
    int    peakWindowTicks = 400;
    float  maxOutPeak = 0.0f;
    int64_t holdActiveTicks = 0;
    float  tickEnvMinDb = 0.0f, tickGrMin = 0.0f, tickGrMax = 0.0f;

    // Per-tick rings
    Ring<double>  gateRing;   double gateSum = 0.0;        // raw mean square, 300 ms
    Ring<uint8_t> activeRing; int activeCount = 0;          // window
    Ring<int16_t> grBinRing;                                 // window, -1 = inactive
    std::vector<int> grHistogram;                            // 0.1 dB bins
    int    grHistogramCount = 0;
    Ring<double>  kInRing, kOutRing;                         // short-term window
    Ring<float>   envMinRing;                                // onset rise reference
    int    hopTicks = 100, hopCounter = 0, momentaryTicks = 400;
    bool   gateOpen = false;
    int64_t ticksSeen = 0;

    // Momentary loudness values (one per hop) over the window; NaN = inactive
    Ring<float> momentaryRing;

    // Ripple spans
    int    spanTicks = 20, spanCounter = 0;
    bool   spanAllActive = true;
    int64_t spanStart = 0;
    float  spanGrMin = 0.0f, spanGrMax = 0.0f;
    Ring<float> rippleRing;    // NaN = not sustained

    // Onsets
    int64_t pending[16] = {};
    int    numPending = 0;
    Ring<float> frontsRing, residualRing;

    // Scratch for percentiles/medians (sized in prepare)
    std::vector<float> scratch;

    Metrics metrics;
};

} // namespace na::analysis
