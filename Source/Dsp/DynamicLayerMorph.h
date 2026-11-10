#pragma once
// Continuous dynamic layer blending for genuine paired acoustic recordings.
// All state belongs to an individual voice and the audio callback performs
// no allocation, I/O or transcendental math. The sampled attacks remain intact.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace windlab
{
class DynamicLayerMorph final
{
public:
    void prepare(double newSampleRate) noexcept
    {
        const double rate = std::clamp(newSampleRate, 8000.0, 192000.0);
        smoothing = static_cast<float>(1.0 - std::exp(-1.0 / (rate * .075)));
        attackHoldFrames = static_cast<std::uint32_t>(rate * .045);
    }

    // Initially play only the sample selected by velocity to retain its attack.
    void reset(bool primaryIsHigh) noexcept
    {
        primaryHigh = primaryIsHigh;
        highMix = primaryHigh ? 1.0f : 0.0f;
        frames = 0;
    }

    // The actual MIDI note velocity selects the attack. Continuous CC2/mic breath
    // changes the recorded performance layer only after its first 45 ms.
    // If no explicit breath data was received, leave legacy velocity selection intact.
    float primaryWeight(float intensity, bool liveBreath) noexcept
    {
        const float initial = primaryHigh ? 1.0f : 0.0f;
        const float target = liveBreath && frames >= attackHoldFrames
                           ? highLayerWeight(intensity) : initial;
        if (frames < attackHoldFrames) ++frames;
        highMix += smoothing * (target - highMix);
        highMix = std::clamp(highMix, 0.0f, 1.0f);
        return primaryHigh ? highMix : 1.0f - highMix;
    }

    static float highLayerWeight(float intensity) noexcept
    {
        const float x = std::clamp((intensity - .20f) / .65f, 0.0f, 1.0f);
        return x * x * (3.0f - 2.0f * x);
    }

private:
    float smoothing = .0003f;
    float highMix = 1.0f;
    std::uint32_t attackHoldFrames = 1984, frames = 0;
    bool primaryHigh = true;
};
}
