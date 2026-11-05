#include "WindMidiPanel.h"
#include <utility>

WindMidiPanel::WindMidiPanel(windlab::WindMidiSettings& config,
                             windlab::WindMidiMonitor& received,
                             std::function<void()> enable,
                             std::function<void()> close,
                             std::function<void()> help)
    : settings(config), monitor(received), onEnable(std::move(enable)),
      onClose(std::move(close)), onHelp(std::move(help))
{
    setOpaque(true);
    sourceBox.addItem("CC2 - BREATH", 1);
    sourceBox.addItem("CC11 - EXPRESSION PEDAL", 2);
    sourceBox.addItem("CHANNEL PRESSURE - AFTERTOUCH", 3);
    sourceBox.setSelectedId(settings.source()+1, juce::dontSendNotification);
    sourceBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff25414c));
    sourceBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    sourceBox.onChange = [this] { settings.setSource(sourceBox.getSelectedId()-1); };
    addAndMakeVisible(sourceBox);
    sourceLabel.setText("SORGENTE DEL FIATO", juce::dontSendNotification);
    sourceLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    for (auto* label : {&sourceLabel,&activity,&noteLabel,&eventLabel,&instructions})
    {
        label->setColour(juce::Label::textColourId, juce::Colour(0xffd6e7eb));
        addAndMakeVisible(*label);
    }
    activity.setFont(juce::Font(17.0f, juce::Font::bold));
    instructions.setText(
        "STANDALONE: Options > Audio/MIDI Settings, abilita il dispositivo MIDI.\n"
        "LOGIC / STUDIO ONE: seleziona il controller nella DAW e abilita il monitoraggio.\n"
        "Dispositivo rilevato dal sistema NON significa che il MIDI sia instradato a WindLab.",
        juce::dontSendNotification);
    instructions.setFont(juce::Font(13.0f));
    addAndMakeVisible(sourceLabel);
    devices.setMultiLine(true, true);
    devices.setReadOnly(true);
    devices.setCaretVisible(false);
    devices.setScrollbarsShown(true);
    devices.setFont(juce::Font(15.0f));
    devices.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff122c35));
    devices.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    addAndMakeVisible(devices);
    for (auto* b : {&enableButton,&refreshButton,&resetButton,&helpButton,&closeButton})
    {
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff395b65));
        b->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        addAndMakeVisible(*b);
    }
    enableButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff46765e));
    closeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffaa7652));
    enableButton.onClick = [this] { if(onEnable) onEnable(); };
    refreshButton.onClick = [this] { refreshDevices(); };
    resetButton.onClick = [this] {
        monitor.reset();
        lastReceivedCount = 0;
        lastMidiTick = 0;
        timerCallback();
    };
    helpButton.onClick = [this] { if(onHelp) onHelp(); };
    closeButton.onClick = [this] { if(onClose) onClose(); };
    refreshDevices();
    timerCallback();
    startTimerHz(6);
}

void WindMidiPanel::refreshDevices()
{
    const auto available = juce::MidiInput::getAvailableDevices();
    juce::String text = "DISPOSITIVI MIDI VISIBILI AL SISTEMA OPERATIVO\n";
    if (available.isEmpty()) text += "Nessun dispositivo MIDI trovato.\n";
    for (const auto& d : available) text += "- " + d.name + "\n";
    text += "\nPer ricevere MIDI abilita l'input nella Standalone o nella DAW.";
    devices.setText(text, false);
    devices.setCaretPosition(0);
}

