cmake_minimum_required(VERSION 3.22)

project(SimpleCompressorVST VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(SimpleCompressorVST
    COMPANY_NAME "YourName"
    PLUGIN_MANUFACTURER_CODE Yorg
    PLUGIN_CODE Scmp
    FORMATS VST3
    PRODUCT_NAME "SimpleCompressorVST"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(SimpleCompressorVST PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(SimpleCompressorVST PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SimpleCompressorVST PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce_recommended_config_flags
    juce_recommended_lto_flags
    juce_recommended_warning_flags
)

if (WIN32)
    target_compile_options(SimpleCompressorVST PRIVATE /utf-8)
endif()
