#!/usr/bin/env python3
"""Fetch a reproducible, legally redistributable starter set of recorded winds.

VSCO-2-CE: CC0 https://github.com/sgossner/VSCO-2-CE
FreePats Tenor Sax: CC0 https://freepats.zenvoid.org/Reed/saxophone.html
Fetch at CI build-time to avoid bloating source Git history. Embedded in VST3/AU
using juce_add_binary_data; runtime needs no separate sample installation.
"""
import hashlib
import http.client
import io
import os
from pathlib import Path
import re
import socket
import tarfile
import time
import urllib.error
import urllib.parse
import urllib.request
import wave

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Resources" / "Samples"
OUT.mkdir(parents=True, exist_ok=True)
SOURCE = "https://raw.githubusercontent.com/sgossner/VSCO-2-CE/440300901dfe9275fd84e0b7763af1f8443ae62e/"
USER_AGENT = {"User-Agent": "WindLab-open-licensed-multisample-builder/0.4"}
# Small, musically useful three-zone starter mapping; note/octave in filenames
# follow the library's own naming, NOT a newly invented pitch assignment.
VSCO = [
    ("trumpet", 0, 45, 1, "sustain", 1, "Brass/Trumpet/sus/Sum_SHTrumpet_sus_A2_v1_rr1.wav"),
    ("trumpet", 0, 45, 3, "sustain", 1, "Brass/Trumpet/sus/Sum_SHTrumpet_sus_A2_v3_rr1.wav"),
    ("trumpet", 0, 48, 1, "sustain", 1, "Brass/Trumpet/sus/Sum_SHTrumpet_sus_C3_v1_rr1.wav"),
    ("trumpet", 0, 48, 3, "sustain", 1, "Brass/Trumpet/sus/Sum_SHTrumpet_sus_C3_v3_rr1.wav"),
    ("trumpet", 0, 55, 1, "sustain", 1, "Brass/Trumpet/sus/Sum_SHTrumpet_sus_G3_v1_rr1.wav"),
    ("trumpet", 0, 55, 3, "sustain", 1, "Brass/Trumpet/sus/Sum_SHTrumpet_sus_G3_v3_rr1.wav"),
    ("trumpet", 0, 62, 1, "sustain", 1, "Brass/Trumpet/sus/Sum_SHTrumpet_sus_D4_v1_rr1.wav"),
    ("trumpet", 0, 62, 3, "sustain", 1, "Brass/Trumpet/sus/Sum_SHTrumpet_sus_D4_v3_rr1.wav"),
    ("trumpet", 0, 65, 1, "sustain", 1, "Brass/Trumpet/sus/Sum_SHTrumpet_sus_F4_v1_rr1.wav"),
    ("trumpet", 0, 65, 3, "sustain", 1, "Brass/Trumpet/sus/Sum_SHTrumpet_sus_F4_v3_rr1.wav"),
    ("trumpet", 0, 72, 1, "sustain", 1, "Brass/Trumpet/sus/Sum_SHTrumpet_sus_C5_v1_rr1.wav"),
    ("trumpet", 0, 72, 3, "sustain", 1, "Brass/Trumpet/sus/Sum_SHTrumpet_sus_C5_v3_rr1.wav"),
    ("trumpet", 0, 48, 1, "staccato", 1, "Brass/Trumpet/stac/Sum_SHTrumpet_stac_C3_v1_rr1.wav"),
    ("trumpet", 0, 48, 1, "staccato", 2, "Brass/Trumpet/stac/Sum_SHTrumpet_stac_C3_v1_rr2.wav"),
    ("trumpet", 0, 48, 3, "staccato", 1, "Brass/Trumpet/stac/Sum_SHTrumpet_stac_C3_v3_rr1.wav"),
    ("trumpet", 0, 48, 3, "staccato", 2, "Brass/Trumpet/stac/Sum_SHTrumpet_stac_C3_v3_rr2.wav"),
    ("trumpet", 0, 58, 1, "staccato", 1, "Brass/Trumpet/stac/Sum_SHTrumpet_stac_A#3_v1_rr1.wav"),
    ("trumpet", 0, 58, 1, "staccato", 2, "Brass/Trumpet/stac/Sum_SHTrumpet_stac_A#3_v1_rr2.wav"),
    ("trumpet", 0, 58, 3, "staccato", 1, "Brass/Trumpet/stac/Sum_SHTrumpet_stac_A#3_v3_rr1.wav"),
    ("trumpet", 0, 58, 3, "staccato", 2, "Brass/Trumpet/stac/Sum_SHTrumpet_stac_A#3_v3_rr2.wav"),
    ("trumpet", 0, 72, 1, "staccato", 1, "Brass/Trumpet/stac/Sum_SHTrumpet_stac_C5_v1_rr1.wav"),
    ("trumpet", 0, 72, 1, "staccato", 2, "Brass/Trumpet/stac/Sum_SHTrumpet_stac_C5_v1_rr2.wav"),
    ("trumpet", 0, 72, 3, "staccato", 1, "Brass/Trumpet/stac/Sum_SHTrumpet_stac_C5_v3_rr1.wav"),
    ("trumpet", 0, 72, 3, "staccato", 2, "Brass/Trumpet/stac/Sum_SHTrumpet_stac_C5_v3_rr2.wav"),
    ("trombone", 2, 34, 1, "sustain", 1, "Brass/Tenor Trombone/sus/tenortbn_sus_A#1_v1_1.wav"),
    ("trombone", 2, 34, 3, "sustain", 1, "Brass/Tenor Trombone/sus/tenortbn_sus_A#1_v3_1.wav"),
    ("trombone", 2, 41, 1, "sustain", 1, "Brass/Tenor Trombone/sus/tenortbn_sus_F2_v1_1.wav"),
    ("trombone", 2, 41, 3, "sustain", 1, "Brass/Tenor Trombone/sus/tenortbn_sus_F2_v3_1.wav"),
    ("trombone", 2, 48, 1, "sustain", 1, "Brass/Tenor Trombone/sus/tenortbn_sus_C3_v1_1.wav"),
    ("trombone", 2, 48, 3, "sustain", 1, "Brass/Tenor Trombone/sus/tenortbn_sus_C3_v3_1.wav"),
    ("trombone", 2, 53, 1, "sustain", 1, "Brass/Tenor Trombone/sus/tenortbn_sus_F3_v1_1.wav"),
    ("trombone", 2, 53, 3, "sustain", 1, "Brass/Tenor Trombone/sus/tenortbn_sus_F3_v3_1.wav"),
    ("trombone", 2, 41, 1, "staccato", 1, "Brass/Tenor Trombone/stac/tenortbn_stac_F2_v1_rr1.wav"),
    ("trombone", 2, 41, 1, "staccato", 2, "Brass/Tenor Trombone/stac/tenortbn_stac_F2_v1_rr2.wav"),
    ("trombone", 2, 41, 4, "staccato", 1, "Brass/Tenor Trombone/stac/tenortbn_stac_F2_v4_rr1.wav"),
    ("trombone", 2, 41, 4, "staccato", 2, "Brass/Tenor Trombone/stac/tenortbn_stac_F2_v4_rr2.wav"),
    ("trombone", 2, 46, 1, "staccato", 1, "Brass/Tenor Trombone/stac/tenortbn_stac_A#2_v1_rr1.wav"),
    ("trombone", 2, 46, 1, "staccato", 2, "Brass/Tenor Trombone/stac/tenortbn_stac_A#2_v1_rr2.wav"),
    ("trombone", 2, 46, 4, "staccato", 1, "Brass/Tenor Trombone/stac/tenortbn_stac_A#2_v4_rr1.wav"),
    ("trombone", 2, 53, 1, "staccato", 1, "Brass/Tenor Trombone/stac/tenortbn_stac_F3_v1_rr1.wav"),
    ("trombone", 2, 53, 1, "staccato", 2, "Brass/Tenor Trombone/stac/tenortbn_stac_F3_v1_rr2.wav"),
    ("trombone", 2, 53, 4, "staccato", 1, "Brass/Tenor Trombone/stac/tenortbn_stac_F3_v4_rr1.wav"),
    ("trombone", 2, 53, 4, "staccato", 2, "Brass/Tenor Trombone/stac/tenortbn_stac_F3_v4_rr2.wav"),
    ("flute", 3, 48, 1, "sustain", 1, "Woodwinds/Flute/susNV/LDFlute_susNV_C3_v1_1.wav"),
    ("flute", 3, 48, 3, "sustain", 1, "Woodwinds/Flute/susNV/LDFlute_susNV_C3_v3_1.wav"),
    ("flute", 3, 60, 1, "sustain", 1, "Woodwinds/Flute/susNV/LDFlute_susNV_C4_v1_1.wav"),
    ("flute", 3, 60, 3, "sustain", 1, "Woodwinds/Flute/susNV/LDFlute_susNV_C4_v3_1.wav"),
    ("flute", 3, 69, 1, "sustain", 1, "Woodwinds/Flute/susNV/LDFlute_susNV_A4_v1_1.wav"),
    ("flute", 3, 69, 3, "sustain", 1, "Woodwinds/Flute/susNV/LDFlute_susNV_A4_v3_1.wav"),
    ("flute", 3, 76, 1, "sustain", 1, "Woodwinds/Flute/susNV/LDFlute_susNV_E5_v1_1.wav"),
    ("flute", 3, 76, 2, "sustain", 1, "Woodwinds/Flute/susNV/LDFlute_susNV_E5_v2_1.wav"),
    ("flute", 3, 84, 1, "sustain", 1, "Woodwinds/Flute/susNV/LDFlute_susNV_C6_v1_1.wav"),
    ("flute", 3, 60, 1, "staccato", 1, "Woodwinds/Flute/stac/LDFlute_stac_C4_v1_rr1.wav"),
    ("flute", 3, 60, 1, "staccato", 2, "Woodwinds/Flute/stac/LDFlute_stac_C4_v1_rr2.wav"),
    ("flute", 3, 60, 4, "staccato", 1, "Woodwinds/Flute/stac/LDFlute_stac_C4_v4_rr1.wav"),
    ("flute", 3, 60, 4, "staccato", 2, "Woodwinds/Flute/stac/LDFlute_stac_C4_v4_rr2.wav"),
    ("flute", 3, 69, 1, "staccato", 1, "Woodwinds/Flute/stac/LDFlute_stac_A4_v1_rr1.wav"),
    ("flute", 3, 69, 1, "staccato", 2, "Woodwinds/Flute/stac/LDFlute_stac_A4_v1_rr2.wav"),
    ("flute", 3, 69, 3, "staccato", 1, "Woodwinds/Flute/stac/LDFlute_stac_A4_v3_rr1.wav"),
    ("flute", 3, 69, 3, "staccato", 2, "Woodwinds/Flute/stac/LDFlute_stac_A4_v3_rr2.wav"),
    ("flute", 3, 76, 1, "staccato", 1, "Woodwinds/Flute/stac/LDFlute_stac_E5_v1_rr1.wav"),
    ("flute", 3, 76, 1, "staccato", 2, "Woodwinds/Flute/stac/LDFlute_stac_E5_v1_rr2.wav"),
    ("flute", 3, 76, 3, "staccato", 1, "Woodwinds/Flute/stac/LDFlute_stac_E5_v3_rr1.wav"),
    ("flute", 3, 76, 3, "staccato", 2, "Woodwinds/Flute/stac/LDFlute_stac_E5_v3_rr2.wav"),
    ("clarinet", 4, 50, 1, "sustain", 1, "Woodwinds/Clarinet/susLong/DCClar_susLong_D3_v1_rr1_sum.wav"),
    ("clarinet", 4, 50, 3, "sustain", 1, "Woodwinds/Clarinet/susLong/DCClar_susLong_D3_v3_rr1_sum.wav"),
    ("clarinet", 4, 58, 1, "sustain", 1, "Woodwinds/Clarinet/susLong/DCClar_susLong_A#3_v1_rr1_sum.wav"),
    ("clarinet", 4, 58, 3, "sustain", 1, "Woodwinds/Clarinet/susLong/DCClar_susLong_A#3_v3_rr1_sum.wav"),
    ("clarinet", 4, 65, 1, "sustain", 1, "Woodwinds/Clarinet/susLong/DCClar_susLong_F4_v1_rr1_sum.wav"),
    ("clarinet", 4, 65, 3, "sustain", 1, "Woodwinds/Clarinet/susLong/DCClar_susLong_F4_v3_rr1_sum.wav"),
    ("clarinet", 4, 74, 1, "sustain", 1, "Woodwinds/Clarinet/susLong/DCClar_susLong_D5_v1_rr1_sum.wav"),
    ("clarinet", 4, 74, 3, "sustain", 1, "Woodwinds/Clarinet/susLong/DCClar_susLong_D5_v3_rr1_sum.wav"),
    ("clarinet", 4, 78, 1, "sustain", 1, "Woodwinds/Clarinet/susLong/DCClar_susLong_F#5_v1_rr1_sum.wav"),
    ("clarinet", 4, 78, 3, "sustain", 1, "Woodwinds/Clarinet/susLong/DCClar_susLong_F#5_v3_rr1_sum.wav"),
    ("clarinet", 4, 50, 1, "staccato", 1, "Woodwinds/Clarinet/stac/DCClar_stac_D3_v1_rr1_sum.wav"),
    ("clarinet", 4, 50, 1, "staccato", 2, "Woodwinds/Clarinet/stac/DCClar_stac_D3_v1_rr2_sum.wav"),
    ("clarinet", 4, 50, 3, "staccato", 1, "Woodwinds/Clarinet/stac/DCClar_stac_D3_v3_rr1_sum.wav"),
    ("clarinet", 4, 50, 3, "staccato", 2, "Woodwinds/Clarinet/stac/DCClar_stac_D3_v3_rr2_sum.wav"),
    ("clarinet", 4, 65, 1, "staccato", 1, "Woodwinds/Clarinet/stac/DCClar_stac_F4_v1_rr1_sum.wav"),
    ("clarinet", 4, 65, 1, "staccato", 2, "Woodwinds/Clarinet/stac/DCClar_stac_F4_v1_rr2_sum.wav"),
    ("clarinet", 4, 65, 3, "staccato", 1, "Woodwinds/Clarinet/stac/DCClar_stac_F4_v3_rr1_sum.wav"),
    ("clarinet", 4, 65, 3, "staccato", 2, "Woodwinds/Clarinet/stac/DCClar_stac_F4_v3_rr2_sum.wav"),
    ("clarinet", 4, 74, 2, "staccato", 1, "Woodwinds/Clarinet/stac/DCClar_stac_D5_v2_rr1_sum.wav"),
    ("clarinet", 4, 74, 2, "staccato", 2, "Woodwinds/Clarinet/stac/DCClar_stac_D5_v2_rr2_sum.wav"),
    ("clarinet", 4, 74, 3, "staccato", 1, "Woodwinds/Clarinet/stac/DCClar_stac_D5_v3_rr1_sum.wav"),
    ("clarinet", 4, 74, 3, "staccato", 2, "Woodwinds/Clarinet/stac/DCClar_stac_D5_v3_rr2_sum.wav"),
    ("horn", 5, 34, 1, "sustain", 1, "Brass/F Horn/sus/MOHorn_sus_A#1_v1_1.wav"),
    ("horn", 5, 34, 3, "sustain", 1, "Brass/F Horn/sus/MOHorn_sus_A#1_v3_1.wav"),
    ("horn", 5, 45, 1, "sustain", 1, "Brass/F Horn/sus/MOHorn_sus_A2_v1_1.wav"),
    ("horn", 5, 45, 3, "sustain", 1, "Brass/F Horn/sus/MOHorn_sus_A2_v3_1.wav"),
    ("horn", 5, 48, 1, "sustain", 1, "Brass/F Horn/sus/MOHorn_sus_C3_v1_1.wav"),
    ("horn", 5, 48, 4, "sustain", 1, "Brass/F Horn/sus/MOHorn_sus_C3_v4_1.wav"),
    ("horn", 5, 62, 1, "sustain", 1, "Brass/F Horn/sus/MOHorn_sus_D4_v1_1.wav"),
    ("horn", 5, 65, 1, "sustain", 1, "Brass/F Horn/sus/MOHorn_sus_F4_v1_1.wav"),
    ("horn", 5, 45, 1, "staccato", 1, "Brass/F Horn/stac/MOHorn_stac_A2_v1_rr1.wav"),
    ("horn", 5, 45, 1, "staccato", 2, "Brass/F Horn/stac/MOHorn_stac_A2_v1_rr2.wav"),
    ("horn", 5, 45, 3, "staccato", 1, "Brass/F Horn/stac/MOHorn_stac_A2_v3_rr1.wav"),
    ("horn", 5, 45, 3, "staccato", 2, "Brass/F Horn/stac/MOHorn_stac_A2_v3_rr2.wav"),
    ("horn", 5, 48, 1, "staccato", 1, "Brass/F Horn/stac/MOHorn_stac_C3_v1_rr1.wav"),
    ("horn", 5, 48, 1, "staccato", 2, "Brass/F Horn/stac/MOHorn_stac_C3_v1_rr2.wav"),
    ("horn", 5, 48, 3, "staccato", 1, "Brass/F Horn/stac/MOHorn_stac_C3_v3_rr1.wav"),
    ("horn", 5, 48, 3, "staccato", 2, "Brass/F Horn/stac/MOHorn_stac_C3_v3_rr2.wav"),
    ("horn", 5, 62, 1, "staccato", 1, "Brass/F Horn/stac/MOHorn_stac_D4_v1_rr1.wav"),
    ("horn", 5, 62, 1, "staccato", 2, "Brass/F Horn/stac/MOHorn_stac_D4_v1_rr2.wav"),
    ("horn", 5, 62, 3, "staccato", 1, "Brass/F Horn/stac/MOHorn_stac_D4_v3_rr1.wav"),
    ("horn", 5, 62, 3, "staccato", 2, "Brass/F Horn/stac/MOHorn_stac_D4_v3_rr2.wav"),
]


