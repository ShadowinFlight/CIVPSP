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


CHORD_INTERVALS = {
    "major": (0, 4, 7),
    "minor": (0, 3, 7),
    "major7": (0, 4, 7, 11),
    "minor7": (0, 3, 7, 10),
    "dom7": (0, 4, 7, 10),
}


def chord_intervals(quality: str) -> tuple[int, ...]:
    return CHORD_INTERVALS.get(quality, CHORD_INTERVALS["major"])


# Eight diatonic chords per loop (one per second) give each era a clearer
# harmonic story and richer sevenths.  The arpeggio is the only melodic voice:
# every lead note therefore lands on the same eighth/sixteenth-note grid as the
# accompaniment instead of floating over it as a slower counter-melody.
MUSIC_STYLES = (
    {
        "name": "ancient",
        "root": 57,
        "chords": (
            (0, "minor"), (8, "major"), (3, "major"), (10, "major"),
            (0, "minor"), (5, "minor"), (8, "major7"), (7, "minor"),
        ),
        "pad_wave": "sine",
        "arp_wave": "triangle",
        "arp_step": 0.50,
        "pad_amp": 0.050,
        "arp_amp": 0.065,
    },
    {
        "name": "classical",
        "root": 60,
        "chords": (
            (0, "major7"), (9, "minor"), (5, "major7"), (7, "dom7"),
            (0, "major"), (5, "major"), (2, "minor7"), (7, "dom7"),
        ),
        "pad_wave": "triangle",
        "arp_wave": "triangle",
        "arp_step": 0.50,
        "pad_amp": 0.045,
        "arp_amp": 0.060,
    },
    {
        "name": "medieval",
        "root": 55,
        "chords": (
            (0, "minor"), (10, "major"), (8, "major"), (10, "major"),
            (0, "minor"), (5, "minor"), (8, "major"), (7, "minor7"),
        ),
        "pad_wave": "sine",
        "arp_wave": "square",
        "arp_step": 0.50,
        "pad_amp": 0.060,
        "arp_amp": 0.045,
    },
    {
        "name": "renaissance",
        "root": 62,
        "chords": (
            (0, "major"), (7, "dom7"), (9, "minor7"), (4, "minor"),
            (5, "major7"), (0, "major"), (2, "minor7"), (7, "dom7"),
        ),
        "pad_wave": "triangle",
        "arp_wave": "saw",
        "arp_step": 0.50,
        "pad_amp": 0.040,
        "arp_amp": 0.045,
    },
)

MUSIC_TRACK_NAMES = (
    "AUDIO_MUSIC_ANCIENT_PEACE",
    "AUDIO_MUSIC_ANCIENT_WAR",
    "AUDIO_MUSIC_CLASSICAL_PEACE",
    "AUDIO_MUSIC_CLASSICAL_WAR",
    "AUDIO_MUSIC_MEDIEVAL_PEACE",
    "AUDIO_MUSIC_MEDIEVAL_WAR",
    "AUDIO_MUSIC_RENAISSANCE_PEACE",
    "AUDIO_MUSIC_RENAISSANCE_WAR",
)


def midi_freq(note: float) -> float:
    return 440.0 * (2.0 ** ((note - 69.0) / 12.0))


def loopify(samples: list[float], seconds: float = 0.22) -> list[float]:
    """Crossfade the tail into the head and trim it for a seamless loop."""
    fade = int(seconds * SAMPLE_RATE)
    if fade < 2 or fade * 2 >= len(samples):
        return samples
    out = [0.0] * (len(samples) - fade)
    for i in range(fade):
        t = i / (fade - 1)
        out[i] = samples[i] * t + samples[len(samples) - fade + i] * (1.0 - t)
    out[fade:] = samples[fade:len(samples) - fade]
    return out


def add_war_drum(buf: list[float], start: int, amp: float,
                 rng: random.Random, deep: bool) -> None:
    add_note(buf, start, 0.22, 48.0 if deep else 62.0, amp, "sine",
             attack=0.001, release=0.85)
    add_note(buf, start, 0.07, 96.0, amp * 0.28, "triangle",
             attack=0.001, release=0.90)
    add_noise(buf, start, 0.055, amp * 0.34, rng,
              attack=0.001, release=0.95)


