// Sustain loop seam matching: real-time independent deterministic smoke checks.
#include "Dsp/SampleLoopPlan.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

int main()
{
    constexpr double pi2 = 6.2831853071795864769;
    for (const double rate : {44100.0, 48000.0, 96000.0})
    {
        std::vector<float> audio(static_cast<std::size_t>(rate * 2.0));
        // Quasi-periodic acoustic-like sustain: mild overtone, slow tremolo
        // and phase drift to make the nearest-zero-crossing seam suboptimal.
        for (std::size_t i = 0; i < audio.size(); ++i)
        {
            const double t = static_cast<double>(i) / rate;
            const double angle = pi2 * (197.0 * t + 1.35 * t * t);
            const double amp = .42 * (1.0 + .045 * std::sin(pi2 * .72 * t));
            audio[i] = static_cast<float>(amp * (std::sin(angle) +
                               .21 * std::sin(2.0 * angle + .3) +
                               .12 * std::sin(3.0 * angle - .24) +
                               .45 * std::sin(pi2 * 210.0 * t + .12)));
        }
        const int count = static_cast<int>(audio.size());
        const auto plan = windlab::chooseSampleLoop(audio.data(), count, rate);
        assert(plan.valid);
        assert(plan.start >= 1 && plan.end < count - 2);
        assert(plan.end > plan.start + 3 * plan.fade);
        assert(audio[static_cast<std::size_t>(plan.end - 1)] <= 0.0f);
        assert(audio[static_cast<std::size_t>(plan.end)] > 0.0f);
        const int radius = static_cast<int>(rate * .012);
        const int oldEnd = windlab::nearestPositiveZeroCrossing(
            audio.data(), count, static_cast<int>(count * .75), radius);
        const auto originalMismatch = windlab::loopFadeMismatch(
            audio.data(), count, plan.start, oldEnd, plan.fade);
        const auto newMismatch = windlab::loopFadeMismatch(
            audio.data(), count, plan.start, plan.end, plan.fade);
        // When unchanged, identical score. When changed, noticeably less
        // waveform discrepancy even accounting for distance penalty.
        assert(std::isfinite(newMismatch));
        assert(newMismatch < originalMismatch * .80f);
        assert(std::abs(plan.end - oldEnd) <= radius + 5);

        const auto repeated = windlab::chooseSampleLoop(audio.data(), count, rate);
        assert(repeated.valid && repeated.start == plan.start &&
               repeated.end == plan.end && repeated.fade == plan.fade);

        // Edge cases remain a safe invalid plan.
        assert(!windlab::chooseSampleLoop(nullptr, count, rate).valid);
        assert(!windlab::chooseSampleLoop(audio.data(), 2000, rate).valid);
        assert(!windlab::chooseSampleLoop(audio.data(), count, 0.0).valid);
    }
    std::cout << "WindLab sustained sample loop matching: PASS\n";
    return 0;
}
