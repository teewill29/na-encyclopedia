// End-to-end tests of the real CompressionAudioProcessor: audio thread → taps →
// FIFO → 30 Hz analysis timer → metrics, plus layouts, state and parameter names.
// Run by ctest in CI. Set NA_SNAPSHOT_DIR=/some/folder to also save editor screenshots.

#include "PluginProcessor.h"
#include "content/Template.h"
#include "Feedback.h"

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


    test ("feedback zones follow the spec for every chapter", [] {
        CompressionAudioProcessor p;
        const auto& c = p.getContent();
        const auto vox = c.findInstrument ("leadVocal"), kick = c.findInstrument ("kick");
        const auto punch = c.findGoal ("leadVocal", "punch");   // GR 3-6, fronts 70-90, release 50-100, residual <= 1
        na::analysis::Metrics m;
        na::vol4::CompressorSettings s;
        using na::vol4::Feedback::zoneFor;
        auto zone = [&] (const char* ch, const juce::var& inst = {}) { return zoneFor (ch, inst.isVoid() ? vox : inst, punch, m, s); };

        m.dynamicRangeDb = 3.0f;  check (zone ("goal") == "notNeeded", "vocal, 3 dB of movement: not needed (min 6)");
        m.dynamicRangeDb = 9.0f;  check (zone ("goal") == "needed", "9 dB: needed");
        check (zone ("goal", kick) == "skipped", "kick has no need check");
        m.dynamicRangeDb = na::analysis::Metrics::none; check (zone ("goal").empty(), "no reading yet");

        m.grP90Db = 1.0f; check (zone ("ratioThreshold") == "low", "GR 1 dB: not enough");
        m.grP90Db = 4.0f; check (zone ("ratioThreshold") == "onTarget", "GR 4 dB: on target");
        m.grP90Db = 9.0f; check (zone ("ratioThreshold") == "high", "GR 9 dB: too much");

        // Chapter III is judged by the knob against attackMs (15-30 ms here)
        s.attackMs = 5.0f;  check (zone ("attack") == "low", "attack 5 ms: too fast");
        s.attackMs = 20.0f; check (zone ("attack") == "onTarget", "attack 20 ms");
        s.attackMs = 60.0f; check (zone ("attack") == "high", "attack 60 ms: too slow");

        s.releaseMs = 70.0f; m.residualGrDb = 0.5f; m.grRippleDb = 1.0f;
        check (zone ("release") == "onTarget", "release 70 ms, recovered, smooth");
        m.residualGrDb = 2.5f; check (zone ("release") == "tooSlow", "2.5 dB left over: too slow");
        m.residualGrDb = 0.5f; s.releaseMs = 300.0f; check (zone ("release") == "tooSlow", "knob above the range: too slow");
        s.releaseMs = 30.0f; check (zone ("release") == "tooFast", "knob below the range: too fast");
        s.releaseMs = 70.0f; m.grRippleDb = 4.0f; check (zone ("release") == "tooFast", "4 dB ripple: too fast");

        m.levelDiffDb = 2.0f;  check (zone ("level") == "high", "+2 dB: too loud");
        m.levelDiffDb = -0.5f; check (zone ("level") == "onTarget", "-0.5 dB: matched");
        m.levelDiffDb = -3.0f; check (zone ("level") == "low", "-3 dB: too quiet");

        const auto smash = c.findGoal ("drumBus", "parallelSmash");   // mix 20-40
        s.mixPct = 10.0f; check (zoneFor ("blend", {}, smash, m, s) == "low", "mix 10%");
        s.mixPct = 30.0f; check (zoneFor ("blend", {}, smash, m, s) == "onTarget", "mix 30%");
    });

    test ("every zone has a status label and a message in the content file", [] {
        CompressionAudioProcessor p;
        const auto& c = p.getContent();
        const std::pair<const char*, const char*> zones[] = {
            { "goal", "needed" }, { "goal", "notNeeded" }, { "goal", "skipped" },
            { "ratioThreshold", "low" }, { "ratioThreshold", "high" }, { "ratioThreshold", "onTarget" },
            { "attack", "low" }, { "attack", "high" }, { "attack", "onTarget" },
            { "release", "tooFast" }, { "release", "tooSlow" }, { "release", "onTarget" },
            { "level", "low" }, { "level", "high" }, { "level", "onTarget" },
            { "blend", "low" }, { "blend", "high" }, { "blend", "onTarget" } };
        for (const auto& [chapter, zone] : zones)
        {
            const auto key = na::vol4::Feedback::messageGroupFor (chapter) + "." + zone;
            check (c.getString ("messages." + key).isNotEmpty(), ("message " + key).toStdString());
            check (c.getString ("book.reading.status." + key).isNotEmpty(), ("status label " + key).toStdString());
            check (na::vol4::Feedback::toneFor (chapter, zone) != na::vol4::Feedback::Tone::neutral, ("tone " + key).toStdString());
        }
        check (c.getString ("messages.silence").isNotEmpty() && c.getString ("book.reading.status.listening").isNotEmpty(), "silence/listening");
    });

    test ("live: fronts kept rises as attack slows", [] {
        float kept[3] = {};
        const float attacks[3] = { 0.3f, 5.0f, 50.0f };
        for (int k = 0; k < 3; ++k)
        {
            CompressionAudioProcessor p;
            p.setPlayConfigDetails (2, 2, sr, block);
            p.prepareToPlay (sr, block);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
            setParam (p, "threshold", -24.0f);
            setParam (p, "attack", attacks[k]);
            int64_t t = 0;
            play (p, 4.0, 2, t);
            kept[k] = p.getMetrics().frontsKeptPct;
            p.releaseResources();
        }
        check (kept[0] < 40.0f, "0.3 ms catches most of the front: " + std::to_string (kept[0]));
        check (kept[0] < kept[1] && kept[1] < kept[2], "monotonic: " + std::to_string (kept[0]) + " < " + std::to_string (kept[1]) + " < " + std::to_string (kept[2]));
        check (kept[2] > 85.0f, "50 ms lets most of it through: " + std::to_string (kept[2]));
    });

    test ("live: a chapter earns its check mark with audio playing", [] {
        CompressionAudioProcessor p;
        p.setPlayConfigDetails (2, 2, sr, block);
        p.prepareToPlay (sr, block);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        p.beginBook ("leadVocal", "punch");
        p.openChapter ("level");          // threshold 0 dB, makeup 0: output matches input
        int64_t t = 0;
        play (p, 3.0, 2, t);
        check (! p.getSession().isDone ("level"), "not after only 3 s (2 s listening + 2 s hold)");
        play (p, 3.0, 2, t);
        check (p.getFeedback().zone == "onTarget", "zone onTarget, got '" + p.getFeedback().zone + "'");
        check (p.getSession().isDone ("level"), "check mark");

        setParam (p, "makeup", 6.0f);     // now too loud
        play (p, 5.0, 2, t);
        check (p.getFeedback().zone == "high", "too loud after +6 dB makeup, got '" + p.getFeedback().zone + "'");
        check (p.getSession().isDone ("level"), "the check mark stays");
        p.releaseResources();
    });

    test ("playback stopped: The Reading stops listening (Logic stops calling the plugin)", [] {
        CompressionAudioProcessor p;
        p.setPlayConfigDetails (2, 2, sr, block);
        p.prepareToPlay (sr, block);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        p.beginBook ("leadVocal", "punch");
        p.openChapter ("level");
        int64_t t = 0;
        play (p, 4.0, 2, t);
        check (p.isListening(), "listening while audio plays");
        check (p.getFeedback().phase == na::FeedbackTracker::Phase::evaluating, "evaluating while audio plays");
        juce::MessageManager::getInstance()->runDispatchLoopUntil (600);   // no processBlock calls: stopped
        check (! p.isListening(), "not listening once audio stops arriving");
        check (p.getFeedback().phase == na::FeedbackTracker::Phase::silence, "status goes to silence");
        play (p, 0.5, 2, t);
        check (p.isListening(), "listening again when playback resumes");
        p.releaseResources();
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
        setParam (p, "threshold", -24.0f);
        play (p, 4.0, 2, t);
        snap (*editor, "3a-chapter-2.png");
        p.openChapter ("attack");
        play (p, 4.0, 2, t);
        snap (*editor, "3-chapter-3.png");
        p.openChapter ("release");
        play (p, 4.0, 2, t);
        snap (*editor, "3b-chapter-4.png");
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
