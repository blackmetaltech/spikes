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
- **Outputs** are rate readouts over a set of neurons.
- **Initial membrane potential.** If a population's `params` has no `V_m`, and the model has `V_m`, `E_L` and `V_th` with `V_th > E_L` (after `params` are applied), each neuron gets a seeded random `V_m = E_L + u * (V_th - E_L)`, with `u` in [0, 1). This breaks symmetry between identical cells (for example a half-centre). `u` comes from splitmix64 keyed by the circuit `seed` and the circuit-level neuron index `i`, independent of NEST's RNG: `z = seed + (i+1) * 0x9E3779B97F4A7C15 (mod 2^64)`; `z = (z ^ z>>30) * 0xBF58476D1CE4E5B9`; `z = (z ^ z>>27) * 0x94D049BB133111EB`; `z ^= z>>31`; `u = (z >> 11) / 2^53`. The C++ engine and `python/spike/circuit.py` implement exactly this. Set `V_m` explicitly (for example to the resting potential) to disable it; the reference circuits and the `iaf_dc` golden do so where timing must be analytic.
- **`pos`** holds editor coordinates, one `[x, y]` per neuron. It is optional and ignored by simulators.
- **Determinism.** `seed` sets NEST's `rng_seed`. The same file and the same seed give the same spikes for the same NEST version and thread count.
- **Unknown keys** under `meta` are preserved. Unknown top-level keys are an error.
