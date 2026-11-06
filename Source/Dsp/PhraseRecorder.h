#pragma once
// WindLab 0.9.3. Fixed-size, allocation-free MIDI phrase capture.
// Audio callback records only external / virtual-keyboard MIDI (channels 1..15).
// Recorded notes remain a PREVIEW until the user explicitly imports into PhraseEngine.
#include "PhraseEngine.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace windlab
{
class PhraseRecorder final
{
public:
    enum State : int { Idle = 0, Recording = 1, Ready = 2 };
    PhraseRecorder() { clearPreview(); }

    bool requestStart() noexcept
    {
        if (state.load(std::memory_order_acquire) == Recording) return false;
        startRequested.store(true, std::memory_order_release);
        return true;
    }
    void requestStop() noexcept { stopRequested.store(true, std::memory_order_release); }
    void discard() noexcept
    {
        if (state.load(std::memory_order_acquire) == Recording) requestStop();
        else { state.store(Idle, std::memory_order_release); clearPreview(); }
    }
    int status() const noexcept { return state.load(std::memory_order_acquire); }
    int progress() const noexcept { return stepProgress.load(std::memory_order_acquire); }
    int notesCaptured() const noexcept { return captured.load(std::memory_order_acquire); }
    int notesDropped() const noexcept { return dropped.load(std::memory_order_acquire); }
    PhraseGrid recordedGrid() const noexcept { return static_cast<PhraseGrid>(grid.load(std::memory_order_acquire)); }
    // Slot 0 of a step (the primary note, and the step's controller values).
    PhraseNote recorded(int step) const noexcept { return chordRecorded(step, 0); }
    PhraseNote chordRecorded(int step, int slot) const noexcept
    {
        if (step < 0 || step >= PhraseEngine::maxSteps || slot < 0 || slot >= PhraseEngine::chordSlots)
            return {};
        return unpackPhraseNote(preview[index(step, slot)].load(std::memory_order_acquire));
    }
    bool importInto(PhraseEngine& destination) noexcept
    {
        if (status() != Ready) return false;
        destination.setPlaying(false);
        destination.setGrid(recordedGrid());
        for (int step = 0; step < PhraseEngine::maxSteps; ++step)
            for (int slot = 0; slot < PhraseEngine::chordSlots; ++slot)
                destination.setChordNote(step, slot, chordRecorded(step, slot));
        // The imported notes persist in the normal WindLabPhraseV1 project state.
        state.store(Idle, std::memory_order_release);
        return true;
    }

    // Called only from the audio callback at the beginning of a block.
    void beginBlock(int length, double sampleRate, int bpm, PhraseGrid newGrid = PhraseGrid::Eighth) noexcept
    {
        blockLength = std::max(0, length);
        if (startRequested.exchange(false, std::memory_order_acq_rel))
        {
            clearPreview();
            elapsedSamples = 0;
            currentSnapshot = -1;
            breath = 112; expression = 110; vibrato = 0; bend = 64;
            sustainDown = false;
            for (auto& voice : voices) voice = {};
            grid.store(static_cast<int>(newGrid), std::memory_order_release);
            totalSteps = 2 * stepsPerBarFor(newGrid);
            // One step = ticksPerStep/240 eighth notes at the given tempo.
            stepSamples = (std::isfinite(sampleRate) && sampleRate > 0.0)
                ? sampleRate * 30.0 / static_cast<double>(std::clamp(bpm, 30, 300))
                  * static_cast<double>(ticksPerStepFor(newGrid)) / 240.0
                : 0.0;
            stopRequested.store(false, std::memory_order_release);
            state.store(stepSamples > 0.0 ? Recording : Idle, std::memory_order_release);
        }
    }

    void noteOn(int channel, int pitch, int velocity, int offset) noexcept
    {
        if (!accept(channel) || pitch < 0 || pitch > 127 || velocity < 1) return;
        const auto sample = eventSample(offset);
        if (sample >= totalSamples()) return;
        const int step = std::clamp(static_cast<int>(std::lround(sample / stepSamples)),
                                    0, totalSteps - 1);
        populateTo(static_cast<int>(sample / stepSamples));
        // Notes already sounding in this block make this one legato.
        bool overlap = false;
        for (const auto& v : voices) if (v.active) overlap = true;
        // Free chord slot in this step; a fifth simultaneous note cannot be stored.
        int slot = -1;
        for (int candidate = 0; candidate < PhraseEngine::chordSlots; ++candidate)
            if (!chordRecorded(step, candidate).active) { slot = candidate; break; }
        if (slot < 0) { dropped.fetch_add(1, std::memory_order_relaxed); return; }
        auto n = chordRecorded(step, slot);
        n.active = true;
        n.pitch = pitch;
        n.velocity = std::clamp(velocity, 1, 127);
        n.gate = 1;
        n.legato = overlap;
        n.breath = breath; n.expression = expression;
        n.vibrato = vibrato; n.bend = bend;
        preview[index(step, slot)].store(packPhraseNote(n), std::memory_order_release);
        for (auto& v : voices)
        {
            if (v.active) continue;
            v.active = true; v.channel = channel; v.pitch = pitch;
            v.startSample = sample; v.step = step; v.slot = slot;
            captured.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        // A bounded maximum: no heap allocations in real time.
        dropped.fetch_add(1, std::memory_order_relaxed);
    }

    void noteOff(int channel, int pitch, int offset) noexcept
    {
        if (!accept(channel)) return;
        for (auto& v : voices)
        {
            if (!v.active || v.sustained || v.channel != channel || v.pitch != pitch) continue;
            if (sustainDown) { v.sustained = true; return; } // closed when the pedal goes up
            finishVoice(v, eventSample(offset));
            return;
        }
    }

    void controller(int channel, int number, int value, int offset) noexcept
    {
        if (!accept(channel)) return;
        value = std::clamp(value, 0, 127);
        if (number == 64)
        {
            const double at = eventSample(offset);
            if (value >= 64) { sustainDown = true; return; }
            sustainDown = false;
            for (auto& v : voices)
                if (v.active && v.sustained) { v.sustained = false; finishVoice(v, at); }
            return;
        }
        const int step = std::clamp(static_cast<int>(eventSample(offset) / stepSamples),
                                    0, totalSteps - 1);
        populateTo(step);
        if (number == 2) breath = value;
        else if (number == 11) expression = value;
        else if (number == 1) vibrato = value;
        else return;
        updateStep(step);
    }

    void pitchWheel(int channel, int value, int offset) noexcept
    {
        if (!accept(channel)) return;
        const int step = std::clamp(static_cast<int>(eventSample(offset) / stepSamples),
                                    0, totalSteps - 1);
        populateTo(step);
        bend = std::clamp(static_cast<int>(std::lround(
            64.0 + (std::clamp(value, 0, 16383) - 8192.0) / 128.0)), 0, 127);
        updateStep(step);
    }

    // Called on audio callback after the last incoming MIDI event.
    void endBlock() noexcept
    {
        if (status() != Recording) return;
        elapsedSamples += static_cast<std::uint64_t>(blockLength);
        populateTo(std::min(totalSteps - 1, static_cast<int>(elapsedSamples / stepSamples)));
        stepProgress.store(std::clamp(static_cast<int>(elapsedSamples / stepSamples),
                                      0, totalSteps), std::memory_order_release);
        if (elapsedSamples >= totalSamples() ||
            stopRequested.exchange(false, std::memory_order_acq_rel))
        {
            for (auto& v : voices) if (v.active) finishVoice(v, elapsedSamples);
            populateTo(totalSteps - 1);
            state.store(Ready, std::memory_order_release);
        }
    }

private:
    struct ActiveNote
    {
        bool active = false;
        bool sustained = false; // key released while the sustain pedal is down
        int channel = 0, pitch = 0, step = 0, slot = 0;
        double startSample = 0.0;
    };
    static constexpr std::size_t index(int step, int slot) noexcept
    {
        return static_cast<std::size_t>(step * PhraseEngine::chordSlots + slot);
    }
    std::array<std::atomic<std::uint64_t>, PhraseEngine::maxSteps * PhraseEngine::chordSlots> preview {};
    std::array<ActiveNote, 16> voices {};
    std::atomic<int> state {Idle}, stepProgress {0}, captured {0}, dropped {0}, grid {0};
    std::atomic<bool> startRequested {false}, stopRequested {false};
    std::uint64_t elapsedSamples = 0;
    int blockLength = 0, currentSnapshot = -1, totalSteps = PhraseEngine::steps;
    int breath = 112, expression = 110, vibrato = 0, bend = 64;
    bool sustainDown = false;
    double stepSamples = 0.0;

    void clearPreview() noexcept
    {
        PhraseNote n;
        for (auto& step : preview)
            step.store(packPhraseNote(n), std::memory_order_release);
        captured.store(0, std::memory_order_release);
        dropped.store(0, std::memory_order_release);
        stepProgress.store(0, std::memory_order_release);
    }
    bool accept(int channel) const noexcept
    {
        return state.load(std::memory_order_acquire) == Recording &&
               channel >= 1 && channel < PhraseEngine::midiChannel && stepSamples > 0.0;
    }
    double eventSample(int offset) const noexcept
    {
        return static_cast<double>(elapsedSamples) +
               std::clamp(offset, 0, std::max(0, blockLength - 1));
    }
    double totalSamples() const noexcept { return stepSamples * totalSteps; }
    // Controller values are written to the primary slot of each step, as before.
    void populateTo(int step) noexcept
    {
        for (int i = currentSnapshot + 1; i <= std::min(step, totalSteps - 1); ++i)
        {
            auto n = recorded(i);
            n.breath = breath; n.expression = expression; n.vibrato = vibrato; n.bend = bend;
            preview[index(i, 0)].store(packPhraseNote(n), std::memory_order_release);
        }
        currentSnapshot = std::max(currentSnapshot, step);
    }
    void updateStep(int step) noexcept
    {
        auto n = recorded(step);
        n.breath = breath; n.expression = expression; n.vibrato = vibrato; n.bend = bend;
        preview[index(step, 0)].store(packPhraseNote(n), std::memory_order_release);
    }
    void finishVoice(ActiveNote& v, double endSample) noexcept
    {
        const double duration = std::max(0.0, endSample - v.startSample);
        auto n = chordRecorded(v.step, v.slot);
        n.gate = std::clamp(static_cast<int>(std::lround(duration / stepSamples)), 1, 8);
        preview[index(v.step, v.slot)].store(packPhraseNote(n), std::memory_order_release);
        v.active = false;
        v.sustained = false;
    }
};
}
