"""spike: the research-tool side of Spike.

    import spike
    net = spike.build(circuit_json)          # identical network to the C++ engine, in PyNEST
    spikes = spike.run(net, 1000.0)          # Spikes(neuron, t_ms)
    edges = spike.motifs.edges_from_nest(net)
    result = spike.motifs.zscores(edges, net.n_neurons, n_null=100, seed=1)
    spike.stats.firing_rates(spikes, net.n_neurons, 0, 1000)

`spike.motifs`, `spike.stats`, `spike.bursts`, `spike.hrv` and `spike.prc` need only numpy (no NEST). `spike.build` / `spike.run` import PyNEST lazily:
set PYTHONPATH to the NEST build's site-packages (third_party/nest-install/lib/python3.*/site-packages).
"""
from . import bursts, hrv, motifs, prc, stats
from .circuit import Network, Spikes, build, load_circuit, run

__all__ = ["bursts", "hrv", "motifs", "prc", "stats", "build", "run", "load_circuit", "Network", "Spikes"]
__version__ = "0.2.0"
