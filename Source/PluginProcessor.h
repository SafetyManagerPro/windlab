#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Dsp/ToneEQ.h"
#include "Dsp/RecordedWinds.h"
#include "Dsp/PhraseEngine.h"
#include "Dsp/PhraseRecorder.h"
#include "Dsp/MidiLearnBindings.h"
#include "Dsp/MidiInputMonitor.h"
#include "Dsp/ParameterCCMap.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <memory>
#include <vector>

class WindLabAudioProcessor final : public juce::AudioProcessor, private juce::Timer
{
public:
    WindLabAudioProcessor();
    ~WindLabAudioProcessor() override { stopTimer(); }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;
    windlab::PhraseEngine phrase; // persistent, edit-safe expression lane and MIDI sequencer
    windlab::PhraseRecorder phraseRecorder; // isolated, non-destructive two-bar MIDI recorder
    windlab::MidiLearnBindings midiLearn; // atomic CC assignments, persisted with project
    windlab::MidiInputMonitor midiInputMonitor; // MIDI diagnostic snapshots, not persisted
    // Notes, breath and bend from the on-screen instrument (UI thread -> audio thread).
    juce::MidiMessageCollector virtualInstrumentInput;
    std::atomic<bool> virtualInputReady {false};

    // 0.9.7 universal MIDI Learn: any parameter (by index in getParameters()) follows a CC.
    windlab::ParameterCCMap parameterCC;
    int parameterIndexFor(const juce::String& parameterID) const;
    juce::String parameterIDAt(int index) const;

    // 0.9.7 presets: factory sounds and user files (*.windlabpreset). Presets change
    // the sound only, never MIDI routing, channels or breath calibration.
    static juce::StringArray factoryPresetNames();
    void applyFactoryPreset(int index);
    static juce::File userPresetFolder();
    bool saveUserPreset(const juce::File& file) const;
    bool loadUserPreset(const juce::File& file);

private:
    void timerCallback() override;
    void setParameterPlain(const juce::String& id, float plainValue);
    static bool presetSkips(const juce::String& id);
    std::unique_ptr<std::atomic<float>[]> pendingCCValues; // audio -> message thread
    int learnableCount = 0;
    windlab::RecordedWinds sampleBank;
    std::unique_ptr<juce::Synthesiser> synthesiser;
    windlab::ToneEQ eq;
    juce::Reverb stereoReverb;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> reverbMix, reverbSize, reverbDamping;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain, roomAmount, panPosition;
    bool lastHostPlaying = false; // audio thread only: DAW transport edge detection
    std::vector<float> earlyReflectionL, earlyReflectionR;
    std::size_t reflectionPositionL = 0, reflectionPositionR = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WindLabAudioProcessor)
};
