#include "PhraseEditor.h"
#include <algorithm>
#include <cmath>

WindLabPhraseEditor::WindLabPhraseEditor(windlab::PhraseEngine& engine,
                                       std::function<void()> close)
    : phrase(engine), onClose(std::move(close))
{
    setOpaque(true);
    for (auto* b : {&playButton, &demoButton, &clearButton, &legatoButton, &closeButton})
    {
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff294651));
        b->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        addAndMakeVisible(*b);
    }
    playButton.onClick = [this] {
        phrase.setPlaying(!phrase.isPlaying());
        updateButtons();
    };
    demoButton.onClick = [this] {
        phrase.setPlaying(false);
        phrase.loadDemo();
        selectedStep = 0;
        updateButtons();
        repaint();
    };
    clearButton.onClick = [this] {
        phrase.setPlaying(false);
        phrase.clear();
        updateButtons();
        repaint();
    };
    legatoButton.onClick = [this] {
        auto n = phrase.note(selectedStep);
        n.legato = !n.legato;
        phrase.setNote(selectedStep, n);
        updateButtons();
        repaint();
    };
    closeButton.onClick = [this] {
        if (onClose) onClose();
    };

    laneBox.addItem("BREATH / FIATO (CC2)", 1);
    laneBox.addItem("VELOCITY / INTENSITY", 2);
    laneBox.addItem("PITCH BEND (+/-2 SEMI)", 3);
    laneBox.setSelectedId(1, juce::dontSendNotification);
    laneBox.onChange = [this] { repaint(); };
    laneBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff213943));
    laneBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    addAndMakeVisible(laneBox);

    tempoLabel.setText("PHRASE BPM", juce::dontSendNotification);
    tempoLabel.setColour(juce::Label::textColourId, juce::Colour(0xffb9ced3));
    tempoLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    addAndMakeVisible(tempoLabel);
    tempoSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    tempoSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 22);
    tempoSlider.setRange(45, 220, 1);
    tempoSlider.setValue(phrase.getTempo(), juce::dontSendNotification);
    tempoSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xffeda972));
    tempoSlider.onValueChange = [this] {
        phrase.setTempo(static_cast<int>(tempoSlider.getValue()));
    };
    addAndMakeVisible(tempoSlider);
    updateButtons();
    startTimerHz(15);
}

juce::Rectangle<float> WindLabPhraseEditor::rollBounds() const
{
    return {75.0f, 166.0f, static_cast<float>(getWidth() - 115), 367.0f};
}

juce::Rectangle<float> WindLabPhraseEditor::laneBounds() const
{
    return {75.0f, 584.0f, static_cast<float>(getWidth() - 115), 181.0f};
}

int WindLabPhraseEditor::stepAt(float x) const
{
    const auto r = rollBounds();
    return juce::jlimit(0, windlab::PhraseEngine::steps - 1,
        static_cast<int>((x - r.getX()) / (r.getWidth() / windlab::PhraseEngine::steps)));
}

int WindLabPhraseEditor::pitchAt(float y) const
{
    const auto r = rollBounds();
    return juce::jlimit(48, 84,
        84 - static_cast<int>((y - r.getY()) / (r.getHeight() / 37.0f)));
}

void WindLabPhraseEditor::resized()
{
    playButton.setBounds(32, 80, 111, 38);
    demoButton.setBounds(150, 80, 98, 38);
    clearButton.setBounds(255, 80, 94, 38);
    legatoButton.setBounds(356, 80, 153, 38);
    tempoLabel.setBounds(538, 66, 130, 20);
    tempoSlider.setBounds(525, 87, 214, 30);
    closeButton.setBounds(getWidth() - 145, 28, 108, 36);
    laneBox.setBounds(75, 543, 287, 34);
}

void WindLabPhraseEditor::updateButtons()
{
    playButton.setButtonText(phrase.isPlaying() ? "STOP" : "PLAY");
    legatoButton.setButtonText(phrase.note(selectedStep).legato
                             ? "LEGATO: ON" : "LEGATO: OFF");
}

