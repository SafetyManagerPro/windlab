#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Dsp/ToneEQ.h"
#include "Dsp/RecordedWinds.h"
#include "Dsp/PhraseEngine.h"
#include "Dsp/PhraseRecorder.h"
#include "Dsp/MidiLearnBindings.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <vector>

class WindLabAudioProcessor final : public juce::AudioProcessor
{
public:
    WindLabAudioProcessor();
    ~WindLabAudioProcessor() override = default;

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

private:
    windlab::RecordedWinds sampleBank;
    std::unique_ptr<juce::Synthesiser> synthesiser;
    windlab::ToneEQ eq;
    juce::Reverb stereoReverb;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> reverbMix, reverbSize, reverbDamping;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain, roomAmount;
    std::vector<float> earlyReflectionL, earlyReflectionR;
    std::size_t reflectionPositionL = 0, reflectionPositionR = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WindLabAudioProcessor)
};
