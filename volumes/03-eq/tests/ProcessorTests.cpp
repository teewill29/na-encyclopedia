// End-to-end tests of EqProcessor (Vol. III). Run by ctest in CI.
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

constexpr double sr = 48000.0, pi = 3.14159265358979323846;
constexpr int block = 512;

void setParam (juce::AudioProcessor& p, const juce::String& id, float value)
{
    for (auto* prm : p.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (prm); ranged && ranged->getParameterID() == id)
            ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
}
float value (EqProcessor& p, const char* id) { return p.getValueTree().getRawParameterValue (id)->load(); }

/** Level change (dB) the plugin applies to a sine at hz. */
double gainAt (EqProcessor& p, double hz)
{
    p.prepareToPlay (sr, block);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    double sumIn = 0, sumOut = 0;
    for (int b = 0; b < 100; ++b)
    {
        for (int i = 0; i < block; ++i)
            for (int ch = 0; ch < 2; ++ch) buf.setSample (ch, i, (float) (0.25 * std::sin (2.0 * pi * hz * (b * block + i) / sr)));
        p.processBlock (buf, midi);
        if (b < 50) continue;
        for (int i = 0; i < block; ++i)
        {
            const double x = 0.25 * std::sin (2.0 * pi * hz * (b * block + i) / sr);
            sumIn += x * x; sumOut += (double) buf.getSample (0, i) * buf.getSample (0, i);
        }
    }
    return 10.0 * std::log10 (sumOut / sumIn);
}

/** Pink noise with a +6 dB bump at 300 Hz (a "muddy" track), deterministic. */
struct MuddyNoise
{
    juce::Random rng { 7 };
    double b[7] {}, z1 = 0, z2 = 0;
    double c0 = 0, c1 = 0, c2 = 0, d1 = 0, d2 = 0;
    MuddyNoise()
    {
        const double A = std::pow (10.0, 6.0 / 40.0), w0 = 2.0 * pi * 300.0 / sr, alpha = std::sin (w0) / (2.0 * 1.4);
        const double a0 = 1 + alpha / A;
        c0 = (1 + alpha * A) / a0; c1 = -2 * std::cos (w0) / a0; c2 = (1 - alpha * A) / a0;
        d1 = -2 * std::cos (w0) / a0; d2 = (1 - alpha / A) / a0;
    }
    float next()
    {
        const double in = rng.nextFloat() * 2.0 - 1.0;
        b[0] = 0.99886 * b[0] + in * 0.0555179;  b[1] = 0.99332 * b[1] + in * 0.0750759;
        b[2] = 0.96900 * b[2] + in * 0.1538520;  b[3] = 0.86650 * b[3] + in * 0.3104856;
        b[4] = 0.55000 * b[4] + in * 0.5329522;  b[5] = -0.7616 * b[5] - in * 0.0168980;
        const double pinkValue = (b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + in * 0.5362) * 0.05;
        b[6] = in * 0.115926;
        const double y = c0 * pinkValue + z1;
        z1 = c1 * pinkValue - d1 * y + z2;
        z2 = c2 * pinkValue - d2 * y;
        return (float) y;
    }
};

