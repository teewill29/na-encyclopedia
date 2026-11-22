#include "Compressor.h"

#include <algorithm>

namespace na::vol4
{

namespace
{
    constexpr float silenceDb = -120.0f;

    inline float gainToDb (float g) noexcept
    {
        return g > 1.0e-6f ? 20.0f * std::log10 (g) : silenceDb;
    }

    inline float dbToGain (float db) noexcept
    {
        return std::pow (10.0f, db * 0.05f);
    }
}

void Compressor::prepare (double newSampleRate)
{
    sampleRate  = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    rampSamples = std::max (1, (int) std::lround (rampSeconds * sampleRate));
    reset();
}

void Compressor::reset()
{
    envelopeDb   = 0.0f;
    blockMaxGrDb = 0.0f;
    needsJump    = true;
}

float Compressor::timeToCoeff (float ms, double sr) noexcept
{
    // One-pole coefficient: reaches ~63% of a step in `ms`.
    const auto samples = std::max (1.0e-3, (double) ms * 0.001 * sr);
    return (float) std::exp (-1.0 / samples);
}

void Compressor::setSettings (const CompressorSettings& s)
{
    const auto r = std::clamp (s.ratio, 1.0f, 20.0f);

    if (needsJump)
    {
        threshold.jumpTo (s.thresholdDb);
        ratio.jumpTo (r);
        makeupGain.jumpTo (dbToGain (s.makeupDb));
        mix.jumpTo (std::clamp (s.mixPct, 0.0f, 100.0f) * 0.01f);
        needsJump = false;
    }
    else
    {
        threshold.rampTo (s.thresholdDb, rampSamples);
        ratio.rampTo (r, rampSamples);
        makeupGain.rampTo (dbToGain (s.makeupDb), rampSamples);
        mix.rampTo (std::clamp (s.mixPct, 0.0f, 100.0f) * 0.01f, rampSamples);
    }

    attackCoeff  = timeToCoeff (std::clamp (s.attackMs,  0.1f, 100.0f),  sampleRate);
    releaseCoeff = timeToCoeff (std::clamp (s.releaseMs, 10.0f, 1000.0f), sampleRate);
}

float Compressor::gainReductionDb (float inputDb, float thresholdDb, float r, float knee) noexcept
{
    const auto over  = inputDb - thresholdDb;
    const auto slope = 1.0f - 1.0f / std::max (1.0f, r);

    if (knee > 0.0f && 2.0f * std::abs (over) <= knee)
    {
        const auto x = over + knee * 0.5f;
        return slope * x * x / (2.0f * knee);
    }

    return over > 0.0f ? slope * over : 0.0f;
}

void Compressor::process (float* const* channels, int numChannels, int numSamples,
                          float* grOut, float* wetGainOut, float* targetGrOut, float* removedOut) noexcept
{
    numChannels = std::clamp (numChannels, 0, maxChannels);
    blockMaxGrDb = 0.0f;

    if (numChannels == 0 || needsJump)
        return; // setSettings() not called yet: pass audio through untouched

    for (int i = 0; i < numSamples; ++i)
    {
        // Stereo-linked peak detector.
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            peak = std::max (peak, std::abs (channels[ch][i]));

        const auto targetGr = gainReductionDb (gainToDb (peak), threshold.next(), ratio.next());

        // Log-domain smoothing: attack while reduction grows, release while it shrinks.
        const auto coeff = targetGr > envelopeDb ? attackCoeff : releaseCoeff;
        envelopeDb = targetGr + coeff * (envelopeDb - targetGr);

        const auto reduction = dbToGain (-envelopeDb);
        const auto wet = reduction * makeupGain.next();
        const auto m   = mix.next();
        const auto gain = m * wet + (1.0f - m); // dry is unity gain, no latency to match

        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][i] *= gain;

        if (grOut != nullptr)
            grOut[i] = envelopeDb;
        if (wetGainOut != nullptr)
            wetGainOut[i] = wet;
        if (targetGrOut != nullptr)
            targetGrOut[i] = targetGr;
        if (removedOut != nullptr)
            removedOut[i] = 1.0f - reduction;

        blockMaxGrDb = std::max (blockMaxGrDb, envelopeDb);
    }

    // Keep denormals out of the envelope during long silences.
    if (envelopeDb < 1.0e-6f)
        envelopeDb = 0.0f;
}

} // namespace na::vol4
