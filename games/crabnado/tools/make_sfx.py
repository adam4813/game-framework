#!/usr/bin/env python3
"""Writes Crabnado's sound effects as 16-bit mono WAVs into assets/audio/crabnado/.

Python stdlib only. Run from anywhere: python3 games/crabnado/tools/make_sfx.py
"""
import math
import random
import struct
import wave
from pathlib import Path

RATE = 44100
OUT = Path(__file__).resolve().parents[3] / "assets" / "audio" / "crabnado"


def write(name, samples, gain=0.8):
    peak = max(1e-9, max(abs(s) for s in samples))
    OUT.mkdir(parents=True, exist_ok=True)
    with wave.open(str(OUT / name), "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(b"".join(struct.pack("<h", int(32767 * gain * s / peak)) for s in samples))


def n(seconds):
    return int(RATE * seconds)


def noise_burst(seconds, decay, rng):
    return [rng.uniform(-1, 1) * math.exp(-decay * i / RATE) for i in range(n(seconds))]


def click_pair(rng, gap):
    """Two sharp ticks (claw closing) with a gap in seconds."""
    out = [0.0] * n(0.12)
    for start, amp in ((0.0, 1.0), (gap, 0.7)):
        for i in range(n(0.03)):
            t = i / RATE
            out[n(start) + i] += amp * (rng.uniform(-1, 1) * 0.6 + math.sin(2 * math.pi * 2600 * t) * 0.5) * math.exp(-170 * t)
    return out


def claw_snap(rng):
    # Sharp chitin crack plus a short low knock.
    out = []
    for i in range(n(0.16)):
        t = i / RATE
        crack = rng.uniform(-1, 1) * math.exp(-90 * t)
        knock = math.sin(2 * math.pi * 180 * t) * math.exp(-40 * t) * 0.6
        out.append(crack + knock)
    return out


def bubble_pop(rng):
    out = []
    phase = 0.0
    for i in range(n(0.14)):
        t = i / RATE
        freq = 500 + 2400 * t / 0.14
        phase += 2 * math.pi * freq / RATE
        out.append(math.sin(phase) * math.exp(-30 * t) + rng.uniform(-1, 1) * 0.05 * math.exp(-60 * t))
    return out


def crest_whoosh(rng):
    # Low-passed noise swelling up then falling away.
    out = []
    lp = 0.0
    length = n(0.9)
    for i in range(length):
        t = i / length
        env = math.sin(math.pi * t) ** 2
        alpha = 0.02 + 0.2 * env
        lp += alpha * (rng.uniform(-1, 1) - lp)
        out.append(lp * env)
    return out


def molt_chime(rng):
    out = [0.0] * n(0.9)
    for k, freq in enumerate((784.0, 1046.5, 1318.5)):
        start = n(0.09 * k)
        for i in range(n(0.7)):
            t = i / RATE
            out[start + i] += math.sin(2 * math.pi * freq * t) * math.exp(-5 * t) / (k + 1) ** 0.3
    return out


def boss_slam(rng):
    out = []
    phase = 0.0
    for i in range(n(0.7)):
        t = i / RATE
        freq = 30 + 90 * math.exp(-12 * t)
        phase += 2 * math.pi * freq / RATE
        out.append(math.sin(phase) * math.exp(-5 * t) + rng.uniform(-1, 1) * 0.5 * math.exp(-30 * t))
    return out


def rogue_warning(rng):
    # Two rising beeps.
    out = [0.0] * n(0.5)
    for start in (0.0, 0.22):
        for i in range(n(0.16)):
            t = i / RATE
            env = min(1.0, t * 200) * math.exp(-6 * t)
            out[n(start) + i] += math.sin(2 * math.pi * (440 + 500 * t / 0.16) * t) * env
    return out


def main():
    rng = random.Random(7)
    write("claw_click.wav", click_pair(rng, 0.045))
    write("claw_snap.wav", claw_snap(rng), 0.7)
    write("bubble_pop.wav", bubble_pop(rng), 0.6)
    write("crest.wav", crest_whoosh(rng), 0.8)
    write("molt.wav", molt_chime(rng), 0.7)
    write("boss_slam.wav", boss_slam(rng), 0.9)
    write("rogue_warning.wav", rogue_warning(rng), 0.6)


if __name__ == "__main__":
    main()
