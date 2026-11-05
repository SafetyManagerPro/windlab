#include "Dsp/WindMidiInput.h"
#include <cassert>
#include <iostream>

int main()
{
    using windlab::WindMidiSettings;
    using windlab::WindMidiMonitor;
    WindMidiSettings wind;
    assert(!wind.isEnabled());
    assert(wind.source() == WindMidiSettings::CC2);
    assert(wind.routeController(11) == 11);
    assert(!wind.usesPressure());

    wind.setEnabled(true);
    assert(wind.routeController(2) == 2);
    assert(wind.routeController(11) == 11);
    assert(wind.setSource(WindMidiSettings::CC11));
    assert(wind.routeController(11) == 2);
    assert(wind.routeController(2) == -1);
    assert(wind.routeController(1) == 1);
    assert(!wind.usesPressure());
    assert(wind.setSource(WindMidiSettings::ChannelPressure));
    assert(wind.routeController(2) == -1);
    assert(wind.routeController(11) == 11);
    assert(wind.usesPressure());
    assert(!wind.setSource(-1));
    assert(!wind.setSource(3));
    assert(wind.source() == WindMidiSettings::ChannelPressure);
    wind.setEnabled(false);
    assert(wind.routeController(2) == 2);
    assert(!wind.usesPressure());
    wind.reset();
    assert(wind.source() == WindMidiSettings::CC2 && !wind.isEnabled());

    WindMidiMonitor monitor;
    auto s = monitor.snapshot();
    assert(s.received == 0 && s.kind == WindMidiMonitor::NoEvent);
    monitor.observeNoteOn(1, 60, 108);
    s = monitor.snapshot();
    assert(s.received == 1 && s.kind == WindMidiMonitor::NoteOn);
    assert(s.channel == 1 && s.lastNote == 60 && s.velocity == 108);
    monitor.observeController(2, 74, 83);
    s = monitor.snapshot();
    assert(s.received == 2 && s.kind == WindMidiMonitor::Controller);
    assert(s.channel == 2 && s.controller == 74 && s.value == 83);
    monitor.observePressure(3, 109);
    s = monitor.snapshot();
    assert(s.received == 3 && s.kind == WindMidiMonitor::Pressure);
    assert(s.channel == 3 && s.lastPressure == 109);
    monitor.observePitchWheel(4, 10000);
    s = monitor.snapshot();
    assert(s.received == 4 && s.kind == WindMidiMonitor::PitchWheel);
    assert(s.value == 10000);
    monitor.observeNoteOff(1, 60);
    s = monitor.snapshot();
    assert(s.received == 5 && s.kind == WindMidiMonitor::NoteOff);
    monitor.observeController(16, 2, 88); // internal phrase channel must not be reported
    monitor.observeNoteOn(16, 62, 120);
    monitor.observePressure(0, 12);
    assert(monitor.snapshot().received == 5);
    monitor.reset();
    assert(monitor.snapshot().received == 0);
    assert(monitor.snapshot().lastNote == -1);
    std::cout << "Wind MIDI v0.9.4: PASS sources, CC mapping and live monitor\n";
}
