// Tuning report (Milestone 5): runs a real recording through the compressor
// and the metrics, sweeping each chapter's knob, and prints what The Reading
// would say at each setting. Use it to check that the content file's knob
// targets (attackMs, releaseMs) and measurement targets (grDb, frontsKeptPct,
// residualGrMaxDb) agree on real audio.
//
//   NAEncyclopediaTuningReport <file.wav> <instrumentId> <goalId>

#include "Feedback.h"
#include "content/Content.h"
#include "dsp/Compressor.h"
#include "analysis/Metrics.h"
#include "BinaryData.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cstdio>

using namespace na;

namespace
{
struct Audio { juce::AudioBuffer<float> buffer; double sampleRate = 48000.0; };

Audio load (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    Audio a;
    if (reader == nullptr) return a;
    a.sampleRate = reader->sampleRate;
    a.buffer.setSize (2, (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 120)));
    reader->read (&a.buffer, 0, a.buffer.getNumSamples(), 0, true, true);
    if (reader->numChannels == 1) a.buffer.copyFrom (1, 0, a.buffer, 0, 0, a.buffer.getNumSamples());
    return a;
}

/** Runs the whole file at these settings; returns metrics averaged over the
    run (sampled every 100 ms while evaluating), like a user listening. */
struct Summary { float grP90, frontsKept, residual, ripple, levelDiff, dynamicRange; int samples; };

Summary run (const Audio& a, const vol4::CompressorSettings& s)
{
    vol4::Compressor comp;
    comp.prepare (a.sampleRate);
    analysis::MetricsEngine metrics;
    metrics.prepare (a.sampleRate);

    juce::AudioBuffer<float> work (a.buffer);
    const int block = 512;
    std::vector<float> in (block), out (block), gr (block), wet (block), target (block);
    Summary sum {};
    int counted = 0, sinceSample = 0;
    auto add = [] (float& acc, float v) { if (! std::isnan (v)) acc += v; };
    float c[6] = {};

    for (int start = 0; start + block <= work.getNumSamples(); start += block)
    {
        float* ch[] = { work.getWritePointer (0, start), work.getWritePointer (1, start) };
        for (int i = 0; i < block; ++i) in[(size_t) i] = 0.5f * (ch[0][i] + ch[1][i]);
        comp.setSettings (s);
        comp.process (ch, 2, block, gr.data(), wet.data(), target.data());
        for (int i = 0; i < block; ++i) out[(size_t) i] = in[(size_t) i] * wet[(size_t) i];
        metrics.process (in.data(), out.data(), gr.data(), target.data(), block);

        if ((sinceSample += block) >= (int) (a.sampleRate * 0.1))
        {
            sinceSample = 0;
            const auto& m = metrics.get();
            if (m.state != analysis::Metrics::State::evaluating) continue;
            ++counted;
            add (sum.grP90, m.grP90Db);         c[0] += std::isnan (m.grP90Db) ? 0 : 1;
            add (sum.frontsKept, m.frontsKeptPct); c[1] += std::isnan (m.frontsKeptPct) ? 0 : 1;
            add (sum.residual, m.residualGrDb);  c[2] += std::isnan (m.residualGrDb) ? 0 : 1;
            add (sum.ripple, m.grRippleDb);      c[3] += std::isnan (m.grRippleDb) ? 0 : 1;
            add (sum.levelDiff, m.levelDiffDb);  c[4] += std::isnan (m.levelDiffDb) ? 0 : 1;
            add (sum.dynamicRange, m.dynamicRangeDb); c[5] += std::isnan (m.dynamicRangeDb) ? 0 : 1;
        }
    }
    auto avg = [] (float total, float n) { return n > 0 ? total / n : analysis::Metrics::none; };
    return { avg (sum.grP90, c[0]), avg (sum.frontsKept, c[1]), avg (sum.residual, c[2]),
             avg (sum.ripple, c[3]), avg (sum.levelDiff, c[4]), avg (sum.dynamicRange, c[5]), counted };
}

analysis::Metrics asMetrics (const Summary& s)
{
    analysis::Metrics m;
    m.state = analysis::Metrics::State::evaluating;
    m.grP90Db = s.grP90; m.frontsKeptPct = s.frontsKept; m.residualGrDb = s.residual;
    m.grRippleDb = s.ripple; m.levelDiffDb = s.levelDiff; m.dynamicRangeDb = s.dynamicRange;
    return m;
}

const char* str (const std::string& s) { return s.empty() ? "-" : s.c_str(); }
} // namespace

