#!/usr/bin/env python3
"""Fail CI if the offline in-plugin manual is lost during UI refactoring."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
header = (root / "Source/WindLabGuidePanel.h").read_text(encoding="utf8")
panel = (root / "Source/WindLabGuidePanel.cpp").read_text(encoding="utf8")
main = (root / "Source/PluginEditor.cpp").read_text(encoding="utf8")
phrase = (root / "Source/PhraseEditor.cpp").read_text(encoding="utf8")
cmake = (root / "CMakeLists.txt").read_text(encoding="utf8")
manual = (root / "docs/Guida-Utente-WindLab-IT.md").read_text(encoding="utf8")

topics = ("start", "instruments", "soundEngine", "keyboard", "midi", "logic",
          "studioOne", "controls", "phraseEditor", "notation",
          "troubleshooting", "limitations")
assert all(f"case {topic}:" in panel for topic in topics), "A help chapter is missing"
assert "topicCount" in header and "sectionTitles()" in panel
assert "article.setScrollbarsShown(true)" in panel, "Long chapters must scroll"
assert "void WindLabAudioProcessorEditor::showGuide" in main
assert 'helpButton {"?"}' in (root / "Source/PluginEditor.h").read_text(encoding="utf8")
assert "WindLabGuidePanel::start" in main
assert "WindLabGuidePanel::midi" in main
assert "WindLabGuidePanel::phraseEditor" in main
assert 'helpButton.onClick' in phrase
assert "Source/WindLabGuidePanel.cpp" in cmake and "Source/WindLabGuidePanel.h" in cmake
assert "Logic Pro" in manual and "PC / NOTEBOOK" in manual
assert "HOST BPM" in manual and "CC2" in manual
assert "legato" in manual.lower() and "AU" in manual
assert "senza Internet" in manual and "VST3" in manual
learn_header = (root / "Source/WindLabMidiLearnPanel.h").read_text(encoding="utf8")
learn_panel = (root / "Source/WindLabMidiLearnPanel.cpp").read_text(encoding="utf8")
assert "midiLearnButton" in main
assert "WindLabMidiLearnPanel" in learn_header
assert "midiLearn.arm" in learn_panel and "midiLearn.reset" in learn_panel
assert "MIDI LEARN" in panel and "MIDI LEARN" in manual
assert "Source/WindLabMidiLearnPanel.cpp" in cmake
assert "midiLearn.route" not in main  # audio routing belongs in processor, not editor

# A plugin can be compiled successfully even when a child overlay is 0x0.
# Editor setSize runs before overlays are constructed; require explicit bounds
# BEFORE addChildComponent so buttons open a visible screen on first click.
assert "setSize(1080, 922)" in main, "Editor dimensions must fit the MIDI/breath controls"
for child in ("phrasePanel", "guidePanel", "midiLearnPanel"):
    add = f"addChildComponent(*{child})"
    if add not in main:
        continue  # MIDI Learn is introduced only in 0.9.2
    born = f"{child} = std::make_unique<"
    sized = f"{child}->setBounds(getLocalBounds())"
    assert born in main and sized in main
    assert main.index(born) < main.index(sized) < main.index(add), (
        f"{child}: missing initial bounds before UI attachment; overlay will be invisible")
assert "if (phrasePanel) phrasePanel->setBounds(getLocalBounds())" in main
assert "if (guidePanel) guidePanel->setBounds(getLocalBounds())" in main

# MIDI recorder popup controls must be part of the actual plugin GUI.
phrase_header = (root / "Source/PhraseEditor.h").read_text(encoding="utf8")
recorder_header = (root / "Source/Dsp/PhraseRecorder.h").read_text(encoding="utf8")
processor = (root / "Source/PluginProcessor.cpp").read_text(encoding="utf8")
for token in ("recordButton", "importButton", "recordingStatus"):
    assert token in phrase_header and token in phrase, f"Missing {token}"
assert "phraseRecorder.beginBlock" in processor and "phraseRecorder.endBlock" in processor
assert processor.index("phraseRecorder.endBlock") < processor.index("phrase.process("), "Prevent channel16 playback recapture"
assert "capture.importInto(phrase)" in phrase
assert "quickRecordButton" in main
assert "processor.phraseRecorder.requestStart()" in main
assert "processor.phraseRecorder.requestStop()" in main
assert "VEDI TAKE" in main
assert "REC MIDI" in manual and "VEDI TAKE" in manual
assert "std::array<std::atomic<std::uint64_t>" in recorder_header
assert "Source/Dsp/PhraseRecorder.h" in cmake and "windlab_phrase_record_smoke" in cmake
assert "REC" in manual and "IMPORTA" in manual
assert "popup" in manual.lower()
# 0.9.5: grids, chords, pedal, mouse chord editing and MIDI export must be documented.
for token in ("GRID", "TRIPLET", "EXPORT MIDI", "4 note", "CC64"):
    assert token in panel, f"In-app guide misses {token}"
    assert token in manual, f"User manual misses {token}"
assert "exportButton" in phrase and "gridBox" in phrase
# 0.9.6: wind controller modes and aftertouch breath (PR #16 + #17).
for token in ("IBRIDO", "SOFFIO DA", "AFTERTOUCH", "ULTIMO AFTERTOUCH"):
    assert token in panel, f"In-app guide misses {token}"
    assert token in manual, f"User manual misses {token}"
assert '"breathSource"' in processor and "breathSourceBox" in main

print("PASS: 12 offline manual topics, main/phrase context buttons, scroll and full Italian user guide")
