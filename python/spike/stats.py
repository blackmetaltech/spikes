"""Spike-train statistics, mirroring spike::analysis in C++. Spikes are `Spikes(neuron, t_ms)` or an (N, 2) array / list of
(neuron, t_ms) pairs. Windows are [t0_ms, t1_ms)."""
from __future__ import annotations

import math
from typing import Sequence

import numpy as np


def _arrays(spikes):
    if hasattr(spikes, "neuron") and hasattr(spikes, "t_ms"):
        return np.asarray(spikes.neuron, dtype=np.int64), np.asarray(spikes.t_ms, dtype=np.float64)
    a = np.asarray(spikes, dtype=np.float64).reshape(-1, 2)
    return a[:, 0].astype(np.int64), a[:, 1]


def _window(spikes, n, t0, t1):
    nr, t = _arrays(spikes)
    m = (nr >= 0) & (nr < n) & (t >= t0) & (t < t1)
    return nr[m], t[m]


def firing_rates(spikes, n: int, t0_ms: float, t1_ms: float) -> np.ndarray:
    """Firing rate (Hz) of each of n neurons."""
    nr, _ = _window(spikes, n, t0_ms, t1_ms)
    if t1_ms <= t0_ms:
        return np.zeros(n)
    return np.bincount(nr, minlength=n).astype(float) * 1000.0 / (t1_ms - t0_ms)


def isi_cv(spikes, n: int, t0_ms: float, t1_ms: float) -> np.ndarray:
    """ISI coefficient of variation per neuron (population sd / mean); NaN with fewer than 3 spikes."""
    nr, t = _window(spikes, n, t0_ms, t1_ms)
    cv = np.full(n, np.nan)
    order = np.lexsort((t, nr))
    nr, t = nr[order], t[order]
    bounds = np.searchsorted(nr, np.arange(n + 1))
    for i in range(n):
        ts = t[bounds[i] : bounds[i + 1]]
        if len(ts) < 3:
            continue
        isi = np.diff(ts)
        mean = isi.mean()
        if mean > 0:
            cv[i] = isi.std() / mean
    return cv


def _nbins(t0, t1, bin_ms):
    return max(1, int(math.ceil((t1 - t0) / bin_ms - 1e-9)))


def golomb_chi(spikes, n: int, t0_ms: float, t1_ms: float, bin_ms: float = 1.0) -> float:
    """Golomb synchrony chi on spike counts: sqrt(var_t(mean_i s_i) / mean_i var_t(s_i)). 0 async .. 1 sync."""
    if n <= 0 or t1_ms <= t0_ms or bin_ms <= 0:
        return 0.0
    nr, t = _window(spikes, n, t0_ms, t1_ms)
    if len(nr) == 0:
        return 0.0
    nb = _nbins(t0_ms, t1_ms, bin_ms)
    b = np.minimum(nb - 1, np.floor((t - t0_ms) / bin_ms).astype(np.int64))
    counts = np.zeros((n, nb))
    np.add.at(counts, (nr, b), 1.0)
    mean_var = counts.var(axis=1).mean()
    if mean_var <= 0:
        return 0.0
    return float(math.sqrt(counts.mean(axis=0).var() / mean_var))


def psth(spikes, n: int, neurons: Sequence[int] | None, t0_ms: float, t1_ms: float, bin_ms: float) -> np.ndarray:
    """Population PSTH: mean rate (Hz per neuron) in each bin over `neurons` (None/empty = all)."""
    mask = np.ones(n, bool) if not neurons else np.zeros(n, bool)
    if neurons:
        mask[[i for i in neurons if 0 <= i < n]] = True
    cnt = int(mask.sum())
    nb = _nbins(t0_ms, t1_ms, bin_ms)
    h = np.zeros(nb)
    if cnt == 0:
        return h
    nr, t = _window(spikes, n, t0_ms, t1_ms)
    sel = mask[nr]
    b = np.minimum(nb - 1, np.floor((t[sel] - t0_ms) / bin_ms).astype(np.int64))
    np.add.at(h, b, 1.0)
    return h * 1000.0 / (cnt * bin_ms)


def psth_aligned(spikes, n: int, neurons: Sequence[int] | None, events_ms: Sequence[float], pre_ms: float, post_ms: float, bin_ms: float) -> np.ndarray:
    """Event-aligned PSTH over [-pre, +post), averaged over events, Hz per neuron."""
    mask = np.ones(n, bool) if not neurons else np.zeros(n, bool)
    if neurons:
        mask[[i for i in neurons if 0 <= i < n]] = True
    cnt = int(mask.sum())
    nb = _nbins(-pre_ms, post_ms, bin_ms)
    h = np.zeros(nb)
    if cnt == 0 or len(events_ms) == 0:
        return h
    nr, t = _arrays(spikes)
    sel = (nr >= 0) & (nr < n)
    nr, t = nr[sel], t[sel]
    t = t[mask[nr]]
    for ev in events_ms:
        rel = t - ev
        r = rel[(rel >= -pre_ms) & (rel < post_ms)]
        b = np.minimum(nb - 1, np.floor((r + pre_ms) / bin_ms).astype(np.int64))
        np.add.at(h, b, 1.0)
    return h * 1000.0 / (len(events_ms) * cnt * bin_ms)
