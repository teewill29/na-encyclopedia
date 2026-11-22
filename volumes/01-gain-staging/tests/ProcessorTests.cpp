// End-to-end tests of GainStagingProcessor (Vol. I). Run by ctest in CI.
// Set NA_SNAPSHOT_DIR=/some/folder to also save editor screenshots.

#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>

namespace
{
int failures = 0;
std::string currentTest;
void check (bool ok, const std::string& what) { if (! ok) { ++failures; std::printf ("  FAIL [%s] %s\n", currentTest.c_str(), what.c_str()); } }
void test (const std::string& name, const std::function<void()>& body)
{
    currentTest = name; const auto before = failures; body();
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

float tone (int64_t t) { return (float) (0.25 * std::sin (2.0 * pi * 220.0 * (double) t / sr)); }

void play (GainStagingProcessor& p, double seconds, int64_t& t)
{
    juce::AudioBuffer<float> buffer (2, block);
    juce::MidiBuffer midi;
    for (int b = 0; b < (int) (seconds * sr / block); ++b)
    {
        for (int i = 0; i < block; ++i, ++t)
            for (int ch = 0; ch < 2; ++ch) buffer.setSample (ch, i, tone (t));
        p.processBlock (buffer, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (2);
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;

    test ("one parameter, Trim, named from the content file", [] {
        GainStagingProcessor p;
        check (p.getParameters().size() == 1 && p.getParameters()[0]->getName (32) == "Trim", "Trim");
        auto* trim = p.getParameters()[0];
        check (trim->getText (trim->getValueForText ("6"), 32) == "+6.0 dB", "text: " + trim->getText (trim->getValueForText ("6"), 32).toStdString());
    });

    test ("trim changes the level by exactly that much, without clicks", [] {
        GainStagingProcessor p;
        p.setPlayConfigDetails (2, 2, sr, block);
        p.prepareToPlay (sr, block);
        setParam (p, "trim", 6.0f);
        // A steady signal, so the gain can be read at every sample.
        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;
        double worstStep = 0.0;
        float last = 0.0f;
        for (int b = 0; b < 200; ++b)
        {
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < block; ++i) buffer.setSample (ch, i, 0.25f);
            if (b == 100) setParam (p, "trim", -12.0f);   // a big move mid-stream
            p.processBlock (buffer, midi);
            for (int i = 0; i < block; ++i)
            {
                const float gain = buffer.getSample (0, i) / 0.25f;
                if (b > 0 || i > 0) worstStep = std::max (worstStep, (double) std::abs (gain - last));
                last = gain;
            }
            if (b == 99)  check (std::abs (last - std::pow (10.0f, 6.0f / 20.0f)) < 1.0e-3f, "+6 dB = x2");
        }
        check (std::abs (last - std::pow (10.0f, -12.0f / 20.0f)) < 1.0e-3f, "-12 dB after the move");
        check (worstStep < 0.01, "gain glides, largest step " + std::to_string (worstStep));
    });

    test ("four chapters; Trim is the knob in II-IV, none in I", [] {
        GainStagingProcessor p;
        p.beginBook ("leadVocal", "mix");
        const auto& ids = p.getSession().getChapterIds();
        check (ids.joinIntoString (",") == "where,peaks,average,headroom", ids.joinIntoString (",").toStdString());
        check (p.knobParamFor ("where") == nullptr, "no knob in Chapter I");
        for (auto* c : { "peaks", "average", "headroom" })
            check (juce::String (p.knobParamFor (c)) == "trim", std::string ("trim in ") + c);
        check (p.targetCaptionFor ("peaks") == juce::String ("Target: -10 to -6 dBFS"),
               "peaks caption: " + p.targetCaptionFor ("peaks").toStdString());
        check (! p.hasListeningTools(), "no listening tools in Vol. I");
    });

    test ("every instrument has a goal with peak and average targets", [] {
        GainStagingProcessor p;
        for (const auto& instrument : *p.getContent().instruments())
            for (const auto& goal : *instrument["goals"].getArray())
                check (goal["peakDb"].isObject() && goal["averageDb"].isObject() && goal.hasProperty ("headroomMinDb"),
                       instrument["id"].toString().toStdString());
    });

    test ("mono -> stereo works", [] {
        GainStagingProcessor p;
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::mono());
        layout.outputBuses.add (juce::AudioChannelSet::stereo());
        check (p.setBusesLayout (layout), "layout accepted");
        p.prepareToPlay (sr, block);
        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;
        for (int i = 0; i < block; ++i) buffer.setSample (0, i, tone (i));
        p.processBlock (buffer, midi);
        check (std::abs (buffer.getSample (1, 100) - buffer.getSample (0, 100)) < 1.0e-9f && std::abs (buffer.getSample (0, 100)) > 1.0e-6f, "left == right, not silent");
    });

    test ("trim and the book are saved with the project", [] {
        juce::MemoryBlock state;
        {
            GainStagingProcessor p;
            p.beginBook ("bass", "mix");
            p.openChapter ("average");
            setParam (p, "trim", -7.5f);
            p.getStateInformation (state);
        }
        GainStagingProcessor q;
        q.setStateInformation (state.getData(), (int) state.getSize());
        check (q.getSession().getInstrumentId() == "bass" && q.getSession().getCurrentChapterId() == "average", "session");
        check (std::abs (q.getValueTree().getRawParameterValue ("trim")->load() + 7.5f) < 0.05f, "trim");
    });

    if (auto dir = juce::SystemStats::getEnvironmentVariable ("NA_SNAPSHOT_DIR", {}); dir.isNotEmpty())
    {
        auto snap = [&dir] (juce::Component& c, const char* name)
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
            auto image = c.createComponentSnapshot (c.getLocalBounds(), true, 2.0f);
            juce::File file = juce::File (dir).getChildFile (name);
            file.deleteFile();
            juce::FileOutputStream out (file);
            juce::PNGImageFormat().writeImageToStream (image, out);
        };
        GainStagingProcessor p;
        p.setPlayConfigDetails (2, 2, sr, block);
        p.prepareToPlay (sr, block);
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        snap (*editor, "v1-title-page.png");
        p.beginBook ("leadVocal", "mix");
        int64_t t = 0;
        play (p, 3.0, t);
        snap (*editor, "v1-chapter-1.png");
        p.openChapter ("peaks");
        snap (*editor, "v1-chapter-2.png");
        p.editorBeingDeleted (editor.get());
        editor.reset();
    }

    std::printf ("\n%s: %d failure(s)\n", failures == 0 ? "PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
