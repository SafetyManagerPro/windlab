#include "WindLabExpressionPanel.h"
#include <cmath>
#include <utility>

namespace
{
const juce::Colour background {0xff12212a}, header {0xff172a33}, card {0xff1d313b}, border {0xff35515b},
                   textMain {0xffbfd0d3}, textMuted {0xff8fa7af}, accent {0xffe8ac77};
constexpr int factoryIdBase = 1, userIdBase = 1000;
}

WindLabExpressionPanel::WindLabExpressionPanel(WindLabAudioProcessor& p, std::function<void()> close,
                                               std::function<void()> help)
    : processor(p), onClose(std::move(close)), onHelp(std::move(help))
{
    setOpaque(true);
    knobNames = {"LEGATO TIME", "VIBRATO RATE", "VIBRATO DAL FIATO", "GROWL", "FLUTTER TONGUE", "PAN"};
    knobIDs = {"legatoTime", "vibratoRate", "vibratoBreath", "growl", "flutter", "pan"};
    for (std::size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 85, 21);
        k.setColour(juce::Slider::rotarySliderFillColourId, accent);
        k.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff314b53));
        k.setColour(juce::Slider::thumbColourId, juce::Colour(0xffffd1a1));
        k.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        k.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(k);
        attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.parameters, knobIDs[i], k);
        knobLabels[i].setText(knobNames[i], juce::dontSendNotification);
        knobLabels[i].setJustificationType(juce::Justification::centred);
        knobLabels[i].setColour(juce::Label::textColourId, textMain);
        knobLabels[i].setFont(juce::Font(12.0f, juce::Font::bold));
        addAndMakeVisible(knobLabels[i]);
        learners.push_back(std::make_unique<windlabui::LearnOnRightClick>(processor, k, knobIDs[i]));
    }
    knobs[0].textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v * 1000.0)) + " ms"; };
    knobs[1].textFromValueFunction = [](double v) { return "x " + juce::String(v, 2); };
    knobs[5].textFromValueFunction = [](double v)
    {
        if (std::abs(v) < .01) return juce::String("C");
        return (v < 0 ? "L " : "R ") + juce::String(juce::roundToInt(std::abs(v) * 100.0));
    };
    for (auto& k : knobs) k.updateText();

    legatoModeBox.addItem("AUTO (legato + accordi)", 1);
    legatoModeBox.addItem("SEMPRE LEGATO (mono)", 2);
    legatoModeBox.addItem("MAI LEGATO (accordi)", 3);
    jazzBox.addItem("NESSUNO", 1);
    jazzBox.addItem("SCOOP (attacco dal basso)", 2);
    jazzBox.addItem("FALL (caduta finale)", 3);
    jazzBox.addItem("DOIT (salita finale)", 4);
    jazzBox.addItem("SHAKE (trillo di labbro)", 5);
    sectionBox.addItem("SPENTA (strumento scelto)", 1);
    sectionBox.addItem("SEZIONE SAX (bar, ten, alto, sop)", 2);
    sectionBox.addItem("SEZIONE OTTONI (trb, corno, tromba)", 3);
    sectionBox.addItem("LEGNI (clarinetto, flauto)", 4);
    sectionBox.addItem("BIG BAND (ottoni + sax)", 5);
    roomBox.addItem("ALGORITMICO (classico)", 1);
    roomBox.addItem("STUDIO", 2);
    roomBox.addItem("CLUB JAZZ", 3);
    roomBox.addItem("SALA DA CONCERTO", 4);
    roomBox.addItem("CHIESA", 5);
    for (auto* box : {&legatoModeBox, &jazzBox, &presetBox, &sectionBox, &roomBox})
    {
        box->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff182831));
        box->setColour(juce::ComboBox::textColourId, juce::Colours::white);
        box->setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff3c555f));
        addAndMakeVisible(*box);
    }
    legatoModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.parameters, "legatoMode", legatoModeBox);
    jazzAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.parameters, "jazzEffect", jazzBox);
    learners.push_back(std::make_unique<windlabui::LearnOnRightClick>(processor, jazzBox, "jazzEffect"));
    sectionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.parameters, "sectionMode", sectionBox);
    roomAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.parameters, "reverbType", roomBox);
    micSensitivity.setSliderStyle(juce::Slider::LinearHorizontal);
    micSensitivity.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    micSensitivity.setColour(juce::Slider::thumbColourId, accent);
    micSensitivity.setColour(juce::Slider::trackColourId, juce::Colour(0xff35515b));
    addAndMakeVisible(micSensitivity);
    micAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, "micSensitivity", micSensitivity);
    learners.push_back(std::make_unique<windlabui::LearnOnRightClick>(processor, sectionBox, "sectionMode"));
    learners.push_back(std::make_unique<windlabui::LearnOnRightClick>(processor, roomBox, "reverbType"));
    learners.push_back(std::make_unique<windlabui::LearnOnRightClick>(processor, micSensitivity, "micSensitivity"));

    for (auto* label : {&legatoModeLabel, &jazzLabel, &presetLabel, &sectionLabel, &roomLabel, &micLabel})
    {
        label->setColour(juce::Label::textColourId, textMuted);
        label->setFont(juce::Font(12.0f, juce::Font::bold));
        addAndMakeVisible(*label);
    }
    legatoModeLabel.setText("MODALITA' LEGATO", juce::dontSendNotification);
    jazzLabel.setText("EFFETTO JAZZ", juce::dontSendNotification);
    presetLabel.setText("PRESET (cambiano il suono, non i canali MIDI)", juce::dontSendNotification);
    sectionLabel.setText("MODALITA' SEZIONE", juce::dontSendNotification);
    roomLabel.setText("SALA DEL RIVERBERO", juce::dontSendNotification);
    micLabel.setText("SENSIBILITA' MICROFONO", juce::dontSendNotification);

    presetBox.setTextWhenNothingSelected("Scegli un preset...");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id >= userIdBase && id - userIdBase < userPresets.size())
            processor.loadUserPreset(userPresets[id - userIdBase]);
        else if (id >= factoryIdBase && id < userIdBase)
            processor.applyFactoryPreset(id - factoryIdBase);
    };

    auto style = [this](juce::TextButton& b, juce::Colour c)
    {
        b.setColour(juce::TextButton::buttonColourId, c);
        b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        addAndMakeVisible(b);
    };
    style(savePresetButton, juce::Colour(0xff416a64));
    style(openPresetButton, juce::Colour(0xff294c59));
    style(clearCCButton, juce::Colour(0xff7a3b37));
    style(closeButton, juce::Colour(0xff294c59));
    style(helpButton, juce::Colour(0xff36525e));
    closeButton.onClick = [this] { if (onClose) onClose(); };
    helpButton.onClick = [this] { if (onHelp) onHelp(); };
    clearCCButton.onClick = [this] { processor.parameterCC.clearAll(); };
    savePresetButton.onClick = [this]
    {
        const auto folder = WindLabAudioProcessor::userPresetFolder();
        folder.createDirectory();
        chooser = std::make_unique<juce::FileChooser>("Salva il preset WindLab",
                                                      folder.getChildFile("Mio preset.windlabpreset"), "*.windlabpreset");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file == juce::File()) return;
                if (!file.hasFileExtension(".windlabpreset")) file = file.withFileExtension(".windlabpreset");
                processor.saveUserPreset(file);
                refreshPresetList();
            });
    };
    openPresetButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser>("Apri un preset WindLab",
                                                      WindLabAudioProcessor::userPresetFolder(), "*.windlabpreset");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();
                if (file.existsAsFile()) processor.loadUserPreset(file);
            });
    };

    assignments.setColour(juce::Label::textColourId, juce::Colour(0xffb6dbcb));
    assignments.setFont(juce::Font(13.5f, juce::Font::bold));
    assignments.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(assignments);
    refreshPresetList();
}

