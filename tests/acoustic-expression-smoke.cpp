// Acoustic recorded-sample timbre must follow breath without damaging low notes.
#include "Dsp/AcousticExpression.h"
#include <cassert>
#include <cmath>
#include <iostream>

using windlab::AcousticExpression;

static double rms(double rate, double frequency, float breath)
{
    AcousticExpression filter;
    filter.prepare(rate);
    filter.reset(breath, .55f);
    double sum = 0.0;
    const int count = static_cast<int>(rate / 2);
    for (int i = 0; i < count; ++i)
    {
        const float input = .5f * static_cast<float>(std::sin(6.28318530718 * frequency * i / rate));
        const float output = filter.process(input, breath, .55f);
        assert(std::isfinite(output) && std::abs(output) <= .501f);
        if (i >= count / 2) sum += static_cast<double>(output) * output;
    }
    return std::sqrt(sum / (count / 2));
}

int main()
{
    for (double rate : {44100.0, 48000.0, 96000.0})
    {
        const double softHigh = rms(rate, 6000.0, .10f);
        const double strongHigh = rms(rate, 6000.0, 1.0f);
        const double softLow = rms(rate, 300.0, .10f);
        const double strongLow = rms(rate, 300.0, 1.0f);
        assert(strongHigh > softHigh * 1.25);
        assert(softLow > .1 && strongLow > .1);
        assert(strongLow / softLow < 1.2);
        AcousticExpression filter;
        filter.prepare(rate);
        float previous = 0.0f;
        for (int i = 0; i < 60000; ++i)
        {
            const float breath = i < 30000 ? .08f : .95f;
            const float sample = .5f * static_cast<float>(std::sin(6.28318530718 * 300.0 * i / rate));
            const float out = filter.process(sample, breath, .65f);
            assert(std::isfinite(out));
            if (i == 30000) assert(std::abs(out - previous) < .15f);
            previous = out;
        }
    }
    std::cout << "WindLab acoustic expression smoke: PASS\n";
}
