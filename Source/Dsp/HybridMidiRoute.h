#pragma once
// JUCE-independent hybrid MIDI routing policy; deterministic and RT-safe.
namespace windlab
{
struct HybridRoute
{
    enum Action { Keep, Drop, Remap };
    Action action = Keep;
    int channel = 1;
};
inline HybridRoute routeHybrid(int channel, bool controller, int cc,
                               int noteChannel, int breathChannel,
                               int vibratoCC, int breathCC, int expressionCC,
                               bool learning = false) noexcept
{
    if (noteChannel < 1 || noteChannel > 15 ||
        breathChannel < 1 || breathChannel > 15 ||
        noteChannel == breathChannel)
        return { HybridRoute::Keep, channel };
    // Phrase engine owns channel 16 and must remain isolated.
    if (channel == 16) return { HybridRoute::Keep, channel };
    if (channel != breathChannel) return { HybridRoute::Keep, channel };
    if (controller && (learning || cc == vibratoCC || cc == breathCC || cc == expressionCC))
        return { HybridRoute::Remap, noteChannel };
    return { HybridRoute::Drop, channel };
}
}
