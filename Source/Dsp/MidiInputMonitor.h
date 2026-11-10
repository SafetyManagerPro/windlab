#pragma once
#include <atomic>
#include <cstdint>

namespace windlab
{
// Lock-free diagnostics: never allocates or logs on the audio thread.
// A zero snapshot means that no event of that category has been observed yet.
class MidiInputMonitor final
{
public:
    void note(int channel, int pitch, bool on) noexcept
    {
        if (channel < 1 || channel > 15 || pitch < 0 || pitch > 127) return;
        const auto packed = 1u | (static_cast<std::uint32_t>(channel) << 1u)
            | (static_cast<std::uint32_t>(pitch) << 5u)
            | (static_cast<std::uint32_t>(on ? 1 : 0) << 12u);
        latestNote.store(packed, std::memory_order_relaxed);
    }
    void controller(int channel, int number, int value) noexcept
    {
        if (channel < 1 || channel > 15 || number < 0 || number > 127 || value < 0 || value > 127) return;
        const auto packed = 1u | (static_cast<std::uint32_t>(channel) << 1u)
            | (static_cast<std::uint32_t>(number) << 5u)
            | (static_cast<std::uint32_t>(value) << 12u);
        latestCC.store(packed, std::memory_order_relaxed);
    }
    void pressure(int channel, int value) noexcept
    {
        if (channel < 1 || channel > 15 || value < 0 || value > 127) return;
        const auto packed = 1u | (static_cast<std::uint32_t>(channel) << 1u)
            | (static_cast<std::uint32_t>(value) << 5u);
        latestPressure.store(packed, std::memory_order_relaxed);
    }
    struct Pressure { bool valid; int channel; int value; };
    Pressure pressureSnapshot() const noexcept
    {
        const auto s = latestPressure.load(std::memory_order_relaxed);
        return {(s & 1u) != 0u, static_cast<int>((s >> 1u) & 15u), static_cast<int>((s >> 5u) & 127u)};
    }
    struct Note { bool valid; int channel; int pitch; bool on; };
    struct CC { bool valid; int channel; int number; int value; };
    Note noteSnapshot() const noexcept
    {
        const auto s = latestNote.load(std::memory_order_relaxed);
        return {(s & 1u) != 0u, static_cast<int>((s >> 1u) & 15u),
                static_cast<int>((s >> 5u) & 127u), ((s >> 12u) & 1u) != 0u};
    }
    CC ccSnapshot() const noexcept
    {
        const auto s = latestCC.load(std::memory_order_relaxed);
        return {(s & 1u) != 0u, static_cast<int>((s >> 1u) & 15u),
                static_cast<int>((s >> 5u) & 127u), static_cast<int>((s >> 12u) & 127u)};
    }
    void clear() noexcept
    {
        latestNote.store(0, std::memory_order_relaxed);
        latestCC.store(0, std::memory_order_relaxed);
        latestPressure.store(0, std::memory_order_relaxed);
    }
private:
    std::atomic<std::uint32_t> latestNote{0};
    std::atomic<std::uint32_t> latestCC{0};
    std::atomic<std::uint32_t> latestPressure{0};
};
}
