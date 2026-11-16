"""Heart-rate variability from beat times (ms). Mirrors spike::analysis in C++ (see analysis.h for the definitions).

Time domain (Task Force 1996): SDNN (sample sd of RR), RMSSD, pNN50. Frequency domain: Lomb-Scargle periodogram of the mean-removed
RR tachogram at the beat times, density S(f) = P(f) * 2T/N in ms^2/Hz, LF 0.04-0.15 Hz and HF 0.15-0.4 Hz band powers.
"""
from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np


@dataclass
class HrvParams:
    lf_lo_hz: float = 0.04
    lf_hi_hz: float = 0.15
    hf_lo_hz: float = 0.15
    hf_hi_hz: float = 0.40
    df_hz: float = 0.001
    min_beats_spectrum: int = 9


@dataclass
class HrvResult:
    n_beats: int = 0
    n_rr: int = 0
    mean_rr_ms: float = math.nan
    mean_hr_bpm: float = math.nan
    sdnn_ms: float = math.nan
    rmssd_ms: float = math.nan
    pnn50_pct: float = math.nan
    lf_ms2: float = math.nan
    hf_ms2: float = math.nan
    lf_hf: float = math.nan
    lf_nu: float = math.nan
    hf_nu: float = math.nan
    spectrum_ok: bool = False


def _tachogram(beats):
    b = np.sort(np.asarray(beats, dtype=np.float64))
    if len(b) < 2:
        return np.zeros(0), np.zeros(0)
    return (b[1:] - b[0]) * 1e-3, np.diff(b)


def _density(t, rr, freqs):
    n = len(rr)
    if n < 3:
        return np.zeros(0)
    y = rr - rr.mean()
    scale = 2.0 * t[-1] / n
    out = np.zeros(len(freqs))
    for i, f in enumerate(freqs):
        if f <= 0:
            continue
        w = 2.0 * math.pi * f
        tau = math.atan2(np.sin(2 * w * t).sum(), np.cos(2 * w * t).sum()) / (2 * w)
        c = np.cos(w * (t - tau))
        s = np.sin(w * (t - tau))
        cc, ss = (c * c).sum(), (s * s).sum()
        p = 0.0
        if cc > 0:
            p += (y * c).sum() ** 2 / cc
        if ss > 0:
            p += (y * s).sum() ** 2 / ss
        out[i] = 0.5 * p * scale
    return out


def lomb_scargle(beat_times_ms, freqs_hz) -> np.ndarray:
    """Density S(f) in ms^2/Hz of the RR tachogram (empty if fewer than 3 RR values)."""
    t, rr = _tachogram(beat_times_ms)
    return _density(t, rr, np.asarray(freqs_hz, dtype=np.float64))


def _band(t, rr, lo, hi, df):
    nk = max(1, int(round((hi - lo) / df)))
    f = lo + (np.arange(nk) + 0.5) * df
    return float(_density(t, rr, f).sum() * df)


def hrv(beat_times_ms, params: HrvParams | None = None) -> HrvResult:
    p = params or HrvParams()
    t, rr = _tachogram(beat_times_ms)
    r = HrvResult(n_beats=len(np.asarray(beat_times_ms)), n_rr=len(rr))
    n = len(rr)
    if n < 1:
        return r
    mean = float(rr.mean())
    r.mean_rr_ms = mean
    r.mean_hr_bpm = 60000.0 / mean if mean > 0 else math.nan
    if n >= 2:
        r.sdnn_ms = float(math.sqrt(((rr - mean) ** 2).sum() / (n - 1)))
        d = np.diff(rr)
        r.rmssd_ms = float(math.sqrt((d * d).sum() / (n - 1)))
        r.pnn50_pct = 100.0 * float((np.abs(d) > 50.0).sum()) / (n - 1)
    if r.n_beats >= p.min_beats_spectrum and t[-1] > 0:
        r.spectrum_ok = True
        r.lf_ms2 = _band(t, rr, p.lf_lo_hz, p.lf_hi_hz, p.df_hz)
        r.hf_ms2 = _band(t, rr, p.hf_lo_hz, p.hf_hi_hz, p.df_hz)
        r.lf_hf = r.lf_ms2 / r.hf_ms2 if r.hf_ms2 > 0 else math.nan
        tot = r.lf_ms2 + r.hf_ms2
        if tot > 0:
            r.lf_nu = 100.0 * r.lf_ms2 / tot
            r.hf_nu = 100.0 * r.hf_ms2 / tot
    return r
