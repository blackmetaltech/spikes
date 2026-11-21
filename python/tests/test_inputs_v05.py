"""0.5 loader additions (mirrors engine/tests/test_spike_inputs.cpp, test_synapse_modulation.cpp): spike-train inputs, tsodyks_synapse rules and
synapse rows of the modulation table. The validation tests need no PyNEST; the PyNEST tests skip without it."""
import copy

import pytest

from conftest import requires_nest
from spike import circuit as C


def inp(**kw):
    return {"name": "g", "kind": "spike_generator", **kw}


def err(fn, *a):
    with pytest.raises(C.CircuitError) as e:
        fn(*a)
    return str(e.value)


def test_spike_generator_validation():
    assert C._parse_input(inp(spike_times=[1.0, 2.5]), 0, 0.5) == ("spike_generator", {"spike_times": [1.0, 2.5]}, [1.0, 2.5])
    assert C._parse_input(inp(), 0, 0.1) == ("spike_generator", {}, [])
    assert C._parse_input(inp(spike_times=[0.3]), 0, 0.1)[2] == [pytest.approx(0.3)]  # 0.3 / 0.1 is not exact in floats
    e = err(C._parse_input, inp(spike_times=[1.0, 1.2]), 0, 0.5)
    assert "inputs[0].spike_times[1]" in e and "multiple of resolution_ms" in e
    assert "sorted" in err(C._parse_input, inp(spike_times=[2.0, 1.0]), 0, 0.1)
    assert "> 0" in err(C._parse_input, inp(spike_times=[0.0]), 0, 0.1)
    assert "inputs[0].value" in err(C._parse_input, inp(value=3.0), 0, 0.1)
    e = err(C._parse_input, {"name": "g", "kind": "dc", "spike_times": [1.0]}, 0, 0.1)
    assert "inputs[0].spike_times" in e and "spike_generator" in e
    e = err(C._parse_input, {"name": "g", "kind": "foo"}, 0, 0.1)
    assert "spike_generator" in e and "sinusoidal_poisson" in e


def test_sinusoidal_poisson_validation():
    s = {"name": "s", "kind": "sinusoidal_poisson", "rate": 40.0, "amplitude": 30.0, "freq_hz": 8.0, "phase": 90.0}
    assert C._parse_input(s, 0, 0.1) == ("sinusoidal_poisson_generator", {"rate": 40.0, "amplitude": 30.0, "frequency": 8.0, "phase": 90.0}, [])
    assert "inputs[0].rate" in err(C._parse_input, {**s, "rate": -1.0}, 0, 0.1)
    assert "inputs[0].amplitude" in err(C._parse_input, {**s, "amplitude": -1.0}, 0, 0.1)
    assert "inputs[0].freq_hz" in err(C._parse_input, {**s, "freq_hz": 5000.0}, 0, 0.1)
    e = err(C._parse_input, {**s, "value": 1.0}, 0, 0.1)
    assert "inputs[0].value" in e
    e = err(C._parse_input, {"name": "p", "kind": "poisson", "amplitude": 1.0}, 0, 0.1)
    assert "inputs[0].amplitude" in e and "sinusoidal_poisson" in e
    # the existing kinds are unchanged
    assert C._parse_input({"name": "d", "kind": "dc", "value": 5.0}, 0, 0.1) == ("dc_generator", {"amplitude": 5.0}, [])
    assert C._parse_input({"name": "p", "kind": "poisson"}, 0, 0.1) == ("poisson_generator", {"rate": 0.0}, [])


def pops():
    return [{"id": "PRE", "model": "parrot_neuron", "n": 1}, {"id": "POST", "model": "iaf_psc_exp", "n": 1}]


def circuit(modulation, syn=None, **extra):
    return {"schema_version": 1, "seed": 1, "populations": pops(),
            "projections": [{"pre": "PRE", "post": "POST", "rule": "one_to_one",
                             "synapse": syn or {"synapse_model": "tsodyks2_synapse", "weight": 200.0, "delay": 1.0, "U": 0.2, "u": 0.2, "tau_rec": 300.0}}],
            "modulation": modulation, **extra}


def test_projection_modulation_validation():
    row = {"modulator": "NE", "projection": 0, "param": "U", "gain": 0.4, "min": 0.0, "max": 1.0}
    r = C._parse_modulation(circuit([row]))
    assert r[0]["projection"] == 0 and r[0]["pop"] is None and r[0]["param"] == "U" and r[0]["amount"] == 0.4 and r[0]["scale"] is False
    # the neuron shape is unchanged
    n = C._parse_modulation(circuit([{"modulator": "NE", "pop": "POST", "param": "I_e", "gain": 1.0}]))
    assert n[0]["pop"] == "POST" and n[0]["projection"] is None
    assert "projection number" in err(C._parse_modulation, circuit([{**row, "projection": 3}]))
    assert "exactly one of 'pop'" in err(C._parse_modulation, circuit([{**row, "pop": "POST"}]))
    assert "exactly one of 'pop'" in err(C._parse_modulation, circuit([{k: v for k, v in row.items() if k != "projection"}]))
    assert "cannot be modulated" in err(C._parse_modulation, circuit([{**row, "param": "delay"}]))
    e = err(C._parse_modulation, circuit([{**row, "param": "weight"}], syn={"synapse_model": "stdp_synapse", "weight": 5.0, "delay": 1.0}))
    assert "plasticity rule" in e
    c = circuit([row])
    c["projections"].append(copy.deepcopy(c["projections"][0]))
    # projections that share (pre, post, synapse model) are allowed: each gets its own synapse-model copy, so modulation addresses one at a time
    r = C._parse_modulation(c)
    assert r[0]["projection"] == 0


