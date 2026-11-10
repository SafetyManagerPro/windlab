#include "Dsp/HybridMidiRoute.h"
#include <cassert>
#include <iostream>

int main()
{
    using R = windlab::HybridRoute;
    const auto route = [](int ch, bool isCC, int cc, int noteCh, int breathCh)
    { return windlab::routeHybrid(ch, isCC, cc, noteCh, breathCh, 1, 2, 11); };

    assert(route(1, false, -1, 1, 2).action == R::Keep);
    assert(route(2, true, 2, 1, 2).action == R::Remap);
    assert(route(2, true, 2, 1, 2).channel == 1);
    assert(route(2, true, 11, 1, 2).action == R::Remap);
    assert(route(2, true, 1, 1, 2).action == R::Remap);
    assert(route(2, true, 64, 1, 2).action == R::Drop);
    assert(route(2, false, -1, 1, 2).action == R::Drop);
    assert(route(16, true, 2, 1, 2).action == R::Keep);
    assert(route(16, false, -1, 1, 2).action == R::Keep);

    assert(route(4, false, -1, 4, 7).action == R::Keep);
    assert(route(7, true, 2, 4, 7).action == R::Remap);
    assert(route(7, true, 2, 4, 7).channel == 4);
    assert(route(7, false, -1, 4, 7).action == R::Drop);
    assert(route(7, true, 74, 4, 7).action == R::Drop);
    assert(route(4, false, -1, 4, 4).action == R::Keep);
    assert(route(4, true, 2, 4, 4).action == R::Keep);
    assert(route(7, true, 2, 1, 16).action == R::Keep);
    assert(route(7, true, 2, 0, 7).action == R::Keep);
    assert(windlab::routeHybrid(7, true, 74, 4, 7, 1, 2, 11, true).action == R::Remap);
    assert(windlab::routeHybrid(7, false, -1, 4, 7, 1, 2, 11, true).action == R::Drop);
    // Releasing a note that began before switching to Hybrid must never be filtered.
    assert(windlab::routeHybrid(7, false, -1, 4, 7, 1, 2, 11, false, true).action == R::Keep);
    // Aftertouch used as breath on the breath channel is moved to the note channel...
    const auto at = windlab::routeHybrid(7, false, -1, 4, 7, 1, 2, 11, false, false, true);
    assert(at.action == R::Remap && at.channel == 4);
    // ...but aftertouch on other channels and the phrase channel stays untouched.
    assert(windlab::routeHybrid(4, false, -1, 4, 7, 1, 2, 11, false, false, true).action == R::Keep);
    assert(windlab::routeHybrid(16, false, -1, 4, 7, 1, 2, 11, false, false, true).action == R::Keep);
    std::cout << "WindLab hybrid MIDI route: PASS\n";
}
