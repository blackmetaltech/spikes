---
title: A circuit is data
section: Under the hood
order: 80
summary: Every circuit you build is one JSON file that the game, the engine and Python all read; this page explains each field with a small example you can run.
---

# A circuit is data

Everything you build in the [lab](how-to-read-the-lab.html) is saved as one plain text file in JSON format. JSON is a text format made of named fields, lists and numbers that any editor can open. The game, the C++ engine, the [command line](spike-run.html) and the [Python package](python.html) all read the same file. A circuit you build in the game runs unchanged in PyNEST (the Python interface to the NEST simulator), and a parity test checks that the spikes are the same.

This page describes format version 1 (`"schema_version": 1`). You do not need it to play. You need it if you want to edit a circuit by hand, generate circuits from a script, or share one.

> **Note:** All NEST model names and parameter names pass straight through. A cell called `iaf_psc_alpha` in the file is NEST's `iaf_psc_alpha`, and `V_th` means NEST's `V_th`. Units are NEST's own: millivolts (mV), milliseconds (ms), picoamperes (pA), nanosiemens (nS) and hertz (Hz).

## Where the game keeps your circuits

In the lab, the More menu has Save and Load once the campaign reaches the Spinal cord chapter (the first chapter after Invertebrates). They open a file dialog that starts in the `circuits` folder inside the game's user data folder (`user://circuits` in Godot terms). The sandbox always has Save and Load. A diagnosis case is read-only, so it has neither.

The reference circuits of the levels live in `godot/circuits` in the source tree, and in a loose `circuits` folder next to the game in the release zip. The game hides the reference of a level until you pass it (or you play in Guided mode); see [modes](modes.html).

## A small example

This is a two-cell relay. A constant current (a "tap") drives cell A. Every spike of A is passed to cell B through a strong excitatory connection with a 4 ms delay. The output reads B. Save it as `relay.json` and run it with `spike-run relay.json 100`; it loads and runs (cell A fires about every 12 ms, and B answers about 7 ms after each A spike).

```json
{
  "schema_version": 1,
  "name": "Relay with a delay",
  "resolution_ms": 0.1,
  "seed": 1,
  "populations": [
    { "id": "A", "model": "iaf_psc_alpha", "n": 1, "params": {},
      "role": "neuron", "pos": [[120, 160]] },
    { "id": "B", "model": "iaf_psc_alpha", "n": 1, "params": {},
      "role": "neuron", "pos": [[320, 160]] }
  ],
  "projections": [
    { "pre": "A", "post": "B", "rule": "all_to_all",
      "synapse": { "synapse_model": "static_synapse",
                   "weight": 1500.0, "delay": 4.0 } }
  ],
  "inputs": [
    { "name": "tap", "kind": "dc",
      "targets": [{ "pop": "A" }], "weight": 300.0, "value": 2.0 }
  ],
  "outputs": [
    { "name": "echo", "kind": "rate", "neurons": [{ "pop": "B" }] }
  ],
  "record": { "spikes": "all" },
  "meta": { "author": "me", "notes": "a first circuit" }
}
```

Read it top to bottom:

| Field | What it means |
|---|---|
| `schema_version` | Always `1` for now. Any other value is refused. |
| `name` | A title for humans. |
| `resolution_ms` | The simulation step in ms. The game uses 0.1 ms. Spike times and delays sit on this grid. |
| `seed` | The random seed. The same file with the same seed gives the same spikes on the same NEST build and thread count. |
| `populations` | The groups of cells. Here two groups of one cell each. |
| `projections` | The connections between groups (the game calls them axons). |
| `inputs` | The sensors: devices that push current or spikes into cells. |
| `outputs` | The motors: read-outs over a set of cells. |
| `record` | What to record. |
| `meta` | Free notes for humans. Unknown keys inside `meta` are kept. |

> **Warning:** Unknown keys anywhere else are an error. The engine says, for example, `$.bogus: unknown key`. A typo in a field name fails loudly instead of being ignored.

## Populations

A population is a group of identical cells, written as one entry of `populations`.

| Key | Meaning |
|---|---|
| `id` | A name, unique in the file. Projections and channels refer to it. |
| `model` | Any NEST neuron model name, or one of the custom cells (see [the parts bin](parts-bin.html) and [make it yours](make-it-yours.html)). |
| `n` | How many cells. |
| `params` | NEST parameters that differ from the model's defaults, for example `{"I_e": 380.0}` (a constant current in pA). Empty means all defaults. |
| `pos` | Editor coordinates, one `[x, y]` pair per cell. Optional. Simulators ignore it. |
| `role` | An editor label. The files in the game use `neuron`, `sensor` and `motor`. Optional. |

Cells are numbered from 0 over all populations in file order. Cell 0 of the second population has the index equal to the size of the first. Inside a population, `idx` counts from 0 again.

**Starting voltage.** If you give no `V_m` (membrane voltage) in `params`, each cell starts at a random voltage between its resting value `E_L` and its threshold `V_th`. The random numbers come from the circuit `seed`, not NEST's generator. This breaks the symmetry between identical cells; without it two copies of a half-centre would fire in lock-step. A few custom cells have their own range (the sinoatrial pacemaker uses -74 to -45 mV). Set `V_m` yourself to start at rest.

## Projections

A projection connects a source group to a target group. It has `pre` and `post` (population ids) and exactly one way to say who connects to whom.

**By rule.** `"rule"` is any NEST connection rule, with its own parameters next to it:

