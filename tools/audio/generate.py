#!/usr/bin/env python3
"""Render the Spike sound set into godot/assets/audio (committed output; rerun only when editing sounds).

usage: python3 tools/audio/generate.py
Writes godot/assets/audio/sfx/*.wav (mono 16-bit) and catalog.json. Needs numpy only. Deterministic: two runs give
byte-identical files. Every sound is synthesized here (sines, filtered noise, envelopes): nothing is sampled, nothing to license.

Groups: spike (click voices by population), body (heart, breath, twitch, gut), ui (cues), ambient (loop).
Levels are the per-cue `db` in the catalog; the Spikes / SFX / Music buses and the settings sliders sit on top."""
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from synth import SR, bell, decay, fade, lowpass, mtof, periodic_noise, place, rng, t_axis, write_wav  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "godot" / "assets" / "audio"
N_PITCH = 8
# C major pentatonic over two octaves: any mix of populations stays consonant, so a crackle sounds like a texture, not a clash.
PENT = [0, 2, 4, 7, 9, 12, 14, 16]


# ---------- spikes ----------

def click_exc(k):
    """Excitatory: warm. Low-mid damped sine with a soft octave and a lowpassed pop; ~25 ms."""
    f = mtof(60 + PENT[k]) * 2          # 523 Hz .. 1.7 kHz
    t = t_axis(0.03)
    x = np.sin(2 * np.pi * f * t) * decay(t, 0.0055) + 0.28 * np.sin(2 * np.pi * 2 * f * t) * decay(t, 0.0025)
    pop = lowpass(rng(f"e{k}").standard_normal(len(t)), 1800) * decay(t, 0.0008) * 1.2
    return fade(x + pop, 0.3, 4)


def click_inh(k):
    """Inhibitory: cool. Higher, thinner, slightly inharmonic (glassy tick); ~15 ms."""
    f = mtof(72 + PENT[k]) * 2          # 1.05 kHz .. 3.5 kHz
    t = t_axis(0.02)
    x = np.sin(2 * np.pi * f * t) * decay(t, 0.0028) + 0.4 * np.sin(2 * np.pi * 2.76 * f * t) * decay(t, 0.0012)
    tick = rng(f"i{k}").standard_normal(len(t))
    tick = (tick - lowpass(tick, 4000)) * decay(t, 0.0004) * 0.6   # high-passed
    return fade(x + tick, 0.2, 3)


# ---------- body ----------

def thump(f0, f1, dur, tau, nz=0.15, seed="t"):
    t = t_axis(dur)
    ph = 2 * np.pi * np.cumsum(f1 + (f0 - f1) * np.exp(-t / 0.025)) / SR
    x = np.sin(ph) * decay(t, tau) * np.minimum(1, t / 0.004)
    x += nz * lowpass(rng(seed).standard_normal(len(t)), 300) * decay(t, tau * 0.5)
    return fade(x, 1, 10)


def heart_lub():
    return thump(80, 48, 0.16, 0.05, seed="lub")


def heart_dub():
    return thump(100, 62, 0.12, 0.035, seed="dub") * 0.75


def twitch():
    t = t_axis(0.14)
    x = thump(130, 60, 0.14, 0.03, nz=0.5, seed="twitch")
    x += 0.2 * lowpass(rng("tw2").standard_normal(len(t)), 900) * decay(t, 0.006)
    return fade(x, 0.5, 15)


def gut(k):
    """Subtle gurgle: a slow glide with fast FM wobble, low and round."""
    g = rng(f"gut{k}")
    dur = 0.35 + 0.1 * k
    t = t_axis(dur)
    f = 110 + 40 * k + 60 * np.sin(2 * np.pi * (3 + k) * t) * np.exp(-t / 0.2)
    ph = 2 * np.pi * np.cumsum(f) / SR
    wob = np.sin(ph + 2.5 * np.sin(2 * np.pi * (14 + 3 * k) * t))
    env = np.sin(np.pi * t / dur) ** 2
    return fade(lowpass(wob, 500) * env + 0.1 * lowpass(g.standard_normal(len(t)), 400) * env, 5, 30)


def breath_loop():
    """2 s seamless soft airflow (periodic noise 250-2200 Hz, tilted down). Level and pitch ride the lung rate at runtime."""
    return periodic_noise(2.0, 250, 2200, 77, slope=-6)


# ---------- ui ----------

def blip(freqs, dur=0.07, tau=0.04):
    return place(dur * len(freqs) + 0.1, [(i * dur, bell(f, 0.12, tau, ((1, 1), (2, 0.2)))) for i, f in enumerate(freqs)])


def ui_run():
    return blip([mtof(72), mtof(79)], 0.06, 0.05)


def ui_pause():
    return blip([mtof(76), mtof(69)], 0.07, 0.05) * 0.9


def ui_click():
    return fade(bell(mtof(84), 0.05, 0.012, ((1, 1), (2, 0.25))), 0.2, 5)


def ui_back():
    return fade(bell(mtof(72), 0.06, 0.015, ((1, 1), (2, 0.25))), 0.2, 5)