def get(url):
    """Retry transient source outages, without swallowing permanent errors.

    Fresh GitHub Actions runners download the samples every time; a temporary
    HTTP 503 from the hosting CDN must not fail the entire plugin build.
    Non-retriable responses (for example HTTP 404) still fail immediately.
    """
    retries = 5
    retryable_http = {408, 425, 429, 500, 502, 503, 504}
    request = urllib.request.Request(url, headers=USER_AGENT)
    for attempt in range(1, retries + 1):
        try:
            with urllib.request.urlopen(request, timeout=90) as response:
                return response.read()
        except urllib.error.HTTPError as exc:
            if exc.code not in retryable_http or attempt == retries:
                raise
            problem = f"HTTP {exc.code}"
        except (urllib.error.URLError, TimeoutError, ConnectionError,
                socket.timeout, http.client.IncompleteRead,
                http.client.RemoteDisconnected) as exc:
            if attempt == retries:
                raise
            problem = type(exc).__name__
        backoff = min(2 ** attempt, 16)
        print(f"[download] transient {problem}; retry {attempt}/{retries - 1} "
              f"in {backoff}s: {url.split('/')[-1]}", flush=True)
        time.sleep(backoff)
    raise RuntimeError("unreachable: download retry loop exhausted")

def valid_wave(blob):
    with wave.open(io.BytesIO(blob), "rb") as w:
        if w.getnframes() < 6000:
            raise ValueError("Suspiciously short audio")
        if w.getcomptype() != "NONE":
            raise ValueError("Compressed WAV not supported")
        return w.getnframes(), w.getframerate()

