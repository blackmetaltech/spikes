# Circuit format (schema_version 1)

A circuit is one JSON file. The game, the C++ engine, the Python package and every exporter read it, so a circuit built in the game runs unchanged in PyNEST. All NEST model names and parameter names pass straight through (NEST units: mV, ms, pA, nS, Hz).

```json
{
  "schema_version": 1,
  "name": "Half-centre oscillator",
  "resolution_ms": 0.1,
  "seed": 42,
  "populations": [
    { "id": "L", "model": "aeif_cond_alpha", "n": 1, "params": { "a": 4.0, "b": 80.5 }, "pos": [[120, 200]], "role": "neuron" },
    { "id": "R", "model": "aeif_cond_alpha", "n": 1, "params": {}, "pos": [[320, 200]] }
  ],
  "projections": [
    { "pre": "L", "post": "R", "rule": "all_to_all", "synapse": { "synapse_model": "static_synapse", "weight": -30.0, "delay": 2.0 } },
    { "pre": "R", "post": "L", "rule": "one_to_one", "synapse": { "synapse_model": "stdp_synapse", "weight": 5.0, "delay": 1.0, "tau_plus": 20.0 } },
    { "pre": "L", "post": "R", "rule": "fixed_indegree", "indegree": 1 },
    { "pre": "L", "post": "R", "edges": [[0, 0, 5.0, 1.0]] }
  ],
  "inputs": [
    { "name": "drive", "kind": "dc", "targets": ["L", "R"], "weight": 1.0, "value": 300.0 },
    { "name": "touch", "kind": "poisson", "targets": [{ "pop": "L", "idx": [0] }], "weight": 20.0, "value": 0.0 }
  ],
  "outputs": [
    { "name": "left_motor", "kind": "rate", "neurons": [{ "pop": "L", "idx": [0] }] }
  ],
  "record": { "spikes": "all", "voltage": [{ "pop": "L", "idx": [0] }] },
  "meta": { "author": "", "notes": "", "task": "lamprey_cpg" }
}
```

## Rules
- **Neuron indices.** Circuit-level neuron indices run from 0 to N−1 over the populations in file order. Inside a population, `idx` is 0-based. The engine converts to NEST's 1-based node IDs.
- **Projections.** A projection has either `rule` (any NEST connection rule, with its parameters next to it, for example `indegree`, `p` or `allow_autapses`) or `edges`. Each edge is `[pre_idx, post_idx, weight, delay]` with optional extra synapse fields. `synapse` is a NEST syn_spec dict, and `synapse_model` defaults to `static_synapse`. Projections are numbered in file order, and that number is the `projection` argument of `Engine::set_weight`.
- **Inputs.** Each input creates one NEST device: `dc` makes a `dc_generator` (value in pA) and `poisson` makes a `poisson_generator` (value in Hz). The device connects to its targets with `weight`, and `value` is the initial amplitude or rate.
- **Empty channels.** `targets` (inputs) and `neurons` (outputs) may be empty (`[]`). The device is still created, nothing is connected, and an empty output always reads 0. This lets the editor run a half-wired circuit.
- **Outputs** are readouts over a set of neurons: `"kind": "rate"` (default, spike counts) or `"kind": "modulator"` (see "Neuromodulation").
- **Initial membrane potential.** If a population's `params` has no `V_m`, and the model has `V_m`, `E_L` and `V_th` with `V_th > E_L` (after `params` are applied), each neuron gets a seeded random `V_m = lo + u * (hi - lo)`, with `u` in [0, 1) and `[lo, hi]` = `[E_L, V_th]`, except for models with their own range: `sa_node_pacemaker` uses `[-74, -45]` mV (its E_L = -30 mV and V_th = 0 mV are not a resting range: drawing from them started every clone depolarised and in lock-step, so five uncoupled cells scored as one; from -74 to -45 mV the clones start at different phases of the diastolic depolarisation and only coupling brings them together). The table is `vm_init_range()` in `engine/src/circuit.cpp` and `VM_INIT_RANGE` in `python/spike/circuit.py`. This breaks symmetry between identical cells (for example a half-centre). `u` comes from splitmix64 keyed by the circuit `seed` and the circuit-level neuron index `i`, independent of NEST's RNG: `z = seed + (i+1) * 0x9E3779B97F4A7C15 (mod 2^64)`; `z = (z ^ z>>30) * 0xBF58476D1CE4E5B9`; `z = (z ^ z>>27) * 0x94D049BB133111EB`; `z ^= z>>31`; `u = (z >> 11) / 2^53`. The C++ engine and `python/spike/circuit.py` implement exactly this. Set `V_m` explicitly (for example to the resting potential) to disable it; the reference circuits and the `iaf_dc` golden do so where timing must be analytic.
- **Top-level keys.** `schema_version`, `name`, `resolution_ms`, `seed`, `populations`, `projections`, `inputs`, `outputs`, `modulation`, `record`, `meta`.
- **`pos`** holds editor coordinates, one `[x, y]` per neuron. It is optional and ignored by simulators.
- **Determinism.** `seed` sets NEST's `rng_seed`. The same file and the same seed give the same spikes for the same NEST version and thread count.
- **Unknown keys** under `meta` are preserved. Unknown top-level keys are an error.

