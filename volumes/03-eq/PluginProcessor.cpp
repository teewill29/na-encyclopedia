#include "PluginProcessor.h"
#include "content/Template.h"
#include "BinaryData.h"

#include <cmath>
#include <limits>

using namespace na::vol3;

namespace
{
    juce::String id (const char* what, int band) { return "band" + juce::String (band) + what; }

    // Per band: frequency range and defaults (docs/EQ.md §2)
    struct BandSpec { float minHz, maxHz, defaultHz, defaultQ; bool defaultOn; };
    const BandSpec bandSpecs[5] = {
        { 20.0f,   1000.0f,  20.0f,    0.707f, false },   // 1: low cut / low shelf
        { 40.0f,   2000.0f,  300.0f,   1.0f,   true  },   // 2: mud
        { 100.0f,  8000.0f,  1000.0f,  1.0f,   true  },   // 3: shaping
        { 500.0f,  16000.0f, 3500.0f,  2.0f,   true  },   // 4: harshness
        { 1000.0f, 20000.0f, 10000.0f, 0.707f, true  },   // 5: air
    };

    juce::NormalisableRange<float> logRange (float min, float max)
    {
        return { min, max,
                 [] (float s, float e, float t) { return s * std::pow (e / s, t); },
                 [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
                 [] (float s, float e, float v) { return juce::jlimit (s, e, v); } };
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout (const na::Content& c)
    {
        const auto hz = c.getString ("book.units.hz"), khz = c.getString ("book.units.khz"), db = c.getString ("book.units.db");
        auto name = [&c] (const juce::String& pid) { return c.getString ("parameters." + pid + ".name", pid); };
        auto freqText = [hz, khz] (float v, int) { return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + khz
                                                                     : juce::String (juce::roundToInt (v)) + hz; };
        auto dbText = [db] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + db; };
        auto parse = [] (const juce::String& t)
        {
            const auto v = t.retainCharacters ("0123456789.-").getFloatValue();
            return t.containsIgnoreCase ("k") ? v * 1000.0f : v;
        };

        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        for (int b = 1; b <= 5; ++b)
        {
            const auto& spec = bandSpecs[b - 1];
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id ("Freq", b), 1 }, name (id ("Freq", b)),
                logRange (spec.minHz, spec.maxHz), spec.defaultHz,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (freqText).withValueFromStringFunction (parse)));
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id ("Gain", b), 1 }, name (id ("Gain", b)),
                juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText).withValueFromStringFunction (parse)));
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id ("Q", b), 1 }, name (id ("Q", b)),
                logRange (0.3f, 8.0f), spec.defaultQ,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2); })
                                                     .withValueFromStringFunction (parse)));
            layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id ("On", b), 1 }, name (id ("On", b)), spec.defaultOn));
        }
        auto choices = [&c] (const char* pid)
        {
            juce::StringArray list;
            if (auto* arr = c.get (juce::String ("parameters.") + pid + ".choices").getArray())
                for (const auto& v : *arr) list.add (v.toString());
            return list;
        };
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "band1Shape", 1 }, name ("band1Shape"), choices ("band1Shape"), 0));
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "band5Shape", 1 }, name ("band5Shape"), choices ("band5Shape"), 0));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "output", 1 }, name ("output"),
            juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText).withValueFromStringFunction (parse)));
        return layout;
    }

    float num (const juce::var& v) { return v.isVoid() || v.isUndefined() ? std::numeric_limits<float>::quiet_NaN() : (float) (double) v; }
    float geoMid (const juce::var& range) { return std::sqrt (num (range["min"]) * num (range["max"])); }
}

EqProcessor::EqProcessor()
    : BookProcessor (BinaryData::eq_json, (size_t) BinaryData::eq_jsonSize, createLayout)
{
    for (int b = 1; b <= 5; ++b)
        bandParams[(size_t) b - 1] = { parameters.getRawParameterValue (id ("Freq", b)), parameters.getRawParameterValue (id ("Gain", b)),
                                       parameters.getRawParameterValue (id ("Q", b)),    parameters.getRawParameterValue (id ("On", b)) };
    band1Shape = parameters.getRawParameterValue ("band1Shape");
    band5Shape = parameters.getRawParameterValue ("band5Shape");
    output     = parameters.getRawParameterValue ("output");
    enableSpectrum();
}

// ── DSP ──────────────────────────────────────────────────────────────────────
EqSettings EqProcessor::readSettings() const noexcept
{
    EqSettings s;
    for (size_t b = 0; b < 5; ++b)
    {
        auto& band = s.bands[b];
        band.freqHz = bandParams[b].freq->load (std::memory_order_relaxed);
        band.gainDb = bandParams[b].gain->load (std::memory_order_relaxed);
        band.q      = bandParams[b].q->load (std::memory_order_relaxed);
        band.on     = bandParams[b].on->load (std::memory_order_relaxed) >= 0.5f;
        band.shape  = BandSettings::Shape::bell;
    }
    s.bands[0].shape = band1Shape->load (std::memory_order_relaxed) < 0.5f ? BandSettings::Shape::lowCut : BandSettings::Shape::lowShelf;
    s.bands[4].shape = band5Shape->load (std::memory_order_relaxed) < 0.5f ? BandSettings::Shape::highShelf : BandSettings::Shape::bell;
    s.outputDb = output->load (std::memory_order_relaxed);
    return s;
}

