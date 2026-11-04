#pragma once

// WindLab 0.3: allocation-free, JUCE-independent expressive wind tone prototype.
// Band-limited additive synthesis; NOT recordings or physical modelling.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace windlab
{
    enum class Instrument : int { Trumpet = 0, TenorSax, Trombone, Flute, Clarinet, FrenchHorn };
    enum class Articulation : int { Sustain = 0, Staccato, Swell };

    class WindEngine final
    {
    public:
        void prepare(double newSampleRate)
        {
            sampleRate = std::max(8000.0, newSampleRate);
            reset();
            controllerAlpha = static_cast<float>(1.0 - std::exp(-1.0 / (sampleRate * .014)));
            pitchAlpha = 1.0 - std::exp(-1.0 / (sampleRate * .008));
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
            latestFrequency = frequency;
        }

        void start(int midiNote, float newVelocity, Instrument newInstrument,
                   float attackSeconds, float releaseSeconds,
                   Articulation newArticulation = Articulation::Sustain)
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
                releaseStep = env / static_cast<float>(sampleRate * releaseTime);
            }
        }

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

        float render()
        {
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

            // Gently delayed vibrato plus independent, tiny slow player drift.
            const float vibratoFade = static_cast<float>(std::clamp((elapsed - .10) / .32, 0.0, 1.0));
            const double drift = humanize * .025 * (std::sin(twoPi * driftPhase)
                                 + .35 * std::sin(twoPi * driftPhase * 1.73));
            const double vibratoDepth = std::min(1.0f, vibratoAmount * .48f + currentMod);
            const double vibratoSemis = .22 * vibratoDepth * vibratoFade * std::sin(twoPi * vibratoPhase);
            const double f = frequency * std::exp2((2.0 * currentPitch + vibratoSemis + drift) / 12.0);
            latestFrequency = f;
            phase += f * invSampleRate;
            phase -= std::floor(phase);
            vibratoPhase += (5.15 + .35 * humanize * std::sin(twoPi * driftPhase)) * invSampleRate;
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
            constexpr std::array<float, 6> instrumentBreath { .025f, .060f, .024f, .085f, .052f, .016f };
            const float noiseLevel = instrumentBreath[static_cast<std::size_t>(instrument)]
                                    * (.22f + 1.4f * air);
            const float onset = static_cast<float>(std::exp(-elapsed / .028));
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
            const float naturalMotion = 1.0f + .022f * humanize
                * static_cast<float>(std::sin(twoPi * driftPhase * 1.31)
                                     + .35 * std::sin(twoPi * driftPhase * 2.29));
            latestLevel = env * v * currentExpression * currentBreath
                        * pressureGain * breathEnvelope * naturalMotion * .24f;
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
        static constexpr std::array<std::array<float, 12>, 6> harmonicProfiles {{
            {{ 1.f, .85f, .72f, .59f, .48f, .38f, .25f, .18f, .11f, .07f, .04f, .02f }}, // trumpet
            {{ 1.f, .45f, .83f, .28f, .56f, .18f, .34f, .14f, .18f, .06f, .08f, .04f }}, // tenor sax
            {{ 1.f, .73f, .48f, .35f, .21f, .14f, .08f, .06f, .03f, .02f, .01f, .00f }}, // trombone
            {{ 1.f, .12f, .035f,.02f, .010f,.00f, .00f, .00f, .00f, .00f, .00f, .00f }}, // flute
            {{ 1.f, .02f, .82f, .015f,.39f, .01f, .23f, .005f,.10f, .00f, .05f, .00f }}, // clarinet
            {{ 1.f, .64f, .36f, .20f, .10f, .06f, .035f,.025f,.015f,.01f, .01f, .00f }}  // horn
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
        std::array<float, 12> spectralWeights{};
        float spectralNormalizer = 1.0f;
        double latestFrequency = 440.0;
        float latestLevel = 0.0f;
        int pitchWheel = 8192;
        std::uint32_t noiseState = 0x9231abcdu;
    };
}
