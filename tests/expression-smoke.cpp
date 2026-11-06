// WindLab 0.9.4: legato entry, natural vibrato and key noises of the JUCE-free engine.
#include "Dsp/WindEngine.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
constexpr double sr = 48000.0;
double hz(int note) { return 440.0 * std::pow(2.0, (note - 69) / 12.0); }
double cents(double f, double reference) { return 1200.0 * std::log2(f / reference); }

struct Trace
{
    std::vector<double> frequency;
    std::vector<float> level, click, audio;
};

Trace run(windlab::WindEngine& e, int samples)
{
    Trace t;
    for (int i = 0; i < samples; ++i)
    {
        t.audio.push_back(e.render());
        t.frequency.push_back(e.currentFrequency());
        t.level.push_back(e.currentLevel());
        t.click.push_back(e.currentClick());
    }
    return t;
}

windlab::WindEngine make(float vibrato, float humanize)
{
    windlab::WindEngine e;
    e.prepare(sr);
    e.setVibrato(vibrato);
    e.setHumanize(humanize);
    e.setTone(.6f);
    return e;
}

double peakToPeakCents(const Trace& t, double from, double to, double nominal)
{
    double lo = 1.0e9, hi = -1.0e9;
    for (int i = static_cast<int>(from * sr); i < static_cast<int>(to * sr); ++i)
    {
        const double c = cents(t.frequency[static_cast<std::size_t>(i)], nominal);
        lo = std::min(lo, c);
        hi = std::max(hi, c);
    }
    return hi - lo;
}

double oscillationHz(const Trace& t, double from, double to, double nominal)
{
    // Frequency from the first to the last upward crossing of the mean (interpolated).
    const int a = static_cast<int>(from * sr), b = static_cast<int>(to * sr);
    double mean = 0.0;
    for (int i = a; i < b; ++i) mean += cents(t.frequency[static_cast<std::size_t>(i)], nominal);
    mean /= (b - a);
    double first = -1.0, last = -1.0, previous = 0.0;
    int count = 0;
    for (int i = a; i < b; ++i)
    {
        const double c = cents(t.frequency[static_cast<std::size_t>(i)], nominal) - mean;
        if (i > a && previous < 0.0 && c >= 0.0)
        {
            const double when = (i - 1 + (-previous) / (c - previous)) / sr;
            if (count++ == 0) first = when;
            last = when;
        }
        previous = c;
    }
    return count < 2 ? 0.0 : (count - 1) / (last - first);
}

float maxAbs(const std::vector<float>& v, double from, double to)
{
    float m = 0.0f;
    for (int i = static_cast<int>(from * sr); i < static_cast<int>(to * sr); ++i)
        m = std::max(m, std::abs(v[static_cast<std::size_t>(i)]));
    return m;
}
}

