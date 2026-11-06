# Changelog WindLab

Cronologia delle versioni precedenti alla 0.9.3. La versione corrente è descritta nel [README](README.md).

## Precedente: 0.9.2 — MIDI Learn

# WindLab 0.9.2 — MIDI Learn per fiato, vibrato ed espressione

**MIDI LEARN**, disponibile nella schermata principale del plugin,
permette di assegnare con un movimento di pedale, slider o controller
hardware uno dei messaggi MIDI CC alle tre funzioni espressive:
**VIBRATO (CC1)**, **FIATO / BREATH (CC2)**, **VOLUME (CC11)**.

- Premi MIDI LEARN → IMPARA per la destinazione → muovi il controller.
- Il pannello mostra il numero CC appreso. ANNULLA disarma l'ascolto;
  RESET CC ripristina CC1, CC2 e CC11.
- Mappature personalizzate salvate e ripristinate nel progetto AU/VST3
  come stato del plugin; i progetti più vecchi restano sui CC originali.
- Cambiare assegnazione di un CC già assegnato a un'altra funzione **scambia**
  i numeri; le tre funzioni non si sovrappongono.
- **Canale MIDI 16** del Phrase Editor escluso da learn/routing.
- Un eventuale pedale CC64 mappato come espressione non invia anche
  sustain tradizionale durante quella mappatura.
- Il motore non modifica i campioni CC0, l'identità del plugin o i vecchi
  ID di automazione. Sono previste compilazioni AU/VST3/Standalone Mac
  e VST3/Standalone Windows in CI.

Il pulsante **?** della guida apre il capitolo MIDI Learn.
Consulta il [manuale italiano](docs/Guida-Utente-WindLab-IT.md)
per la procedura completa e le limitazioni attuali.

## Precedente: 0.9.1 — Guida integrata "?"

# WindLab 0.9.1 — Guida integrata "?" (AU / VST3 / Standalone)

WindLab include ora una **guida offline completa** richiamabile dal pulsante
**?** in alto nella schermata principale. Il **?** dentro il **PHRASE EDITOR**
apre direttamente le istruzioni relative a note, legato e corsie espressive;
il precedente pulsante **MIDI ?** apre il capitolo MIDI/controller.

La guida include **12 capitoli navigabili**, scorrimento dei testi lunghi,
selettore degli argomenti, pulsanti Precedente/Successivo e Chiudi.
Funziona direttamente nella finestra del plugin, senza browser né connessione.

**[Leggi la guida utente completa in italiano](docs/Guida-Utente-WindLab-IT.md)**
— installazione AU/VST3, Logic Pro, Studio One, tastiera PC, MIDI pesato,
campioni, controlli, spartito, Phrase Editor, risoluzione dei problemi,
limiti attuali e differenze fra funzioni disponibili e pianificate.

La versione 0.9.1 non altera i campioni, gli ID del plugin, gli ID dei
parametri né il salvataggio delle frasi. Il manuale e i pulsanti di aiuto
sono visibili sia in AU/VST3 sia nelle Standalone.

## Precedente: 0.9 — loop e code dei campioni

# WindLab 0.9 — Natural Sample Loop & Staccato Tail Refinement

## First realism-engine tranche (non-destructive)

The CC0 VSCO/FreePats WAV library is unchanged. This update improves how
individual recordings are reproduced in the existing AU/VST3/Standalone:

- **Sustain-loop alignment:** inspect each authentic sampled note *once* at
  instrument loading and move both internal loop boundaries toward nearby
  rising zero crossings. This reduces the risk of clicks and discontinuity
  during long sustained notes without fabricating additional recordings.
- **Real staccato tail shaping:** short original staccato WAVs now receive a
  smooth 14ms fade at their natural end; they are still one-shot recordings,
  never turned into artificial sustain loops.
- **No real-time analysis:** expensive loop scanning is done before audio
  starts; render callbacks retain immutable samples, simple arithmetic and
  allocation-free per-note cursors.
- **DSP regression coverage:** run deterministic checks for zero-crossing
  alignment, valid loop regions and sample-tail fades on macOS and Windows.

This does **not** synthesize authentic legato transitions or crossfade between
velocity layers during held notes. Future recordings or careful audio
crossfading will be needed for true legato realism; we do not call a soft
sustain loop a recorded legato transition.

## Previous: v0.8 — Score Preview, Host BPM and Vibrato