def install(name, instrument, root, velocity_layer, articulation, round_robin,
            data, credit, provenance):
    valid_wave(data)
    name = re.sub(r"[^a-z0-9_.]", "_", name.lower())
    dest = OUT / name
    sha = hashlib.sha256(data).hexdigest()
    if not dest.exists() or hashlib.sha256(dest.read_bytes()).hexdigest() != sha:
        dest.write_bytes(data)
    rows.append((instrument, root, velocity_layer, articulation, round_robin,
                 name, credit, provenance, sha))
    print(f"  {name:<43} root={root:<3} vel={velocity_layer:<2} {articulation:<9} size={len(data)//1024}KB")

rows = []
for label, inst, root, velocity_layer, articulation, rr, path in VSCO:
    filename = f"{label}_{articulation}_{root}_v{velocity_layer}_rr{rr}.wav"
    url = SOURCE + urllib.parse.quote(path, safe="/")
    install(filename, inst, root, velocity_layer, articulation, rr, get(url),
            "VSCO 2 CE / Versilian Studios (CC0)", path)

# VCSL->FreePats tenor sax: smaller CC0 edition, looped sax notes.
# Select a handful of spaced natural-pitch zones from the bank's SFZ map.
# If mirror is unreachable, fail: do not present a synthetic sax as recorded.
SAX_SOURCE = "https://freepats.zenvoid.org/Reed/TenorSaxophone/TenorSaxophone-SFZ-20200717.tar.xz"
archive = tarfile.open(fileobj=io.BytesIO(get(SAX_SOURCE)), mode="r:xz")
members = [m for m in archive.getmembers() if m.isfile()]
sfz = [m for m in members if m.name.lower().endswith(".sfz")]
wav = [m for m in members if m.name.lower().endswith(".wav")]
if not sfz or not wav:
    raise ValueError("FreePats tenor-sax archive lacks sfz or wav")

