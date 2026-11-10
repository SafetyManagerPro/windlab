// WindLab 0.9.7: Phrase Editor locked to the DAW song position.
#include "Dsp/PhraseEngine.h"
#include <algorithm>
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
    // --- 0.9.10 longer phrases: 4 and 8 bars.
    {
        windlab::PhraseEngine longPhrase;
        assert(longPhrase.getBars() == 2 && longPhrase.activeSteps() == 16);
        longPhrase.setBars(4);
        assert(longPhrase.activeSteps() == 32);
        longPhrase.setBars(5); // only 2, 4, 8 exist
        assert(longPhrase.getBars() == 4);
        longPhrase.setBars(8);
        longPhrase.setGrid(windlab::PhraseGrid::SixteenthTriplet);
        assert(longPhrase.activeSteps() == 192 && windlab::PhraseEngine::maxSteps == 192);
        // A note on the last step of an 8-bar phrase is stored and played.
        windlab::PhraseNote n;
        n.active = true;
        n.pitch = 72;
        longPhrase.setNote(191, n);
        assert(longPhrase.note(191).active);
        longPhrase.setGrid(windlab::PhraseGrid::Eighth);
        longPhrase.setPlaying(true);
        // 8 bars = 32 quarter notes; quarter 28 = eighth step 56; 33 wraps to step 2.
        longPhrase.process(256, 48000.0, emit, 28.0);
        assert(longPhrase.visualStep() == 56);
        longPhrase.process(256, 48000.0, emit, 33.0);
        assert(longPhrase.visualStep() == 2);
        // The internal loop also runs through all 64 steps of 8 bars of eighths.
        windlab::PhraseEngine looping;
        looping.setBars(8);
        looping.setTempo(120);
        looping.setPlaying(true);
        int highest = 0;
        for (int block = 0; block < 2000; ++block)
        {
            looping.process(480, 48000.0, emit);
            highest = std::max(highest, looping.visualStep());
        }
        assert(highest == 63);
    }

    std::cout << "WindLab 0.9.7 phrase DAW sync: PASS\n";
    return 0;
}
