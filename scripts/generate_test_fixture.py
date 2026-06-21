#!/usr/bin/env python3
"""Generate Tests/Fixtures/mini-album — a tiny WAV set for CI and local smoke runs."""

from __future__ import annotations

import math
import struct
import wave
from pathlib import Path

SAMPLE_RATE = 44100
TRACKS = (
    ("01 Alpha.wav", 440.0, 2.5),
    ("02 Bravo.wav", 554.37, 3.0),
    ("03 Charlie.wav", 659.25, 2.0),
    ("04 Delta.wav", 329.63, 2.5),
)


def write_stereo_wav(path: Path, frequency_hz: float, duration_sec: float, peak: float = 0.35) -> None:
    sample_count = int(SAMPLE_RATE * duration_sec)
    path.parent.mkdir(parents=True, exist_ok=True)

    with wave.open(str(path), "w") as handle:
        handle.setnchannels(2)
        handle.setsampwidth(2)
        handle.setframerate(SAMPLE_RATE)

        for index in range(sample_count):
            sample = peak * math.sin(2.0 * math.pi * frequency_hz * index / SAMPLE_RATE)
            pcm = int(max(-32768, min(32767, round(sample * 32767))))
            handle.writeframes(struct.pack("<hh", pcm, pcm))


def main() -> None:
    root = Path(__file__).resolve().parent.parent / "Tests" / "Fixtures" / "mini-album"
    for name, frequency_hz, duration_sec in TRACKS:
        write_stereo_wav(root / name, frequency_hz, duration_sec)
        print(f"Wrote {root / name}")


if __name__ == "__main__":
    main()