# Parse the SFZ mapping rather than guessing sax source pitches.
def parse_sfz(text):
    regions = []
    scope = {}
    current = {}
    for line in text.splitlines():
        line = line.split("//", 1)[0].strip()
        if not line:
            continue
        for token in re.split(r"(?=<(?:global|group|region)>)+", line):
            token = token.strip()
            if not token:
                continue
            if token.startswith("<global>"):
                scope = {}
                current = {}
                token = token[len("<global>"):]
            elif token.startswith("<group>"):
                current = dict(scope)
                token = token[len("<group>"):]
            elif token.startswith("<region>"):
                if current.get("sample"):
                    regions.append(dict(current))
                current = dict(scope)
                token = token[len("<region>"):]
            for key, value in re.findall(r"([A-Za-z0-9_]+)\s*=\s*([^\s]+)", token):
                current[key.lower()] = value
                if "<global>" in line:
                    scope[key.lower()] = value
    if current.get("sample"):
        regions.append(current)
    return regions

regs = []
for m in sfz:
    txt = archive.extractfile(m).read().decode("utf-8", errors="ignore")
    regs += parse_sfz(txt)
available = {m.name.replace("\\\\", "/"): m for m in wav}
candidates = []
for reg in regs:
    rawname = reg.get("sample", "").replace("\\\\", "/")
    root = reg.get("pitch_keycenter", reg.get("key"))
    if not root or not str(root).isdigit():
        continue
    matches = [name for name in available if name.endswith("/" + rawname) or name.endswith(rawname)]
    if matches:
        vlow = int(reg.get("lovel", 0))
        candidates.append((int(root), vlow, matches[0]))
