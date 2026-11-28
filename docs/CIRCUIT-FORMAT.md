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
    { "name": "touch", "kind": "poisson", "targets": [{ "pop": "L", "idx": [0] }], "weight": 20.0, "value": 0.0 },
    { "name": "taps", "kind": "spike_generator", "targets": ["R"], "weight": 20.0, "spike_times": [50.0, 100.0, 150.0] },
    { "name": "theta", "kind": "sinusoidal_poisson", "targets": ["R"], "weight": 5.0, "rate": 40.0, "amplitude": 30.0, "freq_hz": 8.0, "phase": 0.0 }
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
- **Inputs.** Each input creates one NEST device: `dc` makes a `dc_generator` (value in pA) and `poisson` makes a `poisson_generator` (value in Hz). The device connects to its targets with `weight`, and `value` is the initial amplitude or rate. Two more kinds (0.5, see "Spike-train inputs"): `spike_generator` and `sinusoidal_poisson`.
- **Empty channels.** `targets` (inputs) and `neurons` (outputs) may be empty (`[]`). The device is still created, nothing is connected, and an empty output always reads 0. This lets the editor run a half-wired circuit.
- **Outputs** are readouts over a set of neurons: `"kind": "rate"` (default, spike counts), `"kind": "modulator"` (see "Neuromodulation") or `"kind": "population_vector"` (0.5, see "Population-vector output").
- **Initial membrane potential.** If a population's `params` has no `V_m`, and the model has `V_m`, `E_L` and `V_th` with `V_th > E_L` (after `params` are applied), each neuron gets a seeded random `V_m = lo + u * (hi - lo)`, with `u` in [0, 1) and `[lo, hi]` = `[E_L, V_th]`, except for models with their own range: `sa_node_pacemaker` uses `[-74, -45]` mV (its E_L = -30 mV and V_th = 0 mV are not a resting range: drawing from them started every clone depolarised and in lock-step, so five uncoupled cells scored as one; from -74 to -45 mV the clones start at different phases of the diastolic depolarisation and only coupling brings them together). The table is `vm_init_range()` in `engine/src/circuit.cpp` and `VM_INIT_RANGE` in `python/spike/circuit.py`. This breaks symmetry between identical cells (for example a half-centre). `u` comes from splitmix64 keyed by the circuit `seed` and the circuit-level neuron index `i`, independent of NEST's RNG: `z = seed + (i+1) * 0x9E3779B97F4A7C15 (mod 2^64)`; `z = (z ^ z>>30) * 0xBF58476D1CE4E5B9`; `z = (z ^ z>>27) * 0x94D049BB133111EB`; `z ^= z>>31`; `u = (z >> 11) / 2^53`. The C++ engine and `python/spike/circuit.py` implement exactly this. Set `V_m` explicitly (for example to the resting potential) to disable it; the reference circuits and the `iaf_dc` golden do so where timing must be analytic.
- **Top-level keys.** `schema_version`, `name`, `resolution_ms`, `seed`, `populations`, `projections`, `inputs`, `outputs`, `modulation`, `groups` (0.6), `record`, `meta`.
- **`pos`** holds editor coordinates, one `[x, y]` per neuron. It is optional and ignored by simulators.
- **Determinism.** `seed` sets NEST's `rng_seed`. The same file and the same seed give the same spikes for the same NEST version and thread count.
- **Unknown keys** under `meta` are preserved. Unknown top-level keys are an error.

## Locked parts and groups (0.6, the Atlas wing; add-only)

A locked-wiring level gives the player a circuit whose wiring is data, and lets the player set only what the data does not contain: per cell type, a sign and a scale. A file without `groups` and without `locked` behaves exactly as before.

```json
"populations": [
  { "id": "I", "model": "iaf_psc_alpha", "n": 2, "locked": true },
  { "id": "M", "model": "iaf_psc_alpha", "n": 2, "locked": true } ],
"groups": [
  { "id": "I", "label": "Interneuron", "pops": ["I"], "sign": -1, "scale": 0.98,
    "free": ["sign", "scale"], "ranges": { "scale": [0.25, 2.0] } } ],
"projections": [
  { "pre": "I", "post": "M", "locked": true, "synapse": { "synapse_model": "static_synapse" }, "edges": [[0, 0, 150.0, 2.0], [1, 1, 150.0, 2.0]] } ]
```

