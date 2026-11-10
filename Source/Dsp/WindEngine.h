#pragma once

// WindLab 0.3: allocation-free, JUCE-independent expressive wind tone prototype.
// Band-limited additive synthesis; NOT recordings or physical modelling.
// 0.9.4: slurred (legato) note entry with pitch glide, naturally varying vibrato,
// pitch scoop and key/air noises. The pitch and level also drive the recorded sampler.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace windlab
{
    enum class Instrument : int { Trumpet = 0, TenorSax, Trombone, Flute, Clarinet, FrenchHorn,
                                  SopranoSax, AltoSax, BaritoneSax };
    enum class Articulation : int { Sustain = 0, Staccato, Swell };
    // 0.9.7 jazz effects, chosen separately from the articulation.
    enum class JazzEffect : int { None = 0, Scoop, Fall, Doit, Shake };

    class WindEngine final
    {
    public:
        void prepare(double newSampleRate)
        {
            sampleRate = std::max(8000.0, newSampleRate);
            reset();
            controllerAlpha = static_cast<float>(1.0 - std::exp(-1.0 / (sampleRate * .014)));
            pitchAlpha = 1.0 - std::exp(-1.0 / (sampleRate * .008));
            updateGlide();
            clickDecay = static_cast<float>(std::exp(-1.0 / (sampleRate * .003)));
            updateFilter();
        }

        void reset()
        {
            stage = Stage::Idle;
            env = 0.0f;
            phase = vibratoPhase = elapsed = 0.0;
            driftPhase = .13;
            lowpass = noiseLowpass = 0.0f;
            noiseState = 0x9231abcdu;
            latestLevel = 0.0f;
            latestClick = 0.0f;
            clickEnv = 0.0f;
            glideSemis = 0.0;
            vibratoClock = 0.0;
            effectSemis = 0.0;
            slurDip = 0.0f;
            colourPhase = flutterPhase = 0.0;
            latestFrequency = frequency;
        }

        // legatoFromFrequency > 0 enters the note as a slur from that pitch: no new
        // breath attack, a short pitch glide and a softer fingering click.
        void start(int midiNote, float newVelocity, Instrument newInstrument,
                   float attackSeconds, float releaseSeconds,
                   Articulation newArticulation = Articulation::Sustain,
                   double legatoFromFrequency = 0.0)
        {
            instrument = newInstrument;
            articulation = newArticulation;
            frequency = 440.0 * std::pow(2.0, (std::clamp(midiNote, 0, 127) - 69.0) / 12.0);
            velocity = std::clamp(newVelocity, 0.0f, 1.0f);
            attack = std::clamp(attackSeconds, 0.005f, 3.0f);
            if (articulation == Articulation::Staccato) attack = std::min(attack, 0.012f);
            if (articulation == Articulation::Swell) attack = std::max(attack, 0.55f);
            releaseTime = std::clamp(releaseSeconds, 0.02f, 5.0f);
            if (articulation == Articulation::Staccato) releaseTime = std::min(releaseTime, 0.075f);
            const auto index = static_cast<std::size_t>(instrument);
            legato = legatoFromFrequency > 0.0 && articulation == Articulation::Sustain;
            // Each note gets its own small "personality": vibrato speed, delay and
            // build-up, and how much the pitch scoops in. Deterministic per voice.
            variationState = variationState * 1664525u + 1013904223u
                           + static_cast<std::uint32_t>(midiNote) * 2654435761u;
            const float r1 = random01(), r2 = random01(), r3 = random01();
            vibratoRate = vibratoRates[index] * (.92f + .16f * r1);
            vibratoDelay = .14f + .22f * r2;
            vibratoRamp = .55f + .50f * r3;
            vibratoClock = legato ? 1.5 : 0.0; // a slur continues a vibrato already running
            if (legato)
            {
                attack = std::min(attack, .03f);
                glideSemis = std::clamp(12.0 * std::log2(legatoFromFrequency / frequency), -12.0, 12.0);
                // A real slur loses a little air while the fingers move.
                slurDip = .22f * std::min(1.0f, static_cast<float>(std::abs(glideSemis)) / 5.0f + .45f);
            }
            else
            {
                glideSemis = -.30 * humanize * (.4 + .6 * r1);
                slurDip = 0.0f;
            }
            // Scoop: the note is entered from below like a jazz player; a slur never scoops.
            effectSemis = (jazzEffect == JazzEffect::Scoop && !legato && articulation != Articulation::Staccato)
                        ? -2.6 : 0.0;
            effectClock = 0.0;
            onsetGain = legato ? .30f : 1.0f;
            // Recorded attacks already contain their own onset noises: add a click only
            // to slurred notes (fingers moving) or to synthetic voices.
            clickEnv = (recordedSource && !legato) ? 0.0f
                     : keyClickLevels[index] * (.4f + .6f * velocity) * (.6f + .4f * r2) * (legato ? .8f : 1.0f);
            stage = Stage::Attack;
            env = 0.0f;
            phase = vibratoPhase = elapsed = 0.0;
            lowpass = noiseLowpass = 0.0f;
            currentExpression = targetExpression;
            currentBreath = targetBreath;
            currentPitch = (static_cast<double>(pitchWheel) - 8192.0) / 8192.0;
            currentMod = 0.0f;
            updateFilter();
        }

        void noteOff()
        {
            if (stage != Stage::Idle && stage != Stage::Release)
            {
                stage = Stage::Release;
                float releaseSeconds = releaseTime;
                if ((jazzEffect == JazzEffect::Fall || jazzEffect == JazzEffect::Doit)
                    && articulation != Articulation::Staccato)
                    releaseSeconds = std::max(releaseSeconds, .42f); // room for the fall / doit
                releaseStep = env / static_cast<float>(sampleRate * releaseSeconds);
                releaseClock = 0.0;
                // Lifting a finger/tongue at the end of the note: a faint click.
                clickEnv = std::max(clickEnv, .45f * keyClickLevels[static_cast<std::size_t>(instrument)]
                                               * (.4f + .6f * velocity) * (.6f + .4f * random01()));
            }
        }

        // The previous note of a slur: fade out quickly while the new note takes over.
        void fastRelease(float seconds)
        {
            if (stage == Stage::Idle) return;
            const float step = env / static_cast<float>(sampleRate * std::max(.005f, seconds));
            releaseStep = stage == Stage::Release ? std::max(releaseStep, step) : step;
            stage = Stage::Release;
            releaseClock = -1.0; // a slur hand-over is never a fall or a doit
        }

        // Recorded takes carry their own attack noises; tell the engine so it does
        // not add a second onset click to a non-slurred recorded note.
        void setRecordedSource(bool recorded) { recordedSource = recorded; }

        void setTone(float value)
        {
            const float next = std::clamp(value, 0.0f, 1.0f);
            if (next != tone) { tone = next; updateFilter(); }
        }
        void setAir(float value) { air = std::clamp(value, 0.0f, 1.0f); }
        // At 30 seconds the breath is continuous while MIDI is held.
        // Shorter phrases taper naturally and release at the chosen limit.
        void setBreathLength(float seconds) { breathLength = std::clamp(seconds, 2.0f, 30.0f); }
        void setVibrato(float value) { vibratoAmount = std::clamp(value, 0.0f, 1.0f); }
        void setHumanize(float value) { humanize = std::clamp(value, 0.0f, 1.0f); }
        // Slur glide time in seconds (time constant of the pitch glide between notes).
        void setGlideTime(float seconds)
        {
            const float next = std::clamp(seconds, .004f, .30f);
            if (next != glideTime) { glideTime = next; updateGlide(); }
        }
        void setVibratoRate(float multiplier) { vibratoRateScale = std::clamp(multiplier, .5f, 1.8f); }
        // 0 = fixed vibrato depth; 1 = the vibrato follows the breath (more air, wider vibrato).
        void setVibratoFromBreath(float amount) { vibratoBreath = std::clamp(amount, 0.0f, 1.0f); }
        void setGrowl(float amount) { growl = std::clamp(amount, 0.0f, 1.0f); }
        void setFlutter(float amount) { flutter = std::clamp(amount, 0.0f, 1.0f); }
        void setJazzEffect(JazzEffect effect) { jazzEffect = effect; }
        void setModWheel(float value) { targetMod = std::clamp(value, 0.0f, 1.0f); }
        void setExpression(float value) { targetExpression = std::clamp(value, 0.0f, 1.0f); }
        void setPressure(float value) { pressure = std::clamp(value, 0.0f, 1.0f); }
        void setBreath(float value, bool enabled)
        {
            targetBreath = enabled ? std::clamp(value, 0.0f, 1.0f) : 1.0f;
        }
        void setPitchWheel(int value) { pitchWheel = std::clamp(value, 0, 16383); }
        bool active() const { return stage != Stage::Idle; }
        float currentLevel() const noexcept { return latestLevel; }
        double currentFrequency() const noexcept { return latestFrequency; }
        // Key / fingering click, already scaled; add it to the final mix.
        float currentClick() const noexcept { return latestClick; }

        float render()
        {
            latestClick = 0.0f;
            if (!active()) return 0.0f;
            constexpr double twoPi = 6.283185307179586476925286766559;
            const auto invSampleRate = 1.0 / sampleRate;
            elapsed += invSampleRate;

            if (stage == Stage::Attack)
            {
                env += static_cast<float>(invSampleRate / attack);
                if (env >= 1.0f) { env = 1.0f; stage = Stage::Sustain; }
            }
            else if (stage == Stage::Release)
            {
                releaseClock += invSampleRate;
                env -= releaseStep;
                if (env <= 0.0f) { stage = Stage::Idle; env = 0.0f; return 0.0f; }
            }
            if (articulation == Articulation::Staccato && elapsed >= 0.17 && stage != Stage::Release)
                noteOff();
            if (articulation != Articulation::Staccato && breathLength < 29.99f
                && elapsed >= breathLength && stage != Stage::Release)
                noteOff();

            // Smooth incoming controller automation without allocations or zipper noise.
            currentExpression += controllerAlpha * (targetExpression - currentExpression);
            currentBreath += controllerAlpha * (targetBreath - currentBreath);
            currentMod += controllerAlpha * (targetMod - currentMod);
            const double requestedPitch = (static_cast<double>(pitchWheel) - 8192.0) / 8192.0;
            currentPitch += pitchAlpha * (requestedPitch - currentPitch);

            // Vibrato like a player's: it starts after a short hold, builds up smoothly,
            // speeds up slightly, and its depth wanders a little. Slurs keep it running.
            vibratoClock += invSampleRate;
            const double vibratoProgress = std::clamp((vibratoClock - vibratoDelay) / vibratoRamp, 0.0, 1.0);
            const double vibratoFade = vibratoProgress * vibratoProgress * (3.0 - 2.0 * vibratoProgress);
            const double rateProgress = std::clamp((vibratoClock - vibratoDelay) / 1.1, 0.0, 1.0);
            const double drift = humanize * .025 * (std::sin(twoPi * driftPhase)
                                 + .35 * std::sin(twoPi * driftPhase * 1.73));
            const float breathDepth = (1.0f - vibratoBreath) + vibratoBreath * std::min(1.4f, 1.4f * currentBreath);
            const double vibratoDepth = std::min(1.0f, (vibratoAmount * .48f + currentMod) * breathDepth);
            const double depthWander = 1.0 + .16 * std::sin(twoPi * driftPhase * .71);
            const double vibratoSemis = .24 * vibratoDepthScales[static_cast<std::size_t>(instrument)]
                                      * vibratoDepth * vibratoFade * depthWander * std::sin(twoPi * vibratoPhase);
            glideSemis *= glideDecay;
            if (std::abs(glideSemis) < 1.0e-4) glideSemis = 0.0;
            slurDip *= static_cast<float>(glideDecay);
            // Jazz effects move the pitch: scoop rises into the note, fall / doit bend
            // away while the note is released, shake is a fast lip trill.
            effectClock += invSampleRate;
            double effectOffset = 0.0;
            switch (jazzEffect)
            {
                case JazzEffect::Scoop:
                    effectSemis *= scoopDecay;
                    effectOffset = effectSemis;
                    break;
                case JazzEffect::Fall:
                case JazzEffect::Doit:
                    if (stage == Stage::Release && releaseClock >= 0.0 && articulation != Articulation::Staccato)
                    {
                        const double t = std::min(1.0, releaseClock / .42);
                        effectOffset = (jazzEffect == JazzEffect::Fall ? -7.0 : 5.0) * t * t;
                    }
                    break;
                case JazzEffect::Shake:
                {
                    const double shakeIn = std::clamp((effectClock - .22) / .18, 0.0, 1.0);
                    effectOffset = 1.55 * shakeIn * (1.0 + std::sin(twoPi * 6.8 * effectClock - 1.5707963));
                    break;
                }
                case JazzEffect::None: break;
            }
            const double f = frequency * std::exp2((2.0 * currentPitch + vibratoSemis + drift + glideSemis + effectOffset) / 12.0);
            latestFrequency = f;
            phase += f * invSampleRate;
            phase -= std::floor(phase);
            vibratoPhase += vibratoRate * vibratoRateScale * (.86 + .14 * rateProgress)
                          * (1.0 + .04 * humanize * std::sin(twoPi * driftPhase)) * invSampleRate;
            vibratoPhase -= std::floor(vibratoPhase);
            driftPhase += .63 * invSampleRate;
            driftPhase -= std::floor(driftPhase);

            // Partial frequencies above 45% of Fs are omitted instead of aliasing like a naive saw wave.
            const int limit = std::min(static_cast<int>(spectralWeights.size()),
                                       std::max(1, static_cast<int>(sampleRate * .45 / f)));
            // A rising breath/pressure brightens the harmonic spectrum instead of
            // only turning the volume up. No allocations or pow() inside this loop.
            const float spectralSlope = std::clamp(.91f + .10f * currentBreath
                                                  + .045f * pressure, .86f, 1.055f);
            float sound = 0.0f;
            float harmonicGain = 1.0f;
            for (int h = 1; h <= limit; ++h)
            {
                sound += spectralWeights[static_cast<std::size_t>(h - 1)] * harmonicGain
                       * static_cast<float>(std::sin(twoPi * h * phase));
                harmonicGain *= spectralSlope;
            }
            sound /= spectralNormalizer;
            if (f > sampleRate * .45) sound = 0.0f;

            noiseState = noiseState * 1664525u + 1013904223u;
            const float rawNoise = static_cast<float>((noiseState >> 8) & 0xffffffu) / 8388607.5f - 1.0f;
            noiseLowpass += .065f * (rawNoise - noiseLowpass);
            const float hiss = rawNoise - noiseLowpass;
            constexpr std::array<float, 9> instrumentBreath { .025f, .060f, .024f, .085f, .052f, .016f,
                                                              .058f, .060f, .062f };
            const float noiseLevel = instrumentBreath[static_cast<std::size_t>(instrument)]
                                    * (.22f + 1.4f * air);
            const float onset = static_cast<float>(std::exp(-elapsed / .028)) * onsetGain;
            const float combined = (sound * (1.0f + onset * .14f)
                                   + hiss * noiseLevel * (.38f + .85f * currentBreath + .65f * onset))
                                   * (.8f + .2f * velocity + .12f * pressure);
            lowpass += filterWeight * (combined - lowpass);
            const float pressureGain = .92f + .08f * pressure;
            const float v = (.20f + .80f * velocity) * velocity;
            // An approaching end of breath causes a gradual taper rather than
            // chopping a sustained phrase. 30 s means no automatic taper.
            float breathEnvelope = 1.0f;
            if (breathLength < 29.99f && articulation != Articulation::Staccato
                && stage != Stage::Release)
            {
                const double taper = std::min(1.8, static_cast<double>(breathLength) * .20);
                breathEnvelope = static_cast<float>(1.0 - .48 *
                    std::clamp((elapsed - (breathLength - taper)) / taper, 0.0, 1.0));
            }
            // The pitch wobble of a vibrato comes with a smaller loudness wobble.
            const float vibratoLevel = 1.0f + .055f * static_cast<float>(vibratoDepth * vibratoFade
                                              * std::sin(twoPi * vibratoPhase + 1.1));
            const float naturalMotion = vibratoLevel * (1.0f + .022f * humanize
                * static_cast<float>(std::sin(twoPi * driftPhase * 1.31)
                                     + .35 * std::sin(twoPi * driftPhase * 2.29)));
            clickEnv *= clickDecay;
            latestClick = clickEnv * hiss * 2.0f;
            // Growl (singing/humming into the instrument) = fast irregular roughness;
            // flutter tongue (rolled R) = deeper, slower tremolo with air bursts.
            float colour = 1.0f;
            float roughNoise = 0.0f;
            if (growl > 0.0f || flutter > 0.0f)
            {
                colourPhase += (34.0 + 9.0 * std::sin(twoPi * driftPhase * 3.1)) * invSampleRate;
                colourPhase -= std::floor(colourPhase);
                flutterPhase += 21.0 * invSampleRate;
                flutterPhase -= std::floor(flutterPhase);
                const float growlWave = static_cast<float>(std::sin(twoPi * colourPhase)) * (.7f + .3f * rawNoise);
                const float flutterWave = .5f + .5f * static_cast<float>(std::sin(twoPi * flutterPhase));
                colour = (1.0f + .42f * growl * growlWave) * (1.0f - .62f * flutter * flutterWave);
                roughNoise = hiss * (.10f * growl * std::abs(growlWave) + .16f * flutter * flutterWave);
            }
            latestLevel = env * v * currentExpression * currentBreath
                        * pressureGain * breathEnvelope * naturalMotion * colour * (1.0f - slurDip) * .24f;
            latestClick += roughNoise * env * v * currentBreath * .5f;
            return lowpass * latestLevel;
        }

    private:
        enum class Stage { Idle, Attack, Sustain, Release };
        void updateFilter()
        {
            constexpr double twoPi = 6.283185307179586476925286766559;
            const double cutoff = std::min(sampleRate * .45, 1000.0 + 11000.0 * tone * tone);
            filterWeight = static_cast<float>(1.0 - std::exp(-twoPi * cutoff / sampleRate));
            const auto& partials = harmonicProfiles[static_cast<std::size_t>(instrument)];
            const float brightness = .16f + .84f * tone;
            float sum = 0.0f;
            for (std::size_t i = 0; i < partials.size(); ++i)
            {
                spectralWeights[i] = partials[i] * (i == 0 ? 1.0f : std::pow(brightness, .38f * static_cast<float>(i)));
                sum += spectralWeights[i];
            }
            spectralNormalizer = std::max(.35f, sum * .8f);
        }
        // trumpet, tenor sax, trombone, flute, clarinet, horn, soprano / alto / baritone sax
        static constexpr std::array<float, 9> vibratoRates { 5.0f, 5.4f, 4.8f, 5.6f, 5.0f, 4.9f, 5.5f, 5.4f, 5.2f };
        static constexpr std::array<double, 9> vibratoDepthScales { .85, 1.0, .8, 1.0, .6, .75, 1.05, 1.0, .95 };
        static constexpr std::array<float, 9> keyClickLevels { .012f, .05f, .004f, .035f, .05f, .006f, .045f, .05f, .055f };
        static constexpr std::array<std::array<float, 12>, 9> harmonicProfiles {{
            {{ 1.f, .85f, .72f, .59f, .48f, .38f, .25f, .18f, .11f, .07f, .04f, .02f }}, // trumpet
            {{ 1.f, .45f, .83f, .28f, .56f, .18f, .34f, .14f, .18f, .06f, .08f, .04f }}, // tenor sax
            {{ 1.f, .73f, .48f, .35f, .21f, .14f, .08f, .06f, .03f, .02f, .01f, .00f }}, // trombone
            {{ 1.f, .12f, .035f,.02f, .010f,.00f, .00f, .00f, .00f, .00f, .00f, .00f }}, // flute
            {{ 1.f, .02f, .82f, .015f,.39f, .01f, .23f, .005f,.10f, .00f, .05f, .00f }}, // clarinet
            {{ 1.f, .64f, .36f, .20f, .10f, .06f, .035f,.025f,.015f,.01f, .01f, .00f }}, // horn
            {{ 1.f, .55f, .78f, .36f, .50f, .24f, .32f, .16f, .16f, .08f, .07f, .04f }}, // soprano sax
            {{ 1.f, .48f, .82f, .30f, .54f, .20f, .33f, .15f, .17f, .07f, .08f, .04f }}, // alto sax
            {{ 1.f, .40f, .80f, .24f, .55f, .15f, .30f, .12f, .18f, .05f, .08f, .03f }}  // baritone sax
        }};
        Instrument instrument { Instrument::Trumpet };
        Articulation articulation { Articulation::Sustain };
        Stage stage { Stage::Idle };
        double sampleRate = 44100.0, frequency = 440.0, phase = 0.0, vibratoPhase = 0.0, elapsed = 0.0, driftPhase = .13;
        double currentPitch = 0.0;
        float velocity = 0.0f, env = 0.0f, attack = .04f, releaseTime = .35f, releaseStep = 0.0f;
        float tone = .55f, air = .25f, breathLength = 30.0f, vibratoAmount = .0f, humanize = .3f;
        float targetMod = 0.0f, currentMod = 0.0f;
        float targetExpression = 1.0f, currentExpression = 1.0f;
        float targetBreath = 1.0f, currentBreath = 1.0f, pressure = 0.0f;
        float filterWeight = .2f, lowpass = 0.0f, noiseLowpass = 0.0f;
        float controllerAlpha = .002f;
        double pitchAlpha = .002;
        // 0.9.4 slur / vibrato / noise state
        bool legato = false, recordedSource = false;
        double glideSemis = 0.0, glideDecay = .999, vibratoClock = 0.0;
        float vibratoRate = 5.2f, vibratoDelay = .2f, vibratoRamp = .7f, onsetGain = 1.0f;
        float clickEnv = 0.0f, clickDecay = .99f, latestClick = 0.0f;
        std::uint32_t variationState = 0x1234567u;
        float random01()
        {
            variationState = variationState * 1664525u + 1013904223u;
            return static_cast<float>((variationState >> 8) & 0xffffffu) / 16777216.0f;
        }
        void updateGlide()
        {
            glideDecay = std::exp(-1.0 / (sampleRate * glideTime));
            scoopDecay = std::exp(-1.0 / (sampleRate * .075));
        }
        // 0.9.7 expression state
        float glideTime = .022f, vibratoRateScale = 1.0f, vibratoBreath = 0.0f, growl = 0.0f, flutter = 0.0f;
        float slurDip = 0.0f;
        JazzEffect jazzEffect { JazzEffect::None };
        double effectSemis = 0.0, effectClock = 0.0, releaseClock = 0.0, scoopDecay = .999;
        double colourPhase = 0.0, flutterPhase = 0.0;
        std::array<float, 12> spectralWeights{};
        float spectralNormalizer = 1.0f;
        double latestFrequency = 440.0;
        float latestLevel = 0.0f;
        int pitchWheel = 8192;
        std::uint32_t noiseState = 0x9231abcdu;
    };
}