candidates = sorted(set(candidates))
if len(candidates) < 2:
    raise ValueError("Cannot map saxophone pitches from full-quality FreePats SFZ")

unique_roots = sorted(set(row[0] for row in candidates))
used_roots = set()
for target in (43, 48, 52, 55, 60, 64, 68, 72, 76):
    root = min(unique_roots, key=lambda x: abs(x - target))
    if root in used_roots:
        continue
    used_roots.add(root)
    at_root = sorted((row for row in candidates if row[0] == root),
                     key=lambda x: (x[1], x[2]))
    choices = [at_root[0]]
    if at_root[-1][2] != at_root[0][2]:
        choices.append(at_root[-1])
    for layer, (_root, _vlow, src) in enumerate(choices, start=1):
        data = archive.extractfile(available[src]).read()
        install(f"sax_sustain_{root}_v{layer}_rr1.wav", 1, root,
                layer, "sustain", 1, data,
                "FreePats tenor sax / VCSL (CC0)", src)

# Deterministic C++ map, used by the embedded JUCE WAV sampler.
# No synthetic samples are mislabeled as acoustically recorded.
samples = {(r[0], r[1], r[2], r[3], r[4], r[5]): r for r in rows}
records = sorted(samples.values(), key=lambda x: (x[0], x[3], x[1], x[2], x[4], x[5]))
header = [
    "#pragma once",
    "// Generated by scripts/download-recordings.py; license and hashes: CREDITS.md",
    "#include <array>",
    "namespace windlab {",
    "struct RecordedSampleDescription { int instrument; int midiRoot; int velocityLayer; int articulation; int roundRobin; const char* resourceName; };",
    f"inline constexpr std::array<RecordedSampleDescription, {len(records)}> sampleDescriptions {{{{"
]
for instrument, root, velocity_layer, articulation, rr, filename, *_ in records:
    resource = filename.replace(".", "_")
    art = 1 if articulation == "staccato" else 0
    header.append(f'    {{{instrument}, {root}, {velocity_layer}, {art}, {rr}, "{resource}"}},')
