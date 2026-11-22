#pragma once

#include <algorithm>
#include <cmath>

namespace na::dsp
{

/** The listening tools (FEEDBACK-ENGINE.md §6), for any gain-based processor.

    - normal:  the processed output, untouched
    - compare: the dry input × a level-match gain, so before/after are equally
               loud ("louder always sounds better")
    - removed: what the processor takes away, dry × removedFactor
               (for a compressor: 1 − its gain before makeup)

    Switching crossfades over 20 ms and the level-match gain glides over
    50 ms, so nothing clicks. Real-time safe; plain C++.
*/
class ListeningTools
{
public:
    enum class Mode { normal = 0, compare = 1, removed = 2 };
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate)
    {
        fadeStep = 1.0f / std::max (1.0f, (float) (0.02 * sampleRate));
        gainCoeff = (float) std::exp (-1.0 / (0.05 * sampleRate));
        reset();
    }

    void reset()
    {
        weights[0] = 1.0f; weights[1] = weights[2] = 0.0f;
        mode = Mode::normal;
        compareGain = targetCompareGain;
    }

    void setMode (Mode m) noexcept { mode = m; }
    Mode getMode() const noexcept { return mode; }

    /** Level-match gain for Compare, in dB (out − in loudness). */
    void setCompareGainDb (float db) noexcept
    {
        targetCompareGain = std::pow (10.0f, std::clamp (db, -24.0f, 24.0f) * 0.05f);
    }

    /** out: the processed audio, replaced in place. dry: the input before processing.
        removedFactor: per-sample fraction of the dry signal the processor removed. */
    void process (float* const* out, const float* const* dry, const float* removedFactor,
                  int numChannels, int numSamples) noexcept
    {
        numChannels = std::clamp (numChannels, 0, maxChannels);
        const bool idle = mode == Mode::normal && weights[0] >= 1.0f;
        if (idle || numChannels == 0)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            for (int k = 0; k < 3; ++k)
            {
                const float target = (int) mode == k ? 1.0f : 0.0f;
                weights[k] += std::clamp (target - weights[k], -fadeStep, fadeStep);
            }
            compareGain = targetCompareGain + gainCoeff * (compareGain - targetCompareGain);

            const float dryGain = weights[1] * compareGain + weights[2] * removedFactor[i];
            for (int ch = 0; ch < numChannels; ++ch)
                out[ch][i] = weights[0] * out[ch][i] + dryGain * dry[ch][i];
        }
    }

private:
    Mode mode = Mode::normal;
    float weights[3] { 1.0f, 0.0f, 0.0f };   // normal, compare, removed
    float fadeStep = 1.0f / 960.0f, gainCoeff = 0.9995f;
    float compareGain = 1.0f, targetCompareGain = 1.0f;
};

} // namespace na::dsp