# WindLab 0.8 — Score Preview, Host BPM and Vibrato Lane

## Consolidated music editing improvements

The **PHRASE EDITOR** now includes the original v0.7 piano roll and four
expressive lanes, plus:

- **SCORE VIEW / PIANO ROLL:** alternate between the editable piano roll and
  a read-only staff preview (treble clef register E4–F5 with ledger lines).
  The preview displays pitches, sharp accidentals, visual note-duration
  guides and the LEGATO slur to the next note. It is **not** a formal
  engraving engine or a MusicXML editor; edit pitches, durations and
  legato in Piano Roll.
- **VIBRATO / MOD WHEEL (CC1):** a fifth expression lane with editable
  0–127 points. Playback interpolates vibrato between steps at the same
  control rate as breath, volume and pitch bend.
- **HOST BPM:** optionally follow the tempo reported by Logic Pro, Studio
  One or another AU/VST3 host. Turn it off for the original independent
  45–220 BPM tempo. This preference and the vibrato points are saved in
  project state alongside the existing phrase data.
- **Transport limitation:** pressing PLAY is still necessary, and the phrase
  currently loops independently of the DAW song position. HOST BPM follows
  **tempo only**, not the DAW playback state or measure/beat location.
  In Standalone, without a DAW tempo provider, the last known tempo (or
  110 BPM initially) is used.
- **Project compatibility:** existing v0.7 phrases load; their new
  vibrato lane defaults to zero and HOST BPM is off. Previous parameter
  IDs, sound bank and MIDI channel mapping remain unchanged.

The audio engine still uses recorded VSCO/FreePats CC0 instruments;
legato currently overlaps note envelopes. True recorded transitions,
DAW song-position sync, polyphonic MPE pitch and fully editable notation
remain future development tasks and are **not** claimed in v0.8.

## Previous: v0.7 Expression & Phrase Editor

# WindLab 0.7 — Expression & Phrase Editor (AU / VST3 / Standalone)

## New: edit notes, join phrases and draw breath/pitch curves

Click **PHRASE EDITOR** on the main WindLab screen. The editor is displayed
*inside* the instrument window, so the same interface works in Logic Pro (AU),
Studio One (VST3) and both Standalone applications.

- **Piano-roll grid:** 16 eighth-note steps (two bars), pitches C3–C6.
  Left-click a step to insert/select a note; drag vertically to change pitch
  and horizontally to extend the gate up to eight eighth-note steps.
  Right-click a note to erase it.
- **LEGATO:** select a step, press LEGATO to connect with a short overlapping
  note gate. This is a MIDI-style legato gesture and does **not** claim to
  provide authentic recorded inter-note legato transitions yet.
- **Editable expression lanes:** choose **BREATH CC2**, **VELOCITY**,
  **VOLUME / EXPRESSION CC11**, or **PITCH BEND** beneath the piano roll.
  Click/drag points to alter individual steps. Breath, held-note volume
  and pitch bend interpolate continuously during playback. Velocity changes
  the recorded acoustic sample layer only at note-on, as in v0.5/v0.6.
- **PLAY / STOP:** audition the repeating phrase through WindLab's actual
  audio engine; supports all six wind instruments and the existing effects.
  **DEMO** loads a simple example; **CLEAR** clears all 16 note positions.
- **Tempo:** the editor has its own 45–220 BPM tempo. It is currently a
  **free-running two-bar loop**, deliberately not synced to the Logic/Studio
  One song position. Stop the phrase before editing an unrelated host project.
- **No external routing required:** the phrase generates real internal MIDI
  notes and controllers on MIDI **channel 16**; normal keyboard/MIDI input
  remains available. Avoid external channel 16 for unrelated instruments.
- **Save and recall:** notes, per-step breath, velocity, bend, duration,
  legato and phrase BPM are embedded inside the existing AU/VST3 project state.
  For predictable project loading, playback always starts stopped.

### Current limitations

Pitch bend is channel-wide on the dedicated phrase channel, so simultaneous
overlapping note tails share the same bend curve. True per-note MPE pitch,
natural legato transition recordings, dynamic crossfade between acoustic layers,
MIDI file export and a notation/score view are **not implemented in v0.7**.
Those are candidates for v0.8/v0.9 after listening tests. The phrase is a
live internal sound sequencer, not a separate Logic MIDI region.

## Previously: v0.6 — Computer and MIDI Performance Controller



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

