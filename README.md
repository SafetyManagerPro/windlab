# WindLab 0.9.3 — REC MIDI → Phrase Editor + popup in-plugin

**CI a basso consumo:** il progetto accumula circa tre commit per PR in
modalita' Draft e compila tutti i formati solo quando la PR passa a
**Ready for review**. La build automatica finale su `main` e quella manuale
`Run workflow` restano disponibili.
[Procedura per risparmiare minuti GitHub Actions](docs/CI-Minuti-Gratuiti.md).


**REC MIDI / STOP REC / VEDI TAKE** nella schermata principale, e
**REC / STOP REC / IMPORTA** nello **Expression & Phrase Editor**.
Per usare la tastiera PC premi REC MIDI nella schermata principale: il focus
rimane sui tasti; quando la registrazione è pronta premi VEDI TAKE e infine
IMPORTA nell'editor. Con una periferica MIDI esterna puoi registrare anche
dentro il Phrase Editor.
Suona una tastiera PC, il piano grafico, un controller USB MIDI, un
pianoforte pesato o uno strumento MIDI a fiato. WindLab acquisisce fino a
**due battute / 16 step di crome** in un buffer separato (nessuna cancellazione
automatica della vecchia frase), poi con **IMPORTA** copia le note e le
curve registrate nel piano roll modificabile.

- Note-on/off, velocity, durate (gate 1..8), legato in presenza di sovrapposizione.
- Curve di fiato **CC2**, espressione **CC11**, vibrato **CC1**, pitch bend,
  anche con i CC fisici rimappati tramite MIDI Learn.
- Input canali **1–15**. Il playback interno del Phrase Editor sul
  canale **16** non viene mai registrato di nuovo.
- Sequencer limitato a **una nota per step**: un accordo sullo stesso step
  scarta le note in più. Registrazione quantizzata, non una traccia MIDI
  arbitraria e **non audio WAV**.
- Solo il risultato **importato** viene salvato nel progetto host.
  La cattura temporanea non viene ripristinata se si chiude il plugin prima
  di importarla.
- Parser di registrazione a dimensione fissa: nessuna allocazione,
  lock o I/O nell'audio callback. Test C++ dedicati per registrazione e import.
- **Phrase Editor e ?** mostrati come popup host-safe *interni* alla
  finestra WindLab, non come finestre native separate (confermato
  esplicitamente nella guida). Questo preserva la compatibilità AU,
  VST3 e Standalone.

La compatibilità delle build non prova da sola che tutti i comandi
siano cliccabili nei diversi host: sono necessari test manuali
di registrazione/guida/ritorno alla schermata principale in Windows
Standalone, Mac Standalone e Logic Pro AU.

**[Guida utente in italiano](docs/Guida-Utente-WindLab-IT.md)**

## Cronologia versioni

Le versioni precedenti alla 0.9.3 sono in [CHANGELOG.md](CHANGELOG.md).

## What's in the ZIP

- `Source/` — JUCE plugin wrapper, MIDI synth, JUCE-independent DSP and GUI.
- `CMakeLists.txt` — JUCE 9.0.5 as a pinned CMake dependency (fetched when configuring).
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
