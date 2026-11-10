#include "WindLabMidiLearnPanel.h"
#include <utility>

WindLabMidiLearnPanel::WindLabMidiLearnPanel(windlab::MidiLearnBindings& bindings,
                                           windlab::MidiInputMonitor& monitor,
                                           std::function<void()> close,
                                           std::function<void()> help)
    : midiLearn(bindings), midiMonitor(monitor), onClose(std::move(close)), onHelp(std::move(help))
{
    setOpaque(true);
    const juce::StringArray names {"VIBRATO", "FIATO / BREATH", "VOLUME / EXPRESSION"};
    for (int i = 0; i < 3; ++i)
    {
        auto& learn = learnButtons[static_cast<std::size_t>(i)];
        learn.setButtonText("IMPARA");
        learn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff355763));
        learn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        learn.onClick = [this, i] {
            midiLearn.arm(i + 1);
            updateState();
        };
        addAndMakeVisible(learn);
        auto& target = targetLabels[static_cast<std::size_t>(i)];
        target.setText(names[i], juce::dontSendNotification);
        target.setColour(juce::Label::textColourId, juce::Colour(0xffe1e9e8));
        target.setFont(juce::Font(19.0f, juce::Font::bold));
        addAndMakeVisible(target);
        auto& cc = ccLabels[static_cast<std::size_t>(i)];
        cc.setColour(juce::Label::textColourId, juce::Colour(0xffe7ae80));
        cc.setFont(juce::Font(18.0f, juce::Font::bold));
        addAndMakeVisible(cc);
    }

    for (auto* label : {&noteMonitorLabel, &ccMonitorLabel, &pressureMonitorLabel})
    {
        label->setColour(juce::Label::textColourId, juce::Colour(0xffb6dbcb));
        label->setFont(juce::Font(14.0f, juce::Font::bold));
        addAndMakeVisible(*label);
    }
    status.setColour(juce::Label::textColourId, juce::Colour(0xffb6dbcb));
    status.setFont(juce::Font(15.0f, juce::Font::bold));
    addAndMakeVisible(status);

    instructions.setText(
        "1. Premi IMPARA accanto alla funzione.    "
        "2. Muovi un pedale, slider o controllo MIDI.    "
        "3. Il numero CC verra' assegnato automaticamente.",
        juce::dontSendNotification);
    instructions.setColour(juce::Label::textColourId, juce::Colour(0xffbfd2d6));
    instructions.setFont(juce::Font(14.0f));
    addAndMakeVisible(instructions);

    // Explicit profiles: do not silently override a user's MIDI Learn map.
    profileBox.addItem("STANDARD CC2 / EWI / AEROPHONE", windlab::WindControllerProfiles::StandardCC2);
    profileBox.addItem("ARTINOISE RE.CORDER CC11", windlab::WindControllerProfiles::ArtinoiseReCorderCC11);
    profileBox.setSelectedId(windlab::WindControllerProfiles::StandardCC2, juce::dontSendNotification);
    profileBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff182831));
    profileBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    addAndMakeVisible(profileBox);
    addAndMakeVisible(applyProfileButton);
    applyProfileButton.onClick = [this] {
        if (windlab::WindControllerProfiles::apply(midiLearn, profileBox.getSelectedId()))
            updateState();
    };
    for (auto* button : {&resetButton, &cancelButton, &helpButton, &closeButton, &applyProfileButton})
    {
        button->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff294c59));
        button->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        addAndMakeVisible(*button);
    }
    resetButton.onClick = [this] { midiLearn.reset(); updateState(); };
    cancelButton.onClick = [this] { midiLearn.cancel(); updateState(); };
    helpButton.onClick = [this] { if (onHelp) onHelp(); };
    closeButton.onClick = [this] {
        midiLearn.cancel();
        if (onClose) onClose();
    };
    closeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffb17c52));
    updateState();
    startTimerHz(8);
}

