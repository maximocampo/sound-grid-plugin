cmake_minimum_required(VERSION 3.22)
project(soundGridPlugin VERSION 1.0.0)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/JUCE ${CMAKE_CURRENT_BINARY_DIR}/JUCE)

juce_add_plugin(soundGridPlugin
    COMPANY_NAME "soundGrid"
    PLUGIN_MANUFACTURER_CODE Sgrd
    PLUGIN_CODE Sgpl
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "soundGrid"
    IS_SYNTH FALSE
    PLUGIN_WANTS_MIDI_INPUT FALSE
    PLUGIN_PRODUCES_MIDI_OUTPUT FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
)

target_sources(soundGridPlugin
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/SampleVoice.cpp
        Source/SoundCircle.cpp
        Source/GridComponent.cpp
        Source/SidebarComponent.cpp
        Source/StandaloneApp.cpp
)

target_compile_definitions(soundGridPlugin
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
)

target_link_libraries(soundGridPlugin
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
