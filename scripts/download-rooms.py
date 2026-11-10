#!/usr/bin/env python3
"""Fetch recorded room impulse responses for WindLab's convolution reverb (0.9.10).

Source: the OpenAIR library (University of York, www.openairlib.net), published under
Creative Commons Attribution 4.0 (CC BY 4.0) and distributed as the public `openair`
dataset of audb (audeering). Only STEREO responses are used.

For each WindLab room (studio, jazz club, concert hall, church) the script picks the
first stereo response whose file name matches the room keywords, converts it to
48 kHz / 24-bit stereo WAV and writes Resources/Rooms/room_<name>.wav plus CREDITS.txt.

The rooms are optional: if anything fails (no network, package or file), the script
prints a warning and exits 0, and WindLab keeps its generated rooms.
"""
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / "Resources" / "Rooms"
ROOMS = {
    # name in WindLab : keywords looked for in the OpenAIR file names, in order of preference
    "studio": ["studio", "booth", "recording", "york_minster_chapter", "classroom", "room"],
    "club": ["club", "bar", "pub", "theatre", "theater", "woman"],
    "hall": ["concert_hall", "hall", "auditorium", "lyons"],
    "church": ["baptist", "church", "cathedral", "minster", "chapel", "abbey"],
}


def warn(message: str) -> None:
    print(f"WARNING (rooms are optional, generated rooms stay in use): {message}")


def main() -> int:
    try:
        import audb  # noqa: F401
    except ImportError:
        try:
            # macOS runners ship a Homebrew Python that refuses plain pip installs.
            subprocess.run([sys.executable, "-m", "pip", "install", "--quiet",
                            "--break-system-packages", "audb"], check=True)
        except Exception as error:  # pragma: no cover - network dependent
            warn(f"cannot install audb: {error}")
            return 0
    try:
        import audb

        db = audb.load("openair", version="1.0.0", only_metadata=True, verbose=False)
        # audb returns absolute cache paths; load_media wants the relative names.
        files = sorted({str(pathlib.PurePath(f).as_posix()) for f in db.files})
        files = [f[f.index("wav/"):] if "wav/" in f else f for f in files]
    except Exception as error:  # pragma: no cover - network dependent
        warn(f"cannot read the OpenAIR index: {error}")
        return 0

    stereo = [f for f in files if "__stereo__" in f and f.endswith(".wav")]
    print(f"OpenAIR: {len(files)} files, {len(stereo)} stereo responses")
    chosen = {}
    for room, keywords in ROOMS.items():
        for keyword in keywords:
            match = next((f for f in stereo if keyword in f.lower() and f not in chosen.values()), None)
            if match:
                chosen[room] = match
                break
    if not chosen:
        warn("no stereo response matched the room keywords")
        return 0

    OUT.mkdir(parents=True, exist_ok=True)
    credits = ["Recorded room impulse responses used by WindLab's convolution reverb.",
               "Source: OpenAIR library, University of York - www.openairlib.net",
               "License: Creative Commons Attribution 4.0 International (CC BY 4.0)",
               "Distributed through the public audb dataset 'openair' 1.0.0 (audeering).", ""]
    for room, media in chosen.items():
        try:
            paths = audb.load_media("openair", [media], version="1.0.0", sampling_rate=48000,
                                    bit_depth=24, format="wav", verbose=False)
            target = OUT / f"room_{room}.wav"
            shutil.copyfile(paths[0], target)
            credits.append(f"room_{room}.wav <- {media}")
            print(f"{room}: {media}")
        except Exception as error:  # pragma: no cover - network dependent
            warn(f"{room}: cannot download {media}: {error}")
    (OUT / "CREDITS.txt").write_text("\n".join(credits) + "\n", encoding="utf8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
