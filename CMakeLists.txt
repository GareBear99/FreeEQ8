cmake_minimum_required(VERSION 3.15)
project(FreeEQ8 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

# Universal binary for macOS (Intel + Apple Silicon)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build universal binary")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
endif()

# ---- JUCE ----
# Option A: JUCE as submodule in ./JUCE (recommended)
#   git submodule add https://github.com/juce-framework/JUCE.git JUCE
# Option B: Point JUCE_DIR to your local JUCE folder:
#   cmake -DJUCE_DIR=/path/to/JUCE ..
#
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
  add_subdirectory(JUCE)
elseif (DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
  add_subdirectory("${JUCE_DIR}" JUCE)
else()
  message(FATAL_ERROR "JUCE not found. Add as submodule in ./JUCE or pass -DJUCE_DIR=/path/to/JUCE")
endif()

juce_add_plugin(FreeEQ8
    COMPANY_NAME "TizWildinEntertainment"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE TzWd
    PLUGIN_CODE FEQ8
    FORMATS VST3 AU
    PRODUCT_NAME "FreeEQ8"
    BUNDLE_ID "com.tizwildinentertainment.freeeq8"
    COMPANY_URL "https://github.com/GareBear99/FreeEQ8"
    COMPANY_EMAIL "GareBear99@users.noreply.github.com"
    DESCRIPTION "Professional 8-Band Parametric EQ"
)

target_sources(FreeEQ8 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Config.h
    Source/UpdateChecker.h
    Source/LicenseValidator.h
    Source/DSP/Biquad.h
    Source/DSP/EQBand.h
    Source/DSP/SpectrumFIFO.h
    Source/DSP/LinearPhaseEngine.h
    Source/DSP/MatchEQ.h
    Source/UI/ResponseCurveComponent.h
    Source/UI/ResponseCurveComponent.cpp
    Source/UI/LevelMeter.h
    Source/Presets/PresetManager.h
    Source/Presets/PresetManager.cpp
)

target_compile_definitions(FreeEQ8 PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(FreeEQ8 PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_cryptography
)

# ── ProEQ8 commercial target
juce_add_plugin(ProEQ8
    COMPANY_NAME "TizWildinEntertainment"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE TzWd
    PLUGIN_CODE PEQ8
    FORMATS VST3 AU
    PRODUCT_NAME "ProEQ8"
    BUNDLE_ID "com.tizwildinentertainment.proeq8"
    COMPANY_URL "https://github.com/GareBear99/FreeEQ8"
    COMPANY_EMAIL "GareBear99@users.noreply.github.com"
    DESCRIPTION "Professional 24-Band Parametric EQ"
)

target_sources(ProEQ8 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Config.h
    Source/UpdateChecker.h
    Source/LicenseValidator.h
    Source/DSP/Biquad.h
    Source/DSP/EQBand.h
    Source/DSP/SpectrumFIFO.h
    Source/DSP/LinearPhaseEngine.h
    Source/DSP/MatchEQ.h
    Source/UI/ResponseCurveComponent.h
    Source/UI/ResponseCurveComponent.cpp
    Source/UI/LevelMeter.h
    Source/Presets/PresetManager.h
    Source/Presets/PresetManager.cpp
)

target_compile_definitions(ProEQ8 PRIVATE
    PROEQ8=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ProEQ8 PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_cryptography
)

# ── Optional test target (standalone, no JUCE dependency) ────────────
option(FREEEQ8_BUILD_TESTS "Build unit tests" OFF)

if (FREEEQ8_BUILD_TESTS)
    add_executable(FreeEQ8_Tests Tests/BiquadTest.cpp)
    target_compile_features(FreeEQ8_Tests PRIVATE cxx_std_17)
endif()
