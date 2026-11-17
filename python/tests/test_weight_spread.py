"""Weight spread (projection keys weight_sd / weight_cv): the Python mirror draws exactly what the C++ engine does.

The expected numbers come from engine/tests/test_circuit_spread.cpp (circuit seed 7, one projection of 400 edges of weight 10, weight_cv 0.3).
"""
import copy

import pytest

from spike.circuit import CircuitError, _prepare_projection, edge_spread_normal


def spec(extra, seed_edges=400, w=10.0):
    pr = {"pre": "A", "post": "A", "edges": [[0, 0, w, 1.0] for _ in range(seed_edges)]}
    pr.update(extra)
    return {"populations": [{"id": "A", "model": "iaf_psc_alpha", "n": 1}], "projections": [pr]}


def test_cpp_numbers():
    s = spec({"weight_cv": 0.3})
    ed = _prepare_projection(s, s["projections"][0], 0, 7)["edges"]
    assert ed[0][2] == pytest.approx(7.4750909941732, rel=1e-12)
    assert ed[1][2] == pytest.approx(9.95529935365298, rel=1e-12)
    assert ed[2][2] == pytest.approx(8.03405056160693, rel=1e-12)
    assert ed[0][3] == 1.0


def test_sd_and_cv_and_seed():
    s = spec({"weight_sd": 2.0})
    a = _prepare_projection(s, s["projections"][0], 0, 7)["edges"]
    b = _prepare_projection(s, s["projections"][0], 0, 7)["edges"]
    c = _prepare_projection(s, s["projections"][0], 0, 8)["edges"]
    assert a == b and a != c
    z = edge_spread_normal(7, 0, 3)
    assert a[3][2] == pytest.approx(10.0 + 2.0 * z, rel=1e-12)


def test_sign_kept():
    s = spec({"weight_sd": 50.0}, w=-4.0)
    assert all(e[2] < 0 for e in _prepare_projection(s, s["projections"][0], 0, 3)["edges"])


def test_errors():
    for extra, msg in [({"weight_sd": 1, "weight_cv": 0.1}, "not both"), ({"weight_sd": -1}, "weight_sd")]:
        s = spec(extra)
        with pytest.raises(CircuitError, match=msg):
            _prepare_projection(s, s["projections"][0], 0, 1)


def rule_spec(extra, model="static_synapse"):
    pr = {"pre": "A", "post": "A", "rule": "all_to_all", "synapse": {"synapse_model": model, "weight": 10.0}}
    pr.update(extra)
    return {"populations": [{"id": "A", "model": "iaf_psc_alpha", "n": 3}], "projections": [pr]}


def test_rule_projection_accepted():
    s = rule_spec({"weight_cv": 0.2})
    p = _prepare_projection(s, s["projections"][0], 0, 1)
    assert p["spread"] == (2, 0.2) and "weight_cv" not in p["rule"]
    s = rule_spec({"weight_sd": 1.5})
    assert _prepare_projection(s, s["projections"][0], 0, 1)["spread"] == (1, 1.5)


def test_rule_projection_errors():
    for extra, model, msg in [({"weight_sd": 1, "weight_cv": 0.1}, "static_synapse", "not both"), ({"weight_cv": -1}, "static_synapse", "weight_cv"),
                              ({"weight_cv": 0.1}, "stdp_synapse", "not supported")]:
        s = rule_spec(extra, model)
        with pytest.raises(CircuitError, match=msg):
            _prepare_projection(s, s["projections"][0], 0, 1)
