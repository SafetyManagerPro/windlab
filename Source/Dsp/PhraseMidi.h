#pragma once
// WindLab 0.9.5. Standard MIDI File export of the 16-step phrase, for DAWs
// (Logic, Studio One, Ableton, Reaper, ...). JUCE-free and allocation-light:
// it runs on the message thread only, never in the audio callback.
//
// Layout: format 0, one track, 480 ticks per quarter note. One step is one
// eighth note (240 ticks), so the phrase is two 4/4 bars. The tempo meta event
// carries the phrase BPM. Channel 1 is used for every event.
#include "PhraseEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace windlab
{
namespace midi_detail
{
    inline void appendBE32(std::vector<std::uint8_t>& out, std::uint32_t v)
    {
        out.push_back(static_cast<std::uint8_t>(v >> 24));
        out.push_back(static_cast<std::uint8_t>(v >> 16));
        out.push_back(static_cast<std::uint8_t>(v >> 8));
        out.push_back(static_cast<std::uint8_t>(v));
    }
    inline void appendVlq(std::vector<std::uint8_t>& out, std::uint32_t v)
    {
        std::uint8_t bytes[5];
        int n = 0;
        do { bytes[n++] = static_cast<std::uint8_t>(v & 0x7f); v >>= 7; } while (v && n < 5);
        for (int i = n - 1; i >= 0; --i)
            out.push_back(static_cast<std::uint8_t>(bytes[i] | (i ? 0x80 : 0)));
    }
    struct Event
    {
        Event(std::uint32_t t, int o, std::uint8_t st, std::uint8_t d1, std::uint8_t d2,
              bool isMeta = false, std::vector<std::uint8_t> mb = {})
            : tick(t), order(o), status(st), data1(d1), data2(d2), meta(isMeta), metaBytes(std::move(mb)) {}
        std::uint32_t tick;
        int order;                    // at equal ticks: note-offs, then controllers, then note-ons
        std::uint8_t status, data1, data2;
        bool meta;
        std::vector<std::uint8_t> metaBytes;
    };
}

// Bend 64 is centre; each unit is 128 wheel steps (+-2 semitones over 0..127).
inline int phraseBendToWheel(int bend) noexcept
{
    return std::clamp(8192 + (std::clamp(bend, 0, 127) - 64) * 128, 0, 16383);
}

inline std::vector<std::uint8_t> buildPhraseMidiFile(const PhraseEngine& phrase, int bpm)
{
    using midi_detail::Event;
    constexpr std::uint32_t ticksPerQuarter = 480;
    constexpr std::uint32_t ticksPerStep = ticksPerQuarter / 2;
    constexpr std::uint8_t channel = 0; // MIDI channel 1

    std::vector<Event> events;
    std::uint32_t lastTick = PhraseEngine::steps * ticksPerStep;

    // Tempo: microseconds per quarter note.
    const int safeBpm = std::clamp(bpm, 30, 300);
    const auto micros = static_cast<std::uint32_t>(std::lround(60000000.0 / safeBpm));
    events.emplace_back(0, 0, 0, 0, 0, true, std::vector<std::uint8_t> {0x51, 0x03,
                        static_cast<std::uint8_t>(micros >> 16),
                        static_cast<std::uint8_t>(micros >> 8),
                        static_cast<std::uint8_t>(micros)});

    for (int step = 0; step < PhraseEngine::steps; ++step)
    {
        const auto n = phrase.note(step);
        const std::uint32_t tick = static_cast<std::uint32_t>(step) * ticksPerStep;
        // Expressive curves are written at every step: the engine interpolates them.
        const int wheel = phraseBendToWheel(n.bend);
        events.push_back({tick, 1, static_cast<std::uint8_t>(0xB0 | channel), 2,
                          static_cast<std::uint8_t>(std::clamp(n.breath, 0, 127))});
        events.push_back({tick, 1, static_cast<std::uint8_t>(0xB0 | channel), 11,
                          static_cast<std::uint8_t>(std::clamp(n.expression, 0, 127))});
        events.push_back({tick, 1, static_cast<std::uint8_t>(0xB0 | channel), 1,
                          static_cast<std::uint8_t>(std::clamp(n.vibrato, 0, 127))});
        events.push_back({tick, 1, static_cast<std::uint8_t>(0xE0 | channel),
                          static_cast<std::uint8_t>(wheel & 0x7f),
                          static_cast<std::uint8_t>((wheel >> 7) & 0x7f)});
        if (!n.active) continue;

        const std::uint32_t gateTicks = static_cast<std::uint32_t>(std::clamp(n.gate, 1, 8)) * ticksPerStep;
        const std::uint32_t off = tick + gateTicks;
        lastTick = std::max(lastTick, off);
        events.push_back({tick, 2, static_cast<std::uint8_t>(0x90 | channel),
                          static_cast<std::uint8_t>(std::clamp(n.pitch, 0, 127)),
                          static_cast<std::uint8_t>(std::clamp(n.velocity, 1, 127))});
        events.push_back({off, 0, static_cast<std::uint8_t>(0x80 | channel),
                          static_cast<std::uint8_t>(std::clamp(n.pitch, 0, 127)), 0});
    }

    std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
        return a.tick != b.tick ? a.tick < b.tick : a.order < b.order;
    });

    std::vector<std::uint8_t> track;
    std::uint32_t previous = 0;
    for (const auto& e : events)
    {
        midi_detail::appendVlq(track, e.tick - previous);
        previous = e.tick;
        if (e.meta)
        {
            track.push_back(0xFF);
            track.insert(track.end(), e.metaBytes.begin(), e.metaBytes.end());
        }
        else
        {
            track.push_back(e.status);
            track.push_back(e.data1);
            if (e.status < 0xC0 || e.status >= 0xE0) track.push_back(e.data2);
        }
    }
    // End of track at the end of the phrase, or after the last note-off.
    midi_detail::appendVlq(track, lastTick - previous);
    track.push_back(0xFF);
    track.push_back(0x2F);
    track.push_back(0x00);

    std::vector<std::uint8_t> file;
    file.insert(file.end(), {'M', 'T', 'h', 'd'});
    midi_detail::appendBE32(file, 6);
    file.push_back(0); file.push_back(0);   // format 0
    file.push_back(0); file.push_back(1);   // one track
    file.push_back(static_cast<std::uint8_t>(ticksPerQuarter >> 8));
    file.push_back(static_cast<std::uint8_t>(ticksPerQuarter));
    file.insert(file.end(), {'M', 'T', 'r', 'k'});
    midi_detail::appendBE32(file, static_cast<std::uint32_t>(track.size()));
    file.insert(file.end(), track.begin(), track.end());
    return file;
}
}
