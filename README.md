# WindLab 0.6 — Performance Controller (AU/VST3/Standalone)

## New in v0.6 — PC keyboard, MIDI expression and weighted piano

WindLab now provides a clearly labelled **PERFORMANCE CONTROLLER** strip
directly above its on-screen piano, in both plugin and standalone versions:

1. **Input Method → PIANO + MIDI:** click the on-screen piano, or play
   USB MIDI, MIDI DIN through an interface, or your DAW's routed MIDI.
2. **Input Method → PC / NOTEBOOK:** activates computer-keyboard notes.
   Map: `A W S E D F T G Y H U J K` = C through the following C.
   After selecting PC mode, the on-screen piano receives key focus;
   click it to recover focus if another control or the host steals it.
   This feature depends on whether the DAW allows plugins keyboard focus.
3. **Octave - / +:** selects PC keyboard C2 through C6 and scrolls the
   on-screen piano accordingly, without transposing external MIDI notes.
4. **PC / MOUSE VELOCITY:** sets the 0.15–1.0 note-on intensity when
   playing with a computer keyboard or mouse. Physical MIDI velocity is
   never replaced by this slider.
5. **MIDI DYNAMICS:** four saved/automatable response profiles:
   NATURAL (linear), LIGHT TOUCH (easier loud layers),
   WEIGHTED PIANO (more control over soft attacks), and
   BREATH CONTROLLER (base sample level combined with live CC2 breath).
   The selected profile affects note-on velocity and recorded-layer
   selection, not the live dynamics of an already held note.
6. **MIDI ?** explains routing for Standalone and VST3/AU. The Standalone
   has its own **Options → Audio/MIDI Settings** for selecting connected
   devices; in plugin mode choose MIDI input in the host DAW.

### Hardware tips

- An 88-key weighted digital piano with USB-MIDI needs no audio interface
  if it already sends MIDI via USB. Connect it, select it in Standalone
  Audio/MIDI Settings or arm a MIDI/instrument track in your DAW.
- MIDI CC2 = breath; CC11 = expression; CC1 = vibrato; CC64 = sustain pedal.
  A foot expression pedal can be used if the MIDI keyboard/controller is
  configured to transmit CC11 (or CC2 when appropriate).
- A dedicated wind controller (e.g. EWI) may drive notes and breath CC2;
  check the device's transmission settings. **Advanced MPE**, actual
  fingering views and per-note continuous sample crossfading are not yet
  implemented.
- PC keyboard cannot detect a physical strike velocity, so use the
  new dedicated velocity slider. Weighted controllers preserve their
  real played MIDI velocities, modified only by the selected profile.

No instrument/sample files or pre-existing automation parameter IDs were
removed or renamed. This is not yet a replacement for an expressive
multi-mic orchestral library.

## Previous: v0.5 — Real dynamic recordings



## New in v0.5 — real dynamic and articulation layers

Unlike the 0.4 first-pass bank, the v0.5 library selects actual **soft/loud
performances** from Versilian VSCO 2 CE and **genuine staccato recordings**
with two different takes for repeat notes. There are over 100 CC0 VSCO WAV
zones, plus selected original **full-quality** FreePats tenor sax samples.

- **Velocity sensitivity:** note-on velocity selects a corresponding recorded
  instrument intensity. A weighted MIDI keyboard can now change the tone,
  not merely the gain. The exact available layer varies by instrument/zone.
- **Real staccato:** in Staccato mode the plugin plays short, originally
  recorded attacks, rather than the same looped sustain with a shorter envelope.
- **Round robin:** successive notes alternate recorded takes where available.
- **More pitch zones:** fewer and smaller repitch intervals.
- **Full-resolution tenor sax:** use the CC0 high-quality FreePats SFZ WAV
  package instead of the previous lightweight version.
- **Build hardening:** retry temporary HTTP failures, cache downloaded WAVs
  in GitHub Actions, and validate at CI time that real recordings, velocity
  layers, and actual staccatos are present.

The original Synth and Hybrid source modes, breath length, vibrato,
bass/mid/treble EQ, room reverb, MIDI CC and standalone hosts remain.
The sample library is embedded into Mac AU/VST3/Standalone and Windows
VST3/Standalone, so it does not need a separate end-user download.

