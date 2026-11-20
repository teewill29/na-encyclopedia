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
# Placeholder for now. Content loader, analysis, chapter engine and UI
# components land here in later milestones.
add_library(NAEncyclopediaCore STATIC
    shared/NAEncyclopediaCore.cpp
)
target_include_directories(NAEncyclopediaCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/shared)
target_compile_features(NAEncyclopediaCore PUBLIC cxx_std_20)

# ── Volumes ──────────────────────────────────────────────────────────────────
add_subdirectory(volumes/04-compression)
