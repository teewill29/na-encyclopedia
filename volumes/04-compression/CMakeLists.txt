# NA Encyclopedia Vol. IV: Compression

juce_add_plugin(NAEncyclopediaCompression
    COMPANY_NAME                "Noveltee Audio"
    PRODUCT_NAME                "NA Encyclopedia IV Compression"
    PLUGIN_MANUFACTURER_CODE    Tywl
    PLUGIN_CODE                 NE04
    BUNDLE_ID                   com.novelteeaudio.naencyclopedia.compression
    VERSION                     1.0.0
    FORMATS                     AU VST3
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    VST3_CATEGORIES             Fx Dynamics
)

target_sources(NAEncyclopediaCompression PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
    dsp/Compressor.cpp
)

# All copy, targets and goals, embedded so the plugin never reads files at runtime.
# Accessed in code as BinaryData::compression_json.
juce_add_binary_data(NAEncyclopediaCompressionData
    SOURCES ${PROJECT_SOURCE_DIR}/content/compression.json
)

target_compile_definitions(NAEncyclopediaCompression PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(NAEncyclopediaCompression
    PRIVATE
        NAEncyclopediaCore
        NAEncyclopediaCompressionData
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
