---
title: Use your circuit in Python
section: Under the hood
order: 81
summary: Load a circuit you built in the game as a PyNEST network, run it, and analyse the spikes with the spike package.
---

# Use your circuit in Python

The `spike` Python package is the research side of the project. It builds the same [circuit file](circuit-is-data.html) the game saves as a PyNEST network, runs it, and analyses the spikes. PyNEST is NEST's own Python interface. Because the game uses NEST too, you can take a circuit you built in the [lab](how-to-read-the-lab.html), run it for as long as you like, and measure it with your own code.

There are no tasks, bodies or scoring in Python. To score a circuit on a level, use the game or the [spike-run command line](spike-run.html).

> **Note:** The Python package covers the classic part of the circuit format only: spiking populations, projections, the `dc`, `poisson`, `spike_generator` and `sinusoidal_poisson` inputs, and the neuromodulator table. It does not have the Theory wing's pieces: no `rate_drive` or `tuned_poisson` input (they raise `CircuitError: unsupported input kind`), no rate populations or mean-field links, and no `rate_value` output. It builds no outputs at all (`rate`, `modulator`, `population_vector`): by default the spikes of every cell are recorded, and you read what you need from them. A circuit that uses them will not build in Python; a Theory level that uses only spiking cells and the classic inputs builds as usual.

## Install

From the source tree:

```sh
pip install -e python[test]
```

This installs the package (it needs Python 3.10 or newer and numpy). The `test` extra adds pytest and networkx; the `notebook` extra adds jupyter and matplotlib.

Some modules need only numpy: `spike.motifs`, `spike.stats`, `spike.bursts`, `spike.hrv` and `spike.prc`. Building and running a network needs PyNEST. The package imports it only when you call `spike.build` or `spike.run`, so the analysis modules work without it.

To get PyNEST, build it from the repository's pinned NEST commit:

```sh
scripts/build-nest.sh --pynest
```

This installs into `third_party/nest-install`. The tests find it there. Otherwise set `SPIKE_PYNEST` to a NEST `site-packages` folder, or put it on `PYTHONPATH`. Details are on [building from source](building-from-source.html).

> **Warning:** Use the NEST that the repository pins. A different NEST version can give different spikes from the game even with the same file.

## A first run

```python
import spike

net = spike.build("python/examples/ei_net.json", threads=1)
sp = spike.run(net, 1000.0)          # 1000 ms of model time

rates = spike.stats.firing_rates(sp, net.n_neurons, 0, 1000)   # Hz per cell
cv = spike.stats.isi_cv(sp, net.n_neurons, 0, 1000)
chi = spike.stats.golomb_chi(sp, net.n_neurons, 0, 1000)
```

The example file is a balanced network of 80 excitatory and 20 inhibitory cells. A balanced network is one where excitation and inhibition roughly cancel, so cells fire irregularly.

What the pieces are:

| Name | What it does |
|---|---|
| `spike.build(circuit, threads=1, seed=None, verbose=False)` | Builds the network in PyNEST and resets the NEST kernel. `circuit` can be a path to a `.json` file, a JSON string or a dict. `seed` overrides the file's seed. Returns a `Network`. |
| `spike.run(net, ms, drain=False)` | Simulates `ms` of model time and returns a `Spikes` object with every spike so far. With `drain=True` it returns only the spikes since the last call. |
| `spike.load_circuit(circuit)` | Reads a path, string or dict into a dict. |
| `Spikes` | Two arrays, `neuron` and `t_ms`, sorted by time. `len(sp)`, `sp.pairs()` and `sp.steps(resolution_ms)` are helpers. |
| `Network` | The built network. `net.n_neurons` is the cell count; `net.population(id)` gives a population's NEST nodes; `net.population_slice(id)` gives its range of cell indices. |

Cell indices are the circuit-level ones: 0 to N-1 across all populations in file order.

## Changing things while it runs

| Method | Use |
|---|---|
| `net.set_input(name, value)` | Set a `dc` amplitude (pA) or a `poisson` rate (Hz), or the mean rate of a `sinusoidal_poisson` input. A `spike_generator` raises an error: use `queue_spikes`. |
| `net.set_input_param(name, key, value)` | Change `rate`, `amplitude`, `freq_hz` or `phase` of a `sinusoidal_poisson` input. |
| `net.queue_spikes(name, times_ms)` | Add spikes to a `spike_generator` input. Times are absolute model time in ms, on the resolution grid, strictly in the future. |
| `net.set_modulator(name, conc)` | Set a neuromodulator level (`NE`, `ACh`, `DA`) and apply the circuit's `modulation` table at once. |
| `net.modulator(name)` | Read a level back. |

A neuromodulator that a circuit's own `modulator` output would release is not simulated in Python. That needs the game's closed-loop engine.

## Analysis modules

All of these are plain numpy, and each mirrors a routine in the C++ engine. The tests check that they agree number for number with the game's analysis screen (see [the lesion lab and analysis tools](lesion-lab-and-analysis.html)).

**`spike.stats`**

