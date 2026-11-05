// Runtime assertions must remain enabled even in optimized Release CI.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "Dsp/PhraseEngine.h"
#include "Dsp/PhraseRecorder.h"
#include <cassert>
#include <iostream>

int main()
{
    constexpr int block = 12000; // 120 BPM, 48 kHz -> 1 eighth note/step
    windlab::PhraseEngine existing;
    windlab::PhraseNote oldNote;
    oldNote.active = true;
    oldNote.pitch = 77;
    existing.setNote(0, oldNote);

    windlab::PhraseRecorder recorder;
    assert(recorder.status() == windlab::PhraseRecorder::Idle);
    assert(!recorder.importInto(existing));
    assert(recorder.requestStart());

    recorder.beginBlock(block, 48000.0, 120);
    assert(recorder.status() == windlab::PhraseRecorder::Recording);
    recorder.noteOn(16, 90, 120, 0); // channel 16 is reserved for playback
    recorder.noteOn(1, 60, 100, 0);
    recorder.controller(1, 2, 90, 4000);
    recorder.pitchWheel(1, 8192 + 12 * 128, 8000);
    recorder.endBlock();
    assert(recorder.notesCaptured() == 1);
    assert(recorder.recorded(0).pitch == 60);
    assert(recorder.recorded(0).velocity == 100);
    assert(recorder.recorded(0).breath == 90);
    assert(recorder.recorded(0).bend == 76);
    assert(existing.note(0).pitch == 77); // existing phrase not overwritten by REC

    recorder.beginBlock(block, 48000.0, 120);
    recorder.noteOn(2, 62, 84, 0); // recorded while 60 held => legato
    recorder.controller(2, 11, 75, 400);
    recorder.controller(2, 1, 43, 900);
    recorder.endBlock();
    assert(recorder.recorded(1).pitch == 62);
    assert(recorder.recorded(1).legato);
    assert(recorder.recorded(1).expression == 75);
    assert(recorder.recorded(1).vibrato == 43);
    assert(existing.note(0).pitch == 77);

    recorder.beginBlock(block, 48000.0, 120);
    recorder.noteOff(1, 60, 0); // first note lasted exactly two steps
    recorder.noteOff(2, 62, 5500); // second note approx 1.46 steps
    recorder.endBlock();
    assert(recorder.recorded(0).gate == 2);
    assert(recorder.recorded(1).gate == 1);

    for (int step = 3; step < windlab::PhraseEngine::steps; ++step)
    {
        recorder.beginBlock(block, 48000.0, 120);
        recorder.endBlock();
    }
    assert(recorder.status() == windlab::PhraseRecorder::Ready);
    assert(recorder.progress() == 16);
    assert(recorder.notesCaptured() == 2);
    assert(recorder.recorded(10).breath == 90);
    assert(recorder.recorded(10).expression == 75);
    assert(recorder.recorded(10).vibrato == 43);
    assert(existing.note(0).pitch == 77);
    assert(recorder.importInto(existing));
    assert(existing.note(0).pitch == 60);
    assert(existing.note(0).velocity == 100);
    assert(existing.note(0).gate == 2);
    assert(existing.note(1).pitch == 62);
    assert(existing.note(1).legato);
    assert(existing.note(8).expression == 75);
    assert(recorder.status() == windlab::PhraseRecorder::Idle);

    // Explicit STOP REC works before the 2-bar auto-stop; no existing phrase
    // is changed until the user chooses IMPORTA.
    existing.setNote(0, oldNote);
    assert(recorder.requestStart());
    recorder.beginBlock(block, 48000.0, 120);
    recorder.noteOn(3, 67, 72, 0);
    recorder.endBlock();
    recorder.requestStop();
    recorder.beginBlock(block, 48000.0, 120);
    recorder.endBlock();
    assert(recorder.status() == windlab::PhraseRecorder::Ready);
    assert(existing.note(0).pitch == 77);
    assert(recorder.recorded(0).gate >= 1);
    assert(recorder.importInto(existing));
    assert(existing.note(0).pitch == 67);

    // Stop without any MIDI yields editable empty phrase, not a crash.
    assert(recorder.requestStart());
    recorder.beginBlock(block, 48000.0, 120);
    recorder.endBlock();
    recorder.requestStop();
    recorder.beginBlock(block, 48000.0, 120);
    recorder.endBlock();
    assert(recorder.status() == windlab::PhraseRecorder::Ready);
    assert(recorder.notesCaptured() == 0);
    assert(!recorder.recorded(0).active);

    std::cout << "WindLab 0.9.3 phrase record: PASS (2 bars, notes, gates, CCs, pitch, non-destructive import)\n";
}
