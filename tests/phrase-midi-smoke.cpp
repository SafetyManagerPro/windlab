#include "Dsp/PhraseEngine.h"
#include "Dsp/PhraseMidi.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {
struct Ev { std::uint32_t tick; std::uint8_t status, d1, d2; bool meta; };

std::uint32_t readVlq(const std::vector<std::uint8_t>& b, std::size_t& i) {
    std::uint32_t v = 0; std::uint8_t c;
    do { c = b[i++]; v = (v << 7) | (c & 0x7f); } while (c & 0x80);
    return v;
}
std::uint32_t be32(const std::vector<std::uint8_t>& b, std::size_t i) {
    return (std::uint32_t(b[i]) << 24) | (std::uint32_t(b[i+1]) << 16) |
           (std::uint32_t(b[i+2]) << 8) | b[i+3];
}

// Minimal decoder: header checks plus track events with absolute ticks.
std::vector<Ev> decode(const std::vector<std::uint8_t>& f, std::uint32_t& division, std::uint32_t& micros) {
    assert(std::string(f.begin(), f.begin() + 4) == "MThd");
    assert(be32(f, 4) == 6);
    assert(f[8] == 0 && f[9] == 0 && f[10] == 0 && f[11] == 1);  // format 0, one track
    division = (std::uint32_t(f[12]) << 8) | f[13];
    assert(std::string(f.begin() + 14, f.begin() + 18) == "MTrk");
    const std::uint32_t len = be32(f, 18);
    assert(22 + len == f.size());
    std::vector<Ev> out; std::uint32_t tick = 0; micros = 0;
    std::size_t i = 22; bool ended = false;
    while (i < f.size()) {
        tick += readVlq(f, i);
        if (f[i] == 0xFF) {
            const std::uint8_t type = f[i + 1];
            const std::size_t ml = f[i + 2];
            if (type == 0x51) micros = (std::uint32_t(f[i+3]) << 16) | (std::uint32_t(f[i+4]) << 8) | f[i+5];
            if (type == 0x2F) ended = true;
            out.push_back({tick, 0xFF, type, 0, true});
            i += 3 + ml;
        } else {
            const std::uint8_t st = f[i++];
            const std::uint8_t d1 = f[i++];
            std::uint8_t d2 = 0;
            if (st < 0xC0 || st >= 0xE0) d2 = f[i++];
            out.push_back({tick, st, d1, d2, false});
        }
    }
    assert(ended);
    return out;
}
bool has(const std::vector<Ev>& ev, std::uint32_t tick, std::uint8_t st, std::uint8_t d1, std::uint8_t d2) {
    for (const auto& e : ev) if (!e.meta && e.tick == tick && e.status == st && e.d1 == d1 && e.d2 == d2) return true;
    return false;
}
}

int main() {
    windlab::PhraseEngine phrase;
    windlab::PhraseNote a; a.active = true; a.pitch = 60; a.velocity = 100; a.gate = 2;
    a.breath = 90; a.expression = 80; a.vibrato = 30; a.bend = 64;
    phrase.setNote(0, a);
    windlab::PhraseNote b; b.active = true; b.pitch = 67; b.velocity = 90; b.gate = 1;
    b.bend = 64 + 64;   // +2 semitones at the top of the bend range
    phrase.setNote(3, b);

    const auto file = windlab::buildPhraseMidiFile(phrase, 120);
    std::uint32_t division = 0, micros = 0;
    const auto ev = decode(file, division, micros);

    assert(division == 480);
    assert(micros == 500000);                             // 120 BPM
    assert(has(ev, 0, 0x90, 60, 100));                    // note 60 at step 0
    assert(has(ev, 480, 0x80, 60, 0));                    // gate 2 eighths = 480 ticks
    assert(has(ev, 720, 0x90, 67, 90));                   // step 3 = 720 ticks
    assert(has(ev, 960, 0x80, 67, 0));                    // gate 1 eighth = 240 ticks
    assert(has(ev, 0, 0xB0, 2, 90));                      // breath curve
    assert(has(ev, 0, 0xB0, 11, 80));                     // expression
    assert(has(ev, 0, 0xB0, 1, 30));                      // vibrato
    assert(has(ev, 0, 0xE0, 0, 64));                      // bend centre: 8192 = lsb 0, msb 64
    assert(has(ev, 720, 0xE0, 0, 127));                   // top of bend range: 16256 = lsb 0, msb 127
    // Silent steps carry no note-on.
    for (const auto& e : ev) assert(e.meta || !(e.status == 0x90 && e.tick != 0 && e.tick != 720));

    // Empty phrase still produces a valid file ending at the phrase length.
    windlab::PhraseEngine empty;
    std::uint32_t d2 = 0, m2 = 0;
    const auto ev2 = decode(windlab::buildPhraseMidiFile(empty, 90), d2, m2);
    assert(m2 == 666667);                                 // 90 BPM
    assert(ev2.back().meta && ev2.back().d1 == 0x2F && ev2.back().tick == 3840);

    std::cout << "WindLab 0.9.5 MIDI export: PASS (SMF header, tempo, notes, gates, curves, bend)\n";
}