def test_tsodyks_synapse_rules():
    ps = {"populations": pops(), "projections": [{"pre": "PRE", "post": "POST", "rule": "one_to_one",
                                                   "synapse": {"synapse_model": "tsodyks_synapse", "weight": 1.0, "U": 0.5, "tau_rec": 800.0, "tau_psc": 3.0}}]}
    C._prepare_projection(ps, ps["projections"][0], 0)
    for key, val, msg in (("tau_psc", 0.0, "tau_psc"), ("y", 2.0, "synapse.y"), ("U", 1.5, "synapse.U"), ("tau_rec", 0.0, "tau_rec")):
        bad = copy.deepcopy(ps)
        bad["projections"][0]["synapse"][key] = val
        assert msg in err(C._prepare_projection, bad, bad["projections"][0], 0)


@requires_nest
def test_spike_inputs_in_pynest():
    import spike

    c = {"schema_version": 1, "seed": 3, "resolution_ms": 0.1,
         "populations": [{"id": "P", "model": "parrot_neuron", "n": 100}],
         "inputs": [{"name": "g", "kind": "spike_generator", "targets": ["P"], "weight": 1.0, "spike_times": [5.0, 10.0]},
                    {"name": "s", "kind": "sinusoidal_poisson", "targets": ["P"], "weight": 1.0, "rate": 0.0, "amplitude": 0.0, "freq_hz": 10.0}]}
    net = spike.build(c)
    spike.run(net, 20.0)
    net.queue_spikes("g", [30.0, 25.0])
    with pytest.raises(C.CircuitError, match="not after"):
        net.queue_spikes("g", [10.0])
    with pytest.raises(C.CircuitError, match="multiple of the resolution"):
        net.queue_spikes("g", [40.05])
    with pytest.raises(C.CircuitError, match="queue_spikes"):
        net.set_input("g", 1.0)
    sp = spike.run(net, 20.0)  # now 40 ms: 100 parrots each relayed 4 spikes
    assert len(sp) == 400
    net.set_input("s", 100.0)
    net.set_input_param("s", "amplitude", 50.0)
    sp2 = spike.run(net, 100.0)
    assert len(sp2) > 400 + 100 * 100 * 0.1 * 0.5


@requires_nest
def test_projection_modulation_applies_to_pynest():
    import nest

    import spike

    row = {"modulator": "NE", "projection": 0, "param": "U", "gain": 0.4, "min": 0.0, "max": 1.0}
    net = spike.build(circuit([row, {"modulator": "ACh", "projection": 0, "param": "weight", "scale_per_unit": 1.0}]))
    conn = nest.GetConnections(synapse_model="tsodyks2_synapse")
    assert conn.get("U") == pytest.approx(0.2) and conn.get("weight") == pytest.approx(200.0)
    net.set_modulator("NE", 1.0)
    assert conn.get("U") == pytest.approx(0.6)
    net.set_modulator("NE", 5.0)    # 0.2 + 2.0 clamped to 1
    assert conn.get("U") == pytest.approx(1.0)
    net.set_modulator("ACh", 1.0)   # weight * (1 + 1 * 1)
    assert conn.get("weight") == pytest.approx(400.0)
    net.set_modulator("NE", 0.0)
    net.set_modulator("ACh", 0.0)
    assert conn.get("U") == pytest.approx(0.2) and conn.get("weight") == pytest.approx(200.0)
    bad = circuit([{**row, "param": "no_such_param"}])
    with pytest.raises(C.CircuitError, match="no parameter"):
        spike.build(bad)


@requires_nest
def test_shared_projections_modulate_one_at_a_time_in_pynest():
    import nest

    import spike

    row = {"modulator": "NE", "projection": 1, "param": "U", "gain": 0.4, "min": 0.0, "max": 1.0}
    c = circuit([row])
    c["projections"].append(copy.deepcopy(c["projections"][0]))  # same pre, post and synapse model as projection 0
    net = spike.build(c)
    a = nest.GetConnections(synapse_model="tsodyks2_synapse__p0")
    b = nest.GetConnections(synapse_model="tsodyks2_synapse__p1")
    assert len(a) and len(b)
    net.set_modulator("NE", 1.0)
    assert b.get("U") == pytest.approx(0.6)   # only projection 1 follows NE
    assert a.get("U") == pytest.approx(0.2)
