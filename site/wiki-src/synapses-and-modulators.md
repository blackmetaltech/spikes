---
title: Synapses and modulators
section: Reference
order: 51
summary: Static, depressing, plastic and dopamine-gated synapses, gap junctions, rate and mean-field connections, wiring modes, and the NE, ACh and DA neuromodulators.
---

# Synapses and modulators

A **synapse** is a connection from one cell to another. The lab draws it as an axon: a line from one group of neurons to another. This page covers what an axon can be, what each setting means, and how neuromodulators change a circuit from outside.

The first mention of a new word on each page explains it in one line. For the cells see [the parts bin](parts-bin.html). For the on-screen controls see [how to read the lab](how-to-read-the-lab.html).

## The basics of an axon

To connect, use **Connect** mode: click a source group, then a target group. Or drag from the rim of a neuron disc onto another disc. Click the axon to open its inspector. The editor keeps one axon per ordered pair of groups, so a second drag between the same two groups selects the existing one.

| Setting | What it does | Range in the lab |
|---|---|---|
| **Weight** | How strong each connection is. The unit depends on the target cell (pA, nS or mV, see below) | 0 up to about 20 typical synapses of the target model |
| **E / I** | Flips the sign of the weight. Positive weights excite, negative weights inhibit | |
| **Delay** | How long a spike takes to arrive | 0.1 to 250 ms |
| **Spread** | Gives every synapse of this axon its own random weight around the value above | 0 to 100 % |
| **Delete** | Removes the axon | |

**Excitatory vs inhibitory.** An excitatory synapse pushes the target toward threshold. An inhibitory one pushes it away. In the circuit file the sign of the weight is the whole difference. A neuron group is sienna (excitatory) or Prussian blue (inhibitory) by the sign of the weights it sends out. The **E / I** button on a neuron flips the sign of every axon that leaves it.

**Weight.** The weight slider is square-law: fine steps near zero, coarse steps at the top. You can also click the number beside the slider and type a value. Typed values are clamped to the slider range. The target cell sets the range: twenty times its default input weight, and at least twice the largest weight the level's own reference circuit needs. If a level wants more strength than a slider offers, the verdict tells you to use more cells or another route.

**Weight units by target cell.** A synapse onto a current-based cell is in picoamps (pA). Onto a conductance-based cell it is in nanosiemens (nS). Onto a cell with delta synapses it is a voltage step in millivolts (mV). The Izhikevich cells also take a voltage step in mV. The [parts bin](parts-bin.html) lists the unit of every cell.

**Hard limits.** The engine refuses a circuit whose weights or sizes could destabilise the simulator or freeze the game. The sliders keep you well inside most of these, but a pasted or imported circuit file can break them. The limits:

