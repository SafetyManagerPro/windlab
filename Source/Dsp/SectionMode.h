#pragma once
// 0.9.8 section mode: every note is played by the instrument of the section that
// owns that register (like a sax or brass section voicing a chord), placed at its
// own position in the stereo field. JUCE-free and tested.
#include <algorithm>

namespace windlab
{
enum class SectionMode : int { Off = 0, Saxes, Brass, Woodwinds, BigBand };

// Instrument indices as in the "instrument" parameter:
// 0 trumpet, 1 tenor sax, 2 trombone, 3 flute, 4 clarinet, 5 horn, 6 soprano, 7 alto, 8 baritone.
inline int sectionInstrumentFor(SectionMode mode, int midiNote, int selectedInstrument) noexcept
{
    switch (mode)
    {
        case SectionMode::Saxes:     // baritone, tenor, alto, soprano from low to high
            return midiNote < 50 ? 8 : midiNote < 60 ? 1 : midiNote < 70 ? 7 : 6;
        case SectionMode::Brass:     // trombone, horn, trumpet
            return midiNote < 55 ? 2 : midiNote < 63 ? 5 : 0;
        case SectionMode::Woodwinds: // clarinet (with its low register), flute on top
            return midiNote < 67 ? 4 : 3;
        case SectionMode::BigBand:   // trombone and baritone below, saxes in the middle, trumpet lead
            return midiNote < 48 ? 8 : midiNote < 55 ? 2 : midiNote < 62 ? 1 : midiNote < 70 ? 7 : 0;
        case SectionMode::Off:
        default: return std::clamp(selectedInstrument, 0, 8);
    }
}

// Stereo position (-1 left .. +1 right) of each player when a section is active,
// roughly like a stage seen from the audience. Off = centre (the PAN knob still applies).
inline float sectionPanFor(SectionMode mode, int instrumentIndex) noexcept
{
    if (mode == SectionMode::Off) return 0.0f;
    switch (instrumentIndex)
    {
        case 6: return -.55f; // soprano
        case 7: return -.25f; // alto
        case 1: return .20f;  // tenor
        case 8: return .50f;  // baritone
        case 0: return mode == SectionMode::BigBand ? -.10f : .35f; // trumpet
        case 5: return -.05f; // horn
        case 2: return mode == SectionMode::BigBand ? .65f : -.40f; // trombone
        case 3: return -.35f; // flute
        case 4: return .30f;  // clarinet
        default: return 0.0f;
    }
}
}
