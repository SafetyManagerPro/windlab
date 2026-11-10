// Smoke test for the on-screen instrument fingerings (Source/Dsp/VirtualFingering.h).
#include "../Source/Dsp/VirtualFingering.h"
#include <cassert>
#include <cmath>
#include <iostream>

using namespace windlab;

static std::uint32_t keys(std::initializer_list<int> list)
{
    std::uint32_t m = 0;
    for (int k : list) m |= vkBit(k);
    return m;
}

int main()
{
    // --- Alto sax (index 7): written G4 = B + A + G, sounds Bb3.
    const auto alto = virtualProfileFor(7);
    auto n = computeVirtualNote(alto, keys({VkL1, VkL2, VkL3}), 0);
    assert(n.valid && n.written == 67 && n.concert == 58);
    assert(computeVirtualNote(alto, 0, 0).written == 73);                                   // open C#5
    assert(computeVirtualNote(alto, keys({VkL2}), 0).written == 72);                        // C5
    assert(computeVirtualNote(alto, keys({VkL1, VkL2, VkL3, VkR1, VkR2, VkR3}), 0).written == 62); // D4
    assert(computeVirtualNote(alto, keys({VkL1, VkL2, VkL3, VkR1, VkR2, VkR3, VkLow1}), 0).written == 60);
    assert(computeVirtualNote(alto, keys({VkL1, VkL2, VkL3, VkR2}), 0).written == 66);      // F# fork
    assert(computeVirtualNote(alto, keys({VkL1, VkL2, VkL3, VkLPinky}), 0).written == 68);  // G#
    assert(computeVirtualNote(alto, keys({VkL1, VkSideBb}), 0).written == 70);              // Bb side
    assert(computeVirtualNote(alto, keys({VkOctave, VkL1, VkL2, VkL3}), 0).written == 79);  // G5
    assert(computeVirtualNote(alto, keys({VkPalm1, VkPalm2, VkPalm3}), 0).written == 89);   // F6
    assert(computeVirtualNote(alto, keys({VkL1, VkL2, VkL3, VkSharp}), 0).written == 68);   // helper
    // Tenor and baritone use the same fingering, different transposition.
    assert(computeVirtualNote(virtualProfileFor(1), keys({VkL1, VkL2, VkL3}), 0).concert == 53);
    assert(computeVirtualNote(virtualProfileFor(8), keys({VkL1, VkL2, VkL3}), 0).concert == 46);
    assert(virtualProfileFor(6).straightBody && virtualProfileFor(6).transpose == -2);

    // --- Clarinet (index 4): chalumeau stack, register adds a twelfth.
    const auto clar = virtualProfileFor(4);
    assert(computeVirtualNote(clar, 0, 0).written == 67);                                    // open G4
    assert(computeVirtualNote(clar, keys({VkThumb}), 0).written == 65);                      // F4
    const auto allSix = keys({VkThumb, VkL1, VkL2, VkL3, VkR1, VkR2, VkR3});
    assert(computeVirtualNote(clar, allSix, 0).written == 55);                               // G3
    assert(computeVirtualNote(clar, allSix | vkBit(VkLow1) | vkBit(VkLow2), 0).written == 52); // E3
    n = computeVirtualNote(clar, allSix | vkBit(VkOctave), 0);
    assert(n.written == 74 && n.concert == 72);                                              // D5 -> C5
    assert(computeVirtualNote(clar, keys({VkThroatA}), 0).written == 69);                    // throat A

    // --- Flute (index 3): concert pitch, octave chosen by partial.
    const auto flute = virtualProfileFor(3);
    assert(flute.transpose == 0);
    const auto fluteD = keys({VkThumb, VkL1, VkL2, VkL3, VkR1, VkR2, VkR3});
    assert(computeVirtualNote(flute, fluteD, 0).concert == 62);                              // D4
    assert(computeVirtualNote(flute, fluteD | vkBit(VkLow1), 0).concert == 60);              // C4
    assert(computeVirtualNote(flute, fluteD, 1).concert == 74);                              // D5
    assert(computeVirtualNote(flute, keys({VkThumb, VkL1, VkL2, VkL3}), 2).concert == 91);   // G6
    assert(computeVirtualNote(flute, keys({VkL1}), 0).concert == 72);                        // C5

    // --- Trumpet (index 0): written E4 = valves 1+2 on the G4 partial, sounds D4.
    const auto tpt = virtualProfileFor(0);
    n = computeVirtualNote(tpt, keys({VkValve1, VkValve2}), 1);
    assert(n.written == 64 && n.concert == 62);
    assert(computeVirtualNote(tpt, 0, 2).written == 72);                                     // open C5
    assert(computeVirtualNote(tpt, keys({VkValve1, VkValve2, VkValve3}), 0).written == 54);  // low F#
    assert(computeVirtualNote(tpt, 0, 99).written == 84);                                    // clamped partial

    // --- Horn (index 5): in F, sounds a fifth lower.
    n = computeVirtualNote(virtualProfileFor(5), keys({VkValve2}), 3);
    assert(n.written == 66 && n.concert == 59);

    // --- Trombone (index 2): 4th position on the Bb3 partial = G3, slide bends continuously.
    const auto tbn = virtualProfileFor(2);
    n = computeVirtualNote(tbn, 0, 2, 4.0f);
    assert(n.concert == 55 && std::fabs(n.bendSemitones) < 1e-6f);
    n = computeVirtualNote(tbn, 0, 2, 4.3f);
    assert(n.concert == 55 && std::fabs(n.bendSemitones + 0.3f) < 1e-4f);
    n = computeVirtualNote(tbn, 0, 2, 4.7f);
    assert(n.concert == 54 && std::fabs(n.bendSemitones - 0.3f) < 1e-4f);
    assert(computeVirtualNote(tbn, 0, 0, 9.0f).concert == 40);                              // clamped to 7th

    // --- Alternate Bb "1+1" on sax and flute.
    assert(computeVirtualNote(alto, keys({VkL1, VkR1}), 0).written == 70);
    assert(computeVirtualNote(flute, keys({VkThumb, VkL1, VkR1}), 0).concert == 70);

    // --- MOSTRA DITEGGIATURA: reverse lookup gives back a fingering for every playable note.
    for (int index = 0; index < 9; ++index)
    {
        const auto profile = virtualProfileFor(index);
        int found = 0;
        for (int concert = 30; concert <= 100; ++concert)
        {
            const auto f = fingeringFor(profile, concert);
            if (!f.valid) continue;
            ++found;
            const auto back = computeVirtualNote(profile, f.mask, f.partial, f.slidePosition);
            assert(back.valid && back.concert == concert);
        }
        assert(found >= 20);
    }
    // Plain fingerings win: alto G4 = B A G only; trumpet written E4 = valves 1+2; trombone G3 = 4th position.
    assert(fingeringFor(alto, 58).mask == keys({VkL1, VkL2, VkL3}));
    const auto e4 = fingeringFor(tpt, 62);
    assert(e4.mask == keys({VkValve1, VkValve2}) && e4.partial == 1);
    assert(fingeringFor(tbn, 55).slidePosition == 4.0f);
    assert(!fingeringFor(alto, 20).valid);

    // --- Names.
    assert(italianNoteName(60) == "DO 4" && italianNoteName(58) == "SIb 3" && italianNoteName(55) == "SOL 3");

    std::cout << "WindLab virtual instrument fingering smoke test: PASS\n";
    return 0;
}
