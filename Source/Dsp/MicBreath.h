#pragma once
// 0.9.8 breath from a microphone: blow (or play) into the mic and the level becomes
// breath (0..1). JUCE-free, allocation-free, audio-thread safe.
//
// Blowing noise is broadband, voice and room hum are mostly low: a one-pole
// high-pass removes rumble, then a fast-attack / slower-release envelope follower
// and a noise gate turn the signal into a steady breath value.
#include <algorithm>
#include <cmath>

namespace windlab
{
class MicBreathFollower
{
public:
    void prepare(double sampleRate) noexcept
    {
        rate = std::max(8000.0, sampleRate);
        highpassCoeff = static_cast<float>(std::exp(-2.0 * 3.14159265358979 * 180.0 / rate));
        attackCoeff = static_cast<float>(std::exp(-1.0 / (rate * .008)));
        releaseCoeff = static_cast<float>(std::exp(-1.0 / (rate * .090)));
        reset();
    }
    void reset() noexcept { envelope = previousIn = previousOut = 0.0f; }

    // sensitivity 0..1: 0.5 = normal blowing at ~20 cm, 1 = very sensitive.
    void setSensitivity(float value) noexcept { sensitivity = std::clamp(value, 0.0f, 1.0f); }

    // Process one block of mono microphone samples; returns breath 0..1.
    float process(const float* samples, int count) noexcept
    {
        for (int i = 0; i < count; ++i)
        {
            const float x = samples[i];
            const float hp = highpassCoeff * (previousOut + x - previousIn);
            previousIn = x;
            previousOut = hp;
            const float level = std::abs(hp);
            const float coeff = level > envelope ? attackCoeff : releaseCoeff;
            envelope = level + coeff * (envelope - level);
        }
        if (!std::isfinite(envelope)) envelope = 0.0f;
        // Gate below the room noise floor, then map the useful range to 0..1.
        const float gain = 4.0f + 60.0f * sensitivity * sensitivity;
        const float gate = .004f + .02f * (1.0f - sensitivity);
        const float shaped = std::max(0.0f, envelope - gate) * gain;
        return std::clamp(shaped, 0.0f, 1.0f);
    }

private:
    double rate = 48000.0;
    float highpassCoeff = .97f, attackCoeff = .99f, releaseCoeff = .999f;
    float envelope = 0.0f, previousIn = 0.0f, previousOut = 0.0f, sensitivity = .5f;
};
}
