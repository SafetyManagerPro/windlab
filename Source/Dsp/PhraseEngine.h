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
    int expression = 110;   // CC11, continuous held-note volume
    int vibrato = 0;        // CC1 modulation, continuous vibrato intensity
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
        | (static_cast<std::uint64_t>(n.legato ? 1 : 0) << 33)
        | (static_cast<std::uint64_t>(clamp(n.expression, 0, 127)) << 34)
        | (static_cast<std::uint64_t>(clamp(n.vibrato, 0, 127)) << 41);
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
    n.expression = static_cast<int>((p >> 34) & 127u);
    n.vibrato = static_cast<int>((p >> 41) & 127u);
    return n;
}

// Subdivision of the 2-bar phrase. Ticks are per 480-tick quarter note.
enum class PhraseGrid : int { Eighth = 0, Sixteenth = 1, EighthTriplet = 2, SixteenthTriplet = 3 };
inline int stepsPerBarFor(PhraseGrid g) noexcept
{
    constexpr int table[] {8, 16, 12, 24};
    return table[static_cast<int>(g)];
}
inline int ticksPerStepFor(PhraseGrid g) noexcept
{
    constexpr int table[] {240, 120, 160, 80};
    return table[static_cast<int>(g)];
}

enum class PhraseEvent : int { NoteOn, NoteOff, Breath, Expression, PitchWheel, ModWheel };

class PhraseEngine
{
public:
    // steps: the classic eighth-note phrase length (two bars). Kept for existing callers.
    static constexpr int steps = 16;
    // Largest phrase: 2 bars of sixteenth-note triplets. Chords use up to chordSlots per step.
    static constexpr int maxSteps = 48;
    static constexpr int chordSlots = 4;
    static constexpr int midiChannel = 16; // internal phrase; external controllers normally use ch1

    PhraseEngine() { clear(); }

    PhraseGrid getGrid() const noexcept { return static_cast<PhraseGrid>(grid.load(std::memory_order_acquire)); }
    void setGrid(PhraseGrid g) noexcept
    {
        const int next = static_cast<int>(g);
        if (grid.exchange(next, std::memory_order_acq_rel) != next)
            revision.fetch_add(1, std::memory_order_release);
    }
    // Number of steps in the phrase for the current grid (16 for eighths, up to 48).
    int activeSteps() const noexcept { return 2 * stepsPerBarFor(getGrid()); }
    int ticksPerStep() const noexcept { return ticksPerStepFor(getGrid()); }

    // Slot 0 is the primary note of a step; it also carries the step's controller values.
    PhraseNote note(int step) const noexcept { return chordNote(step, 0); }
    void setNote(int step, PhraseNote n) noexcept { setChordNote(step, 0, n); }
    std::uint64_t packed(int step) const noexcept { return chordPacked(step, 0); }
    void setPacked(int step, std::uint64_t p) noexcept { setChordPacked(step, 0, p); }

