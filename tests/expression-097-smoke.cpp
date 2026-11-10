// WindLab 0.9.7: glide time, slur air dip, breath-driven vibrato, growl, flutter
// tongue and jazz effects (scoop, fall, doit, shake) of the JUCE-free engine.
#include "Dsp/WindEngine.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

using namespace windlab;

namespace
{
constexpr double sr = 48000.0;
double hz(int note) { return 440.0 * std::pow(2.0, (note - 69) / 12.0); }
double semis(double f, double reference) { return 12.0 * std::log2(f / reference); }

WindEngine fresh()
{
    WindEngine e;
    e.prepare(sr);
    e.setHumanize(0.0f);
    e.setVibrato(0.0f);
    return e;
}

void renderFor(WindEngine& e, double seconds)
{
    for (int i = 0; i < static_cast<int>(seconds * sr); ++i) e.render();
}
}

int main()
{
    // --- Glide time: a slower legato is still far from the target after 40 ms.
    {
        auto fast = fresh();
        fast.setGlideTime(.01f);
        fast.start(62, .8f, Instrument::AltoSax, .02f, .3f, Articulation::Sustain, hz(55));
        auto slow = fresh();
        slow.setGlideTime(.20f);
        slow.start(62, .8f, Instrument::AltoSax, .02f, .3f, Articulation::Sustain, hz(55));
        renderFor(fast, .04);
        renderFor(slow, .04);
        const double fastOff = std::abs(semis(fast.currentFrequency(), hz(62)));
        const double slowOff = std::abs(semis(slow.currentFrequency(), hz(62)));
        assert(fastOff < .3 && slowOff > 2.0);
    }

    // --- Slur air dip: right after a slur the level is lower than a moment later.
    {
        auto e = fresh();
        e.setGlideTime(.03f);
        e.start(64, .8f, Instrument::Flute, .01f, .3f, Articulation::Sustain, hz(60));
        renderFor(e, .035);
        const float early = e.currentLevel();
        renderFor(e, .25);
        const float later = e.currentLevel();
        assert(later > early * 1.02f);
    }

    // --- Scoop: the note starts about 2.6 semitones low and arrives within ~0.4 s.
    {
        auto e = fresh();
        e.setJazzEffect(JazzEffect::Scoop);
        e.start(67, .8f, Instrument::Trumpet, .02f, .3f);
        e.render();
        assert(semis(e.currentFrequency(), hz(67)) < -2.0);
        renderFor(e, .4);
        assert(std::abs(semis(e.currentFrequency(), hz(67))) < .05);
    }

    // --- Fall and doit: pitch bends during the release, not before.
    for (const auto effect : {JazzEffect::Fall, JazzEffect::Doit})
    {
        auto e = fresh();
        e.setJazzEffect(effect);
        e.start(70, .9f, Instrument::Trumpet, .01f, .05f);
        renderFor(e, .3);
        assert(std::abs(semis(e.currentFrequency(), hz(70))) < .05);
        e.noteOff();
        renderFor(e, .3);
        assert(e.active()); // release stretched so the fall can be heard
        const double bend = semis(e.currentFrequency(), hz(70));
        if (effect == JazzEffect::Fall) assert(bend < -2.0);
        else assert(bend > 1.5);
    }

    // --- A slur hand-over (fastRelease) never falls.
    {
        auto e = fresh();
        e.setJazzEffect(JazzEffect::Fall);
        e.start(70, .9f, Instrument::Trumpet, .01f, .3f);
        renderFor(e, .2);
        e.fastRelease(.05f);
        renderFor(e, .02);
        assert(std::abs(semis(e.currentFrequency(), hz(70))) < .05);
    }

    // --- Shake: after its delay the pitch swings up to about a minor third.
    {
        auto e = fresh();
        e.setJazzEffect(JazzEffect::Shake);
        e.start(65, .9f, Instrument::Trumpet, .01f, .3f);
        double highest = -10.0;
        for (int i = 0; i < static_cast<int>(.15 * sr); ++i) { e.render(); highest = std::max(highest, semis(e.currentFrequency(), hz(65))); }
        assert(highest < .1);
        for (int i = 0; i < static_cast<int>(.6 * sr); ++i) { e.render(); highest = std::max(highest, semis(e.currentFrequency(), hz(65))); }
        assert(highest > 2.6 && highest < 3.4);
    }

    // --- Growl and flutter modulate the level; none = steady.
    auto levelSpread = [](float growlAmount, float flutterAmount)
    {
        auto e = fresh();
        e.setGrowl(growlAmount);
        e.setFlutter(flutterAmount);
        e.start(60, .9f, Instrument::TenorSax, .01f, .3f);
        renderFor(e, .3);
        float lo = 10.0f, hi = 0.0f;
        for (int i = 0; i < static_cast<int>(.2 * sr); ++i)
        {
            e.render();
            lo = std::min(lo, e.currentLevel());
            hi = std::max(hi, e.currentLevel());
        }
        return (hi - lo) / hi;
    };
    const float steady = levelSpread(0.0f, 0.0f);
    assert(steady < .02f);
    assert(levelSpread(1.0f, 0.0f) > .3f);
    assert(levelSpread(0.0f, 1.0f) > .45f);

    // --- Vibrato from breath: with little breath the vibrato is narrower.
    auto vibratoWidth = [](float breath)
    {
        auto e = fresh();
        e.setVibrato(1.0f);
        e.setVibratoFromBreath(1.0f);
        e.setBreath(breath, true);
        e.start(69, .8f, Instrument::AltoSax, .01f, .3f);
        renderFor(e, 2.0);
        double lo = 99.0, hi = -99.0;
        for (int i = 0; i < static_cast<int>(.5 * sr); ++i)
        {
            e.render();
            const double s = semis(e.currentFrequency(), hz(69));
            lo = std::min(lo, s);
            hi = std::max(hi, s);
        }
        return hi - lo;
    };
    assert(vibratoWidth(1.0f) > vibratoWidth(.2f) * 2.0);

    // --- 0.9.9 unison detune: a doubler plays a few cents off.
    {
        auto e = fresh();
        e.setDetune(.09);
        e.start(69, .8f, Instrument::Flute, .01f, .3f);
        renderFor(e, .2);
        const double cents = 100.0 * semis(e.currentFrequency(), 440.0);
        assert(cents > 8.0 && cents < 10.0);
    }

    std::cout << "WindLab 0.9.7 expression smoke test: PASS\n";
    return 0;
}
