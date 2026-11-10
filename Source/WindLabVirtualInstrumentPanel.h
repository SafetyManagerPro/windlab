#pragma once
#include "PluginProcessor.h"
#include "Dsp/VirtualFingering.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

// "STRUMENTO A VIDEO": the selected WindLab instrument drawn on screen and played
// with the mouse (keys latch on click) and the computer keyboard (keys held down),
// with breath from the space bar or the mouse. Notes, breath (learned breath CC)
// and slide pitch bend are queued to the audio thread through a MidiMessageCollector,
// so they behave exactly like external MIDI: legato, Phrase Editor REC, routing.
class WindLabVirtualInstrumentPanel final : public juce::Component, private juce::Timer
{
public:
    WindLabVirtualInstrumentPanel(WindLabAudioProcessor& processor,
                                  std::function<void()> close,
                                  std::function<void()> record,
                                  std::function<juce::String()> recordLabel,
                                  std::function<void()> help);
    ~WindLabVirtualInstrumentPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;
    void visibilityChanged() override;

    struct KeyShape
    {
        int key;                    // windlab::VirtualKey
        juce::Rectangle<float> box; // in the instrument's design space
        char pc;                    // computer key (lower case), 0 = mouse only
        bool round;
    };

private:
    void timerCallback() override;
    void refreshProfile();
    void releaseAll();
    void send(const juce::MidiMessage&);
    int midiChannel() const;
    std::uint32_t keyboardMask() const;
    std::uint32_t currentMask() const { return heldMask | latchedMask; }
    bool hasPartials() const { return profile.partialCount > 0; }
    juce::Rectangle<float> instrumentArea() const;
    juce::Rectangle<float> ladderArea() const;
    juce::Rectangle<float> breathMeterArea() const;
    juce::AffineTransform designTransform() const;
    juce::Point<float> designPoint(juce::Point<float> screen) const;
    void setSlideFromDesignX(float x);
    void drawInstrument(juce::Graphics&) const;
    void drawKeys(juce::Graphics&) const;
    void drawLadder(juce::Graphics&) const;
    juce::String helpText() const;

    WindLabAudioProcessor& processor;
    std::function<void()> onClose, onRecord, onHelp;
    std::function<juce::String()> recordText;

    juce::Label title, instrumentLabel;
    juce::ComboBox instrumentBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> instrumentAttachment;
    juce::TextButton closeButton {"CHIUDI X"}, helpButton {"?"};
    juce::TextButton spaceSourceButton {"SPAZIO"}, mouseSourceButton {"MOUSE"}, micSourceButton {"MICROFONO"};
    void chooseBreathSource();
    juce::Slider spaceStrength;
    juce::Label spaceStrengthLabel;
    juce::TextButton sharpButton {"# +1 (H)"}, releaseButton {"RILASCIA CHIAVI"}, recordButton {"REC MIDI"};
    juce::TextButton partialUp {"ARMONICO +"}, partialDown {"ARMONICO -"};
    // "MOSTRA DITEGGIATURA": when idle, show how to finger the note WindLab is playing.
    juce::TextButton showFingeringButton {"MOSTRA DITEGGIATURA"};
    bool guiding = false;
    int guideNote = -1;
    windlab::VirtualFingering guide;
    std::uint32_t drawMask = 0;
    int drawPartial = 0;
    float drawSlide = 1.0f;
    juce::Label help;

    int instrumentIndex = -1;
    windlab::VirtualProfile profile = windlab::virtualProfileFor(7);
    std::vector<KeyShape> shapes;
    juce::Rectangle<float> designBounds;
    std::uint32_t heldMask = 0, latchedMask = 0;
    int partial = 0;
    float slidePosition = 1.0f;
    bool draggingSlide = false, mouseBreathActive = false, spaceDown = false, useMouseBreath = false, useMicBreath = false;
    float mouseBreath = 0.0f, breath = 0.0f;
    int soundingNote = -1, lastBreathValue = -1, lastBend = 8192;
    windlab::VirtualNote shown;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WindLabVirtualInstrumentPanel)
};