This is a higher-quality multisample prototype, **not a commercial deep-sampling
library**. Natural legato transitions, seamless note-specific loop tuning,
multi-mic recording, and MPE will still require substantial further work.

**Licenses:** VSCO 2 CE and FreePats tenor sax are both CC0. The download script
pins and records original filenames; the generated `CREDITS.md` lists SHA-256.
Sources: https://versilian-studios.com/vsco-community/ and
https://freepats.zenvoid.org/Reed/saxophone.html

## Earlier: v0.4 — embedded acoustic samples


## Windows Standalone — no DAW required

The GitHub Actions **Windows x64 VST3 + Standalone** job produces separate downloads:
- `WindLab-Windows-x64-Standalone`: the zip contains `WindLab.exe`. Extract and double-click to launch. No VST host is required. Choose your audio device / MIDI keyboard in the standalone app, or play the on-screen keys.
- `WindLab-Windows-x64-VST3`: install the `WindLab.vst3` bundle in `C:\\Program Files\\Common Files\\VST3\\` and rescan your DAW.

The executable includes the same recorded CC0 samples as the VST3. Because this is an unsigned, experimental application, Windows SmartScreen might warn on launch; proceed only when you trust the binary downloaded from this repository. Distributing a signed installer and validating in Windows/DAW remains future work.


## Real musician recordings — 0.4 (experimental sample engine)

This version embeds licensed **CC0 acoustic WAV multisamples** directly into the macOS
AU / VST3 / Standalone and Windows VST3 builds. Select **ACOUSTIC - REAL WAV**
(default), **HYBRID** or **SYNTH** in the top bar.

- Trumpet, tenor trombone, clarinet, flute and French horn: recorded
  VSCO 2 CE (Versilian Studios).
- Tenor saxophone: recorded FreePats sample set, derived from Versilian VCSL.
- A handful of spaced pitch zones per instrument; closest recordings are
  repitched to incoming MIDI notes; synthetic expression follows live MIDI
  controllers and the existing 0.3 controls.
- The sampled engine uses short crossfaded sustain loops, natural note attacks
  and sample-based timbre. Loop quality varies with recording and note.
- **No acoustic multi-sample download at install time:** JUCE embeds the actual
  CC0 WAV contents as binary resources during GitHub Actions build.
- Build without assets is supported for local developer tests but uses only the
  synthetic fallback. To include recordings locally run
  `python scripts/download-recordings.py` before CMake configure.

**Current limitations:** each sample zone initially has a single velocity
layer; timbre still uses a simple envelope and approximate vibrato,
not recorded legato transitions. This is an initial genuine recorded
instrument, not commercial orchestral realism yet.

**Sample provenance:** `scripts/download-recordings.py` pins the VSCO commit
and pulls the FreePats sax pack dated 2020-07-17. It writes a full
`Resources/Samples/CREDITS.md` including each recording's origin, root pitch
and SHA-256. Sources and licenses:
- https://versilian-studios.com/vsco-community/ (CC0 1.0)
- https://freepats.zenvoid.org/Reed/saxophone.html (CC0 1.0)



**Source-code prototype, not a ready-to-install plugin.** Builds AU, VST3, and Standalone from one JUCE project, with Apple Silicon (`arm64`) as the first target. Works *in principle* in hosts accepting those formats (Ableton, Studio One, Logic, etc.) **after compiling and host validation**. This package is **not a sample library**: the six sounds are synthetic sketches for developing MIDI performance and UI.

## What's new in 0.3 — performance, tone and space

Version 0.3 introduced the underlying MIDI dynamics and controls:

- **Breath Length (2–30 s):** shorter settings naturally taper near the end of a held phrase, then trigger normal release. **30 s means continuous sustain while a MIDI key remains held**. There is no arbitrary fixed eight-second cutoff. MIDI note-off always stops the note according to Release.
- **Natural:** subtle slow pitch and amplitude variation; **Vibrato** complements mod-wheel CC1.
- **Expression response:** changing CC2 breath or aftertouch affects harmonic balance as well as overall volume; sharper attack transients and richer air noise.
- **EQ:** smooth, automatable ±12 dB bass (180 Hz shelf), mid (1.25 kHz bell) and treble (5.2 kHz shelf).
- **Space:** existing early reflections plus a new stereo algorithmic reverb with mix, room size and damping.
- **Live GUI:** fifteen real-time host-automatable knobs in PERFORMANCE, TIMBRE/EQ and SPACE/OUTPUT groups.
- **Performance:** no heap allocation in the EQ sample loop; audio parameters are smoothed, and EQ uses stable biquads.
- **Compatibility:** old APVTS IDs remain unchanged; new controls are appended. Host projects must still be checked manually.