**`locked`** (a boolean, default false) on a **projection**: its endpoints, edges, delays and synapse model are data and cannot be edited, added to or deleted. A locked projection lists its `edges` (a rule that draws its wiring is refused: the wiring must be data). On a **population**: it cannot be deleted, resized or have its model changed. The flags are carried through `Engine::structure()` (`PopulationInfo::locked`, `ProjectionInfo::locked`). The engine itself loads a locked circuit like any other; the lock is enforced where a level judges a circuit (below) and by the editor.

**`groups`** is a list of cell types. Each has:

| key | meaning |
|---|---|
| `id` | unique among groups |
| `label` | optional name shown on the card |
| `pops` | population ids in the group (non-empty; a population is in at most one group) |
| `sign` | `+1` or `-1` (default `+1`) |
| `scale` | a positive number (default `1`) |
| `free` | the parameters the player may change, a list of names (default none). Known names: `"sign"`, `"scale"` |
| `ranges` | an object with one `[lo, hi]` (0 < lo <= hi) for every free parameter that has a numeric range; `scale` needs one when it is free |
| `off` | optional boolean (default false; add-only, the worm chapter). `true` lets `scale` and the lower end of its range be 0 (0 <= lo <= hi): a cell type that can be switched off, all its outgoing weights then 0. A negative value stays an error. The key is part of the `types` digest |

At load, the weight of every connection of every projection whose **presynaptic** population is in the group becomes `|stored weight| x scale x sign` (the stored weight is a magnitude: its own sign is ignored). It applies to `edges` weights and to a rule projection's `synapse.weight` (a missing weight is NEST's default 1; a weight that is not a plain number is an error), and `weight_sd` is multiplied by `scale` (`weight_cv` is relative and unchanged). Everything downstream, the live NEST weights, `structure()`, `connections()`, `set_weight()`, sees the effective weights. Populations in no group are unaffected.

A `scale` outside its range still loads (a sandbox bench may hold it); `Structure::groups[i].scale_in_range` says so and a level rejects such a circuit. `free` and `ranges` are a table, not a fixed pair: a later free parameter (a synaptic or a membrane time constant) is one more name in `free`, one more key in the group and one more entry in `ranges`, and needs no format change (the engine's list of known names grows with its code; an older engine refuses a name it does not know with a plain error).

### Holding a submission to the given circuit

A locked level declares the circuit it gave (`Task::given()`: the file name and a **LockedDigest**). The engine computes the same digest of the circuit it actually runs (`Engine::locked_digest()`), and `make_task()`'s guard rejects a run whose digest differs, or whose free values are out of range. The digest is four SHA-256 hashes (first 32 hex digits) of canonical text, so the check lives in the engine and `spike-run` on a hand-edited file fails exactly as the game does:

| section | what it covers | verdict when it differs |
|---|---|---|
| `pops` | the resolution; every population in file order (a locked one: id, model, `n`, `params`; an unlocked one: its id) | the given cells were changed |
| `wiring` | every projection in file order (a locked one: its whole data including `locked`; an unlocked one: a placeholder) and the `modulation` table | the given wiring was changed |
| `types` | every group with every value that is not free (so `sign` and `scale` are left out when free), the `free` list and the `ranges` | a cell type was changed |
| `channels` | every input (all keys except its initial `value`) and every output | the inputs and outputs were changed |

Not part of the digest: editor positions (`pos`), `name`, `meta`, `seed`, `record`, the values of the free parameters (judged by range), a population's `role`. Whole-valued floats equal integers. A run that is rejected never passes and never scores above the cap of a rejected circuit; its verdict names no number. `spike-run --task <id> --lesion pre>post*k` on a locked level is applied to the loaded circuit (an experiment), not to the file. `spike-run --locked-digest circuit.json` prints the four hashes of a file (what a level's `given()` holds).

The worm chapter (`a_path`, `a_predict`, `a_motif`, `dx_a_worm`: `circuits/a_*_given.json`, generator `circuits/tools/gen_atlas_worm.py`, tests `engine/tests/tasks_atlas_worm_test.cpp`, `engine/tests/cheap_rows_h.inc`) uses `off` for its five interneuron classes.
The compass chapter (`a_signs`, `a_places`, `a_gain`, `dx_a_compass`: `circuits/a_*_given.json`, generator `circuits/tools/gen_atlas_compass.py`, tests `engine/tests/tasks_atlas_compass_test.cpp`, `engine/tests/cheap_rows_h.inc`) uses a free `sign` on three cell types in `a_signs`, and a free `scale` with a `ranges` entry on one cell type in `a_gain`; its inputs are the sixteen cue sensors and a negative `clear` current on every cell, its outputs sixteen `rate` readouts that each name the E-PG cells of one heading with `idx`.

