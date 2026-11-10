// WindLab 0.9.7: universal MIDI Learn map (any parameter <- any CC).
#include "Dsp/ParameterCCMap.h"
#include <cassert>
#include <iostream>

int main()
{
    windlab::ParameterCCMap map;
    using M = windlab::ParameterCCMap;
    assert(map.parameterForCC(21) == M::none && map.armedParameter() == M::none);

    // Learn: arm parameter 5, the next CC is assigned and immediately drives it.
    map.arm(5);
    assert(map.handleController(21) == 5);
    assert(map.armedParameter() == M::none && map.ccForParameter(5) == 21);
    assert(map.handleController(22) == M::none);

    // Re-learning the same parameter moves it to the new CC.
    map.arm(5);
    assert(map.handleController(30) == 5);
    assert(map.parameterForCC(21) == M::none && map.ccForParameter(5) == 30);

    // A CC taken by another parameter is reassigned.
    map.arm(7);
    assert(map.handleController(30) == 7);
    assert(map.ccForParameter(5) == M::none && map.ccForParameter(7) == 30);

    // Reserved CCs are never learned: sustain, bank select, mode messages.
    map.arm(9);
    assert(map.handleController(64) == M::none);
    assert(map.handleController(0) == M::none && map.handleController(120) == M::none);
    assert(map.armedParameter() == 9);
    map.cancel();

    // Save / clear.
    map.assign(40, 2);
    assert(map.assignments().size() == 2);
    map.clearParameter(2);
    assert(map.ccForParameter(2) == M::none);
    map.clearAll();
    assert(map.assignments().empty());

    std::cout << "WindLab 0.9.7 universal MIDI Learn: PASS\n";
    return 0;
}
