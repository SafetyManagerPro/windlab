#pragma once
// Real-time-safe, instrument-specific spectral response for acoustic recordings.
// Uses genuine recorded transients; never synthesizes false wind sample layers.
#include "InstrumentExpressionProfiles.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace windlab
{
class AcousticExpression final
{
public:
    void prepare(double newSampleRate) noexcept
    {
        sampleRate = std::clamp(newSampleRate, 8000.0, 192000.0);
        smoothing = static_cast<float>(1.0 - std::exp(-1.0 / (sampleRate * .012)));
        setInstrument(0);
        reset();
    }

    // Set on note-on (never called per sample). Calculate cutoff coefficients
    // only here, not on the real-time rendering loop.
    void setInstrument(int instrument) noexcept
    {
        profile = &instrumentExpressionProfile(instrument);
        constexpr double twoPi = 6.28318530717958647692;
        softAlpha = static_cast<float>(
            1.0 - std::exp(-twoPi * profile->softCutoffHz / sampleRate));
        strongAlpha = static_cast<float>(
            1.0 - std::exp(-twoPi * profile->strongCutoffHz / sampleRate));
        attackFrames = static_cast<std::uint32_t>(
            std::max(1.0, sampleRate * profile->transientMs * .001));
    }

    // With a slur the playback cursor enters the recording's sustain section:
    // never invent a new attack or accentuate a non-existent tongue noise.
    void reset(float breath = 1.0f, float brightness = .55f,
               bool isSlurred = false) noexcept
    {
        state = 0.0f;
        alpha = targetAlpha(breath, brightness);
        elapsedFrames = isSlurred ? attackFrames : 0u;
    }

    float process(float sample, float breath, float brightness) noexcept
    {
        // No trig, allocation, I/O or shared per-voice state in the audio callback.
        const float target = targetAlpha(breath, brightness);
        alpha += smoothing * (target - alpha);
        state += alpha * (sample - state);

        // Briefly preserve the real tongue/transient of the WAV while the
        // instrument-specific lowpass shapes its body. Never boost > dry sample.
        if (elapsedFrames < attackFrames)
        {
            const float progress = static_cast<float>(elapsedFrames++)
                                 / static_cast<float>(attackFrames);
            const float dryBlend = profile->transientDryMix * (1.0f - progress);
            return state + dryBlend * (sample - state);
        }
        return state;
    }

private:
    float targetAlpha(float breath, float brightness) const noexcept
    {
        const float energy = std::clamp(breath, 0.0f, 1.0f);
        const float tone = std::clamp(brightness, 0.0f, 1.0f);
        const float colour = std::clamp(.15f + profile->breathColour * energy
                                         + profile->toneColour * tone, 0.0f, 1.0f);
        return softAlpha + (strongAlpha - softAlpha) * colour * colour;
    }

    const InstrumentExpressionProfile* profile = &instrumentExpressionProfiles[0];
    double sampleRate = 44100.0;
    float softAlpha = .2f, strongAlpha = .85f, smoothing = .001f;
    float alpha = .8f, state = 0.0f;
    std::uint32_t elapsedFrames = 0, attackFrames = 600;
};
}
