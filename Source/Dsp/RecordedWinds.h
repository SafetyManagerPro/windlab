#pragma once
// Real acoustic recordings: voice-safe read-only bank and phase-continuous loop.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "SampleLoopPlan.h"
#include "SamplePhaseAlignment.h"
#include "RecordedArticulation.h"
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
    int tuneCents = 0;     // SFZ-style correction: positive raises the pitch when played at its key
    float gain = 1.0f;     // per-zone level normalisation
    int attackStart = 0;  // precomputed one-shot leading-silence trim
    SamplePhaseAlignment companionAlignment; // computed once at bank load
};

class RecordedWinds
{
public:
    RecordedWinds()
    {
#ifdef WINDLAB_HAS_RECORDED_SAMPLES
        juce::WavAudioFormat wavFormat;
        juce::FlacAudioFormat flacFormat;
        for (const auto& item : sampleDescriptions)
        {
            int len = 0;
            const char* bytes = WindLabRecordedData::getNamedResource(item.resourceName, len);
            if (bytes == nullptr || len <= 0)
                continue;
            // The build embeds either WAV or FLAC files; the resource name keeps the extension.
            const bool isFlac = juce::String(item.resourceName).endsWith("_flac");
            juce::AudioFormat& format = isFlac ? static_cast<juce::AudioFormat&>(flacFormat)
                                               : static_cast<juce::AudioFormat&>(wavFormat);
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
            entry.tuneCents = item.tuneCents;
            entry.gain = item.gain;
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
            // Skip only demonstrably silent staccato pre-roll. Keep the real
            // acoustic transient and original one-shot tail unchanged.
            if (entry.isOneShot)
                entry.attackStart = findStaccatoOnsetFrame(entry.wav.getReadPointer(0), count,
                                                         reader->sampleRate);
            // Staccato uses the genuine played tail (never artificially loops).
            // Sustained recordings crossfade within their natural body.
            if (!entry.isOneShot)
            {
                const auto plan = chooseSampleLoop(entry.wav.getReadPointer(0),
                                                   count, reader->sampleRate);
                if (!plan.valid) continue;
                entry.loopStart = plan.start;
                entry.loopEnd = plan.end;
                entry.fade = plan.fade;
                // Audio after the loop end is never read: free it (long source takes).
                const int keep = std::min(count, plan.end + 4);
                if (keep < count)
                    entry.wav.setSize(1, keep, true);
            }
            bank.push_back(std::move(entry));
        }
        // Correlate real dynamic siblings during bank initialization, never
        // during noteOn/render on the audio thread.
        for (auto& primary : bank)
        {
            const auto* companion = selectCompanion(&primary);
            if (companion == nullptr) continue;
            const double nominalHz = 440.0 * std::exp2((primary.rootNote - 69.0) / 12.0);
            primary.companionAlignment = estimateSamplePhaseAlignment(
                primary.wav.getReadPointer(0), primary.wav.getNumSamples(), primary.sampleRate,
                companion->wav.getReadPointer(0), companion->wav.getNumSamples(), companion->sampleRate,
                nominalHz);
        }
#endif
    }

