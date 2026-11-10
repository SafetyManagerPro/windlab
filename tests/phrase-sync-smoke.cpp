// WindLab 0.9.7: Phrase Editor locked to the DAW song position.
#include "Dsp/PhraseEngine.h"
#include <cassert>
#include <iostream>

int main()
{
    windlab::PhraseEngine phrase;
    phrase.loadDemo();
    phrase.setTempo(120);
    phrase.setPlaying(true);
    int lastNoteOn = -1;
    auto emit = [&](windlab::PhraseEvent type, int note, int, int)
    {
        if (type == windlab::PhraseEvent::NoteOn) lastNoteOn = note;
    };
    // Eighth grid: quarter note 4.0 = step 8 (start of the second bar).
    phrase.process(256, 48000.0, emit, 4.0);
    assert(phrase.visualStep() == 8);
    // A normal following block (no jump) does not re-align or retrigger.
    lastNoteOn = -1;
    phrase.process(256, 48000.0, emit, 4.0 + 256.0 / 24000.0);
    assert(phrase.visualStep() == 8 && lastNoteOn == -1);
    // Loop wraps every 8 quarter notes: 9.5 -> 1.5 quarters = step 3.
    phrase.process(256, 48000.0, emit, 9.5);
    assert(phrase.visualStep() == 3);
    // Sixteenth grid: 2.25 quarters = step 9.
    phrase.setGrid(windlab::PhraseGrid::Sixteenth);
    phrase.process(256, 48000.0, emit, 2.25);
    assert(phrase.visualStep() == 9);
    // Without a host position nothing is forced.
    phrase.process(256, 48000.0, emit);
    assert(phrase.visualStep() == 9);
    std::cout << "WindLab 0.9.7 phrase DAW sync: PASS\n";
    return 0;
}
