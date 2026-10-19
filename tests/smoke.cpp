#include "Dsp/WindEngine.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>

namespace
{
    float meanAbs(windlab::WindEngine& s, int n)
    {
        double sum = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const float x = s.render();
            assert(std::isfinite(x));
            assert(std::abs(x) < 1.0f);
            sum += std::abs(x);
        }
        return static_cast<float>(sum / n);
    }
}

int main()
{
    std::array<float, 6> signatures {};
    for (const auto sampleRate : {44100.0, 48000.0, 96000.0})
    {
        for (int inst = 0; inst < 6; ++inst)
        {
            windlab::WindEngine s;
            s.prepare(sampleRate);
            s.setTone(.8f);
            s.setAir(.2f);
            s.start(60, .8f, static_cast<windlab::Instrument>(inst), .01f, .04f);
            double sum = 0.0, signature = 0.0;
            for (int i = 0; i < static_cast<int>(sampleRate / 4.0); ++i)
            {
                const float x = s.render();
                assert(std::isfinite(x));
                assert(std::abs(x) < 1.0f);
                sum += std::abs(x);
                signature += x * static_cast<float>((i % 37) - 18);
            }
            assert(sum > 0.01);
            if (sampleRate == 48000.0) signatures[static_cast<std::size_t>(inst)] = static_cast<float>(signature);
            s.noteOff();
            meanAbs(s, static_cast<int>(sampleRate));
            assert(!s.active());
        }
    }
    assert(std::abs(signatures[0] - signatures[3]) > .001f);
    assert(std::abs(signatures[1] - signatures[4]) > .001f);

    // A staccato note releases itself without a noteOff event.
    windlab::WindEngine staccato;
    staccato.prepare(48000.0);
    staccato.start(67, .8f, windlab::Instrument::Trumpet, .1f, .7f,
                   windlab::Articulation::Staccato);
    meanAbs(staccato, 48000);
    assert(!staccato.active());

    // The swell articulation rises more slowly than ordinary sustain.
    windlab::WindEngine sustain, swell;
    sustain.prepare(48000.0); swell.prepare(48000.0);
    sustain.start(62, 1.f, windlab::Instrument::FrenchHorn, .01f, .1f);
    swell.start(62, 1.f, windlab::Instrument::FrenchHorn, .01f, .1f,
                windlab::Articulation::Swell);
    const float sustainEarly = meanAbs(sustain, 2400);
    const float swellEarly = meanAbs(swell, 2400);
    assert(swellEarly < sustainEarly * .4f);

    // Expression and CC2 breath reduction should silence a held note smoothly.
    windlab::WindEngine dynamic;
    dynamic.prepare(48000.0);
    dynamic.start(69, 1.f, windlab::Instrument::Clarinet, .01f, .05f);
    const float audible = meanAbs(dynamic, 5000);
    dynamic.setExpression(0.f);
    meanAbs(dynamic, 5000);
    assert(meanAbs(dynamic, 3000) < audible * .01f);
    dynamic.setExpression(1.f);
    dynamic.setBreath(0.f, true);
    meanAbs(dynamic, 5000);
    assert(meanAbs(dynamic, 3000) < audible * .01f);
    dynamic.setBreath(1.f, false);
    assert(meanAbs(dynamic, 5000) > audible * .1f);

    // Extreme notes, controller changes, all articulations: finite and bounded.
    for (int note : {0, 24, 60, 96, 127})
    {
        windlab::WindEngine s;
        s.prepare(44100.0);
        s.setModWheel(1.f);
        s.setPressure(1.f);
        s.setPitchWheel(16383);
        s.start(note, 1.f, windlab::Instrument::TenorSax, .005f, .02f);
        meanAbs(s, 4410);
        s.setPitchWheel(0);
        meanAbs(s, 4410);
        s.reset();
        assert(!s.active());
    }
    std::cout << "WindLab 0.2 DSP: PASS (6 timbres, 3 rates, articulations, CC expression/breath, bends, bounds).\n";
}
