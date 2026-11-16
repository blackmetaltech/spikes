"""Bursts, PRC, HRV and the 4-node census: Python unit tests and C++ == Python cross tests (analysis_extra_test hooks)."""
import itertools
import json
import math
import random
import subprocess

import numpy as np
import pytest

from spike import bursts, hrv, motifs, prc
from conftest import find_exe


@pytest.fixture(scope="session")
def extra_exe():
    exe = find_exe("analysis_extra_test", "SPIKE_ANALYSIS_EXTRA_TEST")
    if exe is None:
        pytest.skip("analysis_extra_test not built (cmake --build build --target analysis_extra_test)")
    return exe


def run(exe, args, text):
    return json.loads(subprocess.run([exe, *args], input=text, capture_output=True, text=True, check=True).stdout)


def close(a, b):
    if a is None:
        return b is None or (isinstance(b, float) and math.isnan(b))
    return a == pytest.approx(b, rel=1e-6, abs=1e-9)


# ---------------------------------------------------------------- bursts

def burst_train(seed=1):
    rng = random.Random(seed)
    t = []
    x = 0.0
    for _ in range(25):
        x += rng.uniform(300, 900)
        k = rng.randint(1, 8)
        for _ in range(k):
            t.append(x)
            x += rng.uniform(2, 40)
    return t


def test_bursts_basic():
    t = [10, 300] + [1000 + 5 * i for i in range(4)] + [1500 + 5 * i for i in range(4)] + [2500]
    p = bursts.BurstParams(20, 20, 100, 10, 3)
    b = bursts.detect_bursts(t, p)
    assert [(x.onset_ms, x.duration_ms, x.n_spikes, x.first_index) for x in b] == [(1000, 15, 4, 2), (1500, 15, 4, 6)]
    assert len(bursts.detect_bursts(t, bursts.BurstParams(20, 20, 600, 10, 3))) == 1
    assert bursts.detect_bursts(t, bursts.BurstParams(20, 20, 100, 10, 5)) == []
    s = bursts.summarize_bursts(b, len(t))
    assert s.mean_period_ms == 500 and s.spikes_in_bursts == 8


def test_bursts_population_pooled():
    sp = [(0, 100.0), (1, 103.0), (2, 106.0), (0, 400.0)]
    pooled = bursts.pooled_spike_times(sp, 3)
    assert len(bursts.detect_bursts(pooled, bursts.BurstParams(20, 20, 100, 5, 3))) == 1
    assert bursts.detect_bursts_per_neuron(sp, 3, 0, 1000) == [[], [], []]


@pytest.mark.parametrize("seed", range(5))
@pytest.mark.parametrize("params", [(170, 170, 200, 10, 3), (50, 30, 100, 20, 4), (30, 80, 0, 0, 2)])
def test_bursts_cpp_matches_python(extra_exe, seed, params):
    t = burst_train(seed)
    cpp = run(extra_exe, ["--bursts", *map(str, params)], "\n".join(f"{x:.17g}" for x in t))
    py = bursts.detect_bursts(t, bursts.BurstParams(*params))
    assert len(cpp["bursts"]) == len(py) == cpp["n_bursts"]
    for c, p in zip(cpp["bursts"], py):
        assert c["onset_ms"] == pytest.approx(p.onset_ms) and c["offset_ms"] == pytest.approx(p.offset_ms)
        assert c["n_spikes"] == p.n_spikes
    s = bursts.summarize_bursts(py, len(t))
    for k in ("n_spikes", "spikes_in_bursts", "fraction_in_bursts", "mean_duration_ms", "mean_spikes_per_burst", "mean_period_ms", "mean_intra_burst_isi_ms"):
        assert close(cpp["summary"][k], getattr(s, k)), k


# ---------------------------------------------------------------- PRC

def prc_data(seed):
    """Oscillator with period 100; sparse perturbations whose effect is shift = 0.2 sin(pi phase)."""
    rng = random.Random(seed)
    t, pt, x = [0.0], [], 0.0
    for _ in range(60):
        if rng.random() < 0.3:
            ph = rng.uniform(0.05, 0.95)
            pt.append(x + 100 * ph)
            if rng.random() < 0.2:
                pt.append(x + 100 * rng.uniform(0.05, 0.95))  # a second pulse in the same cycle
            x += 100 * (1 - 0.2 * math.sin(math.pi * ph))
            t.append(x)
            for _ in range(3):
                x += 100
                t.append(x)
        else:
            x += 100
            t.append(x)
    return t, pt


def test_prc_known_shift():
    t = [0, 100, 200, 300, 400, 500, 590, 690, 790, 890, 990, 1090]
    r = prc.prc_estimate(t, [550])
    assert r.period_ms == 100 and len(r.points) == 1
    assert r.points[0].phase == pytest.approx(0.5) and r.points[0].shift == pytest.approx(0.1)
    assert prc.prc_estimate(t, [520, 550, 5000]).points == []
    r = prc.prc_estimate(t, [550, 650])
    assert len(r.points) == 1 and r.n_rejected == 1
    assert prc.prc_estimate([1], [1]).points == []
    b = prc.prc_bins(prc.prc_estimate(t, [550]), 4)
    assert b[2].count == 1 and math.isnan(b[0].mean_shift)