void EqProcessor::prepareDsp (double sampleRate)
{
    currentSampleRate = sampleRate;
    equalizer.prepare (sampleRate);
}

void EqProcessor::processChunk (float* const* channels, int numChannels, int numSamples,
                                const float* const*, const na::book::Taps& t) noexcept
{
    equalizer.setSettings (readSettings());
    equalizer.process (channels, numChannels, numSamples);
    for (int i = 0; i < numSamples; ++i)
        t.out[i] = numChannels == 2 ? 0.5f * (channels[0][i] + channels[1][i]) : channels[0][i];
}

// ── Chapters ─────────────────────────────────────────────────────────────────
int EqProcessor::bandFor (const juce::String& chapterId) const
{
    if (chapterId == "lowEnd") return 1;
    if (chapterId == "mud")    return 2;
    if (chapterId == "harsh")  return 4;
    if (chapterId == "shape")  { const auto b = (int) session.getGoal()["shapeBand"]; return b >= 1 && b <= 5 ? b : 3; }
    return 0;
}

void EqProcessor::setParam (const juce::String& pid, float value)
{
    if (auto* p = parameters.getParameter (pid))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void EqProcessor::chapterOpened (const juce::String& chapterId, bool firstVisit)
{
    if (! firstVisit) return;
    const auto instrument = session.getInstrument();
    const auto goal = session.getGoal();
    const int band = bandFor (chapterId);

    // Start values on the first visit (docs/EQ.md §5)
    if (chapterId == "lowEnd")
    {
        setParam ("band1Shape", 0.0f);                                   // low cut
        setParam (id ("On", 1), 1.0f);
        setParam (id ("Freq", 1), num (instrument["lowCutHz"]["start"]));
    }
    else if (chapterId == "mud" || chapterId == "harsh")
    {
        const auto range = instrument[chapterId == "mud" ? "mudHz" : "harshHz"];
        setParam (id ("On", band), 1.0f);
        setParam (id ("Freq", band), geoMid (range));
        setParam (id ("Gain", band), 0.0f);
        setParam (id ("Q", band), chapterId == "mud" ? 1.4f : 3.0f);
    }
    else if (chapterId == "shape")
    {
        setParam (id ("On", band), 1.0f);
        setParam (id ("Freq", band), geoMid (goal["shapeHz"]));
        setParam (id ("Gain", band), 0.0f);
        setParam (id ("Q", band), 0.9f);
    }
}

const char* EqProcessor::knobParamFor (const juce::String& chapterId) const
{
    static const juce::String freqIds[6] = { {}, "band1Freq", "band2Freq", "band3Freq", "band4Freq", "band5Freq" };
    if (chapterId == "level") return "output";
    const int band = bandFor (chapterId);
    return band > 0 ? freqIds[band].toRawUTF8() : nullptr;
}

juce::StringArray EqProcessor::extraKnobsFor (const juce::String& chapterId) const
{
    const int band = bandFor (chapterId);
    if (band < 2) return {};                 // the low cut has one knob
    return { id ("Gain", band), id ("Q", band) };
}

juce::String EqProcessor::targetCaptionFor (const juce::String& chapterId) const
{
    const auto instrument = session.getInstrument();
    juce::var range;
    if (chapterId == "lowEnd")     range = instrument["lowCutHz"];
    else if (chapterId == "mud")   range = instrument["mudHz"];
    else if (chapterId == "harsh") range = instrument["harshHz"];
    else if (chapterId == "shape") range = session.getGoal()["shapeHz"];
    if (! range.isObject()) return {};

    auto hzText = [this] (float v) { return v >= 1000.0f ? na::Template::formatNumber (v / 1000.0) + content.getString ("book.units.khz")
                                                         : na::Template::formatNumber (v) + content.getString ("book.units.hz"); };
    return na::Template::fill (content.getString ("book.chapterPage.target"),
                               na::Template::forValues ({ { "min", hzText (num (range["min"])) }, { "max", hzText (num (range["max"])) }, { "unit", {} } }));
}

bool EqProcessor::spectrumFor (const juce::String& chapterId, na::book::SpectrumData& out) const
{
    if (chapterId == "level" || chapterId.isEmpty()) return false;

    // The analyser's 1/6-octave bands from 20 Hz to 20 kHz: the measured track
    // before and after the EQ, and the EQ curve at the same points.
    using Analyser = na::analysis::SpectrumAnalyser;
    const auto settings = readSettings();
    const bool measured = spectrum.hasReading();
    out.freqs.clear(); out.curveDb.clear(); out.inDb.clear(); out.outDb.clear();
    for (int b = 0; b < Analyser::numBands; ++b)
    {
        const float f = Analyser::bandCentreHz (b);
        if (f < 19.9f || f > 20000.0f) continue;
        out.freqs.push_back (f);
        out.curveDb.push_back (Equalizer::responseDb (settings, f, currentSampleRate) - settings.outputDb);
        if (measured)
        {
            out.inDb.push_back (spectrum.levelsDb (Analyser::Stream::in)[(size_t) b]);
            out.outDb.push_back (spectrum.levelsDb (Analyser::Stream::out)[(size_t) b]);
        }
    }

    const auto instrument = session.getInstrument();
    juce::var shade;
    if (chapterId == "lowEnd")                              shade = instrument["lowCutHz"];
    else if (chapterId == "mud" || chapterId == "listen")   shade = instrument["mudHz"];
    else if (chapterId == "harsh")                          shade = instrument["harshHz"];
    else if (chapterId == "shape")                          shade = session.getGoal()["shapeHz"];
    if (shade.isObject()) { out.shadeLowHz = num (shade["min"]); out.shadeHighHz = num (shade["max"]); }
    return true;
}

juce::StringArray EqProcessor::setSoFarParamsFor (const juce::String& chapterId) const
{
    const int band = bandFor (chapterId);
    if (band >= 2) return { id ("Freq", band), id ("Gain", band) };
    return BookProcessor::setSoFarParamsFor (chapterId);
}

// ── Measurements ─────────────────────────────────────────────────────────────
EqProcessor::Reading EqProcessor::reading() const
{
    using Stream = na::analysis::SpectrumAnalyser::Stream;
    const auto instrument = session.getInstrument();
    const auto mud = instrument["mudHz"], harsh = instrument["harshHz"];
    const float lowCutMin = num (instrument["lowCutHz"]["min"]);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    if (! instrument.isObject()) return { nan, nan, nan, nan, nan, nan };

    return { spectrum.buildUpDb (Stream::in,  num (mud["min"]),   num (mud["max"])),
             spectrum.buildUpDb (Stream::out, num (mud["min"]),   num (mud["max"])),
             spectrum.buildUpDb (Stream::in,  num (harsh["min"]), num (harsh["max"])),
             spectrum.buildUpDb (Stream::out, num (harsh["min"]), num (harsh["max"])),
             spectrum.rumbleDb (Stream::in,  lowCutMin),
             spectrum.rumbleDb (Stream::out, lowCutMin) };
}

std::vector<std::pair<juce::String, juce::String>> EqProcessor::debugRows() const
{
    const auto instrument = session.getInstrument();
    auto hzText = [this] (const juce::var& v)
    {
        const float f = num (v);
        return std::isnan (f) ? content.getString ("debug.none")
             : f >= 1000.0f   ? na::Template::formatNumber (f / 1000.0) + content.getString ("book.units.khz")
                              : na::Template::formatNumber (f) + content.getString ("book.units.hz");
    };
    auto label = [&] (const char* key, const juce::var& range)
    {
        return na::Template::fill (content.getString (juce::String ("debug.labels.") + key, key),
                                   na::Template::forValues ({ { "min", hzText (range["min"]) }, { "max", hzText (range["max"]) } }));
    };
    auto db = [this] (float v)
    {
        return std::isnan (v) ? content.getString ("debug.none")
                              : (v > 0.0f ? "+" : "") + juce::String (v, 1) + content.getString ("debug.units.db");
    };

    const auto r = reading();
    return {
        { label ("spectrumSeconds", {}), juce::String (spectrum.getActiveSeconds(), 1) + content.getString ("debug.units.seconds") },
        { label ("mudIn", instrument["mudHz"]),       db (r.mudIn) },
        { label ("mudOut", instrument["mudHz"]),      db (r.mudOut) },
        { label ("harshIn", instrument["harshHz"]),   db (r.harshIn) },
        { label ("harshOut", instrument["harshHz"]),  db (r.harshOut) },
        { label ("rumbleIn", instrument["lowCutHz"]),  db (r.rumbleIn) },
        { label ("rumbleOut", instrument["lowCutHz"]), db (r.rumbleOut) },
    };
}

// Feedback rules arrive in step 3 (docs/EQ.md §7).
std::string EqProcessor::zoneFor (const juce::String&) const                               { return {}; }
na::book::Tone EqProcessor::toneFor (const juce::String&, const std::string&) const      { return na::book::Tone::neutral; }
std::vector<na::book::Meter> EqProcessor::metersFor (const juce::String&) const          { return {}; }

// Called by the plugin wrappers to create a new instance.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EqProcessor();
}
