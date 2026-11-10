#pragma once
// 0.9.7 universal MIDI Learn: any WindLab parameter can follow a MIDI CC.
// JUCE-free and lock-free: the editor arms a parameter (by index), the audio thread
// assigns the next incoming CC and translates CC values into parameter targets.
#include <array>
#include <atomic>
#include <string>
#include <vector>

namespace windlab
{
class ParameterCCMap
{
public:
    static constexpr int none = -1;

    ParameterCCMap() { clearAll(); }

    static constexpr bool learnableCC(int cc) noexcept
    {
        // Bank select (0/32), sustain (64) and channel-mode messages (120+) stay reserved.
        return cc >= 1 && cc <= 119 && cc != 32 && cc != 64;
    }

    // Message thread.
    void arm(int parameterIndex) noexcept { armed.store(parameterIndex, std::memory_order_release); }
    void cancel() noexcept { armed.store(none, std::memory_order_release); }
    int armedParameter() const noexcept { return armed.load(std::memory_order_acquire); }

    int ccForParameter(int parameterIndex) const noexcept
    {
        if (parameterIndex < 0) return none;
        for (int cc = 0; cc < 128; ++cc)
            if (map[static_cast<std::size_t>(cc)].load(std::memory_order_acquire) == parameterIndex)
                return cc;
        return none;
    }
    int parameterForCC(int cc) const noexcept
    {
        return (cc >= 0 && cc < 128) ? map[static_cast<std::size_t>(cc)].load(std::memory_order_acquire) : none;
    }

    // One CC drives one parameter, and a parameter follows one CC.
    bool assign(int cc, int parameterIndex) noexcept
    {
        if (!learnableCC(cc) || parameterIndex < 0) return false;
        clearParameter(parameterIndex);
        map[static_cast<std::size_t>(cc)].store(parameterIndex, std::memory_order_release);
        return true;
    }
    void clearParameter(int parameterIndex) noexcept
    {
        for (auto& slot : map)
        {
            int expected = parameterIndex;
            slot.compare_exchange_strong(expected, none, std::memory_order_acq_rel);
        }
    }
    void clearAll() noexcept
    {
        for (auto& slot : map) slot.store(none, std::memory_order_release);
        armed.store(none, std::memory_order_release);
    }

    // Audio thread: completes a pending learn and returns the parameter driven by
    // this CC (or none).
    int handleController(int cc) noexcept
    {
        if (!learnableCC(cc)) return none;
        int waiting = armed.load(std::memory_order_acquire);
        if (waiting != none && armed.compare_exchange_strong(waiting, none, std::memory_order_acq_rel))
            assign(cc, waiting);
        return map[static_cast<std::size_t>(cc)].load(std::memory_order_acquire);
    }

    // All current pairs, for saving with the project.
    std::vector<std::pair<int, int>> assignments() const
    {
        std::vector<std::pair<int, int>> out;
        for (int cc = 0; cc < 128; ++cc)
        {
            const int p = map[static_cast<std::size_t>(cc)].load(std::memory_order_acquire);
            if (p != none) out.emplace_back(cc, p);
        }
        return out;
    }

private:
    std::array<std::atomic<int>, 128> map {};
    std::atomic<int> armed {none};
};
}
