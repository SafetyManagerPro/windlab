#pragma once
// JUCE-free Wind MIDI routing and diagnostics.
// All audio-thread event writes use atomics; no allocation or locks.
// Hardware inputs are selected by the Standalone wrapper or the host DAW.
#include <atomic>
#include <cstdint>

namespace windlab
{
class WindMidiSettings final
{
public:
    enum Source : int { CC2 = 0, CC11 = 1, ChannelPressure = 2 };
    void setEnabled(bool enabled) noexcept
    {
        active.store(enabled, std::memory_order_release);
    }
    bool isEnabled() const noexcept
    {
        return active.load(std::memory_order_acquire);
    }
    bool setSource(int source) noexcept
    {
        if (source < CC2 || source > ChannelPressure) return false;
        breathSource.store(source, std::memory_order_release);
        return true;
    }
    int source() const noexcept
    {
        return breathSource.load(std::memory_order_acquire);
    }
    void reset() noexcept { setEnabled(false); setSource(CC2); }
    bool usesPressure() const noexcept
    {
        return isEnabled() && source() == ChannelPressure;
    }
    // Consumes a NORMALIZED controller (after MIDI Learn has translated it).
    // In WIND mode, choose exactly one source for continuous breath:
    // CC2, CC11 (rerouted to CC2), or Channel Pressure (handled separately).
    int routeController(int normalizedCC) const noexcept
    {
        if (!isEnabled()) return normalizedCC;
        switch (source())
        {
            case CC11:
                if (normalizedCC == 11) return 2;
                if (normalizedCC == 2) return -1;
                break;
            case ChannelPressure:
                if (normalizedCC == 2) return -1;
                break;
            default: break;
        }
        return normalizedCC;
    }
private:
    std::atomic<bool> active {false};
    std::atomic<int> breathSource {CC2};
};

class WindMidiMonitor final
{
public:
    enum EventKind : int { NoEvent = 0, NoteOn = 1, NoteOff = 2,
                           Controller = 3, Pressure = 4, PitchWheel = 5 };
    struct Snapshot
    {
        std::uint64_t received = 0;
        int kind = NoEvent;
        int channel = 0, lastNote = -1, velocity = 0;
        int controller = -1, value = 0, lastPressure = 0;
    };
    void reset() noexcept
    {
        count.store(0, std::memory_order_release);
        eventKind.store(NoEvent, std::memory_order_release);
        channel.store(0, std::memory_order_release);
        lastNote.store(-1, std::memory_order_release);
        velocity.store(0, std::memory_order_release);
        controller.store(-1, std::memory_order_release);
        value.store(0, std::memory_order_release);
        pressure.store(0, std::memory_order_release);
    }
    void observeNoteOn(int midiChannel, int pitch, int noteVelocity) noexcept
    {
        if (!validChannel(midiChannel)) return;
        channel.store(midiChannel, std::memory_order_relaxed);
        lastNote.store(pitch, std::memory_order_relaxed);
        velocity.store(noteVelocity, std::memory_order_relaxed);
        value.store(noteVelocity, std::memory_order_relaxed);
        eventKind.store(NoteOn, std::memory_order_release);
        count.fetch_add(1, std::memory_order_release);
    }
    void observeNoteOff(int midiChannel, int pitch) noexcept
    {
        if (!validChannel(midiChannel)) return;
        channel.store(midiChannel, std::memory_order_relaxed);
        lastNote.store(pitch, std::memory_order_relaxed);
        value.store(0, std::memory_order_relaxed);
        eventKind.store(NoteOff, std::memory_order_release);
        count.fetch_add(1, std::memory_order_release);
    }
    void observeController(int midiChannel, int cc, int ccValue) noexcept
    {
        if (!validChannel(midiChannel)) return;
        channel.store(midiChannel, std::memory_order_relaxed);
        controller.store(cc, std::memory_order_relaxed);
        value.store(ccValue, std::memory_order_relaxed);
        eventKind.store(Controller, std::memory_order_release);
        count.fetch_add(1, std::memory_order_release);
    }
    void observePressure(int midiChannel, int p) noexcept
    {
        if (!validChannel(midiChannel)) return;
        channel.store(midiChannel, std::memory_order_relaxed);
        pressure.store(p, std::memory_order_relaxed);
        value.store(p, std::memory_order_relaxed);
        eventKind.store(Pressure, std::memory_order_release);
        count.fetch_add(1, std::memory_order_release);
    }
    void observePitchWheel(int midiChannel, int wheel) noexcept
    {
        if (!validChannel(midiChannel)) return;
        channel.store(midiChannel, std::memory_order_relaxed);
        value.store(wheel, std::memory_order_relaxed);
        eventKind.store(PitchWheel, std::memory_order_release);
        count.fetch_add(1, std::memory_order_release);
    }
    Snapshot snapshot() const noexcept
    {
        Snapshot s;
        s.received = count.load(std::memory_order_acquire);
        s.kind = eventKind.load(std::memory_order_acquire);
        s.channel = channel.load(std::memory_order_relaxed);
        s.lastNote = lastNote.load(std::memory_order_relaxed);
        s.velocity = velocity.load(std::memory_order_relaxed);
        s.controller = controller.load(std::memory_order_relaxed);
        s.value = value.load(std::memory_order_relaxed);
        s.lastPressure = pressure.load(std::memory_order_relaxed);
        return s;
    }
private:
    static bool validChannel(int ch) noexcept { return ch >= 1 && ch <= 15; }
    std::atomic<std::uint64_t> count {0};
    std::atomic<int> eventKind {NoEvent}, channel {0}, lastNote {-1};
    std::atomic<int> velocity {0}, controller {-1}, value {0}, pressure {0};
};
} 