def make_music(style: dict, war: bool) -> list[int]:
    duration = 8.0
    count = int(duration * SAMPLE_RATE)
    buf = [0.0] * count
    rng = random.Random(0xA0D10 + sum(ord(c) for c in style["name"])
                        + (0x500 if war else 0))

    transpose = -2 if war else 0
    root = style["root"] + transpose
    # (chord-tone index, octave).  Peace stays in the same mid register as the
    # old lead; war raises every fourth arpeggio step (the seventh, or the root
    # on a triad) and the final fifth an octave for bite without turning the
    # whole loop into a high-register exercise.
    high_octave = 24 if war else 12
    arp_pattern = (
        (0, 12), (1, 12), (2, 12), (3, high_octave),
        (2, 12), (1, 12), (0, 12), (2, high_octave),
    )
    arp_index = 0
    chord_seconds = duration / float(len(style["chords"]))
    arp_step = 0.25 if war else style["arp_step"]
    pad_attack = min(0.22, chord_seconds * 0.20)
    pad_release = min(0.22, chord_seconds * 0.24)

    for chord_index, (semi, quality) in enumerate(style["chords"]):
        start = int(chord_index * chord_seconds * SAMPLE_RATE)
        base = root + semi
        intervals = chord_intervals(quality)

        # Sustained pad.  War variants drop the root an octave for weight.
        pad_base = base - (12 if war else 0)
        add_note(buf, start, chord_seconds, midi_freq(pad_base - 12),
                 style["pad_amp"] * (1.15 if war else 1.0),
                 "sine", attack=pad_attack, release=pad_release,
                 detune=0.002)
        for interval in intervals:
            add_note(buf, start, chord_seconds,
                     midi_freq(pad_base + interval),
                     style["pad_amp"], style["pad_wave"],
                     attack=pad_attack, release=pad_release, detune=0.003)

        # Bass pulse.
        pulse_step = 0.5 if war else 1.0
        pulse_count = int(chord_seconds / pulse_step)
        bass_len = 0.30 if war else min(0.85, chord_seconds * 0.8)
        for pulse in range(pulse_count):
            add_note(buf, start + int(pulse * pulse_step * SAMPLE_RATE),
                     bass_len,
                     midi_freq(base - 24),
                     (0.12 if war else 0.055),
                     "triangle", attack=0.01, release=0.75)

        # Era-specific arpeggio.  War doubles the note density.  The pattern
        # runs across chord boundaries so peace uses all eight contour steps
        # instead of restarting on the same root/third pair for every chord.
        arp_count = int(chord_seconds / arp_step)
        for step in range(arp_count):
            pattern_index, octave = arp_pattern[arp_index % len(arp_pattern)]
            arp_index += 1
            interval = intervals[pattern_index % len(intervals)]
            freq = midi_freq(base + interval + octave)
            add_note(buf, start + int(step * arp_step * SAMPLE_RATE),
                     arp_step * (1.25 if war else 1.55), freq,
                     style["arp_amp"] * (0.85 if war else 1.0),
                     style["arp_wave"], attack=0.012, release=0.58)

        # War adds a low, insistent drum and snare pattern.
        if war:
            for beat in range(int(chord_seconds / 0.5)):
                drum_start = start + int(beat * 0.5 * SAMPLE_RATE)
                if beat % 2 == 0:
                    add_war_drum(buf, drum_start, 0.23, rng, deep=True)
                if beat % 4 == 2:
                    add_war_drum(buf, drum_start, 0.12, rng, deep=False)

    # War gets a sustained low root drone.  There is deliberately no separate
    # long-note lead here: the arpeggio owns the melody, so every attack in the
    # loop shares the same rhythmic subdivision.
    if war:
        add_note(buf, 0, duration, midi_freq(root - 24), 0.045,
                 "sine", attack=0.4, release=0.4, detune=0.002)

    return normalize(loopify(buf), TARGET_PEAK)


def write_header(clips: list[tuple[str, str, list[int]]],
                 music: list[tuple[str, str, list[int]]]) -> None:
    lines = [
        "/* Generated by tools/gen_audio.py. Do not edit by hand. */",
        "#ifndef AUDIO_GEN_H",
        "#define AUDIO_GEN_H",
        "",
        f"#define AUDIO_SAMPLE_RATE {SAMPLE_RATE}",
        f"#define AUDIO_MUSIC_GENERATED_COUNT {len(music)}",
        "",
    ]
    for name, macro, samples in list(clips) + list(music):
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

    music = []
    macro_index = 0
    for style in MUSIC_STYLES:
        for war in (False, True):
            mood = "war" if war else "peace"
            name = f"music_{style['name']}_{mood}"
            macro = MUSIC_TRACK_NAMES[macro_index] + "_SAMPLES"
            samples = make_music(style, war)
            music.append((name, macro, samples))
            write_wav(name, samples)
            macro_index += 1

    for name, _macro, samples in clips:
        peak = max((abs(x) for x in samples), default=0)
        if peak < 1000:
            raise SystemExit(f"{name}: generated silent PCM")
        write_wav(name, samples)
    for name, _macro, samples in music:
        peak = max((abs(x) for x in samples), default=0)
        if peak < 1000:
            raise SystemExit(f"{name}: generated silent PCM")

    stale_ambient = SRC_DIR / "ambient.wav"
    if stale_ambient.exists():
        stale_ambient.unlink()
    write_header(clips, music)
    print(f"generated {OUT_PATH.relative_to(ROOT)} and WAV previews "
          f"({len(clips)} SFX, {len(music)} era/mood music loops)")


if __name__ == "__main__":
    main()
