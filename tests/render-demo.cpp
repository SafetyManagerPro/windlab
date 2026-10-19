#include "Dsp/WindEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

namespace
{
    void write16(std::ofstream& f, std::uint16_t value)
    {
        for (int i = 0; i < 2; ++i) f.put(static_cast<char>((value >> (i * 8)) & 255));
    }
    void write32(std::ofstream& f, std::uint32_t value)
    {
        for (int i = 0; i < 4; ++i) f.put(static_cast<char>((value >> (i * 8)) & 255));
    }
}

int main(int argc, char** argv)
{
    if (argc != 2) { std::cerr << "Usage: render-demo <file.wav>\n"; return 2; }
    constexpr int rate = 48000;
    std::vector<float> audio;
    audio.reserve(rate * 10);
    for (int inst = 0; inst < 6; ++inst)
    {
        windlab::WindEngine voice;
        voice.prepare(rate);
        voice.setTone(.73f);
        voice.setAir(.35f);
        voice.setModWheel(.25f);
        voice.start(60 + inst, .95f, static_cast<windlab::Instrument>(inst), .03f, .17f);
        for (int i = 0; i < rate * 3 / 4; ++i) audio.push_back(voice.render());
        voice.noteOff();
        for (int i = 0; i < rate / 4; ++i) audio.push_back(voice.render());
        for (int i = 0; i < rate / 10; ++i) audio.push_back(0.0f);
    }
    std::ofstream f(argv[1], std::ios::binary);
    if (!f) return 3;
    const auto dataSize = static_cast<std::uint32_t>(audio.size() * 2);
    f.write("RIFF", 4); write32(f, 36 + dataSize); f.write("WAVEfmt ", 8);
    write32(f, 16); write16(f, 1); write16(f, 1); write32(f, rate);
    write32(f, rate * 2); write16(f, 2); write16(f, 16);
    f.write("data", 4); write32(f, dataSize);
    for (float sample : audio)
    {
        const auto v = static_cast<std::int16_t>(std::lround(std::clamp(sample * 2.0f, -1.0f, 1.0f) * 32767.f));
        write16(f, static_cast<std::uint16_t>(v));
    }
    std::cout << "Wrote preview of 6 SYNTHETIC sounds to " << argv[1] << "\n";
    return f.good() ? 0 : 4;
}
