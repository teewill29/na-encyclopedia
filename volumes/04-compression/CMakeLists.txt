# NA Encyclopedia Vol. IV: Compression

# Bump on every build you hand to Logic for testing: Logic caches each plugin's
# name and validation result per version, so a new version forces a fresh look.
set(NA_COMPRESSION_VERSION 1.0.2)

if(APPLE)
    set(NA_COMPRESSION_FORMATS AU VST3)
else()
    set(NA_COMPRESSION_FORMATS VST3)   # Linux builds are only for local compile checks
endif()

juce_add_plugin(NAEncyclopediaCompression
    COMPANY_NAME                "Noveltee Audio"
    PRODUCT_NAME                "NA Encyclopedia IV Compression"
    PLUGIN_MANUFACTURER_CODE    Tywl
    PLUGIN_CODE                 NE04
    BUNDLE_ID                   com.novelteeaudio.naencyclopedia.compression
    VERSION                     ${NA_COMPRESSION_VERSION}
    FORMATS                     ${NA_COMPRESSION_FORMATS}
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
    Parameters.cpp
    dsp/Compressor.cpp
)

target_include_directories(NAEncyclopediaCompression PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# All copy, targets and goals, embedded so the plugin never reads files at runtime.
# Accessed in code as BinaryData::compression_json.
juce_add_binary_data(NAEncyclopediaCompressionData
    SOURCES ${PROJECT_SOURCE_DIR}/content/compression.json
)

target_compile_definitions(NAEncyclopediaCompression PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    NA_BUILD_ID="${NA_BUILD_ID}"
)

target_link_libraries(NAEncyclopediaCompression
    PRIVATE
        NAEncyclopediaCore
        NAEncyclopediaCompressionData
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
# No link-time optimisation (juce_recommended_lto_flags) on purpose: in Front of
# House, LTO made clang drop a store and the first channel came out silent in
# Logic. Not worth the risk for a few percent of speed.

# ── Unit tests for the compressor DSP (plain C++, no JUCE), run by ctest in CI ──
add_executable(NAEncyclopediaCompressionTests
    tests/CompressorTests.cpp
    dsp/Compressor.cpp
)
target_include_directories(NAEncyclopediaCompressionTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(NAEncyclopediaCompressionTests PRIVATE cxx_std_20)
add_test(NAME CompressorTests COMMAND NAEncyclopediaCompressionTests)