- `firing_rates(sp, n, t0_ms, t1_ms)` gives Hz per cell.
- `isi_cv(...)` gives the coefficient of variation of the inter-spike intervals (the gaps between spikes). Near 1 is Poisson-like; near 0 is clock-like.
- `golomb_chi(...)` measures synchrony of the whole population, from 0 (none) towards 1 (full).
- `psth(...)` and `psth_aligned(...)` give a peri-stimulus time histogram: spike counts in bins, optionally aligned to event times.

**`spike.motifs`**

A motif is a small connection pattern between three or four cells. A census counts how often each appears.

- `edges_from_nest(net)` returns `(pre, post, weight)` triples.
- `census(edges, n)` counts the 13 three-cell classes.
- `zscores(edges, n, n_null=100, seed=1)` compares them with degree-preserving random rewirings and gives z-scores. The class numbers follow Milo and colleagues.
- `census4` and `zscores4` do the same for the 199 connected four-cell classes.

**`spike.bursts`**

- `detect_bursts(times, BurstParams(...))` finds bursts with the max-interval method (Cocatre-Zilgien and Delcomyn 1992), the same one the game uses.
- `pooled_spike_times`, `detect_bursts_per_neuron` and `summarize_bursts` give per-cell and summary numbers (number of bursts, mean period, mean duration, spikes per burst, fraction of spikes in bursts).

**`spike.hrv`**

- `hrv(beat_times_ms)` returns heart-rate variability: mean heart rate, SDNN, RMSSD, pNN50 and the low- and high-frequency power from a Lomb-Scargle spectrum. The spectrum needs at least 9 beats.

**`spike.prc`**

- `prc_estimate(spike_times_ms, perturb_times_ms)` estimates a phase-response curve: how much a perturbation at each phase of the cycle shifts the next spike.
- `prc_from_spikes` and `prc_bins` help with cells and binning.
- The closed-loop experiment that delivers pulses at even phases is C++ only.

## Custom cells

The custom NESTML cells need an extra loader library. Build it with `scripts/build-nestml.sh --pynest`, and rebuild it after any change to a `.nestml` file; a stale library gives different spikes from the engine. `spike.build` registers the cells itself. If a circuit uses a custom model that cannot be registered, `build` raises `CircuitError` naming the model and the build step. Set `SPIKE_NESTML_LIB` to use a library in another place. Circuits without custom cells do not need it. See [make it yours](make-it-yours.html).

## Parity

`spike.build(c)` and `spike-run` give identical spikes (same cell, same step) for the same circuit, seed and thread count, when:

1. both use the same NEST commit (`NEST_REF` in `scripts/build-nest.sh`);
2. both use the same OpenMP setting (`SPIKE_OPENMP`, off by default) and the same number of threads (`threads=` in Python, `--threads` for `spike-run`);
3. the circuit uses only features the package supports (see the note at the top), and gives a `seed`.

The engine and the package both draw the starting voltages with the same integer hash generator, keyed by seed and cell index, so the starts match. A circuit with no `seed` is seed 1 in the engine, but the package reads a missing seed as 0, so the starts would differ: always write a `seed`, or pass `seed=` to `spike.build`.

## Limits

- `spike.build` repeats only some of the C++ loader's checks: the synapse rules (gap junctions, plasticity, weight spread, modulation rows) and the weight ceilings of 500 nS and 10000 pA onto conductance and current cells. It does not check unknown keys, delays against the resolution, the size and connection budgets (20000 cells, about 2 million connections), the rate, current and delay ceilings, the larger weight ceiling of the custom cells, or the nesting depth. A file can build in PyNEST and still be refused by the game and by `spike-run`; see [limits and errors](circuit-is-data.html#limits-and-errors).
- Rate units, mean-field links, `tuned_poisson` and `rate_drive` inputs and `rate_value` outputs (the Theory wing) are not built in Python yet. `spike.build` refuses such a circuit with a plain message that says so.
- `spike.build` mirrors the engine's size and range limits (20000 neurons in all, about 2 million connections, delays up to 1000 ms, rates and constant currents up to 100000, JSON nested at most 32 levels) and its seed rule: an integer from 1 to 4294967295, and a missing seed is 1, as in the engine.
- A `spike-run --task` run is the way to see a level's score; Python only runs the network.

## Tests and the example notebook

```sh
pytest python                       # PyNEST tests skip if PyNEST is missing
SPIKE_REQUIRE_NEST=1 pytest python  # fail instead of skipping
```

`python/examples/motifs.ipynb` walks through building a circuit, running it and counting motifs. The parity tests need the built `spike-run` and `analysis_test` (set `SPIKE_RUN` and `SPIKE_ANALYSIS_TEST`, or let the tests find them); under ctest the whole suite is the `python_pytest` test.

> **Note:** The package version in `python/pyproject.toml` is 0.5.2.

## Related pages

[A circuit is data](circuit-is-data.html), [the spike-run command line](spike-run.html), [architecture](architecture.html), [glossary](glossary.html).