int main (int argc, char** argv)
{
    if (argc < 4) { std::printf ("usage: %s <file.wav> <instrumentId> <goalId>\n", argv[0]); return 2; }

    const auto content = Content::fromJson (BinaryData::compression_json, (size_t) BinaryData::compression_jsonSize);
    const auto instrument = content.findInstrument (argv[2]);
    const auto goal = content.findGoal (argv[2], argv[3]);
    if (! goal.isObject()) { std::printf ("unknown instrument/goal\n"); return 2; }

    const auto audio = load (juce::File (juce::String (argv[1])));
    if (audio.buffer.getNumSamples() == 0) { std::printf ("could not read %s\n", argv[1]); return 2; }
    std::printf ("%s / %s on %s (%.1f s at %.0f Hz)\n\n", argv[2], argv[3], argv[1],
                 audio.buffer.getNumSamples() / audio.sampleRate, audio.sampleRate);

    auto range = [&goal] (const char* key) { return juce::String (goal[key]["min"].toString() + "-" + goal[key]["max"].toString()); };
    vol4::CompressorSettings s;
    s.ratio = (float) (double) goal["ratio"]["start"];

    // Chapter I
    s.thresholdDb = 0; s.ratio = 1;
    auto dry = run (audio, s);
    std::printf ("I    dynamic range %.1f dB (need-check min %s) -> %s\n\n", dry.dynamicRange,
                 instrument["needCheckMinRangeDb"].toString().toRawUTF8(),
                 str (vol4::Feedback::zoneFor ("goal", instrument, goal, asMetrics (dry), s)));

    // Chapter II: threshold sweep at the goal's ratio, start attack/release
    s.ratio = (float) (double) goal["ratio"]["start"];
    s.attackMs = (float) (double) goal["attackMs"]["start"];
    s.releaseMs = (float) (double) goal["releaseMs"]["start"];
    std::printf ("II   threshold sweep, ratio %.0f:1 (target GR %s dB)\n", s.ratio, range ("grDb").toRawUTF8());
    float chosenThreshold = 0;
    float bestDistance = 1e9f;
    const float grMid = 0.5f * (float) ((double) goal["grDb"]["min"] + (double) goal["grDb"]["max"]);
    for (float t = -6; t >= -48; t -= 3)
    {
        s.thresholdDb = t;
        const auto r = run (audio, s);
        const auto zone = vol4::Feedback::zoneFor ("ratioThreshold", instrument, goal, asMetrics (r), s);
        std::printf ("     threshold %5.0f dB  GR p90 %5.1f dB  -> %s\n", t, r.grP90, str (zone));
        if (std::abs (r.grP90 - grMid) < bestDistance) { bestDistance = std::abs (r.grP90 - grMid); chosenThreshold = t; }
    }
    s.thresholdDb = chosenThreshold;
    std::printf ("     using threshold %.0f dB from here on\n\n", chosenThreshold);

    // Chapter III: attack sweep (target knob range vs fronts kept target)
    std::printf ("III  attack sweep (knob target %s ms, fronts kept target %s %%)\n", range ("attackMs").toRawUTF8(), range ("frontsKeptPct").toRawUTF8());
    for (float a : { 0.3f, 1.0f, 3.0f, 5.0f, 10.0f, 15.0f, 20.0f, 30.0f, 45.0f, 70.0f, 100.0f })
    {
        s.attackMs = a;
        const auto r = run (audio, s);
        std::printf ("     attack %6.1f ms  fronts kept %5.1f %%  GR p90 %5.1f  -> %s\n", a, r.frontsKept, r.grP90,
                     str (vol4::Feedback::zoneFor ("attack", instrument, goal, asMetrics (r), s)));
    }
    s.attackMs = 0.5f * (float) ((double) goal["attackMs"]["min"] + (double) goal["attackMs"]["max"]);
    std::printf ("     using attack %.1f ms from here on\n\n", s.attackMs);

    // Chapter IV: release sweep
    std::printf ("IV   release sweep (knob target %s ms, residual GR max %s dB, ripple max %.0f dB)\n",
                 range ("releaseMs").toRawUTF8(), goal["residualGrMaxDb"].toString().toRawUTF8(), vol4::Feedback::rippleMaxDb);
    for (float rel : { 10.0f, 20.0f, 40.0f, 60.0f, 80.0f, 100.0f, 150.0f, 250.0f, 400.0f, 700.0f, 1000.0f })
    {
        s.releaseMs = rel;
        const auto r = run (audio, s);
        std::printf ("     release %6.0f ms  residual %4.1f dB  ripple %4.1f dB  -> %s\n", rel, r.residual, r.ripple,
                     str (vol4::Feedback::zoneFor ("release", instrument, goal, asMetrics (r), s)));
    }
    s.releaseMs = 0.5f * (float) ((double) goal["releaseMs"]["min"] + (double) goal["releaseMs"]["max"]);

    // Chapter V: makeup for level match
    std::printf ("\nV    makeup sweep\n");
    for (float mk = 0; mk <= 12; mk += 1.5f)
    {
        s.makeupDb = mk;
        const auto r = run (audio, s);
        std::printf ("     makeup %+5.1f dB  out-in %+5.1f dB  -> %s\n", mk, r.levelDiff,
                     str (vol4::Feedback::zoneFor ("level", instrument, goal, asMetrics (r), s)));
    }
    return 0;
}
