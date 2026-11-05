#pragma once
#include "Dsp/WindMidiInput.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>

// Host-safe popup: the Standalone wrapper or DAW owns actual MIDI device routing.
class WindMidiPanel final : public juce::Component, private juce::Timer
{
public:
    WindMidiPanel(windlab::WindMidiSettings&, windlab::WindMidiMonitor&,
                  std::function<void()> enable, std::function<void()> close,
                  std::function<void()> help);
    ~WindMidiPanel() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void refreshDevices();
    windlab::WindMidiSettings& settings;
    windlab::WindMidiMonitor& monitor;
    std::function<void()> onEnable, onClose, onHelp;
    juce::ComboBox sourceBox;
    juce::Label sourceLabel, activity, noteLabel, eventLabel, instructions;
    juce::TextEditor devices;
    juce::TextButton enableButton {"ATTIVA WIND MIDI"}, refreshButton {"AGGIORNA DISPOSITIVI"},
                     helpButton {"?"}, closeButton {"CHIUDI X"};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WindMidiPanel)
};
