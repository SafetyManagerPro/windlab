#pragma once
// Precompute an approximate phase match between two separately recorded
// sustain takes before real-time playback. No allocations, locks, or I/O.
#include <algorithm>
#include <cmath>

namespace windlab
{
struct SamplePhaseAlignment
{
    // Advance the secondary recording by this many of its original frames.
    double companionAdvance = 0.0;
    float correlation = 0.0f;
};

inline float phaseSampleAt(const float* audio, int count, double frame) noexcept
{
    const int i = std::clamp(static_cast<int>(frame), 0, count - 2);
    const float fractional = static_cast<float>(frame - static_cast<double>(i));
    return audio[i] + (audio[i + 1] - audio[i]) * fractional;
}

// Normalized correlation ignores the naturally different loudness of piano
// and forte takes. Only an early, short sustain region is compared.
inline float samplePairCorrelation(const float* primary, int primaryCount, double primaryRate,
                                   const float* secondary, int secondaryCount, double secondaryRate,
                                   double secondaryAdvance) noexcept
{
    if (!primary || !secondary || primaryCount < 2 || secondaryCount < 2
        || !std::isfinite(primaryRate) || !std::isfinite(secondaryRate)
        || primaryRate < 8000.0 || secondaryRate < 8000.0
        || !std::isfinite(secondaryAdvance) || secondaryAdvance < 0.0)
        return 0.0f;
    constexpr int probes = 256;
    constexpr double startTime = .075;
    constexpr double spanTime = .065;
    const double lastTime = startTime + spanTime;
    if (lastTime * primaryRate >= primaryCount - 2
        || lastTime * secondaryRate + secondaryAdvance >= secondaryCount - 2)
        return 0.0f;

    double dot = 0.0, energyA = 0.0, energyB = 0.0;
    for (int i = 0; i < probes; ++i)
    {
        const double time = startTime + spanTime * static_cast<double>(i)
                                        / static_cast<double>(probes - 1);
        const double a = phaseSampleAt(primary, primaryCount, time * primaryRate);
        const double b = phaseSampleAt(secondary, secondaryCount,
                                      time * secondaryRate + secondaryAdvance);
        dot += a * b;
        energyA += a * a;
        energyB += b * b;
    }
    if (energyA < 1.0e-6 || energyB < 1.0e-6) return 0.0f;
    return static_cast<float>(std::clamp(dot / std::sqrt(energyA * energyB), -1.0, 1.0));
}

inline SamplePhaseAlignment estimateSamplePhaseAlignment(
    const float* primary, int primaryCount, double primaryRate,
    const float* secondary, int secondaryCount, double secondaryRate,
    double nominalPitchHz) noexcept
{
    SamplePhaseAlignment plan;
    if (!std::isfinite(nominalPitchHz) || nominalPitchHz < 35.0 || nominalPitchHz > 2500.0)
        return plan;

    const float original = samplePairCorrelation(primary, primaryCount, primaryRate,
                                                 secondary, secondaryCount, secondaryRate, 0.0);
    plan.correlation = original;

    // Search a single expected wave period in the companion take. A positive
    // advance can align every phase without rewinding into a negative sample.
    // The maximum advance is <= 29ms at the lowest supported notes.
    const double periodFrames = secondaryRate / nominalPitchHz;
    const int slots = std::clamp(static_cast<int>(periodFrames / 3.0), 12, 64);
    double bestAdvance = 0.0;
    float bestCorrelation = original;
    float bestScore = original;

    for (int i = 1; i <= slots; ++i)
    {
        const double advance = periodFrames * static_cast<double>(i)
                                            / static_cast<double>(slots);
        const float correlation = samplePairCorrelation(primary, primaryCount, primaryRate,
                                                        secondary, secondaryCount, secondaryRate,
                                                        advance);
        // Avoid rotating a nearly aligned take by a full period unnecessarily.
        const float score = correlation - .018f * static_cast<float>(advance / periodFrames);
        if (score > bestScore)
        {
            bestScore = score;
            bestCorrelation = correlation;
            bestAdvance = advance;
        }
    }
    // If these takes are too dissimilar, leave the original pairing unchanged.
    // A doubtful phase guess can sound worse than the prior linear crossfade.
    if (bestCorrelation >= .70f && bestCorrelation > original + .13f)
    {
        plan.companionAdvance = bestAdvance;
        plan.correlation = bestCorrelation;
    }
    return plan;
}
}
