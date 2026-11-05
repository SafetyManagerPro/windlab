#include "Dsp/PhraseEngine.h"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

struct Event
{
    windlab::PhraseEvent type;
    int note, value, offset;
};

static bool check(bool condition, const char* reason)
{
    if (!condition) std::cerr << "Phrase test failed: " << reason << '\n';
    return condition;
}

int main()
{
    windlab::PhraseEngine phrase;
    windlab::PhraseNote n;
    n.pitch = 64;
    n.velocity = 91;
    n.breath = 35;
    n.expression = 42;
    n.bend = 78;
    n.gate = 3;
    n.active = true;
    n.legato = true;
    auto unpacked = windlab::unpackPhraseNote(windlab::packPhraseNote(n));
    if (!check(unpacked.pitch == 64 && unpacked.velocity == 91 &&
               unpacked.breath == 35 && unpacked.expression == 42 && unpacked.bend == 78 &&
               unpacked.gate == 3 && unpacked.active && unpacked.legato,
               "packed note must roundtrip")) return 1;
    phrase.clear();
    phrase.setTempo(120);
    n.gate = 2;
    n.pitch = 60;
    n.legato = false;
    phrase.setNote(0, n);
    auto b = n;
    b.pitch = 67; b.breath = 110; b.expression = 112; b.bend = 58; b.gate = 2; b.legato = true;
    phrase.setNote(2, b);
    // Independent expression point between sounding notes.
    auto point = phrase.note(1);
    point.breath = 105;
    point.expression = 100;
    point.bend = 42;
    phrase.setNote(1, point);

    std::vector<Event> events;
    const auto emit = [&](windlab::PhraseEvent t, int p, int v, int offset)
    { events.push_back({t, p, v, offset}); };
    phrase.setPlaying(true);
    phrase.process(33000, 48000.0, emit); // into third 1/8-step note
    int on60 = 0, on67 = 0, off60 = 0, breathEvents = 0, expressionEvents = 0, pitchEvents = 0;
    bool sawInterpolatedBreath = false, sawInterpolatedVolume = false;
    for (const auto& e : events)
    {
        if (!check(e.offset >= 0 && e.offset < 33000, "event must be inside buffer")) return 2;
        if (e.type == windlab::PhraseEvent::NoteOn)
        {
            if (e.note == 60 && e.value == 91) ++on60;
            if (e.note == 67) ++on67;
        }
        if (e.type == windlab::PhraseEvent::NoteOff && e.note == 60) ++off60;
        if (e.type == windlab::PhraseEvent::Breath)
        {
            ++breathEvents;
            sawInterpolatedBreath |= (e.value > 35 && e.value < 105);
        }
        if (e.type == windlab::PhraseEvent::Expression)
        {
            ++expressionEvents;
            sawInterpolatedVolume |= (e.value > 42 && e.value < 100);
        }
        if (e.type == windlab::PhraseEvent::PitchWheel) ++pitchEvents;
    }
    if (!check(on60 == 1 && on67 == 1, "real MIDI notes must start on their steps") ||
        !check(off60 == 1, "notes must end") ||
        !check(breathEvents >= 30 && expressionEvents >= 30 && pitchEvents >= 30,
               "all controller lanes must emit continuously") ||
        !check(sawInterpolatedBreath, "CC2 breath must interpolate between points") ||
        !check(sawInterpolatedVolume, "CC11 volume must interpolate between points")) return 3;

    phrase.setPlaying(false);
    events.clear();
    phrase.process(128, 48000.0, emit);
    if (!check(std::any_of(events.begin(), events.end(), [](const Event& e)
        { return e.type == windlab::PhraseEvent::NoteOff && e.note == 67; }),
        "stopping a playing phrase must release sustained voices")) return 4;
    phrase.setTempo(2000);
    if (!check(phrase.getTempo() == 220, "tempo must clamp")) return 5;
    phrase.loadDemo();
    int count = 0;
    for (int i = 0; i < 16; ++i)
        count += phrase.note(i).active ? 1 : 0;
    if (!check(count == 8, "demo should create eight playable notes")) return 6;
    phrase.clear();
    for (int i = 0; i < 16; ++i)
        if (!check(!phrase.note(i).active, "clear should remove every note")) return 7;

    std::cout << "WindLab 0.7: PASS packed notes, expression curves, MIDI playback, stop and state.\n";
}
