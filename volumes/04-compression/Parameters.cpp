#include "Parameters.h"

namespace na::vol4
{

namespace
{
    // Log taper for the time knobs (README → Compressor DSP).
    juce::NormalisableRange<float> logRange (float min, float max)
    {
        return { min, max,
                 [] (float start, float end, float t)   { return start * std::pow (end / start, t); },
                 [] (float start, float end, float v)   { return std::log (v / start) / std::log (end / start); },
                 [] (float start, float end, float v)   { return juce::jlimit (start, end, v); } };
    }

    struct Spec
    {
        const char* id;
        juce::NormalisableRange<float> range;
        float defaultValue;
        int   (*decimals) (float);
        bool  showPlus;
    };

    int oneDecimal (float)        { return 1; }
    int noDecimals (float)        { return 0; }
    int timeDecimals (float v)    { return v < 10.0f ? 1 : 0; }

    std::unique_ptr<juce::AudioParameterFloat> makeParameter (const Spec& spec, const na::Content& content)
    {
        const auto base = juce::String ("parameters.") + spec.id;
        const auto name = content.getString (base + ".name", spec.id);
        const auto unit = content.getString (base + ".unit");
        const auto decimals = spec.decimals;
        const auto showPlus = spec.showPlus;

        // The unit is part of the value text ("-18.0 dB"), so no separate
        // label: hosts and the generic editor would otherwise show it twice.
        auto attributes = juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([unit, decimals, showPlus] (float v, int)
            {
                auto text = juce::String (v, decimals (v));
                if (showPlus && v > 0.0f)
                    text = "+" + text;
                return text + unit;
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                return text.retainCharacters ("0123456789.-").getFloatValue();
            });

        return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { spec.id, 1 },
                                                            name, spec.range, spec.defaultValue, attributes);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (const na::Content& content)
{
    juce::NormalisableRange<float> ratioRange (1.0f, 20.0f, 0.01f);
    ratioRange.setSkewForCentre (4.0f);

    // Ranges from README → Compressor DSP (V1). Defaults: no compression until
    // the user lowers Threshold (Chapter II), ratio 4:1.
    const Spec specs[] = {
        { ParamID::threshold, { -60.0f, 0.0f, 0.1f },  0.0f,   oneDecimal,   false },
        { ParamID::ratio,     ratioRange,              4.0f,   oneDecimal,   false },
        { ParamID::attack,    logRange (0.1f, 100.0f), 10.0f,  timeDecimals, false },
        { ParamID::release,   logRange (10.0f, 1000.0f), 100.0f, noDecimals, false },
        { ParamID::makeup,    { -12.0f, 24.0f, 0.1f }, 0.0f,   oneDecimal,   true  },
        { ParamID::mix,       { 0.0f, 100.0f, 1.0f },  100.0f, noDecimals,   false },
    };

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& s : specs)
        layout.add (makeParameter (s, content));
    return layout;
}

} // namespace na::vol4
