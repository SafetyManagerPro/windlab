#include "PhraseEditor.h"
#include "Dsp/PhraseMidi.h"
#include <algorithm>
#include <cmath>

WindLabPhraseEditor::WindLabPhraseEditor(windlab::PhraseEngine& engine,
                                       windlab::PhraseRecorder& recorder,
                                       std::function<void()> close,
                                       std::function<void()> help)
    : phrase(engine), capture(recorder), onClose(std::move(close)), onHelp(std::move(help))
{
    setOpaque(true);
    for (auto* b : {&playButton, &demoButton, &clearButton, &legatoButton, &closeButton, &hostButton, &scoreButton, &helpButton, &recordButton, &importButton})
    {
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff294651));
        b->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        addAndMakeVisible(*b);
    }
    recordingStatus.setColour(juce::Label::textColourId, juce::Colour(0xffb5d0dc));
    recordingStatus.setFont(juce::Font(12.0f, juce::Font::bold));
    addAndMakeVisible(recordingStatus);
    recordButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffad433b));
    recordButton.setTooltip("Registra MIDI esterno o dalla tastiera PC per due battute (16 step)");
    importButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff507e5c));
    importButton.setTooltip("Sostituisce la frase del piano roll con la registrazione completata");
    recordButton.onClick = [this] {
        if (capture.status() == windlab::PhraseRecorder::Recording)
            capture.requestStop();
        else
        {
            phrase.setPlaying(false);
            capture.requestStart();
        }
        updateButtons();
    };
    importButton.onClick = [this] {
        if (capture.importInto(phrase))
        {
            selectedStep = 0;
            scoreView = false;
            updateButtons();
            repaint();
        }
    };
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
        if (capture.status() == windlab::PhraseRecorder::Recording)
            capture.requestStop();
        if (onClose) onClose();
    };
    helpButton.onClick = [this] { if (onHelp) onHelp(); };
    helpButton.setTooltip("Guida Expression & Phrase Editor");
    hostButton.onClick = [this] {
        phrase.setFollowHostTempo(!phrase.followsHostTempo());
        updateButtons();
    };
    scoreButton.onClick = [this] {
        scoreView = !scoreView;
        updateButtons();
        repaint();
    };

    laneBox.addItem("BREATH / FIATO (CC2)", 1);
    laneBox.addItem("VELOCITY / INTENSITY", 2);
    laneBox.addItem("VOLUME / EXPRESSION CC11", 3);
    laneBox.addItem("PITCH BEND (+/-2 SEMI)", 4);
    laneBox.addItem("VIBRATO / MOD WHEEL (CC1)", 5);
    laneBox.setSelectedId(1, juce::dontSendNotification);
    laneBox.onChange = [this] { repaint(); };
    laneBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff213943));
    laneBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    addAndMakeVisible(laneBox);

    gridBox.addItem("GRID 1/8 (16 STEPS)", 1);
    gridBox.addItem("GRID 1/16 (32 STEPS)", 2);
    gridBox.addItem("GRID 1/8 TRIPLET (24)", 3);
    gridBox.addItem("GRID 1/16 TRIPLET (48)", 4);
    gridBox.setSelectedId(static_cast<int>(phrase.getGrid()) + 1, juce::dontSendNotification);
    gridBox.onChange = [this] {
        phrase.setGrid(static_cast<windlab::PhraseGrid>(gridBox.getSelectedId() - 1));
        selectedStep = std::min(selectedStep, stepCount() - 1);
        repaint();
    };
    gridBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff213943));
    gridBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    addAndMakeVisible(gridBox);
    exportButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff416a64));
    exportButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    exportButton.onClick = [this] { exportMidi(); };
    addAndMakeVisible(exportButton);
    syncButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff294c59));
    syncButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    syncButton.setTooltip("La frase parte e si ferma con il play della DAW e segue la sua posizione");
    syncButton.onClick = [this] {
        const bool on = !phrase.followsHostTransport();
        phrase.setFollowHostTransport(on);
        if (on) phrase.setFollowHostTempo(true); // a locked loop must also use the DAW tempo
        else phrase.setPlaying(false);
        updateButtons();
    };
    addAndMakeVisible(syncButton);
    dragButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff8a5a2e));
    dragButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    dragButton.setTooltip("Trascina questo pulsante su una traccia della DAW per inserire la frase come clip MIDI");
    dragButton.makeFile = [this] { return writePhraseToTempFile(); };
    addAndMakeVisible(dragButton);

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
    return juce::jlimit(0, stepCount() - 1,
        static_cast<int>((x - r.getX()) / (r.getWidth() / static_cast<float>(stepCount()))));
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
    recordButton.setBounds(getWidth() - 640, 27, 102, 36);
    importButton.setBounds(getWidth() - 526, 27, 125, 36);
    recordingStatus.setBounds(getWidth() - 389, 29, 180, 34);
    helpButton.setBounds(getWidth() - 199, 28, 43, 36);
    hostButton.setBounds(757, 80, 147, 38);
    scoreButton.setBounds(911, 80, 140, 38);
    laneBox.setBounds(75, 543, 287, 34);
    gridBox.setBounds(75, 790, 240, 32);
    exportButton.setBounds(330, 790, 170, 32);
    syncButton.setBounds(512, 790, 220, 32);
    dragButton.setBounds(744, 790, 200, 32);
}