## Weight spread (v0.3)

A projection (`edges` or `rule`) may carry one of two projection-level keys (next to `pre`, `post`), both numbers >= 0:

| key | per-edge weight |
|---|---|
| `weight_sd` | `w' = w + sd * z` (same unit as the weight) |
| `weight_cv` | `w' = w * (1 + cv * z)` (relative spread) |

`z` is a standard normal draw that depends only on the circuit `seed`, the projection number `k` (file order, 0-based) and the edge index `i`, never on NEST's RNG: `key = (((k+1) << 32) | i) * 2 + j + 1` for the two uniforms `j = 0, 1`; `z64 = seed + key * 0x9E3779B97F4A7C15 (mod 2^64)`, then the same splitmix64 finalizer as the initial `V_m`; `u_j = (z64 >> 11) / 2^53`; `z = sqrt(-2 ln(1 - u_0)) * cos(2 pi u_1)`. The result keeps the sign of `w`: `|w'| >= 1e-3 |w|`. On `edges` projections it is applied when the circuit is loaded, before the synapse rules (so a `stdp_synapse` default `Wmax` sees the spread weights, and a gap-junction mirror edge gets the same value). Giving both keys or a negative number is an error.

**On a `rule` projection** (`all_to_all`, `one_to_one`, `fixed_indegree`, ...) the targets are drawn by NEST, so the spread is applied after the connect, per connection, through NEST's connection weights: the projection's connections are read back, sorted by `(source node id, target node id, port)` (canonical, independent of thread layout), and the connection with rank `i` in that order gets `z` for `(seed, k, i)` and the formula above, starting from the weight NEST gave it (the `synapse.weight` number, or the drawn value of a weight distribution). The Python mirror does the same through `GetConnections` / `set`; `python/tests/circuits/weight_spread_rule.json` is the parity fixture. Not supported on a rule projection with `gap_junction` (it must stay symmetric) or `stdp*` synapses (`Wmax`): use `edges` there. Because `spike-run --task --seeds` reseeds the circuit (`1000 * seed + circuit seed`), every task seed sees a different spread: this is what the deferred 0.2 "weight spread" control uses to make a circuit that only works with identical weights fail honestly. The C++ loader (`spread_normal` / `spread_weight` in `engine/src/circuit.cpp`) and `python/spike/circuit.py` (`edge_spread_normal`) agree bit for bit (`python/tests/test_parity.py` runs `weight_spread.json` through both; `engine/tests/test_circuit_spread.cpp`, `python/tests/test_weight_spread.py`).

## Instant check (headless scoring from the game)

The engine is one per process, so the game scores a circuit headless by running the `spike-run` CLI in a child process, from a worker thread:

```
spike-run --task <id> --circuit file.json --seeds 1,2,3 [--mode guided|standard|researcher] [--variant <id>]
```

Add `--circuit-seed n` or `--circuit-seeds a,b,c` to pin the circuit seed instead of the `1000 * seed + circuit seed` reseed (one value for every seed, or one per seed): the game passes its live circuit seed so the instant check scores the same circuit (the same weight spread, the same initial V_m) as the run the player watched. Each run reports `circuit_seed`.

It prints one JSON object (per seed: `seed`, `circuit_seed`, `value`, `passed`, `verdict`, `parts`, `hints`, `metrics`, `neurons`, `spikes`; plus `n`, `passes`, `mean_value`). In Godot: `SpikeTask.score_headless_async(circuit_json, seeds, mode, variant, circuit_seed = -1)` returns a job id at once (a thread runs the process), or 0 if `spike-run` cannot be found or started (a warning names the places searched); `circuit_seed` >= 0 pins the circuit seed for every task seed (-1 = the reseed above); poll `SpikeTask.score_headless_poll(job)` each frame (`{done, ok, error, ms, passed, value, passes, seeds, runs}`; a finished job is returned once, then forgotten). `SpikeTask.headless_cancel(job)` kills the child process (its whole process group), deletes its temp files and forgets the job (true if it was still running; the job then reads as unknown). The circuit goes through a private `mkstemp` file (mode 0600, unique per job, in `$TMPDIR` or `/tmp`) that is deleted when the job ends, is cancelled, or the game exits; the child's stderr likewise. The runner is searched in this order: `SpikeTask.set_headless_runner(path)`, `$SPIKE_RUN`, the directory of the GDExtension library and its `bin/`, the directory of the executable and its `bin/`, the project's `res://bin` (globalized), a development tree (`./spike-run`, `build/bin/spike-run`, `build/engine/spike-run`, ...), then PATH; `SpikeTask.headless_runner_path()` returns the one that will be used (empty if none). A shipped game puts `spike-run` next to the extension library (or in `res://bin`). A task runs 1 ms slices, so the wall time is about the model duration / 50 per seed on a debug build and less on release; micro-puzzles (2-5 s episodes) finish in well under 2 s.