def test_prc_recovers_curve():
    t, pt = prc_data(2)
    r = prc.prc_estimate(t, pt)
    assert r.period_ms == pytest.approx(100)
    assert len(r.points) >= 5
    for p in r.points:
        assert p.shift == pytest.approx(0.2 * math.sin(math.pi * p.phase), abs=1e-9)


@pytest.mark.parametrize("seed", range(4))
def test_prc_cpp_matches_python(extra_exe, seed):
    t, pt = prc_data(seed)
    text = f"{len(t)} {len(pt)}\n" + "\n".join(f"{x:.17g}" for x in t + pt)
    for period in (0.0, 100.0):
        cpp = run(extra_exe, ["--prc", str(period)], text)
        py = prc.prc_estimate(t, pt, period)
        assert cpp["period_ms"] == pytest.approx(py.period_ms)
        assert (cpp["n_perturbations"], cpp["n_rejected"], cpp["n_clean_cycles"]) == (py.n_perturbations, py.n_rejected, py.n_clean_cycles)
        assert len(cpp["points"]) == len(py.points)
        for c, p in zip(cpp["points"], py.points):
            assert c["phase"] == pytest.approx(p.phase) and c["shift"] == pytest.approx(p.shift)
        for c, b in zip(cpp["bins"], prc.prc_bins(py, 8)):
            assert c["count"] == b.count and close(c["mean_shift"], b.mean_shift) and close(c["sd_shift"], b.sd_shift)


# ---------------------------------------------------------------- HRV

def beats(seed, kind):
    rng = random.Random(seed)
    b = [0.0]
    while b[-1] < 300000:
        s = b[-1] * 1e-3
        if kind == "lf":
            rr = 900 + 50 * math.sin(2 * math.pi * 0.1 * s)
        elif kind == "hf":
            rr = 900 + 50 * math.sin(2 * math.pi * 0.25 * s)
        else:
            rr = 850 + rng.gauss(0, 40)
        b.append(b[-1] + rr)
    return b


def test_hrv_time_domain_and_bands():
    r = hrv.hrv([800.0 * i for i in range(50)])
    assert r.mean_hr_bpm == pytest.approx(75) and r.sdnn_ms == pytest.approx(0) and r.pnn50_pct == 0
    alt = np.concatenate([[0], np.cumsum([800 if i % 2 == 0 else 900 for i in range(40)])])
    r = hrv.hrv(alt)
    assert r.rmssd_ms == pytest.approx(100) and r.pnn50_pct == pytest.approx(100)
    rr = np.diff(alt)
    assert r.sdnn_ms == pytest.approx(rr.std(ddof=1))
    lf, hf = hrv.hrv(beats(0, "lf")), hrv.hrv(beats(0, "hf"))
    assert lf.lf_hf > 10 and hf.lf_hf < 0.1
    assert lf.lf_ms2 == pytest.approx(1250, rel=0.4) and hf.hf_ms2 == pytest.approx(1250, rel=0.4)  # A^2/2
    assert not hrv.hrv([0, 800, 1600]).spectrum_ok and math.isnan(hrv.hrv([0.0]).sdnn_ms)


def test_lomb_scargle_matches_direct_formula():
    b = beats(3, "noise")[:200]
    t = (np.array(b[1:]) - b[0]) * 1e-3
    rr = np.diff(b)
    y = rr - rr.mean()
    f = np.array([0.05, 0.1, 0.2, 0.33])
    got = hrv.lomb_scargle(b, f)
    for fi, g in zip(f, got):
        w = 2 * math.pi * fi
        tau = math.atan2(np.sum(np.sin(2 * w * t)), np.sum(np.cos(2 * w * t))) / (2 * w)
        c, s = np.cos(w * (t - tau)), np.sin(w * (t - tau))
        P = 0.5 * (np.sum(y * c) ** 2 / np.sum(c * c) + np.sum(y * s) ** 2 / np.sum(s * s))
        assert g == pytest.approx(P * 2 * t[-1] / len(rr), rel=1e-9)
    # a sinusoid on an uneven grid peaks at its frequency
    rng = np.random.default_rng(1)
    tt = np.sort(rng.uniform(0, 300, 400))
    beat_ms = np.concatenate([[0], tt * 1e3])
    # build RR so the tachogram is a sinusoid: construct beats with rr = 900 + 40 sin at 0.1 Hz
    bb = [0.0]
    while bb[-1] < 300000:
        bb.append(bb[-1] + 900 + 40 * math.sin(2 * math.pi * 0.1 * bb[-1] * 1e-3))
    fs = np.arange(0.02, 0.4, 0.002)
    assert fs[np.argmax(hrv.lomb_scargle(bb, fs))] == pytest.approx(0.1, abs=0.006)


