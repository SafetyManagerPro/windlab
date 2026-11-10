#pragma once
// 0.9.7 universal MIDI Learn helpers shared by the main window and the panels:
// right-click (or ctrl-click) on a learnable control opens a small menu.
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <utility>

namespace windlabui
{
// "  [CC21]" when the parameter follows a CC, "  [MUOVI UN CONTROLLO]" while learning.
inline juce::String learnSuffix(WindLabAudioProcessor& processor, const juce::String& parameterID)
{
    const int index = processor.parameterIndexFor(parameterID);
    if (index < 0) return {};
    if (processor.parameterCC.armedParameter() == index) return "  [MUOVI UN CONTROLLO]";
    const int cc = processor.parameterCC.ccForParameter(index);
    return cc >= 0 ? "  [CC" + juce::String(cc) + "]" : juce::String();
}

inline void showLearnMenu(WindLabAudioProcessor& processor, const juce::String& parameterID, juce::Component* target)
{
    const int index = processor.parameterIndexFor(parameterID);
    if (index < 0) return;
    const int cc = processor.parameterCC.ccForParameter(index);
    juce::PopupMenu menu;
    menu.addSectionHeader("MIDI LEARN");
    menu.addItem(1, "Impara: muovi un controllo MIDI");
    menu.addItem(2, cc >= 0 ? "Rimuovi CC" + juce::String(cc) : juce::String("Nessun CC assegnato"), cc >= 0);
    if (processor.parameterCC.armedParameter() == index) menu.addItem(3, "Annulla apprendimento");
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(target),
        [&processor, index](int choice)
        {
            if (choice == 1) processor.parameterCC.arm(index);
            if (choice == 2) processor.parameterCC.clearParameter(index);
            if (choice == 3) processor.parameterCC.cancel();
        });
}

// Attach to any control: a right-click opens the MIDI Learn menu for its parameter.
class LearnOnRightClick final : public juce::MouseListener
{
public:
    LearnOnRightClick(WindLabAudioProcessor& p, juce::Component& c, juce::String id)
        : processor(p), component(&c), parameterID(std::move(id))
    {
        c.addMouseListener(this, true);
    }
    // Safe in any destruction order: the control may already be gone.
    ~LearnOnRightClick() override
    {
        if (auto* c = component.getComponent()) c->removeMouseListener(this);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            if (auto* c = component.getComponent()) showLearnMenu(processor, parameterID, c);
    }

private:
    WindLabAudioProcessor& processor;
    juce::Component::SafePointer<juce::Component> component;
    juce::String parameterID;
};
}