void WindLabPhraseEditor::paint(juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff0b151c), 0, 0,
                juce::Colour(0xff203b45), full.getRight(), full.getBottom(), false));
    g.fillAll();
    g.setColour(juce::Colour(0xffeeb17d));
    g.setFont(juce::Font(26.0f, juce::Font::bold));
    g.drawText("WINDLAB  /  EXPRESSION & PHRASE EDITOR", 31, 19,
               getWidth() - 205, 38, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xffb7cbd0));
    g.setFont(juce::Font(13.0f));
    g.drawText("16 steps = 2 bars of eighth-notes. Click to add a note; drag sideways for length, vertically for pitch.",
               34, 129, getWidth() - 65, 28, juce::Justification::centredLeft);

    const auto r = rollBounds(), lane = laneBounds();
    g.setColour(juce::Colour(0xff172b34));
    g.fillRoundedRectangle(r.expanded(2.0f), 5.0f);
    g.fillRoundedRectangle(lane.expanded(2.0f), 5.0f);

    const auto stepWidth = r.getWidth() / 16.0f;
    const auto pitchHeight = r.getHeight() / 37.0f;
    for (int pitch = 48; pitch <= 84; ++pitch)
    {
        const float y = r.getY() + (84 - pitch) * pitchHeight;
        if (pitch % 12 == 0)
        {
            g.setColour(juce::Colour(0xff314d59));
            g.fillRect(r.getX(), y, r.getWidth(), pitchHeight);
            g.setColour(juce::Colour(0xffd3dfe2));
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText("C" + juce::String(pitch / 12 - 1), 35, static_cast<int>(y), 36, 15,
                       juce::Justification::centredRight);
        }
        else
        {
            g.setColour(juce::Colour(0xff213943));
            g.drawHorizontalLine(static_cast<int>(y), r.getX(), r.getRight());
        }
    }

    for (int step = 0; step <= 16; ++step)
    {
        const float x = r.getX() + step * stepWidth;
        g.setColour(step % 4 == 0 ? juce::Colour(0xff7a9299)
                                   : juce::Colour(0xff3b5660));
        g.drawLine(x, r.getY(), x, lane.getBottom(), step % 4 == 0 ? 1.3f : 0.7f);
        if (step < 16)
        {
            g.setColour(juce::Colour(0xffafc3c8));
            g.setFont(juce::Font(11.0f));
            g.drawText(juce::String(step + 1), static_cast<int>(x + 2),
                       static_cast<int>(r.getY() - 18), 38, 16,
                       juce::Justification::centredLeft);
        }
    }

    if (phrase.isPlaying())
    {
        const float x = r.getX() + phrase.visualStep() * stepWidth;
        g.setColour(juce::Colour(0x30f3b879));
        g.fillRect(x, r.getY(), stepWidth, lane.getBottom() - r.getY());
    }

    for (int step = 0; step < 16; ++step)
    {
        const auto n = phrase.note(step);
        if (!n.active) continue;
        const float x = r.getX() + step * stepWidth + 2.0f;
        const float y = r.getY() + (84 - juce::jlimit(48, 84, n.pitch)) * pitchHeight + .7f;
        const float width = std::min(static_cast<float>(n.gate) * stepWidth - 4.0f,
                                    r.getRight() - x - 1.0f);
        g.setColour(step == selectedStep ? juce::Colour(0xffffcb83)
                                         : juce::Colour(0xffed9f6c));
        g.fillRoundedRectangle(x, y, std::max(7.0f, width), pitchHeight - 1.8f, 3.0f);
        if (n.legato)
        {
            g.setColour(juce::Colour(0xffb0eff1));
            g.drawLine(x + width - 11.0f, y - 3.0f,
                       x + width + 4.0f, y - 3.0f, 2.0f);
        }
    }

    // Draw editable continuous expression points: no score renderer required.
    const int laneType = laneBox.getSelectedId();
    g.setColour(juce::Colour(0xffa6bbc1));
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText(laneType == 1 ? "BREATH CC2" :
               laneType == 2 ? "NOTE VELOCITY" : "PITCH BEND", 377, 545, 210, 26,
               juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff92aeb6));
    g.setFont(juce::Font(11.0f));
    g.drawText("Drag points in the expression lane. Breath and pitch interpolate during playback.",
               565, 549, getWidth() - 610, 22, juce::Justification::centredLeft);

    if (laneType == 3)
    {
        g.setColour(juce::Colour(0xff4c6871));
        g.drawHorizontalLine(static_cast<int>(lane.getCentreY()), lane.getX(), lane.getRight());
    }
    for (int step = 0; step < 16; ++step)
    {
        auto getValue = [this, laneType](int s) {
            const auto n = phrase.note(s);
            return laneType == 1 ? n.breath : laneType == 2 ? n.velocity : n.bend;
        };
        const float x = lane.getX() + (step + .5f) * stepWidth;
        const float y = lane.getBottom() - (getValue(step) / 127.0f) * lane.getHeight();
        if (step != 15)
        {
            const float nx = x + stepWidth;
            const float ny = lane.getBottom() - (getValue(step + 1) / 127.0f) * lane.getHeight();
            g.setColour(juce::Colour(0xffeeb17d));
            g.drawLine(x, y, nx, ny, 2.0f);
        }
        g.setColour(step == selectedStep ? juce::Colour(0xffffffff)
                                        : juce::Colour(0xffefa973));
        g.fillEllipse(x - 4.0f, y - 4.0f, 8.0f, 8.0f);
    }

    const auto current = phrase.note(selectedStep);
    g.setColour(juce::Colour(0xffb4cbd2));
    g.setFont(juce::Font(13.0f));
    g.drawText("SELECTED STEP " + juce::String(selectedStep + 1)
        + "   MIDI " + juce::String(current.pitch)
        + "   VELOCITY " + juce::String(current.velocity)
        + "   BREATH " + juce::String(current.breath)
        + "   BEND " + juce::String(current.bend - 64)
        + "   LENGTH " + juce::String(current.gate) + " STEPS",
        75, 781, getWidth() - 115, 23, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff91aeb7));
    g.setFont(juce::Font(11.0f));
    g.drawText("Right click note = delete. LEGATO joins adjacent phrases with a short overlap. "
               "Free-running tempo; DAW timeline synchronization will follow.",
               75, 806, getWidth() - 115, 27, juce::Justification::centredLeft);
}