The development-only level `atlas_probe` (`circuits/atlas_probe_given.json`, its reference `circuits/atlas_probe.json`, generated by `circuits/tools/gen_atlas_probe.py`) proves the loop; tests: `engine/tests/tasks_atlas_test.cpp`, `engine/tests/cheap_rows_g.inc`.

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

## Weight limits

A synapse onto a conductance-based cell (any model with `_cond_` in its name, such as `aeif_cond_alpha`, `iaf_cond_alpha`, `iaf_cond_exp`, `hh_cond_exp_traub`) must have `|weight| <= 500` nS; onto a current-based cell (`_psc_` / `_curr_`, such as `iaf_psc_alpha`) `|weight| <= 10000` pA. This is checked at load for every edge (after weight spread) and for a plain `synapse.weight`, and the error names the path (for example `projections[0].edges[3]: weight 10000.000000 is outside the allowed range |w| <= 500 nS for aeif_cond_alpha ...`). `Engine::set_weight` applies the same ceiling and returns false with `error()` set. The four NESTML cells (`butera_burster`, `stg_cell`, `sa_node_pacemaker`, `icc_slow_wave`) take `|weight| <= 2000` nS and a constant current `I_e` of at most 2000 pA and a `dc` input of at most 1000 pA (weight x value; `set_input` clamps to it). Gap junctions are limited to 100 nS (see "Budgets"); other models are not limited. Why: a very large conductance makes the GSL integrator of `aeif_cond_*` fail ("numerical instability"); a NEST built without `NDEBUG` then aborts the whole process at the next simulate call (`simulation_manager.cpp: inconsistent_state`).

Even inside the limits a cell can diverge (for example 500 nS onto a tiny `C_m`). `Engine::step` then sets `error()` ("... numerical instability ... reload the circuit"), `last_step_ok()` is false, and every further `step()` is refused with an error until `load()` is called again: the engine never forwards a step to NEST after a failed update.

## Budgets (checked at load, with plain messages)

A circuit that asks for more than the engine can do responsively is refused before anything is built: at most 20000 neurons in total, about 2 million connections (estimated from the rule: `all_to_all` n x m, `fixed_indegree` indegree x n_post, `fixed_total_number` N, `edges` their count; a gap junction counts twice), every `delay` at most 1000 ms, poisson / sinusoidal / tuned rates at most 100000 Hz, constant currents (`dc` weight x value, `I_e`) at most 100000 pA (2000 pA on NESTML cells), a `gap_junction` weight at most 100 nS, JSON nested at most 32 levels, `seed` an integer from 1 to 4294967295 (a missing seed is 1). `Engine::set_input` clamps rates and currents to the same limits, and `rate_drive` values are clamped to [-500, 1e6] where they are written, so a player cannot reach a failed integral that way. A cell that nevertheless stalls or goes non-finite ends the step with an error: NESTML cells carry a step-count / minimum-step guard (injected by `scripts/nestml_generate.py`), and `Engine::step` has a watchdog (wall clock per ms of model time) and an `isfinite` spot check; GSL's error handler is off, so a failed integral is an engine error ("faulted"), never an abort. `Engine::error()` is cleared by the next successful `step()`.

## Synapse models

`synapse_model` may be any NEST connection model; these have extra rules, checked when the circuit loads (errors name the path, for example `projections[2].synapse.weight: ...`). The same rules are implemented in `python/spike/circuit.py`.

