#include "WindLabVirtualInstrumentPanel.h"
#include <cmath>
#include <utility>

namespace
{
using namespace windlab;
constexpr char shiftKey = 1; // KeyShape::pc value meaning "SHIFT"

const juce::Colour background {0xff12212a}, header {0xff172a33}, card {0xff1d313b}, border {0xff2a434d},
                   textMain {0xffbfd0d3}, textMuted {0xff8fa7af}, accent {0xffeda972}, accentLight {0xffffd9b0},
                   green {0xffb6dbcb}, pearl {0xffe9dfc6}, brassDark {0xff6a5220};

void brassFill(juce::Graphics& g, float y0, float y1)
{
    juce::ColourGradient grad(juce::Colour(0xffe2c47c), 0.0f, y0, juce::Colour(0xff8a6a2c), 0.0f, y1, false);
    grad.addColour(0.5, juce::Colour(0xffc8a35c));
    g.setGradientFill(grad);
}

void strokeTube(juce::Graphics& g, const juce::Path& p, float width)
{
    g.strokePath(p, juce::PathStrokeType(width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

WindLabVirtualInstrumentPanel::KeyShape key(int k, float cx, float cy, float w, float h, char pc, bool round = true)
{
    return {k, juce::Rectangle<float>(w, h).withCentre({cx, cy}), pc, round};
}

bool isWoodwind(VirtualFamily f)
{
    return f == VirtualFamily::Sax || f == VirtualFamily::Clarinet || f == VirtualFamily::Flute;
}

float slideCrookX(float position) { return 310.0f + (position - 1.0f) * 47.0f + 59.0f; }
}

WindLabVirtualInstrumentPanel::WindLabVirtualInstrumentPanel(WindLabAudioProcessor& p,
                                                             std::function<void()> close,
                                                             std::function<void()> record,
                                                             std::function<juce::String()> recordLabel,
                                                             std::function<void()> helpCallback)
    : processor(p), onClose(std::move(close)), onRecord(std::move(record)), onHelp(std::move(helpCallback)),
      recordText(std::move(recordLabel))
{
    setOpaque(true);
    setWantsKeyboardFocus(true);

    title.setText("STRUMENTO A VIDEO", juce::dontSendNotification);
    title.setFont(juce::Font(22.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);
    instrumentLabel.setText("STRUMENTO", juce::dontSendNotification);
    instrumentLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    instrumentLabel.setColour(juce::Label::textColourId, textMuted);
    instrumentLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(instrumentLabel);

    const juce::StringArray instruments {"Trumpet", "Tenor Sax", "Trombone", "Flute", "Clarinet", "French Horn",
                                         "Soprano Sax", "Alto Sax", "Baritone Sax"};
    for (int i = 0; i < instruments.size(); ++i)
        instrumentBox.addItem(instruments[i], i + 1);
    instrumentBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff182831));
    instrumentBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    instrumentBox.setWantsKeyboardFocus(false);
    addAndMakeVisible(instrumentBox);
    instrumentAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.parameters, "instrument", instrumentBox);

    auto styleButton = [this](juce::TextButton& b, juce::Colour colour)
    {
        b.setColour(juce::TextButton::buttonColourId, colour);
        b.setColour(juce::TextButton::buttonOnColourId, accent);
        b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        b.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff1a1206));
        b.setWantsKeyboardFocus(false);
        addAndMakeVisible(b);
    };
    styleButton(closeButton, juce::Colour(0xff294c59));
    styleButton(helpButton, juce::Colour(0xff36525e));
    styleButton(spaceSourceButton, juce::Colour(0xff294c59));
    styleButton(mouseSourceButton, juce::Colour(0xff294c59));
    styleButton(micSourceButton, juce::Colour(0xff294c59));
    styleButton(sharpButton, juce::Colour(0xff294c59));
    styleButton(releaseButton, juce::Colour(0xff294c59));
    styleButton(recordButton, juce::Colour(0xffb0413e));
    styleButton(partialUp, juce::Colour(0xff294c59));
    styleButton(partialDown, juce::Colour(0xff294c59));

    closeButton.onClick = [this] { if (onClose) onClose(); };
    helpButton.onClick = [this] { if (onHelp) onHelp(); };
    spaceSourceButton.setClickingTogglesState(true);
    mouseSourceButton.setClickingTogglesState(true);
    micSourceButton.setClickingTogglesState(true);
    spaceSourceButton.setRadioGroupId(4101);
    mouseSourceButton.setRadioGroupId(4101);
    micSourceButton.setRadioGroupId(4101);
    spaceSourceButton.setToggleState(true, juce::dontSendNotification);
    spaceSourceButton.onClick = [this] { chooseBreathSource(); };
    mouseSourceButton.onClick = [this] { chooseBreathSource(); };
    micSourceButton.onClick = [this] { chooseBreathSource(); };
    sharpButton.setClickingTogglesState(true);
    sharpButton.onClick = [this]
    {
        if (sharpButton.getToggleState()) latchedMask |= vkBit(VkSharp);
        else latchedMask &= ~vkBit(VkSharp);
        grabKeyboardFocus();
    };
    releaseButton.onClick = [this]
    {
        latchedMask = 0;
        sharpButton.setToggleState(false, juce::dontSendNotification);
        grabKeyboardFocus();
    };
    recordButton.onClick = [this] { if (onRecord) onRecord(); grabKeyboardFocus(); };
    partialUp.onClick = [this] { partial = juce::jmin(profile.partialCount - 1, partial + 1); grabKeyboardFocus(); };
    partialDown.onClick = [this] { partial = juce::jmax(0, partial - 1); grabKeyboardFocus(); };

    spaceStrength.setSliderStyle(juce::Slider::LinearHorizontal);
    spaceStrength.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    spaceStrength.setRange(0.2, 1.0, 0.01);
    spaceStrength.setValue(0.8, juce::dontSendNotification);
    spaceStrength.setColour(juce::Slider::thumbColourId, accent);
    spaceStrength.setColour(juce::Slider::trackColourId, juce::Colour(0xff35515b));
    spaceStrength.setWantsKeyboardFocus(false);
    addAndMakeVisible(spaceStrength);
    spaceStrengthLabel.setText("FORZA DEL FIATO (SPAZIO)", juce::dontSendNotification);
    spaceStrengthLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    spaceStrengthLabel.setColour(juce::Label::textColourId, textMuted);
    addAndMakeVisible(spaceStrengthLabel);

    help.setFont(juce::Font(13.5f));
    help.setColour(juce::Label::textColourId, textMain);
    help.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(help);

    refreshProfile();
}