void WindMidiPanel::timerCallback()
{
    const int id = settings.source()+1;
    if(sourceBox.getSelectedId()!=id) sourceBox.setSelectedId(id,juce::dontSendNotification);
    enableButton.setButtonText(settings.isEnabled() ? "WIND MIDI ATTIVO" : "ATTIVA WIND MIDI");
    const auto s = monitor.snapshot();
    // A cumulative count is historical, not proof that a device is
    // connected right now. Indicate recent traffic only within 3 seconds.
    const auto now = juce::Time::getMillisecondCounter();
    if (s.received != lastReceivedCount)
    {
        lastReceivedCount = s.received;
        lastMidiTick = now;
    }
    const bool recent = s.received > 0 &&
        static_cast<juce::uint32>(now - lastMidiTick) < 3000u;
    const juce::String activityText = s.received == 0
        ? "NESSUN MIDI RICEVUTO"
        : (recent ? "MIDI IN ARRIVO - " : "ULTIMO MIDI (STORICO) - ") +
          juce::String(static_cast<juce::int64>(s.received)) + " eventi";
    if(activity.getText()!=activityText)
        activity.setText(activityText,juce::dontSendNotification);
    const juce::String note = "NOTA: " + (s.lastNote<0 ? juce::String("-") : juce::String(s.lastNote)) +
                              " | VELOCITY: " + juce::String(s.velocity);
    if(noteLabel.getText()!=note) noteLabel.setText(note,juce::dontSendNotification);
    juce::String type = "NESSUNO";
    switch(s.kind)
    {
        case windlab::WindMidiMonitor::NoteOn: type="NOTE ON"; break;
        case windlab::WindMidiMonitor::NoteOff: type="NOTE OFF"; break;
        case windlab::WindMidiMonitor::Controller: type="CC "+juce::String(s.controller); break;
        case windlab::WindMidiMonitor::Pressure: type="CHANNEL PRESSURE"; break;
        case windlab::WindMidiMonitor::PitchWheel: type="PITCH BEND"; break;
        default: break;
    }
    const int v = s.kind==windlab::WindMidiMonitor::Pressure ? s.lastPressure : s.value;
    const juce::String msg = "MIDI CH: "+juce::String(s.channel)+" | "+type+" | VALORE: "+juce::String(v);
    if(eventLabel.getText()!=msg) eventLabel.setText(msg,juce::dontSendNotification);
}

void WindMidiPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff08161e));
    const auto r = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff3c6472));
    g.fillRoundedRectangle(5.0f,5.0f,r.getWidth()-10,r.getHeight()-10,12.0f);
    g.setColour(juce::Colour(0xff162c36));
    g.fillRoundedRectangle(8.0f,8.0f,r.getWidth()-16,r.getHeight()-16,11.0f);
    g.setFont(juce::Font(25.0f,juce::Font::bold));
    g.setColour(juce::Colour(0xffedb18a));
    g.drawText("WINDLAB / WIND MIDI - EWI",32,22,getWidth()-300,44,juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xffd7e3e5));
    g.setFont(juce::Font(14.0f));
    g.drawText("Configura il fiato e controlla se WindLab riceve note e controller MIDI.",
               36,80,getWidth()-72,32,juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff264653));
    g.fillRoundedRectangle(35,203,static_cast<float>(getWidth()-70),168.0f,9.0f);
    g.setFont(juce::Font(16.0f,juce::Font::bold));
    g.setColour(juce::Colour(0xfff0be9e));
    g.drawText("MONITOR MIDI IN INGRESSO",52,213,getWidth()-100,28,juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff99b7c1));
    g.setFont(juce::Font(13.0f));
    g.drawFittedText("Sono mostrati soltanto i messaggi realmente ricevuti; il canale 16 del Phrase Editor e' escluso.",
                     39,764,getWidth()-78,36,juce::Justification::centredLeft,2);
}

void WindMidiPanel::resized()
{
    const int w=getWidth();
    sourceLabel.setBounds(36,130,210,25);
    sourceBox.setBounds(36,158,420,38);
    enableButton.setBounds(w-288,155,250,39);
    activity.setBounds(53,254,w-104,30);
    noteLabel.setBounds(53,289,w-104,30);
    eventLabel.setBounds(53,326,w-104,31);
    resetButton.setBounds(38,385,172,39);
    refreshButton.setBounds(w-257,385,217,39);
    devices.setBounds(36,433,w-72,177);
    instructions.setBounds(37,621,w-74,115);
    helpButton.setBounds(w-242,24,45,38);
    closeButton.setBounds(w-188,24,150,38);
}
