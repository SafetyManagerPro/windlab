#pragma once
// Fingerings for the on-screen instrument ("STRUMENTO A VIDEO").
// JUCE-free so it can be tested on any machine (tests/virtual-fingering-smoke.cpp).
//
// The UI turns pressed keys (mouse or computer keyboard) into a bit mask, picks a
// partial (brass "lips" / flute overblowing) and, for the trombone, a slide
// position. This file turns that into the written note, the sounding (concert)
// MIDI note and, for the slide, a pitch-bend offset in semitones.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace windlab
{
enum class VirtualFamily { Sax, Clarinet, Flute, Valves, Slide };

// Abstract keys. Each family uses the subset that exists on the real instrument.
enum VirtualKey : int
{
    VkOctave = 0,   // sax octave key, clarinet register key, flute "overblow"
    VkThumb,        // clarinet thumb hole, flute B thumb key
    VkL1, VkL2, VkL3, // left hand index, middle, ring
    VkLPinky,       // sax/flute G# key
    VkR1, VkR2, VkR3, // right hand index, middle, ring
    VkLow1,         // sax low C, clarinet F key, flute foot-joint C
    VkLow2,         // sax low B, clarinet E key
    VkPalm1, VkPalm2, VkPalm3, // sax palm keys D, Eb, F
    VkSideBb, VkSideC, // sax side keys
    VkThroatA,      // clarinet throat A key
    VkSharp,        // helper: raises any woodwind fingering by a semitone
    VkValve1, VkValve2, VkValve3,
    VkCount
};

constexpr std::uint32_t vkBit(int key) noexcept { return 1u << static_cast<unsigned>(key); }

struct VirtualProfile
{
    VirtualFamily family;
    int transpose;            // written -> concert, in semitones (alto sax = -9)
    std::array<int, 8> partials; // valves/slide: written partial notes; flute: octave offsets
    int partialCount;
    int defaultPartial;
    bool straightBody;        // soprano sax is drawn straight
    const char* writtenKey;   // "Sib", "Mib", "Fa", "Do"
};

// Same order as WindLab's "instrument" parameter.
inline VirtualProfile virtualProfileFor(int instrumentIndex) noexcept
{
    switch (instrumentIndex)
    {
        case 0: return {VirtualFamily::Valves, -2, {60, 67, 72, 76, 79, 84, 0, 0}, 6, 1, false, "Sib"};   // trumpet in Bb
        case 1: return {VirtualFamily::Sax, -14, {}, 0, 0, false, "Sib"};                                  // tenor sax
        case 2: return {VirtualFamily::Slide, 0, {46, 53, 58, 62, 65, 70, 0, 0}, 6, 2, false, "Do"};       // trombone
        case 3: return {VirtualFamily::Flute, 0, {0, 12, 24, 0, 0, 0, 0, 0}, 3, 0, false, "Do"};           // flute
        case 4: return {VirtualFamily::Clarinet, -2, {}, 0, 0, false, "Sib"};                              // clarinet in Bb
        case 5: return {VirtualFamily::Valves, -7, {55, 60, 64, 67, 72, 74, 76, 79}, 8, 3, false, "Fa"};   // horn in F
        case 6: return {VirtualFamily::Sax, -2, {}, 0, 0, true, "Sib"};                                    // soprano sax
        case 8: return {VirtualFamily::Sax, -21, {}, 0, 0, false, "Mib"};                                  // baritone sax
        case 7:
        default: return {VirtualFamily::Sax, -9, {}, 0, 0, false, "Mib"};                                  // alto sax
    }
}

struct VirtualNote
{
    bool valid = false;
    int written = -1;     // written MIDI note (what the player reads)
    int concert = -1;     // sounding MIDI note sent to the engine
    float bendSemitones = 0.0f; // slide only: offset from `concert`, within +-0.5
};

namespace detail
{
inline bool down(std::uint32_t mask, int key) noexcept { return (mask & vkBit(key)) != 0u; }

template <std::size_t N>
inline int closedStack(std::uint32_t mask, const std::array<int, N>& order) noexcept
{
    int n = 0;
    while (n < static_cast<int>(N) && down(mask, order[static_cast<std::size_t>(n)])) ++n;
    return n;
}

inline int saxWritten(std::uint32_t m) noexcept
{
    if (down(m, VkPalm1))
        return down(m, VkPalm2) ? (down(m, VkPalm3) ? 89 : 87) : 86; // D6, Eb6, F6
    static constexpr std::array<int, 8> order {VkL1, VkL2, VkL3, VkR1, VkR2, VkR3, VkLow1, VkLow2};
    static constexpr std::array<int, 9> notes {73, 71, 69, 67, 65, 64, 62, 60, 59};
    const int n = closedStack(m, order);
    int note = notes[static_cast<std::size_t>(n)];
    if (n == 0 && down(m, VkL2) && !down(m, VkL3)) note = 72;            // C5: middle finger only
    if (n == 1 && down(m, VkSideBb)) note = 70;                           // Bb4 side key
    if (n == 1 && down(m, VkSideC)) note = 72;                            // C5 side key
    if (n == 3 && down(m, VkLPinky)) note = 68;                           // G#4
    if (n == 3 && !down(m, VkR1) && down(m, VkR2)) note = 66;             // F#4 fork
    if (down(m, VkOctave) && note <= 73) note += 12;
    return note;
}

inline int clarinetWritten(std::uint32_t m) noexcept
{
    static constexpr std::array<int, 9> order {VkThumb, VkL1, VkL2, VkL3, VkR1, VkR2, VkR3, VkLow1, VkLow2};
    static constexpr std::array<int, 10> notes {67, 65, 64, 62, 60, 59, 57, 55, 53, 52};
    const int n = closedStack(m, order);
    if (n == 0)
        return (down(m, VkThroatA) || down(m, VkOctave)) ? (down(m, VkOctave) ? 70 : 69) : 67;
    int note = notes[static_cast<std::size_t>(n)];
    if (down(m, VkOctave)) note += 19; // the clarinet overblows a twelfth
    return note;
}

inline int fluteWritten(std::uint32_t m, int octaveOffset) noexcept
{
    static constexpr std::array<int, 8> order {VkThumb, VkL1, VkL2, VkL3, VkR1, VkR2, VkR3, VkLow1};
    static constexpr std::array<int, 9> notes {73, 70, 71, 69, 67, 65, 64, 62, 60};
    const int n = closedStack(m, order);
    int note = notes[static_cast<std::size_t>(n)];
    if (n == 0 && down(m, VkL1)) note = 72;                               // C5: index only
    if (n == 4 && down(m, VkLPinky)) note = 68;                           // G#4
    if (n == 4 && !down(m, VkR1) && down(m, VkR3)) note = 66;             // F#4 fork
    note += octaveOffset;
    if (down(m, VkOctave)) note += 12;
    return note;
}
} // namespace detail

inline VirtualNote computeVirtualNote(const VirtualProfile& p, std::uint32_t mask,
                                      int partialIndex, float slidePosition = 1.0f) noexcept
{
    VirtualNote out;
    const int partial = p.partialCount > 0 ? std::clamp(partialIndex, 0, p.partialCount - 1) : 0;
    int written = -1;
    switch (p.family)
    {
        case VirtualFamily::Sax:      written = detail::saxWritten(mask); break;
        case VirtualFamily::Clarinet: written = detail::clarinetWritten(mask); break;
        case VirtualFamily::Flute:
            written = detail::fluteWritten(mask, p.partials[static_cast<std::size_t>(partial)]);
            break;
        case VirtualFamily::Valves:
        {
            const int lower = (detail::down(mask, VkValve1) ? 2 : 0)
                            + (detail::down(mask, VkValve2) ? 1 : 0)
                            + (detail::down(mask, VkValve3) ? 3 : 0);
            written = p.partials[static_cast<std::size_t>(partial)] - lower;
            break;
        }
        case VirtualFamily::Slide:
        {
            const float semis = std::clamp(slidePosition, 1.0f, 7.0f) - 1.0f;
            const int nearest = static_cast<int>(std::lround(semis));
            written = p.partials[static_cast<std::size_t>(partial)] - nearest;
            out.bendSemitones = -(semis - static_cast<float>(nearest));
            break;
        }
    }
    if (p.family == VirtualFamily::Sax || p.family == VirtualFamily::Clarinet || p.family == VirtualFamily::Flute)
        if (detail::down(mask, VkSharp)) ++written;

    out.written = written;
    out.concert = written + p.transpose;
    out.valid = out.concert >= 21 && out.concert <= 108;
    if (!out.valid) out.bendSemitones = 0.0f;
    return out;
}

// Italian note name, MIDI 60 = "DO 4".
inline std::string italianNoteName(int midi)
{
    static constexpr std::array<const char*, 12> names {"DO", "DO#", "RE", "MIb", "MI", "FA",
                                                         "FA#", "SOL", "SOL#", "LA", "SIb", "SI"};
    if (midi < 0 || midi > 127) return "-";
    return std::string(names[static_cast<std::size_t>(midi % 12)]) + " " + std::to_string(midi / 12 - 1);
}
} // namespace windlab