## Per-task sensor weights (v0.3)

`Task::ChannelKind::weight` (inputs; 0 = unset) is the default synaptic weight the lab uses the first time the player connects that sensor to a population. `SpikeTask.channel_kinds()["inputs"][name]["weight"]` exposes it.

## Synapse models

`synapse_model` may be any NEST connection model; these have extra rules, checked when the circuit loads (errors name the path, for example `projections[2].synapse.weight: ...`). The same rules are implemented in `python/spike/circuit.py`.

| model | rules |
|---|---|
| `gap_junction` | Electrical coupling. Needs `hh_psc_alpha_gap` or `hh_cond_beta_gap_traub` on **both** sides. Instantaneous: `delay` is an error (the 4th element of an edge is ignored). `weight` is a conductance in nS and must be > 0. Always symmetric: with `rule: one_to_one` the loader sets `make_symmetric: true`; `all_to_all` is allowed only within one population (pre = post; autapses default to off); `edges` projections get their mirror edge (post to pre) added by the engine. `set_weight` changes both directions. Other rules are rejected. |
| `tsodyks2_synapse` | Short-term plasticity (depression and facilitation). `U` in [0, 1] (utilisation), `u` in [0, 1], `x` in [0, 1], `tau_rec` > 0 ms, `tau_fac` >= 0 ms (0 = depression only). `weight` is the absolute amplitude scale; the first PSP is `weight * U`. |
| `quantal_stp_synapse` | Stochastic release with short-term plasticity: `n` release sites (integer >= 1), `a` available sites (integer, 0 to `n`), `U`, `u`, `tau_rec`, `tau_fac` as above. `weight` is the amplitude of one released quantum. |
| `stdp_synapse` | Spike-timing-dependent plasticity, excitatory only: every `weight` must be > 0 (inhibitory STDP is rejected with an error: use `static_synapse` or `tsodyks2_synapse`). `Wmax` defaults to 2 x `weight` (for `edges`: 2 x the largest edge weight); if `weight` is a distribution, `Wmax` must be given. `weight` > `Wmax` is an error. `tau_plus` > 0 ms. |
| `stdp_dopamine_synapse` | Dopamine-gated STDP (reward learning). Same weight and `Wmax` rules as `stdp_synapse` (plus `Wmin` in [0, Wmax], `tau_c`, `tau_n` > 0 ms). The projection needs a `"dopamine_source": "<population id>"` (a projection-level key, next to `pre` and `post`). |

