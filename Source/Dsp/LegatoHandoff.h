#pragma once
// WindLab 0.9.12: allocation-free overlap envelope for recorded-note legato.
// The outgoing voice fades while this new recorded voice enters its sustained
// body. This does NOT claim authentic recorded interval transition samples.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace windlab
{
class LegatoHandoff final
{
public:
    void prepare(double newSampleRate) noexcept
    {
        sampleRate = std::clamp(newSampleRate, 8000.0, 192000.0);
        reset();
    }

    static float durationForSemitones(int interval) noexcept
    {
        // Keep small fingering changes quick and larger slurs a little slower.
        // Upper bound is below a tenth of a second to avoid dragging melodies.
        return .032f + .0035f * static_cast<float>(std::clamp(std::abs(interval), 0, 12));
    }

    void start(bool isLegato, float seconds) noexcept
    {
        if (!isLegato)
        {
            reset();
            return;
        }
        const float safeSeconds = std::clamp(seconds, .02f, .09f);
        length = std::max<std::uint32_t>(
            1u, static_cast<std::uint32_t>(sampleRate * safeSeconds));
        position = 0;
        enabled = true;
    }

    float nextGain() noexcept
    {
        if (!enabled) return 1.0f;
        const float t = static_cast<float>(++position) / static_cast<float>(length);
        if (position >= length)
        {
            enabled = false;
            return 1.0f;
        }
        // Monotonic smoothstep, with zero slope at both ends. The previous
        // voice uses WindEngine::fastRelease over the matching transition time.
        return t * t * (3.0f - 2.0f * t);
    }

    bool isActive() const noexcept { return enabled; }

    void reset() noexcept
    {
        enabled = false;
        length = 1u;
        position = 0u;
    }

private:
    double sampleRate = 44100.0;
    std::uint32_t length = 1u, position = 0u;
    bool enabled = false;
};
}