def ui_pass():
    notes = [67, 72, 76, 79]
    return place(1.3, [(i * 0.09, bell(mtof(n + 12), 0.9, 0.35)) for i, n in enumerate(notes)])


def star(n):
    base = [72, 76, 79]
    return place(0.2 + 0.7 * n, [(i * 0.11, bell(mtof(b + 12), 0.8, 0.3)) for i, b in enumerate(base[:n])]) \
        if n < 3 else place(1.4, [(i * 0.11, bell(mtof(b + 12), 1.0, 0.4)) for i, b in enumerate(base)] + [(0.38, bell(mtof(96), 0.9, 0.5))])


def ui_hint():
    return place(0.7, [(0.0, bell(mtof(86), 0.4, 0.12)), (0.12, bell(mtof(90), 0.5, 0.15))]) * 0.8


def ui_lesion_cut():
    t = t_axis(0.26)
    zap = np.sin(2 * np.pi * np.cumsum(1800 * np.exp(-t / 0.06) + 120) / SR) * decay(t, 0.07)
    sw = rng("cut").standard_normal(len(t))
    sw = (sw - lowpass(sw, 2500)) * decay(t, 0.02)
    return fade(0.8 * zap + 0.6 * sw, 0.3, 20)


def ui_celebrate():
    g = rng("cel")
    notes = [72, 76, 79, 84, 88, 91, 96]
    parts = [(i * 0.075, bell(mtof(n), 0.9, 0.3)) for i, n in enumerate(notes)]
    parts += [(0.5 + 0.06 * i, bell(mtof(96 + int(g.integers(0, 8)) % 5 * 2), 0.5, 0.1) * 0.35) for i in range(10)]
    parts.append((0.55, bell(mtof(84), 1.6, 0.6) * 0.5 + bell(mtof(91), 1.6, 0.6) * 0.4))
    return place(2.4, parts)


def ui_fail():
    return place(0.8, [(0.0, bell(mtof(64), 0.5, 0.18)), (0.13, bell(mtof(60), 0.6, 0.25))]) * 0.9


def ambient_loop():
    """8 s seamless lab bed: very low room tone (periodic noise 60-700 Hz) plus a faint mains hum at 50 / 100 / 150 Hz.
    Hum frequencies are whole cycles per loop, so the loop has no seam."""
    sr = 22050
    t = np.arange(8 * sr) / sr
    room = periodic_noise(8.0, 60, 700, 5, sr=sr, slope=-9)
    hum = 0.25 * np.sin(2 * np.pi * 50 * t) + 0.08 * np.sin(2 * np.pi * 100 * t + 1) + 0.04 * np.sin(2 * np.pi * 150 * t + 2)
    drift = 0.5 + 0.5 * np.sin(2 * np.pi * t / 8.0)           # one slow swell per loop
    return room * (0.5 + 0.3 * drift) + hum


# name -> (fn, group, db, jitter, sample_rate)
def build_catalog():
    c = {}
    for k in range(N_PITCH):
        c[f"spike_exc_{k}"] = (lambda k=k: click_exc(k), "spike", -9.0, False)
        c[f"spike_inh_{k}"] = (lambda k=k: click_inh(k), "spike", -12.0, False)
    c["heart_lub"] = (heart_lub, "body", -6.0, False)
    c["heart_dub"] = (heart_dub, "body", -9.0, False)
    c["twitch"] = (twitch, "body", -9.0, True)
    for k in range(3):
        c[f"gut_{k}"] = (lambda k=k: gut(k), "body", -22.0, True)
    c["breath_loop"] = (breath_loop, "body", -24.0, False)
    for name, fn, db in [("ui_run", ui_run, -10), ("ui_pause", ui_pause, -10), ("ui_click", ui_click, -12), ("ui_back", ui_back, -12),
                         ("ui_pass", ui_pass, -9), ("ui_star1", lambda: star(1), -10), ("ui_star2", lambda: star(2), -10),
                         ("ui_star3", lambda: star(3), -8), ("ui_hint", ui_hint, -12), ("ui_lesion_cut", ui_lesion_cut, -9),
                         ("ui_celebrate", ui_celebrate, -7), ("ui_fail", ui_fail, -11)]:
        c[name] = (fn, "ui", float(db), False)
    c["ambient_lab"] = (ambient_loop, "ambient", -34.0, False)
    return c


def main():
    d = OUT / "sfx"
    d.mkdir(parents=True, exist_ok=True)
    cat = {}
    for name, (fn, group, db, jitter) in build_catalog().items():
        w = fn()
        sr = 22050 if name == "ambient_lab" else SR
        write_wav(d / f"{name}.wav", w, peak=0.89, sr=sr)
        cat[name] = {"group": group, "db": db, "jitter": jitter, "ms": int(len(w) / sr * 1000), "loop": group in ("ambient",) or name == "breath_loop"}
        print(f"{name:16s} {group:8s} {len(w) / sr * 1000:6.0f} ms")
    (OUT / "catalog.json").write_text(json.dumps({"version": 1, "pitches": N_PITCH, "sfx": cat}, indent=1, sort_keys=True) + "\n")
    print(f"{len(cat)} sounds -> {OUT}")


if __name__ == "__main__":
    main()
