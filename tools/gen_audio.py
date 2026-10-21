#!/usr/bin/env python3
"""Generate small deterministic mono sound effects and an ambient loop.

Outputs:
  assets/src/audio/*.wav   preview copies
  src/gen/audio_gen.h      PCM arrays linked into the PSP build
"""
from __future__ import annotations

import math
import random
import struct
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC_DIR = ROOT / "assets" / "src" / "audio"
OUT_PATH = ROOT / "src" / "gen" / "audio_gen.h"

SAMPLE_RATE = 22050
TARGET_PEAK = 0.80  # peak at 80% of 16-bit full scale
TAU = math.pi * 2.0


def clamp(value: float, lo: float, hi: float) -> float:
    return lo if value < lo else hi if value > hi else value


def envelope(i: int, count: int, attack: float, release: float) -> float:
    if count <= 1:
        return 0.0
    t = i / (count - 1)
    if t < attack:
        return t / attack
    if t > 1.0 - release:
        return (1.0 - t) / release
    return 1.0


def add_note(buf: list[float], start: int, duration: float, freq: float,
             amp: float, wave_shape: str = "sine",
             attack: float = 0.02, release: float = 0.25,
             detune: float = 0.0) -> None:
    count = int(duration * SAMPLE_RATE)
    for i in range(count):
        idx = start + i
        if idx < 0 or idx >= len(buf):
            continue
        t = i / SAMPLE_RATE
        env = envelope(i, count, attack, release)
        phase = TAU * freq * t
        if wave_shape == "square":
            value = 1.0 if math.sin(phase) >= 0.0 else -1.0
        elif wave_shape == "triangle":
            value = 2.0 / math.pi * math.asin(math.sin(phase))
        elif wave_shape == "saw":
            value = 2.0 * ((freq * t) % 1.0) - 1.0
        else:
            value = math.sin(phase)
        if detune:
            value = 0.65 * value + 0.35 * math.sin(phase * (1.0 + detune))
        buf[idx] += value * amp * env


def add_noise(buf: list[float], start: int, duration: float, amp: float,
              rng: random.Random, attack: float = 0.005,
              release: float = 0.8) -> None:
    count = int(duration * SAMPLE_RATE)
    for i in range(count):
        idx = start + i
        if idx < 0 or idx >= len(buf):
            continue
        env = envelope(i, count, attack, release)
        buf[idx] += (rng.random() * 2.0 - 1.0) * amp * env


def normalize(buf: list[float], peak: float) -> list[int]:
    maximum = max((abs(x) for x in buf), default=0.0)
    if maximum < 1e-9:
        return [0] * len(buf)
    # peak is a fraction of full scale; PCM samples use the 16-bit range.
    scale = (peak * 32767.0) / maximum
    return [int(clamp(x * scale, -32768.0, 32767.0)) for x in buf]


def write_wav(name: str, samples: list[int]) -> None:
    SRC_DIR.mkdir(parents=True, exist_ok=True)
    path = SRC_DIR / f"{name}.wav"
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(struct.pack("<" + "h" * len(samples), *samples))


def make_move() -> list[int]:
    buf = [0.0] * int(0.055 * SAMPLE_RATE)
    add_note(buf, 0, 0.045, 1320.0, 0.7, "triangle",
             attack=0.01, release=0.75)
    return normalize(buf, TARGET_PEAK)


def make_select() -> list[int]:
    buf = [0.0] * int(0.14 * SAMPLE_RATE)
    add_note(buf, 0, 0.08, 660.0, 0.55, "triangle",
             attack=0.01, release=0.55)
    add_note(buf, int(0.045 * SAMPLE_RATE), 0.09, 990.0, 0.45,
             "sine", attack=0.01, release=0.7)
    return normalize(buf, TARGET_PEAK)


def make_back() -> list[int]:
    buf = [0.0] * int(0.14 * SAMPLE_RATE)
    add_note(buf, 0, 0.12, 440.0, 0.55, "triangle",
             attack=0.01, release=0.8)
    add_note(buf, 0, 0.12, 220.0, 0.25, "sine",
             attack=0.01, release=0.8)
    return normalize(buf, TARGET_PEAK)


def make_turn() -> list[int]:
    buf = [0.0] * int(0.52 * SAMPLE_RATE)
    notes = (392.0, 523.25, 659.25, 783.99)
    for i, freq in enumerate(notes):
        add_note(buf, int(i * 0.075 * SAMPLE_RATE), 0.34, freq, 0.35,
                 "triangle", attack=0.01, release=0.8)
    return normalize(buf, TARGET_PEAK)


def make_build() -> list[int]:
    buf = [0.0] * int(0.48 * SAMPLE_RATE)
    notes = (523.25, 659.25, 783.99)
    for i, freq in enumerate(notes):
        add_note(buf, int(i * 0.06 * SAMPLE_RATE), 0.38, freq, 0.4,
                 "sine", attack=0.01, release=0.75)
    return normalize(buf, TARGET_PEAK)


