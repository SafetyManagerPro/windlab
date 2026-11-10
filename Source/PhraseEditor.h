#pragma once
#include "Dsp/PhraseEngine.h"
#include "Dsp/PhraseRecorder.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>

// The phrase editor is an in-plugin overlay, not a floating native window.
// This allows DAW-hosted plugins and Standalone to share exactly the same UI.
class WindLabPhraseEditor final : public juce::Component, private juce::Timer
{
public:
    WindLabPhraseEditor(windlab::PhraseEngine& engine, windlab::PhraseRecorder& recorder,
                        std::function<void()> close, std::function<void()> help);
    ~WindLabPhraseEditor() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    // 0.9.9: mouse wheel over a note = that note's velocity (also inside a chord).
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    windlab::PhraseEngine& phrase;
    windlab::PhraseRecorder& capture;
    std::function<void()> onClose;
    std::function<void()> onHelp;
    juce::TextButton playButton {"PLAY"}, demoButton {"DEMO"},
                     clearButton {"CLEAR"}, legatoButton {"LEGATO: OFF"},
                     closeButton {"CLOSE"}, hostButton {"HOST BPM: OFF"},
                     scoreButton {"SCORE VIEW"}, helpButton {"?"},
                     recordButton {"REC"}, importButton {"IMPORTA"};
    juce::Label recordingStatus;
    juce::Slider tempoSlider;
    juce::ComboBox laneBox, gridBox;
    juce::TextButton exportButton {"EXPORT MIDI"}, syncButton {"SEGUI PLAY DAW: OFF"};
    // 0.9.7: drag this button onto a DAW track to drop the phrase as a MIDI clip.
    class DragMidiButton final : public juce::TextButton
    {
    public:
        DragMidiButton() : juce::TextButton("TRASCINA IN DAW") {}
        std::function<juce::File()> makeFile;
        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (dragging || e.getDistanceFromDragStart() < 6 || !makeFile) return;
            dragging = true;
            const auto file = makeFile();
            if (file.existsAsFile())
                juce::DragAndDropContainer::performExternalDragDropOfFiles({file.getFullPathName()}, false, this);
        }
        void mouseUp(const juce::MouseEvent& e) override
        {
            dragging = false;
            juce::TextButton::mouseUp(e);
        }
    private:
        bool dragging = false;
    };
    DragMidiButton dragButton;
    juce::File writePhraseToTempFile() const;
    std::unique_ptr<juce::FileChooser> exportChooser;
    juce::Label tempoLabel;
    int selectedStep = 0, firstDragStep = 0, dragSlot = 0;
    bool editingNote = false, editingLane = false;
    bool dragMoved = false, clickedExisting = false;
    bool scoreView = false;
    juce::Rectangle<float> rollBounds() const;
    juce::Rectangle<float> laneBounds() const;
    int stepAt(float x) const;
    int stepCount() const { return phrase.activeSteps(); }
    // 0.9.10 longer phrases: the roll shows two bars at a time.
    juce::ComboBox barsBox;
    juce::TextButton previousPage {"<"}, nextPage {">"};
    juce::Label pageLabel;
    int page = 0;
    int windowSteps() const { return 2 * windlab::stepsPerBarFor(phrase.getGrid()); }
    int pageCount() const { return juce::jmax(1, phrase.getBars() / 2); }
    int firstStep() const { return juce::jlimit(0, pageCount() - 1, page) * windowSteps(); }
    void updatePage();
    int slotAtPitch(int step, int pitch) const;
    int firstFreeSlot(int step) const;
    void exportMidi();
    int pitchAt(float y) const;
    void updateLane(float x, float y);
    void updateButtons();
    void timerCallback() override { updatePage(); updateButtons(); repaint(); }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WindLabPhraseEditor)
};
