# NA Encyclopedia Vol. I: Gain Staging (docs/GAIN-STAGING.md)

# Bump on every build handed to Logic for testing (Logic caches per version).
set(NA_GAINSTAGING_VERSION 1.0.0)

if(APPLE)
    set(NA_GAINSTAGING_FORMATS AU VST3)
else()
    set(NA_GAINSTAGING_FORMATS VST3)   # Linux builds are only for local compile checks
endif()

juce_add_plugin(NAEncyclopediaGainStaging
    COMPANY_NAME                "Noveltee Audio"
    PRODUCT_NAME                "NA Encyclopedia I Gain Staging"
    PLUGIN_MANUFACTURER_CODE    Tywl
    PLUGIN_CODE                 NE01
    BUNDLE_ID                   com.novelteeaudio.naencyclopedia.gainstaging
    VERSION                     ${NA_GAINSTAGING_VERSION}
    FORMATS                     ${NA_GAINSTAGING_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    VST3_CATEGORIES             Fx Tools
)

target_sources(NAEncyclopediaGainStaging PRIVATE
    PluginProcessor.cpp
)
target_include_directories(NAEncyclopediaGainStaging PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

juce_add_binary_data(NAEncyclopediaGainStagingData
    SOURCES ${PROJECT_SOURCE_DIR}/content/gain-staging.json
)

target_compile_definitions(NAEncyclopediaGainStaging PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    NA_BUILD_ID="${NA_BUILD_ID}"
    NA_DEBUG_OVERLAY=$<BOOL:${NA_DEBUG_OVERLAY}>
)

target_link_libraries(NAEncyclopediaGainStaging
    PRIVATE
        NAEncyclopediaBook
        NAEncyclopediaGainStagingData
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
# No LTO on purpose (see 04-compression/CMakeLists.txt).

# ── End-to-end processor tests ───────────────────────────────────────────────
juce_add_console_app(NAEncyclopediaGainStagingTests PRODUCT_NAME "NAEncyclopediaGainStagingTests")
target_sources(NAEncyclopediaGainStagingTests PRIVATE
    tests/ProcessorTests.cpp
    PluginProcessor.cpp
)
target_include_directories(NAEncyclopediaGainStagingTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(NAEncyclopediaGainStagingTests PRIVATE
    JucePlugin_Name="NA Encyclopedia I Gain Staging"
    JucePlugin_VersionString="${NA_GAINSTAGING_VERSION}"
    NA_BUILD_ID="${NA_BUILD_ID}"
    NA_DEBUG_OVERLAY=$<BOOL:${NA_DEBUG_OVERLAY}>
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_link_libraries(NAEncyclopediaGainStagingTests
    PRIVATE
        NAEncyclopediaBook
        NAEncyclopediaGainStagingData
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
add_test(NAME GainStagingTests COMMAND NAEncyclopediaGainStagingTests)
