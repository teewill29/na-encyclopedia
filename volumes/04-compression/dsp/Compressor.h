#pragma once

#include <cmath>

namespace na::vol4
{

/** Control values, in user units. */
struct CompressorSettings
{
    float thresholdDb = 0.0f;   // −60 … 0
    float ratio       = 4.0f;   // 1 … 20
    float attackMs    = 10.0f;  // 0.1 … 100
    float releaseMs   = 100.0f; // 10 … 1000
    float makeupDb    = 0.0f;   // −12 … +24
    float mixPct      = 100.0f; // 0 … 100
};

/** Feed-forward, stereo-linked compressor (README → Compressor DSP V1).

    - Peak detector: the loudest channel's |x| each sample (stereo-linked).
    - Static curve with a fixed 6 dB soft knee.
    - Gain reduction smoothed in the log (dB) domain with separate attack and
      release ballistics.
    - Zero latency. Threshold, ratio, makeup and mix are ramped over 20 ms so
      knob moves and automation never click.

    Real-time safe: no allocation, locks or system calls after prepare().
*/
class Compressor
{
public:
    static constexpr int   maxChannels = 2;
    static constexpr float kneeDb      = 6.0f;
    static constexpr float rampSeconds = 0.02f;

    void prepare (double sampleRate);
    void reset();

    /** Call once per block before process(). The first call after prepare()
        or reset() jumps straight to the values; later calls ramp to them. */
    void setSettings (const CompressorSettings& s);

    /** Processes numChannels (1 or 2) channels in place.
        Optional per-sample outputs for the analysis taps:
          grOut      gain reduction in dB (positive = turning down)
          wetGainOut gain of the compressed path incl. makeup, before Mix
                     (so `out` = input × wetGain, see FEEDBACK-ENGINE.md §1)
          targetGrOut the reduction an instant attack would apply right now
                     (the static curve); lets the analysis tell how much of
                     each note's front the attack lets through (§2.5) */
    void process (float* const* channels, int numChannels, int numSamples,
                  float* grOut = nullptr, float* wetGainOut = nullptr, float* targetGrOut = nullptr) noexcept;

    /** Largest gain reduction (dB) in the most recent block. */
    float getBlockMaxGainReductionDb() const noexcept { return blockMaxGrDb; }

    /** The static curve on its own: dB of gain reduction for a level. */
    static float gainReductionDb (float inputDb, float thresholdDb, float ratio, float knee = kneeDb) noexcept;

private:
    struct Ramp
    {
        float current = 0.0f, target = 0.0f, step = 0.0f;
        int   remaining = 0;

        void jumpTo (float v) noexcept { current = target = v; step = 0.0f; remaining = 0; }
        void rampTo (float v, int numSteps) noexcept
        {
            if (! (v < target || v > target)) return; // unchanged (exact compare is intended)
            target = v;
            remaining = numSteps;
            step = (target - current) / (float) numSteps;
        }
        float next() noexcept
        {
            if (remaining > 0) { current += step; if (--remaining == 0) current = target; }
            return current;
        }
    };

    static float timeToCoeff (float ms, double sampleRate) noexcept;

    double sampleRate = 44100.0;
    int    rampSamples = 882;
    bool   needsJump = true;

    Ramp threshold, ratio, makeupGain, mix;
    float attackCoeff = 0.0f, releaseCoeff = 0.0f;
    float envelopeDb = 0.0f;
    float blockMaxGrDb = 0.0f;
};

} // namespace na::vol4
