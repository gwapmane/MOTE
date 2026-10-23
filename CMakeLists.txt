cmake_minimum_required(VERSION 3.22)

# Windows: build the C++ runtime INTO the plugin, so nothing extra (Visual C++ redistributable) has to be installed.
# This has to be set before project().
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(MOTE VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MOTE_BUILD_PLUGIN "Build the JUCE plugin (VST3 + Standalone)" ON)
option(MOTE_BUILD_TOOLS  "Build the offline renderer and DSP self-test" ON)
option(MOTE_VIDEO_MF     "Windows: play MP4/MOV/WMV/AVI in the window (Media Foundation). GIF and pictures always work." ON)

# ---- DSP core tools (no JUCE needed) -------------------------------------------------------
if(MOTE_BUILD_TOOLS)
    add_executable(mote_selftest tests/selftest.cpp)
    add_executable(mote_render   tools/render.cpp)
    if(MSVC)
        target_compile_options(mote_selftest PRIVATE /O2 /W3)
        target_compile_options(mote_render   PRIVATE /O2 /W3)
    else()
        target_compile_options(mote_selftest PRIVATE -O2 -Wall -Wextra)
        target_compile_options(mote_render   PRIVATE -O2 -Wall -Wextra)
    endif()
    enable_testing()
    add_test(NAME dsp_selftest COMMAND mote_selftest)
endif()

# ---- the plugin ----------------------------------------------------------------------------
if(MOTE_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.6
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)

    # Mac: also make the Audio Unit (AU) version, which FL Studio and Logic on a Mac pick up through a different route
    set(MOTE_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND MOTE_FORMATS AU)
    endif()

    juce_add_plugin(MOTE
        COMPANY_NAME "MOTE"
        PLUGIN_MANUFACTURER_CODE Mtpd
        PLUGIN_CODE Mote
        FORMATS ${MOTE_FORMATS}
        PRODUCT_NAME "MOTE"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Instrument Synth
        AU_MAIN_TYPE kAudioUnitType_MusicDevice)

    # the hardware artwork, body renders and fonts, compiled into the plugin
    include(assets/CMakeAssets.cmake)
    juce_add_binary_data(MoteAssets SOURCES ${MOTE_ASSET_FILES})
    set_target_properties(MoteAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

    target_sources(MOTE PRIVATE
        plugin/PluginProcessor.cpp
        plugin/PluginEditor.cpp
        plugin/PresetManager.cpp
        plugin/Settings.cpp
        plugin/Assets.cpp
        plugin/Body.cpp
        plugin/WaveView.cpp
        plugin/PedalView.cpp
        plugin/Chrome.cpp
        plugin/Panels.cpp
        plugin/VideoLoop.cpp
        plugin/VideoMF.cpp)

    target_include_directories(MOTE PRIVATE core plugin assets)

    target_compile_definitions(MOTE PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_USE_MP3AUDIOFORMAT=1)

    if(NOT MOTE_VIDEO_MF)
        target_compile_definitions(MOTE PRIVATE MOTE_NO_MF=1)
    endif()

    target_link_libraries(MOTE
        PRIVATE
            MoteAssets
            juce::juce_audio_utils
            juce::juce_audio_formats
            juce::juce_audio_processors
            juce::juce_gui_basics
            $<$<AND:$<PLATFORM_ID:Windows>,$<BOOL:${MOTE_VIDEO_MF}>>:mfplat>
            $<$<AND:$<PLATFORM_ID:Windows>,$<BOOL:${MOTE_VIDEO_MF}>>:mfreadwrite>
            $<$<AND:$<PLATFORM_ID:Windows>,$<BOOL:${MOTE_VIDEO_MF}>>:mfuuid>
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
