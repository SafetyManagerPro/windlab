#pragma once
#include "Dsp/MidiLearnBindings.h"
#include "Dsp/WindControllerProfiles.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>

// One shared, offline MIDI Learn panel for Standalone and hosted plugins.
// All event capture and value translation happen in the audio callback.
class WindLabMidiLearnPanel final : public juce::Component, private juce::Timer
{
public:
    WindLabMidiLearnPanel(windlab::MidiLearnBindings& bindings,
                          windlab::MidiInputMonitor& monitor,
                          std::function<void()> close, std::function<void()> help);
    ~WindLabMidiLearnPanel() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void updateState();
    void timerCallback() override { updateState(); }
    windlab::MidiLearnBindings& midiLearn;
    windlab::MidiInputMonitor& midiMonitor;
    std::function<void()> onClose, onHelp;
    std::array<juce::TextButton, 3> learnButtons;
    std::array<juce::Label, 3> targetLabels, ccLabels;
    juce::Label status, instructions, noteMonitorLabel, ccMonitorLabel;
    juce::ComboBox profileBox;
    juce::TextButton applyProfileButton {"APPLICA PROFILO"};
    juce::TextButton resetButton {"RESET CC"}, cancelButton {"ANNULLA"},
                     helpButton {"?"}, closeButton {"CHIUDI X"};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WindLabMidiLearnPanel)
};
