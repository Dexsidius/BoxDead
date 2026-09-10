#!/usr/bin/env python3
"""Synthesise BoxDead's sound effects.

Like the tile art, every sample here is generated from code -- no recordings,
no third-party audio -- so the output carries no licence beyond this
repository's own and ships with the game.

Emits 16-bit mono 44.1kHz WAVs into assets/sfx/, which SDL_LoadWAV reads
directly (no SDL_mixer dependency):

    bullet_wall.wav  - a round chipping stone: bright noise tick + short ping
    enemy_hit.wav    - a wet thud: low body plus a filtered noise splat
    explosion.wav    - barrels, rockets and grenades: rumble + brown-noise roar

Usage:
    python tools/build_sfx.py
"""
import math
import os
import random
import struct
import wave

RATE = 44100


def envelope(n, attack, decay, curve=2.0):
    """attack/decay in samples; decay falls off as a power curve."""
    out = []
    for i in range(n):
        if i < attack:
            a = i / max(1, attack)
        else:
            t = (i - attack) / max(1, decay)
            a = max(0.0, 1.0 - t) ** curve
        out.append(a)
    return out


def one_pole_lowpass(sig, cutoff):
    """Simple one-pole filter; cutoff is 0..1 (fraction of Nyquist-ish)."""
    out = []
    y = 0.0
    a = max(0.001, min(0.999, cutoff))
    for x in sig:
        y += a * (x - y)
        out.append(y)
    return out


def highpass(sig, cutoff):
    """Complement of the lowpass: keeps the bright part."""
    low = one_pole_lowpass(sig, cutoff)
    return [x - l for x, l in zip(sig, low)]


def noise(n, rng):
    return [rng.uniform(-1.0, 1.0) for _ in range(n)]


def brown(n, rng, leak=0.996):
    """Integrated noise - much heavier than white, the body of an explosion."""
    out = []
    y = 0.0
    for _ in range(n):
        y = y * leak + rng.uniform(-1.0, 1.0) * 0.06
        out.append(y)
    peak = max(1e-6, max(abs(v) for v in out))
    return [v / peak for v in out]


def sine(n, freq, rate=RATE, sweep_to=None):
    out = []
    phase = 0.0
    for i in range(n):
        f = freq if sweep_to is None else freq + (sweep_to - freq) * (i / n)
        phase += 2.0 * math.pi * f / rate
        out.append(math.sin(phase))
    return out


def mix(*layers):
    n = max(len(l) for l in layers)
    out = [0.0] * n
    for l in layers:
        for i, v in enumerate(l):
            out[i] += v
    return out


def normalise(sig, peak=0.92):
    m = max(1e-6, max(abs(v) for v in sig))
    return [v / m * peak for v in sig]


def declick(sig, ms=3.0):
    """Fade the last few ms so the sample does not end on a step."""
    n = int(RATE * ms / 1000.0)
    out = list(sig)
    for i in range(min(n, len(out))):
        out[-1 - i] *= i / max(1, n)
    return out


def save(path, sig):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    frames = b''.join(struct.pack('<h', int(max(-1.0, min(1.0, v)) * 32767))
                      for v in declick(normalise(sig)))
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(frames)
    print(f"  {os.path.basename(path):18s} {len(sig) / RATE * 1000:6.0f} ms")


def bullet_wall(rng):
    """A round chipping stone: a bright noise tick with a short pitched ping."""
    n = int(RATE * 0.09)
    env = envelope(n, int(RATE * 0.0006), n, curve=3.4)
    tick = [v * e for v, e in zip(highpass(noise(n, rng), 0.55), env)]
    # A little ringing gives it the "stone chip" character rather than a hiss.
    ping_n = int(RATE * 0.05)
    ping_env = envelope(ping_n, 20, ping_n, curve=4.0)
    ping = [v * e * 0.5 for v, e in
            zip(sine(ping_n, 2100.0, sweep_to=1400.0), ping_env)]
    return mix(tick, ping)


def enemy_hit(rng):
    """A wet thud: low body plus a filtered noise splat."""
    n = int(RATE * 0.16)
    body_env = envelope(n, int(RATE * 0.001), n, curve=2.6)
    body = [v * e for v, e in zip(sine(n, 190.0, sweep_to=95.0), body_env)]
    splat_env = envelope(n, int(RATE * 0.0008), int(RATE * 0.06), curve=3.0)
    splat = [v * e * 0.85 for v, e in
             zip(one_pole_lowpass(noise(n, rng), 0.22), splat_env)]
    return mix(body, splat)


def explosion(rng):
    """Barrels, rockets and grenades: sharp crack, roar, and a long rumble."""
    n = int(RATE * 0.75)
    crack_n = int(RATE * 0.03)
    crack = [v * e * 0.7 for v, e in
             zip(highpass(noise(crack_n, rng), 0.4),
                 envelope(crack_n, 8, crack_n, curve=3.0))]
    roar_env = envelope(n, int(RATE * 0.004), n, curve=1.7)
    roar = [v * e for v, e in
            zip(one_pole_lowpass(brown(n, rng), 0.30), roar_env)]
    rumble_env = envelope(n, int(RATE * 0.01), n, curve=1.3)
    rumble = [v * e * 0.8 for v, e in
              zip(sine(n, 62.0, sweep_to=32.0), rumble_env)]
    return mix(crack + [0.0] * (n - crack_n), roar, rumble)


def main():
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(repo)
    out = os.path.join('assets', 'sfx')
    print('Synthesising BoxDead sound effects...')
    # Fixed seeds keep regeneration byte-for-byte reproducible.
    save(os.path.join(out, 'bullet_wall.wav'), bullet_wall(random.Random(11)))
    save(os.path.join(out, 'enemy_hit.wav'), enemy_hit(random.Random(22)))
    save(os.path.join(out, 'explosion.wav'), explosion(random.Random(33)))
    print('Done.')


if __name__ == '__main__':
    main()