WindLabVirtualInstrumentPanel::~WindLabVirtualInstrumentPanel()
{
    stopTimer();
    releaseAll();
}

void WindLabVirtualInstrumentPanel::visibilityChanged()
{
    if (isVisible())
    {
        refreshProfile();
        startTimerHz(60);
        grabKeyboardFocus();
    }
    else
    {
        stopTimer();
        latchedMask = heldMask = 0;
        spaceDown = mouseBreathActive = false;
        sharpButton.setToggleState(false, juce::dontSendNotification);
        releaseAll();
    }
}

void WindLabVirtualInstrumentPanel::refreshProfile()
{
    const int index = static_cast<int>(processor.parameters.getRawParameterValue("instrument")->load());
    if (index == instrumentIndex) return;
    releaseAll();
    instrumentIndex = index;
    profile = virtualProfileFor(index);
    partial = profile.defaultPartial;
    latchedMask = 0;
    slidePosition = 1.0f;
    sharpButton.setToggleState(false, juce::dontSendNotification);
    shapes.clear();

    switch (profile.family)
    {
        case VirtualFamily::Sax:
            designBounds = {40.0f, 20.0f, 470.0f, 580.0f};
            shapes = {key(VkOctave, 230, 118, 22, 22, shiftKey),
                      key(VkPalm1, 292, 140, 22, 15, 'q', false), key(VkPalm2, 297, 162, 22, 15, 'w', false),
                      key(VkPalm3, 302, 184, 22, 15, 'e', false),
                      key(VkL1, 256, 210, 34, 34, 'f'), key(VkL2, 257, 254, 34, 34, 'd'),
                      key(VkL3, 258, 298, 34, 34, 's'), key(VkLPinky, 220, 312, 18, 26, 'a', false),
                      key(VkR1, 260, 356, 34, 34, 'j'), key(VkR2, 261, 400, 34, 34, 'k'),
                      key(VkR3, 262, 444, 34, 34, 'l'),
                      key(VkSideBb, 298, 368, 14, 26, 'r', false), key(VkSideC, 300, 400, 14, 26, 'u', false),
                      key(VkLow1, 302, 498, 28, 18, 'm', false), key(VkLow2, 226, 502, 28, 18, 'n', false)};
            break;
        case VirtualFamily::Clarinet:
            designBounds = {40.0f, 0.0f, 260.0f, 640.0f};
            shapes = {key(VkOctave, 100, 110, 20, 20, shiftKey), key(VkThumb, 100, 155, 30, 30, 'v'),
                      key(VkThroatA, 196, 120, 16, 26, 'r', false),
                      key(VkL1, 150, 180, 28, 28, 'f'), key(VkL2, 150, 225, 28, 28, 'd'),
                      key(VkL3, 150, 270, 28, 28, 's'),
                      key(VkR1, 150, 350, 28, 28, 'j'), key(VkR2, 150, 395, 28, 28, 'k'),
                      key(VkR3, 150, 440, 28, 28, 'l'),
                      key(VkLow1, 198, 490, 16, 26, 'm', false), key(VkLow2, 204, 525, 16, 26, 'n', false)};
            break;
        case VirtualFamily::Flute:
            designBounds = {0.0f, 50.0f, 760.0f, 170.0f};
            shapes = {key(VkThumb, 230, 172, 28, 18, 'v', false),
                      key(VkL1, 250, 128, 30, 30, 'f'), key(VkL2, 305, 128, 30, 30, 'd'),
                      key(VkL3, 360, 128, 30, 30, 's'), key(VkLPinky, 396, 92, 16, 22, 'a', false),
                      key(VkR1, 465, 128, 30, 30, 'j'), key(VkR2, 520, 128, 30, 30, 'k'),
                      key(VkR3, 575, 128, 30, 30, 'l'), key(VkLow1, 690, 128, 22, 30, 'm', false)};
            break;
        case VirtualFamily::Valves:
            if (index == 5)
            {
                designBounds = {20.0f, 60.0f, 470.0f, 420.0f};
                shapes = {key(VkValve1, 190, 150, 40, 40, 'j'), key(VkValve2, 240, 150, 40, 40, 'k'),
                          key(VkValve3, 290, 150, 40, 40, 'l')};
            }
            else
            {
                designBounds = {10.0f, 60.0f, 610.0f, 300.0f};
                shapes = {key(VkValve1, 253, 118, 46, 46, 'j'), key(VkValve2, 303, 118, 46, 46, 'k'),
                          key(VkValve3, 353, 118, 46, 46, 'l')};
            }
            break;
        case VirtualFamily::Slide:
            designBounds = {0.0f, 40.0f, 640.0f, 380.0f};
            break;
    }

    help.setText(helpText(), juce::dontSendNotification);
    const bool woodwind = isWoodwind(profile.family);
    sharpButton.setVisible(woodwind);
    partialUp.setVisible(hasPartials());
    partialDown.setVisible(hasPartials());
    resized();
    repaint();
}

