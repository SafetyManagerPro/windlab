#pragma once
#include "PluginProcessor.h"
#include "PhraseEditor.h"
#include <array>
#include <memory>

class WindLabAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit WindLabAudioProcessorEditor(WindLabAudioProcessor&);
    ~WindLabAudioProcessorEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    WindLabAudioProcessor& processor;
    juce::ComboBox instrumentBox, articulationBox, sourceBox;
    juce::ComboBox inputModeBox, velocityResponseBox;
    juce::Label instrumentLabel, articulationLabel, sourceLabel;
    juce::Label inputModeLabel, velocityLabel, octaveLabel, velocityResponseLabel;
    juce::TextButton octaveDown {"-"}, octaveUp {"+"}, midiHelpButton {"MIDI ?"};
    juce::Slider keyVelocity;
    juce::TextButton phraseButton {"PHRASE EDITOR"};
    std::unique_ptr<WindLabPhraseEditor> phrasePanel;
    int keyboardOctave = 5; // physical keyboard A = MIDI 60 (middle C)
    void changeKeyboardOctave(int delta);
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> instrumentAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> articulationAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> inputModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> velocityResponseAttachment;
    std::array<juce::Slider, 15> knobs;
    std::array<juce::Label, 15> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 15> knobAttachments;
    juce::MidiKeyboardComponent keyboard;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WindLabAudioProcessorEditor)
};