header += ["}};", "}"]
(OUT / "SampleManifest.h").write_text("\n".join(header) + "\n")
credits = ["# WindLab 0.5: recorded multi-velocity wind source licenses and provenance", "",
           "All bundled source recordings are CC0; copies of these samples may be embedded",
           "in WindLab AU/VST3. These samples are NOT claimed as WindLab's recordings.", "",
           "- VSCO 2 Community Edition: https://versilian-studios.com/vsco-community/",
           "  Recorded by Sam Gossner and Simon Dalzell; CC0 1.0.",
           "  GitHub pin: 440300901dfe9275fd84e0b7763af1f8443ae62e.",
           "- FreePats Tenor Sax: https://freepats.zenvoid.org/Reed/saxophone.html",
           "  VCSL recordings adapted/looped by FreePats; CC0 1.0; 2020-07-17.",
           "", "## Included recording files (SHA-256 for reproducibility)", ""]
for _, root, velocity_layer, articulation, rr, name, credit, provenance, sha in records:
    credits.append(f"- {name}: MIDI {root}; velocity layer {velocity_layer}, "
                   f"{articulation}, variation {rr}; {credit}; "
                   f"source {provenance}; sha256={sha}")
(OUT / "CREDITS.md").write_text("\n".join(credits) + "\n")
print(f"READY: {len(records)} recorded WAV instruments zones. Embedded when building WindLab.")