juce::String WindLabVirtualInstrumentPanel::helpText() const
{
    switch (profile.family)
    {
        case VirtualFamily::Sax:
            return "COME SI SUONA (diteggiatura vera del sax)\n"
                   "F D S = mano sinistra (Si, La, Sol)\n"
                   "J K L = mano destra (Fa, Mi, Re)\n"
                   "M / N = Do e Si gravi, A = Sol#\n"
                   "Q W E = chiavi palmari, R / U = laterali\n"
                   "SHIFT = chiave d'ottava, H = # +1\n"
                   "SPAZIO = soffia (tieni premuto)";
        case VirtualFamily::Clarinet:
            return "COME SI SUONA (diteggiatura vera del clarinetto)\n"
                   "V = foro del pollice\n"
                   "F D S = mano sinistra, J K L = mano destra\n"
                   "M / N = chiavi del Fa e del Mi gravi\n"
                   "R = chiave del La, H = # +1\n"
                   "SHIFT = portavoce (sale di una dodicesima)\n"
                   "SPAZIO = soffia (tieni premuto)";
        case VirtualFamily::Flute:
            return "COME SI SUONA (diteggiatura vera del flauto)\n"
                   "V = chiave del pollice (Si)\n"
                   "F D S = mano sinistra, J K L = mano destra\n"
                   "A = Sol#, M = Do grave, H = # +1\n"
                   "FRECCE SU/GIU' = ottava di soffio\n"
                   "SHIFT = ottava sopra\n"
                   "SPAZIO = soffia (tieni premuto)";
        case VirtualFamily::Valves:
            return "COME SI SUONA\n"
                   "J K L = pistoni / cilindri 1, 2, 3\n"
                   "FRECCE SU/GIU' = armonico (come le labbra)\n"
                   "oppure clic sulla colonna ARMONICO\n"
                   "SPAZIO = soffia (tieni premuto)\n"
                   "Pistone 1 = -1 tono, 2 = -1/2 tono, 3 = -1 tono e 1/2";
        case VirtualFamily::Slide:
            return "COME SI SUONA\n"
                   "MOUSE = trascina la coulisse (glissando)\n"
                   "1 - 7 = salta alla posizione\n"
                   "FRECCE SU/GIU' = armonico (come le labbra)\n"
                   "SPAZIO = soffia (tieni premuto)\n"
                   "Ogni posizione abbassa di un semitono";
    }
    return {};
}

int WindLabVirtualInstrumentPanel::midiChannel() const
{
    // In hybrid mode notes must arrive on NOTE CH, otherwise channel 1.
    if (static_cast<int>(processor.parameters.getRawParameterValue("performanceMode")->load()) == 2)
        return 1 + static_cast<int>(processor.parameters.getRawParameterValue("noteMidiChannel")->load());
    return 1;
}

void WindLabVirtualInstrumentPanel::send(const juce::MidiMessage& message)
{
    if (!processor.virtualInputReady.load()) return;
    auto m = message;
    m.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
    processor.virtualInstrumentInput.addMessageToQueue(m);
}

void WindLabVirtualInstrumentPanel::chooseBreathSource()
{
    useMouseBreath = mouseSourceButton.getToggleState();
    useMicBreath = micSourceButton.getToggleState();
    // The microphone breath is produced by the audio engine (SOFFIO DA = MICROFONO);
    // space bar and mouse send their own breath, so the engine must not add the mic's.
    if (auto* source = processor.parameters.getParameter("breathSource"))
    {
        const int current = static_cast<int>(processor.parameters.getRawParameterValue("breathSource")->load());
        if (useMicBreath && current != WindLabAudioProcessor::breathFromMicrophone)
            source->setValueNotifyingHost(source->convertTo0to1(static_cast<float>(WindLabAudioProcessor::breathFromMicrophone)));
        else if (!useMicBreath && current == WindLabAudioProcessor::breathFromMicrophone)
            source->setValueNotifyingHost(source->convertTo0to1(0.0f));
    }
    releaseAll();
    grabKeyboardFocus();
}

void WindLabVirtualInstrumentPanel::releaseAll()
{
    const int ch = midiChannel();
    if (soundingNote >= 0)
        send(juce::MidiMessage::noteOff(ch, soundingNote));
    if (lastBreathValue > 0 && !useMicBreath)
        send(juce::MidiMessage::controllerEvent(ch, processor.midiLearn.binding(MidiLearnBindings::Breath), 0));
    if (lastBend != 8192)
        send(juce::MidiMessage::pitchWheel(ch, 8192));
    soundingNote = -1;
    lastBreathValue = -1;
    lastBend = 8192;
    breath = mouseBreath = 0.0f;
}

std::uint32_t WindLabVirtualInstrumentPanel::keyboardMask() const
{
    std::uint32_t mask = 0;
    for (const auto& s : shapes)
    {
        if (s.pc == shiftKey)
        {
            if (juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown()) mask |= vkBit(s.key);
        }
        else if (s.pc != 0 && juce::KeyPress::isKeyCurrentlyDown(s.pc))
            mask |= vkBit(s.key);
    }
    if (isWoodwind(profile.family))
    {
        if (juce::KeyPress::isKeyCurrentlyDown('h')) mask |= vkBit(VkSharp);
        if (profile.family == VirtualFamily::Flute && juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown())
            mask |= vkBit(VkOctave);
    }
    return mask;
}

bool WindLabVirtualInstrumentPanel::keyStateChanged(bool)
{
    heldMask = keyboardMask();
    spaceDown = juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey);
    if (profile.family == VirtualFamily::Slide)
        for (char c = '1'; c <= '7'; ++c)
            if (juce::KeyPress::isKeyCurrentlyDown(c))
                slidePosition = static_cast<float>(c - '0');
    return true;
}

bool WindLabVirtualInstrumentPanel::keyPressed(const juce::KeyPress& press)
{
    if (press == juce::KeyPress::escapeKey)
    {
        if (onClose) onClose();
        return true;
    }
    if (hasPartials())
    {
        if (press == juce::KeyPress::upKey) { partial = juce::jmin(profile.partialCount - 1, partial + 1); return true; }
        if (press == juce::KeyPress::downKey) { partial = juce::jmax(0, partial - 1); return true; }
    }
    return true; // the panel owns the keyboard while it is open
}

juce::Rectangle<float> WindLabVirtualInstrumentPanel::instrumentArea() const
{
    auto area = juce::Rectangle<float>(28.0f, 92.0f, static_cast<float>(getWidth()) - 28.0f - 300.0f - 48.0f,
                                       static_cast<float>(getHeight()) - 112.0f);
    if (hasPartials()) area.removeFromRight(130.0f);
    return area.reduced(24.0f, 30.0f);
}

