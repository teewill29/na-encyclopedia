// End-to-end tests of the real CompressionAudioProcessor: audio thread → taps →
// FIFO → 30 Hz analysis timer → metrics, plus layouts, state and parameter names.
// Run by ctest in CI. Set NA_SNAPSHOT=/path/to/file.png to also save an editor screenshot.

#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>

namespace
{
int failures = 0;
std::string currentTest;

void check (bool ok, const std::string& what)
{
    if (! ok) { ++failures; std::printf ("  FAIL [%s] %s\n", currentTest.c_str(), what.c_str()); }
}

void test (const std::string& name, const std::function<void()>& body)
{
    currentTest = name;
    const auto before = failures;
    body();
    std::printf ("%s %s\n", failures == before ? "ok  " : "FAIL", name.c_str());
}

constexpr double sr = 48000.0;
constexpr int block = 512;
constexpr double pi = 3.14159265358979323846;

void setParam (juce::AudioProcessor& p, const juce::String& id, float value)
{
    for (auto* prm : p.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (prm); ranged && ranged->getParameterID() == id)
            ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
}

/** Drum-like hits, 4 per second: 2 ms attack, exponential decay, 120 Hz. */
float hit (int64_t t)
{
    const double pos = std::fmod ((double) t / sr, 0.25);
    const double env = pos < 0.002 ? 0.9 * pos / 0.002 : 0.9 * std::exp (-(pos - 0.002) * 12.0);
    return (float) (env * std::sin (2.0 * pi * 120.0 * (double) t / sr));
}

/** Plays `seconds` of hits through the processor, letting the message loop
    (and so the analysis timer) run between blocks, as a host would. */
void play (CompressionAudioProcessor& p, double seconds, int inChannels, int64_t& t)
{
    juce::AudioBuffer<float> buffer (2, block);
    juce::MidiBuffer midi;
    for (int b = 0; b < (int) (seconds * sr / block); ++b)
    {
        buffer.clear();
        for (int i = 0; i < block; ++i, ++t)
            for (int ch = 0; ch < inChannels; ++ch)
                buffer.setSample (ch, i, hit (t));
        p.processBlock (buffer, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (2);
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;

    test ("parameter names and units come from the content file", [] {
        CompressionAudioProcessor p;
        juce::StringArray names;
        for (auto* prm : p.getParameters()) names.add (prm->getName (64));
        check (names.joinIntoString (",") == "Threshold,Ratio,Attack,Release,Makeup Gain,Mix", names.joinIntoString (",").toStdString());
        auto* attack = p.getParameters()[2];
        check (attack->getText (attack->getValueForText ("10"), 32) == "10 ms", "attack text: " + attack->getText (attack->getValueForText ("10"), 32).toStdString());
    });

    test ("audio reaches the analysis and every metric fills in", [] {
        CompressionAudioProcessor p;
        p.setPlayConfigDetails (2, 2, sr, block);
        p.prepareToPlay (sr, block);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100); // timer applies the sample rate
        setParam (p, "threshold", -30.0f);
        setParam (p, "attack", 5.0f);
        setParam (p, "release", 80.0f);

        int64_t t = 0;
        play (p, 5.0, 2, t);
        const auto& m = p.getMetrics();
        check (m.state == na::analysis::Metrics::State::evaluating, "state is evaluating");
        check (m.onsetCount >= 17 && m.onsetCount <= 21, "about 20 hits detected, got " + std::to_string (m.onsetCount));
        check (m.grP90Db > 3.0f, "compressing: GR p90 " + std::to_string (m.grP90Db));
        check (std::isfinite (m.frontsKeptPct) && m.frontsKeptCount == 8, "fronts kept measured: " + std::to_string (m.frontsKeptPct));
        check (std::isfinite (m.residualGrDb), "residual GR measured");
        check (std::isfinite (m.dynamicRangeDb), "dynamic range measured");
        check (m.levelDiffDb < -1.0f, "output quieter than input without makeup: " + std::to_string (m.levelDiffDb));
        check (p.getDroppedAnalysisBlocks() == 0, "no analysis blocks dropped");

        if (auto path = juce::SystemStats::getEnvironmentVariable ("NA_SNAPSHOT", {}); path.isNotEmpty())
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
            play (p, 0.5, 2, t);
            auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
            juce::File file (path);
            file.deleteFile();
            juce::FileOutputStream out (file);
            juce::PNGImageFormat().writeImageToStream (image, out);
            p.editorBeingDeleted (editor.get());
        }
        p.releaseResources();
    });

    test ("mono -> stereo: both outputs carry the compressed signal", [] {
        CompressionAudioProcessor p;
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::mono());
        layout.outputBuses.add (juce::AudioChannelSet::stereo());
        check (p.setBusesLayout (layout), "layout accepted");
        p.prepareToPlay (sr, block);
        setParam (p, "threshold", -30.0f);

        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;
        double energy = 0.0;
        bool identical = true;
        int64_t t = 0;
        for (int b = 0; b < 50; ++b)
        {
            buffer.clear();
            for (int i = 0; i < block; ++i, ++t) buffer.setSample (0, i, hit (t));
            buffer.setSample (1, 0, 123.0f); // junk on the unused input channel must be ignored
            p.processBlock (buffer, midi);
            for (int i = 0; i < block; ++i)
            {
                identical &= std::memcmp (buffer.getReadPointer (0) + i, buffer.getReadPointer (1) + i, sizeof (float)) == 0;
                energy += buffer.getSample (0, i) * buffer.getSample (0, i);
            }
        }
        check (identical, "left == right");
        check (energy > 1.0, "not silent");
    });

    test ("unsupported layouts are refused", [] {
        CompressionAudioProcessor p;
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        layout.outputBuses.add (juce::AudioChannelSet::mono());
        check (! p.checkBusesLayoutSupported (layout), "stereo -> mono refused");
        layout.inputBuses.getReference (0) = juce::AudioChannelSet::create5point1();
        layout.outputBuses.getReference (0) = juce::AudioChannelSet::create5point1();
        check (! p.checkBusesLayoutSupported (layout), "5.1 refused");
    });

    test ("settings survive save and reload", [] {
        juce::MemoryBlock state;
        {
            CompressionAudioProcessor p;
            setParam (p, "threshold", -24.0f);
            setParam (p, "ratio", 8.0f);
            setParam (p, "attack", 25.0f);
            setParam (p, "mix", 40.0f);
            p.getStateInformation (state);
        }
        CompressionAudioProcessor q;
        q.setStateInformation (state.getData(), (int) state.getSize());
        auto value = [&q] (const char* id) { return q.getValueTree().getRawParameterValue (id)->load(); };
        check (std::abs (value ("threshold") + 24.0f) < 0.05f, "threshold");
        check (std::abs (value ("ratio") - 8.0f) < 0.05f, "ratio");
        check (std::abs (value ("attack") - 25.0f) < 0.05f, "attack");
        check (std::abs (value ("mix") - 40.0f) < 0.5f, "mix");
    });

    std::printf ("\n%s: %d failure(s)\n", failures == 0 ? "PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