void WindLabPhraseEditor::updateButtons()
{
    playButton.setButtonText(phrase.isPlaying() ? "STOP" : "PLAY");
    legatoButton.setButtonText(phrase.note(selectedStep).legato
                             ? "LEGATO: ON" : "LEGATO: OFF");
    syncButton.setButtonText(phrase.followsHostTransport() ? "SEGUI PLAY DAW: ON" : "SEGUI PLAY DAW: OFF");
    syncButton.setColour(juce::TextButton::buttonColourId,
                         phrase.followsHostTransport() ? juce::Colour(0xff416a64) : juce::Colour(0xff294c59));
    hostButton.setButtonText(phrase.followsHostTempo()
        ? "HOST " + juce::String(phrase.effectiveTempo()) + " BPM" : "HOST BPM: OFF");
    scoreButton.setButtonText(scoreView ? "PIANO ROLL" : "SCORE VIEW");
    tempoSlider.setEnabled(!phrase.followsHostTempo());
    const int recordState = capture.status();
    recordButton.setButtonText(recordState == windlab::PhraseRecorder::Recording
                                  ? "STOP REC" : "REC");
    importButton.setEnabled(recordState == windlab::PhraseRecorder::Ready);
    const juce::String recordText =
        recordState == windlab::PhraseRecorder::Recording
        ? ("REC " + juce::String(capture.progress()) + "/" + juce::String(phrase.activeSteps()))
        : recordState == windlab::PhraseRecorder::Ready
          ? ("PRONTA " + juce::String(capture.notesCaptured()) + " NOTE")
          : "MIDI: PRONTO";
    if (recordingStatus.getText() != recordText)
        recordingStatus.setText(recordText, juce::dontSendNotification);
}

