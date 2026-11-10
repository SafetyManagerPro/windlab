// Verify phase matching uses real samples offline, not on the audio thread.
#include "Dsp/SamplePhaseAlignment.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace
{
constexpr double twoPi = 6.2831853071795864769;
std::vector<float> makeTake(double rate, double pitch, double phase, double gain)
{
    std::vector<float> audio(static_cast<std::size_t>(rate * .42));
    for (std::size_t i = 0; i < audio.size(); ++i)
    {
        const double t = static_cast<double>(i) / rate;
        const double angle = twoPi * pitch * t + phase;
        const double breath = 1.0 + .025 * std::sin(twoPi * 1.1 * t);
        audio[i] = static_cast<float>(gain * breath *
                                     (std::sin(angle) + .23 * std::sin(2.0 * angle + .16)));
    }
    return audio;
}
}

int main()
{
    using namespace windlab;
    for (double primaryRate : {44100.0, 48000.0, 96000.0})
    {
        const double companionRate = primaryRate == 44100.0 ? 48000.0 : primaryRate;
        constexpr double rootHz = 227.0;
        const auto primary = makeTake(primaryRate, rootHz, 0.0, .58);
        const auto loud = makeTake(companionRate, rootHz, 1.35, .92);
        const float before = samplePairCorrelation(
            primary.data(), static_cast<int>(primary.size()), primaryRate,
            loud.data(), static_cast<int>(loud.size()), companionRate, 0.0);
        const auto match = estimateSamplePhaseAlignment(
            primary.data(), static_cast<int>(primary.size()), primaryRate,
            loud.data(), static_cast<int>(loud.size()), companionRate, rootHz);
        const float after = samplePairCorrelation(
            primary.data(), static_cast<int>(primary.size()), primaryRate,
            loud.data(), static_cast<int>(loud.size()), companionRate,
            match.companionAdvance);
        assert(match.companionAdvance > 0.0);
        assert(match.companionAdvance < companionRate / rootHz);
        assert(after > before + .4f);
        assert(after > .94f);
        assert(std::abs(after - match.correlation) < 1.0e-4f);

        // Already phase-aligned takes must not be shifted unnecessarily.
        const auto same = makeTake(companionRate, rootHz, 0.0, 1.15);
        const auto aligned = estimateSamplePhaseAlignment(
            primary.data(), static_cast<int>(primary.size()), primaryRate,
            same.data(), static_cast<int>(same.size()), companionRate, rootHz);
        assert(aligned.companionAdvance == 0.0);
        assert(aligned.correlation > .99f);

        // Silence and invalid buffers never trigger speculative alignment.
        std::vector<float> silence(loud.size(), 0.0f);
        const auto quiet = estimateSamplePhaseAlignment(
            primary.data(), static_cast<int>(primary.size()), primaryRate,
            silence.data(), static_cast<int>(silence.size()), companionRate, rootHz);
        assert(quiet.companionAdvance == 0.0);

        // Unrelated/noisy takes must never be force-aligned.
        std::vector<float> uncorrelated(loud.size());
        std::uint32_t seed = 0x24f06a5du;
        for (auto& value : uncorrelated)
        {
            seed = seed * 1664525u + 1013904223u;
            value = (static_cast<float>((seed >> 8) & 0xffffffu) / 8388608.0f - 1.0f) * .25f;
        }
        const auto unrelated = estimateSamplePhaseAlignment(
            primary.data(), static_cast<int>(primary.size()), primaryRate,
            uncorrelated.data(), static_cast<int>(uncorrelated.size()), companionRate, rootHz);
        assert(unrelated.companionAdvance == 0.0);

        assert(estimateSamplePhaseAlignment(nullptr, 0, 0,
            loud.data(), static_cast<int>(loud.size()), companionRate, rootHz)
            .companionAdvance == 0.0);
        assert(estimateSamplePhaseAlignment(primary.data(), static_cast<int>(primary.size()),
            primaryRate, loud.data(), static_cast<int>(loud.size()), companionRate, 0.0)
            .companionAdvance == 0.0);
    }
    std::cout << "WindLab sample phase alignment: PASS\n";
}
