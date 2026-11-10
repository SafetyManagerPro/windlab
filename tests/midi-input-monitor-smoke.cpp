#include "Dsp/MidiInputMonitor.h"
#include <cassert>
#include <iostream>

int main()
{
    windlab::MidiInputMonitor monitor;
    assert(!monitor.noteSnapshot().valid);
    assert(!monitor.ccSnapshot().valid);
    monitor.note(1, 60, true);
    auto note = monitor.noteSnapshot();
    assert(note.valid && note.channel == 1 && note.pitch == 60 && note.on);
    monitor.note(7, 60, false);
    note = monitor.noteSnapshot();
    assert(note.valid && note.channel == 7 && note.pitch == 60 && !note.on);
    monitor.controller(2, 11, 96);
    auto cc = monitor.ccSnapshot();
    assert(cc.valid && cc.channel == 2 && cc.number == 11 && cc.value == 96);
    monitor.controller(16, 2, 127); // Internal phrase channel must be hidden.
    assert(monitor.ccSnapshot().channel == 2);
    monitor.note(0, 80, true);
    assert(monitor.noteSnapshot().channel == 7);
    assert(!monitor.pressureSnapshot().valid);
    monitor.pressure(3, 77);
    auto at = monitor.pressureSnapshot();
    assert(at.valid && at.channel == 3 && at.value == 77);
    monitor.pressure(16, 10); // phrase channel hidden
    assert(monitor.pressureSnapshot().channel == 3);
    monitor.clear();
    assert(!monitor.noteSnapshot().valid && !monitor.ccSnapshot().valid && !monitor.pressureSnapshot().valid);
    std::cout << "WindLab MIDI input monitor: PASS\n";
}