void WindLabPhraseEditor::paint(juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff0a151b));
    // Dialog-like window rendered within the plugin: works in hosts which
    // restrict external native windows (AU and VST3), and in Standalone.
    g.setColour(juce::Colour(0xff42606c));
    g.fillRoundedRectangle(5.0f, 5.0f, full.getWidth() - 10.0f, full.getHeight() - 10.0f, 13.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff152b35), 0, 0,
                juce::Colour(0xff203b45), full.getRight(), full.getBottom(), false));
    g.fillRoundedRectangle(8.0f, 8.0f, full.getWidth() - 16.0f, full.getHeight() - 16.0f, 11.0f);
    g.setColour(juce::Colour(0xffeeb17d));
    g.setFont(juce::Font(21.0f, juce::Font::bold));
    g.drawText("WINDLAB / PHRASE EDITOR", 27, 19,
               395, 38, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xffb7cbd0));
    g.setFont(juce::Font(13.0f));
    g.drawText(capture.status() == windlab::PhraseRecorder::Recording
                   ? "REGISTRAZIONE MIDI IN CORSO: suona la tastiera o il controller. STOP REC per terminare prima."
                   : (capture.status() == windlab::PhraseRecorder::Ready
                      ? "REGISTRAZIONE PRONTA: premi IMPORTA per modificare le note; la frase originale non e' stata cancellata."
                      : (scoreView
                         ? "Score preview: staff, accidentals, note lengths and slurs. Switch to Piano Roll for editing."
                         : "Two bars. Click an empty row to add a chord note; click a note to remove it; drag it to move or change its length. Right click removes.")),
               34, 129, getWidth() - 65, 28, juce::Justification::centredLeft);

    const auto r = rollBounds(), lane = laneBounds();
    g.setColour(juce::Colour(0xff172b34));
    g.fillRoundedRectangle(r.expanded(2.0f), 5.0f);
    g.fillRoundedRectangle(lane.expanded(2.0f), 5.0f);

    const auto stepWidth = r.getWidth() / static_cast<float>(stepCount());
    const auto pitchHeight = r.getHeight() / 37.0f;
    if (!scoreView)
    {
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

    }
    else
    {
        // Staff preview uses semitone-aware diatonic positions. The on-screen
        // piano roll remains the primary editable representation.
        const float staffBottom = r.getY() + 277.0f;
        constexpr float halfSpace = 9.3f;
        for (int line = 0; line < 5; ++line)
        {
            const float y = staffBottom - line * (2.0f * halfSpace);
            g.setColour(juce::Colour(0xff9bb4ba));
            g.drawLine(r.getX() + 2.0f, y, r.getRight() - 3.0f, y, 1.2f);
        }
        g.setColour(juce::Colour(0xffe4c19f));
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText("TREBLE   E4 - F5", static_cast<int>(r.getX() + 7), 
                   static_cast<int>(staffBottom + 33), 235, 24, juce::Justification::centredLeft);
    }

    for (int step = 0; step <= stepCount(); ++step)
    {
        const float x = r.getX() + step * stepWidth;
        g.setColour(step % 4 == 0 ? juce::Colour(0xff7a9299)
                                   : juce::Colour(0xff3b5660));
        g.drawLine(x, r.getY(), x, lane.getBottom(), step % 4 == 0 ? 1.3f : 0.7f);
        if (step < stepCount())
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

    if (!scoreView)
    {
    for (int step = 0; step < stepCount(); ++step)
    for (int slot = 0; slot < windlab::PhraseEngine::chordSlots; ++slot)
    {
        const auto n = phrase.chordNote(step, slot);
        if (!n.active) continue;
        const float x = r.getX() + step * stepWidth + 2.0f;
        const float y = r.getY() + (84 - juce::jlimit(48, 84, n.pitch)) * pitchHeight + .7f;
        const float width = std::min(static_cast<float>(n.gate) * stepWidth - 4.0f,
                                    r.getRight() - x - 1.0f);
        // Softer notes are drawn darker, so chord balance is visible at a glance.
        const float strength = .45f + .55f * static_cast<float>(n.velocity) / 127.0f;
        g.setColour((step == selectedStep ? juce::Colour(0xffffcb83) : juce::Colour(0xffed9f6c))
                        .withMultipliedBrightness(strength));
        g.fillRoundedRectangle(x, y, std::max(7.0f, width), pitchHeight - 1.8f, 3.0f);
        if (width >= 24.0f && pitchHeight >= 8.0f)
        {
            g.setColour(juce::Colour(0xff2a1a0c));
            g.setFont(juce::Font(std::min(10.0f, pitchHeight - 1.0f), juce::Font::bold));
            g.drawText(juce::String(n.velocity), juce::Rectangle<float>(x + 3.0f, y, width - 4.0f, pitchHeight - 1.8f).toNearestInt(),
                       juce::Justification::centredLeft, false);
        }
        if (n.legato)
        {
            g.setColour(juce::Colour(0xffb0eff1));
            g.drawLine(x + width - 11.0f, y - 3.0f,
                       x + width + 4.0f, y - 3.0f, 2.0f);
        }
    }

    }
    else
    {
        const float staffBottom = r.getY() + 277.0f;
        constexpr float halfSpace = 9.3f;
        constexpr int semiToDiatonic[12] {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
        const auto staffIndex = [&](int midi) {
            const int clamped = juce::jlimit(0, 127, midi);
            return (clamped / 12 - 5) * 7 + semiToDiatonic[clamped % 12];
        };
        const auto noteY = [&](int midi) {
            return staffBottom - (staffIndex(midi) - 2) * halfSpace;
        };
        for (int step = 0; step < stepCount(); ++step)
        {
            const auto n = phrase.note(step);
            if (!n.active) continue;
            const float x = r.getX() + (step + .5f) * stepWidth;
            const float y = noteY(n.pitch);
            const int staff = staffIndex(n.pitch);
            g.setColour(juce::Colour(0xff96b5c0));
            for (int index = 0; index >= staff; index -= 2)
            {
                const float ly = staffBottom - (index - 2) * halfSpace;
                g.drawLine(x - 12.0f, ly, x + 16.0f, ly, 1.1f);
            }
            for (int index = 12; index <= staff; index += 2)
            {
                const float ly = staffBottom - (index - 2) * halfSpace;
                g.drawLine(x - 12.0f, ly, x + 16.0f, ly, 1.1f);
            }
            g.setColour(step == selectedStep ? juce::Colour(0xffffd18e)
                                             : juce::Colour(0xffeeaa79));
            g.fillEllipse(x - 9.0f, y - 5.5f, 18.0f, 11.0f);
            if (staff < 6)
                g.drawLine(x + 8.0f, y, x + 8.0f, y - 29.0f, 1.7f);
            else
                g.drawLine(x - 8.0f, y, x - 8.0f, y + 29.0f, 1.7f);
            const int pitchClass = juce::jlimit(0, 127, n.pitch) % 12;
            if (pitchClass == 1 || pitchClass == 3 || pitchClass == 6 ||
                pitchClass == 8 || pitchClass == 10)
            {
                g.setFont(juce::Font(15.0f, juce::Font::bold));
                g.drawText("#", static_cast<int>(x - 23), static_cast<int>(y - 12), 14, 25,
                           juce::Justification::centred);
            }
            // Length is represented visually by an unobtrusive duration guide,
            // not an invented musical rhythmic value.
            if (n.gate > 1)
            {
                g.setColour(juce::Colour(0xff688c94));
                g.drawLine(x + 12.0f, y + 12.0f,
                           std::min(r.getRight() - 3.0f,
                                    x + stepWidth * (n.gate - 1) + 12.0f),
                           y + 12.0f, 1.8f);
            }
            // In the MIDI engine, LEGATO belongs to the destination note:
            // draw a slur from a previous sounding note into this attack.
            if (n.legato)
            {
                for (int previous = step - 1; previous >= 0; --previous)
                {
                    const auto src = phrase.note(previous);
                    if (!src.active || previous + src.gate < step) continue;
                    const float x0 = r.getX() + (previous + .5f) * stepWidth;
                    const float y0 = noteY(src.pitch);
                    juce::Path slur;
                    slur.startNewSubPath(x0 + 9.0f, y0 - 9.0f);
                    slur.quadraticTo((x0 + x) * .5f, std::min(y0, y) - 29.0f,
                                    x - 9.0f, y - 9.0f);
                    g.setColour(juce::Colour(0xffa6ebef));
                    g.strokePath(slur, juce::PathStrokeType(1.8f));
                    break;
                }
            }
        }
    }

    // Draw editable continuous expression points: no score renderer required.
    const int laneType = laneBox.getSelectedId();
    g.setColour(juce::Colour(0xffa6bbc1));
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText(laneType == 1 ? "BREATH CC2" :
               laneType == 2 ? "NOTE VELOCITY" :
               laneType == 3 ? "VOLUME CC11" :
               laneType == 4 ? "PITCH BEND" : "VIBRATO CC1", 377, 545, 210, 26,
               juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff92aeb6));
    g.setFont(juce::Font(11.0f));
    g.drawText("Drag lane points. Breath, volume, pitch and vibrato interpolate during held notes.",
               565, 549, getWidth() - 610, 22, juce::Justification::centredLeft);

    if (laneType == 4)
    {
        g.setColour(juce::Colour(0xff4c6871));
        g.drawHorizontalLine(static_cast<int>(lane.getCentreY()), lane.getX(), lane.getRight());
    }
    for (int step = 0; step < stepCount(); ++step)
    {
        auto getValue = [this, laneType](int s) {
            const auto n = phrase.note(s);
            return laneType == 1 ? n.breath :
                   laneType == 2 ? n.velocity :
                   laneType == 3 ? n.expression :
                   laneType == 4 ? n.bend : n.vibrato;
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
        + "   VOLUME " + juce::String(current.expression)
        + "   BEND " + juce::String(current.bend - 64)
        + "   VIBRATO " + juce::String(current.vibrato)
        + "   LENGTH " + juce::String(current.gate) + " STEPS",
        75, 781, getWidth() - 115, 23, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff91aeb7));
    g.setFont(juce::Font(11.0f));
    g.drawText(scoreView
              ? "Notation is a preview, not editable. Use Piano Roll to edit notes and legato."
              : (phrase.followsHostTempo()
                 ? "Tempo follows DAW BPM; Play remains manual and does not follow host transport position."
                 : "Click empty row = add note (max 4 per step). Click note = remove, drag = pitch/length. Right-click = delete."),
               75, 806, getWidth() - 115, 27, juce::Justification::centredLeft);
}

void WindLabPhraseEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto r = rollBounds();
    if (scoreView || !r.contains(e.position) || std::abs(wheel.deltaY) < 1.0e-4f) return;
    const int pitch = pitchAt(e.position.y);
    // The note under the mouse may have started on an earlier step (long notes).
    const int hovered = stepAt(e.position.x);
    for (int step = hovered; step >= 0 && hovered - step <= 8; --step)
    {
        const int slot = slotAtPitch(step, pitch);
        if (slot < 0) continue;
        auto n = phrase.chordNote(step, slot);
        if (step + n.gate <= hovered) break;
        n.velocity = juce::jlimit(1, 127, n.velocity + (wheel.deltaY > 0 ? 4 : -4));
        phrase.setChordNote(step, slot, n);
        selectedStep = step;
        repaint();
        return;
    }
}

int WindLabPhraseEditor::slotAtPitch(int step, int pitch) const
{
    for (int slot = 0; slot < windlab::PhraseEngine::chordSlots; ++slot)
    {
        const auto n = phrase.chordNote(step, slot);
        if (n.active && n.pitch == pitch) return slot;
    }
    return -1;
}

int WindLabPhraseEditor::firstFreeSlot(int step) const
{
    for (int slot = 0; slot < windlab::PhraseEngine::chordSlots; ++slot)
        if (!phrase.chordNote(step, slot).active) return slot;
    return -1;
}

