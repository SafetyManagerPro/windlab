#include "Dsp/RecordedArticulation.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    using namespace windlab;
    RepeatedNoteCycle cycle;
    // One key alternates real takes even when another key interleaves.
    assert(cycle.takeIndex(0, 60, 1, 2) == 0u);
    assert(cycle.takeIndex(0, 62, 1, 2) == 0u);
    assert(cycle.takeIndex(0, 60, 1, 2) == 1u);
    assert(cycle.takeIndex(0, 62, 1, 2) == 1u);
    assert(cycle.takeIndex(0, 60, 1, 2) == 0u);

    // Articulations and instruments have independent counters.
    assert(cycle.takeIndex(0, 60, 0, 2) == 0u);
    assert(cycle.takeIndex(1, 60, 1, 2) == 0u);
    assert(cycle.takeIndex(0, 60, 0, 2) == 1u);
    // No alternate take: retain original, do not advance the cycle.
    assert(cycle.takeIndex(0, 60, 1, 1) == 0u);
    assert(cycle.takeIndex(0, 60, 1, 2) == 1u);
    // Works for three real takes without being hard-coded to RR1/RR2.
    assert(cycle.takeIndex(2, 48, 1, 3) == 0u);
    assert(cycle.takeIndex(2, 48, 1, 3) == 1u);
    assert(cycle.takeIndex(2, 48, 1, 3) == 2u);
    assert(cycle.takeIndex(2, 48, 1, 3) == 0u);

    for (double rate : {44100.0, 48000.0, 96000.0})
    {
        const int count = static_cast<int>(rate * .30);
        std::vector<float> take(static_cast<std::size_t>(count), 0.0f);
        const int onset = static_cast<int>(rate * .03);
        for (int i = onset; i < count; ++i)
            take[static_cast<std::size_t>(i)] =
                .44f * static_cast<float>(std::sin(6.283185307179586 * 220.0
                                                  * (i - onset) / rate));
        const int start = findStaccatoOnsetFrame(take.data(), count, rate);
        // Retain a pre-roll of about 1.5 ms and never skip the transient.
        assert(start >= static_cast<int>(rate * .027));
        assert(start < onset);
        // Early attack: don't cut it.
        std::vector<float> immediate(static_cast<std::size_t>(count), .2f);
        assert(findStaccatoOnsetFrame(immediate.data(), count, rate) == 0);
        // Silent or extremely quiet material is not trimmed speculatively.
        std::vector<float> silence(static_cast<std::size_t>(count), 0.0f);
        assert(findStaccatoOnsetFrame(silence.data(), count, rate) == 0);
        std::vector<float> noise(static_cast<std::size_t>(count), .0002f);
        assert(findStaccatoOnsetFrame(noise.data(), count, rate) == 0);
        // A single isolated transient must not hide the actual attack.
        take[20] = .6f;
        const int afterClick = findStaccatoOnsetFrame(take.data(), count, rate);
        assert(afterClick >= static_cast<int>(rate * .027));
        assert(afterClick < onset);
        assert(findStaccatoOnsetFrame(nullptr, count, rate) == 0);
        assert(findStaccatoOnsetFrame(take.data(), count, 0.0) == 0);
    }
    std::cout << "WindLab articulation and repeated notes: PASS\n";
}