| model | rules |
|---|---|
| `gap_junction` | Electrical coupling. Needs `hh_psc_alpha_gap` or `hh_cond_beta_gap_traub` on **both** sides. Instantaneous: `delay` is an error (the 4th element of an edge is ignored). `weight` is a conductance in nS and must be > 0. Always symmetric: with `rule: one_to_one` the loader sets `make_symmetric: true`; `all_to_all` is allowed only within one population (pre = post; autapses default to off); `edges` projections get their mirror edge (post to pre) added by the engine. `set_weight` changes both directions. Other rules are rejected. |
| `tsodyks2_synapse` | Short-term plasticity (depression and facilitation). `U` in [0, 1] (utilisation), `u` in [0, 1], `x` in [0, 1], `tau_rec` > 0 ms, `tau_fac` >= 0 ms (0 = depression only). `weight` is the absolute amplitude scale: a spike gives a PSC of `weight * u * x`. **The first spike of a connection uses the connection's own initial `u` (NEST default 0.5), not `U`; every later spike uses `u = U` (with `tau_fac` = 0).** Set `"u"` equal to `"U"` for a first PSP of `weight * U`. A 20 Hz train onto one cell then shows a falling PSP that recovers over `tau_rec` (`engine/tests/test_synapse_modulation.cpp`). |
| `tsodyks_synapse` | The older Tsodyks-Markram model, same idea with a postsynaptic-current kinetic: `U`, `x`, `y`, `u` in [0, 1], `tau_psc` > 0 ms, `tau_rec` > 0 ms, `tau_fac` >= 0 ms. Prefer `tsodyks2_synapse`; this one is accepted and validated the same way. |
| `quantal_stp_synapse` | Stochastic release with short-term plasticity: `n` release sites (integer >= 1), `a` available sites (integer, 0 to `n`), `U`, `u`, `tau_rec`, `tau_fac` as above. `weight` is the amplitude of one released quantum. |
| `stdp_synapse` | Spike-timing-dependent plasticity, excitatory only: every `weight` must be > 0 (inhibitory STDP is rejected with an error: use `static_synapse` or `tsodyks2_synapse`). `Wmax` defaults to 2 x `weight` (for `edges`: 2 x the largest edge weight); if `weight` is a distribution, `Wmax` must be given. `weight` > `Wmax` is an error. `tau_plus` > 0 ms. |
| `stdp_dopamine_synapse` | Dopamine-gated STDP (reward learning). Same weight and `Wmax` rules as `stdp_synapse` (plus `Wmin` in [0, Wmax], `tau_c`, `tau_n` > 0 ms). The projection needs a `"dopamine_source": "<population id>"` (a projection-level key, next to `pre` and `post`). |

### Dopamine-gated STDP

For each distinct `dopamine_source` population the engine creates one NEST `volume_transmitter`; every spike of that population is a dopamine pulse. The synapse's common properties (`A_plus`, `A_minus`, `Wmin`, `Wmax`, `b`, `tau_c`, `tau_n`, `tau_plus` and the transmitter itself) are set on a private copy of the model named `stdp_dopamine_synapse__p<k>` (k = projection number), because NEST cannot set them per connection. The engine's own DA level (`Engine::set_modulator("DA", conc)`, or released by a `modulator` output) also drives every transmitter: a Poisson generator through a `parrot_neuron` (a device cannot connect to a transmitter) fires at 100 Hz per unit of DA. With no pulses and `b = 0` the weights do not change.

## Neuromodulation

Three engine-level concentration channels, `NE` (noradrenaline), `ACh` (acetylcholine) and `DA` (dopamine), in arbitrary units (0 = baseline; typically 0 to 1). `Engine::set_modulator(name, conc)` sets an external level and `Engine::modulator(name)` reads the total: external level plus the release pools below, as of the end of the last step.

### The `modulation` table

An array that maps a modulator to a parameter of **every neuron of one population** (`pop`) or, since 0.5, to a parameter of **every connection of one projection** (`projection`). It is applied between steps (at the start of `step()`, only when the value changed), never inside one.

```json
"modulation": [
  { "modulator": "NE",  "pop": "SA", "param": "ne",  "gain": 1.0 },
  { "modulator": "ACh", "pop": "SA", "param": "ach", "gain": 1.0 },
  { "modulator": "NE",  "pop": "R",  "param": "I_e", "scale_per_unit": 0.5, "min": 0.0, "max": 800.0 },
  { "modulator": "NE",  "projection": 0, "param": "U",      "gain": 0.4, "min": 0.0, "max": 1.0 },
  { "modulator": "ACh", "projection": 2, "param": "weight", "scale_per_unit": 1.0 }
]
```