juce::Rectangle<float> WindLabVirtualInstrumentPanel::ladderArea() const
{
    const float right = static_cast<float>(getWidth()) - 300.0f - 48.0f;
    return {right - 130.0f, 140.0f, 110.0f, static_cast<float>(getHeight()) - 260.0f};
}

juce::Rectangle<float> WindLabVirtualInstrumentPanel::breathMeterArea() const
{
    const float x = static_cast<float>(getWidth()) - 328.0f;
    return {x + 20.0f, 288.0f, 40.0f, 196.0f};
}

juce::AffineTransform WindLabVirtualInstrumentPanel::designTransform() const
{
    const auto area = instrumentArea();
    const float scale = juce::jmin(area.getWidth() / designBounds.getWidth(),
                                   area.getHeight() / designBounds.getHeight());
    const float w = designBounds.getWidth() * scale, h = designBounds.getHeight() * scale;
    return juce::AffineTransform::translation(-designBounds.getX(), -designBounds.getY())
        .scaled(scale)
        .translated(area.getX() + (area.getWidth() - w) * 0.5f, area.getY() + (area.getHeight() - h) * 0.5f);
}

juce::Point<float> WindLabVirtualInstrumentPanel::designPoint(juce::Point<float> screen) const
{
    auto t = designTransform().inverted();
    float x = screen.x, y = screen.y;
    t.transformPoint(x, y);
    return {x, y};
}

void WindLabVirtualInstrumentPanel::setSlideFromDesignX(float x)
{
    slidePosition = juce::jlimit(1.0f, 7.0f, 1.0f + (x - 59.0f + 58.0f - 310.0f) / 47.0f);
}

void WindLabVirtualInstrumentPanel::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto pos = e.position;
    if (breathMeterArea().expanded(14.0f).contains(pos))
    {
        if (!useMouseBreath)
        {
            mouseSourceButton.setToggleState(true, juce::dontSendNotification);
            chooseBreathSource();
        }
        mouseBreathActive = true;
        mouseDrag(e);
        return;
    }
    if (hasPartials() && ladderArea().contains(pos))
    {
        const auto ladder = ladderArea();
        if (pos.y > ladder.getY() + juce::jmin(58.0f, ladder.getHeight() / static_cast<float>(profile.partialCount))
                                    * static_cast<float>(profile.partialCount))
            return;
        const float rowH = juce::jmin(58.0f, ladder.getHeight() / static_cast<float>(profile.partialCount));
        const int row = static_cast<int>((pos.y - ladder.getY()) / rowH);
        partial = juce::jlimit(0, profile.partialCount - 1, profile.partialCount - 1 - row);
        return;
    }
    const auto d = designPoint(pos);
    if (profile.family == VirtualFamily::Slide)
    {
        for (int i = 0; i < 7; ++i)
            if (juce::Point<float>(310.0f + 47.0f * static_cast<float>(i), 360.0f).getDistanceFrom(d) < 20.0f)
            {
                slidePosition = static_cast<float>(i + 1);
                return;
            }
        if (d.x > 280.0f && d.x < 640.0f && d.y > 215.0f && d.y < 335.0f)
        {
            draggingSlide = true;
            setSlideFromDesignX(d.x);
        }
        return;
    }
    for (const auto& s : shapes)
        if (s.box.expanded(5.0f).contains(d))
        {
            latchedMask ^= vkBit(s.key);
            return;
        }
}

void WindLabVirtualInstrumentPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (mouseBreathActive)
    {
        const auto meter = breathMeterArea();
        mouseBreath = juce::jlimit(0.0f, 1.0f, (meter.getBottom() - e.position.y) / meter.getHeight());
    }
    else if (draggingSlide)
        setSlideFromDesignX(designPoint(e.position).x);
}

void WindLabVirtualInstrumentPanel::mouseUp(const juce::MouseEvent&)
{
    mouseBreathActive = false;
    mouseBreath = 0.0f;
    draggingSlide = false;
}

void WindLabVirtualInstrumentPanel::timerCallback()
{
    refreshProfile();
    if (hasKeyboardFocus(true))
        keyStateChanged(false); // keeps held keys right even after a focus change

    const float target = useMicBreath ? processor.micBreathLevel.load()
                       : useMouseBreath ? (mouseBreathActive ? mouseBreath : 0.0f)
                                        : (spaceDown ? static_cast<float>(spaceStrength.getValue()) : 0.0f);
    breath = useMicBreath ? target : breath + (target - breath) * (target > breath ? 0.35f : 0.28f);
    if (breath < 0.004f) breath = 0.0f;

    const auto note = computeVirtualNote(profile, currentMask(), partial, slidePosition);
    shown = note;
    const int ch = midiChannel();
    const int breathCC = processor.midiLearn.binding(MidiLearnBindings::Breath);
    const int breathValue = juce::jlimit(0, 127, static_cast<int>(std::lround(breath * 127.0f)));
    const bool blowing = breath > 0.03f && note.valid;

    if (blowing)
    {
        if (breathValue != lastBreathValue)
        {
            if (!useMicBreath) // with the mic the engine already sends the breath CC
                send(juce::MidiMessage::controllerEvent(ch, breathCC, breathValue));
            lastBreathValue = breathValue;
        }
        const int bend = juce::jlimit(0, 16383, 8192 + static_cast<int>(std::lround(note.bendSemitones / 2.0f * 8192.0f)));
        if (bend != lastBend)
        {
            send(juce::MidiMessage::pitchWheel(ch, bend));
            lastBend = bend;
        }
        if (note.concert != soundingNote)
        {
            const auto velocity = static_cast<juce::uint8>(juce::jlimit(20, 127, 30 + static_cast<int>(breath * 97.0f)));
            send(juce::MidiMessage::noteOn(ch, note.concert, velocity));
            if (soundingNote >= 0)
                send(juce::MidiMessage::noteOff(ch, soundingNote)); // overlap = WindLab legato
            soundingNote = note.concert;
        }
    }
    else if (soundingNote >= 0 || lastBreathValue > 0)
    {
        releaseAll();
    }

    if (recordText)
    {
        const auto text = recordText();
        if (recordButton.getButtonText() != text) recordButton.setButtonText(text);
    }
    repaint();
}