@pytest.mark.parametrize("kind", ["lf", "hf", "noise"])
def test_hrv_cpp_matches_python(extra_exe, kind):
    b = beats(5, kind)
    cpp = run(extra_exe, ["--hrv"], "\n".join(f"{x:.17g}" for x in b))
    py = hrv.hrv(b)
    for k in ("n_beats", "n_rr", "mean_rr_ms", "mean_hr_bpm", "sdnn_ms", "rmssd_ms", "pnn50_pct", "lf_ms2", "hf_ms2", "lf_hf", "lf_nu", "hf_nu"):
        assert close(cpp[k], getattr(py, k)), k
    assert cpp["spectrum_ok"] == py.spectrum_ok


# ---------------------------------------------------------------- 4-node census

def random_edges(n, p, seed, p_neg=0.0):
    rng = random.Random(seed)
    return [(a, b, -1.0 if rng.random() < p_neg else 1.0) for a in range(n) for b in range(n) if a != b and rng.random() < p]


def brute4(edges, n):
    g = motifs.make_graph(edges, n)
    out = [0] * motifs.N_CLASSES4
    perms = list(itertools.permutations(range(4)))
    for quad in itertools.combinations(range(n), 4):
        es = [(i, j) for i in range(4) for j in range(4) if i != j and g.has_edge(quad[i], quad[j])]
        und = {frozenset(e) for e in es}
        # connectivity by flood fill
        seen, stack = {0}, [0]
        while stack:
            x = stack.pop()
            for y in range(4):
                if y not in seen and frozenset((x, y)) in und:
                    seen.add(y)
                    stack.append(y)
        if len(seen) < 4:
            continue
        cid = min(sum(1 << (15 - (4 * p[i] + p[j])) for i, j in es) for p in perms)
        out[motifs.IDS4.index(cid)] += 1
    return out


def test_table4():
    assert motifs.N_CLASSES4 == 199 and len(set(motifs.IDS4)) == 199
    assert motifs.MOTIFS4[0].edges == 3 and motifs.MOTIFS4[-1].edges == 12 and motifs.MOTIFS4[-1].mutual_pairs == 6
    # 3 arcs = oriented spanning trees: 4 stars + 4 paths
    by_edges = {}
    for m in motifs.MOTIFS4:
        by_edges[m.edges] = by_edges.get(m.edges, 0) + 1
    assert sum(by_edges.values()) == 199 and by_edges[3] == 8 and by_edges[12] == 1


@pytest.mark.parametrize("seed", range(6))
def test_census4_vs_bruteforce(seed):
    n = 6 + seed
    e = random_edges(n, 0.12 + 0.05 * seed, seed)
    assert motifs.census4(e, n) == brute4(e, n)


def test_census4_cycle_and_planted_z():
    cyc = [(0, 1), (1, 2), (2, 3), (3, 0)]
    c = motifs.census4(cyc, 4)
    assert sum(c) == 1
    e = random_edges(100, 0.012, 9)
    for k in range(25):
        b = 4 * k
        e += [(b, b + 1), (b + 1, b + 2), (b + 2, b + 3), (b + 3, b)]
    r = motifs.zscores4(e, 100, n_null=15, seed=5)
    idx = motifs.census4(cyc, 4).index(1)
    assert r.z[idx] > 5 and r.significant[idx] and not r.degenerate


@pytest.mark.parametrize("sign", ["any", "E", "I"])
def test_census4_cpp_matches_python(extra_exe, sign):
    n = 30
    e = random_edges(n, 0.12, 21, p_neg=0.3)
    text = f"{n} {len(e)}\n" + "\n".join(f"{a} {b} {w}" for a, b, w in e)
    cpp = run(extra_exe, ["--census4", sign, "5", "77"], text)
    py = motifs.zscores4(e, n, sign=sign, n_null=5, seed=77)
    cl = cpp["classes"]
    assert [c["id"] for c in cl] == motifs.IDS4
    assert [c["count"] for c in cl] == py.counts
    assert [c["edges"] for c in cl] == [m.edges for m in motifs.MOTIFS4]
    assert [c["mutual_pairs"] for c in cl] == [m.mutual_pairs for m in motifs.MOTIFS4]
    for c, mean, sd, z, sig in zip(cl, py.null_mean, py.null_sd, py.z, py.significant):
        assert c["null_mean"] == pytest.approx(mean, rel=1e-9, abs=1e-12)
        assert c["null_sd"] == pytest.approx(sd, rel=1e-9, abs=1e-12)
        assert c["significant"] == bool(sig)
        if c["z"] is None:
            assert not math.isfinite(z)
        else:
            assert c["z"] == pytest.approx(z, rel=1e-9, abs=1e-12)
    assert cpp["swap_acceptance"] == pytest.approx(py.swap_acceptance) and cpp["degenerate"] == py.degenerate
    assert cpp["edges"] == py.edge_count
