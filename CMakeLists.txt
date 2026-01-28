cmake_minimum_required(VERSION 3.15)
project(FreeEQ8 VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 17)

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
    COMPANY_NAME "FreeEQ"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE FrEQ
    PLUGIN_CODE FEQ8
    FORMATS VST3 AU
    PRODUCT_NAME "FreeEQ8"
)

target_sources(FreeEQ8 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/DSP/Biquad.h
    Source/DSP/EQBand.h
)

target_compile_definitions(FreeEQ8 PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(FreeEQ8 PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
)
