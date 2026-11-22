#include "Spectrum.h"

#include <cmath>

namespace na::ui
{

namespace
{
    constexpr float loHz = 20.0f, hiHz = 20000.0f;
    constexpr float floorDb = -90.0f, topDb = -6.0f, curveRangeDb = 18.0f;

    float xFor (float hz, juce::Rectangle<float> r)
    {
        const float t = std::log (juce::jlimit (loHz, hiHz, hz) / loHz) / std::log (hiHz / loHz);
        return r.getX() + t * r.getWidth();
    }

    juce::Path curvePath (const std::vector<float>& freqs, const std::vector<float>& db, juce::Rectangle<float> r,
                          float lo, float hi, bool closeToBottom)
    {
        juce::Path p;
        bool started = false;
        for (size_t i = 0; i < freqs.size() && i < db.size(); ++i)
        {
            if (! std::isfinite (db[i])) continue;
            const float x = xFor (freqs[i], r);
            const float y = juce::jmap (juce::jlimit (lo, hi, db[i]), lo, hi, r.getBottom(), r.getY());
            if (! started) { p.startNewSubPath (x, closeToBottom ? r.getBottom() : y); if (closeToBottom) p.lineTo (x, y); started = true; }
            else p.lineTo (x, y);
        }
        if (started && closeToBottom)
        {
            p.lineTo (p.getCurrentPosition().x, r.getBottom());
            p.closeSubPath();
        }
        return p;
    }
}

void drawSpectrum (juce::Graphics& g, juce::Rectangle<float> area, const book::SpectrumData& d,
                   juce::Colour curveColour, const juce::String& khzSuffix)
{
    auto labels = area.removeFromBottom (16.0f);
    const auto r = area;

    g.setColour (Palette::paperField.withAlpha (0.6f));
    g.fillRoundedRectangle (r, 4.0f);

    // The chapter's problem range
    if (d.shadeHighHz > d.shadeLowHz && d.shadeLowHz > 0.0f)
    {
        const auto zone = juce::Rectangle<float>::leftTopRightBottom (xFor (d.shadeLowHz, r), r.getY(), xFor (d.shadeHighHz, r), r.getBottom());
        g.setColour (Palette::good.withAlpha (0.16f));
        g.fillRect (zone);
        g.setColour (Palette::good.withAlpha (0.6f));
        g.fillRect (zone.withWidth (1.0f));
        g.fillRect (zone.withX (zone.getRight() - 1.0f).withWidth (1.0f));
    }

    // Decade grid and labels: 100 Hz, 1 kHz, 10 kHz
    g.setFont (Fonts::garamond (13.0f));
    for (float hz : { 100.0f, 1000.0f, 10000.0f })
    {
        const float x = xFor (hz, r);
        g.setColour (Palette::rule.withAlpha (0.5f));
        g.fillRect (x, r.getY(), 1.0f, r.getHeight());
        g.setColour (Palette::inkMuted);
        const auto text = hz >= 1000.0f ? juce::String ((int) (hz / 1000.0f)) + khzSuffix : juce::String ((int) hz);
        g.drawText (text, juce::Rectangle<float> (x - 30.0f, labels.getY(), 60.0f, labels.getHeight()), juce::Justification::centred);
    }

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (r.toNearestInt());

        // Before (faint outline), after (filled)
        if (! d.inDb.empty())
        {
            g.setColour (Palette::inkMuted.withAlpha (0.35f));
            g.strokePath (curvePath (d.freqs, d.inDb, r, floorDb, topDb, false), juce::PathStrokeType (1.0f));
        }
        if (! d.outDb.empty())
        {
            g.setColour (Palette::inkMuted.withAlpha (0.3f));
            g.fillPath (curvePath (d.freqs, d.outDb, r, floorDb, topDb, true));
        }

        // The volume's curve on its own ±18 dB scale, 0 dB through the middle
        g.setColour (Palette::rule.withAlpha (0.8f));
        g.fillRect (r.getX(), r.getCentreY(), r.getWidth(), 1.0f);
        if (! d.curveDb.empty())
        {
            g.setColour (curveColour);
            g.strokePath (curvePath (d.freqs, d.curveDb, r, -curveRangeDb, curveRangeDb, false),
                          juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    g.setColour (Palette::rule);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

} // namespace na::ui
