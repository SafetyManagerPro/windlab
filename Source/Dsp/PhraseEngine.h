#pragma once
// WindLab v0.7: fixed-size lock-free phrase sequencer.
// No JUCE dependency, no heap allocation, I/O or locking on audio callback.
// Phrase notes and expression points are stored atomically for UI/DAW threads.
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace windlab
{
struct PhraseNote
{
    int pitch = 60;          // MIDI note
    int velocity = 96;       // note-on timbre / intensity
    int breath = 112;        // CC2, automatically interpolated into next step
    int bend = 64;           // 0..127: 64 = centre, +-2 semitones
    int gate = 1;            // duration in eighth-note steps (1..8)
    bool active = false;
    bool legato = false;     // small overlap into following attack
};

inline std::uint64_t packPhraseNote(PhraseNote n) noexcept
{
    const auto clamp = [](int x, int a, int b) { return std::clamp(x, a, b); };
    return static_cast<std::uint64_t>(clamp(n.pitch, 0, 127))
        | (static_cast<std::uint64_t>(clamp(n.velocity, 1, 127)) << 7)
        | (static_cast<std::uint64_t>(clamp(n.breath, 0, 127)) << 14)
        | (static_cast<std::uint64_t>(clamp(n.bend, 0, 127)) << 21)
        | (static_cast<std::uint64_t>(clamp(n.gate, 1, 8)) << 28)
        | (static_cast<std::uint64_t>(n.active ? 1 : 0) << 32)
        | (static_cast<std::uint64_t>(n.legato ? 1 : 0) << 33);
}

inline PhraseNote unpackPhraseNote(std::uint64_t p) noexcept
{
    PhraseNote n;
    n.pitch = static_cast<int>(p & 127u);
    n.velocity = static_cast<int>((p >> 7) & 127u);
    n.breath = static_cast<int>((p >> 14) & 127u);
    n.bend = static_cast<int>((p >> 21) & 127u);
    n.gate = std::clamp(static_cast<int>((p >> 28) & 15u), 1, 8);
    n.active = ((p >> 32) & 1u) != 0;
    n.legato = ((p >> 33) & 1u) != 0;
    return n;
}

enum class PhraseEvent : int { NoteOn, NoteOff, Breath, PitchWheel };

class PhraseEngine
{
public:
    static constexpr int steps = 16;
    static constexpr int midiChannel = 16; // internal phrase; external controllers normally use ch1

    PhraseEngine() { clear(); }

    PhraseNote note(int step) const noexcept
    {
        return unpackPhraseNote(notes[static_cast<std::size_t>(std::clamp(step, 0, steps - 1))]
                                .load(std::memory_order_relaxed));
    }
    void setNote(int step, PhraseNote n) noexcept
    {
        if (step < 0 || step >= steps) return;
        notes[static_cast<std::size_t>(step)].store(packPhraseNote(n), std::memory_order_release);
    }
    std::uint64_t packed(int step) const noexcept
    {
        return notes[static_cast<std::size_t>(std::clamp(step, 0, steps - 1))]
                    .load(std::memory_order_acquire);
    }
    void setPacked(int step, std::uint64_t p) noexcept
    {
        if (step < 0 || step >= steps) return;
        notes[static_cast<std::size_t>(step)].store(p, std::memory_order_release);
    }
    void clear() noexcept
    {
        for (auto& item : notes)
            item.store(packPhraseNote(PhraseNote{}), std::memory_order_release);
    }
    void loadDemo() noexcept
    {
        clear();
        constexpr int pitches[] {60, 62, 64, 67, 69, 67, 64, 62};
        for (int i = 0; i < 8; ++i)
        {
            PhraseNote n;
            n.pitch = pitches[i];
            n.velocity = 70 + (i % 3) * 16;
            n.breath = 80 + (i % 4) * 12;
            n.bend = 64;
            n.active = true;
            n.legato = i > 0 && i < 6;
            n.gate = (i == 7 ? 2 : 1);
            setNote(i * 2, n);
        }
    }
    void setPlaying(bool value) noexcept
    {
        const bool old = playing.exchange(value, std::memory_order_acq_rel);
        if (old != value)
            revision.fetch_add(1, std::memory_order_release);
    }
    bool isPlaying() const noexcept { return playing.load(std::memory_order_acquire); }
    void setTempo(int v) noexcept { tempo.store(std::clamp(v, 45, 220), std::memory_order_release); }
    int getTempo() const noexcept { return tempo.load(std::memory_order_acquire); }
    int visualStep() const noexcept { return displayedStep.load(std::memory_order_relaxed); }

    template <class Emit>
    void process(int blockSamples, double sampleRate, Emit&& emit) noexcept
    {
        if (blockSamples <= 0 || sampleRate <= 1.0) return;
        const bool on = isPlaying();
        const unsigned requestedRev = revision.load(std::memory_order_acquire);
        if (!on || requestedRev != seenRevision)
        {
            stopSounding(0, emit);
            currentStep = 0;
            phase = 0.0;
            controllerCountdown = 0.0;
            nextStepPending = true;
            displayedStep.store(0, std::memory_order_relaxed);
            seenRevision = requestedRev;
            if (!on) return;
        }

        // 16 eighth-note steps = two 4/4 bars. Tempo intentionally independent
        // of DAW transport in this version; Play/Stop is explicit and predictable.
        const double stepSamples = sampleRate * 30.0 / static_cast<double>(getTempo());
        const double controlInterval = std::max(32.0, sampleRate / 55.0);
        for (int i = 0; i < blockSamples; ++i)
        {
            for (auto& v : voices)
            {
                if (!v.active) continue;
                v.remaining -= 1.0;
                if (v.remaining <= 0.0)
                {
                    emit(PhraseEvent::NoteOff, v.pitch, 0, i);
                    v.active = false;
                }
            }

            if (nextStepPending)
            {
                displayedStep.store(currentStep, std::memory_order_relaxed);
                const auto n = note(currentStep);
                // Set the wind controllers before note-on so the first sample
                // starts at the intended breath and pitch, not the previous step.
                emit(PhraseEvent::Breath, 2, n.breath, i);
                emit(PhraseEvent::PitchWheel, 0,
                     std::clamp(8192 + (n.bend - 64) * 128, 0, 16383), i);
                if (n.active)
                {
                    // A detached note ends previous phrase notes before retrigger;
                    // a slurred note may overlap by about 15ms.
                    if (!n.legato)
                        stopSounding(i, emit);
                    auto* freeSlot = findFreeSlot();
                    if (freeSlot != nullptr)
                    {
                        freeSlot->pitch = n.pitch;
                        freeSlot->remaining = std::max(stepSamples * .20,
                            stepSamples * n.gate - (n.legato ? -sampleRate * .015 : sampleRate * .012));
                        freeSlot->active = true;
                        emit(PhraseEvent::NoteOn, n.pitch, n.velocity, i);
                    }
                }
                nextStepPending = false;
                controllerCountdown = 0.0;
            }

            if (controllerCountdown <= 0.0)
            {
                // Continuous curves interpolated between the two expression
                // points of neighbouring steps, even during held notes.
                const auto a = note(currentStep), b = note((currentStep + 1) % steps);
                const float fraction = static_cast<float>(std::clamp(phase / stepSamples, 0.0, 1.0));
                const int cc2 = std::clamp(static_cast<int>(std::lround(
                    a.breath + (b.breath - a.breath) * fraction)), 0, 127);
                const int bend = std::clamp(static_cast<int>(std::lround(
                    a.bend + (b.bend - a.bend) * fraction)), 0, 127);
                emit(PhraseEvent::Breath, 2, cc2, i);
                emit(PhraseEvent::PitchWheel, 0, std::clamp(8192 + (bend - 64) * 128, 0, 16383), i);
                controllerCountdown += controlInterval;
            }
            controllerCountdown -= 1.0;
            phase += 1.0;
            if (phase >= stepSamples)
            {
                phase -= stepSamples;
                currentStep = (currentStep + 1) % steps;
                nextStepPending = true;
            }
        }
    }

private:
    struct Voice { int pitch = 60; double remaining = 0.0; bool active = false; };
    std::array<std::atomic<std::uint64_t>, steps> notes {};
    std::atomic<bool> playing {false};
    std::atomic<int> tempo {110}, displayedStep {0};
    std::atomic<unsigned> revision {0};
    // Below: audio-thread only.
    std::array<Voice, 16> voices {};
    unsigned seenRevision = 0;
    int currentStep = 0;
    double phase = 0.0, controllerCountdown = 0.0;
    bool nextStepPending = true;

    Voice* findFreeSlot() noexcept
    {
        for (auto& v : voices)
            if (!v.active) return &v;
        return nullptr;
    }
    template <class Emit>
    void stopSounding(int offset, Emit&& emit) noexcept
    {
        for (auto& v : voices)
        {
            if (!v.active) continue;
            emit(PhraseEvent::NoteOff, v.pitch, 0, offset);
            v.active = false;
        }
    }
};
}
