"""Load limits and the unsupported Theory kinds (mirrors the engine's load checks, engine/src/circuit.cpp; tests/test_hardening.cpp). Needs no PyNEST."""
import json

import pytest

from spike import circuit as C


def base(**kw):
    c = {"schema_version": 1, "populations": [{"id": "A", "model": "iaf_psc_alpha", "n": 2}], "projections": [], "inputs": [], "outputs": []}
    c.update(kw)
    return c


def err(fn, *a):
    with pytest.raises(C.CircuitError) as e:
        fn(*a)
    return str(e.value)


def test_a_missing_seed_is_one_and_zero_is_refused():
    C.check_limits(base())                         # no seed: fine (build() uses 1, as the engine does)
    assert "from 1 to 4294967295" in err(C.check_limits, base(seed=0))
    assert "from 1 to 4294967295" in err(C.check_limits, base(seed=4294967296))
    assert "from 1 to 4294967295" in err(C.check_seed, 0)
    assert "from 1 to 4294967295" in err(C.check_seed, 1.5)
    assert C.check_seed(1) == 1 and C.check_seed(4294967295) == 4294967295


def test_neuron_total_and_population_size():
    assert C.MAX_NEURONS == 20000
    C.check_limits(base(populations=[{"id": "A", "model": "iaf_psc_alpha", "n": 20000}]))
    assert "more than 20000 neurons" in err(C.check_limits, base(populations=[{"id": "A", "model": "iaf_psc_alpha", "n": 20001}]))
    two = [{"id": "A", "model": "iaf_psc_alpha", "n": 10000}, {"id": "B", "model": "iaf_psc_alpha", "n": 10001}]
    assert "populations[1].n" in err(C.check_limits, base(populations=two))
    assert "integer >= 1" in err(C.check_limits, base(populations=[{"id": "A", "model": "iaf_psc_alpha", "n": 0}]))


def test_connection_delay_rate_and_current_budgets():
    big = [{"id": "A", "model": "iaf_psc_alpha", "n": 2000}]
    pr = {"pre": "A", "post": "A", "rule": "all_to_all", "synapse": {"synapse_model": "static_synapse", "weight": 1.0, "delay": 1.0}}
    assert "connections" in err(C.check_limits, base(populations=big, projections=[pr]))
    pr2 = {**pr, "synapse": {**pr["synapse"], "delay": 1001.0}}
    assert "delay" in err(C.check_limits, base(projections=[pr2]))
    assert "rate" in err(C.check_limits, base(inputs=[{"name": "p", "kind": "poisson", "targets": ["A"], "value": 2.0e5}]))
    assert "constant current" in err(C.check_limits, base(inputs=[{"name": "d", "kind": "dc", "targets": ["A"], "weight": 1000.0, "value": 1000.0}]))
    assert "constant current" in err(C.check_limits, base(populations=[{"id": "A", "model": "sa_node_pacemaker", "n": 1, "params": {"I_e": 5000.0}}]))
    assert "resolution_ms" in err(C.check_limits, base(resolution_ms=0.0))


def test_json_depth(tmp_path):
    deep = "[" * 40 + "]" * 40
    assert "nested deeper than 32" in err(C.load_circuit, '{"schema_version": 1, "x": ' + deep + "}")
    p = tmp_path / "c.json"
    p.write_text(json.dumps(base()))
    assert C.load_circuit(str(p))["schema_version"] == 1


def test_theory_wing_kinds_are_refused_plainly():
    e = err(C.check_supported, base(inputs=[{"name": "r", "kind": "rate_drive", "targets": ["A"]}]))
    assert "rate_drive" in e and "not supported by the Python package yet" in e
    assert "tuned_poisson" in err(C.check_supported, base(inputs=[{"name": "s", "kind": "tuned_poisson", "targets": ["A"]}]))
    assert "siegert_neuron" in err(C.check_supported, base(populations=[{"id": "S", "model": "siegert_neuron", "n": 1}]))
    assert "tanh_rate_ipn" in err(C.check_supported, base(populations=[{"id": "S", "model": "tanh_rate_ipn", "n": 1}]))
    C.check_supported(base())


def test_every_reference_circuit_passes_the_limits():
    from pathlib import Path
    for f in sorted((Path(__file__).resolve().parents[2] / "circuits").glob("*.json")):
        C.check_limits(json.loads(f.read_text()))
