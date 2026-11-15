import json

import numpy as np
import pytest

from conftest import requires_nest

pytestmark = requires_nest


def test_build_run_ei(circuits_dir):
    import spike

    net = spike.build(circuits_dir / "ei_net.json")
    assert net.n_neurons == 100 and net.seed == 7
    sp = spike.run(net, 300.0)
    assert len(sp) > 50
    assert sp.neuron.min() >= 0 and sp.neuron.max() < 100
    assert np.all(np.diff(sp.t_ms) >= 0)
    # deterministic: same seed twice, different seed differs
    a = spike.run(spike.build(circuits_dir / "ei_net.json"), 300.0).steps(0.1)
    b = spike.run(spike.build(circuits_dir / "ei_net.json"), 300.0).steps(0.1)
    c = spike.run(spike.build(circuits_dir / "ei_net.json", seed=8), 300.0).steps(0.1)
    assert a == b and a != c


def test_edges_and_motifs_from_nest(circuits_dir):
    import spike

    net = spike.build(circuits_dir / "ei_net.json")
    edges = spike.motifs.edges_from_nest(net)
    assert len(edges) == 80 * 8 + 20 * 8 + 80 * 4 + 20 * 4  # fixed_indegree: indegree x targets
    assert all((w < 0) == (a >= 80) for a, b, w in edges)  # I neurons (80..99) are the inhibitory sources
    ex = spike.motifs.make_graph(edges, 100, "E")
    inh = spike.motifs.make_graph(edges, 100, "I")
    assert ex.edge_count() > 0 and inh.edge_count() > 0
    r = spike.motifs.zscores(edges, 100, n_null=5, seed=1)
    assert sum(r.counts) > 0


def test_half_centre_edges_and_spikes(circuits_dir):
    import spike

    net = spike.build(circuits_dir / "half_centre.json")
    assert sorted((a, b) for a, b, w in spike.motifs.edges_from_nest(net)) == [(0, 1), (1, 0)]
    sp = spike.run(net, 500.0)
    assert len(sp) > 0 and set(sp.neuron.tolist()) <= {0, 1}


def test_bad_schema():
    import spike

    with pytest.raises(spike.circuit.CircuitError):
        spike.build('{"schema_version": 2, "populations": []}')
