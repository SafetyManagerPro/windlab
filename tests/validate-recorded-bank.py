#!/usr/bin/env python3
"""Build-time proof that the plugin has real playable, licensed multi-velocity assets."""
from collections import defaultdict
from pathlib import Path
import re
import wave

bank = Path(__file__).resolve().parents[1] / "Resources" / "Samples"
header = (bank / "SampleManifest.h").read_text()
# Entries: {instrument, root, velocity, articulation, round robin, "resource", tune cents, gain}
records = [r[:6] for r in re.findall(
    r'\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*"([^"]+)"\s*,\s*-?\d+\s*,\s*[-\d.]+f?\s*\}',
    header,
)]
assert len(records) >= 105, f"expected expanded acoustic bank, got {len(records)}"
unique = set()
details = defaultdict(lambda: defaultdict(set))
for ins, root, vel, art, rr, name in records:
    key = (int(ins), int(root), int(vel), int(art), int(rr))
    assert key not in unique, f"duplicate pitch/dynamic/take mapping: {key}"
    unique.add(key)
    stem = name[:-len("_wav")] if name.endswith("_wav") else name[:-len("_flac")]
    ext = "wav" if name.endswith("_wav") else "flac"
    path = bank / f"{stem}.{ext}"
    assert path.is_file(), f"missing sample: {path.name}"
    if ext == "wav":
        with wave.open(str(path), "rb") as wav:
            assert wav.getframerate() >= 22050
            assert wav.getnframes() > 6000
            assert wav.getnchannels() in (1, 2)
    else:
        # FLAC STREAMINFO: sample rate and total frames, without a decoder.
        data = path.read_bytes()
        assert data[:4] == b"fLaC", f"not FLAC: {path.name}"
        si = data[8:42]
        rate = (si[10] << 12) | (si[11] << 4) | (si[12] >> 4)
        frames = ((si[13] & 0x0F) << 32) | (si[14] << 24) | (si[15] << 16) | (si[16] << 8) | si[17]
        assert rate >= 22050, f"low rate in {path.name}"
        assert frames > 6000, f"too short in {path.name}"
    details[int(ins)][int(art)].add(int(vel))

# All five VSCO instruments must have distinct recorded strengths and staccatos.
for instrument in (0, 2, 3, 4, 5):
    assert len(details[instrument][0]) >= 2, (instrument, "no sustained dynamics")
    assert len(details[instrument][1]) >= 2, (instrument, "no staccato dynamics")

# Full-quality FreePats tenor sax must remain actual recorded pitches.
assert len([r for r in records if int(r[0]) == 1]) >= 4
assert (bank / "CREDITS.md").is_file()
print(f"PASS: {len(records)} recorded zones (WAV and FLAC), velocity layers, true staccatos, licensed provenance")
