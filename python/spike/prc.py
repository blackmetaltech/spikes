"""Phase-response curve estimation from spike (cycle marker) times and perturbation times. Mirrors spike::analysis in C++.

shift = (period - perturbed_cycle) / period in cycles (positive = advance); phase = (t_perturb - t_prev_spike) / period.
A cycle is "clean" if neither it nor the two cycles before it hold a perturbation; period = median clean cycle unless given.
A perturbation is used if it is alone in its cycle and the two cycles before are clean (first-order PRC).
The Engine-driven experiment (periodic oscillator + pulses at evenly spaced phases) is C++ only: spike::analysis::run_prc_experiment.
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field

import numpy as np


@dataclass
class PrcPoint:
    perturb_ms: float
    phase: float
    shift: float
    cycle_ms: float
    prev_spike_ms: float


@dataclass
class PrcResult:
    period_ms: float = math.nan
    n_clean_cycles: int = 0
    n_perturbations: int = 0
    n_rejected: int = 0
    points: list[PrcPoint] = field(default_factory=list)

    @property
    def phase(self) -> np.ndarray:
        return np.array([p.phase for p in self.points])

    @property
    def shift(self) -> np.ndarray:
        return np.array([p.shift for p in self.points])


def prc_estimate(spike_times_ms, perturb_times_ms, period_ms: float = 0.0) -> PrcResult:
    t = np.sort(np.asarray(spike_times_ms, dtype=np.float64))
    pt = np.sort(np.asarray(perturb_times_ms, dtype=np.float64))
    r = PrcResult(n_perturbations=len(pt))
    ncyc = len(t) - 1
    if ncyc < 1:
        r.n_rejected = len(pt)
        return r
    cnt = [0] * ncyc
    cyc_of = []
    for x in pt:
        k = int(np.searchsorted(t, x, side="right")) - 1
        if 0 <= k < ncyc:
            cyc_of.append(k)
            cnt[k] += 1
        else:
            cyc_of.append(-1)

    def quiet(k):
        return k < 0 or cnt[k] == 0

    def clean(k):
        return quiet(k) and quiet(k - 1) and quiet(k - 2)

    clean_cycles = [k for k in range(ncyc) if clean(k)]
    r.n_clean_cycles = len(clean_cycles)
    if period_ms > 0:
        r.period_ms = period_ms
    else:
        if not clean_cycles:
            r.n_rejected = len(pt)
            return r
        r.period_ms = float(np.median([t[k + 1] - t[k] for k in clean_cycles]))
    for x, k in zip(pt, cyc_of):
        if k < 0 or cnt[k] != 1 or not quiet(k - 1) or not quiet(k - 2):
            r.n_rejected += 1
            continue
        cyc = float(t[k + 1] - t[k])
        r.points.append(PrcPoint(float(x), float((x - t[k]) / r.period_ms), (r.period_ms - cyc) / r.period_ms, cyc, float(t[k])))
    r.points.sort(key=lambda p: p.phase)
    return r


def prc_from_spikes(spikes, n: int, neurons, perturb_times_ms, period_ms: float = 0.0, merge_gap_ms: float = 0.0) -> PrcResult:
    """Markers = spikes of `neurons` (None = all); a spike closer than merge_gap_ms to the previous pooled one is the same marker."""
    from .bursts import pooled_spike_times

    pooled = pooled_spike_times(spikes, n, neurons)
    marks, last = [], -math.inf
    for x in pooled:
        if not marks or merge_gap_ms <= 0 or x - last > merge_gap_ms:
            marks.append(float(x))
        last = x
    return prc_estimate(marks, perturb_times_ms, period_ms)


@dataclass
class PrcBin:
    phase_center: float
    mean_shift: float
    sd_shift: float
    count: int


def prc_bins(r: PrcResult, nbins: int) -> list[PrcBin]:
    nbins = max(1, nbins)
    v: list[list[float]] = [[] for _ in range(nbins)]
    for p in r.points:
        v[min(nbins - 1, max(0, int(math.floor(p.phase * nbins))))].append(p.shift)
    out = []
    for b in range(nbins):
        xs = v[b]
        mean = sum(xs) / len(xs) if xs else math.nan
        sd = math.sqrt(sum((x - mean) ** 2 for x in xs) / (len(xs) - 1)) if len(xs) >= 2 else math.nan
        out.append(PrcBin((b + 0.5) / nbins, mean, sd, len(xs)))
    return out
