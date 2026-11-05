#include "PluginEditor.h"
#include <array>
#include <utility>

WindLabAudioProcessorEditor::WindLabAudioProcessorEditor(WindLabAudioProcessor& p)
    : juce::AudioProcessorEditor(&p),
      processor(p),
      keyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setSize(1080, 922);
    const juce::StringArray instruments {"Trumpet", "Tenor Sax", "Trombone", "Flute", "Clarinet", "French Horn"};
    for (int i = 0; i < instruments.size(); ++i)
        instrumentBox.addItem(instruments[i], i + 1);
    const juce::StringArray articulations {"Sustain - held notes", "Staccato - short", "Swell - slow rise"};
    for (int i = 0; i < articulations.size(); ++i)
        articulationBox.addItem(articulations[i], i + 1);

    const juce::StringArray sources {"ACOUSTIC - REAL WAV", "HYBRID", "SYNTH"};
    for (int i = 0; i < sources.size(); ++i) sourceBox.addItem(sources[i], i + 1);
    for (auto* choice : {&instrumentBox, &articulationBox, &sourceBox})
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

    sourceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.parameters, "sourceMode", sourceBox);
    sourceLabel.setText("SOUND ENGINE", juce::dontSendNotification);
    instrumentLabel.setText("INSTRUMENT", juce::dontSendNotification);
    articulationLabel.setText("ARTICULATION", juce::dontSendNotification);
    for (auto* label : {&instrumentLabel, &articulationLabel, &sourceLabel})
    {
        label->setColour(juce::Label::textColourId, juce::Colour(0xff9cb6bd));
        label->setFont(juce::Font(13.0f, juce::Font::bold));
        addAndMakeVisible(*label);
    }

    performanceModeBox.addItem("TASTIERA / MIDI", 1);
    performanceModeBox.addItem("STRUMENTO A FIATO", 2);
    performanceModeBox.addItem("IBRIDO: NOTE CH + SOFFIO CH", 3);
    performanceModeBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff182831));
    performanceModeBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    addAndMakeVisible(performanceModeBox);
    performanceModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.parameters, "performanceMode", performanceModeBox);
    performanceModeLabel.setText("MIDI PERFORMANCE", juce::dontSendNotification);
    performanceModeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffbfd0d3));
    performanceModeLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    addAndMakeVisible(performanceModeLabel);

    for (int ch = 1; ch <= 15; ++ch)
    {
        noteMidiChannelBox.addItem(juce::String(ch), ch);
        breathMidiChannelBox.addItem(juce::String(ch), ch);
    }
    for (auto* box : {&noteMidiChannelBox, &breathMidiChannelBox})
    {
        box->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff182831));
        box->setColour(juce::ComboBox::textColourId, juce::Colours::white);
        addAndMakeVisible(*box);
    }
    noteMidiChannelLabel.setText("NOTE CH", juce::dontSendNotification);
    breathMidiChannelLabel.setText("BREATH CH", juce::dontSendNotification);
    for (auto* label : {&noteMidiChannelLabel, &breathMidiChannelLabel})
    {
        label->setColour(juce::Label::textColourId, juce::Colour(0xffbfd0d3));
        label->setFont(juce::Font(12.0f, juce::Font::bold));
        addAndMakeVisible(*label);
    }
    noteMidiChannelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.parameters, "noteMidiChannel", noteMidiChannelBox);
    breathMidiChannelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.parameters, "breathMidiChannel", breathMidiChannelBox);

    for (auto* slider : {&breathThresholdSlider, &breathCurveSlider})
    {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 23);
        addAndMakeVisible(*slider);
    }
    breathThresholdLabel.setText("SOFFIO: SOGLIA", juce::dontSendNotification);
    breathCurveLabel.setText("SOFFIO: CURVA", juce::dontSendNotification);
    for (auto* label : {&breathThresholdLabel, &breathCurveLabel})
    {
        label->setColour(juce::Label::textColourId, juce::Colour(0xffbfd0d3));
        label->setFont(juce::Font(12.0f, juce::Font::bold));
        addAndMakeVisible(*label);
    }
    breathThresholdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, "breathThreshold", breathThresholdSlider);
    breathCurveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, "breathCurve", breathCurveSlider);

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
    keyboard.setMouseClickGrabsKeyboardFocus(true);
    keyboard.setWantsKeyboardFocus(false);
    // Explicit Italian/English-friendly PC key map. The regular MIDI routing
    // remains active independently of this optional computer keyboard.
    keyboard.clearKeyMappings();
    const std::array<std::pair<int, int>, 13> computerKeys {{{'a', 0}, {'w', 1},
        {'s', 2}, {'e', 3}, {'d', 4}, {'f', 5}, {'t', 6}, {'g', 7},
        {'y', 8}, {'h', 9}, {'u', 10}, {'j', 11}, {'k', 12}}};
    for (const auto& key : computerKeys)
        keyboard.setKeyPressForNote(juce::KeyPress(key.first), key.second);
    keyboard.setVelocity(.82f, false);
    addAndMakeVisible(keyboard);

    for (auto* option : {&inputModeBox, &velocityResponseBox})
    {
        option->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff182831));
        option->setColour(juce::ComboBox::textColourId, juce::Colour(0xfff6f2ea));
        option->setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff3c555f));
        addAndMakeVisible(*option);
    }
    inputModeBox.addItem("PIANO + MIDI", 1);
    inputModeBox.addItem("PC / NOTEBOOK", 2);
    const juce::StringArray responses {"NATURAL", "LIGHT TOUCH",
                                       "WEIGHTED PIANO", "BREATH CONTROLLER"};
    for (int i = 0; i < responses.size(); ++i)
        velocityResponseBox.addItem(responses[i], i + 1);

    inputModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.parameters, "inputMode", inputModeBox);
    velocityResponseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.parameters, "velocityResponse", velocityResponseBox);

    inputModeLabel.setText("INPUT METHOD", juce::dontSendNotification);
    velocityResponseLabel.setText("MIDI DYNAMICS", juce::dontSendNotification);
    velocityLabel.setText("PC / MOUSE VELOCITY", juce::dontSendNotification);
    octaveLabel.setText("C4", juce::dontSendNotification);
    for (auto* label : {&inputModeLabel, &velocityResponseLabel, &velocityLabel, &octaveLabel})
    {
        label->setColour(juce::Label::textColourId, juce::Colour(0xffbfd0d3));
        label->setFont(juce::Font(12.0f, juce::Font::bold));
        addAndMakeVisible(*label);
    }
    octaveLabel.setJustificationType(juce::Justification::centred);

    for (auto* button : {&octaveDown, &octaveUp, &midiHelpButton})
    {
        button->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff314b53));
        button->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        addAndMakeVisible(*button);
    }
    octaveDown.onClick = [this] { changeKeyboardOctave(-1); };
    octaveUp.onClick = [this] { changeKeyboardOctave(1); };

    keyVelocity.setSliderStyle(juce::Slider::LinearHorizontal);
    keyVelocity.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 20);
    keyVelocity.setRange(.15, 1.0, .01);
    keyVelocity.setValue(.82, juce::dontSendNotification);
    keyVelocity.setColour(juce::Slider::trackColourId, juce::Colour(0xffe8ac77));
    keyVelocity.onValueChange = [this] {
        keyboard.setVelocity(static_cast<float>(keyVelocity.getValue()), false);
    };
    addAndMakeVisible(keyVelocity);

    inputModeBox.onChange = [this] {
        const bool pcEnabled = inputModeBox.getSelectedId() == 2;
        keyboard.setWantsKeyboardFocus(pcEnabled);
        if (pcEnabled)
            keyboard.grabKeyboardFocus();
        repaint();
    };
    // The short MIDI explanation is now a chapter of the full offline guide.
    midiHelpButton.onClick = [this] { showGuide(WindLabGuidePanel::midi); };
    // A DAW may restore a saved PC input mode before the editor opens.
    keyboard.setWantsKeyboardFocus(inputModeBox.getSelectedId() == 2);
    changeKeyboardOctave(0);

    quickRecordButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffa34b43));
    quickRecordButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    quickRecordButton.setTooltip("Registra due battute di MIDI senza coprire la tastiera PC. Dopo premi VEDI TAKE.");
    addAndMakeVisible(quickRecordButton);
    quickRecordButton.onClick = [this] {
        const int state = processor.phraseRecorder.status();
        if (state == windlab::PhraseRecorder::Recording)
            processor.phraseRecorder.requestStop();
        else if (state == windlab::PhraseRecorder::Ready)
        {
            phrasePanel->setVisible(true);
            phrasePanel->toFront(true);
        }
        else
        {
            processor.phrase.setPlaying(false);
            processor.phraseRecorder.requestStart();
        }
        // Return focus to virtual keys after interacting with REC MIDI.
        if (inputModeBox.getSelectedId() == 2 && processor.phraseRecorder.status() != windlab::PhraseRecorder::Ready)
            keyboard.grabKeyboardFocus();
        timerCallback();
    };

    phraseButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffb8754d));
    phraseButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(phraseButton);
    phrasePanel = std::make_unique<WindLabPhraseEditor>(processor.phrase, processor.phraseRecorder, [this] {
        phrasePanel->setVisible(false);
        if (inputModeBox.getSelectedId() == 2) keyboard.grabKeyboardFocus();
    }, [this] { showGuide(WindLabGuidePanel::phraseEditor); });
    phrasePanel->setBounds(getLocalBounds()); // created after initial setSize: must not stay 0x0
    addChildComponent(*phrasePanel); // overlay; host-safe, no floating native dialog
    phraseButton.onClick = [this] {
        const bool show = !phrasePanel->isVisible();
        phrasePanel->setVisible(show);
        if (show) phrasePanel->toFront(true);
    };

    helpButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff36525e));
    helpButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    helpButton.setTooltip("Apri la guida completa di WindLab");
    addAndMakeVisible(helpButton);
    helpButton.onClick = [this] { showGuide(WindLabGuidePanel::start); };

    // One shared guide handles both the main instrument and Phrase Editor,
    // without spawning a host-dependent secondary window.
    guidePanel = std::make_unique<WindLabGuidePanel>([this] {
        guidePanel->setVisible(false);
        if (midiLearnPanel && midiLearnPanel->isVisible())
            midiLearnPanel->toFront(true);
        else if (phrasePanel && phrasePanel->isVisible())
            phrasePanel->toFront(true);
        else if (inputModeBox.getSelectedId() == 2)
            keyboard.grabKeyboardFocus();
    });
    guidePanel->setBounds(getLocalBounds()); // standalone AU/VST3: initialize dimensions immediately
    addChildComponent(*guidePanel);

    midiLearnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff416a64));
    midiLearnButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    midiLearnButton.setTooltip("Assegna i controller MIDI: vibrato, fiato ed espressione");
    addAndMakeVisible(midiLearnButton);
    midiLearnPanel = std::make_unique<WindLabMidiLearnPanel>(
        processor.midiLearn,
        processor.midiInputMonitor,
        [this] {
            midiLearnPanel->setVisible(false);
            if (inputModeBox.getSelectedId() == 2)
                keyboard.grabKeyboardFocus();
        },
        [this] { showGuide(WindLabGuidePanel::midi); });
    midiLearnPanel->setBounds(getLocalBounds()); // same zero-size fix for new MIDI Learn panel
    addChildComponent(*midiLearnPanel);
    midiLearnButton.onClick = [this] {
        const bool show = !midiLearnPanel->isVisible();
        if (!show) processor.midiLearn.cancel();
        midiLearnPanel->setVisible(show);
        if (show) midiLearnPanel->toFront(true);
    };
    startTimerHz(8);
}

