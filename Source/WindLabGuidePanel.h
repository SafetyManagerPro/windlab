#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// In-plugin, offline, topic-based manual. Reused by AU/VST3/Standalone.
// No web view, network request, message boxes, or audio-thread work.
class WindLabGuidePanel final : public juce::Component
{
public:
    enum Topic : int
    {
        start = 0, instruments, soundEngine, keyboard, midi, logic,
        studioOne, controls, phraseEditor, notation, troubleshooting, limitations,
        topicCount
    };

    explicit WindLabGuidePanel(std::function<void()> onClose);
    void openTopic(int topic);
    void paint(juce::Graphics&) override;
    void resized() override;
    static juce::StringArray sectionTitles();
    static juce::String articleFor(int topic);

private:
    std::function<void()> closeAction;
    juce::ComboBox sections;
    juce::TextEditor article;
    juce::TextButton previousButton {"< PRECEDENTE"}, nextButton {"SUCCESSIVO >"},
                     closeButton {"CHIUDI  X"};
    juce::Label subtitle;
    int currentTopic = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WindLabGuidePanel)
};
