#include "Dsp/RecordedRelease.h"
#include "Dsp/PhraseBreathEnvelope.h"
#include "Dsp/WindEngine.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    using namespace windlab;
    constexpr double twoPi = 6.28318530717958647692;
    for (double sr : {44100.0, 48000.0, 96000.0})
    {
        const int count = static_cast<int>(sr * 2.0);
        const int loopEnd = static_cast<int>(sr * 1.5);
        std::vector<float> audio(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i)
        {
            const double t = i / sr;
            const double releaseProgress = std::clamp((t - 1.78) / .22, 0.0, 1.0);
            const double gain = .55 * std::exp(-6.0 * releaseProgress);
            audio[static_cast<std::size_t>(i)] =
                static_cast<float>(gain * std::sin(twoPi * 220.0 * t));
        }
        const auto plan = detectRecordedRelease(audio.data(), count, sr, loopEnd);
        assert(plan.valid);
        assert(plan.start > loopEnd);
        assert(plan.end == count - 2);
        assert(plan.end - plan.start >= sr * .18);

        RecordedReleaseTransition fade;
        fade.prepare(sr);
        // No detected ending means the existing sustain sample is untouched.
        assert(fade.mix(.5f, -.2f, 1.0) == .5f);
        assert(!fade.begin({}, sr));
        assert(!fade.active());
        assert(fade.begin(plan, sr));
        const double originalStart = fade.sourcePosition();
        assert(!fade.begin(plan, sr));
        assert(fade.sourcePosition() == originalStart); // idempotent

        float last = .5f;
        for (int i = 0; i < static_cast<int>(sr * .027); ++i)
        {
            const float output = fade.mix(.5f, .2f, 1.0);
            assert(std::isfinite(output) && output >= 0.0f && output <= .5f);
            assert(std::abs(output - last) < .002f);
            last = output;
        }
        assert(last < .22f);
        for (int i = 0; i < static_cast<int>(sr * .40); ++i)
            last = fade.mix(.5f, .2f, 1.0);
        assert(fade.finished());
        assert(last == 0.0f);
        assert(fade.mix(.5f, .2f, 1.0) == 0.0f);
        fade.reset();
        assert(fade.mix(.5f, .2f, 1.0) == .5f);

        // A sustained recording with no audible ending must not be mistaken
        // for a natural phrase release, and its existing loop is preserved.
        std::vector<float> constant(static_cast<std::size_t>(count), .36f);
        assert(!detectRecordedRelease(constant.data(), count, sr, loopEnd).valid);
        std::vector<float> silence(static_cast<std::size_t>(count), 0.0f);
        assert(!detectRecordedRelease(silence.data(), count, sr, loopEnd).valid);
        assert(!detectRecordedRelease(nullptr, count, sr, loopEnd).valid);
        assert(!detectRecordedRelease(audio.data(), count, 0.0, loopEnd).valid);
        assert(!detectRecordedRelease(audio.data(), count, sr, count - 2).valid);
        assert(!detectRecordedRelease(audio.data(), static_cast<int>(sr * .2), sr, loopEnd).valid);

        // Genuine note-off starts the phrase release. Interval-legato fast
        // release never starts a separate recorded ending.
        WindEngine engine;
        engine.prepare(sr);
        engine.start(60, .9f, Instrument::TenorSax, .015f, .3f);
        for (int i = 0; i < static_cast<int>(sr * .08); ++i) engine.render();
        assert(!engine.releasingPhraseNaturally());
        engine.noteOff();
        assert(engine.releasingPhraseNaturally());
        engine.fastRelease(.04f);
        assert(!engine.releasingPhraseNaturally());
        engine.start(62, .9f, Instrument::TenorSax, .015f, .3f);
        assert(!engine.releasingPhraseNaturally());
        engine.noteOff();
        assert(engine.releasingPhraseNaturally());
        engine.reset();
        assert(!engine.releasingPhraseNaturally());

        // Breath-length exhaustion, like an explicit note-off, can use the
        // recorded ending rather than playing the sustain loop indefinitely.
        engine.prepare(sr);
        engine.setBreathLength(2.0f);
        engine.start(60, .8f, Instrument::Flute, .015f, .3f);
        for (int i = 0; i < static_cast<int>(sr * 2.02); ++i) engine.render();
        assert(engine.releasingPhraseNaturally());
    }
    // Phrase breath falls continuously to a lower air level and never
    // restores full gain when a note enters its release stage.
    for (int instrument = 0; instrument < 9; ++instrument)
    {
        float previous = 1.0f;
        for (int i = 0; i <= 100; ++i)
        {
            const auto shape = phraseBreathShape(1.6 + .004 * i, 2.0f, instrument, false);
            assert(shape.level > .45f && shape.level <= previous + 1.0e-6f);
            assert(shape.vibratoScale >= .71f && shape.vibratoScale <= 1.0f);
            previous = shape.level;
        }
        const auto finished = phraseBreathShape(2.5, 2.0f, instrument, false);
        assert(std::abs(finished.level - previous) < 1.0e-6f);
        assert(phraseBreathShape(2.0, 30.0f, instrument, false).level == 1.0f);
        assert(phraseBreathShape(2.0, 2.0f, instrument, true).level == 1.0f);
    }

    // Regression: the short-breath auto note-off may not double the loudness
    // by clearing the previous taper when Release begins.
    WindEngine e;
    e.prepare(48000.0);
    e.setHumanize(0.0f);
    e.setVibrato(0.0f);
    e.setBreathLength(2.0f);
    e.start(60, .9f, Instrument::TenorSax, .01f, .4f);
    for (int i = 0; i < static_cast<int>(48000 * 2.0) - 2; ++i) e.render();
    const float justBefore = e.currentLevel();
    for (int i = 0; i < 8; ++i) e.render();
    const float justAfter = e.currentLevel();
    assert(e.releasingPhraseNaturally());
    assert(justBefore > 0.0f && justAfter > 0.0f);
    assert(justAfter <= justBefore * 1.025f);

    std::cout << "WindLab recorded acoustic release 0.9.17: PASS\\n";
}