void WindLabMidiLearnPanel::updateState()
{
    const auto target = midiLearn.armedTarget();
    constexpr int standardCC[] {1, 2, 11};
    for (int i = 0; i < 3; ++i)
    {
        const int learned = midiLearn.binding(i + 1);
        const juce::String display = "CC " + juce::String(learned) +
            (learned == standardCC[i] ? " (STANDARD)" : " (PERSONALIZZATO)");
        auto& label = ccLabels[static_cast<std::size_t>(i)];
        if (label.getText() != display)
            label.setText(display, juce::dontSendNotification);
        auto& learn = learnButtons[static_cast<std::size_t>(i)];
        const juce::String caption = target == i + 1 ? "IN ASCOLTO..." : "IMPARA";
        if (learn.getButtonText() != caption)
            learn.setButtonText(caption);
    }
    const juce::String message = target != 0
        ? "IN ASCOLTO: muovi un controllo MIDI fisico (canali 1-15)."
        : "PRONTO. Mappature salvate nel progetto. MIDI interno canale 16 escluso.";
    if (status.getText() != message)
        status.setText(message, juce::dontSendNotification);

    const auto note = midiMonitor.noteSnapshot();
    const auto cc = midiMonitor.ccSnapshot();
    const juce::String noteText = note.valid
        ? "ULTIMA NOTA: CH " + juce::String(note.channel) + "  NOTE "
          + juce::String(note.pitch) + (note.on ? "  ON" : "  OFF")
        : "ULTIMA NOTA: nessun dato MIDI";
    const juce::String ccText = cc.valid
        ? "ULTIMO CC: CH " + juce::String(cc.channel)
          + "  CC " + juce::String(cc.number) + "  VAL " + juce::String(cc.value)
        : "ULTIMO CC: nessun dato MIDI";
    if (noteMonitorLabel.getText() != noteText)
        noteMonitorLabel.setText(noteText, juce::dontSendNotification);
    if (ccMonitorLabel.getText() != ccText)
        ccMonitorLabel.setText(ccText, juce::dontSendNotification);
    const auto at = midiMonitor.pressureSnapshot();
    const juce::String atText = at.valid
        ? "ULTIMO AFTERTOUCH: CH " + juce::String(at.channel) + "  VAL " + juce::String(at.value)
        : "ULTIMO AFTERTOUCH: nessun dato MIDI";
    if (pressureMonitorLabel.getText() != atText)
        pressureMonitorLabel.setText(atText, juce::dontSendNotification);
}

void WindLabMidiLearnPanel::paint(juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff0f1d25), 0, 0,
                                         juce::Colour(0xff294854),
                                         full.getRight(), full.getBottom(), false));
    g.fillAll();
    g.setColour(juce::Colour(0xffedb389));
    g.setFont(juce::Font(30.0f, juce::Font::bold));
    g.drawText("WINDLAB  /  MIDI LEARN", 30, 18, getWidth() - 350, 43,
               juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xffafc5ce));
    g.setFont(juce::Font(15.0f));
    g.drawText("Assegna i messaggi CC di una tastiera, EWI, slider o pedale alle espressioni dei fiati.",
               34, 79, getWidth() - 75, 32, juce::Justification::centredLeft);

    for (int i = 0; i < 3; ++i)
    {
        const float y = static_cast<float>(190 + i * 108);
        g.setColour(juce::Colour(0xff1d3843));
        g.fillRoundedRectangle(35.0f, y, getWidth() - 70.0f, 90.0f, 11.0f);
        g.setColour(juce::Colour(0xff41636d));
        g.drawRoundedRectangle(35.0f, y, getWidth() - 70.0f, 90.0f, 11.0f, 1.0f);
    }
    g.setColour(juce::Colour(0xffacbfc5));
    g.setFont(juce::Font(14.0f));
    g.drawText("PROFILO MIDI: controlla il CC del dispositivo prima di applicare",
               38, 582, getWidth() - 76, 22, juce::Justification::centredLeft);
    g.setFont(juce::Font(14.0f));
    g.drawFittedText(
        "Se usi CC64 (pedale sustain) per un'espressione, quel pedale non inviera' "
        "il sustain tradizionale mentre e' assegnato. Le altre note MIDI restano invariate.",
        38, 687, getWidth() - 76, 51, juce::Justification::centredLeft, 2);
}

void WindLabMidiLearnPanel::resized()
{
    const int w = getWidth();
    const int h = getHeight();
    instructions.setBounds(38, 121, w - 77, 34);
    for (int i = 0; i < 3; ++i)
    {
        const int y = 190 + i * 108;
        targetLabels[static_cast<std::size_t>(i)].setBounds(55, y + 18, 345, 45);
        ccLabels[static_cast<std::size_t>(i)].setBounds(431, y + 18, 295, 45);
        learnButtons[static_cast<std::size_t>(i)].setBounds(w - 224, y + 18, 164, 46);
    }
    status.setBounds(38, 539, w - 70, 37);
    noteMonitorLabel.setBounds(38, 750, w - 76, 26);
    ccMonitorLabel.setBounds(38, 778, w - 76, 26);
    pressureMonitorLabel.setBounds(38, 806, w - 76, 26);
    profileBox.setBounds(38, 611, 420, 34);
    applyProfileButton.setBounds(474, 611, 190, 34);
    resetButton.setBounds(38, h - 76, 145, 40);
    cancelButton.setBounds(198, h - 76, 138, 40);
    helpButton.setBounds(w - 255, 20, 43, 41);
    closeButton.setBounds(w - 196, 20, 165, 41);
}
