#include "Equalizer.h"

#include <algorithm>
#include <complex>

namespace na::vol3
{

namespace
{
    constexpr double pi = 3.14159265358979323846;
    constexpr int updateEvery = 16;
}

void Equalizer::prepare (double sr)
{
    sampleRate = sr > 0.0 ? sr : 48000.0;
    // ~20 ms time constant, applied once per 16-sample step
    glide = (float) std::exp (-(double) updateEvery / (0.02 * sampleRate));
    outputGlide = (float) std::exp (-1.0 / (0.02 * sampleRate));
    reset();
}

void Equalizer::reset()
{
    for (auto& ch : state)
        for (auto& s : ch) s = {};
    needsJump = true;
    sinceUpdate = 0;
}

BandSettings Equalizer::effective (const BandSettings& b)
{
    if (b.on) return b;
    auto off = b;
    if (b.shape == BandSettings::Shape::lowCut) off.freqHz = 3.0f;    // glides well below the audio range
    else off.gainDb = 0.0f;                                           // glides flat
    off.on = true;
    return off;
}

Equalizer::Coeffs Equalizer::design (BandSettings::Shape shape, float freqHz, float gainDb, float q, double sr)
{
    // RBJ Audio EQ Cookbook
    const double f = std::clamp ((double) freqHz, 2.0, sr * 0.49);
    const double w0 = 2.0 * pi * f / sr;
    const double cw = std::cos (w0), sw = std::sin (w0);
    const double A = std::pow (10.0, gainDb / 40.0);
    const double Q = std::max (0.1, (double) q);
    double b0, b1, b2, a0, a1, a2;

    switch (shape)
    {
        case BandSettings::Shape::lowCut:
        {
            const double alpha = sw / (2.0 * 0.70710678);
            b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2;
            a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
            break;
        }
        case BandSettings::Shape::bell:
        {
            const double alpha = sw / (2.0 * Q);
            b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
            a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A;
            break;
        }
        case BandSettings::Shape::lowShelf:
        case BandSettings::Shape::highShelf:
        {
            const double alpha = sw / (2.0 * std::min (Q, 2.0));   // shelves: keep the slope sensible
            const double s2a = 2.0 * std::sqrt (A) * alpha;
            if (shape == BandSettings::Shape::lowShelf)
            {
                b0 = A * ((A + 1) - (A - 1) * cw + s2a);
                b1 = 2 * A * ((A - 1) - (A + 1) * cw);
                b2 = A * ((A + 1) - (A - 1) * cw - s2a);
                a0 = (A + 1) + (A - 1) * cw + s2a;
                a1 = -2 * ((A - 1) + (A + 1) * cw);
                a2 = (A + 1) + (A - 1) * cw - s2a;
            }
            else
            {
                b0 = A * ((A + 1) + (A - 1) * cw + s2a);
                b1 = -2 * A * ((A - 1) + (A + 1) * cw);
                b2 = A * ((A + 1) + (A - 1) * cw - s2a);
                a0 = (A + 1) - (A - 1) * cw + s2a;
                a1 = 2 * ((A - 1) - (A + 1) * cw);
                a2 = (A + 1) - (A - 1) * cw - s2a;
            }
            break;
        }
        default:
            return {};
    }
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

void Equalizer::setSettings (const EqSettings& s)
{
    target = s;
    outputTarget = std::pow (10.0f, std::clamp (s.outputDb, -24.0f, 24.0f) * 0.05f);
    if (needsJump)
    {
        for (int b = 0; b < numBands; ++b)
        {
            const auto e = effective (target.bands[(size_t) b]);
            smooth[(size_t) b] = { std::log (std::max (2.0f, e.freqHz)), e.gainDb, e.q };
        }
        outputGain = outputTarget;
        updateCoefficients();
        needsJump = false;
    }
}

void Equalizer::updateCoefficients() noexcept
{
    for (int b = 0; b < numBands; ++b)
    {
        const auto e = effective (target.bands[(size_t) b]);
        auto& sm = smooth[(size_t) b];
        const float tf = std::log (std::max (2.0f, e.freqHz));
        sm.logFreq = tf + glide * (sm.logFreq - tf);
        sm.gain    = e.gainDb + glide * (sm.gain - e.gainDb);
        sm.q       = e.q + glide * (sm.q - e.q);
        coeffs[(size_t) b] = design (e.shape, std::exp (sm.logFreq), sm.gain, sm.q, sampleRate);
    }
}

void Equalizer::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    numChannels = std::clamp (numChannels, 0, maxChannels);
    if (needsJump || numChannels == 0)
        return;   // setSettings() not called yet: pass through

    for (int i = 0; i < numSamples; ++i)
    {
        if (++sinceUpdate >= updateEvery)
        {
            sinceUpdate = 0;
            updateCoefficients();
        }
        outputGain = outputTarget + outputGlide * (outputGain - outputTarget);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            double x = channels[ch][i];
            for (int b = 0; b < numBands; ++b)
            {
                const auto& c = coeffs[(size_t) b];
                auto& st = state[(size_t) ch][(size_t) b];
                const double y = c.b0 * x + st.z1;          // transposed direct form II
                st.z1 = c.b1 * x - c.a1 * y + st.z2;
                st.z2 = c.b2 * x - c.a2 * y;
                x = y;
            }
            channels[ch][i] = (float) x * outputGain;
        }
    }

    // Keep denormals out of the filter states during silence
    for (auto& ch : state)
        for (auto& s : ch)
        {
            if (std::abs (s.z1) < 1.0e-20) s.z1 = 0.0;
            if (std::abs (s.z2) < 1.0e-20) s.z2 = 0.0;
        }
}

float Equalizer::responseDb (const EqSettings& s, float freqHz, double sr)
{
    const double w = 2.0 * pi * freqHz / sr;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
    double db = 20.0 * std::log10 (std::pow (10.0, s.outputDb / 20.0));
    for (const auto& band : s.bands)
    {
        const auto e = effective (band);
        const auto c = design (e.shape, e.freqHz, e.gainDb, e.q, sr);
        const auto h = (c.b0 + c.b1 * z1 + c.b2 * z2) / (1.0 + c.a1 * z1 + c.a2 * z2);
        db += 20.0 * std::log10 (std::max (1.0e-9, std::abs (h)));
    }
    return (float) db;
}

} // namespace na::vol3
