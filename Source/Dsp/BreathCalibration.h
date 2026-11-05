#pragma once
#include <algorithm>
#include <cmath>

namespace windlab
{
// Stateless, real-time safe MIDI breath calibration.
// Defaults (threshold=0, curve=1) preserve original CC2 behaviour.
inline float calibrateBreath(float input, float threshold, float curve) noexcept
{
    input = std::clamp(input, 0.0f, 1.0f);
    threshold = std::clamp(threshold, 0.0f, 0.95f);
    curve = std::clamp(curve, 0.35f, 3.0f);
    const float normalized = std::clamp((input - threshold) / (1.0f - threshold), 0.0f, 1.0f);
    return std::pow(normalized, curve);
}
}
