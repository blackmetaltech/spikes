# spike (Python)

Build a Spike circuit JSON ([docs/CIRCUIT-FORMAT.md](../docs/CIRCUIT-FORMAT.md)) as a PyNEST network identical to the one the C++ engine builds, run it, and analyse motifs and spike statistics.

## Install

```
pip install -e python[test]      # numpy; test extra adds pytest and networkx; notebook extra adds jupyter, matplotlib
```
`spike.motifs` and `spike.stats` need only numpy. `spike.build` and `spike.run` need **PyNEST**, imported lazily. Install it from the repo's pinned NEST commit:

```
scripts/build-nest.sh --pynest
```
This installs into `third_party/nest-install`. The tests find `third_party/nest-install/lib/python3*/site-packages` automatically; otherwise set `SPIKE_PYNEST` to a NEST site-packages directory (or put it on `PYTHONPATH`).

## Example

```python
import spike

net = spike.build("python/examples/ei_net.json", threads=1)   # seed= overrides the circuit's seed
sp = spike.run(net, 1000.0)                                    # Spikes(neuron, t_ms); drain=True returns only new spikes

rates = spike.stats.firing_rates(sp, net.n_neurons, 0, 1000)   # Hz per neuron
cv = spike.stats.isi_cv(sp, net.n_neurons, 0, 1000)
chi = spike.stats.golomb_chi(sp, net.n_neurons, 0, 1000)       # 1 ms bins

edges = spike.motifs.edges_from_nest(net)                      # (pre, post, weight)
res = spike.motifs.zscores(edges, net.n_neurons, n_null=100, seed=1)   # 13 triad classes (Milo ids)
```
`spike.motifs.census(edges, n)` counts triad classes without a null model. Empty input `targets` and output `neurons` are accepted (the device is created and nothing is connected).

## Parity guarantees and limits

`spike.build(c)` and the C++ `spike-run` give identical spikes (same neuron and same step) for the same circuit, seed and thread count. This holds when:
- both use the **same NEST commit** (see `NEST_REF` in `scripts/build-nest.sh`);
- both use the **same OpenMP setting** (`SPIKE_OPENMP`) and the same number of threads (`threads=` here, `--threads` for `spike-run`);
- the circuit uses only features documented in the format spec.

Initial membrane potentials: when a population has no `V_m` in `params`, each neuron gets a seeded random `V_m` between `E_L` and `V_th`, from a splitmix64 stream keyed by seed and neuron index (not NEST's RNG), implemented identically in `spike/circuit.py` and the engine. Set `V_m` explicitly to start at rest.

Limits: `spike.build` does not run the C++ loader's validation (unknown keys, delay below resolution), so a file can build in PyNEST and still be rejected by the game. There are no tasks in Python.

## Tests

```
pytest python                          # PyNEST tests skip when PyNEST is missing
SPIKE_REQUIRE_NEST=1 pytest python     # ...fail instead (ctest sets this)
```
The parity and C++-vs-Python motif tests need the built `spike-run` and `analysis_test` (`SPIKE_RUN`, `SPIKE_ANALYSIS_TEST` override the search under `build*/`).
