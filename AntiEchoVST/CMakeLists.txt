cmake_minimum_required(VERSION 3.22)

project(AntiEchoVST VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCEを取得(初回ビルド時にgithubから自動ダウンロードされます) ---
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(AntiEchoVST
    COMPANY_NAME "YourName"
    PLUGIN_MANUFACTURER_CODE Yorg
    PLUGIN_CODE Aevc
    FORMATS VST3
    PRODUCT_NAME "AntiEchoVST"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(AntiEchoVST PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/LoopbackCapture.cpp
)

target_compile_definitions(AntiEchoVST PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AntiEchoVST PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce_recommended_config_flags
    juce_recommended_lto_flags
    juce_recommended_warning_flags
)

# WASAPIループバック用のWindows COMライブラリ
if (WIN32)
    target_link_libraries(AntiEchoVST PRIVATE
        Ole32 Avrt Mmdevapi Propsys
    )
    # ソースファイルの文字コード解釈をUTF-8に固定(日本語文字列の文字化け対策)
    target_compile_options(AntiEchoVST PRIVATE /utf-8)
endif()
