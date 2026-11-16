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
- **Initial membrane potential.** If a population's `params` has no `V_m`, and the model has `V_m`, `E_L` and `V_th` with `V_th > E_L` (after `params` are applied), each neuron gets a seeded random `V_m = E_L + u * (V_th - E_L)`, with `u` in [0, 1). This breaks symmetry between identical cells (for example a half-centre). `u` comes from splitmix64 keyed by the circuit `seed` and the circuit-level neuron index `i`, independent of NEST's RNG: `z = seed + (i+1) * 0x9E3779B97F4A7C15 (mod 2^64)`; `z = (z ^ z>>30) * 0xBF58476D1CE4E5B9`; `z = (z ^ z>>27) * 0x94D049BB133111EB`; `z ^= z>>31`; `u = (z >> 11) / 2^53`. The C++ engine and `python/spike/circuit.py` implement exactly this. Set `V_m` explicitly (for example to the resting potential) to disable it; the reference circuits and the `iaf_dc` golden do so where timing must be analytic.
- **Top-level keys.** `schema_version`, `name`, `resolution_ms`, `seed`, `populations`, `projections`, `inputs`, `outputs`, `modulation`, `record`, `meta`.
- **`pos`** holds editor coordinates, one `[x, y]` per neuron. It is optional and ignored by simulators.
- **Determinism.** `seed` sets NEST's `rng_seed`. The same file and the same seed give the same spikes for the same NEST version and thread count.
- **Unknown keys** under `meta` are preserved. Unknown top-level keys are an error.

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

`engine/include/spike/catalog.h` lists the cell models the game offers as cards (description, biological examples, key parameters with unit and range, default input weight, E/I suitability, thumbnail drive). `build/engine/spike-catalog` measures every card in its own process (one engine per process) and writes `catalog/thumbs.json`: for each card the f-I points (`fi`: rate over the last 1 s of a 1.2 s run at each drive level) and a 300 ms V_m `trace` at the level nearest 20 Hz. Cards for the NESTML cells are marked `available: false` until the module is compiled into the build.
