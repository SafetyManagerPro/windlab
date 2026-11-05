#pragma once
// Host-independent note-on velocity response curves for physical MIDI keyboards.
// The original MIDI velocity is preserved for standard mode; no allocations.
#include <algorithm>
#include <cmath>

namespace windlab
{
    enum class VelocityResponse : int
    {
        Natural = 0,
        LightTouch = 1,
        WeightedPiano = 2,
        BreathController = 3
    };

    inline float applyVelocityResponse(float velocity, int mode) noexcept
    {
        const float v = std::clamp(velocity, 0.0f, 1.0f);
        switch (static_cast<VelocityResponse>(mode))
        {
            case VelocityResponse::LightTouch:
                return std::sqrt(v); // softer touches can reach a louder recorded layer
            case VelocityResponse::WeightedPiano:
                return std::pow(v, 1.35f); // more resolution at quieter key strikes
            case VelocityResponse::BreathController:
                return v <= 0.0f ? 0.0f : .72f + .28f * v;
                // Establishes an audible sample layer; CC2 shapes the held note.
            case VelocityResponse::Natural:
            default:
                return v;
        }
    }
}
