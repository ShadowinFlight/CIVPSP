#!/usr/bin/env python3
"""Guard against silent generated WAV/PCM assets."""
from __future__ import annotations

import struct
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
AUDIO_DIR = ROOT / "assets" / "src" / "audio"
MIN_PEAK = 1000


def main() -> None:
    files = sorted(AUDIO_DIR.glob("*.wav"))
    if not files:
        raise SystemExit("no generated WAV files found")
    errors = []
    for path in files:
        with wave.open(str(path), "rb") as wav:
            frames = wav.getnframes()
            rate = wav.getframerate()
            channels = wav.getnchannels()
            width = wav.getsampwidth()
            data = wav.readframes(frames)
        values = struct.unpack("<" + "h" * (len(data) // 2), data)
        peak = max((abs(v) for v in values), default=0)
        if rate != 22050 or channels != 1 or width != 2:
            errors.append(f"{path.name}: format {rate}Hz/{channels}ch/{width*8}bit")
        if peak < MIN_PEAK:
            errors.append(f"{path.name}: silent or too quiet (peak {peak})")
        print(f"{path.name}: peak={peak} frames={frames}")
    if errors:
        for error in errors:
            print("FAIL", error)
        raise SystemExit(1)
    print(f"PASS: generated audio is non-silent ({len(files)} WAV files)")


if __name__ == "__main__":
    main()
