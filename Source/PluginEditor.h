#pragma once
#include "PluginProcessor.h"
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
    juce::ComboBox instrumentBox, articulationBox;
    juce::Label instrumentLabel, articulationLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> instrumentAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> articulationAttachment;
    std::array<juce::Slider, 6> knobs;
    std::array<juce::Label, 6> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 6> knobAttachments;
    juce::MidiKeyboardComponent keyboard;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WindLabAudioProcessorEditor)
};