int main()
{
    using windlab::Instrument;
    using windlab::Articulation;

    // 1. Legato: the new note glides in from the previous pitch, without scoop or new attack.
    {
        auto legato = make(0.0f, 0.0f);
        legato.start(64, .8f, Instrument::AltoSax, .3f, .2f, Articulation::Sustain, hz(60));
        const auto t = run(legato, static_cast<int>(sr * .4));
        assert(std::abs(cents(t.frequency[0], hz(60))) < 25.0);          // starts at the old pitch
        assert(cents(t.frequency[static_cast<std::size_t>(sr * .005)], hz(60)) > 0.0);
        assert(std::abs(cents(t.frequency.back(), hz(64))) < 1.0);       // arrives in tune
        for (std::size_t i = 1; i < t.frequency.size(); ++i)             // monotonic, no wobble
            assert(t.frequency[i] >= t.frequency[i - 1] - 1.0e-9);
        // The breath attack (300 ms) is skipped: the level is almost full after 60 ms.
        const float full = t.level.back();
        assert(t.level[static_cast<std::size_t>(sr * .06)] > .85f * full);

        auto plain = make(0.0f, 0.0f);
        plain.start(64, .8f, Instrument::AltoSax, .3f, .2f, Articulation::Sustain);
        const auto p = run(plain, static_cast<int>(sr * .4));
        assert(std::abs(cents(p.frequency[0], hz(64))) < 1.0);           // no glide, no scoop at humanize 0
        assert(p.level[static_cast<std::size_t>(sr * .06)] < .4f * p.level.back());

        // A staccato note never slurs, even when asked to.
        auto stac = make(0.0f, 0.0f);
        stac.start(64, .8f, Instrument::AltoSax, .01f, .05f, Articulation::Staccato, hz(60));
        assert(std::abs(cents(run(stac, 16).frequency[0], hz(64))) < 1.0);
    }

    // 2. The previous note of a slur fades out quickly.
    {
        auto e = make(0.0f, 0.0f);
        e.start(60, .8f, Instrument::TenorSax, .01f, 3.0f);
        run(e, static_cast<int>(sr * .2));
        e.fastRelease(.05f);
        run(e, static_cast<int>(sr * .07));
        assert(!e.active());
    }

    // 3. Vibrato starts after a short hold, builds up, stays in a natural range and speed.
    {
        auto e = make(1.0f, 0.3f);
        e.start(60, .8f, Instrument::TenorSax, .02f, .2f);
        const auto t = run(e, static_cast<int>(sr * 3.5));
        const double early = peakToPeakCents(t, 0.0, .12, hz(60));
        const double late = peakToPeakCents(t, 1.8, 3.4, hz(60));
        assert(early < 12.0);
        assert(late > 25.0 && late < 90.0);
        const double rate = oscillationHz(t, 1.8, 3.4, hz(60));
        assert(rate > 4.3 && rate < 6.6);

        // Loudness follows the vibrato a little; without vibrato it stays steady.
        auto flat = make(0.0f, 0.0f);
        flat.start(60, .8f, Instrument::TenorSax, .02f, .2f);
        const auto f = run(flat, static_cast<int>(sr * 3.0));
        float fLo = 9, fHi = 0, vLo = 9, vHi = 0;
        for (int i = static_cast<int>(sr * 1.8); i < static_cast<int>(sr * 2.9); ++i)
        {
            fLo = std::min(fLo, f.level[static_cast<std::size_t>(i)]);
            fHi = std::max(fHi, f.level[static_cast<std::size_t>(i)]);
            vLo = std::min(vLo, t.level[static_cast<std::size_t>(i)]);
            vHi = std::max(vHi, t.level[static_cast<std::size_t>(i)]);
        }
        assert((fHi - fLo) / fHi < .02f);
        assert((vHi - vLo) / vHi > .03f);

        // Notes do not all vibrate at the same speed.
        auto a = make(1.0f, 0.0f), b = make(1.0f, 0.0f);
        a.start(60, .8f, Instrument::TenorSax, .02f, .2f);
        b.start(62, .8f, Instrument::TenorSax, .02f, .2f);
        const double ra = oscillationHz(run(a, static_cast<int>(sr * 3.5)), 1.8, 3.4, hz(60));
        const double rb = oscillationHz(run(b, static_cast<int>(sr * 3.5)), 1.8, 3.4, hz(62));
        assert(std::abs(ra - rb) > .02);
    }

    // 4. A slurred note continues a vibrato that is already running.
    {
        auto e = make(1.0f, 0.0f);
        e.start(62, .8f, Instrument::TenorSax, .02f, .2f, Articulation::Sustain, hz(60));
        const auto t = run(e, static_cast<int>(sr * 1.2));
        assert(peakToPeakCents(t, .5, 1.1, hz(62)) > 20.0);
    }

    // 5. Key clicks: synthetic notes and slurs have them; a recorded attack does not get a second one.
    {
        auto synth = make(0.0f, 0.0f);
        synth.start(60, .8f, Instrument::AltoSax, .02f, .2f);
        const auto s = run(synth, static_cast<int>(sr * .03));
        assert(maxAbs(s.click, 0.0, .02) > .001f);

        auto recorded = make(0.0f, 0.0f);
        recorded.setRecordedSource(true);
        recorded.start(60, .8f, Instrument::AltoSax, .02f, .2f);
        assert(maxAbs(run(recorded, static_cast<int>(sr * .03)).click, 0.0, .02) == 0.0f);

        auto slur = make(0.0f, 0.0f);
        slur.setRecordedSource(true);
        slur.start(62, .8f, Instrument::AltoSax, .02f, .2f, Articulation::Sustain, hz(60));
        assert(maxAbs(run(slur, static_cast<int>(sr * .03)).click, 0.0, .02) > .001f);

        // Release click at the end of a note, even for a recorded voice.
        recorded.noteOff();
        assert(maxAbs(run(recorded, static_cast<int>(sr * .02)).click, 0.0, .015) > .0005f);
    }

    // 6. All nine instruments stay finite and bounded with legato, vibrato and clicks.
    for (int i = 0; i <= static_cast<int>(Instrument::BaritoneSax); ++i)
    {
        auto e = make(1.0f, 1.0f);
        e.start(55 + i, 1.0f, static_cast<Instrument>(i), .02f, .1f, Articulation::Sustain, hz(52 + i));
        const auto t = run(e, static_cast<int>(sr * 1.0));
        for (std::size_t n = 0; n < t.audio.size(); ++n)
        {
            assert(std::isfinite(t.audio[n]) && std::abs(t.audio[n]) < 1.5f);
            assert(std::isfinite(t.click[n]) && std::abs(t.click[n]) < .5f);
        }
        assert(maxAbs(t.audio, .3, .9) > .005f);
    }

    std::cout << "WindLab 0.9.4 expression: PASS (legato glide, vibrato, clicks, 9 instruments).\n";
}
