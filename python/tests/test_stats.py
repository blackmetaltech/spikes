import math

import numpy as np
import pytest

from spike import stats


def test_rates_cv():
    sp = [(0, i * 10.0 + 1.0) for i in range(100)] + [(2, 100.0 * i + 3) for i in range(5)]
    r = stats.firing_rates(sp, 3, 0, 1000)
    assert r == pytest.approx([100, 0, 5])
    cv = stats.isi_cv(sp, 3, 0, 1000)
    assert cv[0] == pytest.approx(0) and math.isnan(cv[1]) and cv[2] == pytest.approx(0)


def test_poisson_cv():
    rng = np.random.default_rng(1)
    t = np.cumsum(rng.exponential(20.0, 20000))
    assert stats.isi_cv([(0, x) for x in t], 1, 0, t[-1] + 1)[0] == pytest.approx(1.0, abs=0.05)


def test_chi():
    rng = np.random.default_rng(2)
    common = np.floor(rng.uniform(0, 1000, 50)) + 0.5
    sync = [(n, t) for n in range(40) for t in common]
    asyn = [(n, np.floor(rng.uniform(0, 1000)) + 0.5) for n in range(40) for _ in range(50)]
    assert stats.golomb_chi(sync, 40, 0, 1000) == pytest.approx(1.0)
    assert 0 < stats.golomb_chi(asyn, 40, 0, 1000) < 0.3
    assert stats.golomb_chi([], 40, 0, 1000) == 0.0


def test_psth():
    sp = [(0, i * 10.0 + 1.0) for i in range(100)] + [(2, 100.0 * i + 3) for i in range(5)]
    h = stats.psth(sp, 3, None, 0, 1000, 100)
    assert len(h) == 10 and h[0] == pytest.approx(11 * 1000.0 / 300)
    assert stats.psth(sp, 3, [0], 0, 1000, 100)[5] == pytest.approx(100.0)
    ha = stats.psth_aligned(sp, 3, [0], [100.0, 500.0], 0, 100, 50)
    assert len(ha) == 2 and ha[0] == pytest.approx(100.0)
