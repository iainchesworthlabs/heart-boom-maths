#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthesise the placeholder sound effects and music beds of the game.

    python3 tools/assets/make_placeholder_sounds.py            write apps/game/audio/*.wav
    python3 tools/assets/make_placeholder_sounds.py --check    fail when the files differ from the generator

The sounds are placeholders for a sound designer to replace (docs/audio.md). They are made from plain sine tones,
harmonics and a seeded noise source, so they are entirely this project's own work and carry its licence.

The output is the same on every machine: the script uses only the standard library, and of the floating point
operations only + - * / and floor, which IEEE 754 defines exactly. It never calls a library sine or exponential, whose
last digit can differ between platforms. The noise comes from an integer generator with a fixed seed.
"""

from __future__ import annotations

import argparse
import math
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_OUTPUT = ROOT / "apps" / "game" / "audio"

SAMPLE_RATE = 22050
PI = 3.141592653589793

# The names of the files are what apps/game/src/services/audio_assets.cpp refers to. The test of the app layer checks that
# every sound has its file and that every file here has a sound.
SOUND_FILES = (
    "coin_chime.wav",
    "soft_buzz.wav",
    "rising_tone.wav",
    "ticking.wav",
    "cartoon_pop.wav",
    "fanfare.wav",
    "whoosh.wav",
    "music_menu.wav",
    "music_play.wav",
)


def sine_cycle(phase: float) -> float:
    """sin(2 * pi * phase), by a Taylor series on one quarter of the wave, with basic operations only."""
    phase -= math.floor(phase)
    sign = 1.0
    if phase >= 0.5:
        sign = -1.0
        phase -= 0.5
    if phase > 0.25:
        phase = 0.5 - phase
    x = 2.0 * PI * phase
    x2 = x * x
    term = x
    total = x
    for n in range(1, 9):
        term = -term * x2 / ((2 * n) * (2 * n + 1))
        total += term
    return sign * total


class Noise:
    """A 32-bit linear congruential generator: white noise in [-1, 1) from an integer seed."""

    def __init__(self, seed: int) -> None:
        self.state = seed & 0xFFFFFFFF

    def next(self) -> float:
        self.state = (self.state * 1664525 + 1013904223) & 0xFFFFFFFF
        return (self.state >> 8) / 8388608.0 - 1.0


def frames(seconds: float) -> int:
    return int(seconds * SAMPLE_RATE + 0.5)


def silence(seconds: float) -> list[float]:
    return [0.0] * frames(seconds)


def note_frequency(semitones_from_a4: int) -> float:
    """Equal temperament from A4 = 440 Hz, by repeated multiplication with the twelfth root of two."""
    ratio = 1.0594630943592953
    frequency = 440.0
    for _ in range(abs(semitones_from_a4)):
        frequency = frequency * ratio if semitones_from_a4 > 0 else frequency / ratio
    return frequency


def add_tone(buffer: list[float], start: int, seconds: float, start_hz: float, end_hz: float, level: float,
             harmonics: tuple[float, ...] = (1.0,), attack: float = 0.004, decay: float = 0.9995,
             wrap: bool = False) -> None:
    """Mix one decaying tone into the buffer. The pitch glides from start_hz to end_hz over the tone.

    The envelope is a linear attack, then a decay that multiplies by a constant each sample, then a short fade
    so no tone ends on a click. With wrap the tail runs round to the start of the buffer, so a loop is seamless.
    """
    count = frames(seconds)
    attack_frames = max(1, frames(attack))
    fade_frames = max(1, frames(0.01))
    phase = 0.0
    envelope = 1.0
    for i in range(count):
        t = i / count
        hz = start_hz + (end_hz - start_hz) * t
        phase += hz / SAMPLE_RATE
        value = 0.0
        for index, weight in enumerate(harmonics):
            value += weight * sine_cycle(phase * (index + 1))
        gain = level * envelope
        if i < attack_frames:
            gain *= i / attack_frames
        if count - i < fade_frames:
            gain *= (count - i) / fade_frames
        position = start + i
        if wrap:
            position %= len(buffer)
        elif position >= len(buffer):
            break
        buffer[position] += value * gain
        envelope *= decay


def add_noise(buffer: list[float], start: int, seconds: float, noise: Noise, level: float, smoothing_start: float,
              smoothing_end: float, fade_in: float, fade_out: float) -> None:
    """Mix low-passed noise. smoothing is the weight of the previous output: 0 is white, near 1 is dull."""
    count = frames(seconds)
    fade_in_frames = max(1, frames(fade_in))
    fade_out_frames = max(1, frames(fade_out))
    previous = 0.0
    for i in range(count):
        t = i / count
        smoothing = smoothing_start + (smoothing_end - smoothing_start) * t
        previous = previous * smoothing + noise.next() * (1.0 - smoothing)
        gain = level
        if i < fade_in_frames:
            gain *= i / fade_in_frames
        if count - i < fade_out_frames:
            gain *= (count - i) / fade_out_frames
        position = start + i
        if position >= len(buffer):
            break
        buffer[position] += previous * gain


def normalise(buffer: list[float], peak: float) -> list[float]:
    """Scale so the loudest sample is at peak. A buffer of silence stays silent."""
    loudest = max(abs(v) for v in buffer)
    if loudest == 0.0:
        return buffer
    scale = peak / loudest
    return [v * scale for v in buffer]


def to_wav(buffer: list[float]) -> bytes:
    """16-bit PCM, mono, little-endian, in a canonical 44-byte header."""
    samples = bytearray()
    for value in buffer:
        value = max(-1.0, min(1.0, value))
        samples += struct.pack("<h", int(value * 32767.0 + (0.5 if value >= 0.0 else -0.5)))
    header = b"RIFF" + struct.pack("<I", 36 + len(samples)) + b"WAVE"
    header += b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, SAMPLE_RATE, SAMPLE_RATE * 2, 2, 16)
    header += b"data" + struct.pack("<I", len(samples))
    return header + bytes(samples)


# --- Effects -----------------------------------------------------------------------------------------------------------


def coin_chime() -> list[float]:
    buffer = silence(0.44)
    add_tone(buffer, 0, 0.32, note_frequency(14), note_frequency(14), 0.6, (1.0, 0.3), decay=0.99985)
    add_tone(buffer, frames(0.09), 0.33, note_frequency(19), note_frequency(19), 0.6, (1.0, 0.3), decay=0.99985)
    return normalise(buffer, 0.7)


def soft_buzz() -> list[float]:
    buffer = silence(0.3)
    add_tone(buffer, 0, 0.28, 150.0, 140.0, 0.5, (1.0, 0.5, 0.33, 0.25, 0.2), attack=0.02, decay=0.9998)
    return normalise(buffer, 0.45)


def rising_tone() -> list[float]:
    buffer = silence(0.34)
    add_tone(buffer, 0, 0.32, 520.0, 1040.0, 0.6, (1.0, 0.25), attack=0.02, decay=0.99995)
    return normalise(buffer, 0.55)


def ticking() -> list[float]:
    """One tick and one tock, a half second in all: the sound loops while the timer is in the alert zone."""
    buffer = silence(0.5)
    add_tone(buffer, 0, 0.05, 1800.0, 1500.0, 0.6, (1.0, 0.4), attack=0.001, decay=0.9985)
    add_tone(buffer, frames(0.25), 0.05, 1300.0, 1100.0, 0.45, (1.0, 0.4), attack=0.001, decay=0.9985)
    return normalise(buffer, 0.55)


def cartoon_pop() -> list[float]:
    """A pop and a falling boing. Cartoon, never frightening."""
    buffer = silence(0.55)
    add_tone(buffer, 0, 0.5, 700.0, 120.0, 0.7, (1.0, 0.4, 0.15), attack=0.002, decay=0.9997)
    add_noise(buffer, 0, 0.12, Noise(7), 0.9, 0.2, 0.7, 0.001, 0.08)
    return normalise(buffer, 0.7)


def fanfare() -> list[float]:
    buffer = silence(1.7)
    harmonics = (1.0, 0.4, 0.2)
    for offset, semitones in ((0.0, -9), (0.14, -5), (0.28, -2), (0.42, 3)):
        add_tone(buffer, frames(offset), 0.3, note_frequency(semitones), note_frequency(semitones), 0.5, harmonics,
                 decay=0.9998)
    for semitones in (3, 7, 10):
        add_tone(buffer, frames(0.62), 1.05, note_frequency(semitones), note_frequency(semitones), 0.33, harmonics,
                 attack=0.01, decay=0.99995)
    return normalise(buffer, 0.7)


def whoosh() -> list[float]:
    buffer = silence(0.44)
    add_noise(buffer, 0, 0.4, Noise(11), 1.0, 0.95, 0.4, 0.14, 0.2)
    return normalise(buffer, 0.5)


# --- Music beds --------------------------------------------------------------------------------------------------------
#
# Each bed is four bars of four beats and loops without a seam: a note whose tail passes the end runs on at the start.
# Semitones count from A4. The chords are C, A minor, F and G, so a loop closes on a note that leads back to C.

CHORDS = (
    (-21, (-9, -5, -2)),  # C: bass C3 over C4 E4 G4
    (-24, (-12, -9, -5)),  # A minor: bass A2 over A3 C4 E4
    (-28, (-16, -12, -9)),  # F: bass F2 over F3 A3 C4
    (-26, (-14, -10, -7)),  # G: bass G2 over G3 B3 D4
)


def music_menu() -> list[float]:
    """Slow and soft: a bass note on the bar and a rolling arpeggio of the chord."""
    beat = 0.5
    buffer = silence(16 * beat)
    for bar, (bass, chord) in enumerate(CHORDS):
        bar_start = frames(bar * 4 * beat)
        add_tone(buffer, bar_start, 1.8, note_frequency(bass), note_frequency(bass), 0.5, (1.0, 0.3), attack=0.02,
                 decay=0.99992, wrap=True)
        for eighth in range(8):
            semitones = chord[(0, 1, 2, 1, 2, 1, 2, 1)[eighth]] + (12 if eighth in (4, 6) else 0)
            add_tone(buffer, bar_start + frames(eighth * beat / 2), 0.7, note_frequency(semitones),
                     note_frequency(semitones), 0.22, (1.0, 0.2), attack=0.01, decay=0.9997, wrap=True)
    return normalise(buffer, 0.32)


def music_play() -> list[float]:
    """Quicker, with a bouncing bass on every beat and a short melody in the chord."""
    beat = 0.4
    buffer = silence(16 * beat)
    melody = (2, 1, 0, 1, 2, 2, 2, -1, 1, 1, 1, -1, 2, 0, 2, -1)
    for bar, (bass, chord) in enumerate(CHORDS):
        bar_start = frames(bar * 4 * beat)
        for hit in range(4):
            semitones = bass + (12 if hit % 2 else 0)
            add_tone(buffer, bar_start + frames(hit * beat), 0.3, note_frequency(semitones), note_frequency(semitones),
                     0.42, (1.0, 0.4), attack=0.005, decay=0.9993, wrap=True)
            index = melody[bar * 4 + hit]
            if index >= 0:
                lead = chord[index] + 12
                add_tone(buffer, bar_start + frames(hit * beat), 0.34, note_frequency(lead), note_frequency(lead), 0.2,
                         (1.0, 0.3, 0.1), attack=0.006, decay=0.9995, wrap=True)
    return normalise(buffer, 0.32)


BUILDERS = {
    "coin_chime.wav": coin_chime,
    "soft_buzz.wav": soft_buzz,
    "rising_tone.wav": rising_tone,
    "ticking.wav": ticking,
    "cartoon_pop.wav": cartoon_pop,
    "fanfare.wav": fanfare,
    "whoosh.wav": whoosh,
    "music_menu.wav": music_menu,
    "music_play.wav": music_play,
}


def generate() -> dict[str, bytes]:
    return {name: to_wav(build()) for name, build in BUILDERS.items()}


def check(output: Path) -> int:
    first = generate()
    if first != generate():
        print("FAIL  the generator is not deterministic: two runs differ", file=sys.stderr)
        return 1
    status = 0
    for name, content in first.items():
        path = output / name
        if not path.is_file():
            print(f"FAIL  {path} is missing", file=sys.stderr)
            status = 1
        elif path.read_bytes() != content:
            print(f"FAIL  {path} differs from the generator: run tools/assets/make_placeholder_sounds.py",
                  file=sys.stderr)
            status = 1
    extra = sorted(p.name for p in output.glob("*.wav") if p.name not in first) if output.is_dir() else []
    for name in extra:
        print(f"FAIL  {output / name} is not made by the generator", file=sys.stderr)
        status = 1
    if status == 0:
        print(f"PASS  {len(first)} placeholder sounds match the generator")
    return status


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT, help="where the .wav files go")
    parser.add_argument("--check", action="store_true", help="write nothing; fail when the files are out of date")
    args = parser.parse_args()
    if args.check:
        return check(args.output_dir)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for name, content in generate().items():
        (args.output_dir / name).write_bytes(content)
        print(f"wrote {args.output_dir / name} ({len(content)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
