cmake_minimum_required(VERSION 3.22)
project(WindLab VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Downloads JUCE at configure time. For offline builds provide -DJUCE_SOURCE_DIR=/path/to/JUCE.
if (DEFINED JUCE_SOURCE_DIR)
    add_subdirectory("${JUCE_SOURCE_DIR}" JUCE-build)
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

set(WINDLAB_FORMATS VST3 Standalone)
if (APPLE)
    list(APPEND WINDLAB_FORMATS AU)
endif()

juce_add_plugin(WindLab
    COMPANY_NAME "WindLab Prototype"
    BUNDLE_ID "com.windlab.prototype"
    PLUGIN_MANUFACTURER_CODE Wlab
    PLUGIN_CODE Wl01
    PRODUCT_NAME "WindLab"
    DESCRIPTION "Six expressive wind synth sketches (development prototype)"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS ${WINDLAB_FORMATS})

target_sources(WindLab PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Dsp/WindEngine.h)

target_compile_definitions(WindLab PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

target_link_libraries(WindLab PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# The JUCE-free DSP smoke test can also be built and run on Linux.
include(CTest)
if (BUILD_TESTING)
    add_executable(windlab_smoke tests/smoke.cpp)
    target_include_directories(windlab_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_smoke COMMAND windlab_smoke)
endif()
