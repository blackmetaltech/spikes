"""Loader rules for synapse models and the modulation table (mirrors engine/tests/test_synapses.cpp and test_modulators.cpp)."""
import copy

import pytest

from conftest import requires_nest
from spike import circuit as C


def pops(*models):
    return [{"id": i, "model": m, "n": 2} for i, m in zip("ABCD", models)]


def spec(projection, models=("hh_psc_alpha_gap", "hh_psc_alpha_gap", "iaf_psc_alpha")):
    return {"schema_version": 1, "populations": pops(*models), "projections": [projection]}


def prep(projection, **kw):
    return C._prepare_projection(spec(projection, **kw), projection, 0)


def err(projection, **kw):
    with pytest.raises(C.CircuitError) as e:
        prep(projection, **kw)
    return str(e.value)


def test_gap_junction_resolution():
    p = prep({"pre": "A", "post": "B", "rule": "one_to_one", "synapse": {"synapse_model": "gap_junction", "weight": 2.0}})
    assert p["gap"] and p["rule"]["make_symmetric"] is True
    p = prep({"pre": "A", "post": "A", "rule": "all_to_all", "synapse": {"synapse_model": "gap_junction", "weight": 2.0}})
    assert p["rule"]["allow_autapses"] is False and "make_symmetric" not in p["rule"]


def test_gap_junction_errors():
    syn = {"synapse_model": "gap_junction", "weight": 1.0}
    e = err({"pre": "A", "post": "C", "rule": "one_to_one", "synapse": syn})
    assert "projections[0].synapse.synapse_model" in e and "gap-capable" in e and "iaf_psc_alpha" in e
    assert "instantaneous" in err({"pre": "A", "post": "B", "rule": "one_to_one", "synapse": {**syn, "delay": 1.0}})
    assert "conductance" in err({"pre": "A", "post": "B", "rule": "one_to_one", "synapse": {**syn, "weight": -1.0}})
    assert "symmetric" in err({"pre": "A", "post": "B", "rule": "one_to_one", "make_symmetric": False, "synapse": syn})
    assert "gap_junction supports the rules" in err({"pre": "A", "post": "B", "rule": "fixed_indegree", "indegree": 1, "synapse": syn})
    assert "same population" in err({"pre": "A", "post": "B", "rule": "all_to_all", "synapse": syn})


def test_stdp_defaults_and_errors():
    p = prep({"pre": "C", "post": "C", "rule": "all_to_all", "synapse": {"synapse_model": "stdp_synapse", "weight": 7.0}})
    assert p["syn"]["Wmax"] == 14.0
    p = prep({"pre": "C", "post": "C", "edges": [[0, 1, 3.0, 1.0], [1, 0, 5.0, 1.0]], "synapse": {"synapse_model": "stdp_synapse"}})
    assert p["syn"]["Wmax"] == 10.0
    e = err({"pre": "C", "post": "C", "rule": "all_to_all", "synapse": {"synapse_model": "stdp_synapse", "weight": -5.0}})
    assert "excitatory" in e and "Inhibitory plasticity" in e
    assert "exceeds Wmax" in err({"pre": "C", "post": "C", "rule": "all_to_all", "synapse": {"synapse_model": "stdp_synapse", "weight": 5.0, "Wmax": 3.0}})


def test_stp_errors():
    base = {"pre": "C", "post": "C", "rule": "all_to_all"}
    assert "[0, 1]" in err({**base, "synapse": {"synapse_model": "tsodyks2_synapse", "U": 1.5}})
    assert "tau_rec" in err({**base, "synapse": {"synapse_model": "tsodyks2_synapse", "tau_rec": 0.0}})
    assert "synapse.n" in err({**base, "synapse": {"synapse_model": "quantal_stp_synapse", "n": 0}})
    assert "synapse.a" in err({**base, "synapse": {"synapse_model": "quantal_stp_synapse", "n": 2, "a": 5}})


def test_dopamine_errors_and_common():
    base = {"pre": "C", "post": "C", "rule": "all_to_all"}
    assert "dopamine_source" in err({**base, "synapse": {"synapse_model": "stdp_dopamine_synapse", "weight": 1.0}})
    assert "unknown population 'nope'" in err({**base, "dopamine_source": "nope", "synapse": {"synapse_model": "stdp_dopamine_synapse", "weight": 1.0}})
    assert "only valid with" in err({**base, "dopamine_source": "A", "synapse": {"synapse_model": "static_synapse"}})
    p = prep({**base, "dopamine_source": "A", "synapse": {"synapse_model": "stdp_dopamine_synapse", "weight": 5.0, "A_plus": 2.0, "tau_c": 80.0}})
    assert p["common"] == {"A_plus": 2.0, "tau_c": 80.0, "Wmax": 10.0}
    assert "A_plus" not in p["syn"] and "Wmax" not in p["syn"]


def test_modulation_validation():
    s = {"schema_version": 1, "populations": pops("iaf_psc_alpha")}
    for row, msg in [
        ({"modulator": "XX", "pop": "A", "param": "I_e", "gain": 1}, "modulation[0].modulator"),
        ({"modulator": "NE", "pop": "Q", "param": "I_e", "gain": 1}, "modulation[0].pop"),
        ({"modulator": "NE", "pop": "A", "param": "I_e"}, "exactly one of 'gain'"),
        ({"modulator": "NE", "pop": "A", "param": "I_e", "gain": 1, "scale_per_unit": 1}, "exactly one of 'gain'"),
        ({"modulator": "NE", "pop": "A", "param": "I_e", "gain": 1, "min": 2, "max": 1}, "min must be <= max"),
    ]:
        t = copy.deepcopy(s)
        t["modulation"] = [row]
        with pytest.raises(C.CircuitError, match=msg.replace("[", r"\[").replace("]", r"\]")):
            C._parse_modulation(t)


@requires_nest
def test_modulation_applies_to_pynest():
    import spike

    c = {"schema_version": 1, "seed": 1,
         "populations": [{"id": "N", "model": "iaf_psc_alpha", "n": 2, "params": {"I_e": 300.0}}],
         "modulation": [{"modulator": "NE", "pop": "N", "param": "I_e", "gain": 100.0},
                        {"modulator": "ACh", "pop": "N", "param": "I_e", "scale_per_unit": 0.5, "max": 700.0}]}
    net = spike.build(c)
    assert net.population("N")[0].get("I_e") == 300.0
    net.set_modulator("NE", 2.0)
    assert net.population("N")[0].get("I_e") == 500.0
    net.set_modulator("ACh", 4.0)   # (300 + 200) * 3 = 1500, clamped to 700
    assert net.population("N")[0].get("I_e") == 700.0
    with pytest.raises(C.CircuitError):
        net.set_modulator("5HT", 1.0)
    bad = copy.deepcopy(c)
    bad["modulation"][0]["param"] = "no_such_param"
    with pytest.raises(C.CircuitError, match="no parameter"):
        spike.build(bad)


@requires_nest
def test_dopamine_chain_is_built():
    import spike

    net = spike.build({"schema_version": 1, "seed": 1,
                       "populations": [{"id": "P", "model": "parrot_neuron", "n": 1}, {"id": "N", "model": "iaf_psc_alpha", "n": 1}, {"id": "D", "model": "parrot_neuron", "n": 1}],
                       "projections": [{"pre": "P", "post": "N", "rule": "all_to_all", "dopamine_source": "D",
                                        "synapse": {"synapse_model": "stdp_dopamine_synapse", "weight": 10.0}}]})
    assert net.da_generator is not None
    net.set_modulator("DA", 0.5)
    assert net.da_generator.get("rate") == 50.0
