#include "PluginEditor.h"

WindLabAudioProcessorEditor::WindLabAudioProcessorEditor(WindLabAudioProcessor& p)
    : juce::AudioProcessorEditor(&p),
      processor(p),
      keyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setSize(1080, 775);
    const juce::StringArray instruments {"Trumpet", "Tenor Sax", "Trombone", "Flute", "Clarinet", "French Horn"};
    for (int i = 0; i < instruments.size(); ++i)
        instrumentBox.addItem(instruments[i], i + 1);
    const juce::StringArray articulations {"Sustain - held notes", "Staccato - short", "Swell - slow rise"};
    for (int i = 0; i < articulations.size(); ++i)
        articulationBox.addItem(articulations[i], i + 1);

    for (auto* choice : {&instrumentBox, &articulationBox})
    {
        choice->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff182831));
        choice->setColour(juce::ComboBox::textColourId, juce::Colour(0xfff6f2ea));
        choice->setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff3c555f));
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
        label->setColour(juce::Label::textColourId, juce::Colour(0xff9cb6bd));
        label->setFont(juce::Font(13.0f, juce::Font::bold));
        addAndMakeVisible(*label);
    }

    const std::array<juce::String, 15> names {
        "ATTACK", "RELEASE", "BREATH LENGTH", "VIBRATO", "NATURAL",
        "BRIGHTNESS", "AIR", "BASS dB", "MID dB", "TREBLE dB",
        "ROOM", "REVERB", "REVERB SIZE", "DAMPING", "OUTPUT"
    };
    const std::array<juce::String, 15> ids {
        "attack", "release", "breathLength", "vibrato", "humanize",
        "tone", "air", "bass", "mid", "treble",
        "space", "reverb", "reverbSize", "reverbDamp", "gain"
    };
    for (std::size_t i = 0; i < knobs.size(); ++i)
    {
        knobs[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 85, 21);
        knobs[i].setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffe8ac77));
        knobs[i].setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff314b53));
        knobs[i].setColour(juce::Slider::thumbColourId, juce::Colour(0xffffd1a1));
        knobs[i].setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        knobs[i].setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(knobs[i]);

        labels[i].setText(names[i], juce::dontSendNotification);
        labels[i].setJustificationType(juce::Justification::centred);
        labels[i].setColour(juce::Label::textColourId, juce::Colour(0xffbfd0d3));
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
    const auto full = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff0c171f), 0, 0,
                                          juce::Colour(0xff253b44), full.getRight(), full.getBottom(), false));
    g.fillAll();

    g.setColour(juce::Colour(0xffe8ad7a));
    g.setFont(juce::Font(34.0f, juce::Font::bold));
    g.drawText("WINDLAB", 27, 12, 475, 48, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xffc2d2d7));
    g.setFont(juce::Font(13.0f));
    g.drawText("NATURAL EXPRESSION  /  0.3  /  LIVE MIDI + TONE SHAPING", 32, 63, 910, 21,
               juce::Justification::centredLeft);

    constexpr int rowTop[3] {207, 359, 511};
    constexpr const char* groups[3] {"01   PERFORMANCE / BREATH", "02   TIMBRE / 3-BAND EQ", "03   SPACE / OUTPUT"};
    for (int group = 0; group < 3; ++group)
    {
        const auto y = static_cast<float>(rowTop[group]);
        g.setColour(juce::Colour(0xff1d313b));
        g.fillRoundedRectangle(22.0f, y, full.getWidth() - 44.0f, 142.0f, 12.0f);
        g.setColour(juce::Colour(0xff35515b));
        g.drawRoundedRectangle(22.0f, y, full.getWidth() - 44.0f, 142.0f, 12.0f, 1.0f);
        g.setColour(juce::Colour(0xffe0b488));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText(groups[group], 35, rowTop[group] + 8, 490, 20, juce::Justification::centredLeft);
    }
    g.setColour(juce::Colour(0xffbbccd0));
    g.setFont(juce::Font(12.0f));
    g.drawText("CC1 VIBRATO  /  CC2 BREATH  /  CC11 EXPRESSION  /  AFTERTOUCH  /  PITCH BEND",
               29, 658, 960, 19, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff8faab3));
    g.drawText("BREATH LENGTH: 30 = continuous while held. Synthetic instruments, not sampled acoustic winds.",
               29, 679, 1020, 18, juce::Justification::centredLeft);
}

void WindLabAudioProcessorEditor::resized()
{
    constexpr int inset = 35;
    const int fieldW = (getWidth() - inset * 2 - 22) / 2;
    instrumentLabel.setBounds(inset, 104, fieldW, 22);
    articulationLabel.setBounds(inset + fieldW + 22, 104, fieldW, 22);
    instrumentBox.setBounds(inset, 130, fieldW, 41);
    articulationBox.setBounds(inset + fieldW + 22, 130, fieldW, 41);

    const int contentWidth = getWidth() - 62;
    const int cellWidth = contentWidth / 5;
    constexpr int rowTop[3] {207, 359, 511};
    for (int i = 0; i < 15; ++i)
    {
        const int row = i / 5;
        const int column = i % 5;
        const int x = 31 + cellWidth * column;
        labels[static_cast<std::size_t>(i)].setBounds(x, rowTop[row] + 27, cellWidth, 20);
        knobs[static_cast<std::size_t>(i)].setBounds(x + (cellWidth - 122) / 2, rowTop[row] + 47, 122, 91);
    }
    keyboard.setBounds(26, 711, getWidth() - 52, 55);
}
