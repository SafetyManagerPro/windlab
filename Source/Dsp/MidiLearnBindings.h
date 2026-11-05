#pragma once
// Allocation-free MIDI CC learn and routing for WindLab v0.9.2.
// A single 32-bit atomic snapshot keeps all three assignments consistent.
// The phrase sequencer's channel 16 bypasses learn and routing.
#include <atomic>
#include <cstdint>

namespace windlab
{
class MidiLearnBindings final
{
public:
    enum Target : int { None = 0, Vibrato = 1, Breath = 2, Expression = 3 };

    MidiLearnBindings() = default;
    static constexpr std::uint32_t defaults() noexcept
    {
        return 1u | (2u << 7) | (11u << 14);
    }
    static constexpr int ccFrom(std::uint32_t state, int target) noexcept
    {
        return target >= Vibrato && target <= Expression
                   ? static_cast<int>((state >> (7 * (target - 1))) & 127u) : -1;
    }
    static constexpr bool validCC(int cc) noexcept { return cc >= 1 && cc <= 119; }

    void reset() noexcept
    {
        armed.store(None, std::memory_order_release);
        assignments.store(defaults(), std::memory_order_release);
    }
    int binding(int target) const noexcept
    {
        return ccFrom(assignments.load(std::memory_order_acquire), target);
    }
    int armedTarget() const noexcept { return armed.load(std::memory_order_acquire); }
    std::uint32_t snapshot() const noexcept
    {
        return assignments.load(std::memory_order_acquire);
    }
    void arm(int target) noexcept
    {
        armed.store(target >= Vibrato && target <= Expression ? target : None,
                    std::memory_order_release);
    }
    void cancel() noexcept { armed.store(None, std::memory_order_release); }

    // Collision-safe: assigning one CC to another target swaps the old binding.
    // This preserves all three mappings and ensures no event has two meanings.
    bool assign(int target, int cc) noexcept
    {
        if (target < Vibrato || target > Expression || !validCC(cc)) return false;
        auto previous = assignments.load(std::memory_order_acquire);
        for (;;)
        {
            const int old = ccFrom(previous, target);
            std::uint32_t next = previous;
            const int shift = 7 * (target - 1);
            next = (next & ~(127u << shift)) | (static_cast<std::uint32_t>(cc) << shift);
            for (int other = Vibrato; other <= Expression; ++other)
            {
                if (other == target || ccFrom(previous, other) != cc) continue;
                const int otherShift = 7 * (other - 1);
                next = (next & ~(127u << otherShift))
                       | (static_cast<std::uint32_t>(old) << otherShift);
            }
            if (assignments.compare_exchange_weak(previous, next,
                      std::memory_order_acq_rel, std::memory_order_acquire))
                return true;
        }
    }
    bool restore(std::uint32_t packed) noexcept
    {
        const int cc1 = ccFrom(packed, Vibrato);
        const int cc2 = ccFrom(packed, Breath);
        const int cc3 = ccFrom(packed, Expression);
        if ((packed & ~((1u << 21) - 1u)) != 0u ||
            !validCC(cc1) || !validCC(cc2) || !validCC(cc3) ||
            cc1 == cc2 || cc1 == cc3 || cc2 == cc3)
            return false;
        armed.store(None, std::memory_order_release);
        assignments.store(packed, std::memory_order_release);
        return true;
    }

    // Call on the audio thread before JUCE's Synthesiser::handleController.
    // Returns a normalized CC (1,2,11), the unchanged non-special CC, or -1
    // if a formerly special CC is no longer assigned and must not affect sound.
    int route(int midiChannel, int cc) noexcept
    {
        if (midiChannel < 1 || midiChannel >= 16 || !validCC(cc))
            return cc;  // channel 16 remains entirely owned by Phrase Editor.
        int waiting = armed.load(std::memory_order_acquire);
        if (waiting != None && armed.compare_exchange_strong(
                waiting, None, std::memory_order_acq_rel))
            assign(waiting, cc);

        const auto map = snapshot();
        if (cc == ccFrom(map, Vibrato)) return 1;
        if (cc == ccFrom(map, Breath)) return 2;
        if (cc == ccFrom(map, Expression)) return 11;
        if (cc == 1 || cc == 2 || cc == 11) return -1;
        return cc;  // preserve MIDI sustain, bank select and other CCs.
    }

private:
    std::atomic<std::uint32_t> assignments { defaults() };
    std::atomic<int> armed { None };
};
}
