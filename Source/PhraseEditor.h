#pragma once
#include "Dsp/PhraseEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// The phrase editor is an in-plugin overlay, not a floating native window.
// This allows DAW-hosted plugins and Standalone to share exactly the same UI.
class WindLabPhraseEditor final : public juce::Component, private juce::Timer
{
public:
    WindLabPhraseEditor(windlab::PhraseEngine& engine, std::function<void()> close,
                        std::function<void()> help);
    ~WindLabPhraseEditor() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    windlab::PhraseEngine& phrase;
    std::function<void()> onClose;
    std::function<void()> onHelp;
    juce::TextButton playButton {"PLAY"}, demoButton {"DEMO"},
                     clearButton {"CLEAR"}, legatoButton {"LEGATO: OFF"},
                     closeButton {"CLOSE"}, hostButton {"HOST BPM: OFF"},
                     scoreButton {"SCORE VIEW"}, helpButton {"?"};
    juce::Slider tempoSlider;
    juce::ComboBox laneBox;
    juce::Label tempoLabel;
    int selectedStep = 0, firstDragStep = 0;
    bool editingNote = false, editingLane = false;
    bool scoreView = false;
    juce::Rectangle<float> rollBounds() const;
    juce::Rectangle<float> laneBounds() const;
    int stepAt(float x) const;
    int pitchAt(float y) const;
    void updateLane(float x, float y);
    void updateButtons();
    void timerCallback() override { updateButtons(); repaint(); }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WindLabPhraseEditor)
};
