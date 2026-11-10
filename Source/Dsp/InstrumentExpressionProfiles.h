#pragma once
// WindLab 0.9.16: instrument-aware performance defaults.
// Indexed by the stable Instrument enum in WindEngine.h. Pure, constexpr data:
// no parameter IDs, preset changes, heap allocations, or audio-thread locks.
#include <array>
#include <algorithm>

namespace windlab
{
struct InstrumentExpressionProfile
{
    float softCutoffHz, strongCutoffHz;
    float breathColour, toneColour;
    float vibratoDelayScale, vibratoRampScale;
    float transientMs, transientDryMix;
};

// Order: trumpet, tenor sax, trombone, flute, clarinet, French horn,
// soprano sax, alto sax, baritone sax.
inline constexpr std::array<InstrumentExpressionProfile, 9> instrumentExpressionProfiles {{
    { 1550.f, 16000.f, .65f, .20f, .92f,  .95f, 14.f, .19f }, // trumpet: bright tongue and edge
    { 1200.f, 14400.f, .59f, .23f, 1.0f,  1.0f,  9.f, .14f }, // tenor sax: reed warmth
    { 1100.f, 13700.f, .56f, .23f, 1.12f, 1.04f, 17.f, .16f }, // trombone: brass body
    { 2600.f, 17300.f, .52f, .28f, 1.20f, 1.10f,  6.f, .10f }, // flute: airy, gentle articulation
    {  940.f, 11500.f, .52f, .20f, 1.28f, 1.15f, 11.f, .11f }, // clarinet: dark reed, restrained vibrato
    { 1120.f, 12800.f, .54f, .22f, 1.15f, 1.12f, 16.f, .15f }, // French horn: rounded attack
    { 1490.f, 14900.f, .63f, .23f,  .97f, 1.0f,  8.f, .17f }, // soprano sax
    { 1270.f, 14500.f, .61f, .23f, 1.0f,  1.0f, 10.f, .15f }, // alto sax
    { 1050.f, 13200.f, .58f, .22f, 1.07f, 1.07f, 13.f, .13f }  // baritone sax
}};

inline constexpr const InstrumentExpressionProfile& instrumentExpressionProfile(int instrument) noexcept
{
    return instrumentExpressionProfiles[static_cast<std::size_t>(
        std::clamp(instrument, 0, static_cast<int>(instrumentExpressionProfiles.size() - 1)))];
}
}
