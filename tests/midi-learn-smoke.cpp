// Runtime assertions must remain enabled even in optimized Release CI.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "Dsp/MidiLearnBindings.h"
#include <cassert>
#include <cstdint>
#include <iostream>

int main()
{
    using CC = windlab::MidiLearnBindings;
    CC learn;
    assert(learn.binding(CC::Vibrato) == 1);
    assert(learn.binding(CC::Breath) == 2);
    assert(learn.binding(CC::Expression) == 11);
    assert(learn.armedTarget() == CC::None);
    assert(learn.route(1, 1) == 1 && learn.route(1, 2) == 2);
    assert(learn.route(1, 11) == 11 && learn.route(1, 64) == 64);

    // Learn a hardware breath fader CC74, preserving all other controller types.
    learn.arm(CC::Breath);
    assert(learn.armedTarget() == CC::Breath);
    assert(learn.route(1, 74) == 2);
    assert(learn.binding(CC::Breath) == 74);
    assert(learn.armedTarget() == CC::None);
    assert(learn.route(4, 74) == 2);
    assert(learn.route(1, 2) == -1); // old breath CC is no longer an active input
    assert(learn.route(1, 1) == 1 && learn.route(1, 11) == 11);

    // The internal sequencer on channel 16 must never alter MIDI Learn.
    learn.arm(CC::Vibrato);
    assert(learn.route(16, 90) == 90);
    assert(learn.armedTarget() == CC::Vibrato);
    assert(learn.route(1, 11) == 1);  // controller 11 is assigned to vibrato
    assert(learn.binding(CC::Vibrato) == 11);
    assert(learn.binding(CC::Expression) == 1); // collision swaps, never duplicates
    assert(learn.binding(CC::Breath) == 74);
    assert(learn.route(1, 1) == 11);
    assert(learn.route(16, 11) == 11); // internal phrase remains native CC11

    // Ordinary sustain CC64 can be learned; when mapped, it no longer
    // accidentally turns on the MIDI sustain pedal in the synth.
    learn.arm(CC::Expression);
    assert(learn.route(1, 64) == 11);
    assert(learn.binding(CC::Expression) == 64);
    assert(learn.route(1, 1) == -1);

    // Cancel stops listening, e.g. when closing the MIDI Learn panel.
    learn.arm(CC::Breath);
    learn.cancel();
    const auto previous = learn.snapshot();
    assert(learn.route(1, 98) == 98);
    assert(learn.snapshot() == previous);

    // Settings live in the plugin state, not in the global operating system.
    CC restored;
    assert(restored.restore(previous));
    assert(restored.snapshot() == previous);
    assert(!restored.restore(0u)); // no CC0
    assert(restored.snapshot() == previous);
    assert(!restored.restore(1u | (1u << 7) | (11u << 14)));
    assert(restored.snapshot() == previous);
    assert(!restored.restore(previous | (1u << 28)));

    learn.reset();
    assert(learn.snapshot() == CC::defaults());
    assert(learn.route(16, 2) == 2);
    assert(!learn.assign(CC::Breath, -4));
    assert(!learn.assign(CC::None, 3));

    // Stable map change under repeated assignment, avoiding duplicate targets.
    for (int sourceCC = 1; sourceCC <= 119; ++sourceCC)
    {
        assert(learn.assign(CC::Vibrato, sourceCC));
        const int a = learn.binding(CC::Vibrato);
        const int b = learn.binding(CC::Breath);
        const int c = learn.binding(CC::Expression);
        assert(a != b && b != c && a != c);
        assert(learn.route(3, sourceCC) == 1);
    }
    std::cout << "WindLab 0.9.2 MIDI Learn: PASS defaults, capture, swaps, phrase isolation and restore\n";
}
