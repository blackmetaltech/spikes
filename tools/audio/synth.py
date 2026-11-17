"""Tiny numpy synthesis kit for the Spike sound set. Everything is deterministic (seeded) and licence-clean: no samples,
no third-party audio, only sums of sines, filtered noise and envelopes computed here."""
import wave

import numpy as np

SR = 44100


def rng(name):
    """A reproducible generator per sound name (stable across runs and machines)."""
    return np.random.default_rng(sum(ord(c) * (i + 1) for i, c in enumerate(name)) + 12345)


def t_axis(dur, sr=SR):
    return np.arange(int(dur * sr)) / sr


def mtof(m):
    return 440.0 * 2.0 ** ((m - 69.0) / 12.0)


def decay(t, tau):
    return np.exp(-t / tau)


def fade(x, ms_in=0.5, ms_out=3.0, sr=SR):
    """Declick: short linear fade at both ends."""
    x = x.copy()
    a, b = max(1, int(ms_in * sr / 1000)), max(1, int(ms_out * sr / 1000))
    x[:a] *= np.linspace(0, 1, a)
    x[-b:] *= np.linspace(1, 0, b)
    return x


def lowpass(x, fc, sr=SR):
    """One-pole low-pass (fc Hz)."""
    a = 1.0 - np.exp(-2 * np.pi * fc / sr)
    y = np.empty_like(x)
    s = 0.0
    for i, v in enumerate(x):
        s += a * (v - s)
        y[i] = s
    return y


def periodic_noise(dur, lo, hi, seed, sr=SR, slope=0.0):
    """Band-limited noise built in the frequency domain, so it is EXACTLY periodic (a seamless loop). slope: dB/oct tilt."""
    n = int(dur * sr)
    f = np.fft.rfftfreq(n, 1 / sr)
    g = np.random.default_rng(seed)
    spec = np.zeros(len(f), complex)
    band = (f >= lo) & (f <= hi)
    amp = np.where(band, (np.maximum(f, 1.0) / max(lo, 1.0)) ** (slope / 6.0), 0.0)
    spec[band] = amp[band] * np.exp(1j * g.uniform(0, 2 * np.pi, band.sum()))
    x = np.fft.irfft(spec, n)
    return x / (np.max(np.abs(x)) + 1e-9)


def bell(freq, dur, tau, partials=((1, 1.0), (2.76, 0.35), (5.4, 0.12))):
    t = t_axis(dur)
    x = sum(a * np.sin(2 * np.pi * freq * r * t) * decay(t, tau / (1 + 0.6 * i)) for i, (r, a) in enumerate(partials))
    return x * np.minimum(1.0, t / 0.002)


def place(total, parts, sr=SR):
    """Mix (start_seconds, array) parts into one buffer."""
    out = np.zeros(int(total * sr))
    for st, x in parts:
        i = int(st * sr)
        out[i:i + len(x)] += x[: max(0, len(out) - i)]
    return out


def write_wav(path, x, peak=0.89, sr=SR):
    x = np.asarray(x, dtype=float)
    m = np.max(np.abs(x))
    if m > 0:
        x = x / m * peak
    pcm = np.round(x * 32767).astype("<i2")
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(pcm.tobytes())
