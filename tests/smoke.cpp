#include "Dsp/WindEngine.h"
#include "Dsp/ToneEQ.h"
#include "Dsp/VelocityResponse.h"
#include "Dsp/SampleLoopPlan.h"
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
    // Breath length can end a phrase without hard-cutting a note;
    // the maximum setting is intentionally indefinite while MIDI is held.
    windlab::WindEngine phrase;
    phrase.prepare(48000.0);
    phrase.setBreathLength(2.0f);
    phrase.start(67, .8f, windlab::Instrument::Trumpet, .01f, .12f);
    assert(meanAbs(phrase, 24000) > .001f);
    meanAbs(phrase, 120000);
    assert(!phrase.active());

    windlab::WindEngine continuous;
    continuous.prepare(48000.0);
    continuous.setBreathLength(30.0f);
    continuous.setHumanize(.8f);
    continuous.setVibrato(.55f);
    continuous.start(67, .8f, windlab::Instrument::FrenchHorn, .01f, .12f);
    assert(meanAbs(continuous, 144000) > .001f);
    assert(continuous.active()); // 3s exceeds the short phrase, not the continuous phrase.
    continuous.noteOff();
    meanAbs(continuous, 24000);
    assert(!continuous.active());

    // Three-band EQ: bounded output and smooth coefficient changes at all rates.
    for (const auto rate : {44100.0, 48000.0, 96000.0})
    {
        windlab::ToneEQ eq;
        eq.prepare(rate);
        eq.setGains(12.0f, -7.0f, 8.0f);
        for (int i = 0; i < 120000; ++i)
        {
            if (i == 60000) eq.setGains(-12.0f, 6.0f, -12.0f);
            const float input = .07f * static_cast<float>(std::sin(i * 440.0 * 6.28318530718 / rate));
            const float left = eq.process(0, input);
            const float right = eq.process(1, input);
            assert(std::isfinite(left) && std::isfinite(right));
            assert(std::abs(left) < 1.0f && std::abs(right) < 1.0f);
        }
    }
    // MIDI controller response: preserve original velocity unless user opts in.
    using windlab::applyVelocityResponse;
    assert(std::abs(applyVelocityResponse(.4f, 0) - .4f) < 1e-6f);
    assert(applyVelocityResponse(.25f, 1) > .25f); // light keys become more expressive
    assert(applyVelocityResponse(.25f, 2) < .25f); // weighted key dynamic resolution
    assert(applyVelocityResponse(.25f, 3) > .7f);  // CC2 provides the breath dynamic
    assert(applyVelocityResponse(.0f, 3) == .0f); // velocity-zero still means note off
    for (int profile = 0; profile < 4; ++profile)
    {
        float previous = 0.0f;
        for (int n = 0; n <= 127; ++n)
        {
            const float v = applyVelocityResponse(n / 127.0f, profile);
            assert(std::isfinite(v) && v >= 0.0f && v <= 1.0f);
            assert(v + 1e-6f >= previous);
            previous = v;
        }
        assert(applyVelocityResponse(-4.0f, profile) == 0.0f);
        assert(applyVelocityResponse(4.0f, profile) == 1.0f);
    }

    // Real WAV sustain regions are aligned to nearby rising zero crossings.
    std::array<float, 48000> syntheticSample {};
    for (std::size_t i = 0; i < syntheticSample.size(); ++i)
        syntheticSample[i] = .6f * static_cast<float>(
            std::sin(6.28318530718 * 440.0 * static_cast<double>(i) / 48000.0));
    const auto plan = windlab::chooseSampleLoop(syntheticSample.data(),
                                                 static_cast<int>(syntheticSample.size()), 48000.0);
    assert(plan.valid);
    assert(plan.start > 12000 && plan.start < 15000);
    assert(plan.end > 34500 && plan.end < 37500);
    assert(plan.fade >= 16 && plan.end > plan.start + plan.fade * 3);
    assert(syntheticSample[static_cast<std::size_t>(plan.start - 1)] <= 0.0f);
    assert(syntheticSample[static_cast<std::size_t>(plan.start)] > 0.0f);
    assert(syntheticSample[static_cast<std::size_t>(plan.end - 1)] <= 0.0f);
    assert(syntheticSample[static_cast<std::size_t>(plan.end)] > 0.0f);
    assert(!windlab::chooseSampleLoop(nullptr, 1000, 48000.0).valid);

    // Authentic staccato one-shots must approach silence without hard cuts.
    assert(windlab::oneShotTailGain(0.0, 48000, 48000.0) == 1.0f);
    assert(windlab::oneShotTailGain(48000 - 2, 48000, 48000.0) == 0.0f);
    assert(windlab::oneShotTailGain(48000 - 2 - 336, 48000, 48000.0) > .45f);
    assert(windlab::oneShotTailGain(48000 - 2 - 336, 48000, 48000.0) < .55f);

    std::cout << "WindLab 0.9 DSP: PASS (winds, breath length, natural variation, EQ, controllers, 3 rates).\\n";
}
