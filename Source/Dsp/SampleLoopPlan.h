#pragma once
// WindLab v0.9: deterministic, read-only loop-point analysis at bank load time.
// This code has no JUCE dependency and does not execute in the audio callback.
#include <algorithm>
#include <cmath>

namespace windlab
{
struct SampleLoopPlan
{
    int start = 0;
    int end = 0;
    int fade = 0;
    bool valid = false;
};

inline int nearestPositiveZeroCrossing(const float* audio, int count,
                                       int preferred, int radius) noexcept
{
    if (audio == nullptr || count < 4) return std::max(0, preferred);
    const int low = std::clamp(preferred - radius, 1, count - 3);
    const int high = std::clamp(preferred + radius, 1, count - 3);
    int best = std::clamp(preferred, 1, count - 3);
    float bestScore = 1.0e9f;
    for (int i = low; i <= high; ++i)
    {
        if (audio[i - 1] <= 0.0f && audio[i] > 0.0f)
        {
            // Prefer a small, ascending zero crossing near the original zone.
            const float magnitude = std::abs(audio[i - 1]) + std::abs(audio[i]);
            const float displacement = static_cast<float>(std::abs(i - preferred)) /
                                       static_cast<float>(std::max(1, radius));
            const float score = magnitude + .008f * displacement;
            if (score < bestScore)
            {
                bestScore = score;
                best = i;
            }
        }
    }
    return best;
}

inline SampleLoopPlan chooseSampleLoop(const float* audio, int count, double sampleRate) noexcept
{
    SampleLoopPlan loop;
    if (audio == nullptr || count < 4000 || !std::isfinite(sampleRate) || sampleRate <= 0.0)
        return loop;
    // Preserve the same conservative sustain region from v0.5/v0.8;
    // only align the boundaries to nearby natural zero crossings.
    const int approximateStart = static_cast<int>(count * .28);
    const int approximateEnd = static_cast<int>(count * .75);
    const int radius = std::max(2, static_cast<int>(sampleRate * .012));
    loop.start = nearestPositiveZeroCrossing(audio, count, approximateStart, radius);
    loop.end = nearestPositiveZeroCrossing(audio, count, approximateEnd, radius);
    loop.fade = std::max(16, std::min(static_cast<int>(sampleRate * .065),
                                    (loop.end - loop.start) / 5));
    loop.valid = loop.start >= 1 && loop.end < count - 2 &&
                 loop.end > loop.start + 3 * loop.fade;
    return loop;
}

// Staccato samples are one-shots: smoothly taper the last 14 milliseconds.
// This avoids clipping the natural sample tail at an arbitrary read boundary.
inline float oneShotTailGain(double position, int sampleCount, double sampleRate) noexcept
{
    if (sampleCount <= 2 || sampleRate <= 0.0) return 0.0f;
    const double remaining = static_cast<double>(sampleCount - 2) - position;
    const double fadeSamples = std::max(8.0, .014 * sampleRate);
    return static_cast<float>(std::clamp(remaining / fadeSamples, 0.0, 1.0));
}
}
