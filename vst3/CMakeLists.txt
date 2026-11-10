cmake_minimum_required(VERSION 3.22)
project(SuperSat VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(SuperSat
  COMPANY_NAME "SuperSat"
  PLUGIN_MANUFACTURER_CODE SuSa
  PLUGIN_CODE Ssat
  FORMATS VST3 Standalone
  PRODUCT_NAME "Super Sat"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx Distortion)

juce_add_binary_data(SuperSatArt SOURCES
  art/goku0.jpg art/goku1.jpg art/goku2.jpg
  art/goku3.jpg art/goku4.jpg art/goku5.jpg)

target_sources(SuperSat PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_compile_definitions(SuperSat PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SuperSat
  PRIVATE
    SuperSatArt
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)
