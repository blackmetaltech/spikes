# spike (Python)

Build a Spike circuit JSON ([docs/CIRCUIT-FORMAT.md](../docs/CIRCUIT-FORMAT.md)) as a PyNEST network identical to the one the C++ engine builds, run it, and analyse motifs and spike statistics.

## Install

```
pip install -e python[test]      # numpy; test extra adds pytest and networkx; notebook extra adds jupyter, matplotlib
```
`spike.motifs`, `spike.stats`, `spike.bursts`, `spike.hrv` and `spike.prc` need only numpy. `spike.build` and `spike.run` need **PyNEST**, imported lazily. Install it from the repo's pinned NEST commit:

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

## Analysis modules (0.2)

All pure numpy, and each mirrors a C++ routine in `spike::analysis` (the tests cross-check them number for number).

```python
# bursts: max-interval detection (Cocatre-Zilgien & Delcomyn 1992), the same one the game's analysis panel shows
t = spike.bursts.pooled_spike_times(sp, net.n_neurons, neurons=range(10), t0_ms=2000, t1_ms=20000)
b = spike.bursts.detect_bursts(t, spike.bursts.BurstParams(max_isi_start_ms=170, min_ibi_ms=200, min_spikes=3))
s = spike.bursts.summarize_bursts(b, len(t))   # n_bursts, mean_period_ms, mean_duration_ms, mean_spikes_per_burst, fraction_in_bursts, ...
per_cell = spike.bursts.detect_bursts_per_neuron(sp, net.n_neurons, 2000, 20000)

# hrv: heart-rate variability from beat times (e.g. the sa_beat output): SDNN, RMSSD, pNN50 and Lomb-Scargle LF / HF power
r = spike.hrv.hrv(beat_times_ms)               # HrvResult: mean_hr_bpm, sdnn_ms, rmssd_ms, pnn50_pct, lf_ms2, hf_ms2, lf_hf (spectrum_ok needs >= 9 beats)

# prc: phase-response curve from a cycle marker's spike times and the times of the perturbations
c = spike.prc.prc_estimate(spike_times_ms, perturb_times_ms)    # PrcResult: period_ms, points[(phase, shift)], .phase / .shift arrays
bins = spike.prc.prc_bins(c, 10)
# (the closed-loop PRC experiment, pulses at evenly spaced phases of a running oscillator, is C++ only: spike::analysis::run_prc_experiment)

# 4-node motifs: the 199 connected directed classes, ESU enumeration, degree-preserving null as for the triads
counts = spike.motifs.census4(edges, n)
res4 = spike.motifs.zscores4(edges, n, n_null=100, seed=1)      # MOTIFS4 / BY_ID4 give each class' id, edge and mutual-pair counts
```

## Neuromodulators, default modulation and the NESTML cells

`net.set_modulator("ACh", 0.5)` sets a global level (`NE`, `ACh`, `DA`) and applies the circuit's `modulation` table at once (the C++ engine applies it at the start of the next step). `net.modulator(name)` reads it back. Release from `"modulator"` outputs is closed-loop engine behaviour and is **not** simulated in Python.

**Default modulation** (mirrors the engine): a population whose model has an `ne` and/or `ach` parameter (`sa_node_pacemaker`, `butera_burster`, `icc_slow_wave`) follows the NE / ACh levels with gain 1 and no table, so `set_modulator("ACh", 1.0)` slows a plain SA cell. A row in the circuit's `modulation` table for a `(population, parameter)` overrides the default for that parameter only. See "Default modulation" in the format spec.

The NESTML cells need the PyNEST loader library: run `scripts/build-nestml.sh --pynest` (rebuild it after any change to `models/nestml/*.nestml`; a stale library gives different spikes from the engine). `spike.build` registers the cells itself after resetting the kernel. If a circuit uses a NESTML model that cannot be registered, `build` raises `CircuitError` naming the model, the underlying failure and the build step (it no longer skips silently, which used to surface as an unknown-model error from NEST). Set `SPIKE_NESTML_LIB` to use a library elsewhere. Circuits without NESTML models do not need it.

## Parity guarantees and limits

`spike.build(c)` and the C++ `spike-run` give identical spikes (same neuron and same step) for the same circuit, seed and thread count. This holds when:
- both use the **same NEST commit** (see `NEST_REF` in `scripts/build-nest.sh`);
- both use the **same OpenMP setting** (`SPIKE_OPENMP`) and the same number of threads (`threads=` here, `--threads` for `spike-run`);
- the circuit uses only features documented in the format spec.

Initial membrane potentials: when a population has no `V_m` in `params`, each neuron gets a seeded random `V_m` between `E_L` and `V_th` (a model can have its own range: `sa_node_pacemaker` uses -74 to -45 mV so that uncoupled clones start at different beat phases; `spike.circuit.VM_INIT_RANGE`), from a splitmix64 stream keyed by seed and neuron index (not NEST's RNG), implemented identically in `spike/circuit.py` and the engine. Set `V_m` explicitly to start at rest.

Limits: `spike.build` does not run the C++ loader's validation (unknown keys, delay below resolution), so a file can build in PyNEST and still be rejected by the game. There are no tasks or body modules in Python (tasks are scored with the C++ `tasks_circuits_test run` tuning aid or in the game).

## Tests

```
pytest python                          # PyNEST tests skip when PyNEST is missing
SPIKE_REQUIRE_NEST=1 pytest python     # ...fail instead (ctest sets this)
```
The parity and C++-vs-Python motif tests need the built `spike-run` and `analysis_test` (`SPIKE_RUN`, `SPIKE_ANALYSIS_TEST` override the search under `build*/`).