| Rule | Meaning |
|---|---|
| `all_to_all` | Every source cell to every target cell. |
| `one_to_one` | Cell k to cell k. The groups must be the same size. |
| `fixed_indegree` | Each target cell listens to `indegree` source cells picked at random. |
| `fixed_outdegree`, `pairwise_bernoulli` and others | As in NEST, with `outdegree`, `p` and so on. |

**By edges.** `"edges": [[pre_idx, post_idx, weight, delay], ...]` lists every connection. Use it when you need exact control.

The `synapse` object is a NEST synapse specification:

| Key | Meaning |
|---|---|
| `synapse_model` | Defaults to `static_synapse`, which never changes. Others are covered in [synapses and modulators](synapses-and-modulators.html). |
| `weight` | A positive weight excites; a negative weight inhibits. Units are pA for current-based cells and nS for conductance-based cells. |
| `delay` | Ms. Must be at least the resolution. |

**Weight limits.** To keep NEST's integrator stable, a synapse onto a conductance-based cell (a model name containing `_cond_`) must have `|weight| <= 500` nS, and onto a current-based cell (`_psc_` or `_curr_`) `|weight| <= 10000` pA. Reference circuits stay far below this: under 60 nS and 1500 pA. The loader names the exact path of an offending edge.

**Weight spread.** Two optional projection-level keys give every connection its own weight: `weight_sd` (add a normal random amount, in the weight's unit) and `weight_cv` (multiply by one plus a relative spread). The random numbers depend only on `seed`, the projection number and the connection index, so the result is reproducible. Give only one of them. The game's lab shows this as the "Spread" control on an axon, and Test x5 draws it again for every seed. Gap junctions and plastic synapses cannot use spread on a `rule` projection; use `edges` there.

## Inputs (sensors)

Each input creates one NEST device and connects it to its `targets` with `weight`.

| `kind` | Device | What `value` means | Extra keys |
|---|---|---|---|
| `dc` | constant current | A level. The current delivered is level times weight, in pA. | none |
| `poisson` | random spike train | Rate in Hz. Each target gets its own independent train. The weight is the size of one spike's kick. | none |
| `spike_generator` | spikes at listed times | Not used. | `spike_times`, absolute times in ms on the resolution grid |
| `sinusoidal_poisson` | rate that swings up and down | Not used. | `rate`, `amplitude` (Hz), `freq_hz`, `phase` (degrees) |
| `rate_drive` | drive for rate units | Level times weight is written to the unit's drive. | Theory wing only |
| `tuned_poisson` | stimulus angle drives tuned cells | The stimulus angle in degrees. | `rate_max`, `rate_base`, `kappa`, `period_deg` |

`targets` can be a list of population ids, or objects such as `{"pop": "A", "idx": [0, 2]}` to pick cells. An empty list is allowed: the device exists and connects to nothing. That is how the lab lets you run a half-wired circuit. A task sets sensor levels while it runs; you only choose where the sensor is wired and with what weight.

## Outputs (motors)

An output reads a set of cells. `neurons` uses the same selectors as `targets`. An empty list always reads 0.

| `kind` | Reads |
|---|---|
| `rate` | Spike counts (the default). The level reports a rate in Hz per cell. |
| `modulator` | Spikes release a neuromodulator (`modulator`: NE, ACh or DA; `gain` per spike, `tau_ms` decay). |
| `population_vector` | A ring readout: the list order is the ring order. Gives an angle and a strength. |
| `rate_value` | The mean rate of rate units (Theory wing). |

See [inputs and outputs](inputs-and-outputs.html) for how each one behaves in the lab.

## Modulation

An optional top-level `modulation` list lets a neuromodulator change a parameter of every cell in a population (`pop`) or of every connection in a projection (`projection`, counted from 0 in file order). Each row names the `modulator` (`NE`, `ACh` or `DA`), the `param`, and exactly one of `gain` (add gain times level) or `scale_per_unit` (multiply by one plus the amount times level), with optional `min` and `max`. It is applied between steps, never inside one. The game's Aplysia levels use this to make serotonin strengthen a synapse.

The custom cells with `ne` and `ach` parameters follow the circuit-wide levels without any table, with gain 1.

## Record

`"record": {"spikes": "all", "voltage": [{"pop": "A", "idx": [0]}]}`. Spikes are recorded for all cells. `voltage` lists cells whose membrane voltage you want.

## Rules the loader enforces

- The format has exactly these top-level keys: `schema_version`, `name`, `resolution_ms`, `seed`, `populations`, `projections`, `inputs`, `outputs`, `modulation`, `record` and `meta`.
- A projection has `rule` or `edges`, not both.
- A gap junction (electrical coupling) needs gap-junction cell models on both sides, has no delay, and is always symmetric.
- A plastic `stdp_synapse` is excitatory only.
- Rate units connect with rate connections, spiking cells with spiking synapses; mixing them is an error.
- Errors name the path, such as `projections[0].edges[3]: weight ... outside the allowed range`.

## Determinism and parity

The engine and PyNEST give the same spikes, step for step, when they use the same NEST commit, the same thread count and the same file. The Theory wing's rate units, mean-field links and tuned inputs are engine-only; the Python package does not build them yet. See [Use your circuit in Python](python.html).

> **Note:** A circuit file does not carry a "this is the reference" flag. The game decides whether your bench is a copy of a level's reference by comparing the structure and numbers with the reference file. A circuit you type from scratch with round numbers is never flagged. See [stars, par and the hint ladder](stars-and-hints.html).

## Related pages

[Use your circuit in Python](python.html), [the spike-run command line](spike-run.html), [architecture](architecture.html), [make it yours](make-it-yours.html).
