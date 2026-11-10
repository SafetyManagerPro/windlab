# MIDI Performance Modes — first implementation

WindLab now offers **MIDI PERFORMANCE** below the instrument selectors.

- **TASTIERA / MIDI** (default): existing keyboard, on-screen keyboard, DAW routing and existing CC mappings are unchanged.
- **STRUMENTO A FIATO**: accepts notes and expressive CC from the controller on the same MIDI channel (existing universal MIDI behaviour). Configure the hardware to transmit breath on CC2, or use **MIDI LEARN** if it uses CC11 or another MIDI CC.
- **IBRIDO: NOTE CH + SOFFIO CH**: keyboard MIDI note-on/off on the chosen **NOTE CH** (default 1); breath controller CC messages on the chosen **BREATH CH** (default 2). The plugin forwards only the three configured learned MIDI CCs (vibrato, breath, expression) from the breath channel to the note channel, and suppresses notes from the breath channel. If the two channels are set to the same number, routing is bypassed safely. MIDI on channels other than the selected breath channel remains unchanged. The internal Phrase Editor keeps channel 16.

## Setup example
1. Connect a keyboard and set it to transmit on MIDI channel **1** (or choose another NOTE CH from 1–15).
2. Connect a wind/breath controller that sends MIDI and set its transmit channel to **2** (or choose another BREATH CH from 1–15).
3. In the DAW, route both controller streams to the *same* WindLab plugin instance, or choose the relevant inputs through Standalone audio/MIDI settings.
4. Set MIDI PERFORMANCE to **IBRIDO** and choose matching NOTE CH and BREATH CH in the top control strip.
5. If the controller sends breath on CC11, open MIDI LEARN, arm **FIATO** and blow once, so the source CC11 is assigned to the breath destination. Note this changes the expression mapping by swapping existing CCs.
6. For Artinoise Bluetooth LE MIDI on Windows, pair and configure the manufacturer's recommended MIDI bridge as needed. Pairing/bridging is done outside WindLab.

## Scope and limitations
- This first version has **user-configurable hybrid channels 1–15**. Controller CC mapping presets and calibration are now available. Automatic device detection and BLE pairing UI are not implemented. A read-only incoming note/CC/aftertouch monitor is shown in MIDI Learn.
- A breath controller with **analog output** requires compatible MIDI conversion hardware; WindLab cannot receive that analog signal as native MIDI.
- In the plugin, OS hardware port selection and BLE pairing remain the responsibility of the DAW; Standalone relies on JUCE's audio/MIDI settings.
- The existing MIDI Learn, project persistence, sample bank and internal channel-16 phrase playback remain unchanged.
- Live validation should cover Windows Standalone/VST3, macOS Standalone/AU/VST3, CC2 and learned CC11 routing, both modes and switching modes while notes are held.

## Breath source (0.9.6)
**SOFFIO DA** selects where continuous breath comes from:
- **CC (MIDI LEARN)** (default): the CC learned for breath (CC2 unless changed).
- **AFTERTOUCH**: MIDI Channel Pressure becomes breath (ported from PR #16). In Hybrid mode, aftertouch on BREATH CH is moved to NOTE CH like the breath CCs. Channel 16 is never affected.

The phrase recorder stores breath after calibration, so recorded phrases play back as they were heard.

## Breath calibration
The bottom strip includes **SOGLIA** (threshold) and **CURVA** (curve).

- **Soglia / activation threshold**: 0–0.50. Values below the threshold result in silent breath; values above it are normalized to 0–1.
- **Curva / response exponent**: 0.35–3.00. 1.0 is linear; lower values give more response to soft breath, higher values give more precision at stronger breath levels.
- Defaults (0.0 and 1.0) preserve previous external MIDI CC2 behaviour.
- Calibration is applied to incoming MIDI CCs mapped to breath, before the existing WindEngine smoothing, both on held notes and before note-on. Internal Phrase Editor channel 16 is deliberately excluded from external hardware calibration.
- The settings are plugin parameters and persist in the DAW project.

**Test**: a separate JUCE-free `windlab_breath_calibration_smoke` checks the default identity curve, activation threshold, normalization, exponent response and input bounds. CI/build is still required before claiming runtime compatibility.

## Profili controller MIDI (MIDI LEARN)
Open **MIDI LEARN** and choose a preset from **PROFILO MIDI**, then press **APPLICA PROFILO**. This is explicit and overwrites the current three CC assignments; it never runs automatically on device connection. The mapping is saved in the existing MIDI Learn project state.

- **STANDARD CC2 / EWI / AEROPHONE**: vibrato CC1, breath CC2, expression CC11. For EWI or Aerophone, configure the hardware to transmit **CC2** for breath first; this is not a claim that every hardware model defaults to CC2.
- **ARTINOISE RE.CORDER CC11**: vibrato CC1, breath CC11, expression CC2 (swapped to ensure there is no conflicting assignment). Artinoise's factory breath/pressure default is CC11 and inclination typically CC1. If the device has been reconfigured, use manual MIDI Learn instead.
- **PERSONALIZZATO**: MIDI Learn remains available and can override any preset one destination at a time; saved mapping is not replaced unless APPLICA PROFILO or RESET CC is clicked.

Controller presets do not pair Bluetooth devices, change fingering, detect hardware IDs or remap notes. To use Artinoise on Windows, configure its MIDI bridge outside the plug-in and route its MIDI output to WindLab through the DAW.

## Monitor MIDI in tempo reale
Open **MIDI LEARN** to see the latest received MIDI note (channel, pitch, on/off) and continuous controller (channel, CC number, value 0-127). The display refreshes several times per second; it is intended for identifying cabling, MIDI channel and CC mappings, not as an event-history log or a precise oscilloscope. It observes incoming external MIDI before hybrid routing, does not modify the sound and excludes the internal Phrase Editor channel 16. If no event is received, it shows "nessun dato MIDI". The monitor is ephemeral and not saved with the DAW project.

The JUCE-free `windlab_midi_input_monitor_smoke` regression test verifies note on/off, CC values, channel 16 isolation and reset. Full plugin compilation and physical-controller testing are still required.
