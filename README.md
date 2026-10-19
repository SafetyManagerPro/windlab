# WindLab 0.2 — virtual wind instrument for macOS

**Source-code prototype, not a ready-to-install plugin.** Builds AU, VST3, and Standalone from one JUCE project, with Apple Silicon (`arm64`) as the first target. Works *in principle* in hosts accepting those formats (Ableton, Studio One, Logic, etc.) **after compiling and host validation**. This package is **not a sample library**: the six sounds are synthetic sketches for developing MIDI performance and UI.

## What's improved in 0.2

| Feature | 0.1 | 0.2 |
| --- | --- | --- |
| Voices and timbres | 12 voices, 6 coarse timbres | 12 voices, 6 harmonic profiles with Nyquist-aware partial limiting |
| Articulations | Sustained only | Sustain, short auto-release Staccato, slow-attack Swell |
| MIDI expression | Pitch bend, CC1, CC11 | Pitch bend, CC1, CC2 breath, CC11, aftertouch/channel pressure; controller smoothing |
| CCs sent before notes | Not reliable | Cached per MIDI channel, applied when each new note begins |
| Dynamics | Velocity only | Velocity and pressure-responsive brightness/loudness |
| Stereo space | Identical stereo signal | Subtle stereo early-reflection effect with amount control and soft-clipped output |
| UI | One instrument selector and 5 knobs | Instrument + articulation selectors and 6 knobs |
| Testing | Simple voice/release smoke | 3 sample rates, extremes, controllers, articulations, WAV sound demo |

`Staccato` automatically releases in approximately 170 ms (plus short fade). `Swell` sets at least 550 ms attack. Both are **envelope sketches**, not recorded performer articulations.

## What's in the ZIP

- `Source/` — JUCE plugin wrapper, MIDI synth, JUCE-independent DSP and GUI.
- `CMakeLists.txt` — JUCE 9.0.4 as a pinned CMake dependency (fetched when configuring).
- `build-mac.sh` — build AU, VST3, Standalone via Xcode generator.
- `tests/run-smoke.sh` — JUCE-free C++ functional tests.
- `tests/render-demo.sh` — render the six synthesized timbres to a WAV file.
- `Preview/WindLab_0.2_demo.wav` — a small mono preview: trumpet, tenor sax, trombone, flute, clarinet, horn, **in that order**. Do not interpret this as a realistic audio library.
- `TESTING.md` — validation results and untested paths.

## Build on an Apple Silicon Mac

Prerequisites: macOS, Xcode and Xcode Command Line Tools, Git, CMake >= 3.22, internet to fetch JUCE. Ensure the licensing terms of JUCE are compatible with intended distribution.

```bash
xcode-select --install
# If CMake isn't installed already: brew install cmake
cd /path/to/WindLab_Mac_v0.2
chmod +x build-mac.sh
./build-mac.sh
```

For a locally checked-out JUCE source tree, configure without fetching JUCE:

```bash
cmake -S . -B build -G Xcode -DJUCE_SOURCE_DIR=/absolute/path/to/JUCE -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --config Release --parallel 4
ctest --test-dir build --build-config Release --output-on-failure
```

Find generated bundles:

```bash
find build -type d \( -name 'WindLab.vst3' -o -name 'WindLab.component' -o -name 'WindLab.app' \)
```

Install local AU/VST3 builds:

```bash
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3" "$HOME/Library/Audio/Plug-Ins/Components"
cp -R build/WindLab_artefacts/Release/VST3/WindLab.vst3 "$HOME/Library/Audio/Plug-Ins/VST3/"
cp -R build/WindLab_artefacts/Release/AU/WindLab.component "$HOME/Library/Audio/Plug-Ins/Components/"
```

If output paths differ, locate the bundles using `find` as above. Restart/rescan the relevant DAW if needed. For wider distribution, sign and notarize with Apple Developer ID and arrange manufacturer/plugin identity before a public release. Pro Tools additionally needs AAX, not included.

## How to play

1. Insert WindLab as an instrument plugin in your DAW, or open the Standalone build.
2. Play MIDI notes or click the 61-note on-screen keyboard (`C2` through `C7`).
3. Choose one of six instruments and an articulation.
4. Shape `Attack`, `Release`, `Brightness`, `Air`, `Space`, `Output`.
5. Send MIDI CC1 = vibrato, CC2 = breath gain, CC11 = expression, aftertouch = brightness/dynamics, pitch bend ±2 semitones. If no CC2 has been sent, breath gain defaults to 100%; if CC2 is sent with value 0 it deliberately mutes the current channel until raised again.
6. Change articulation or instrument, then trigger a new note; these two selections are sampled at note-on rather than morphing sustained voices mid-note.
7. Test a 12-note chord, MIDI note-off, sustain pedal, host automation, save/reopen session, sample rates and CPU load inside each DAW.

## Known limitations

- **Not authentic woodwind/brass samples.** Realism requires recorded multi-dynamic and multi-articulation libraries (or advanced physical modelling), real legato transition mapping, round robins, tunings, sample layers, and controllable noise/transients.
- `Space` is a two-tap early-reflection / short-delay sketch, *not* a concert hall reverb.
- Staccato and Swell are envelope behaviours rather than complete musically captured articulations.
- Voice count is fixed at 12, no advanced legato/divisi or ensemble arranging. No native preset browser yet (the host saves automatable parameters).
- No MPE per-note pitch, microtuning, sample drag/drop, or mic positions. Basic per-note aftertouch is supported by JUCE voice callback.
- Changing attack/release or articulation/instrument affects new notes; other controls update held voices.
- **No macOS or DAW binary build has been executed in this environment**. The JUCE-free C++ DSP tests pass on Linux. You must verify AU and VST3 build, performance and format validation using the target Mac and versions of the hosts.
- IDs are provisional; finalize bundle ID and manufacturer/product codes before shipping to prevent session compatibility breakage.

## Roadmap (recommended)

**0.3 — Authentic trumpet sample engine.** Record or license 3–4 velocity layers, notes at sensible intervals over trumpet's playable range, note starts/sustain/release, staccato, marcato. Implement resampling, SFZ or custom note/velocity mapping, round robins and audio asset manifest with legal metadata.

**0.4 — Performance realism.** Connect legato transitions between intervals, breath control crossfades, vibrato style/lag, ensemble unison detuning, instrument-specific attack and release noise.

**0.5 — Brass section builder.** Independent trumpet 1/2, tenor sax, trombone parts, divisi/voice-leading, panorama, timing offsets and preset management.

**0.6 — Production.** Profiling and anti-denormal monitoring, unit/host QA on Apple Silicon and Intel, accurate plugin IDs, binary signing/notarization, installer, license and redistribution review.

## Licensing

JUCE is distributed under its own licensing terms, potentially including AGPLv3/commercial options depending on the edition and mode of use; consult https://juce.com/get-juce/ and https://juce.com/legal/. Redistribution of third-party audio samples requires separate permission, even when the sound library can otherwise be used to produce music.

## API links

- https://docs.juce.com/master/classjuce_1_1Synthesiser.html
- https://docs.juce.com/master/classjuce_1_1SynthesiserVoice.html
- https://github.com/juce-framework/JUCE/blob/master/examples/CMake/AudioPlugin/CMakeLists.txt