void WindLabExpressionPanel::refreshPresetList()
{
    presetBox.clear(juce::dontSendNotification);
    presetBox.addSectionHeading("PRESET DI FABBRICA");
    const auto factory = WindLabAudioProcessor::factoryPresetNames();
    for (int i = 0; i < factory.size(); ++i)
        presetBox.addItem(factory[i], factoryIdBase + i);
    userPresets = WindLabAudioProcessor::userPresetFolder().findChildFiles(
        juce::File::findFiles, false, "*.windlabpreset");
    userPresets.sort();
    if (!userPresets.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading("I MIEI PRESET");
        for (int i = 0; i < userPresets.size(); ++i)
            presetBox.addItem(userPresets[i].getFileNameWithoutExtension(), userIdBase + i);
    }
}

void WindLabExpressionPanel::visibilityChanged()
{
    if (isVisible())
    {
        refreshPresetList();
        startTimerHz(15);
    }
    else
        stopTimer();
}

juce::String WindLabExpressionPanel::assignmentsText() const
{
    juce::String text;
    for (const auto& [cc, index] : processor.parameterCC.assignments())
    {
        const auto* parameter = processor.getParameters()[index];
        text << "CC" << cc << " -> " << (parameter != nullptr ? parameter->getName(40) : juce::String("?")) << "     ";
    }
    const int armed = processor.parameterCC.armedParameter();
    if (armed >= 0)
        if (const auto* parameter = processor.getParameters()[armed])
            text << "\nIN ASCOLTO: muovi un controllo MIDI per \"" << parameter->getName(40) << "\"";
    return text.isEmpty() ? juce::String("Nessun controllo assegnato.") : text;
}

