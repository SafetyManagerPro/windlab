#include "Dsp/DynamicLayerMorph.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    using windlab::DynamicLayerMorph;
    assert(DynamicLayerMorph::highLayerWeight(.05f) == 0.0f);
    assert(DynamicLayerMorph::highLayerWeight(1.0f) == 1.0f);
    float previous = 0.0f;
    for (int i = 0; i <= 100; ++i)
    {
        const float next = DynamicLayerMorph::highLayerWeight(i / 100.0f);
        assert(next >= previous && next <= 1.0f);
        previous = next;
    }

    for (double rate : {44100.0, 48000.0, 96000.0})
    {
        DynamicLayerMorph morph;
        morph.prepare(rate);
        morph.reset(true);
        for (int i = 0; i < static_cast<int>(rate * .04); ++i)
            assert(morph.primaryWeight(.05f, true) == 1.0f); // sustain attack preserved
        float w = 1.0f;
        float last = w;
        for (int i = 0; i < static_cast<int>(rate); ++i)
        {
            w = morph.primaryWeight(.05f, true); // play actual soft recording
            assert(std::isfinite(w) && w >= 0.0f && w <= 1.0f);
            assert(std::abs(w - last) < .002f);
            last = w;
        }
        assert(w < .01f);
        for (int i = 0; i < static_cast<int>(rate); ++i)
        {
            w = morph.primaryWeight(1.0f, true); // now actual loud recording
            assert(std::isfinite(w) && w >= 0.0f && w <= 1.0f);
            assert(std::abs(w - last) < .002f);
            last = w;
        }
        assert(w > .99f);

        // No external breath input: existing velocity-selected voice stays untouched.
        morph.reset(false);
        for (int i = 0; i < static_cast<int>(rate); ++i)
            assert(morph.primaryWeight(1.0f, false) == 1.0f);
        morph.reset(true);
        for (int i = 0; i < static_cast<int>(rate); ++i)
            assert(morph.primaryWeight(0.0f, false) == 1.0f);
    }
    std::cout << "WindLab dynamic layer morph smoke: PASS\n";
}