/** Plays `seconds` of the muddy track with the message loop running, like a host. */
void playMuddy (EqProcessor& p, MuddyNoise& noise, double seconds)
{
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    for (int b = 0; b < (int) (seconds * sr / block); ++b)
    {
        for (int i = 0; i < block; ++i) { const float x = noise.next(); buf.setSample (0, i, x); buf.setSample (1, i, x); }
        p.processBlock (buf, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;

    test ("parameters: five bands, shapes and output, named from the content file", [] {
        EqProcessor p;
        check (p.getParameters().size() == 5 * 4 + 3, std::to_string (p.getParameters().size()) + " parameters");
        auto* f = p.getValueTree().getParameter ("band4Freq");
        check (f->getName (32) == "Band 4 Frequency", f->getName (32).toStdString());
        check (f->getText (f->convertTo0to1 (3500.0f), 32) == "3.50 kHz", f->getText (f->convertTo0to1 (3500.0f), 32).toStdString());
        check (p.getValueTree().getParameter ("band2Freq")->getText (p.getValueTree().getParameter ("band2Freq")->convertTo0to1 (300.0f), 32) == "300 Hz", "300 Hz");
        auto* shape = p.getValueTree().getParameter ("band1Shape");
        check (shape->getText (0.0f, 32) == "Low Cut" && shape->getText (1.0f, 32) == "Low Shelf", "band 1 shapes");
    });

    test ("out of the box the EQ is flat", [] {
        EqProcessor p;
        for (double hz : { 50.0, 1000.0, 12000.0 })
            check (std::abs (gainAt (p, hz)) < 0.02, "flat at " + std::to_string ((int) hz) + " Hz");
    });

    test ("the plugin applies the bands", [] {
        EqProcessor p;
        setParam (p, "band3Gain", 6.0f);       // band 3 at 1 kHz
        check (std::abs (gainAt (p, 1000.0) - 6.0) < 0.1, "+6 dB at 1 kHz");
        setParam (p, "band1On", 1.0f);
        setParam (p, "band1Freq", 200.0f);
        check (gainAt (p, 50.0) < -20.0, "low cut at 200 Hz removes 50 Hz");
        setParam (p, "band3Gain", 0.0f);
        setParam (p, "output", -6.0f);
        check (std::abs (gainAt (p, 5000.0) + 6.0) < 0.05, "output -6 dB");
    });

    test ("chapters, knobs and captions", [] {
        EqProcessor p;
        p.beginBook ("leadVocal", "clear");
        check (p.getSession().getChapterIds().joinIntoString (",") == "listen,lowEnd,mud,harsh,shape,level",
               p.getSession().getChapterIds().joinIntoString (",").toStdString());
        check (p.knobParamFor ("listen") == nullptr, "no knob in Chapter I");
        check (juce::String (p.knobParamFor ("lowEnd")) == "band1Freq" && p.extraKnobsFor ("lowEnd").isEmpty(), "low cut: one knob");
        check (juce::String (p.knobParamFor ("mud")) == "band2Freq" && p.extraKnobsFor ("mud").joinIntoString (",") == "band2Gain,band2Q", "mud: three knobs");
        check (juce::String (p.knobParamFor ("harsh")) == "band4Freq", "harsh: band 4");
        check (juce::String (p.knobParamFor ("shape")) == "band3Freq", "shape: the goal's band (3 for Clear & Present)");
        check (juce::String (p.knobParamFor ("level")) == "output", "level: output");
        check (p.targetCaptionFor ("mud") == "Target: 200 Hz to 500 Hz", p.targetCaptionFor ("mud").toStdString());
        check (p.targetCaptionFor ("harsh") == "Target: 2.5 kHz to 5 kHz", p.targetCaptionFor ("harsh").toStdString());
    });

    test ("start values on the first visit, kept on return", [] {
        EqProcessor p;
        p.beginBook ("leadVocal", "clear");
        p.openChapter ("lowEnd");
        check (value (p, "band1On") > 0.5f && std::abs (value (p, "band1Freq") - 30.0f) < 0.5f, "low cut on at 30 Hz");
        p.openChapter ("mud");
        check (std::abs (value (p, "band2Freq") - std::sqrt (200.0f * 500.0f)) < 2.0f, "mud band in the middle of its range");
        setParam (p, "band2Gain", -4.0f);
        p.openChapter ("harsh");
        p.openChapter ("mud");
        check (std::abs (value (p, "band2Gain") + 4.0f) < 0.05f, "your cut is kept when you come back");
    });

    test ("every chapter's text fills in for every goal", [] {
        EqProcessor p;
        int checked = 0;
        for (const auto& instrument : *p.getContent().instruments())
            for (const auto& goal : *instrument["goals"].getArray())
            {
                p.beginBook (instrument["id"].toString(), goal["id"].toString());
                for (const auto& chapterId : p.getSession().getChapterIds())
                {
                    const auto chapter = p.getContent().findChapter (chapterId);
                    for (const char* field : { "intro", "subtitle", "doThis", "listenFor", "tryThis" })
                        if (chapter.hasProperty (field))
                        {
                            const auto text = na::Template::fill (chapter[field].toString(), p.templateLookup());
                            check (! text.containsChar ('{') && text.isNotEmpty(),
                                   (instrument["id"].toString() + "/" + goal["id"].toString() + " " + chapterId + "." + field + ": " + text).toStdString());
                            ++checked;
                        }
                }
            }
        check (checked > 300, std::to_string (checked) + " strings");
    });

    test ("the spectrum view shows the live EQ curve and the problem range", [] {
        EqProcessor p;
        p.prepareToPlay (sr, block);
        p.beginBook ("leadVocal", "clear");
        p.openChapter ("mud");
        setParam (p, "band2Gain", -6.0f);
        na::book::SpectrumData d;
        check (p.spectrumFor ("mud", d), "mud has a spectrum");
        check (std::abs (d.shadeLowHz - 200.0f) < 0.01f && std::abs (d.shadeHighHz - 500.0f) < 0.01f, "mud range shaded");
        float deepest = 0.0f, deepestHz = 0.0f;
        for (size_t i = 0; i < d.freqs.size(); ++i) if (d.curveDb[i] < deepest) { deepest = d.curveDb[i]; deepestHz = d.freqs[i]; }
        check (deepest < -5.5f && deepestHz > 250.0f && deepestHz < 400.0f, "curve dips ~6 dB near 316 Hz: " + std::to_string (deepest) + " at " + std::to_string (deepestHz));
        check (! p.spectrumFor ("level", d), "no spectrum in Level & Compare");
    });

    test ("the analysis: a muddy track reads as mud on `in`, and cutting it with band 2 clears `out`", [] {
        EqProcessor p;
        p.setPlayConfigDetails (2, 2, sr, block);
        p.prepareToPlay (sr, block);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        p.beginBook ("leadVocal", "clear");          // mud 200-500 Hz, harsh 2.5-5 kHz
        MuddyNoise noise;
        check (std::isnan (p.reading().mudIn), "no reading before audio");

        playMuddy (p, noise, 4.0);
        auto r = p.reading();
        check (r.mudIn > 2.5f, "mud build-up on `in`: " + std::to_string (r.mudIn));
        check (std::abs (r.mudOut - r.mudIn) < 0.3f, "flat EQ: out reads the same: " + std::to_string (r.mudOut));
        check (std::abs (r.harshIn) < 1.0f, "no harshness: " + std::to_string (r.harshIn));

        na::book::SpectrumData d;
        p.openChapter ("listen");
        check (p.spectrumFor ("listen", d) && d.inDb.size() == d.freqs.size() && d.outDb.size() == d.freqs.size(),
               "The Reading gets the measured in/out spectrum");

        p.openChapter ("mud");
        setParam (p, "band2Freq", 300.0f);
        setParam (p, "band2Q", 1.4f);
        setParam (p, "band2Gain", -6.0f);
        playMuddy (p, noise, 4.0);
        r = p.reading();
        check (r.mudIn > 2.5f, "`in` still muddy: " + std::to_string (r.mudIn));
        check (std::abs (r.mudOut) < 1.0f, "`out` cleared by the cut: " + std::to_string (r.mudOut));
        check (std::isfinite (r.rumbleIn) && r.rumbleIn < 0.0f, "rumble is measured: " + std::to_string (r.rumbleIn));

        p.openChapter ("lowEnd");                    // low cut on at the instrument's start value (30 Hz)
        setParam (p, "band1Freq", 100.0f);
        playMuddy (p, noise, 4.0);
        r = p.reading();
        check (r.rumbleOut < r.rumbleIn - 10.0f, "a 100 Hz low cut takes the rumble below 80 Hz down: "
                                                   + std::to_string (r.rumbleIn) + " -> " + std::to_string (r.rumbleOut));
        check (p.debugRows().size() == 7, "debug rows");
    });

    test ("settings and the book are saved with the project", [] {
        juce::MemoryBlock state;
        {
            EqProcessor p;
            p.beginBook ("snare", "crack");
            p.openChapter ("harsh");
            setParam (p, "band4Gain", -3.5f);
            setParam (p, "band5Shape", 1.0f);
            p.getStateInformation (state);
        }
        EqProcessor q;
        q.setStateInformation (state.getData(), (int) state.getSize());
        check (q.getSession().getGoalId() == "crack" && q.getSession().getCurrentChapterId() == "harsh", "session");
        check (std::abs (value (q, "band4Gain") + 3.5f) < 0.05f && value (q, "band5Shape") > 0.5f, "parameters");
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
        EqProcessor p;
        p.setPlayConfigDetails (2, 2, sr, block);
        p.prepareToPlay (sr, block);
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        snap (*editor, "v3-title-page.png");
        p.beginBook ("leadVocal", "clear");
        MuddyNoise noise;
        playMuddy (p, noise, 3.0);
        snap (*editor, "v3-chapter-1.png");
        p.openChapter ("lowEnd");
        setParam (p, "band1Freq", 100.0f);
        snap (*editor, "v3-chapter-2.png");
        p.openChapter ("mud");
        setParam (p, "band2Gain", -5.0f);
        playMuddy (p, noise, 3.5);
        snap (*editor, "v3-chapter-3.png");
        p.openChapter ("shape");
        setParam (p, "band3Gain", 3.0f);
        snap (*editor, "v3-chapter-5.png");
        p.editorBeingDeleted (editor.get());
        editor.reset();
    }

    std::printf ("\n%s: %d failure(s)\n", failures == 0 ? "PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
