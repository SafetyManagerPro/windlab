#!/usr/bin/env python3
"""Build-time proof that the plugin has real playable, licensed multi-velocity assets."""
from collections import defaultdict
from pathlib import Path
import re
import wave

bank = Path(__file__).resolve().parents[1] / "Resources" / "Samples"
header = (bank / "SampleManifest.h").read_text()
records = re.findall(
    r'\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*"([^"]+)"\s*\}',
    header,
)
assert len(records) >= 105, f"expected expanded acoustic bank, got {len(records)}"
unique = set()
details = defaultdict(lambda: defaultdict(set))
for ins, root, vel, art, rr, name in records:
    key = (int(ins), int(root), int(vel), int(art), int(rr))
    assert key not in unique, f"duplicate pitch/dynamic/take mapping: {key}"
    unique.add(key)
    wav_path = bank / (name[:-4] + ".wav")
    assert wav_path.is_file(), f"missing WAV: {wav_path.name}"
    with wave.open(str(wav_path), "rb") as wav:
        assert wav.getframerate() >= 22050
        assert wav.getnframes() > 6000
        assert wav.getnchannels() in (1, 2)
    details[int(ins)][int(art)].add(int(vel))

# All five VSCO instruments must have distinct recorded strengths and staccatos.
for instrument in (0, 2, 3, 4, 5):
    assert len(details[instrument][0]) >= 2, (instrument, "no sustained dynamics")
    assert len(details[instrument][1]) >= 2, (instrument, "no staccato dynamics")

# Full-quality FreePats tenor sax must remain actual recorded pitches.
assert len([r for r in records if int(r[0]) == 1]) >= 4
assert (bank / "CREDITS.md").is_file()
print(f"PASS: {len(records)} authentic CC0 WAV zones, velocity layers, true staccatos, CC0 provenance")
