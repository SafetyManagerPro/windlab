#pragma once
#include "MidiLearnBindings.h"
#include <cstdint>

namespace windlab
{
// Named MIDI wiring suggestions, not device auto-detection.
// Hardware-specific MIDI output must match the selected preset.
struct WindControllerProfiles
{
    enum Profile { StandardCC2 = 1, ArtinoiseReCorderCC11 = 2 };
    static constexpr std::uint32_t packed(int profile) noexcept
    {
        // Source CC1 -> vibrato; CC2 or CC11 -> breath.
        // On Artinoise, expression moves to CC2 to avoid a collision.
        return profile == ArtinoiseReCorderCC11
            ? (1u | (11u << 7) | (2u << 14))
            : MidiLearnBindings::defaults();
    }
    static bool apply(MidiLearnBindings& bindings, int profile) noexcept
    {
        if (profile != StandardCC2 && profile != ArtinoiseReCorderCC11)
            return false;
        return bindings.restore(packed(profile));
    }
};
}
