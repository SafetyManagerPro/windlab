#pragma once
#include "PluginProcessor.h"
#include "WindLabParameterLearn.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <memory>
#include <vector>

// 0.9.7 "ESPRESSIONE": legato, vibrato, growl / flutter, jazz effects, pan,
// presets and the overview of the universal MIDI Learn assignments.
class WindLabExpressionPanel final : public juce::Component, private juce::Timer
{
public:
    WindLabExpressionPanel(WindLabAudioProcessor& processor, std::function<void()> close, std::function<void()> help);
    ~WindLabExpressionPanel() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    void refreshPresetList();
    juce::String assignmentsText() const;

    WindLabAudioProcessor& processor;
    std::function<void()> onClose, onHelp;
    static constexpr int knobCount = 6;
    std::array<juce::Slider, knobCount> knobs;
    std::array<juce::Label, knobCount> knobLabels;
    std::array<juce::String, knobCount> knobNames;
    std::array<juce::String, knobCount> knobIDs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, knobCount> attachments;
    juce::ComboBox legatoModeBox, jazzBox, presetBox, sectionBox, roomBox, unisonBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> legatoModeAttachment, jazzAttachment,
                                                                            sectionAttachment, roomAttachment, unisonAttachment;
    juce::Label legatoModeLabel, jazzLabel, presetLabel, assignments, sectionLabel, roomLabel, micLabel, unisonLabel;
    juce::Slider micSensitivity;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> micAttachment;
    float shownMicLevel = 0.0f;
    juce::TextButton savePresetButton {"SALVA PRESET..."}, openPresetButton {"APRI PRESET..."},
                     clearCCButton {"RIMUOVI TUTTI I CC"}, closeButton {"CHIUDI X"}, helpButton {"?"};
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Array<juce::File> userPresets;
    std::vector<std::unique_ptr<windlabui::LearnOnRightClick>> learners;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WindLabExpressionPanel)
};
