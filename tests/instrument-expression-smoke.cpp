// WindLab 0.9.16: acoustic timbre and instrument-specific expression regressions.
#include "Dsp/InstrumentExpressionProfiles.h"
#include "Dsp/AcousticExpression.h"
#include "Dsp/WindEngine.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

namespace
{
constexpr double twoPi = 6.28318530717958647692;
double rms(int instrument, double rate, double hz, float breath)
{
    windlab::AcousticExpression filter;
    filter.prepare(rate);
    filter.setInstrument(instrument);
    filter.reset(breath, .55f, false);
    const int size = static_cast<int>(rate * .30);
    double power = 0.0;
    for (int i = 0; i < size; ++i)
    {
        const float input = .5f * static_cast<float>(std::sin(twoPi * hz * i / rate));
        const float output = filter.process(input, breath, .55f);
        assert(std::isfinite(output));
        assert(std::abs(output) <= .501f);
        if (i >= size / 2) power += static_cast<double>(output) * output;
    }
    return std::sqrt(power / static_cast<double>(size - size / 2));
}
double earlyAttackPower(bool slurred)
{
    constexpr double rate = 48000.0;
    windlab::AcousticExpression filter;
    filter.prepare(rate);
    filter.setInstrument(static_cast<int>(windlab::Instrument::Trumpet));
    filter.reset(.08f, .40f, slurred);
    double power = 0.0;
    for (int i = 0; i < static_cast<int>(rate * .009); ++i)
    {
        const float sample = .5f * static_cast<float>(std::sin(twoPi * 9000.0 * i / rate));
        const float output = filter.process(sample, .08f, .40f);
        power += static_cast<double>(output) * output;
    }
    return power;
}
}

int main()
{
    using namespace windlab;
    static_assert(instrumentExpressionProfiles.size() == 9, "One profile per instrument");
    const auto& trumpet = instrumentExpressionProfile(static_cast<int>(Instrument::Trumpet));
    const auto& flute = instrumentExpressionProfile(static_cast<int>(Instrument::Flute));
    const auto& clarinet = instrumentExpressionProfile(static_cast<int>(Instrument::Clarinet));
    assert(trumpet.vibratoDelayScale < clarinet.vibratoDelayScale);
    assert(flute.softCutoffHz > clarinet.softCutoffHz);
    assert(instrumentExpressionProfile(-1).softCutoffHz == trumpet.softCutoffHz);
    assert(instrumentExpressionProfile(200).softCutoffHz ==
           instrumentExpressionProfile(8).softCutoffHz);

    for (int i = 0; i < 9; ++i)
    {
        const auto& p = instrumentExpressionProfile(i);
        assert(p.softCutoffHz >= 400.0f && p.softCutoffHz < p.strongCutoffHz);
        assert(p.strongCutoffHz < 20000.0f);
        assert(p.breathColour > 0.0f && p.toneColour > 0.0f);
        assert(p.vibratoDelayScale > 0.75f && p.vibratoDelayScale < 1.5f);
        assert(p.vibratoRampScale > 0.75f && p.vibratoRampScale < 1.5f);
        assert(p.transientMs > 2.0f && p.transientMs < 20.0f);
        assert(p.transientDryMix > 0.0f && p.transientDryMix < .25f);
    }

    for (double rate : {44100.0, 48000.0, 96000.0})
    {
        const double darkClarinet = rms(4, rate, 6500.0, .12f);
        const double darkFlute = rms(3, rate, 6500.0, .12f);
        const double brightClarinet = rms(4, rate, 6500.0, .95f);
        const double brightFlute = rms(3, rate, 6500.0, .95f);
        assert(darkFlute > darkClarinet * 1.04);
        assert(brightFlute > brightClarinet * 1.04);
        assert(brightClarinet > darkClarinet * 1.10);
        assert(brightFlute > darkFlute * 1.10);
        assert(rms(4, rate, 300.0, .12f) > .10);

        // Sample rate changes must retain valid bounds for every instrument.
        for (int i = 0; i < 9; ++i)
        {
            const double high = rms(i, rate, 7200.0, .90f);
            assert(std::isfinite(high) && high > 0.0 && high <= .36);
        }
    }

    // A non-legato recorded note briefly retains its genuine high-frequency
    // tongue transient, whereas an already slurred sustain must not add one.
    assert(earlyAttackPower(false) > earlyAttackPower(true) * 1.02);

    // Existing synthetic engine still accepts all instruments and vibrato.
    for (int i = 0; i < 9; ++i)
    {
        WindEngine engine;
        engine.prepare(48000.0);
        engine.setVibrato(1.0f);
        engine.start(62, .85f, static_cast<Instrument>(i), .015f, .2f);
        for (int sample = 0; sample < 48000; ++sample)
        {
            const float v = engine.render();
            assert(std::isfinite(v) && std::abs(v) < 1.5f);
            assert(std::isfinite(engine.currentFrequency()) && engine.currentFrequency() > 0.0);
        }
    }
    std::cout << "WindLab instrument expression 0.9.16: PASS\n";
}