void WindLabVirtualInstrumentPanel::resized()
{
    const int w = getWidth();
    title.setBounds(28, 18, 300, 36);
    instrumentLabel.setBounds(330, 20, 110, 36);
    instrumentBox.setBounds(450, 20, 230, 36);
    helpButton.setBounds(w - 210, 20, 45, 36);
    closeButton.setBounds(w - 155, 20, 127, 36);

    const int x = w - 328;
    spaceSourceButton.setBounds(x + 80, 330, 64, 30);
    mouseSourceButton.setBounds(x + 147, 330, 60, 30);
    micSourceButton.setBounds(x + 210, 330, 84, 30);
    spaceStrengthLabel.setBounds(x + 80, 384, 210, 20);
    spaceStrength.setBounds(x + 80, 406, 205, 26);
    help.setBounds(x + 16, 528, 284, 170);
    partialDown.setBounds(x, 712, 145, 40);
    partialUp.setBounds(x + 155, 712, 145, 40);
    sharpButton.setBounds(x, 760, 145, 40);
    releaseButton.setBounds(sharpButton.isVisible() ? x + 155 : x, 760, sharpButton.isVisible() ? 145 : 300, 40);
    recordButton.setBounds(x, 812, 300, 46);
}

void WindLabVirtualInstrumentPanel::paint(juce::Graphics& g)
{
    g.fillAll(background);
    g.setColour(header);
    g.fillRect(0, 0, getWidth(), 72);
    g.setColour(border);
    g.drawHorizontalLine(72, 0.0f, static_cast<float>(getWidth()));

    // Instrument stage.
    const auto stage = juce::Rectangle<float>(28.0f, 92.0f, static_cast<float>(getWidth()) - 376.0f,
                                              static_cast<float>(getHeight()) - 112.0f);
    g.setColour(card);
    g.fillRoundedRectangle(stage, 14.0f);
    g.setColour(border);
    g.drawRoundedRectangle(stage, 14.0f, 1.0f);
    {
        juce::Graphics::ScopedSaveState state(g);
        g.addTransform(designTransform());
        drawInstrument(g);
        drawKeys(g);
    }
    if (hasPartials()) drawLadder(g);
    g.setColour(textMuted);
    g.setFont(juce::Font(12.5f));
    g.drawText(profile.family == VirtualFamily::Slide
                   ? "Trascina la coulisse o clicca una posizione. Fra una posizione e l'altra la nota scivola."
                   : "Clic = blocca o sblocca una chiave. Arancione = premuta. Lettera = tasto del computer.",
               stage.withTrimmedTop(stage.getHeight() - 30.0f).reduced(18.0f, 0.0f).toNearestInt(),
               juce::Justification::centredLeft);

    // Right column: note card.
    const float x = static_cast<float>(getWidth()) - 328.0f;
    auto noteCard = juce::Rectangle<float>(x, 92.0f, 300.0f, 140.0f);
    g.setColour(card);
    g.fillRoundedRectangle(noteCard, 14.0f);
    g.setColour(textMuted);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText("NOTA", noteCard.reduced(20.0f, 14.0f).removeFromTop(16.0f).toNearestInt(), juce::Justification::left);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(46.0f, juce::Font::bold));
    g.drawText(shown.valid ? juce::String(italianNoteName(shown.written)) : juce::String("-"),
               juce::Rectangle<float>(x + 20.0f, 120.0f, 270.0f, 54.0f).toNearestInt(), juce::Justification::left);
    g.setFont(juce::Font(13.5f));
    g.setColour(textMain);
    juce::String sub = profile.transpose == 0
        ? juce::String("strumento in Do: suona come scritto")
        : "scritta per strumento in " + juce::String(profile.writtenKey) + " - suona "
              + juce::String(shown.valid ? italianNoteName(shown.concert) : std::string("-"));
    g.drawText(sub, juce::Rectangle<float>(x + 20.0f, 176.0f, 275.0f, 20.0f).toNearestInt(), juce::Justification::left);
    g.setColour(green);
    juce::String detail;
    if (profile.family == VirtualFamily::Slide)
        detail = "posizione " + juce::String(slidePosition, 1);
    else
    {
        for (const auto& s : shapes)
            if ((currentMask() & vkBit(s.key)) != 0u)
                detail << (s.pc == shiftKey ? juce::String("SHIFT") : juce::String::charToString(static_cast<juce::juce_wchar>(s.pc)).toUpperCase()) << " ";
        if ((currentMask() & vkBit(VkSharp)) != 0u) detail << "# ";
        detail = detail.isEmpty() ? juce::String("nessuna chiave premuta") : "chiavi: " + detail.trim();
    }
    g.drawText(detail, juce::Rectangle<float>(x + 20.0f, 200.0f, 275.0f, 20.0f).toNearestInt(), juce::Justification::left);

    // Breath card.
    auto breathCard = juce::Rectangle<float>(x, 246.0f, 300.0f, 260.0f);
    g.setColour(card);
    g.fillRoundedRectangle(breathCard, 14.0f);
    g.setColour(textMuted);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText("FIATO " + juce::String(static_cast<int>(std::lround(breath * 127.0f))),
               juce::Rectangle<float>(x + 20.0f, 258.0f, 200.0f, 20.0f).toNearestInt(), juce::Justification::left);
    g.drawText("FIATO DA", juce::Rectangle<float>(x + 84.0f, 306.0f, 200.0f, 20.0f).toNearestInt(), juce::Justification::left);
    const auto meter = breathMeterArea();
    g.setColour(juce::Colour(0xff0f1b22));
    g.fillRoundedRectangle(meter, 12.0f);
    g.setColour(juce::Colour(0xff35515b));
    g.drawRoundedRectangle(meter, 12.0f, 1.0f);
    g.setColour(accent);
    g.fillRoundedRectangle(meter.withTrimmedTop(meter.getHeight() * (1.0f - breath)), 12.0f);
    g.setColour(textMain);
    g.setFont(juce::Font(12.0f));
    g.drawFittedText(useMicBreath ? "Soffia nel microfono. Nessun segnale? Standalone: Options > Audio/MIDI "
                                    "Settings, togli 'Mute audio input'. DAW: collega un ingresso."
                     : useMouseBreath ? "Tieni premuto il mouse sulla barra: piu' in alto = piu' forte."
                                      : "Tieni premuta la barra spaziatrice per soffiare.",
                     juce::Rectangle<float>(x + 80.0f, 440.0f, 210.0f, 54.0f).toNearestInt(),
                     juce::Justification::topLeft, 3);

    g.setColour(card);
    g.fillRoundedRectangle(juce::Rectangle<float>(x, 518.0f, 300.0f, 182.0f), 14.0f);
}

