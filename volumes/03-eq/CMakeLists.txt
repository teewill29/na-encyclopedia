# NA Encyclopedia Vol. III: EQ (docs/EQ.md)

# Bump on every build handed to Logic for testing (Logic caches per version).
set(NA_EQ_VERSION 1.0.1)

if(APPLE)
    set(NA_EQ_FORMATS AU VST3)
else()
    set(NA_EQ_FORMATS VST3)   # Linux builds are only for local compile checks
endif()

juce_add_plugin(NAEncyclopediaEQ
    COMPANY_NAME                "Noveltee Audio"
    PRODUCT_NAME                "NA Encyclopedia III EQ"
    PLUGIN_MANUFACTURER_CODE    Tywl
    PLUGIN_CODE                 NE03
    BUNDLE_ID                   com.novelteeaudio.naencyclopedia.eq
    VERSION                     ${NA_EQ_VERSION}
    FORMATS                     ${NA_EQ_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    VST3_CATEGORIES             Fx EQ
)

set(NA_EQ_SOURCES PluginProcessor.cpp dsp/Equalizer.cpp)
target_sources(NAEncyclopediaEQ PRIVATE ${NA_EQ_SOURCES})
target_include_directories(NAEncyclopediaEQ PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

juce_add_binary_data(NAEncyclopediaEQData SOURCES ${PROJECT_SOURCE_DIR}/content/eq.json)

target_compile_definitions(NAEncyclopediaEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    NA_BUILD_ID="${NA_BUILD_ID}"
    NA_DEBUG_OVERLAY=$<BOOL:${NA_DEBUG_OVERLAY}>
)
target_link_libraries(NAEncyclopediaEQ
    PRIVATE NAEncyclopediaBook NAEncyclopediaEQData juce::juce_audio_utils
    PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
# No LTO on purpose (see 04-compression/CMakeLists.txt).

# ── Unit tests for the EQ DSP (plain C++) ─────────────────────────────────────
add_executable(NAEncyclopediaEqualizerTests tests/EqualizerTests.cpp dsp/Equalizer.cpp)
target_include_directories(NAEncyclopediaEqualizerTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(NAEncyclopediaEqualizerTests PRIVATE cxx_std_20)
add_test(NAME EqualizerTests COMMAND NAEncyclopediaEqualizerTests)

# ── End-to-end processor tests ───────────────────────────────────────────────
juce_add_console_app(NAEncyclopediaEQTests PRODUCT_NAME "NAEncyclopediaEQTests")
target_sources(NAEncyclopediaEQTests PRIVATE tests/ProcessorTests.cpp ${NA_EQ_SOURCES})
target_include_directories(NAEncyclopediaEQTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(NAEncyclopediaEQTests PRIVATE
    JucePlugin_Name="NA Encyclopedia III EQ"
    JucePlugin_VersionString="${NA_EQ_VERSION}"
    NA_BUILD_ID="${NA_BUILD_ID}"
    NA_DEBUG_OVERLAY=$<BOOL:${NA_DEBUG_OVERLAY}>
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)
target_link_libraries(NAEncyclopediaEQTests
    PRIVATE NAEncyclopediaBook NAEncyclopediaEQData juce::juce_audio_utils
    PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
add_test(NAME EQTests COMMAND NAEncyclopediaEQTests)

# ── Tuning report (not shipped): the spectrum analysis over a real recording ──
juce_add_console_app(NAEncyclopediaSpectrumReport PRODUCT_NAME "NAEncyclopediaSpectrumReport")
target_sources(NAEncyclopediaSpectrumReport PRIVATE tools/SpectrumReport.cpp ${CMAKE_SOURCE_DIR}/shared/analysis/SpectrumAnalyser.cpp)
target_include_directories(NAEncyclopediaSpectrumReport PRIVATE ${CMAKE_SOURCE_DIR}/shared)
target_compile_definitions(NAEncyclopediaSpectrumReport PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(NAEncyclopediaSpectrumReport
    PRIVATE NAEncyclopediaEQData juce::juce_audio_formats
    PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