| key | meaning |
|---|---|
| `modulator` | `"NE"`, `"ACh"` or `"DA"` |
| `pop` | population id (the whole population): the row modulates a **neuron** parameter |
| `projection` | projection number (0-based, file order: the same number as the `projection` argument of `Engine::set_weight`): the row modulates a **synapse** parameter of every connection of that projection |
| `param` | a numeric NEST parameter of the population's model, or of the projection's synapse model (for `tsodyks2_synapse`: `U`, `u`, `x`, `tau_rec`, `tau_fac`, `weight`, ...); checked at load |
| `gain` | additive: `value = value + gain * conc` |
| `scale_per_unit` | multiplicative: `value = value * (1 + scale_per_unit * conc)` |
| `min`, `max` | optional clamp of the result |

Exactly one of `pop` and `projection` per row, and exactly one of `gain` and `scale_per_unit`. The law, the units (concentration in arbitrary units, 0 = baseline) and the timing are the same for both targets. The starting `value` is the parameter after the population's `params` are applied (neuron row), or each connection's own value after the projection is built, weight spread included (synapse row: the law is applied per connection, so a spread of weights stays a spread). Rows on the same `(pop, param)` or `(projection, param)` combine in file order. Use `gain` for parameters that are the channel themselves (a NESTML cell's `ne` input: base 0, so `value = conc`) and `scale_per_unit` or `gain` on a real parameter such as `I_e`. A modulated parameter is owned by the table: live edits with `set_neuron_param` (neuron row) or `set_weight` (synapse row on `weight`) are overwritten the next time the level changes.

Synapse rows, in detail:
- **Aplysia-style sensitisation.** On a `tsodyks2_synapse` projection, `{ "modulator": "NE", "projection": 0, "param": "U", "gain": 0.4, "min": 0, "max": 1 }` raises the release probability with the modulator level, so the same presynaptic train gives larger PSPs (with `tau_fac` = 0, `U` acts from the second spike of a connection on; give the first spike the same efficacy with `"u"` = `"U"` in the synapse). `param: "weight"` with `scale_per_unit` scales the amplitude instead. Both are tested in `engine/tests/test_synapse_modulation.cpp` (PSP x3 when `U` goes 0.2 to 0.6).
- A modulated `weight` is clamped to the postsynaptic model's weight ceiling (see "Weight limits").
- Not allowed (error at load): a `gap_junction` projection; `weight` of `stdp_synapse` / `stdp_dopamine_synapse` (the plasticity rule owns it); `delay`, `synapse_model`, `port`, `source`, `target`; an unknown parameter name; a projection index out of range. Projections that share pre population, post population and synapse model are allowed: each gets its own copy of the synapse model, so queries, `set_weight` and modulation address one projection at a time.
- A synapse row on a projection with no connections does nothing (and cannot check `param`).
- The Python package (`python/spike/circuit.py`) applies synapse rows the same way (`Network.set_modulator`).

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

## Spike-train inputs (0.5)

| `kind` | NEST device | keys |
|---|---|---|
| `spike_generator` | `spike_generator` | `spike_times`: array of times in ms (optional, default none) |
| `sinusoidal_poisson` | `sinusoidal_poisson_generator` | `rate` (mean, Hz, default 0), `amplitude` (Hz, default 0), `freq_hz` (Hz, default 0), `phase` (degrees, as NEST, default 0) |

Both connect to `targets` with `weight` (and `delay`, default the resolution) like the other inputs. `value` is not used by these kinds (giving it is an error): a `spike_generator` has no scalar, and `sinusoidal_poisson` uses `rate`. Keys of one kind on another kind are errors.

**spike_generator.** `spike_times` are **absolute model times in ms**, on the resolution grid (each time must be a multiple of `resolution_ms`; within 1e-6 steps it is snapped to the grid, otherwise the loader refuses it with `inputs[i].spike_times[k]: ... not a multiple of resolution_ms`), strictly greater than 0, sorted ascending. Equal times are allowed (two spikes in one step). A spike at time `t` is emitted by the device in the step that ends at `t` and arrives at the target at `t + delay`. The list may be empty or absent: a task or the game queues spikes at run time with `Engine::queue_spikes(channel, times_ms)` (GDScript `SpikeSim.queue_spikes(name, PackedFloat64Array)`): times are absolute model time (`stats().t_ms` is the clock), need not be sorted, are added to the spikes already queued, and must be a multiple of the resolution and strictly after the time reached (otherwise the call returns false, `error()` says why and nothing is queued). They reach NEST at the start of the next `step()`. `set_input()` on a `spike_generator` is an error (a spike train has no single value). `spike_times` and `queue_spikes` are the only task-settable parts.

**sinusoidal_poisson.** The rate is `max(0, rate + amplitude * sin(2 pi freq_hz t + phase))` in Hz, with `t` the absolute model time; each target gets its own independent train (`individual_spike_trains`, like `poisson`). `freq_hz` must be below the Nyquist limit of the resolution (`500 / resolution_ms` Hz). Between steps a task can change the parameters: `Engine::set_input(channel, value)` sets the mean `rate`, and `Engine::set_input_param(channel, key, value)` sets `rate`, `amplitude`, `freq_hz` or `phase` (degrees) (GDScript `SpikeSim.set_input_param`). NEST's sinusoidal generator derives its oscillator state from amplitude, frequency and phase when a run is prepared, and only `rate` is read live, so the engine re-prepares NEST before the next step when `amplitude`, `freq_hz` or `phase` changed (the same mechanism the Poisson rate uses). The phase follows the absolute clock, so a change of `freq_hz` keeps `phase` as the phase at t = 0, not at the moment of the change.

`python/spike/circuit.py` builds both kinds (`spike_generator`, `sinusoidal_poisson_generator`; the Theory wing's `rate_drive` and `tuned_poisson` inputs and its rate populations are not supported by the package yet and are refused with a plain error) and `Network.set_input(name, value)` sets the mean rate; for `spike_generator` it raises (use `Network.queue_spikes`).

## Population-vector output (0.5)

```json
{ "name": "heading", "kind": "population_vector", "neurons": [{ "pop": "EPG" }] }
```

A ring readout (the fly compass, a head-direction ring): `neurons` is an **ordered** list, and neuron `k` of `N` has preferred angle `2 pi k / N` (evenly spaced on the ring, k = 0 at angle 0, increasing). Unlike `rate`, the list is not sorted: the order the selectors give is the ring order (`[{ "pop": "EPG", "idx": [7,6,5,4,3,2,1,0] }]` is the mirror ring), and repeating a neuron is an error. Other keys (`modulator`, `gain`, `tau_ms`) are errors for this kind.

After each `step()`, from the spike counts `c_k` of that step: the vector `V = sum_k c_k (cos a_k, sin a_k)`. `Engine::output_angle(name)` is the bump angle `atan2(V_y, V_x)` in radians in [0, 2 pi) and `Engine::output_strength(name)` is `|V| / sum_k c_k` in [0, 1]: 1 when every spike is at one angle, about 0.79 for a half-rectified cosine bump, 0 for a flat or cancelling profile. With no spike in the step both are 0 (no direction). Both are 0 for other output kinds and unknown names. The output also counts spikes like a `rate` output (`output_rate`, `output_count`). GDScript: `SpikeSim.output_angle(name)`, `SpikeSim.output_strength(name)`. The window is the last `step()`, so a task chooses it (steps of 50 to 100 ms smooth the Poisson noise of a bump). Tests: `engine/tests/test_population_vector.cpp`.

## Rate neurons, mean-field projections and tuned inputs (0.6, the Theory wing)

All additions are engine-only (the Python package does not build these yet).

* **Rate populations.** `model` may be a NEST rate model: `lin_rate_ipn`, `tanh_rate_ipn`, `sigmoid_rate_gg_1998_ipn`, `threshold_lin_rate_ipn`, `gauss_rate_ipn` (and the `_opn` forms), or `siegert_neuron`. `params` are NEST's (`tau`, `mu`, `sigma`, `g`, ...; siegert: `tau`, `tau_m`, `t_ref`, `theta`, `V_reset`, `mean`). Rate neurons do not spike: they are not connected to the spike recorder, they never show in `drain_spikes()`, and a `rate` output over them reads 0. They are joined by `rate_connection_instantaneous` (no `delay`) or `rate_connection_delayed` (`delay` required), with a `weight`; any other synapse model between rate neurons, and any rate synapse between spiking ones, is a load error. Use `"allow_autapses": true` for a unit that excites itself.
* **`diffusion_connection`** (siegert_neuron to siegert_neuron only, a `rule` projection): `weight` is `J`, the postsynaptic amplitude in mV of the spiking network the unit stands for (negative: inhibition); `K` (default 1) is the number of such inputs per cell. The loader sets NEST's `drift_factor = tau_m K J / 1000` and `diffusion_factor = tau_m K J^2 / 1000` with `tau_m` (ms) of the target (default 10), so rates are in spikes/s. No `delay`. `Engine::set_weight` changes `J`. A driving unit with no input of its own is a `siegert_neuron` with `mean` = the external rate times the number of external inputs (1/s).
* **Input kind `rate_drive`** (targets: rate populations only): `value * weight` is written to each target's `mu` (rate neurons) or `mean` (siegert) before the next step. `set_input` changes it. Spiking inputs on rate populations are load errors.
* **Output kind `rate_value`** (neurons: rate populations only): `Engine::output_value(name)` is the mean `rate` of its neurons after the last step.
* **Input kind `tuned_poisson`**: `targets` are the afferent cells in tuning order. Target `k` of `N` has preferred angle `period_deg * k / N`; the stimulus angle (`value`, degrees, set with `set_input`) drives it with a Poisson train of rate `rate_base + (rate_max - rate_base) * exp(kappa * (cos(2 pi (angle - pref) / period_deg) - 1))` Hz (von Mises). Keys: `rate_max` (Hz, default 50), `rate_base` (Hz, default 0), `kappa` (default 2), `period_deg` (default 360; 180 for an orientation). `weight` and `delay` as for the other inputs. One generator per target (independent trains).
* **Engine reads**: `Engine::neuron_state(neuron, key)` (any numeric NEST status entry; NaN if unknown), `Engine::input_weight(channel)`.
* **`spike-run --sample`** (development aid): `spike-run circuit.json 1000 --sample V_m@0,w@0,out:rate,cnt:motor --every 10 --set 300:cur=5` prints `{"t":..,"V_m@0":..}` lines every 10 ms and sets an input at a model time.

## Parts bin (catalog)

`engine/include/spike/catalog.h` lists the cell models the game offers as cards (description, biological examples, key parameters with unit and range, default input weight, E/I suitability, thumbnail drive). `build/engine/spike-catalog` measures every card in its own process (one engine per process) and writes `catalog/thumbs.json`: for each card the f-I points (`fi`: rate at each drive level) and a V_m `trace` at the level nearest 20 Hz. Each card declares its window in the `probe` block of `catalog_json()` (`run_ms`, `skip_ms`, `trace_ms`, `trace_dt_ms`; default 1.2 s run, first 0.2 s discarded). Slow oscillators are measured over **at least 10 cycles**: a level is extended in `run_ms` chunks until more than `min_cycles` (10) spikes were seen or `max_ms` is reached, and the rate is then `(n-1)/(t_last-t_first)`. So the SA node card (one beat per cycle) runs 12 s, extended up to 60 s; the Butera burster runs 60 s (its rate is spikes per second, many per burst); the ICC card (a wave every ~20 s) runs 60 s, extended up to 6 min. The card's `vm_init` is the seeded initial V_m range the engine uses for the model (null = the default `[E_L, V_th]`).

Every card has a measured default input `input_weight` with a `weight_unit`: the weight at which one presynaptic spike gives a unitary EPSP of about 1-2 mV at rest (measured with a single `spike_generator` spike; Izhikevich presets 2 mV: NEST steps `V_m` by exactly the weight; `hh_cond_exp_traub` 2 nS; `ht_neuron` 1 on the AMPA port; `sa_node_pacemaker` 0.2 nS; `butera_burster` 0.05 nS; `icc_slow_wave` 200 pA). The parrot relay is unit-free. Use this one table for default weights in the editor. Cards for the NESTML cells are marked `available: false` until the module is compiled into the build.

## Structure (v0.4)

`Engine::structure()` reports populations (id, model, size, first neuron index, numeric params, E/I `sign` from the summed outgoing weights), projections (pre and post population, rule or `edges`, synapse model, connection count, mean/min/max weight, sign of the mean weight, in-degree mean and max) and channels (the populations each input targets and each output reads, with the channel `kind`: inputs `dc`, `poisson`, `spike_generator`, `sinusoidal_poisson`; outputs `rate`, `modulator`, `population_vector`; a `population_vector` lists its neurons in ring order), read from the live NEST connections. `has_path`, `has_loop` and `has_loop_via` answer reachability and feedback-loop questions on that graph; projections with zero connections or a zero mean weight are not edges. API and semantics: `docs/ENGINE-STRUCTURE.md`.