void WindLabVirtualInstrumentPanel::drawInstrument(juce::Graphics& g) const
{
    switch (profile.family)
    {
        case VirtualFamily::Sax:
        {
            g.setColour(juce::Colour(0xff151515));
            g.fillRoundedRectangle(150.0f, 34.0f, 44.0f, 18.0f, 7.0f);
            juce::Path neck;
            neck.startNewSubPath(192.0f, 43.0f);
            neck.cubicTo(236.0f, 38.0f, 250.0f, 60.0f, 252.0f, 96.0f);
            brassFill(g, 30.0f, 110.0f);
            strokeTube(g, neck, 16.0f);
            juce::Path body;
            body.startNewSubPath(252.0f, 96.0f);
            if (profile.straightBody)
            {
                body.lineTo(262.0f, 520.0f);
                brassFill(g, 90.0f, 600.0f);
                strokeTube(g, body, 40.0f);
                juce::Path bell;
                bell.startNewSubPath(240.0f, 515.0f);
                bell.lineTo(284.0f, 515.0f);
                bell.lineTo(304.0f, 585.0f);
                bell.lineTo(220.0f, 585.0f);
                bell.closeSubPath();
                g.fillPath(bell);
                g.setColour(juce::Colour(0xff5e4718));
                g.fillEllipse(220.0f, 576.0f, 84.0f, 18.0f);
            }
            else
            {
                body.lineTo(262.0f, 468.0f);
                body.cubicTo(266.0f, 548.0f, 392.0f, 552.0f, 396.0f, 468.0f);
                body.lineTo(398.0f, 410.0f);
                brassFill(g, 90.0f, 560.0f);
                strokeTube(g, body, 54.0f);
                juce::Path bell;
                bell.startNewSubPath(372.0f, 414.0f);
                bell.lineTo(424.0f, 414.0f);
                bell.lineTo(462.0f, 322.0f);
                bell.lineTo(334.0f, 322.0f);
                bell.closeSubPath();
                g.fillPath(bell);
                g.setColour(juce::Colour(0xff5e4718));
                g.fillEllipse(334.0f, 307.0f, 128.0f, 30.0f);
                g.setColour(juce::Colour(0xffe2c47c));
                g.drawEllipse(334.0f, 307.0f, 128.0f, 30.0f, 3.0f);
            }
            g.setColour(textMuted);
            g.setFont(juce::Font(13.0f, juce::Font::bold));
            g.drawText("MANO SINISTRA", 40, 245, 130, 20, juce::Justification::centred);
            g.drawText("MANO DESTRA", 40, 392, 130, 20, juce::Justification::centred);
            break;
        }
        case VirtualFamily::Clarinet:
        {
            g.setColour(juce::Colour(0xff111111));
            g.fillRoundedRectangle(137.0f, 0.0f, 26.0f, 36.0f, 6.0f);
            juce::ColourGradient wood(juce::Colour(0xff2b2b2b), 130.0f, 0.0f, juce::Colour(0xff0e0e0e), 170.0f, 0.0f, false);
            g.setGradientFill(wood);
            g.fillRect(130.0f, 34.0f, 40.0f, 530.0f);
            juce::Path bell;
            bell.startNewSubPath(130.0f, 560.0f);
            bell.lineTo(170.0f, 560.0f);
            bell.lineTo(206.0f, 632.0f);
            bell.lineTo(94.0f, 632.0f);
            bell.closeSubPath();
            g.fillPath(bell);
            g.setColour(juce::Colour(0xffc9d1d4));
            for (float y : {34.0f, 140.0f, 320.0f, 560.0f})
                g.fillRect(127.0f, y - 3.0f, 46.0f, 6.0f);
            g.setColour(textMuted);
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText("REGISTRO", 40, 82, 80, 16, juce::Justification::centred);
            g.drawText("POLLICE", 40, 176, 80, 16, juce::Justification::centred);
            break;
        }
        case VirtualFamily::Flute:
        {
            juce::ColourGradient silver(juce::Colour(0xfff1f5f6), 0.0f, 110.0f, juce::Colour(0xff7f898e), 0.0f, 146.0f, false);
            g.setGradientFill(silver);
            g.fillRoundedRectangle(20.0f, 110.0f, 710.0f, 36.0f, 8.0f);
            g.setColour(juce::Colour(0xffb9c2c6));
            g.fillRoundedRectangle(8.0f, 112.0f, 16.0f, 32.0f, 4.0f);
            g.setColour(juce::Colour(0xffdfe6e9));
            g.fillEllipse(56.0f, 104.0f, 52.0f, 24.0f);
            g.setColour(juce::Colour(0xff2a2f31));
            g.fillEllipse(72.0f, 110.0f, 20.0f, 10.0f);
            g.setColour(textMuted);
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText("MANO SINISTRA", 230, 60, 160, 16, juce::Justification::centred);
            g.drawText("MANO DESTRA", 445, 60, 160, 16, juce::Justification::centred);
            break;
        }
        case VirtualFamily::Valves:
        {
            if (instrumentIndex == 5)
            {
                brassFill(g, 80.0f, 420.0f);
                juce::Path coil;
                coil.addEllipse(80.0f, 120.0f, 280.0f, 280.0f);
                strokeTube(g, coil, 14.0f);
                juce::Path inner;
                inner.addEllipse(115.0f, 155.0f, 210.0f, 210.0f);
                strokeTube(g, inner, 9.0f);
                juce::Path lead;
                lead.startNewSubPath(60.0f, 110.0f);
                lead.cubicTo(110.0f, 96.0f, 150.0f, 120.0f, 170.0f, 190.0f);
                strokeTube(g, lead, 9.0f);
                juce::Path bell;
                bell.startNewSubPath(320.0f, 330.0f);
                bell.lineTo(345.0f, 312.0f);
                bell.lineTo(470.0f, 420.0f);
                bell.lineTo(392.0f, 470.0f);
                bell.closeSubPath();
                g.fillPath(bell);
                g.setColour(juce::Colour(0xff5e4718));
                g.fillEllipse(388.0f, 412.0f, 86.0f, 60.0f);
                g.setColour(juce::Colour(0xff151515));
                g.fillRoundedRectangle(36.0f, 103.0f, 30.0f, 14.0f, 5.0f);
                g.setColour(juce::Colour(0xffa88a48));
                for (float vx : {190.0f, 240.0f, 290.0f})
                    g.fillEllipse(vx - 17.0f, 196.0f, 34.0f, 34.0f);
                g.setColour(textMuted);
                g.setFont(juce::Font(12.0f, juce::Font::bold));
                g.drawText("CILINDRI", 190, 92, 100, 16, juce::Justification::centred);
            }
            else
            {
                brassFill(g, 120.0f, 300.0f);
                juce::Path tubes;
                tubes.startNewSubPath(60.0f, 190.0f);
                tubes.lineTo(230.0f, 190.0f);
                strokeTube(g, tubes, 13.0f);
                juce::Path loop;
                loop.startNewSubPath(230.0f, 290.0f);
                loop.lineTo(120.0f, 290.0f);
                loop.cubicTo(70.0f, 290.0f, 70.0f, 240.0f, 120.0f, 240.0f);
                loop.lineTo(380.0f, 240.0f);
                loop.startNewSubPath(380.0f, 290.0f);
                loop.lineTo(230.0f, 290.0f);
                loop.startNewSubPath(380.0f, 190.0f);
                loop.lineTo(500.0f, 190.0f);
                strokeTube(g, loop, 15.0f);
                juce::Path bell;
                bell.startNewSubPath(496.0f, 182.0f);
                bell.lineTo(600.0f, 130.0f);
                bell.lineTo(600.0f, 250.0f);
                bell.lineTo(496.0f, 198.0f);
                bell.closeSubPath();
                g.fillPath(bell);
                g.setColour(juce::Colour(0xff5e4718));
                g.fillEllipse(586.0f, 130.0f, 28.0f, 120.0f);
                g.setColour(juce::Colour(0xff151515));
                g.fillRoundedRectangle(22.0f, 181.0f, 44.0f, 18.0f, 7.0f);
                for (float vx : {236.0f, 286.0f, 336.0f})
                {
                    brassFill(g, 150.0f, 310.0f);
                    g.fillRoundedRectangle(vx, 150.0f, 34.0f, 160.0f, 8.0f);
                    g.setColour(brassDark);
                    g.drawRoundedRectangle(vx, 150.0f, 34.0f, 160.0f, 8.0f, 2.0f);
                }
                g.setColour(textMuted);
                g.setFont(juce::Font(12.0f, juce::Font::bold));
                g.drawText("PISTONI", 253, 330, 100, 16, juce::Justification::centred);
            }
            break;
        }
        case VirtualFamily::Slide:
        {
            brassFill(g, 40.0f, 200.0f);
            juce::Path bell;
            bell.startNewSubPath(140.0f, 110.0f);
            bell.lineTo(20.0f, 50.0f);
            bell.lineTo(20.0f, 186.0f);
            bell.lineTo(140.0f, 126.0f);
            bell.closeSubPath();
            g.fillPath(bell);
            g.setColour(juce::Colour(0xff5e4718));
            g.fillEllipse(6.0f, 50.0f, 28.0f, 136.0f);
            brassFill(g, 100.0f, 200.0f);
            juce::Path upper;
            upper.startNewSubPath(136.0f, 118.0f);
            upper.lineTo(300.0f, 118.0f);
            upper.cubicTo(350.0f, 118.0f, 350.0f, 190.0f, 300.0f, 190.0f);
            upper.lineTo(150.0f, 190.0f);
            strokeTube(g, upper, 15.0f);
            g.setColour(juce::Colour(0xff8a6a2c));
            g.fillRect(227.0f, 118.0f, 6.0f, 132.0f);
            g.setColour(juce::Colour(0xff151515));
            g.fillRoundedRectangle(110.0f, 250.0f, 40.0f, 16.0f, 6.0f);
            brassFill(g, 250.0f, 300.0f);
            g.fillRect(150.0f, 252.0f, 150.0f, 11.0f);
            g.fillRect(150.0f, 290.0f, 150.0f, 11.0f);
            const float crook = slideCrookX(slidePosition);
            g.setColour(juce::Colour(0xffe8cf92));
            g.fillRect(300.0f, 254.5f, crook - 300.0f, 7.0f);
            g.fillRect(300.0f, 292.5f, crook - 300.0f, 7.0f);
            juce::Path end;
            end.startNewSubPath(crook, 258.0f);
            end.cubicTo(crook + 38.0f, 258.0f, crook + 38.0f, 296.0f, crook, 296.0f);
            brassFill(g, 250.0f, 300.0f);
            strokeTube(g, end, 11.0f);
            g.setColour(accent);
            g.fillRoundedRectangle(crook - 66.0f, 248.0f, 16.0f, 60.0f, 5.0f);
            g.setColour(accentLight);
            g.drawRoundedRectangle(crook - 66.0f, 248.0f, 16.0f, 60.0f, 5.0f, 3.0f);
            g.setColour(juce::Colour(0xff35515b));
            g.drawLine(300.0f, 360.0f, 600.0f, 360.0f, 2.0f);
            const int nearest = static_cast<int>(std::lround(slidePosition));
            for (int i = 1; i <= 7; ++i)
            {
                const float cx = 310.0f + 47.0f * static_cast<float>(i - 1);
                const bool on = i == nearest;
                g.setColour(on ? accent : header);
                g.fillEllipse(cx - 16.0f, 344.0f, 32.0f, 32.0f);
                g.setColour(on ? accentLight : juce::Colour(0xff35515b));
                g.drawEllipse(cx - 16.0f, 344.0f, 32.0f, 32.0f, 2.0f);
                g.setColour(on ? juce::Colour(0xff1a1206) : textMain);
                g.setFont(juce::Font(15.0f, juce::Font::bold));
                g.drawText(juce::String(i), juce::Rectangle<float>(cx - 16.0f, 344.0f, 32.0f, 32.0f).toNearestInt(),
                           juce::Justification::centred);
            }
            g.setColour(textMuted);
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText("POSIZIONI (tasti 1-7 o mouse)", 300, 392, 300, 16, juce::Justification::centred);
            break;
        }
    }
}