### Dopamine-gated STDP

For each distinct `dopamine_source` population the engine creates one NEST `volume_transmitter`; every spike of that population is a dopamine pulse. The synapse's common properties (`A_plus`, `A_minus`, `Wmin`, `Wmax`, `b`, `tau_c`, `tau_n`, `tau_plus` and the transmitter itself) are set on a private copy of the model named `stdp_dopamine_synapse__p<k>` (k = projection number), because NEST cannot set them per connection. The engine's own DA level (`Engine::set_modulator("DA", conc)`, or released by a `modulator` output) also drives every transmitter: a Poisson generator through a `parrot_neuron` (a device cannot connect to a transmitter) fires at 100 Hz per unit of DA. With no pulses and `b = 0` the weights do not change.

## Neuromodulation

Three engine-level concentration channels, `NE` (noradrenaline), `ACh` (acetylcholine) and `DA` (dopamine), in arbitrary units (0 = baseline; typically 0 to 1). `Engine::set_modulator(name, conc)` sets an external level and `Engine::modulator(name)` reads the total: external level plus the release pools below, as of the end of the last step.

### The `modulation` table

An array that maps a modulator to a parameter of **every neuron of one population**. It is applied between steps (at the start of `step()`, only when the value changed), never inside one.

```json
"modulation": [
  { "modulator": "NE",  "pop": "SA", "param": "ne",  "gain": 1.0 },
  { "modulator": "ACh", "pop": "SA", "param": "ach", "gain": 1.0 },
  { "modulator": "NE",  "pop": "R",  "param": "I_e", "scale_per_unit": 0.5, "min": 0.0, "max": 800.0 }
]
```

| key | meaning |
|---|---|
| `modulator` | `"NE"`, `"ACh"` or `"DA"` |
| `pop` | population id (the whole population) |
| `param` | a numeric NEST parameter of the population's model (checked at load) |
| `gain` | additive: `value = value + gain * conc` |
| `scale_per_unit` | multiplicative: `value = value * (1 + scale_per_unit * conc)` |
| `min`, `max` | optional clamp of the result |

Exactly one of `gain` and `scale_per_unit` per row. The starting `value` is the parameter after the population's `params` are applied. Rows on the same `(pop, param)` combine in file order. Use `gain` for parameters that are the channel themselves (a NESTML cell's `ne` input: base 0, so `value = conc`) and `scale_per_unit` or `gain` on a real parameter such as `I_e`. A modulated parameter is owned by the table: live edits with `set_neuron_param` on it are overwritten the next time the level changes.

### Default modulation

A population whose model has an `ne` and/or `ach` parameter (the NESTML cells `sa_node_pacemaker`, `butera_burster`, `icc_slow_wave`) follows the circuit-wide levels without any table: the engine adds the implicit rows `{NE -> ne, gain 1, min 0}` and `{ACh -> ach, gain 1, min 0}`, after the circuit's own rows. A circuit overrides this **per (population, parameter)** by listing any row for that pair in its `modulation` table (for example `baroreflex` maps ACh to `ach` with gain 0.5); parameters it does not mention keep the default. So `Engine::set_modulator("ACh", 0.5)` slows a plain SA cell on any circuit, with an empty `modulation`. Models without `ne` / `ach` parameters are untouched. `python/spike/circuit.py` does the same (`DEFAULT_MODULATION`).

