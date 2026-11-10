#pragma once
// Conservative analysis and playback of a recorded sustain's *existing* ending.
// Analysis occurs at bank load; playback has no I/O, allocations or locks.
// A decaying source ending is not claimed to be a genuine interval release.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace windlab
{
struct RecordedReleasePlan
{
    int start = 0;
    int end = 0;
    bool valid = false;
};

inline float releaseWindowRms(const float* data, int count, int start, int frames) noexcept
{
    if (data == nullptr || frames <= 0 || start < 0 || start > count - frames)
        return 0.0f;
    double sum = 0.0;
    for (int i = start; i < start + frames; ++i)
    {
        const double v = data[i];
        if (!std::isfinite(v)) return 0.0f;
        sum += v * v;
    }
    return static_cast<float>(std::sqrt(sum / frames));
}

inline RecordedReleasePlan detectRecordedRelease(const float* data, int count,
                                                  double sampleRate, int loopEnd) noexcept
{
    RecordedReleasePlan plan;
    if (data == nullptr || !std::isfinite(sampleRate) || sampleRate < 8000.0
        || sampleRate > 192000.0 || count < static_cast<int>(sampleRate * .45)
        || loopEnd < 1 || loopEnd > count - 3)
        return plan;

    const int window = std::max(32, static_cast<int>(sampleRate * .025));
    const int preRoll = static_cast<int>(sampleRate * .030);
    // The source part after the looping region is never invented: require
    // a sizeable original tail, and only use its final ~260ms.
    const int start = std::max(loopEnd + preRoll,
                               count - static_cast<int>(sampleRate * .260));
    if (start + 4 * window >= count || start <= loopEnd)
        return plan;

    const float sustain = releaseWindowRms(data, count, loopEnd - window, window);
    const float head = releaseWindowRms(data, count, start, window);
    const float end = releaseWindowRms(data, count, count - window, window);
    // Require a clearly audible start and a tapered original ending.
    // Otherwise keep the existing loop + note-off envelope unchanged.
    if (sustain < .0008f || head < .0008f || head < sustain * .12f
        || end > head * .20f || end > sustain * .17f)
        return plan;
    plan.start = start;
    plan.end = count - 2;
    plan.valid = true;
    return plan;
}

class RecordedReleaseTransition final
{
public:
    void prepare(double playbackRate) noexcept
    {
        rate = std::clamp(std::isfinite(playbackRate) ? playbackRate : 44100.0,
                          8000.0, 192000.0);
        reset();
    }

    void reset() noexcept
    {
        started = false;
        positionFrames = 0.0;
        endFrame = 0;
        sourceFadeFrames = 1.0;
        crossfadeFrames = 1u;
        elapsedFrames = 0u;
    }

    // Idempotent: note-off or the envelope's release can be signalled again
    // without restarting the recorded ending.
    bool begin(RecordedReleasePlan plan, double sourceSampleRate) noexcept
    {
        if (started || !plan.valid || plan.start < 0 || plan.end <= plan.start
            || !std::isfinite(sourceSampleRate) || sourceSampleRate <= 0.0)
            return false;
        started = true;
        positionFrames = static_cast<double>(plan.start);
        endFrame = plan.end;
        sourceFadeFrames = std::max(8.0, sourceSampleRate * .012);
        crossfadeFrames = std::max(16u, static_cast<unsigned>(rate * .023));
        elapsedFrames = 0u;
        return true;
    }

    bool active() const noexcept { return started; }
    double sourcePosition() const noexcept { return positionFrames; }
    bool finished() const noexcept { return started && positionFrames >= endFrame; }

    float mix(float sustained, float recordedTail, double advance) noexcept
    {
        if (!started) return sustained;
        if (finished()) return 0.0f;
        const float t = std::clamp(static_cast<float>(elapsedFrames + 1u)
                                  / static_cast<float>(crossfadeFrames), 0.0f, 1.0f);
        const float fade = t * t * (3.0f - 2.0f * t);
        const float remain = static_cast<float>(endFrame - positionFrames);
        const float endGain = std::clamp(remain / static_cast<float>(sourceFadeFrames),
                                          0.0f, 1.0f);
        const float result = (1.0f - fade) * sustained + fade * recordedTail * endGain;
        if (elapsedFrames < crossfadeFrames) ++elapsedFrames;
        positionFrames += std::max(0.0, std::isfinite(advance) ? advance : 0.0);
        return result;
    }

private:
    double rate = 44100.0;
    double positionFrames = 0.0;
    double sourceFadeFrames = 1.0;
    int endFrame = 0;
    unsigned crossfadeFrames = 1u, elapsedFrames = 0u;
    bool started = false;
};
}
