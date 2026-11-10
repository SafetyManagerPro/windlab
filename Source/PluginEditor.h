#pragma once
#include "PluginProcessor.h"
#include "PhraseEditor.h"
#include "WindLabGuidePanel.h"
#include "WindLabMidiLearnPanel.h"
#include "WindLabVirtualInstrumentPanel.h"
#include "WindLabExpressionPanel.h"
#include "WindLabParameterLearn.h"
#include <vector>
#include <array>
#include <memory>

class WindLabAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WindLabAudioProcessorEditor(WindLabAudioProcessor&);
    ~WindLabAudioProcessorEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    WindLabAudioProcessor& processor;
    juce::ComboBox instrumentBox, articulationBox, sourceBox;
    juce::ComboBox inputModeBox, velocityResponseBox, performanceModeBox, noteMidiChannelBox, breathMidiChannelBox;
    juce::Label instrumentLabel, articulationLabel, sourceLabel;
    juce::Label inputModeLabel, velocityLabel, octaveLabel, velocityResponseLabel, performanceModeLabel, noteMidiChannelLabel, breathMidiChannelLabel;
    juce::TextButton octaveDown {"-"}, octaveUp {"+"}, midiHelpButton {"MIDI ?"};
    juce::Slider keyVelocity;
    juce::Slider breathThresholdSlider, breathCurveSlider;
    juce::Label breathThresholdLabel, breathCurveLabel, breathSourceLabel;
    juce::ComboBox breathSourceBox;
    juce::TextButton phraseButton {"PHRASE EDITOR"}, helpButton {"?"}, midiLearnButton {"MIDI LEARN"}, quickRecordButton {"REC MIDI"}, virtualInstrumentButton {"STRUMENTO A VIDEO"}, expressionButton {"ESPRESSIONE"};
    std::unique_ptr<WindLabPhraseEditor> phrasePanel;
    std::unique_ptr<WindLabGuidePanel> guidePanel;
    std::unique_ptr<WindLabMidiLearnPanel> midiLearnPanel;
    std::unique_ptr<WindLabVirtualInstrumentPanel> virtualInstrumentPanel;
    std::unique_ptr<WindLabExpressionPanel> expressionPanel;
    std::array<juce::String, 15> knobNames, knobIDs;
    std::vector<std::unique_ptr<windlabui::LearnOnRightClick>> learners;
    int keyboardOctave = 5; // physical keyboard A = MIDI 60 (middle C)
    void changeKeyboardOctave(int delta);
    void showGuide(int topic);
    void timerCallback() override;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> instrumentAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> articulationAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> inputModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> velocityResponseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> performanceModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> breathThresholdAttachment, breathCurveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> noteMidiChannelAttachment, breathMidiChannelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> breathSourceAttachment;
    std::array<juce::Slider, 15> knobs;
    std::array<juce::Label, 15> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 15> knobAttachments;
    juce::MidiKeyboardComponent keyboard;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WindLabAudioProcessorEditor)
};
