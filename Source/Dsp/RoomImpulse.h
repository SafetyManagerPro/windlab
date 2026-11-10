#pragma once
// 0.9.8 convolution reverb rooms. The impulse responses are GENERATED (not recorded):
// a few early reflections followed by a dense, exponentially decaying tail whose high
// frequencies die faster, like air and walls absorbing them. Deterministic and JUCE-free.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace windlab
{
enum class RoomType : int { Algorithmic = 0, Studio, JazzClub, ConcertHall, Church };

struct RoomShape
{
    double rt60;        // seconds for the tail to fall by 60 dB
    double preDelay;    // seconds before the first reflection
    double brightness;  // 0 = dark, 1 = bright
    double width;       // stereo decorrelation 0..1
};

inline RoomShape roomShapeFor(RoomType type) noexcept
{
    switch (type)
    {
        case RoomType::Studio:      return {0.45, .004, .75, .55};
        case RoomType::JazzClub:    return {0.95, .009, .55, .70};
        case RoomType::ConcertHall: return {2.10, .022, .50, .90};
        case RoomType::Church:      return {4.20, .035, .38, 1.0};
        case RoomType::Algorithmic:
        default:                    return {0.0, 0.0, 0.0, 0.0};
    }
}

// size and damping 0..1 come from the REVERB SIZE and DAMPING knobs:
// size stretches the decay (x0.6 .. x1.4), damping darkens the tail.
inline void generateRoomImpulse(RoomType type, double sampleRate, float size, float damping,
                                std::vector<float>& left, std::vector<float>& right)
{
    left.clear();
    right.clear();
    if (type == RoomType::Algorithmic || sampleRate < 8000.0) return;
    const auto shape = roomShapeFor(type);
    const double rt60 = shape.rt60 * (0.6 + 0.8 * std::clamp(static_cast<double>(size), 0.0, 1.0));
    const auto length = static_cast<std::size_t>(std::min(6.0, rt60 * 1.15 + shape.preDelay + .05) * sampleRate);
    left.assign(length, 0.0f);
    right.assign(length, 0.0f);

    std::uint32_t seed = 0x51ab3c7du + static_cast<std::uint32_t>(type) * 7919u;
    auto noise = [&seed]()
    {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>((seed >> 8) & 0xffffffu) / 8388607.5f - 1.0f;
    };

    // Early reflections: a handful of discrete echoes, alternating sides.
    const std::array<double, 8> early {1.0, 1.37, 1.83, 2.41, 2.96, 3.62, 4.4, 5.1};
    for (std::size_t i = 0; i < early.size(); ++i)
    {
        const auto at = static_cast<std::size_t>((shape.preDelay * early[i]) * sampleRate);
        if (at >= length) break;
        const float gain = static_cast<float>(.55 * std::pow(.78, static_cast<double>(i)));
        (i % 2 == 0 ? left : right)[at] += gain;
        (i % 2 == 0 ? right : left)[std::min(length - 1, at + static_cast<std::size_t>(.0007 * sampleRate))] += gain * .6f;
    }

    // Diffuse tail: decaying noise, low-passed more and more as it decays.
    const double decayPerSample = std::log(1000.0) / (rt60 * sampleRate);
    const auto tailStart = static_cast<std::size_t>(shape.preDelay * 2.0 * sampleRate);
    const double dampingAmount = std::clamp(static_cast<double>(damping), 0.0, 1.0);
    float lpL = 0.0f, lpR = 0.0f;
    for (std::size_t n = tailStart; n < length; ++n)
    {
        const double t = static_cast<double>(n - tailStart);
        const float env = static_cast<float>(std::exp(-decayPerSample * t));
        const double progress = t / std::max(1.0, rt60 * sampleRate);
        const float cutoff = static_cast<float>(std::clamp((.12 + .75 * shape.brightness) * (1.0 - .55 * dampingAmount)
                                                            * (1.0 - .7 * progress), .02, .95));
        const float common = noise();
        const float l = common * (1.0f - static_cast<float>(shape.width) * .5f) + noise() * static_cast<float>(shape.width) * .5f;
        const float r = common * (1.0f - static_cast<float>(shape.width) * .5f) + noise() * static_cast<float>(shape.width) * .5f;
        lpL += cutoff * (l - lpL);
        lpR += cutoff * (r - lpR);
        left[n] += lpL * env * .35f;
        right[n] += lpR * env * .35f;
    }
    // Fade the last 30 ms so the response never ends with a click.
    const auto fade = std::min(length, static_cast<std::size_t>(.03 * sampleRate));
    for (std::size_t i = 0; i < fade; ++i)
    {
        const float g = static_cast<float>(i) / static_cast<float>(fade);
        left[length - 1 - i] *= g;
        right[length - 1 - i] *= g;
    }
}
}
