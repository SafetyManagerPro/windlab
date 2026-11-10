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

// Approximate similarity of the two pieces of audio mixed in the loop seam.
// Measure a sparse, normalized squared difference; audio is scanned only at
// sample-bank load time, never from the real-time audio callback.
inline float loopFadeMismatch(const float* audio, int count,
                              int start, int end, int fade) noexcept
{
    if (audio == nullptr || start < 0 || fade < 16 || end <= start + 3 * fade
        || end >= count - 2 || start + fade >= count)
        return 2.0f;
    constexpr int probes = 96;
    double error = 0.0, energy = 0.0;
    for (int k = 0; k < probes; ++k)
    {
        const int offset = k * (fade - 1) / (probes - 1);
        const double a = audio[start + offset];
        const double b = audio[end - fade + offset];
        const double delta = a - b;
        error += delta * delta;
        energy += a * a + b * b;
    }
    // Reject quiet/degenerate regions rather than amplifying numerical noise.
    return static_cast<float>(error / (energy + 1.0e-9));
}

inline SampleLoopPlan chooseSampleLoop(const float* audio, int count, double sampleRate) noexcept
{
    SampleLoopPlan loop;
    if (audio == nullptr || count < 4000 || !std::isfinite(sampleRate) || sampleRate <= 0.0)
        return loop;
    // Keep the proven sustain region and positive zero-crossing boundaries.
    // Additionally choose a compatible waveform segment for the 65ms seam,
    // so a soft/loud sustain loops with less phase/level beating.
    const int approximateStart = static_cast<int>(count * .28);
    const int approximateEnd = static_cast<int>(count * .75);
    const int radius = std::max(2, static_cast<int>(sampleRate * .012));
    loop.start = nearestPositiveZeroCrossing(audio, count, approximateStart, radius);
    loop.end = nearestPositiveZeroCrossing(audio, count, approximateEnd, radius);
    loop.fade = std::max(16, std::min(static_cast<int>(sampleRate * .065),
                                     (loop.end - loop.start) / 5));
    loop.valid = loop.start >= 1 && loop.end < count - 2 &&
                 loop.end > loop.start + 3 * loop.fade;
    if (!loop.valid) return loop;

    // Compare possible rising zero crossings within 12ms of the original end.
    // Always preserve the old endpoint unless an alternative is clearly better.
    int bestEnd = loop.end;
    float bestScore = loopFadeMismatch(audio, count, loop.start, bestEnd, loop.fade);
    const int low = std::clamp(approximateEnd - radius, loop.start + 3 * loop.fade + 1, count - 3);
    const int high = std::clamp(approximateEnd + radius, low, count - 3);
    for (int candidate = low; candidate <= high; ++candidate)
    {
        if (audio[candidate - 1] > 0.0f || audio[candidate] <= 0.0f)
            continue;
        const float mismatch = loopFadeMismatch(audio, count, loop.start, candidate, loop.fade);
        const float distancePenalty = .018f * static_cast<float>(std::abs(candidate - approximateEnd))
                                     / static_cast<float>(radius);
        const float score = mismatch + distancePenalty;
        if (score + .007f < bestScore)
        {
            bestScore = score;
            bestEnd = candidate;
        }
    }
    loop.end = bestEnd;
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