### Output kind `"modulator"`

Spikes of a population release a modulator, so a circuit can make its own NE or ACh:

```json
{ "name": "locus_coeruleus", "kind": "modulator", "modulator": "NE", "neurons": [{ "pop": "LC" }], "gain": 0.05, "tau_ms": 1000 }
```

Each spike adds `gain` (default 0.05 concentration units) to the output's pool, which decays with time constant `tau_ms` (default 1000 ms): `dc/dt = -c / tau_ms`, exactly, from each spike's own time. At a steady rate `f` the level is `gain * f * tau_ms / 1000`. `modulator(name)` is the external level plus every pool of that modulator. The output also counts spikes like a `rate` output (`output_rate`, `output_count`).

`python/spike/circuit.py` applies the table (`Network.set_modulator`) and builds the dopamine plumbing, but does not simulate `modulator` releases: those need the closed-loop engine.

### Creation order additions

After the recorder, nodes are created in this order (neuron ids are unchanged): one `volume_transmitter` per distinct `dopamine_source` (order of first use), then one `poisson_generator` (rate 0 Hz) and one `parrot_neuron`, only if a `stdp_dopamine_synapse` projection exists. Connections: each source population to its transmitter, generator to parrot, parrot to every transmitter (all `static_synapse`, weight 1, delay = resolution), then the projections.

## Parts bin (catalog)

`engine/include/spike/catalog.h` lists the cell models the game offers as cards (description, biological examples, key parameters with unit and range, default input weight, E/I suitability, thumbnail drive). `build/engine/spike-catalog` measures every card in its own process (one engine per process) and writes `catalog/thumbs.json`: for each card the f-I points (`fi`: rate at each drive level) and a V_m `trace` at the level nearest 20 Hz. Each card declares its window in the `probe` block of `catalog_json()` (`run_ms`, `skip_ms`, `trace_ms`, `trace_dt_ms`; default 1.2 s run, first 0.2 s discarded). Slow oscillators are measured over **at least 10 cycles**: a level is extended in `run_ms` chunks until more than `min_cycles` (10) spikes were seen or `max_ms` is reached, and the rate is then `(n-1)/(t_last-t_first)`. So the SA node card (one beat per cycle) runs 12 s, extended up to 60 s; the Butera burster runs 60 s (its rate is spikes per second, many per burst); the ICC card (a wave every ~20 s) runs 60 s, extended up to 6 min. The card's `vm_init` is the seeded initial V_m range the engine uses for the model (null = the default `[E_L, V_th]`).

Every card has a measured default input `input_weight` with a `weight_unit`: the weight at which one presynaptic spike gives a unitary EPSP of about 1-2 mV at rest (measured with a single `spike_generator` spike; Izhikevich presets 2 mV: NEST steps `V_m` by exactly the weight; `hh_cond_exp_traub` 2 nS; `ht_neuron` 1 on the AMPA port; `sa_node_pacemaker` 0.2 nS; `butera_burster` 0.05 nS; `icc_slow_wave` 200 pA). The parrot relay is unit-free. Use this one table for default weights in the editor. Cards for the NESTML cells are marked `available: false` until the module is compiled into the build.

## Structure (v0.4)

`Engine::structure()` reports populations (id, model, size, first neuron index, numeric params, E/I `sign` from the summed outgoing weights), projections (pre and post population, rule or `edges`, synapse model, connection count, mean/min/max weight, sign of the mean weight, in-degree mean and max) and channels (the populations each input targets and each output reads), read from the live NEST connections. `has_path`, `has_loop` and `has_loop_via` answer reachability and feedback-loop questions on that graph; projections with zero connections or a zero mean weight are not edges. API and semantics: `docs/ENGINE-STRUCTURE.md`.