void WindLabPhraseEditor::mouseDown(const juce::MouseEvent& e)
{
    if (rollBounds().contains(e.position))
    {
        selectedStep = stepAt(e.position.x);
        firstDragStep = selectedStep;
        editingNote = true;
        auto n = phrase.note(selectedStep);
        if (e.mods.isPopupMenu())
            n.active = false;
        else
        {
            n.active = true;
            n.pitch = pitchAt(e.position.y);
            n.gate = 1;
        }
        phrase.setNote(selectedStep, n);
        updateButtons();
        repaint();
    }
    else if (laneBounds().contains(e.position))
    {
        editingLane = true;
        updateLane(e.position.x, e.position.y);
    }
}

void WindLabPhraseEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (editingNote && !e.mods.isPopupMenu())
    {
        auto n = phrase.note(firstDragStep);
        n.pitch = pitchAt(e.position.y);
        n.gate = juce::jlimit(1, 8, stepAt(e.position.x) - firstDragStep + 1);
        n.active = true;
        phrase.setNote(firstDragStep, n);
        repaint();
    }
    else if (editingLane)
        updateLane(e.position.x, e.position.y);
}

void WindLabPhraseEditor::mouseUp(const juce::MouseEvent&)
{
    editingNote = false;
    editingLane = false;
}

void WindLabPhraseEditor::updateLane(float x, float y)
{
    const auto area = laneBounds();
    const int step = stepAt(x);
    selectedStep = step;
    auto n = phrase.note(step);
    const int value = juce::jlimit(0, 127, static_cast<int>(
        std::lround((area.getBottom() - y) / area.getHeight() * 127.0f)));
    switch (laneBox.getSelectedId())
    {
        case 1: n.breath = value; break;
        case 2: n.velocity = std::max(1, value); break;
        case 3: n.bend = value; break;
        default: return;
    }
    phrase.setNote(step, n);
    updateButtons();
    repaint();
}
