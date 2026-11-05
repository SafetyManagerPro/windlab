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
print("PASS: 12 offline manual topics, main/phrase context buttons, scroll and full Italian user guide")
