cmake_minimum_required(VERSION 3.22)

# macOS 12+ (see README → Tech stack). Must be set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary: Apple Silicon + Intel")

project(NAEncyclopedia VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── JUCE 8, pinned to an exact release tag ───────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ── Build ID (short git commit), shown in the editor so it's obvious which
#    build Logic has loaded ─────────────────────────────────────────────────────
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE NA_BUILD_ID
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT NA_BUILD_ID)
    set(NA_BUILD_ID "local")
endif()

# ── Shared code used by every volume ─────────────────────────────────────────
# An INTERFACE library: its sources compile inside each plugin, with that
# plugin's JUCE settings. (A STATIC library that uses JUCE would need its own
# copy of JUCE built with matching settings, a known source of hard-to-find
# bugs.) Content loader now; analysis, chapter engine and UI in later milestones.
add_library(NAEncyclopediaCore INTERFACE)
target_sources(NAEncyclopediaCore INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/content/Content.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/content/Template.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/chapters/Session.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/chapters/FeedbackTracker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/analysis/Metrics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/ui/MetricsOverlay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/ui/Style.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/ui/BookFrame.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/ui/Controls.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/ui/Spectrum.cpp
)

# The book's fonts (Cinzel, EB Garamond; SIL OFL), embedded. See assets/fonts/README.md.
juce_add_binary_data(NAEncyclopediaFonts
    HEADER_NAME NAFonts.h
    NAMESPACE NAFonts
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Cinzel-Medium.ttf
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Cinzel-SemiBold.ttf
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Cinzel-Bold.ttf
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/EBGaramond-Regular.ttf
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/EBGaramond-Medium.ttf
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/EBGaramond-SemiBold.ttf
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/EBGaramond-Italic.ttf
)
target_link_libraries(NAEncyclopediaCore INTERFACE NAEncyclopediaFonts)

# The book itself (processor base, screens, window). Only plugins and their
# end-to-end tests use it: it needs the plugin's own defines (JucePlugin_Name,
# version, NA_BUILD_ID, NA_DEBUG_OVERLAY).
add_library(NAEncyclopediaBook INTERFACE)
target_sources(NAEncyclopediaBook INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/book/BookProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/book/BookViews.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/book/BookEditor.cpp
)
target_link_libraries(NAEncyclopediaBook INTERFACE NAEncyclopediaCore)
target_include_directories(NAEncyclopediaCore INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/shared)
target_compile_features(NAEncyclopediaCore INTERFACE cxx_std_20)

# Debug overlay with every live metric (FEEDBACK-ENGINE.md §7). On during
# development so the owner can see it in CI builds; turn OFF for release (M8).
option(NA_DEBUG_OVERLAY "Show the analysis debug overlay" ON)

enable_testing()

# Unit tests for shared/analysis (plain C++, no JUCE), run by ctest in CI
add_executable(NAEncyclopediaAnalysisTests
    shared/tests/AnalysisTests.cpp
    shared/analysis/Metrics.cpp
)
target_include_directories(NAEncyclopediaAnalysisTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shared)
target_compile_features(NAEncyclopediaAnalysisTests PRIVATE cxx_std_20)
add_test(NAME AnalysisTests COMMAND NAEncyclopediaAnalysisTests)

# Unit tests for the feedback state machine (plain C++)
add_executable(NAEncyclopediaFeedbackTests
    shared/tests/FeedbackTests.cpp
    shared/chapters/FeedbackTracker.cpp
)
target_include_directories(NAEncyclopediaFeedbackTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shared)
target_compile_features(NAEncyclopediaFeedbackTests PRIVATE cxx_std_20)
add_test(NAME FeedbackTests COMMAND NAEncyclopediaFeedbackTests)

# Unit tests for the listening tools (plain C++)
add_executable(NAEncyclopediaListeningTests shared/tests/ListeningTests.cpp)
target_include_directories(NAEncyclopediaListeningTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shared)
target_compile_features(NAEncyclopediaListeningTests PRIVATE cxx_std_20)
add_test(NAME ListeningTests COMMAND NAEncyclopediaListeningTests)

# Unit tests for shared/content and shared/chapters (need JUCE core + data structures)
juce_add_console_app(NAEncyclopediaContentTests PRODUCT_NAME "NAEncyclopediaContentTests")
target_sources(NAEncyclopediaContentTests PRIVATE
    shared/tests/ContentTests.cpp
    shared/content/Content.cpp
    shared/content/Template.cpp
    shared/chapters/Session.cpp
)
target_include_directories(NAEncyclopediaContentTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shared)
target_compile_definitions(NAEncyclopediaContentTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(NAEncyclopediaContentTests
    PRIVATE juce::juce_data_structures
    PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
add_test(NAME ContentTests COMMAND NAEncyclopediaContentTests)

# ── Volumes ──────────────────────────────────────────────────────────────────
add_subdirectory(volumes/01-gain-staging)
add_subdirectory(volumes/03-eq)
add_subdirectory(volumes/04-compression)
