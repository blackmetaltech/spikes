"""Max-interval burst detection (Cocatre-Zilgien & Delcomyn 1992, J Neurosci Methods 41:19-30). Mirrors spike::analysis in C++.

1. A burst starts at two spikes closer than `max_isi_start_ms` and extends while ISIs are <= `max_isi_end_ms`.
2. Bursts with a gap (last spike -> next first spike) < `min_ibi_ms` are merged.
3. Bursts shorter than `min_duration_ms` or with fewer than `min_spikes` spikes are dropped.
A spike belongs to at most one burst; ISI comparisons are "<=", the merge gap "<".
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field

import numpy as np


@dataclass
class BurstParams:
    max_isi_start_ms: float = 170.0
    max_isi_end_ms: float = 170.0
    min_ibi_ms: float = 200.0
    min_duration_ms: float = 10.0
    min_spikes: int = 3


@dataclass
class Burst:
    onset_ms: float
    offset_ms: float
    n_spikes: int
    first_index: int

    @property
    def duration_ms(self) -> float:
        return self.offset_ms - self.onset_ms


def detect_bursts(spike_times_ms, params: BurstParams | None = None) -> list[Burst]:
    p = params or BurstParams()
    t = np.sort(np.asarray(spike_times_ms, dtype=np.float64))
    n = len(t)
    raw = []
    i = 0
    while i + 1 < n:
        if t[i + 1] - t[i] <= p.max_isi_start_ms:
            j = i + 1
            while j + 1 < n and t[j + 1] - t[j] <= p.max_isi_end_ms:
                j += 1
            raw.append([i, j])
            i = j + 1
        else:
            i += 1
    merged: list[list[int]] = []
    for a, b in raw:
        if merged and t[a] - t[merged[-1][1]] < p.min_ibi_ms:
            merged[-1][1] = b
        else:
            merged.append([a, b])
    out = []
    for a, b in merged:
        ns = b - a + 1
        if t[b] - t[a] < p.min_duration_ms or ns < p.min_spikes:
            continue
        out.append(Burst(float(t[a]), float(t[b]), ns, a))
    return out


def pooled_spike_times(spikes, n: int, neurons=None, t0_ms: float = -math.inf, t1_ms: float = math.inf) -> np.ndarray:
    """Sorted spike times of `neurons` (None/empty = all) in [t0, t1): population bursts = detect_bursts(pooled)."""
    from .stats import _arrays

    nr, t = _arrays(spikes)
    m = (nr >= 0) & (nr < n) & (t >= t0_ms) & (t < t1_ms)
    if neurons:
        mask = np.zeros(n, bool)
        mask[[i for i in neurons if 0 <= i < n]] = True
        m &= mask[np.clip(nr, 0, n - 1)]
    return np.sort(t[m])


def detect_bursts_per_neuron(spikes, n: int, t0_ms: float, t1_ms: float, params: BurstParams | None = None) -> list[list[Burst]]:
    from .stats import _arrays

    nr, t = _arrays(spikes)
    return [detect_bursts(t[(nr == i) & (t >= t0_ms) & (t < t1_ms)], params) for i in range(n)]


@dataclass
class BurstSummary:
    n_bursts: int = 0
    n_spikes: int = 0
    spikes_in_bursts: int = 0
    fraction_in_bursts: float = 0.0
    mean_duration_ms: float = math.nan
    mean_spikes_per_burst: float = math.nan
    mean_period_ms: float = math.nan
    mean_intra_burst_isi_ms: float = math.nan


def summarize_bursts(bursts: list[Burst], n_spikes: int) -> BurstSummary:
    s = BurstSummary(n_bursts=len(bursts), n_spikes=n_spikes)
    if not bursts:
        return s
    nb = len(bursts)
    s.spikes_in_bursts = sum(b.n_spikes for b in bursts)
    s.fraction_in_bursts = s.spikes_in_bursts / n_spikes if n_spikes > 0 else 0.0
    s.mean_duration_ms = sum(b.duration_ms for b in bursts) / nb
    s.mean_spikes_per_burst = s.spikes_in_bursts / nb
    s.mean_intra_burst_isi_ms = sum(b.duration_ms / (b.n_spikes - 1) for b in bursts) / nb
    if nb >= 2:
        s.mean_period_ms = (bursts[-1].onset_ms - bursts[0].onset_ms) / (nb - 1)
    return s
