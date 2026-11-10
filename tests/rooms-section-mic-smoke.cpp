// WindLab 0.9.8: microphone breath follower, section mode and generated reverb rooms.
#include "Dsp/MicBreath.h"
#include "Dsp/RoomImpulse.h"
#include "Dsp/SectionMode.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace windlab;

namespace
{
double energy(const std::vector<float>& v, std::size_t from, std::size_t to)
{
    double e = 0.0;
    for (std::size_t i = from; i < std::min(to, v.size()); ++i) e += static_cast<double>(v[i]) * v[i];
    return e;
}
}

int main()
{
    // --- Microphone breath.
    {
        MicBreathFollower mic;
        mic.prepare(48000.0);
        mic.setSensitivity(.5f);
        std::vector<float> block(480, 0.0f);
        // Silence and a quiet low hum stay at zero breath.
        assert(mic.process(block.data(), 480) == 0.0f);
        for (int i = 0; i < 480; ++i) block[static_cast<std::size_t>(i)] = .01f * std::sin(2.0 * 3.14159265 * 50.0 * i / 48000.0);
        float hum = 0.0f;
        for (int b = 0; b < 20; ++b) hum = mic.process(block.data(), 480);
        assert(hum < .05f);
        // Blowing (broadband noise) gives breath; stronger blowing gives more.
        std::uint32_t seed = 1;
        auto blow = [&](float amp)
        {
            float breath = 0.0f;
            for (int b = 0; b < 30; ++b)
            {
                for (auto& s : block)
                {
                    seed = seed * 1664525u + 1013904223u;
                    s = amp * (static_cast<float>((seed >> 8) & 0xffffffu) / 8388607.5f - 1.0f);
                }
                breath = mic.process(block.data(), 480);
            }
            return breath;
        };
        const float soft = blow(.05f), strong = blow(.25f);
        assert(soft > .05f && strong > soft && strong <= 1.0f);
        // Stopping: the breath falls back within a few hundred ms.
        std::fill(block.begin(), block.end(), 0.0f);
        float after = 1.0f;
        for (int b = 0; b < 40; ++b) after = mic.process(block.data(), 480);
        assert(after == 0.0f);
        // More sensitivity = more breath for the same blowing.
        mic.setSensitivity(.9f);
        assert(blow(.05f) > soft);
    }

    // --- Section mode: register decides the player, selected instrument when off.
    assert(sectionInstrumentFor(SectionMode::Off, 40, 3) == 3);
    assert(sectionInstrumentFor(SectionMode::Saxes, 45, 0) == 8);
    assert(sectionInstrumentFor(SectionMode::Saxes, 55, 0) == 1);
    assert(sectionInstrumentFor(SectionMode::Saxes, 65, 0) == 7);
    assert(sectionInstrumentFor(SectionMode::Saxes, 75, 0) == 6);
    assert(sectionInstrumentFor(SectionMode::Brass, 50, 7) == 2);
    assert(sectionInstrumentFor(SectionMode::Brass, 60, 7) == 5);
    assert(sectionInstrumentFor(SectionMode::Brass, 70, 7) == 0);
    assert(sectionInstrumentFor(SectionMode::Woodwinds, 60, 0) == 4);
    assert(sectionInstrumentFor(SectionMode::Woodwinds, 80, 0) == 3);
    assert(sectionInstrumentFor(SectionMode::BigBand, 74, 3) == 0);
    assert(sectionPanFor(SectionMode::Off, 7) == 0.0f);
    assert(sectionPanFor(SectionMode::Saxes, 6) < 0.0f && sectionPanFor(SectionMode::Saxes, 8) > 0.0f);

    // --- Rooms: none for the algorithmic reverb; longer and darker for big rooms.
    std::vector<float> l, r;
    generateRoomImpulse(RoomType::Algorithmic, 48000.0, .5f, .5f, l, r);
    assert(l.empty() && r.empty());
    generateRoomImpulse(RoomType::Studio, 48000.0, .5f, .5f, l, r);
    const auto studioLength = l.size();
    assert(studioLength > 10000 && l.size() == r.size());
    for (float v : l) assert(std::isfinite(v));
    // The tail decays: the first half holds much more energy than the second.
    assert(energy(l, 0, studioLength / 2) > 20.0 * energy(l, studioLength / 2, studioLength));
    assert(std::abs(l.back()) < 1e-4f);
    generateRoomImpulse(RoomType::Church, 48000.0, .5f, .5f, l, r);
    assert(l.size() > studioLength * 5);
    // REVERB SIZE stretches the room.
    std::vector<float> small, smallR, big, bigR;
    generateRoomImpulse(RoomType::ConcertHall, 48000.0, 0.0f, .5f, small, smallR);
    generateRoomImpulse(RoomType::ConcertHall, 48000.0, 1.0f, .5f, big, bigR);
    assert(big.size() > small.size() * 2);
    // Stereo: left and right are different (decorrelated).
    assert(energy(l, 0, l.size()) > 0.0);
    double diff = 0.0;
    for (std::size_t i = 0; i < l.size(); i += 97) diff += std::abs(l[i] - r[i]);
    assert(diff > 1.0);

    std::cout << "WindLab 0.9.8 mic breath, section mode, convolution rooms: PASS\n";
    return 0;
}
