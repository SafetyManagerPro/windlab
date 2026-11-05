#include "Dsp/WindControllerProfiles.h"
#include <cassert>
#include <iostream>
int main()
{
    using P = windlab::WindControllerProfiles;
    using M = windlab::MidiLearnBindings;
    M map;
    assert(P::packed(P::StandardCC2) == M::defaults());
    assert(P::apply(map, P::ArtinoiseReCorderCC11));
    assert(map.binding(M::Vibrato) == 1);
    assert(map.binding(M::Breath) == 11);
    assert(map.binding(M::Expression) == 2);
    assert(map.route(1, 11) == 2);
    assert(map.route(1, 2) == 11);
    assert(map.route(16, 11) == 11);
    const auto saved = map.snapshot();
    assert(!P::apply(map, -10) && map.snapshot() == saved);
    assert(P::apply(map, P::StandardCC2));
    assert(map.snapshot() == M::defaults());
    assert(map.route(1, 2) == 2);
    std::cout << "WindLab controller profiles: PASS\n";
}
