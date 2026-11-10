#pragma once
// Real-time-safe breath-responsive spectral shaping for recorded wind samples.
// This is expressive filtering, not a substitute for recorded dynamic layers.
#include <algorithm>
#include <cmath>

namespace windlab
{
class AcousticExpression final
{
public:
    void prepare(double newSampleRate) noexcept
    {
        const double rate = std::clamp(newSampleRate, 8000.0, 192000.0);
        constexpr double twoPi = 6.28318530717958647692;
        softAlpha = static_cast<float>(1.0 - std::exp(-twoPi * 1400.0 / rate));
        strongAlpha = static_cast<float>(1.0 - std::exp(-twoPi * 16000.0 / rate));
        smoothing = static_cast<float>(1.0 - std::exp(-1.0 / (rate * .012)));
        reset();
    }

    void reset(float breath = 1.0f, float brightness = .55f) noexcept
    {
        state = 0.0f;
        alpha = targetAlpha(breath, brightness);
    }

    float process(float sample, float breath, float brightness) noexcept
    {
        // No trig, allocation, locks or state shared between voices on the audio thread.
        const float target = targetAlpha(breath, brightness);
        alpha += smoothing * (target - alpha);
        state += alpha * (sample - state);
        return state;
    }

private:
    float targetAlpha(float breath, float brightness) const noexcept
    {
        const float energy = std::clamp(breath, 0.0f, 1.0f);
        const float tone = std::clamp(brightness, 0.0f, 1.0f);
        const float colour = std::clamp(.18f + .58f * energy + .24f * tone, 0.0f, 1.0f);
        return softAlpha + (strongAlpha - softAlpha) * colour * colour;
    }
    float softAlpha = .2f, strongAlpha = .85f, smoothing = .001f;
    float alpha = .8f, state = 0.0f;
};
}
