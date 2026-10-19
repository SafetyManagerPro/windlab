#include "PluginEditor.h"

WindLabAudioProcessorEditor::WindLabAudioProcessorEditor(WindLabAudioProcessor& p)
    : juce::AudioProcessorEditor(&p),
      processor(p),
      keyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setSize(900, 520);
    const juce::StringArray instruments {"Trumpet", "Tenor Sax", "Trombone", "Flute", "Clarinet", "French Horn"};
    for (int i = 0; i < instruments.size(); ++i) instrumentBox.addItem(instruments[i], i + 1);
    const juce::StringArray articulations {"Sustain — long notes", "Staccato — short", "Swell — slow rise"};
    for (int i = 0; i < articulations.size(); ++i) articulationBox.addItem(articulations[i], i + 1);

    for (auto* choice : {&instrumentBox, &articulationBox})
    {
        choice->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff182731));
        choice->setColour(juce::ComboBox::textColourId, juce::Colour(0xfff6f2ea));
        choice->setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff3b4b52));
        addAndMakeVisible(*choice);
    }
    instrumentAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.parameters, "instrument", instrumentBox);
    articulationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.parameters, "articulation", articulationBox);

    instrumentLabel.setText("INSTRUMENT", juce::dontSendNotification);
    articulationLabel.setText("ARTICULATION", juce::dontSendNotification);
    for (auto* label : {&instrumentLabel, &articulationLabel})
    {
        label->setColour(juce::Label::textColourId, juce::Colour(0xff9db1b8));
        label->setFont(juce::Font(13.0f, juce::Font::bold));
        addAndMakeVisible(*label);
    }

    const std::array<juce::String, 6> names {"ATTACK", "RELEASE", "BRIGHTNESS", "AIR", "SPACE", "OUTPUT"};
    const std::array<juce::String, 6> ids {"attack", "release", "tone", "air", "space", "gain"};
    for (std::size_t i = 0; i < knobs.size(); ++i)
    {
        knobs[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 88, 21);
        knobs[i].setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffe4a668));
        knobs[i].setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff30444b));
        knobs[i].setColour(juce::Slider::thumbColourId, juce::Colour(0xfff3c694));
        knobs[i].setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        knobs[i].setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(knobs[i]);
        labels[i].setText(names[i], juce::dontSendNotification);
        labels[i].setJustificationType(juce::Justification::centred);
        labels[i].setColour(juce::Label::textColourId, juce::Colour(0xffb7c6cb));
        labels[i].setFont(juce::Font(12.0f, juce::Font::bold));
        addAndMakeVisible(labels[i]);
        knobAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.parameters, ids[i], knobs[i]);
    }
    keyboard.setAvailableRange(36, 96);
    keyboard.setKeyWidth(16.0f);
    keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour(0xffe6a86e));
    addAndMakeVisible(keyboard);
}

void WindLabAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff0c171f), 0, 0,
                                          juce::Colour(0xff24343d), full.getRight(), full.getBottom(), false));
    g.fillAll();

    g.setColour(juce::Colour(0xff20313b));
    g.fillRoundedRectangle(22.0f, 107.0f, full.getWidth() - 44.0f, 263.0f, 13.0f);
    g.setColour(juce::Colour(0xff33454d));
    g.drawRoundedRectangle(22.0f, 107.0f, full.getWidth() - 44.0f, 263.0f, 13.0f, 1.0f);
    g.setColour(juce::Colour(0xffe6ae75));
    g.setFont(juce::Font(31.0f, juce::Font::bold));
    g.drawText("WINDLAB", 28, 13, 440, 50, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xffd1dde0));
    g.setFont(juce::Font(13.0f));
    g.drawText("WINDS INSTRUMENT  /  VERSION 0.2  /  SYNTHETIC SOUND ENGINE", 31, 64, 800, 23,
               juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xffaebfc6));
    g.drawText("MIDI CC1 VIBRATO    ·    CC2 BREATH    ·    CC11 EXPRESSION    ·    AFTERTOUCH    ·    BEND ±2", 29, 385, 840, 20,
               juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff80969e));
    g.drawText("Audio is modelled with harmonic synthesis. No sampled brass or woodwinds included yet.", 29, 408, 835, 21,
               juce::Justification::centredLeft);
}

void WindLabAudioProcessorEditor::resized()
{
    const int inset = 37;
    const int fieldW = (getWidth() - inset * 2 - 22) / 2;
    instrumentLabel.setBounds(inset, 117, fieldW, 24);
    articulationLabel.setBounds(inset + fieldW + 22, 117, fieldW, 24);
    instrumentBox.setBounds(inset, 142, fieldW, 36);
    articulationBox.setBounds(inset + fieldW + 22, 142, fieldW, 36);

    const int left = 30;
    const int groupWidth = (getWidth() - 60) / 6;
    for (int i = 0; i < 6; ++i)
    {
        labels[static_cast<std::size_t>(i)].setBounds(left + groupWidth * i, 216, groupWidth, 23);
        knobs[static_cast<std::size_t>(i)].setBounds(left + groupWidth * i, 238, groupWidth, 117);
    }
    keyboard.setBounds(25, 445, getWidth() - 50, 60);
}
