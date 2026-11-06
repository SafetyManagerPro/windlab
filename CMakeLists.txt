cmake_minimum_required(VERSION 3.22)
project(WindLab VERSION 0.9.3 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Downloads JUCE at configure time. For offline builds provide -DJUCE_SOURCE_DIR=/path/to/JUCE.
if (DEFINED JUCE_SOURCE_DIR)
    add_subdirectory("${JUCE_SOURCE_DIR}" JUCE-build)
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.5
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
    DESCRIPTION "Recorded multilayer winds with expressive MIDI, EQ, reverb and live controls"
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
    Source/PhraseEditor.cpp
    Source/PhraseEditor.h
    Source/WindLabGuidePanel.cpp
    Source/WindLabGuidePanel.h
    Source/WindLabMidiLearnPanel.cpp
    Source/WindLabMidiLearnPanel.h
    Source/Dsp/WindEngine.h
    Source/Dsp/ToneEQ.h
    Source/Dsp/RecordedWinds.h
    Source/Dsp/SampleLoopPlan.h
    Source/Dsp/VelocityResponse.h
    Source/Dsp/PhraseEngine.h
    Source/Dsp/PhraseRecorder.h
    Source/Dsp/MidiLearnBindings.h)

# WindLab has never shipped as VST2: do not claim VST2 replacement compatibility.
# JUCE 9 rejects modern VST3 parameter IDs when VST2 replacement is enabled.
target_compile_definitions(WindLab PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# CI downloads licensed CC0 WAVs before configure. Without them, build retains
# the previous synthetic engine and does not pretend to have recorded assets.
file(GLOB WINDLAB_RECORDED_WAVS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Samples/*.wav"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Samples/*.flac")
if(WINDLAB_RECORDED_WAVS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Samples/SampleManifest.h")
    juce_add_binary_data(WindLabRecordedData
        HEADER_NAME WindLabRecordedData.h
        NAMESPACE WindLabRecordedData
        SOURCES ${WINDLAB_RECORDED_WAVS})
    target_link_libraries(WindLab PRIVATE WindLabRecordedData)
    target_include_directories(WindLab PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Samples")
    target_compile_definitions(WindLab PRIVATE WINDLAB_HAS_RECORDED_SAMPLES=1)
else()
    message(WARNING "No CC0 sample bank found. Run python scripts/download-recordings.py for acoustic builds.")
endif()

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

    add_executable(windlab_expression_smoke tests/expression-smoke.cpp)
    target_include_directories(windlab_expression_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_expression_smoke COMMAND windlab_expression_smoke)

    add_executable(windlab_phrase_smoke tests/phrase-smoke.cpp)
    target_include_directories(windlab_phrase_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_phrase_smoke COMMAND windlab_phrase_smoke)

    add_executable(windlab_midi_learn_smoke tests/midi-learn-smoke.cpp)
    target_include_directories(windlab_midi_learn_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_midi_learn_smoke COMMAND windlab_midi_learn_smoke)

    add_executable(windlab_phrase_record_smoke tests/phrase-record-smoke.cpp)
    target_include_directories(windlab_phrase_record_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_phrase_record_smoke COMMAND windlab_phrase_record_smoke)

    add_executable(windlab_phrase_midi_smoke tests/phrase-midi-smoke.cpp)
    target_include_directories(windlab_phrase_midi_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_phrase_midi_smoke COMMAND windlab_phrase_midi_smoke)

    add_executable(windlab_phrase_grid_smoke tests/phrase-grid-smoke.cpp)
    target_include_directories(windlab_phrase_grid_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_phrase_grid_smoke COMMAND windlab_phrase_grid_smoke)
endif()
