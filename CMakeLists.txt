cmake_minimum_required(VERSION 3.22)

project(RecklessChronos2 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

option(RC2_BUILD_TESTS "Build the offline render/validation test app" ON)

# JUCE: fetched automatically. Pass -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE to use a local checkout.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.10
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

set(RC2_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND RC2_FORMATS AU)
endif()

juce_add_plugin(RecklessChronos2
    COMPANY_NAME                "Reckless"
    COMPANY_WEBSITE             "https://github.com/RecklessBoise/reckless-chronos-2"
    BUNDLE_ID                   com.reckless.chronos2
    PLUGIN_MANUFACTURER_CODE    Rckl
    PLUGIN_CODE                 Rch2
    FORMATS                     ${RC2_FORMATS}
    PRODUCT_NAME                "Reckless Chronos 2"
    DESCRIPTION                 "Workstation synthesizer with 10 synthesis engines"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Synth
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice)

juce_add_binary_data(RC2Data SOURCES Presets/factory.json)
set_target_properties(RC2Data PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(RC2_SOURCES
    Source/Params.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/Tables.cpp
    Source/dsp/Voice.cpp
    Source/dsp/Timbre.cpp
    Source/dsp/Effects.cpp
    Source/presets/PresetManager.cpp
    Source/ui/LookAndFeel.cpp
    Source/ui/Widgets.cpp
    Source/ui/Screen.cpp)

set(RC2_DEFINES
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

set(RC2_LIBS
    RC2Data
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp)

target_sources(RecklessChronos2 PRIVATE ${RC2_SOURCES})
target_include_directories(RecklessChronos2 PRIVATE Source)
target_compile_definitions(RecklessChronos2 PUBLIC ${RC2_DEFINES})
target_link_libraries(RecklessChronos2
    PRIVATE ${RC2_LIBS}
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

if(RC2_BUILD_TESTS)
    juce_add_console_app(RC2Tests PRODUCT_NAME "RC2Tests")
    target_sources(RC2Tests PRIVATE tests/RenderTests.cpp ${RC2_SOURCES})
    target_include_directories(RC2Tests PRIVATE Source)
    target_compile_definitions(RC2Tests PRIVATE ${RC2_DEFINES}
        JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1 RC2_TEST_BUILD=1)
    if(APPLE)
        target_compile_definitions(RC2Tests PRIVATE JUCE_PLUGINHOST_AU=1)
    endif()
    target_link_libraries(RC2Tests PRIVATE ${RC2_LIBS}
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
