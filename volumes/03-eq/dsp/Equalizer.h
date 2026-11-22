#pragma once

#include <array>
#include <cmath>

namespace na::vol3
{

/** One band's settings, in user units (docs/EQ.md §2). */
struct BandSettings
{
    enum class Shape { lowCut, lowShelf, bell, highShelf };
    Shape shape = Shape::bell;
    float freqHz = 1000.0f;
    float gainDb = 0.0f;     // ignored by the low cut
    float q = 1.0f;          // width; the low cut is fixed at 0.707 (Butterworth, 12 dB/oct)
    bool  on = true;
};

struct EqSettings
{
    std::array<BandSettings, 5> bands;
    float outputDb = 0.0f;
};

/** Five-band parametric EQ (RBJ cookbook biquads), stereo, zero latency.

    Changes never click: every band's frequency (in log space), gain and
    width glide over ~20 ms and the filters are recalculated every 16
    samples. Turning a band off glides it to "transparent" (0 dB, or the
    low cut down to 3 Hz) rather than switching it out.

    Real-time safe; plain C++.
*/
class Equalizer
{
public:
    static constexpr int numBands = 5, maxChannels = 2;

    void prepare (double sampleRate);
    void reset();

    /** Call once per block. The first call after prepare()/reset() jumps straight there. */
    void setSettings (const EqSettings&);

    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    /** Magnitude response of a settings set at a frequency, in dB (for drawing the curve). */
    static float responseDb (const EqSettings&, float freqHz, double sampleRate);

private:
    struct Coeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    struct State  { double z1 = 0, z2 = 0; };
    struct Smoothed { float logFreq = 0, gain = 0, q = 1; };

    static Coeffs design (BandSettings::Shape, float freqHz, float gainDb, float q, double sampleRate);
    static BandSettings effective (const BandSettings&);   // what "off" glides to
    void updateCoefficients() noexcept;

    double sampleRate = 48000.0;
    float glide = 0.0f;                 // per-16-sample-step smoothing coefficient
    bool needsJump = true;

    EqSettings target;
    std::array<Smoothed, numBands> smooth {};
    std::array<Coeffs, numBands> coeffs {};
    std::array<std::array<State, numBands>, maxChannels> state {};
    float outputGain = 1.0f, outputTarget = 1.0f, outputGlide = 0.999f;
    int sinceUpdate = 0;
};

} // namespace na::vol3