    // Select recorded note, articulation, velocity zone and repeated-note take.
    // All buffers are immutable and the search is allocation-free on the audio thread.
    const RecordedWind* select(int instrument, int midiNote, float velocity,
                               int articulation) const noexcept
    {
        const int requestedArticulation = articulation == 1 ? 1 : 0;
        const int wantedLayer = velocity < .42f ? 1 : (velocity < .73f ? 2 : 3);
        // First choose the true nearest recorded pitch and dynamic layer.
        // Round-robin only among interchangeable recordings of that EXACT
        // pitch/layer/articulation: unrelated MIDI notes cannot advance it.
        for (const int passArticulation : {requestedArticulation, 0})
        {
            const RecordedWind* chosen = nullptr;
            int best = 99999;
            for (const auto& sample : bank)
            {
                if (sample.instrument != instrument || sample.articulation != passArticulation)
                    continue;
                const int score = std::abs(sample.rootNote - midiNote) * 128
                                + std::abs(sample.velocityLayer - wantedLayer) * 8;
                if (score < best) { chosen = &sample; best = score; }
            }
            if (chosen == nullptr) continue;

            unsigned takes = 0;
            for (const auto& sample : bank)
                if (sample.instrument == instrument && sample.articulation == passArticulation
                    && sample.rootNote == chosen->rootNote
                    && sample.velocityLayer == chosen->velocityLayer)
                    ++takes;
            unsigned index = roundRobin.takeIndex(instrument, midiNote, passArticulation, takes);
            for (const auto& sample : bank)
                if (sample.instrument == instrument && sample.articulation == passArticulation
                    && sample.rootNote == chosen->rootNote
                    && sample.velocityLayer == chosen->velocityLayer)
                {
                    if (index-- == 0u) return &sample;
                }
            return chosen; // defensive fallback: the immutable bank never changes
        }
        return nullptr;
    }
    // Choose a genuine alternative dynamic take of the SAME pitch zone and
    // articulation. Never pair a nearby sampled pitch or a different one-shot.
    // If no real complementary layer exists, keep the original sample unchanged.
    const RecordedWind* selectCompanion(const RecordedWind* primary) const noexcept
    {
        if (primary == nullptr || primary->isOneShot) return nullptr;
        const RecordedWind* companion = nullptr;
        int bestScore = -1;
        for (const auto& candidate : bank)
        {
            if (candidate.instrument != primary->instrument
                || candidate.rootNote != primary->rootNote
                || candidate.articulation != primary->articulation
                || candidate.velocityLayer == primary->velocityLayer)
                continue;
            // Prefer the largest genuine dynamic difference, then a matching
            // round-robin take where the sample library offers one.
            const int score = 4 * std::abs(candidate.velocityLayer - primary->velocityLayer)
                            + (candidate.roundRobin == primary->roundRobin ? 1 : 0);
            if (score > bestScore) { bestScore = score; companion = &candidate; }
        }
        return companion;
    }

    std::size_t size() const noexcept { return bank.size(); }

private:
    std::vector<RecordedWind> bank;
    mutable RepeatedNoteCycle roundRobin;
};

// A per-MIDI-voice cursor. No memory allocations and no mutable shared buffers.
class RecordedCursor
{
public:
    // legatoEntry: a slurred note joins the sustained body of the take (as a player
    // changing fingering would), so the recorded attack is not played again.
    // entryFraction (0..1) varies the entry point inside the first part of the loop.
    void start(const RecordedWind* newSample, double noteFrequency, double hostRate,
               bool legatoEntry = false, float entryFraction = 0.0f,
               double advanceFrames = 0.0)
    {
        sample = newSample;
        position = sample && sample->isOneShot ? static_cast<double>(sample->attackStart) : 0.0;
        rate = hostRate > 0 ? sampleRate() / hostRate : 1.0;
        sourceFrequency = 440.0 * std::exp2((sample ? sample->rootNote : 69) / 12.0 - 69.0 / 12.0)
                        * std::exp2(-(sample ? sample->tuneCents : 0) / 1200.0);
        if (sample != nullptr && legatoEntry && !sample->isOneShot)
        {
            const double body = std::max(0.0, sample->loopEnd - sample->fade - sample->loopStart);
            position = sample->loopStart + .3 * body * std::clamp(static_cast<double>(entryFraction), 0.0, 1.0);
        }
        if (sample != nullptr && !sample->isOneShot && std::isfinite(advanceFrames))
            position += std::clamp(advanceFrames, 0.0,
                                   std::max(0.0, sample->loopEnd - position - 4.0));
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
        if (sample->isOneShot)
            output *= oneShotTailGain(position, b.getNumSamples(), sample->sampleRate);
        const double fadeStart = sample->loopEnd - sample->fade;
        if (!sample->isOneShot && position >= fadeStart && position < sample->loopEnd)
        {
            const auto alpha = static_cast<float>((position - fadeStart) / sample->fade);
            const auto other = readAt(sample->loopStart + (position - fadeStart));
            output = first * (1.0f - alpha) + other * alpha;
        }
        output *= sample->gain;
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