**Important:** these improvements make expressive synthetic sketches more playable, but cannot reproduce real trumpet/saxophone legato, breath transitions or ensemble attacks without further recorded samples or physical modelling.

## Previously added in 0.2

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
4. Shape `Attack`, `Release`, `Breath Length`, `Vibrato`, `Natural`, `Brightness`, `Air`, `Bass`, `Mid`, `Treble`, `Room`, `Reverb`, `Reverb Size`, `Damping`, and `Output` live, or automate these from the DAW.
5. Send MIDI CC1 = vibrato, CC2 = breath gain, CC11 = expression, aftertouch = brightness/dynamics, pitch bend ±2 semitones. If no CC2 has been sent, breath gain defaults to 100%; if CC2 is sent with value 0 it deliberately mutes the current channel until raised again.
6. Change articulation or instrument, then trigger a new note; these two selections are sampled at note-on rather than morphing sustained voices mid-note.
7. Test a 12-note chord, MIDI note-off, sustain pedal, host automation, save/reopen session, sample rates and CPU load inside each DAW.

## Known limitations

- **Not authentic woodwind/brass samples.** Realism requires recorded multi-dynamic and multi-articulation libraries (or advanced physical modelling), real legato transition mapping, round robins, tunings, sample layers, and controllable noise/transients.
- `Room` is a two-tap early-reflection sketch. The new `Reverb` is an algorithmic stereo reverb, not an impulse response of a sampled space.
- Staccato and Swell are envelope behaviours rather than complete musically captured articulations.
- Voice count is fixed at 12, no advanced legato/divisi or ensemble arranging. No native preset browser yet (the host saves automatable parameters).
- No MPE per-note pitch, microtuning, sample drag/drop, or mic positions. Basic per-note aftertouch is supported by JUCE voice callback.
- Changing attack/release or articulation/instrument affects new notes; other controls update held voices.
- CI previously compiled WindLab 0.2 AU/VST3 on GitHub's macOS and Windows runners. The WindLab 0.3 plugin must pass the new CI and still requires host validation, performance testing and manual listening on an actual Mac/Windows DAW.
- IDs are provisional; finalize bundle ID and manufacturer/product codes before shipping to prevent session compatibility breakage.

## Roadmap (recommended)

**0.4 — Authentic trumpet sample engine.** Record or license 3–4 velocity layers, notes at sensible intervals over trumpet's playable range, note starts/sustain/release, staccato, marcato. Implement resampling, SFZ or custom note/velocity mapping, round robins and audio asset manifest with legal metadata.

**0.5 — Performance realism.** Connect legato transitions between intervals, breath control crossfades, vibrato style/lag, ensemble unison detuning, instrument-specific attack and release noise.

**0.6 — Brass section builder.** Independent trumpet 1/2, tenor sax, trombone parts, divisi/voice-leading, panorama, timing offsets and preset management.

**0.7 — Production.** Profiling and anti-denormal monitoring, unit/host QA on Apple Silicon and Intel, accurate plugin IDs, binary signing/notarization, installer, license and redistribution review.

## Licensing

JUCE is distributed under its own licensing terms, potentially including AGPLv3/commercial options depending on the edition and mode of use; consult https://juce.com/get-juce/ and https://juce.com/legal/. Redistribution of third-party audio samples requires separate permission, even when the sound library can otherwise be used to produce music.

## API links

- https://docs.juce.com/master/classjuce_1_1Synthesiser.html
- https://docs.juce.com/master/classjuce_1_1SynthesiserVoice.html
- https://github.com/juce-framework/JUCE/blob/master/examples/CMake/AudioPlugin/CMakeLists.txt
