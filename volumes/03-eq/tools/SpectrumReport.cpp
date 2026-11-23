// Vol. III tuning report (not shipped): runs the shared spectrum analysis over a
// real recording and prints what each instrument's chapters would measure, so
// the content numbers (mudNeedDb, harshNeedDb, …) can be checked against real
// tracks (docs/EQ.md §9).
//
// Usage: NAEncyclopediaSpectrumReport <file.wav> [instrumentId]

#include "analysis/SpectrumAnalyser.h"
#include "EqReferences.h"
#include "BinaryData.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using na::analysis::SpectrumAnalyser;
using Stream = SpectrumAnalyser::Stream;

namespace
{
float num (const juce::var& v) { return (float) (double) v; }

float percentile (std::vector<float> v, double p)
{
    v.erase (std::remove_if (v.begin(), v.end(), [] (float x) { return std::isnan (x); }), v.end());
    if (v.empty()) return std::nanf ("");
    std::sort (v.begin(), v.end());
    return v[(size_t) std::lround (p * (double) (v.size() - 1))];
}
} // namespace

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::printf ("Usage: %s <file.wav> [instrumentId]\n", argv[0]);
        return 1;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (juce::String (argv[1]))));
    if (reader == nullptr) { std::printf ("Can't read %s\n", argv[1]); return 1; }

    const auto length = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> buffer ((int) reader->numChannels, length);
    reader->read (&buffer, 0, length, 0, true, true);
    std::vector<float> mono ((size_t) length);
    for (int i = 0; i < length; ++i)
    {
        float s = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) s += buffer.getSample (ch, i);
        mono[(size_t) i] = s / (float) buffer.getNumChannels();
    }
    std::printf ("%s: %.1f s, %d ch, %.0f Hz\n\n", argv[1], length / reader->sampleRate, buffer.getNumChannels(), reader->sampleRate);

    const auto content = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::eq_json, BinaryData::eq_jsonSize));
    const auto* instruments = content["instruments"].getArray();
    if (instruments == nullptr) { std::printf ("No instruments in eq.json\n"); return 1; }

    // Run the analyser over the file like a host would, sampling the reading once a second.
    SpectrumAnalyser analyser;
    analyser.prepare (reader->sampleRate);
    struct Series { std::vector<float> mud, harsh, rumble; };
    std::vector<Series> series ((size_t) instruments->size());
    std::vector<na::vol3::EqReferences> refs;
    for (const auto& inst : *instruments)
        refs.push_back (na::vol3::EqReferences::forInstrument (inst));
    const int second = (int) reader->sampleRate;
    for (int start = 0; start < length; start += second)
    {
        const int n = std::min (second, length - start);
        analyser.process (mono.data() + start, mono.data() + start, n);
        if (! analyser.hasReading()) continue;
        for (size_t k = 0; k < (size_t) instruments->size(); ++k)
        {
            const auto& inst = (*instruments)[(int) k];
            series[k].mud.push_back    (analyser.buildUpDb (Stream::in, num (inst["mudHz"]["min"]), num (inst["mudHz"]["max"]), refs[k].mud));
            series[k].harsh.push_back  (analyser.buildUpDb (Stream::in, num (inst["harshHz"]["min"]), num (inst["harshHz"]["max"]), refs[k].harsh));
            series[k].rumble.push_back (analyser.rumbleDb (Stream::in, num (inst["lowCutHz"]["min"])));
        }
    }

    // The last 3 s average, in 1/3 octaves, so the shape of the track is visible
    std::printf ("Spectrum (last %.1f s of active audio), dB per 1/6 octave, every other band shown:\n", analyser.getActiveSeconds());
    const auto& db = analyser.levelsDb (Stream::in);
    for (int b = 6; b < SpectrumAnalyser::numBands; b += 2)
    {
        const float hz = SpectrumAnalyser::bandCentreHz (b);
        const int bar = std::max (0, (int) std::lround ((db[(size_t) b] + 90.0f) / 2.0f));
        std::printf ("  %7.0f Hz %6.1f  %s\n", hz, db[(size_t) b], std::string ((size_t) bar, '#').c_str());
    }

    // Per instrument: what Chapter I would see (median and 90th percentile of the 3 s readings)
    std::printf ("\n%-26s %-14s %18s %10s   %-14s %18s %10s   %-10s %14s\n",
                 "instrument", "mud range", "build-up med/p90", "need", "harsh range", "build-up med/p90", "need", "rumble <", "med");
    const juce::String only = argc > 2 ? juce::String (argv[2]) : juce::String();
    for (size_t k = 0; k < (size_t) instruments->size(); ++k)
    {
        const auto& inst = (*instruments)[(int) k];
        if (only.isNotEmpty() && inst["id"].toString() != only) continue;
        const auto& s = series[k];
        char mud[32], harsh[32];
        std::snprintf (mud, sizeof (mud), "%.0f-%.0f", num (inst["mudHz"]["min"]), num (inst["mudHz"]["max"]));
        std::snprintf (harsh, sizeof (harsh), "%.0f-%.0f", num (inst["harshHz"]["min"]), num (inst["harshHz"]["max"]));
        std::printf ("%-26s %-14s %+8.1f / %+6.1f %+10.1f   %-14s %+8.1f / %+6.1f %+10.1f   %-10.0f %+14.1f\n",
                     inst["name"].toString().toRawUTF8(),
                     mud, percentile (s.mud, 0.5), percentile (s.mud, 0.9), num (inst["mudNeedDb"]),
                     harsh, percentile (s.harsh, 0.5), percentile (s.harsh, 0.9), num (inst["harshNeedDb"]),
                     num (inst["lowCutHz"]["min"]), percentile (s.rumble, 0.5));
    }
    return 0;
}