void WindLabVirtualInstrumentPanel::drawKeys(juce::Graphics& g) const
{
    const auto mask = currentMask();
    for (const auto& s : shapes)
    {
        const bool pressed = (mask & vkBit(s.key)) != 0u;
        auto box = s.box;
        const bool valve = s.key >= VkValve1;
        if (valve)
        {
            // Valve stem, cap pressed down when the valve is used.
            if (pressed) box = box.translated(0.0f, 12.0f);
            g.setColour(juce::Colour(0xff8a6a2c));
            g.fillRect(box.getCentreX() - 10.0f, box.getBottom() - 4.0f, 20.0f,
                       juce::jmax(4.0f, (instrumentIndex == 5 ? 196.0f : 150.0f) - box.getBottom() + 4.0f));
        }
        const bool hole = profile.family == VirtualFamily::Clarinet && s.round && s.key != VkOctave;
        juce::Colour fill = pressed ? accent
                          : hole ? juce::Colour(0xff050505)
                          : profile.family == VirtualFamily::Flute ? juce::Colour(0xffdfe6e9) : pearl;
        g.setColour(fill);
        if (s.round) g.fillEllipse(box); else g.fillRoundedRectangle(box, juce::jmin(box.getWidth(), box.getHeight()) * 0.45f);
        g.setColour(pressed ? accentLight : (hole ? juce::Colour(0xffc9d1d4) : brassDark));
        if (s.round) g.drawEllipse(box, 2.5f); else g.drawRoundedRectangle(box, juce::jmin(box.getWidth(), box.getHeight()) * 0.45f, 2.0f);

        if (s.pc == 0) continue;
        const juce::String label = s.pc == shiftKey ? juce::String("SHIFT")
                                                    : juce::String::charToString(static_cast<juce::juce_wchar>(s.pc)).toUpperCase();
        const bool inside = s.pc != shiftKey && juce::jmin(box.getWidth(), box.getHeight()) >= 26.0f;
        g.setFont(juce::Font(inside ? 16.0f : 12.0f, juce::Font::bold));
        if (inside)
        {
            g.setColour(pressed ? juce::Colour(0xff1a1206) : (hole ? juce::Colours::white : juce::Colour(0xff3b2c0e)));
            g.drawText(label, box.toNearestInt(), juce::Justification::centred);
        }
        else
        {
            g.setColour(pressed ? accentLight : textMain);
            const bool leftSide = profile.family == VirtualFamily::Clarinet ? box.getCentreX() < 150.0f : box.getCentreX() < 240.0f;
            auto labelBox = leftSide ? juce::Rectangle<float>(box.getX() - 54.0f, box.getCentreY() - 8.0f, 50.0f, 16.0f)
                                     : juce::Rectangle<float>(box.getRight() + 4.0f, box.getCentreY() - 8.0f, 50.0f, 16.0f);
            if (profile.family == VirtualFamily::Flute)
                labelBox = juce::Rectangle<float>(box.getCentreX() - 25.0f, box.getBottom() + 2.0f, 50.0f, 16.0f);
            g.drawText(label, labelBox.toNearestInt(),
                       leftSide && profile.family != VirtualFamily::Flute ? juce::Justification::centredRight
                                                                          : juce::Justification::centredLeft);
        }
    }
}

