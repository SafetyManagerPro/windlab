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
    if (n == 1 && down(m, VkR1)) note = 70;                               // Bb4 "1+1" alternate
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
    if (n == 2 && down(m, VkR1) && !down(m, VkL2)) note = 70;             // Bb4 "1+1" alternate
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

// Reverse lookup for "MOSTRA DITEGGIATURA": the simplest fingering that sounds
// `concertNote` on this instrument. Woodwinds search every combination of the keys
// the instrument has (2^15 at most, only when the note changes) and prefer fewer
// keys, no helper # key and no side/palm keys when a plain fingering exists.
struct VirtualFingering
{
    bool valid = false;
    std::uint32_t mask = 0;
    int partial = 0;
    float slidePosition = 1.0f;
};

inline VirtualFingering fingeringFor(const VirtualProfile& p, int concertNote) noexcept
{
    VirtualFingering best;
    const int written = concertNote - p.transpose;
    if (p.family == VirtualFamily::Valves || p.family == VirtualFamily::Slide)
    {
        // Lowest lowering first = standard fingering / closest slide position.
        static constexpr std::array<std::uint32_t, 7> valves {
            0u, 1u << VkValve2, 1u << VkValve1, (1u << VkValve1) | (1u << VkValve2),
            (1u << VkValve2) | (1u << VkValve3), (1u << VkValve1) | (1u << VkValve3),
            (1u << VkValve1) | (1u << VkValve2) | (1u << VkValve3)};
        int bestLowering = 99;
        for (int i = 0; i < p.partialCount; ++i)
        {
            const int lowering = p.partials[static_cast<std::size_t>(i)] - written;
            if (lowering < 0 || lowering > 6 || lowering >= bestLowering) continue;
            bestLowering = lowering;
            best.valid = true;
            best.partial = i;
            best.mask = p.family == VirtualFamily::Valves ? valves[static_cast<std::size_t>(lowering)] : 0u;
            best.slidePosition = static_cast<float>(lowering + 1);
        }
        return best;
    }
    std::array<int, 16> keys {};
    int count = 0;
    auto add = [&](int k) { keys[static_cast<std::size_t>(count++)] = k; };
    switch (p.family)
    {
        case VirtualFamily::Sax:
            for (int k : {VkOctave, VkL1, VkL2, VkL3, VkLPinky, VkR1, VkR2, VkR3, VkLow1, VkLow2,
                          VkPalm1, VkPalm2, VkPalm3, VkSideBb, VkSharp}) add(k);
            break;
        case VirtualFamily::Clarinet:
            for (int k : {VkOctave, VkThumb, VkL1, VkL2, VkL3, VkR1, VkR2, VkR3, VkLow1, VkLow2, VkThroatA, VkSharp}) add(k);
            break;
        default:
            for (int k : {VkOctave, VkThumb, VkL1, VkL2, VkL3, VkLPinky, VkR1, VkR2, VkR3, VkLow1, VkSharp}) add(k);
            break;
    }
    int bestScore = 1 << 30;
    const int partials = p.family == VirtualFamily::Flute ? p.partialCount : 1;
    for (int part = 0; part < partials; ++part)
        for (std::uint32_t combo = 0; combo < (1u << count); ++combo)
        {
            std::uint32_t mask = 0;
            int score = part * 3;
            for (int b = 0; b < count; ++b)
                if (combo & (1u << b))
                {
                    const int k = keys[static_cast<std::size_t>(b)];
                    mask |= vkBit(k);
                    score += (k == VkSharp) ? 12 : (k == VkSideBb || k == VkLPinky || k == VkThroatA) ? 3
                           : (k >= VkPalm1 && k <= VkPalm3) ? 2 : 1;
                }
            if (score >= bestScore) continue;
            const auto n = computeVirtualNote(p, mask, part);
            if (n.valid && n.concert == concertNote)
            {
                bestScore = score;
                best = {true, mask, part, 1.0f};
            }
        }
    return best;
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
