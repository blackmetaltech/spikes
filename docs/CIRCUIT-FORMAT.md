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
- **Outputs** are rate readouts over a set of neurons.
- **`pos`** holds editor coordinates, one `[x, y]` per neuron. It is optional and ignored by simulators.
- **Determinism.** `seed` sets NEST's `rng_seed`. The same file and the same seed give the same spikes for the same NEST version and thread count.
- **Unknown keys** under `meta` are preserved. Unknown top-level keys are an error.
