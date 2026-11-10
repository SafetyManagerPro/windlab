#include "Dsp/LegatoHandoff.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    using windlab::LegatoHandoff;
    assert(LegatoHandoff::durationForSemitones(1) <
           LegatoHandoff::durationForSemitones(12));
    assert(LegatoHandoff::durationForSemitones(-8) ==
           LegatoHandoff::durationForSemitones(8));
    assert(LegatoHandoff::durationForSemitones(127) <= .075f);
    for (double rate : {44100.0, 48000.0, 96000.0})
    {
        LegatoHandoff fade;
        fade.prepare(rate);
        fade.start(false, .05f);
        for (int i = 0; i < 2000; ++i) assert(fade.nextGain() == 1.0f);

        fade.start(true, LegatoHandoff::durationForSemitones(5));
        assert(fade.isActive());
        float prior = 0.0f;
        const int expected = static_cast<int>(
            rate * LegatoHandoff::durationForSemitones(5));
        for (int i = 0; i < expected + 50; ++i)
        {
            const float current = fade.nextGain();
            assert(std::isfinite(current));
            assert(current >= prior && current >= 0.0f && current <= 1.0f);
            assert(current - prior < .003f);
            prior = current;
        }
        assert(!fade.isActive());
        assert(prior == 1.0f);

        fade.start(true, .04f);
        assert(fade.nextGain() < .01f);
        fade.start(false, .04f);
        assert(!fade.isActive() && fade.nextGain() == 1.0f);
    }
    std::cout << "WindLab legato handoff smoke: PASS\n";
    return 0;
}