void WindLabExpressionPanel::timerCallback()
{
    for (std::size_t i = 0; i < knobs.size(); ++i)
    {
        const auto text = knobNames[i] + windlabui::learnSuffix(processor, knobIDs[i]);
        if (knobLabels[i].getText() != text) knobLabels[i].setText(text, juce::dontSendNotification);
    }
    const auto text = assignmentsText();
    if (assignments.getText() != text) assignments.setText(text, juce::dontSendNotification);
    const float level = processor.micBreathLevel.load();
    if (std::abs(level - shownMicLevel) > .01f)
    {
        shownMicLevel = level;
        repaint(880, 830, 160, 30);
    }
}

void WindLabExpressionPanel::resized()
{
    const int w = getWidth();
    closeButton.setBounds(w - 155, 20, 127, 36);
    helpButton.setBounds(w - 210, 20, 45, 36);

    presetLabel.setBounds(40, 102, 500, 20);
    presetBox.setBounds(40, 128, 460, 38);
    savePresetButton.setBounds(520, 128, 190, 38);
    openPresetButton.setBounds(722, 128, 190, 38);

    legatoModeLabel.setBounds(40, 236, 300, 20);
    legatoModeBox.setBounds(40, 260, 300, 34);
    jazzLabel.setBounds(40, 446, 300, 20);
    jazzBox.setBounds(40, 470, 300, 34);

    constexpr int rowTop[2] {222, 432};
    const int columns[3] {380, 600, 820};
    for (int i = 0; i < knobCount; ++i)
    {
        const int row = i / 3, column = i % 3;
        knobLabels[static_cast<std::size_t>(i)].setBounds(columns[column] - 30, rowTop[row] + 8, 220, 20);
        knobs[static_cast<std::size_t>(i)].setBounds(columns[column] + 18, rowTop[row] + 32, 124, 120);
    }
    assignments.setBounds(40, 690, 660, 66);
    clearCCButton.setBounds(w - 260, 690, 220, 38);
    sectionLabel.setBounds(40, 806, 300, 20);
    sectionBox.setBounds(40, 830, 300, 32);
    roomLabel.setBounds(360, 806, 240, 20);
    roomBox.setBounds(360, 830, 240, 32);
    micLabel.setBounds(620, 806, 250, 20);
    micSensitivity.setBounds(616, 832, 250, 28);
}

