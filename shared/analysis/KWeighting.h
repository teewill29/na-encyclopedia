#pragma once

#include <cmath>

namespace na::analysis
{

/** ITU-R BS.1770 K-weighting filter (high shelf + high pass), any sample rate.
    Coefficients from the analog prototype, as in libebur128. */
class KWeighting
{
public:
    void prepare (double sampleRate)
    {
        constexpr double pi = 3.14159265358979323846;
        {   // Stage 1: high shelf (head effects)
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (pi * f0 / sampleRate);
            const double Vh = std::pow (10.0, G / 20.0);
            const double Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            s1 = { (Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0,
                   2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
        }
        {   // Stage 2: high pass (RLB)
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (pi * f0 / sampleRate);
            const double a0 = 1.0 + K / Q + K * K;
            s2 = { 1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
        }
        reset();
    }

    void reset() { s1.z1 = s1.z2 = s2.z1 = s2.z2 = 0.0; }

    double process (double x) noexcept { return s2.process (s1.process (x)); }

    /** Loudness (LUFS) of a mean-square value of K-weighted samples. */
    static double loudness (double meanSquare) noexcept
    {
        return meanSquare > 1.0e-20 ? -0.691 + 10.0 * std::log10 (meanSquare) : -200.0;
    }

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double process (double x) noexcept
        {   // transposed direct form II
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };
    Biquad s1, s2;
};

} // namespace na::analysis