void WindLabAudioProcessorEditor::timerCallback()
{
    const int status = processor.phraseRecorder.status();
    const juce::String label =
        status == windlab::PhraseRecorder::Recording ? "STOP REC" :
        status == windlab::PhraseRecorder::Ready ? "VEDI TAKE" : "REC MIDI";
    if (quickRecordButton.getButtonText() != label)
        quickRecordButton.setButtonText(label);
}

void WindLabAudioProcessorEditor::showGuide(int topic)
{
    if (!guidePanel) return;
    guidePanel->openTopic(topic);
    guidePanel->setVisible(true);
    guidePanel->toFront(true);
}

void WindLabAudioProcessorEditor::changeKeyboardOctave(int delta)
{
    keyboardOctave = juce::jlimit(3, 7, keyboardOctave + delta);
    keyboard.setKeyPressBaseOctave(keyboardOctave);
    keyboard.setLowestVisibleKey(12 * keyboardOctave);
    octaveLabel.setText("C" + juce::String(keyboardOctave - 1), juce::dontSendNotification);
    if (inputModeBox.getSelectedId() == 2)
        keyboard.grabKeyboardFocus();
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
    g.drawText("RECORDED WINDS  /  0.9.3  /  MIDI RECORD + POPUP EDITOR + REAL WAV", 32, 63, 910, 21,
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
    g.drawText("BREATH LENGTH: 30 = continuous. Recorded CC0 winds by Versilian Studios / FreePats.",
               29, 679, 1020, 18, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff1d313b));
    g.fillRoundedRectangle(22.0f, 852.0f, full.getWidth() - 44.0f, 50.0f, 9.0f);
    g.setColour(juce::Colour(0xff1d313b));
    g.fillRoundedRectangle(22.0f, 702.0f, full.getWidth() - 44.0f, 143.0f, 12.0f);
    g.setColour(juce::Colour(0xff35515b));
    g.drawRoundedRectangle(22.0f, 702.0f, full.getWidth() - 44.0f, 143.0f, 12.0f, 1.0f);
    g.setColour(juce::Colour(0xff9dbbc2));
    g.setFont(juce::Font(12.0f));
    const bool pc = inputModeBox.getSelectedId() == 2;
    g.drawText(pc ? "PC ACTIVE: A W S E D F T G Y H U J K  |  Click the piano below if keyboard focus is lost."
                  : "MIDI READY: external USB/MIDI keyboard stays enabled; choose PC / NOTEBOOK to use typing keys.",
               35, 762, 1010, 20, juce::Justification::centredLeft);
}

