cmake_minimum_required(VERSION 3.22)
project(WindLab VERSION 0.9.13 LANGUAGES C CXX)

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
    # 0.9.8 breath from the microphone: macOS asks the user once (Standalone).
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "WindLab usa il microfono solo per misurare il soffio quando SOFFIO DA e' impostato su MICROFONO. L'audio non viene registrato ne' inviato."
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
    Source/WindLabVirtualInstrumentPanel.cpp
    Source/WindLabVirtualInstrumentPanel.h
    Source/Dsp/VirtualFingering.h
    Source/WindLabExpressionPanel.cpp
    Source/WindLabExpressionPanel.h
    Source/WindLabParameterLearn.h
    Source/Dsp/ParameterCCMap.h
    Source/Dsp/MicBreath.h
    Source/Dsp/SectionMode.h
    Source/Dsp/RoomImpulse.h
    Source/Dsp/WindEngine.h
    Source/Dsp/AcousticExpression.h
    Source/Dsp/DynamicLayerMorph.h
    Source/Dsp/LegatoHandoff.h
    Source/Dsp/ToneEQ.h
    Source/Dsp/RecordedWinds.h
    Source/Dsp/SampleLoopPlan.h
    Source/Dsp/VelocityResponse.h
    Source/Dsp/PhraseEngine.h
    Source/Dsp/PhraseRecorder.h
    Source/Dsp/MidiLearnBindings.h
    Source/Dsp/HybridMidiRoute.h
    Source/Dsp/BreathCalibration.h
    Source/Dsp/WindControllerProfiles.h
    Source/Dsp/MidiInputMonitor.h)

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

# 0.9.10 recorded rooms (OpenAIR, CC BY 4.0) for the convolution reverb, if downloaded.
file(GLOB WINDLAB_ROOM_WAVS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Rooms/room_*.wav")
if(WINDLAB_ROOM_WAVS)
    juce_add_binary_data(WindLabRoomData
        HEADER_NAME WindLabRoomData.h
        NAMESPACE WindLabRoomData
        SOURCES ${WINDLAB_ROOM_WAVS})
    target_link_libraries(WindLab PRIVATE WindLabRoomData)
    target_compile_definitions(WindLab PRIVATE WINDLAB_HAS_RECORDED_ROOMS=1)
else()
    message(STATUS "No recorded rooms: the convolution reverb uses generated rooms.")
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
    add_executable(windlab_sample_loop_quality_smoke tests/sample-loop-quality-smoke.cpp)
    target_include_directories(windlab_sample_loop_quality_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_sample_loop_quality_smoke COMMAND windlab_sample_loop_quality_smoke)

    add_executable(windlab_smoke tests/smoke.cpp)
    target_include_directories(windlab_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_smoke COMMAND windlab_smoke)

    add_executable(windlab_legato_handoff_smoke tests/legato-handoff-smoke.cpp)
    target_include_directories(windlab_legato_handoff_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_legato_handoff_smoke COMMAND windlab_legato_handoff_smoke)

    add_executable(windlab_dynamic_layer_morph_smoke tests/dynamic-layer-morph-smoke.cpp)
    target_include_directories(windlab_dynamic_layer_morph_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_dynamic_layer_morph_smoke COMMAND windlab_dynamic_layer_morph_smoke)

    add_executable(windlab_acoustic_expression_smoke tests/acoustic-expression-smoke.cpp)
    target_include_directories(windlab_acoustic_expression_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_acoustic_expression_smoke COMMAND windlab_acoustic_expression_smoke)

    add_executable(windlab_expression_smoke tests/expression-smoke.cpp)
    target_include_directories(windlab_expression_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_expression_smoke COMMAND windlab_expression_smoke)

    add_executable(windlab_phrase_smoke tests/phrase-smoke.cpp)
    target_include_directories(windlab_phrase_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_phrase_smoke COMMAND windlab_phrase_smoke)

    add_executable(windlab_midi_learn_smoke tests/midi-learn-smoke.cpp)
    target_include_directories(windlab_midi_learn_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_midi_learn_smoke COMMAND windlab_midi_learn_smoke)

    add_executable(windlab_breath_calibration_smoke tests/breath-calibration-smoke.cpp)
    target_include_directories(windlab_breath_calibration_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_breath_calibration_smoke COMMAND windlab_breath_calibration_smoke)

    add_executable(windlab_midi_input_monitor_smoke tests/midi-input-monitor-smoke.cpp)
    target_include_directories(windlab_midi_input_monitor_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_midi_input_monitor_smoke COMMAND windlab_midi_input_monitor_smoke)

    add_executable(windlab_controller_profiles_smoke tests/wind-controller-profiles-smoke.cpp)
    target_include_directories(windlab_controller_profiles_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_controller_profiles_smoke COMMAND windlab_controller_profiles_smoke)

    add_executable(windlab_hybrid_midi_smoke tests/hybrid-midi-smoke.cpp)
    target_include_directories(windlab_hybrid_midi_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_hybrid_midi_smoke COMMAND windlab_hybrid_midi_smoke)

    add_executable(windlab_phrase_record_smoke tests/phrase-record-smoke.cpp)
    target_include_directories(windlab_phrase_record_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_phrase_record_smoke COMMAND windlab_phrase_record_smoke)

    add_executable(windlab_phrase_midi_smoke tests/phrase-midi-smoke.cpp)
    target_include_directories(windlab_phrase_midi_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_phrase_midi_smoke COMMAND windlab_phrase_midi_smoke)

    add_executable(windlab_phrase_grid_smoke tests/phrase-grid-smoke.cpp)
    target_include_directories(windlab_phrase_grid_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_phrase_grid_smoke COMMAND windlab_phrase_grid_smoke)
    add_executable(windlab_virtual_fingering_smoke tests/virtual-fingering-smoke.cpp)
    target_include_directories(windlab_virtual_fingering_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    add_test(NAME windlab_virtual_fingering_smoke COMMAND windlab_virtual_fingering_smoke)
    foreach(test_name expression_097 phrase_sync parameter_cc_map rooms_section_mic)
        string(REPLACE "_" "-" test_file "${test_name}")
        add_executable(windlab_${test_name}_smoke tests/${test_file}-smoke.cpp)
        target_include_directories(windlab_${test_name}_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
        add_test(NAME windlab_${test_name}_smoke COMMAND windlab_${test_name}_smoke)
    endforeach()
endif()
