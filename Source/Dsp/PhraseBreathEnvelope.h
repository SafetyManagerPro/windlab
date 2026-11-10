#pragma once
// Natural phrase-end breathing: no abrupt gain restoration on auto note-off.
// Pure C++17, suitable for the real-time thread at constant per-sample cost.
#include <algorithm>
#include <array>
#include <cmath>

namespace windlab
{
struct PhraseBreathShape
{
    float level = 1.0f;
    float vibratoScale = 1.0f;
};

inline PhraseBreathShape phraseBreathShape(double elapsedSeconds, float breathLengthSeconds,
                                           int instrument, bool staccato) noexcept
{
    if (staccato || !std::isfinite(elapsedSeconds)
        || !std::isfinite(breathLengthSeconds) || breathLengthSeconds >= 29.99f)
        return {};

    const double duration = std::clamp(static_cast<double>(breathLengthSeconds), 2.0, 30.0);
    const double taper = std::min(1.8, duration * .20);
    const double t = std::clamp((elapsedSeconds - (duration - taper)) / taper, 0.0, 1.0);
    const float smooth = static_cast<float>(t * t * (3.0 - 2.0 * t));
    // Index order matches Instrument: trumpet, tenor, trombone, flute,
    // clarinet, horn, soprano, alto, baritone sax.
    constexpr std::array<float, 9> endDips {
        .43f, .46f, .44f, .50f, .43f, .46f, .45f, .46f, .47f
    };
    const int i = std::clamp(instrument, 0, 8);
    return {1.0f - endDips[static_cast<std::size_t>(i)] * smooth,
            1.0f - .28f * smooth};
}
}