def make_found() -> list[int]:
    buf = [0.0] * int(0.85 * SAMPLE_RATE)
    notes = (261.63, 329.63, 392.0, 523.25, 659.25)
    for i, freq in enumerate(notes):
        add_note(buf, int(i * 0.08 * SAMPLE_RATE), 0.65, freq, 0.36,
                 "triangle", attack=0.01, release=0.75)
    return normalize(buf, TARGET_PEAK)


def make_combat() -> list[int]:
    rng = random.Random(0xC0B47)
    buf = [0.0] * int(0.32 * SAMPLE_RATE)
    add_note(buf, 0, 0.18, 110.0, 0.7, "square",
             attack=0.005, release=0.9)
    add_note(buf, 0, 0.20, 55.0, 0.45, "sine",
             attack=0.005, release=0.95)
    add_noise(buf, 0, 0.16, 0.30, rng, release=0.95)
    return normalize(buf, TARGET_PEAK)


def make_research() -> list[int]:
    buf = [0.0] * int(0.5 * SAMPLE_RATE)
    for i, freq in enumerate((880.0, 1046.5, 1318.5)):
        add_note(buf, int(i * 0.06 * SAMPLE_RATE), 0.34, freq, 0.38,
                 "sine", attack=0.01, release=0.8)
    return normalize(buf, TARGET_PEAK)


def make_ambient() -> list[int]:
    duration = 8.0
    count = int(duration * SAMPLE_RATE)
    buf = [0.0] * count
    # Four calm two-second chords: Am, F, C, G.
    chords = (
        (110.0, 164.81, 220.0),
        (87.31, 130.81, 174.61),
        (130.81, 164.81, 196.0),
        (98.0, 123.47, 146.83),
    )
    for index, chord in enumerate(chords):
        start = int(index * 2.0 * SAMPLE_RATE)
        for freq in chord:
            add_note(buf, start, 2.0, freq, 0.10, "sine",
                     attack=0.25, release=0.25, detune=0.002)
        # Gentle arpeggio over the pad.
        for step, freq in enumerate((chord[2] * 2.0, chord[1] * 2.0,
                                     chord[0] * 2.0)):
            add_note(buf, start + int(step * 0.42 * SAMPLE_RATE), 0.55,
                     freq, 0.07, "triangle",
                     attack=0.03, release=0.6)
    # A low sustained pulse that makes the loop feel less empty.
    for beat in range(16):
        start = int(beat * 0.5 * SAMPLE_RATE)
        add_note(buf, start, 0.22, 55.0, 0.06, "sine",
                 attack=0.02, release=0.8)
    return normalize(buf, TARGET_PEAK)


def write_header(clips: list[tuple[str, str, list[int]]],
                 ambient: list[int]) -> None:
    lines = [
        "/* Generated by tools/gen_audio.py. Do not edit by hand. */",
        "#ifndef AUDIO_GEN_H",
        "#define AUDIO_GEN_H",
        "",
        f"#define AUDIO_SAMPLE_RATE {SAMPLE_RATE}",
        f"#define AUDIO_AMBIENT_SAMPLES {len(ambient)}",
        "static const short audio_ambient[AUDIO_AMBIENT_SAMPLES]"
        " __attribute__((aligned(16))) = {",
    ]
    for start in range(0, len(ambient), 16):
        lines.append("    " + ", ".join(str(x) for x in ambient[start:start + 16]) + ",")
    lines += ["};", ""]
    for name, macro, samples in clips:
        lines.append(f"#define {macro} {len(samples)}")
        lines.append(f"static const short audio_{name}[{macro}]"
                     " __attribute__((aligned(16))) = {")
        for start in range(0, len(samples), 16):
            lines.append("    " + ", ".join(str(x) for x in samples[start:start + 16]) + ",")
        lines += ["};", ""]
    lines += ["#endif", ""]
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    clips = [
        ("sfx_move", "AUDIO_SFX_MOVE_SAMPLES", make_move()),
        ("sfx_select", "AUDIO_SFX_SELECT_SAMPLES", make_select()),
        ("sfx_back", "AUDIO_SFX_BACK_SAMPLES", make_back()),
        ("sfx_turn", "AUDIO_SFX_TURN_SAMPLES", make_turn()),
        ("sfx_build", "AUDIO_SFX_BUILD_SAMPLES", make_build()),
        ("sfx_found", "AUDIO_SFX_FOUND_SAMPLES", make_found()),
        ("sfx_combat", "AUDIO_SFX_COMBAT_SAMPLES", make_combat()),
        ("sfx_research", "AUDIO_SFX_RESEARCH_SAMPLES", make_research()),
    ]
    ambient = make_ambient()
    for name, _macro, samples in clips:
        peak = max((abs(x) for x in samples), default=0)
        if peak < 1000:
            raise SystemExit(f"{name}: generated silent PCM")
        write_wav(name, samples)
    if max((abs(x) for x in ambient), default=0) < 1000:
        raise SystemExit("ambient: generated silent PCM")
    write_wav("ambient", ambient)
    write_header(clips, ambient)
    print(f"generated {OUT_PATH.relative_to(ROOT)} and WAV previews "
          f"({len(ambient) / SAMPLE_RATE:.1f}s ambient, {len(clips)} SFX)")


if __name__ == "__main__":
    main()
