// End-to-end tests of the real CompressionAudioProcessor: audio thread → taps →
// FIFO → 30 Hz analysis timer → metrics, plus layouts, state and parameter names.
// Run by ctest in CI. Set NA_SNAPSHOT_DIR=/some/folder to also save editor screenshots.

#include "PluginProcessor.h"
#include "content/Template.h"

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


    test ("every chapter of every goal fills in (no {placeholders} left)", [] {
        CompressionAudioProcessor p;
        const auto& content = p.getContent();
        int checked = 0;
        for (const auto& instrument : *content.instruments())
            for (const auto& goal : *instrument["goals"].getArray())
            {
                const auto where = (instrument["id"].toString() + "/" + goal["id"].toString()).toStdString();
                for (const auto& id : content.chapterIdsFor (goal))
                {
                    const auto chapter = content.findChapter (id);
                    for (const char* field : { "intro", "subtitle", "doThis", "doThisFromFast", "doThisFromSlow", "listenFor", "tryThis" })
                    {
                        if (! chapter.hasProperty (field)) continue;
                        const auto text = na::Template::fill (chapter[field].toString(), na::Template::forGoal (goal));
                        check (! text.containsChar ('{') && text.isNotEmpty(), where + " " + id.toStdString() + "." + field + ": " + text.toStdString());
                        ++checked;
                    }
                }
                const auto approach = goal["attackMs"]["approach"].toString();
                check (approach == "fromFast" || approach == "fromSlow", where + " attackMs.approach");
                check (goal["name"].toString().isNotEmpty() && goal["tagline"].toString().isNotEmpty(), where + " name and tagline");
            }
        check (checked > 100, "checked " + std::to_string (checked) + " strings");
    });

    test ("chapters set their start values once, on the first visit", [] {
        CompressionAudioProcessor p;
        auto value = [&p] (const char* id) { return p.getValueTree().getRawParameterValue (id)->load(); };
        p.beginBook ("leadVocal", "punch");          // ratio 4, attack 5 (fromFast), release 250
        check (p.getSession().getCurrentChapterId() == "goal", "opens Chapter I");
        p.nextChapter();
        check (std::abs (value ("ratio") - 4.0f) < 0.05f, "Chapter II sets the goal's ratio");
        p.nextChapter();
        check (std::abs (value ("attack") - 5.0f) < 0.05f, "Chapter III sets attack.start");
        setParam (p, "attack", 22.0f);               // the user dials it in
        p.nextChapter();
        check (std::abs (value ("release") - 250.0f) < 0.5f, "Chapter IV sets release.start");
        p.previousChapter();
        check (std::abs (value ("attack") - 22.0f) < 0.05f, "coming back keeps the user's attack");
        check (p.getSession().getChapterIds().size() == 5 && ! p.getSession().getChapterIds().contains ("blend"), "5 chapters, no Blend");
    });

    test ("parallel goal: Blend chapter, Mix forced to 100% in II-IV", [] {
        CompressionAudioProcessor p;
        p.beginBook ("drumBus", "parallelSmash");
        const auto& ids = p.getSession().getChapterIds();
        check (ids.size() == 6 && ids[5] == "blend", "6 chapters ending in Blend");
        check (! p.isMixForcedToFull(), "not forced in Chapter I");
        p.openChapter ("ratioThreshold"); check (p.isMixForcedToFull(), "forced in II");
        p.openChapter ("attack");         check (p.isMixForcedToFull(), "forced in III");
        p.openChapter ("release");        check (p.isMixForcedToFull(), "forced in IV");
        p.openChapter ("level");          check (! p.isMixForcedToFull(), "not forced in V");
        p.openChapter ("blend");
        check (! p.isMixForcedToFull(), "not forced in Blend");
        check (std::abs (p.getValueTree().getRawParameterValue ("mix")->load() - 0.0f) < 0.5f, "Blend starts Mix at mixPct.start");

        CompressionAudioProcessor q;
        q.beginBook ("leadVocal", "punch");
        q.openChapter ("attack");
        check (! q.isMixForcedToFull(), "normal goals never force Mix");
    });

    test ("the book is saved with the project", [] {
        juce::MemoryBlock state;
        {
            CompressionAudioProcessor p;
            p.beginBook ("bass", "punchy");
            p.openChapter ("attack");
            p.getSession().setDone ("goal", true);
            p.getStateInformation (state);
        }
        CompressionAudioProcessor q;
        q.setStateInformation (state.getData(), (int) state.getSize());
        const auto& s = q.getSession();
        check (s.getInstrumentId() == "bass" && s.getGoalId() == "punchy", "setup");
        check (s.getCurrentChapterId() == "attack" && ! s.isOnTitlePage(), "chapter");
        check (s.isDone ("goal"), "progress");
        check (std::abs (q.getValueTree().getRawParameterValue ("attack")->load() - 5.0f) < 0.05f, "parameters too");

        juce::MemoryBlock again;   // saving twice doesn't pile up copies
        q.getStateInformation (again);
        auto xml = juce::AudioProcessor::getXmlFromBinary (again.getData(), (int) again.getSize());
        int sessions = 0;
        for (auto* child : xml->getChildIterator()) sessions += child->hasTagName ("Session") ? 1 : 0;
        check (sessions == 1, "one session in the saved state");
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
        CompressionAudioProcessor p;
        p.setPlayConfigDetails (2, 2, sr, block);
        p.prepareToPlay (sr, block);
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        snap (*editor, "1-title-page.png");
        p.beginBook ("leadVocal", "punch");
        int64_t t = 0;
        play (p, 3.0, 2, t);
        snap (*editor, "2-chapter-1.png");
        p.openChapter ("ratioThreshold");
        p.openChapter ("attack");
        snap (*editor, "3-chapter-3.png");
        p.beginBook ("electricDistorted", "lightTouch");
        p.getSession().showTitlePage();
        snap (*editor, "4-title-lesson.png");
        p.beginBook ("drumBus", "parallelSmash");
        p.openChapter ("blend");
        snap (*editor, "5-blend.png");
        p.editorBeingDeleted (editor.get());
        editor.reset();
        p.releaseResources();
    }

    std::printf ("\n%s: %d failure(s)\n", failures == 0 ? "PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
