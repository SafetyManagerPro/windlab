#pragma once
// Voice-safe repeated-note selection and sample onset analysis.
// Selection uses separate atomic counters per instrument, MIDI key and articulation.
// All onset scanning occurs once while loading samples, not in the audio callback.
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace windlab
{
class RepeatedNoteCycle final
{
public:
    RepeatedNoteCycle() noexcept
    {
        for (auto& counter : counters)
            counter.store(0u, std::memory_order_relaxed);
    }

    unsigned takeIndex(int instrument, int midiNote, int articulation,
                       unsigned availableTakes) const noexcept
    {
        if (availableTakes <= 1u) return 0u;
        const unsigned voice = static_cast<unsigned>(std::clamp(instrument, 0, 8));
        const unsigned key = static_cast<unsigned>(std::clamp(midiNote, 0, 127));
        const unsigned art = articulation == 1 ? 1u : 0u;
        const unsigned index = (voice * 128u + key) * 2u + art;
        return counters[index].fetch_add(1u, std::memory_order_relaxed) % availableTakes;
    }

private:
    mutable std::array<std::atomic<unsigned>, 9 * 128 * 2> counters {};
};

// Staccato one-shots sometimes carry leading silence in the original take.
// Remove only an obvious quiet pre-roll (up to 80ms), retaining ~1.5ms of air
// before the actual acoustic transient and the entire unmodified WAV tail.
inline int findStaccatoOnsetFrame(const float* audio, int count, double sampleRate) noexcept
{
    if (audio == nullptr || count < 128 || !std::isfinite(sampleRate)
        || sampleRate < 8000.0 || sampleRate > 192000.0)
        return 0;

    const int peakLimit = std::min(count, static_cast<int>(sampleRate * .25));
    float peak = 0.0f;
    for (int i = 0; i < peakLimit; ++i)
        if (std::isfinite(audio[i])) peak = std::max(peak, std::abs(audio[i]));
    if (peak < .001f) return 0;

    const float threshold = std::max(.0008f, peak * .025f);
    const int searchLimit = std::min(count / 2, static_cast<int>(sampleRate * .08));
    const int frameBlock = std::max(8, static_cast<int>(sampleRate * .0005));
    const int preRoll = std::max(1, static_cast<int>(sampleRate * .0015));
    for (int start = 0; start < searchLimit; start += frameBlock)
    {
        int hits = 0;
        const int end = std::min(searchLimit, start + frameBlock);
        for (int i = start; i < end; ++i)
        {
            if (std::isfinite(audio[i]) && std::abs(audio[i]) >= threshold)
                if (++hits >= 2) return std::max(0, start - preRoll);
        }
    }
    // Never skip the entire sample just because its attack is too quiet.
    return 0;
}
}