void WindLabExpressionPanel::paint(juce::Graphics& g)
{
    g.fillAll(background);
    g.setColour(header);
    g.fillRect(0, 0, getWidth(), 72);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(22.0f, juce::Font::bold));
    g.drawText("ESPRESSIONE, PRESET E MIDI LEARN", 28, 18, 600, 36, juce::Justification::centredLeft);

    const float w = static_cast<float>(getWidth());
    const std::array<juce::Rectangle<float>, 4> cards {
        juce::Rectangle<float>(22.0f, 90.0f, w - 44.0f, 98.0f),
        juce::Rectangle<float>(22.0f, 204.0f, w - 44.0f, 196.0f),
        juce::Rectangle<float>(22.0f, 414.0f, w - 44.0f, 196.0f),
        juce::Rectangle<float>(22.0f, 624.0f, w - 44.0f, 142.0f)};
    for (const auto& c : cards)
    {
        g.setColour(card);
        g.fillRoundedRectangle(c, 12.0f);
        g.setColour(border);
        g.drawRoundedRectangle(c, 12.0f, 1.0f);
    }
    g.setColour(juce::Colour(0xffe0b488));
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText("01   LEGATO E VIBRATO", 40, 210, 400, 20, juce::Justification::centredLeft);
    g.drawText("02   COLORI DEL SUONO, EFFETTI JAZZ E POSIZIONE", 40, 420, 500, 20, juce::Justification::centredLeft);
    g.drawText("03   MIDI LEARN UNIVERSALE", 40, 632, 400, 20, juce::Justification::centredLeft);
    {
        const auto band = juce::Rectangle<float>(22.0f, 778.0f, w - 44.0f, 124.0f);
        g.setColour(card);
        g.fillRoundedRectangle(band, 12.0f);
        g.setColour(border);
        g.drawRoundedRectangle(band, 12.0f, 1.0f);
        g.setColour(juce::Colour(0xffe0b488));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("04   SEZIONE, SALA E FIATO DAL MICROFONO", 40, 782, 500, 20, juce::Justification::centredLeft);
        // Live microphone breath meter.
        const auto meter = juce::Rectangle<float>(882.0f, 834.0f, 150.0f, 22.0f);
        g.setColour(juce::Colour(0xff0f1b22));
        g.fillRoundedRectangle(meter, 6.0f);
        g.setColour(accent);
        g.fillRoundedRectangle(meter.withWidth(meter.getWidth() * juce::jlimit(0.0f, 1.0f, shownMicLevel)), 6.0f);
        g.setColour(textMuted);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("SOFFIO MIC", 882, 862, 150, 16, juce::Justification::centred);
    }
    g.setColour(textMain);
    g.setFont(juce::Font(13.0f));
    g.drawFittedText("Clic destro su QUALSIASI manopola di WindLab (anche nella finestra principale) e scegli "
                     "\"Impara\": poi muovi un controllo MIDI. Le assegnazioni sono salvate con il progetto.",
                     juce::Rectangle<int>(40, 652, getWidth() - 80, 34), juce::Justification::topLeft, 2);
    g.setColour(textMuted);
    g.setFont(juce::Font(12.0f));
    g.drawFittedText("AUTO: legato quando suoni una nota alla volta, accordi quando premi piu' tasti insieme. "
                     "LEGATO TIME: durata della scivolata fra le note.",
                     juce::Rectangle<int>(40, 304, 320, 80), juce::Justification::topLeft, 4);
    g.drawFittedText("SCOOP all'attacco, FALL e DOIT al rilascio della nota, SHAKE sulle note tenute. "
                     "GROWL = voce nello strumento, FLUTTER = frullato di lingua.",
                     juce::Rectangle<int>(40, 514, 320, 80), juce::Justification::topLeft, 4);
}
