"""Cell-type groups and locked flags (docs/CIRCUIT-FORMAT.md "Locked parts and groups", 0.6): the same effective weights as the C++ engine
(engine/tests/tasks_atlas_test.cpp). Needs no PyNEST."""
import copy

import pytest

from spike import circuit as C


def tiny(groups=None):
    c = {"schema_version": 1, "seed": 3,
         "populations": [{"id": "A", "model": "iaf_psc_alpha", "n": 2}, {"id": "B", "model": "iaf_psc_alpha", "n": 2}, {"id": "C", "model": "iaf_psc_alpha", "n": 1}],
         "projections": [{"pre": "A", "post": "C", "edges": [[0, 0, 40.0, 1.5], [1, 0, -60.0, 1.5]]},
                         {"pre": "B", "post": "C", "rule": "all_to_all", "synapse": {"weight": 25.0, "delay": 2.0}},
                         {"pre": "C", "post": "A", "edges": [[0, 0, 10.0, 1.0]]}],
         "inputs": [], "outputs": []}
    if groups is not None:
        c["groups"] = groups
    return c


def err(c):
    with pytest.raises(C.CircuitError) as e:
        C.apply_groups(c)
    return str(e.value)


def test_sign_and_scale_are_applied_to_the_weights_of_the_projections_out_of_the_group():
    g = [{"id": "ga", "pops": ["A"], "sign": -1, "scale": 2.0, "free": ["sign", "scale"], "ranges": {"scale": [0.5, 4.0]}},
         {"id": "gb", "pops": ["B"], "sign": 1, "scale": 0.5}]
    out = C.apply_groups(tiny(g))
    assert out["projections"][0]["edges"] == [[0, 0, -80.0, 1.5], [1, 0, -120.0, 1.5]]   # |w| x scale x sign: the stored -60 is a magnitude
    assert out["projections"][1]["synapse"]["weight"] == 12.5 and out["projections"][1]["synapse"]["delay"] == 2.0
    assert out["projections"][2]["edges"] == [[0, 0, 10.0, 1.0]]                          # C is in no group
    assert "groups" not in out                                                            # applied once: a second pass cannot scale again
    assert C.apply_groups(out) == out


def test_a_circuit_without_groups_is_unchanged_and_the_input_is_not_mutated():
    c = tiny()
    assert C.apply_groups(c) == c
    g = tiny([{"id": "ga", "pops": ["A"], "scale": 3.0}])
    before = copy.deepcopy(g)
    C.apply_groups(g)
    assert g == before


def test_weight_sd_follows_the_scale_and_locked_flags_are_dropped():
    c = tiny([{"id": "ga", "pops": ["A"], "scale": 2.0}])
    c["projections"][0]["weight_sd"] = 5.0
    c["projections"][0]["locked"] = True
    c["populations"][0]["locked"] = True
    out = C.apply_groups(c)
    assert out["projections"][0]["weight_sd"] == 10.0
    assert "locked" not in out["projections"][0] and "locked" not in out["populations"][0]


def test_a_rule_projection_without_a_weight_gets_nests_default_of_one():
    c = tiny([{"id": "gb", "pops": ["B"], "sign": -1, "scale": 3.0}])
    del c["projections"][1]["synapse"]["weight"]
    assert C.apply_groups(c)["projections"][1]["synapse"]["weight"] == -3.0


@pytest.mark.parametrize("bad,word", [
    ({"id": "g", "pops": ["A"], "sign": 0}, "groups[0].sign"),
    ({"id": "g", "pops": ["A"], "scale": 0}, "groups[0].scale"),
    ({"id": "g", "pops": ["A"], "free": ["tau"]}, "unknown free parameter"),
    ({"id": "g", "pops": ["A"], "free": ["scale"]}, "range"),
    ({"id": "g", "pops": ["A"], "free": ["scale"], "ranges": {"scale": [2.0, 1.0]}}, "range"),
    ({"id": "g", "pops": ["A"], "ranges": {"scale": [0.5, 2.0]}}, "not in the free list"),
    ({"id": "g", "pops": ["Z"]}, "unknown population"),
    ({"id": "g", "pops": []}, "groups[0].pops"),
    ({"id": "g", "pops": ["A"], "colour": "red"}, "unknown key"),
])
def test_invalid_groups_are_refused_with_the_engines_messages(bad, word):
    assert word in err(tiny([bad]))


def test_a_population_in_two_groups_and_a_duplicate_id_are_refused():
    assert "already in a group" in err(tiny([{"id": "g", "pops": ["A"]}, {"id": "h", "pops": ["A"]}]))
    assert "unique" in err(tiny([{"id": "g", "pops": ["A"]}, {"id": "g", "pops": ["B"]}]))


def test_a_locked_rule_projection_and_a_distribution_weight_are_refused():
    c = tiny()
    c["projections"][1]["locked"] = True
    assert "lists its edges" in err(c)
    d = tiny([{"id": "g", "pops": ["B"]}])
    d["projections"][1]["synapse"]["weight"] = {"distribution": "normal"}
    assert "plain number" in err(d)


def test_a_free_value_outside_its_range_still_loads():
    c = tiny([{"id": "g", "pops": ["A"], "scale": 3.0, "free": ["scale"], "ranges": {"scale": [0.5, 1.0]}}])
    assert C.apply_groups(c)["projections"][0]["edges"][0][2] == 120.0     # the level judges the range, not the loader