- A weight above 500 nS (onto a cell with `_cond_` in its model name) or above 10000 pA (onto a `_psc_` cell) is refused. The four custom NESTML cells take at most 2000 nS. The Izhikevich, rate and mean-field cells have no weight limit of this kind.
- A gap junction is at most 100 nS.
- A delay in a circuit file is at most 1000 ms (the lab's slider stops at 250 ms).
- At most 20000 neurons in total and about 2 million connections.
- A Poisson, sinusoidal or tuned input is at most 100000 Hz. A constant current is at most 100000 pA (2000 pA on a custom NESTML cell, 1000 pA for a `dc` input onto one).

A refused circuit does not run. The lab shows a banner "The engine rejected the circuit:" and the reason, which names the part at fault, for example the number of the connection and the allowed range. A cell can still blow up numerically inside these limits (a huge weight onto a tiny capacitance, say). The engine then stops the run and says "numerical instability". Reload the circuit to continue. The exact format rules are in [a circuit is data](circuit-is-data.html).

**Delay.** A new axon starts at 2 ms. The delay slider is square-law too. One to three milliseconds fits a single synapse. A hundred or two hundred milliseconds stands for a whole chain of cells you do not draw, which some burst generators need.

**Spread.** With spread above 0, each synapse of the axon gets its own weight, drawn from a normal distribution whose standard deviation is that percentage of the weight. The circuit seed fixes the draw, so the same circuit always gets the same weights. **Test x5** draws it again for every seed. A circuit that only works with identical weights fails here. The lab uses this to ask whether your design is robust.

## Synapse types at a glance

| Type | NEST model | What it does | Where you meet it |
|---|---|---|---|
| Static | `static_synapse` | The same strength for every spike | Everywhere. The default |
| Depressing | `tsodyks2_synapse` | Gets weaker with use, recovers slowly | The Aplysia chapter |
| Plastic (STDP) | `stdp_synapse` | Learns: strengthens when the sender fires just before the receiver | **Writing a memory with plasticity** |
| Dopamine-gated | `stdp_dopamine_synapse` | Weakens when its input was active and a dopamine source fires | The fly mushroom body chapter |
| Gap junction | `gap_junction` | Electrical coupling that synchronises two cells | Sandbox and your own designs |
| Rate connection | `rate_connection_instantaneous`, `rate_connection_delayed` | The gain of one rate unit onto another | The Rate models chapter |
| Mean-field connection | `diffusion_connection` | J and K of the spiking network a Siegert unit stands for | **The network and its mean field** |

The synapse type stepper appears in the axon inspector only where the chapter uses it: the Aplysia levels (static or depressing), the fly mushroom body levels (static or dopamine-gated), and **Writing a memory with plasticity** (static or plastic). The sandbox and your own files can use any NEST connection model through the circuit file, see [a circuit is data](circuit-is-data.html). The file also offers `tsodyks_synapse` (the older kinetic form; prefer the `tsodyks2` model) and `quantal_stp_synapse` (stochastic release with short-term plasticity).

## Static synapses

A static synapse keeps its weight. Every spike gives the target the same push. The delay and the target cell's synaptic time constant set its timing (see `tau_syn_ex` and `tau_syn_in` in the parts bin). Alpha and exponential shapes both rise or jump quickly, then fade over a few milliseconds.

## Depressing and facilitating synapses (`tsodyks2_synapse`)

**Short-term plasticity** is a change in a synapse's strength over milliseconds to seconds, caused by its own recent use. A synapse used hard runs short of transmitter and releases less each time. That is **depression**. It refills slowly. Some synapses do the opposite: use leaves calcium behind, and the next spike releases more. That is **facilitation**.

The model is Tsodyks-Markram. In the lab, a spike gives a current of `weight * u * x`, where `x` is the share of transmitter still available (starts at 1) and `u` is the share released by each spike.

| Setting | Unit | Range in the lab | Meaning |
|---|---|---|---|
| **U** | none, 0 to 1 | 0.01 to 1 | The share of the available transmitter released by each spike. Larger means deeper depression |
| **Recovery** (`tau_rec`) | ms | 50 to 5000 | How long the synapse takes to refill |

Switching a synapse to depressing in the lab sets `x` to 1, `tau_rec` to 1000 ms and `tau_fac` to 0 (depression only). A non-zero `tau_fac` adds facilitation in files you write yourself.

> **Note:** NEST starts the first spike of a connection from `u` = 0.5, not from `U`. Later spikes use `U` when `tau_fac` is 0. The lab sets `u` equal to `U` for you, so the first spike is as strong as it should be. In your own circuit file, set `"u"` equal to `"U"`.

Depression is how the Aplysia chapter shows habituation. The sensory neurons keep firing at the same rate, but the connection to the motor neuron weakens with each touch. See [campaign-bestiary](campaign-bestiary.html).

## Plastic synapses (`stdp_synapse`)

**Spike-timing-dependent plasticity (STDP)** lets a connection learn from the order of spikes. If the sender fires just *before* the receiver, the connection strengthens. If it fires just *after*, the connection weakens. This is Hebbian learning ("cells that fire together wire together"), with the arrow of time added.

In **Writing a memory with plasticity** you can make a connection among the pattern's cells plastic. The settings:

| Setting | Meaning | Lab range | Starting value |
|---|---|---|---|
| **Learning rate** (`lambda`) | How much one spike pair changes the weight | 0.0001 to 0.03 | 0.0015 |
| **Weakening** (`alpha`) | How much stronger a late pairing weakens than an early one strengthens | 0.1 to 3 | 0.5 |
| **Ceiling** (`Wmax`) | The weight a connection can never exceed | 10 to 3000 pA | the larger of ten times the weight and 100 pA |
| `tau_plus` | Time window of the pairing | file only | 20 ms |

Only excitatory connections can be plastic. NEST's `stdp_synapse` is excitatory only, and the circuit loader rejects an inhibitory STDP synapse. So the inhibition between two patterns in the memory levels is built in, not learned.

## Dopamine-gated synapses (`stdp_dopamine_synapse`)

A **three-factor rule** changes a synapse only when three things line up: the sender was active, the receiver was active (or just was driven), and a **dopamine** signal arrives afterwards. It is how a reward or a punishment can teach a circuit.

The fly mushroom body chapter uses the synapse as a depression. When the odour input was active and the dopamine source fires, the synapse loses strength. That is how pairing an odour with punishment teaches the fly to avoid it. The settings:

| Setting | Meaning | Lab range |
|---|---|---|
| **Dopamine source** | A group of cells whose spikes release dopamine onto this synapse | any group of the circuit |
| **Learning** | How much one pairing takes away. The lab sets the signs so that the synapse depresses | 0 to 3000 |
| **Trace** (`tau_c`) | How long a synapse stays eligible after its input fired | 5 to 500 ms (30 ms at first) |
| **Max weight** | The largest strength it can have | 1 to 10000 |

Behind the scenes, the engine creates one NEST `volume_transmitter` for each dopamine source and treats every spike of that group as a dopamine pulse. It also sets a private copy of the synapse model for each projection, because NEST cannot set these properties per connection. The engine's own DA level drives every transmitter too, at 100 Hz per unit of DA.

> **Note:** The real mushroom-body rule needs no spike in the output cell. NEST's rule looks at the output cell's own spikes, so the odours must drive those cells during the pairing. This is a game choice. See [what is real](what-is-real.html).

## Gap junctions

A **gap junction** is an electrical synapse: a pore that joins two cells so their voltages pull together. It is instant (no delay) and works both ways. It is a quick way to synchronise a cluster of pacemaker cells.

Only two cell models accept them: `hh_psc_alpha_gap` and `hh_cond_beta_gap_traub`. The rules, enforced when a circuit loads:

- Both sides must be one of those two models.
- The weight is a conductance in nS, above 0 and at most 100.
- The delay is not allowed.
- It is always symmetric. With `one_to_one` wiring the loader makes it symmetric. With `all_to_all` it is allowed only within one population. An `edges` list gets its mirror edge added.
- The neuromodulator table cannot modulate a gap junction.

> **Note:** The heart levels do not use gap junctions. They use excitatory synapses between the pacemaker cells as a stand-in for electrical coupling, and say so. The crab chapter also approximates the electrical coupling between AB and PD cells with fast reciprocal excitation.

## Rate connections

Rate units have no spikes. **Rate connections** join them. The weight is the **gain** of one unit onto another, and a negative weight inhibits. Rates are in arbitrary units. There are two flavours:

- `rate_connection_instantaneous`: no delay. The lab creates this when you connect two rate units.
- `rate_connection_delayed`: a delay is required. Available in circuit files.

A new rate connection has weight 1 (excitatory) or -1 (inhibitory). A unit that excites itself needs `allow_autapses`, which the lab sets for a self-connection.

The loader rejects any other synapse type between rate units, and a rate connection between spiking cells. The axon inspector of a rate connection shows the weight but not the delay or the spread.

## Mean-field connections (`diffusion_connection`)

The **mean-field** idea: in a large network each cell hears so many weak, independent inputs that you can describe the whole population by its average rate. A Siegert unit applies a formula to the mean and the variance of the input and gives the population rate.

A `diffusion_connection` joins one Siegert unit to another. Its weight is **J**: the size of one input's voltage step in mV, in the spiking network the unit stands for (negative for inhibition). A second number, **K**, is how many such inputs each cell gets. The lab shows K as a slider from 1 to 200 with the unit "per cell". A new one has weight 0.1 mV (excitatory) or -0.1 mV (inhibitory).

The engine turns J and K into NEST's drift and diffusion factors: the mean input grows with `J * K * rate` and its variance with `J^2 * K * rate`. The result is in spikes per second. Delay and spread do not apply.

## Wiring modes: which cells does an axon connect?

When both groups have more than one neuron, the axon inspector shows a **wiring** stepper.

| Mode | What it connects | Where it is offered |
|---|---|---|
| **Every pair** (`all`) | Every cell of the sender to every cell of the target | Always |
| **Neuron k to k** (`segment`) | Neuron k of the sender to neuron k of the target, one synapse per segment | Always |
| **Random few inputs** (`random`) | Each target cell listens to just a fixed number of sender cells, picked at random. A fresh draw for every circuit seed | Fly mushroom body, Rate models and Attractor memory chapters |

**Segment wiring** also has a **shift** stepper (-2 to +2). Neuron k of the sender connects to neuron k plus the shift of the target. It builds chains and travelling waves in a few clicks. In the fly compass chapter the ends of the chain join into a ring. **Random wiring** has an **inputs per cell** slider: 1 to 12, and up to 100 in the Rate models chapter.

> **Note:** In the circuit file, "Every pair" is the rule `all_to_all`, "Neuron k to k" with no shift and equal group sizes is `one_to_one`, a shifted chain or a ring is an explicit list of `edges`, and "Random few inputs" is `fixed_indegree`. Any simulator reads these the same way.

## Neuromodulation

A **neuromodulator** does not make the next cell fire. It changes how cells and synapses respond. Acetylcholine slows the heart's pacemaker. Noradrenaline speeds it up. Serotonin strengthens a synapse. Dopamine gates learning. In Spike a modulator is one number, the same everywhere in the circuit: a **concentration**, in arbitrary units, where 0 is baseline and 1 is strong.

There are three channels:

| Channel | Name | Typical role in the game |
|---|---|---|
| **NE** | Noradrenaline | Sympathetic nerves: speeds up the heart, excites the breathing burster. In the Aplysia levels the NE channel stands for **serotonin** (5-HT), and its dial says so |
| **ACh** | Acetylcholine | Vagus nerve: slows the heart, drives the gut's pacemaker |
| **DA** | Dopamine | Gates learning in the dopamine-gated synapse |

### Where the level comes from

A modulator level is the sum of two things:

1. **An external level**, set by the task or by you in the sandbox with the dials.
2. **Release pools.** An output of the kind `modulator` turns the spikes of its neurons into a concentration. Each spike adds `gain` to a pool. The pool decays with time constant `tau`. At a steady rate `f` the level settles near `gain * f * tau / 1000`.

In the lab, the **Rate readout / Releases modulator** stepper turns a motor output into a releaser. The heart levels, for example, read ACh (vagal) and NE (sympathetic) this way. See [inputs and outputs](inputs-and-outputs.html#modulator-outputs).

| Setting | Unit | Lab range | Starting value |
|---|---|---|---|
| **Modulator** | NE, ACh or DA | | the one the level wants |
| **Gain** | concentration per spike | 0.001 to 0.5 | 0.05 |
| **Tau** | ms | 50 to 5000 | 1000 |

Example from the inspector: at gain 0.05 per spike, tau 1000 ms and 20 spikes per second, the level settles near 1.0.

### What a modulator acts on

A **modulation row** maps a modulator to one parameter. It can act on every neuron of one group, or on every connection of one axon. Two laws:

- **Add** (`gain`): `value = value + gain * level`.
- **Scale** (`scale_per_unit`): `value = value * (1 + scale * level)`.

An optional `min` and `max` clamp the result. A row targets one group or one axon, and uses add or scale, never both. Rows are applied between steps, never in the middle of one.

**Cells that follow modulators by themselves.** The NESTML cells `sa_node_pacemaker`, `butera_burster` and `icc_slow_wave` have parameters called `ne` and `ach`. The engine adds an implicit row for each: NE to `ne` and ACh to `ach`, gain 1, minimum 0. They follow the circuit-wide levels with no setup. A circuit overrides this for one cell and one parameter by listing a row of its own. Other cells ignore the modulators until you add a row.

**In the lab.** Select a neuron group and press **Advanced**. The section **Responds to neuromodulators** lists the rows for that group. A row has a modulator stepper, a parameter stepper, an **add / scale** stepper, a **per unit** slider and an **X** to remove it. **Add row** appends one. Cells that expose `ne` or `ach` offer those as the first parameters. Every other parameter in the **Advanced** list can be modulated too. The lab warns when a model has no parameter a modulator can act on.

**On a synapse.** In the Aplysia levels, the axon inspector has a button **Strengthen with serotonin (NE channel)**. It adds a row that acts on the axon: on `U` for a depressing synapse (add law), or on `weight` for a static one (scale law). A **per unit** slider (0 to 8) sets how strongly. **Remove modulation** deletes the row. This is how the engine makes a sensitised reflex: shock the tail, release serotonin, and the sensory synapse strengthens.

Synapse rows have rules, checked when the circuit loads:

- A modulated `weight` is clamped to the target model's weight ceiling.
- A gap junction cannot be modulated.
- The `weight` of a plastic (STDP or dopamine-gated) synapse cannot be modulated: the learning rule owns it.
- `delay`, `synapse_model` and similar fixed fields cannot be modulated.
- Two axons that share the same sender, target and synapse model cannot both carry a row, since their connections cannot be told apart.

### The modulator dials

In a level whose task declares modulators, **gauges** on the scopes side show the levels the circuit sees: read-only sliders with a chip for each channel (NE amber, ACh cyan, DA violet). In the Aplysia levels the NE chip reads **5-HT**, serotonin. In the **sandbox** the dials are real sliders from 0 to 1, in steps of 0.01. Set a level by hand and watch what the cells do. This is the quickest way to learn what ACh or NE does to a pacemaker. See [the sandbox](sandbox.html).

Levels that declare modulators include Neuromodulation, the heart tasks, the baroreflex, fight or flight, the autonomic boss and the gut level (ACh). See [campaign-body](campaign-body.html).

## A circuit file example

This fragment shows a depressing synapse, a modulation row that sensitises it, and a modulator output:

```json
"projections": [
  { "pre": "S", "post": "M", "rule": "all_to_all",
    "synapse": { "synapse_model": "tsodyks2_synapse", "weight": 400.0, "delay": 2.0,
                 "U": 0.2, "u": 0.2, "x": 1.0, "tau_rec": 1000.0, "tau_fac": 0.0 } }
],
"outputs": [
  { "name": "serotonin", "kind": "modulator", "modulator": "NE", "neurons": [{ "pop": "F" }],
    "gain": 0.05, "tau_ms": 1000 }
],
"modulation": [
  { "modulator": "NE", "projection": 0, "param": "U", "gain": 3.0, "min": 0.0, "max": 1.0 }
]
```

The numbers only show the format. They are not a solution to any level.

## Related pages

- [The parts bin](parts-bin.html): cell models and their parameters.
- [Inputs and outputs](inputs-and-outputs.html): sensors, motors and modulator readouts.
- [A circuit is data](circuit-is-data.html): the circuit file in full.
- [Glossary](glossary.html): plain-language definitions of the concepts used here.
