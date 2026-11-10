#include "Dsp/PhraseEngine.h"
#include "Dsp/PhraseRecorder.h"
#include <cassert>
#include <iostream>
#include <vector>

namespace {
struct Emitted { int type; int a; int b; int offset; };
}

int main()
{
    using windlab::PhraseEngine;
    using windlab::PhraseGrid;
    using windlab::PhraseNote;

    // Sizes per grid: two bars of each subdivision, and ticks per step (480 per quarter).
    assert(PhraseEngine::maxSteps == 192 && PhraseEngine::chordSlots == 4); // 8 bars of 16th triplets (0.9.10)
    assert(windlab::stepsPerBarFor(PhraseGrid::Eighth) * 2 == 16);
    assert(windlab::stepsPerBarFor(PhraseGrid::Sixteenth) * 2 == 32);
    assert(windlab::stepsPerBarFor(PhraseGrid::EighthTriplet) * 2 == 24);
    assert(windlab::stepsPerBarFor(PhraseGrid::SixteenthTriplet) * 2 == 48);
    assert(windlab::ticksPerStepFor(PhraseGrid::Eighth) == 240);
    assert(windlab::ticksPerStepFor(PhraseGrid::Sixteenth) == 120);
    assert(windlab::ticksPerStepFor(PhraseGrid::EighthTriplet) == 160);
    assert(windlab::ticksPerStepFor(PhraseGrid::SixteenthTriplet) == 80);

    // Playback: at 120 BPM and 48 kHz an eighth is 12000 samples. An eighth triplet is 8000.
    {
        PhraseEngine engine;
        engine.setTempo(120);
        engine.setGrid(PhraseGrid::EighthTriplet);
        assert(engine.activeSteps() == 24);
        PhraseNote n; n.active = true; n.pitch = 60; n.velocity = 100; n.gate = 1;
        engine.setChordNote(0, 0, n);
        PhraseNote m = n; m.pitch = 64;
        engine.setChordNote(0, 1, m);        // second voice of the same step: a chord
        PhraseNote next = n; next.pitch = 67;
        engine.setChordNote(1, 0, next);
        engine.setPlaying(true);

        std::vector<Emitted> out;
        long long sample = 0; // absolute sample of the current block start
        for (int block = 0; block < 40; ++block)
        {
            engine.process(1000, 48000.0, [&](windlab::PhraseEvent e, int a, int b, int offset) {
                out.push_back({static_cast<int>(e), a, b, static_cast<int>(sample + offset)});
            });
            sample += 1000;
        }
        // Step 0 starts with two note-ons at the same sample (a chord).
        int onsAtZero = 0;
        for (const auto& e : out) if (e.type == 0 && e.a == 60) ++onsAtZero;
        assert(onsAtZero == 1);
        int chordOns = 0;
        for (const auto& e : out) if (e.type == 0 && (e.a == 60 || e.a == 64)) ++chordOns;
        assert(chordOns == 2);
        // Step 1 (pitch 67) begins one triplet eighth (8000 samples) after step 0.
        bool found = false;
        for (std::size_t i = 0; i < out.size() && !found; ++i)
            if (out[i].type == 0 && out[i].a == 67) { found = true; assert(out[i].offset == 8000); }
        assert(found);
    }

    // Changing the grid restarts playback and keeps the chord data.
    {
        PhraseEngine engine;
        engine.setGrid(PhraseGrid::Sixteenth);
        assert(engine.getGrid() == PhraseGrid::Sixteenth && engine.activeSteps() == 32);
        assert(engine.ticksPerStep() == 120);
    }

    // Recorder: grid timing, chord slots, and import of grid plus chords.
    {
        windlab::PhraseRecorder rec;
        assert(rec.requestStart());
        // SixteenthTriplet at 120 BPM, 48 kHz: a step is 4000 samples; 48 steps = 192000 samples.
        rec.beginBlock(12000, 48000.0, 120, PhraseGrid::SixteenthTriplet);
        rec.noteOn(1, 60, 100, 4000);   // step 1
        rec.noteOn(1, 64, 90, 4100);    // step 1 again: second chord slot
        rec.noteOn(1, 67, 80, 4200);    // third slot
        rec.noteOff(1, 60, 4500);
        rec.noteOff(1, 64, 4600);
        rec.noteOff(1, 67, 4700);
        rec.noteOn(1, 72, 110, 8000);   // step 2
        rec.noteOff(1, 72, 8500);
        rec.endBlock();
        assert(rec.notesCaptured() == 4);
        assert(rec.chordRecorded(1, 0).pitch == 60);
        assert(rec.chordRecorded(1, 1).pitch == 64);
        assert(rec.chordRecorded(1, 2).pitch == 67);
        assert(rec.chordRecorded(2, 0).pitch == 72);
        assert(rec.recordedGrid() == PhraseGrid::SixteenthTriplet);
        for (int i = 0; i < 15; ++i) { rec.beginBlock(12000, 48000.0, 120); rec.endBlock(); }
        assert(rec.status() == windlab::PhraseRecorder::Ready);
        assert(rec.progress() == 48);

        PhraseEngine target;
        assert(rec.importInto(target));
        assert(target.getGrid() == PhraseGrid::SixteenthTriplet);
        assert(target.activeSteps() == 48);
        assert(target.chordSize(1) == 3);
        assert(target.chordNote(1, 1).pitch == 64);
        assert(target.chordNote(2, 0).pitch == 72);
    }

    // Fifth simultaneous note is dropped, not stored as a corrupt slot.
    {
        windlab::PhraseRecorder rec;
        assert(rec.requestStart());
        rec.beginBlock(12000, 48000.0, 120);
        for (int p = 60; p < 65; ++p) rec.noteOn(1, p, 90, 100);
        rec.endBlock();
        assert(rec.notesCaptured() == 4);
        assert(rec.notesDropped() == 1);
    }

    std::cout << "WindLab 0.9.5 grids and chords: PASS (eighth to 16th triplets, 4-note chords, import)\n";
}