void WindLabPhraseEditor::mouseDown(const juce::MouseEvent& e)
{
    if (rollBounds().contains(e.position))
    {
        const int step = stepAt(e.position.x);
        selectedStep = step;
        if (scoreView) { updateButtons(); repaint(); return; }
        const int pitch = pitchAt(e.position.y);
        const int existing = slotAtPitch(step, pitch);
        if (e.mods.isPopupMenu())
        {
            // Right click removes the chord note under the pointer.
            if (existing >= 0)
            {
                auto n = phrase.chordNote(step, existing);
                n.active = false;
                phrase.setChordNote(step, existing, n);
            }
        }
        else if (existing >= 0)
        {
            // Grab a note of the chord: dragging moves it, a plain click removes it.
            firstDragStep = step;
            dragSlot = existing;
            editingNote = true;
            clickedExisting = true;
            dragMoved = false;
        }
        else
        {
            const int slot = firstFreeSlot(step);
            if (slot >= 0)
            {
                // Controller values of the step travel with its primary slot; the new note takes its own pitch.
                auto n = phrase.note(step);
                n.active = true;
                n.pitch = pitch;
                n.velocity = 96;
                n.gate = 1;
                n.legato = false;
                phrase.setChordNote(step, slot, n);
                firstDragStep = step;
                dragSlot = slot;
                editingNote = true;
                clickedExisting = false;
                dragMoved = false;
            }
            // else: the step already holds the maximum of four notes.
        }
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
        auto n = phrase.chordNote(firstDragStep, dragSlot);
        const int pitch = pitchAt(e.position.y);
        const int gate = juce::jlimit(1, 8, stepAt(e.position.x) - firstDragStep + 1);
        // Two notes of one chord cannot share a pitch: a drag onto another note keeps the old pitch.
        const int clash = slotAtPitch(firstDragStep, pitch);
        const bool pitchFree = clash < 0 || clash == dragSlot;
        if ((pitchFree && pitch != n.pitch) || gate != n.gate) dragMoved = true;
        if (pitchFree) n.pitch = pitch;
        n.gate = gate;
        n.active = true;
        phrase.setChordNote(firstDragStep, dragSlot, n);
        repaint();
    }
    else if (editingLane)
        updateLane(e.position.x, e.position.y);
}

void WindLabPhraseEditor::mouseUp(const juce::MouseEvent&)
{
    if (editingNote && clickedExisting && !dragMoved)
    {
        auto n = phrase.chordNote(firstDragStep, dragSlot);
        n.active = false;
        phrase.setChordNote(firstDragStep, dragSlot, n);
    }
    editingNote = false;
    editingLane = false;
    clickedExisting = false;
    dragMoved = false;
    updateButtons();
    repaint();
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
        case 3: n.expression = value; break;
        case 4: n.bend = value; break;
        case 5: n.vibrato = value; break;
        default: return;
    }
    phrase.setNote(step, n);
    updateButtons();
    repaint();
}

juce::File WindLabPhraseEditor::writePhraseToTempFile() const
{
    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("WindLab-frase.mid");
    const auto bytes = windlab::buildPhraseMidiFile(phrase, phrase.effectiveTempo());
    file.replaceWithData(bytes.data(), bytes.size());
    return file;
}

void WindLabPhraseEditor::exportMidi()
{
    exportChooser = std::make_unique<juce::FileChooser>(
        "Esporta la frase come file MIDI", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("WindLab-frase.mid"), "*.mid");
    exportChooser->launchAsync(juce::FileBrowserComponent::saveMode
                                   | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& chooser) {
            const auto file = chooser.getResult();
            if (file == juce::File()) return;
            const auto target = file.hasFileExtension(".mid") ? file : file.withFileExtension(".mid");
            const auto bytes = windlab::buildPhraseMidiFile(phrase, phrase.effectiveTempo());
            target.replaceWithData(bytes.data(), bytes.size());
        });
}
