#pragma once
// Real acoustic recordings: voice-safe read-only bank and phase-continuous loop.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

#ifdef WINDLAB_HAS_RECORDED_SAMPLES
  #include <WindLabRecordedData.h>
  #include <SampleManifest.h>
#endif

namespace windlab
{
struct RecordedWind
{
    int instrument = 0, rootNote = 60;
    int velocityLayer = 1, articulation = 0, roundRobin = 1;
    double sampleRate = 44100.0;
    juce::AudioBuffer<float> wav; // read-only after construction
    double loopStart = 0.0, loopEnd = 0.0, fade = 0.0;
    bool isOneShot = false;
};

class RecordedWinds
{
public:
    RecordedWinds()
    {
#ifdef WINDLAB_HAS_RECORDED_SAMPLES
        juce::WavAudioFormat format;
        for (const auto& item : sampleDescriptions)
        {
            int len = 0;
            const char* bytes = WindLabRecordedData::getNamedResource(item.resourceName, len);
            if (bytes == nullptr || len <= 0)
                continue;
            auto reader = std::unique_ptr<juce::AudioFormatReader>(
                format.createReaderFor(new juce::MemoryInputStream(bytes, static_cast<std::size_t>(len), false), true));
            if (!reader || reader->lengthInSamples < 1024 || reader->sampleRate <= 0)
                continue;
            RecordedWind entry;
            entry.instrument = item.instrument;
            entry.rootNote = item.midiRoot;
            entry.velocityLayer = item.velocityLayer;
            entry.articulation = item.articulation;
            entry.roundRobin = item.roundRobin;
            entry.isOneShot = item.articulation == 1;
            entry.sampleRate = reader->sampleRate;
            // Upper bound limits corrupt file / memory exhaustion.
            const auto count = static_cast<int>(std::min<int64_t>(reader->lengthInSamples, reader->sampleRate * 12.0));
            entry.wav.setSize(1, count);
            juce::AudioBuffer<float> tmp(static_cast<int>(reader->numChannels), count);
            if (!reader->read(&tmp, 0, count, 0, true, true))
                continue;
            for (int n = 0; n < count; ++n)
            {
                float sum = 0.0f;
                for (int ch = 0; ch < tmp.getNumChannels(); ++ch)
                    sum += tmp.getSample(ch, n);
                entry.wav.setSample(0, n, sum / static_cast<float>(tmp.getNumChannels()));
            }
            // Staccato uses the genuine played tail (never artificially loops).
            // Sustained recordings crossfade within their natural body.
            if (!entry.isOneShot)
            {
                entry.loopStart = static_cast<double>(count) * .28;
                entry.loopEnd = static_cast<double>(count) * .75;
                entry.fade = std::min(0.065 * reader->sampleRate, (entry.loopEnd - entry.loopStart) * .20);
                if (entry.loopEnd - entry.loopStart <= entry.fade * 3.0)
                    continue;
            }
            bank.push_back(std::move(entry));
        }
#endif
    }

    // Select recorded note, articulation, velocity zone and repeated-note take.
    // All buffers are immutable and the search is allocation-free on the audio thread.
    const RecordedWind* select(int instrument, int midiNote, float velocity,
                               int articulation) const noexcept
    {
        const int requestedArticulation = articulation == 1 ? 1 : 0;
        const unsigned variation = (roundRobinCounter.fetch_add(1, std::memory_order_relaxed) & 1u) + 1u;
        const int wantedLayer = velocity < .42f ? 1 : (velocity < .73f ? 2 : 3);
        const RecordedWind* chosen = nullptr;
        int best = 99999;
        // Prefer actual staccato takes; if absent (tenor sax) reuse sustains.
        for (const int passArticulation : {requestedArticulation, 0})
        {
            for (const auto& sample : bank)
            {
                if (sample.instrument != instrument || sample.articulation != passArticulation)
                    continue;
                const int score = std::abs(sample.rootNote - midiNote) * 128
                                + std::abs(sample.velocityLayer - wantedLayer) * 8
                                + (sample.roundRobin == static_cast<int>(variation) ? 0 : 2);
                if (score < best) { chosen = &sample; best = score; }
            }
            if (chosen != nullptr) return chosen;
        }
        return nullptr;
    }
    std::size_t size() const noexcept { return bank.size(); }

private:
    std::vector<RecordedWind> bank;
    mutable std::atomic<unsigned> roundRobinCounter {0};
};

// A per-MIDI-voice cursor. No memory allocations and no mutable shared buffers.
class RecordedCursor
{
public:
    void start(const RecordedWind* newSample, double noteFrequency, double hostRate)
    {
        sample = newSample;
        position = 0.0;
        rate = hostRate > 0 ? sampleRate() / hostRate : 1.0;
        sourceFrequency = 440.0 * std::exp2((sample ? sample->rootNote : 69) / 12.0 - 69.0 / 12.0);
        (void) noteFrequency; // current pitch follows WindEngine every sample.
    }
    bool valid() const noexcept { return sample != nullptr; }
    float read(double currentFrequency)
    {
        if (!sample) return 0.0f;
        const auto& b = sample->wav;
        if (sample->isOneShot && position >= static_cast<double>(b.getNumSamples() - 2))
            return 0.0f;
        const double speed = rate * currentFrequency / sourceFrequency;
        const auto readAt = [&](double p)
        {
            const int i = std::clamp(static_cast<int>(p), 0, b.getNumSamples() - 2);
            const float fraction = static_cast<float>(p - i);
            return b.getSample(0, i) * (1.0f - fraction) + b.getSample(0, i + 1) * fraction;
        };
        const float first = readAt(position);
        float output = first;
        const double fadeStart = sample->loopEnd - sample->fade;
        if (!sample->isOneShot && position >= fadeStart && position < sample->loopEnd)
        {
            const auto alpha = static_cast<float>((position - fadeStart) / sample->fade);
            const auto other = readAt(sample->loopStart + (position - fadeStart));
            output = first * (1.0f - alpha) + other * alpha;
        }
        position += speed;
        if (!sample->isOneShot && position >= sample->loopEnd)
            position = sample->loopStart + sample->fade
                     + std::fmod(position - sample->loopEnd, sample->loopEnd - sample->loopStart - sample->fade);
        return output;
    }
private:
    double sampleRate() const { return sample ? sample->sampleRate : 44100.0; }
    const RecordedWind* sample = nullptr;
    double position = 0.0, rate = 1.0, sourceFrequency = 440.0;
};
}