void WindLabAudioProcessorEditor::resized()
{
    constexpr int inset = 35;
    const int fieldW = (getWidth() - inset * 2 - 30) / 3;
    instrumentLabel.setBounds(inset, 104, fieldW, 22);
    articulationLabel.setBounds(inset + fieldW + 15, 104, fieldW, 22);
    sourceLabel.setBounds(inset + (fieldW + 15) * 2, 104, fieldW, 22);
    instrumentBox.setBounds(inset, 130, fieldW, 41);
    articulationBox.setBounds(inset + fieldW + 15, 130, fieldW, 41);
    sourceBox.setBounds(inset + (fieldW + 15) * 2, 130, fieldW, 41);

    performanceModeLabel.setBounds(35, 177, 180, 23);
    performanceModeBox.setBounds(215, 175, 330, 27);
    noteMidiChannelLabel.setBounds(558, 177, 73, 23);
    noteMidiChannelBox.setBounds(633, 175, 68, 27);
    breathMidiChannelLabel.setBounds(722, 177, 93, 23);
    breathMidiChannelBox.setBounds(818, 175, 68, 27);
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
    inputModeLabel.setBounds(35, 705, 235, 22);
    inputModeBox.setBounds(35, 729, 235, 32);
    octaveDown.setBounds(291, 729, 34, 32);
    octaveLabel.setBounds(332, 729, 67, 32);
    octaveUp.setBounds(405, 729, 34, 32);
    velocityLabel.setBounds(470, 705, 205, 22);
    keyVelocity.setBounds(467, 729, 218, 32);
    velocityResponseLabel.setBounds(710, 705, 240, 22);
    velocityResponseBox.setBounds(710, 729, 245, 32);
    midiHelpButton.setBounds(967, 729, 78, 32);
    keyboard.setBounds(30, 786, getWidth() - 60, 53);
    phraseButton.setBounds(getWidth() - 205, 20, 176, 41);
    quickRecordButton.setBounds(getWidth() - 550, 20, 123, 41);
    helpButton.setBounds(getWidth() - 263, 20, 45, 41);
    midiLearnButton.setBounds(getWidth() - 418, 20, 145, 41);
    breathThresholdLabel.setBounds(35, 858, 160, 24);
    breathThresholdSlider.setBounds(192, 857, 320, 28);
    breathCurveLabel.setBounds(545, 858, 160, 24);
    breathCurveSlider.setBounds(701, 857, 310, 28);
    if (phrasePanel) phrasePanel->setBounds(getLocalBounds());
    if (guidePanel) guidePanel->setBounds(getLocalBounds());
    if (midiLearnPanel) midiLearnPanel->setBounds(getLocalBounds());
}