    PhraseNote chordNote(int step, int slot) const noexcept
    {
        return unpackPhraseNote(chordPacked(step, slot));
    }
    void setChordNote(int step, int slot, PhraseNote n) noexcept
    {
        setChordPacked(step, slot, packPhraseNote(n));
    }
    std::uint64_t chordPacked(int step, int slot) const noexcept
    {
        if (step < 0 || step >= maxSteps || slot < 0 || slot >= chordSlots) return packPhraseNote(PhraseNote{});
        return notes[index(step, slot)].load(std::memory_order_acquire);
    }
    void setChordPacked(int step, int slot, std::uint64_t p) noexcept
    {
        if (step < 0 || step >= maxSteps || slot < 0 || slot >= chordSlots) return;
        notes[index(step, slot)].store(p, std::memory_order_release);
    }
    // Number of sounding notes in a step (0..chordSlots).
    int chordSize(int step) const noexcept
    {
        int count = 0;
        for (int slot = 0; slot < chordSlots; ++slot)
            if (chordNote(step, slot).active) ++count;
        return count;
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
            n.expression = 80 + (i % 3) * 16;
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
    void setFollowHostTempo(bool enabled) noexcept { followHostTempo.store(enabled, std::memory_order_release); }
    // 0.9.7: start/stop with the DAW and lock the two-bar loop to the song position.
    void setFollowHostTransport(bool enabled) noexcept { followHostTransport.store(enabled, std::memory_order_release); }
    bool followsHostTransport() const noexcept { return followHostTransport.load(std::memory_order_acquire); }
    bool followsHostTempo() const noexcept { return followHostTempo.load(std::memory_order_acquire); }
    // Audio callback may read host BPM without changing the editor's internal tempo.
    // This follows the DAW's tempo, not the DAW playhead/song position.
    void setHostTempo(double bpm) noexcept
    {
        if (std::isfinite(bpm) && bpm > 0.0)
            hostTempo.store(std::clamp(static_cast<int>(std::lround(bpm)), 30, 300),
                            std::memory_order_release);
    }
    int effectiveTempo() const noexcept
    {
        return followsHostTempo() ? hostTempo.load(std::memory_order_acquire) : getTempo();
    }
    int visualStep() const noexcept { return displayedStep.load(std::memory_order_relaxed); }

    template <class Emit>
    void process(int blockSamples, double sampleRate, Emit&& emit, double hostQuarterNotes = -1.0) noexcept
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
        // The phrase is two 4/4 bars whatever the grid: the step length follows the grid.
        const int count = activeSteps();
        const double stepSamples = sampleRate * 30.0 / static_cast<double>(effectiveTempo())
                                 * static_cast<double>(ticksPerStep()) / 240.0;
        const double controlInterval = std::max(32.0, sampleRate / 55.0);
        if (hostQuarterNotes >= 0.0 && std::isfinite(hostQuarterNotes))
        {
            // Two 4/4 bars = 8 quarter notes. Re-align only when the DAW jumped or
            // drifted by more than a quarter of a step, so normal playback is untouched.
            const double loopTicks = std::fmod(hostQuarterNotes, 8.0) * 480.0;
            const int targetStep = std::clamp(static_cast<int>(loopTicks / ticksPerStep()), 0, count - 1);
            const double targetPhase = (loopTicks - targetStep * ticksPerStep()) / ticksPerStep() * stepSamples;
            const double expected = currentStep * stepSamples + phase;
            const double wanted = targetStep * stepSamples + targetPhase;
            if (std::abs(expected - wanted) > stepSamples * .25)
            {
                stopSounding(0, emit);
                currentStep = targetStep;
                phase = targetPhase;
                nextStepPending = targetPhase < stepSamples * .08;
                controllerCountdown = 0.0;
            }
        }
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
                emit(PhraseEvent::Expression, 11, n.expression, i);
                emit(PhraseEvent::ModWheel, 1, n.vibrato, i);
                emit(PhraseEvent::PitchWheel, 0,
                     std::clamp(8192 + (n.bend - 64) * 128, 0, 16383), i);
                // The first sounding note decides legato for the whole step (a chord is one attack).
                PhraseNote head;
                for (int slot = 0; slot < chordSlots && !head.active; ++slot)
                    head = chordNote(currentStep, slot);
                if (head.active)
                {
                    // A detached note ends previous phrase notes before retrigger;
                    // a slurred note may overlap by about 15ms.
                    if (!head.legato)
                        stopSounding(i, emit);
                    for (int slot = 0; slot < chordSlots; ++slot)
                    {
                        const auto c = chordNote(currentStep, slot);
                        if (!c.active) continue;
                        auto* freeSlot = findFreeSlot();
                        if (freeSlot == nullptr) break;
                        freeSlot->pitch = c.pitch;
                        freeSlot->remaining = std::max(stepSamples * .20,
                            stepSamples * c.gate - (c.legato ? -sampleRate * .015 : sampleRate * .012));
                        freeSlot->active = true;
                        emit(PhraseEvent::NoteOn, c.pitch, c.velocity, i);
                    }
                }
                nextStepPending = false;
                controllerCountdown = 0.0;
            }

            if (controllerCountdown <= 0.0)
            {
                // Continuous curves interpolated between the two expression
                // points of neighbouring steps, even during held notes.
                const auto a = note(currentStep), b = note((currentStep + 1) % count);
                const float fraction = static_cast<float>(std::clamp(phase / stepSamples, 0.0, 1.0));
                const int cc2 = std::clamp(static_cast<int>(std::lround(
                    a.breath + (b.breath - a.breath) * fraction)), 0, 127);
                const int bend = std::clamp(static_cast<int>(std::lround(
                    a.bend + (b.bend - a.bend) * fraction)), 0, 127);
                const int cc11 = std::clamp(static_cast<int>(std::lround(
                    a.expression + (b.expression - a.expression) * fraction)), 0, 127);
                emit(PhraseEvent::Breath, 2, cc2, i);
                emit(PhraseEvent::Expression, 11, cc11, i);
                const int cc1 = std::clamp(static_cast<int>(std::lround(
                    a.vibrato + (b.vibrato - a.vibrato) * fraction)), 0, 127);
                emit(PhraseEvent::ModWheel, 1, cc1, i);
                emit(PhraseEvent::PitchWheel, 0, std::clamp(8192 + (bend - 64) * 128, 0, 16383), i);
                controllerCountdown += controlInterval;
            }
            controllerCountdown -= 1.0;
            phase += 1.0;
            if (phase >= stepSamples)
            {
                phase -= stepSamples;
                currentStep = (currentStep + 1) % count;
                nextStepPending = true;
            }
        }
    }

private:
    struct Voice { int pitch = 60; double remaining = 0.0; bool active = false; };
    static constexpr std::size_t index(int step, int slot) noexcept
    {
        return static_cast<std::size_t>(step * chordSlots + slot);
    }
    std::array<std::atomic<std::uint64_t>, maxSteps * chordSlots> notes {};
    std::atomic<bool> playing {false};
    std::atomic<int> tempo {110}, hostTempo {110}, displayedStep {0}, grid {0};
    std::atomic<bool> followHostTempo {false}, followHostTransport {false};
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