void WindLabVirtualInstrumentPanel::drawLadder(juce::Graphics& g) const
{
    const auto ladder = ladderArea();
    g.setColour(textMuted);
    g.setFont(juce::Font(11.5f, juce::Font::bold));
    g.drawFittedText(profile.family == VirtualFamily::Flute ? "OTTAVA\n(soffio) SU/GIU'" : "ARMONICO\n(labbra) SU/GIU'",
                     juce::Rectangle<float>(ladder.getX(), ladder.getY() - 40.0f, ladder.getWidth(), 34.0f).toNearestInt(),
                     juce::Justification::centred, 2);
    const float rowH = juce::jmin(58.0f, ladder.getHeight() / static_cast<float>(profile.partialCount));
    for (int i = 0; i < profile.partialCount; ++i)
    {
        const int index = profile.partialCount - 1 - i;
        const auto row = juce::Rectangle<float>(ladder.getX(), ladder.getY() + rowH * static_cast<float>(i),
                                                ladder.getWidth(), rowH).reduced(0.0f, 3.0f);
        const bool on = index == partial;
        g.setColour(on ? accent : header);
        g.fillRoundedRectangle(row, 6.0f);
        juce::String text;
        if (profile.family == VirtualFamily::Flute)
            text = juce::String(index + 1) + "a OTTAVA";
        else
            text = italianNoteName(profile.partials[static_cast<std::size_t>(index)]);
        g.setColour(on ? juce::Colour(0xff1a1206) : textMain);
        g.setFont(juce::Font(13.0f, on ? juce::Font::bold : juce::Font::plain));
        g.drawText(text, row.toNearestInt(), juce::Justification::centred);
    }
}
