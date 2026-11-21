---
title: Parts bin
section: Reference
order: 50
summary: Every cell model you can place in Spike, what it does, its parameters with units and defaults, when to use it and which levels use it.
---

# Parts bin

The parts bin is the shelf of cell types. Every neuron you add to the bench is one of these. Each card has a plain description, real examples, the key parameters, and two small plots measured by the engine: an f-I curve (firing rate against input current) and a membrane-voltage trace.

Every model here is a real [NEST](https://www.nest-simulator.org/) model, or a custom cell written in NESTML (a language for describing neuron models) and compiled into the engine. The names and units are NEST's own: mV, ms, pA, nS, pF, Hz. See [what is real](what-is-real.html) for which parts are NEST's own maths and which are re-fits.

> **Note:** There are 29 cards. A card is one model with one set of preset parameters. The seven Izhikevich cards share one model and differ only in their presets.

## How the parts bin works

**Opening it.** In a level the **Parts** button sits on the palette above the circuit editor. The same bin is on the campaign map as a browse-only reference ([the lab](how-to-read-the-lab.html) shows where the buttons are). On a phone the button is in the Menu.

**Picking a part.** In the lab, pick a card and press **Use**. The button on the palette then reads `Parts: <card title>`, and every neuron you add with **+ Excitatory** or **+ Inhibitory** is made from that card, with its preset parameters. Neurons already on the bench keep their type. To change one, select it and use the model and preset steppers in the inspector.

**For this task.** Inside a level the bin shows a heading **For this task** first. These are the cells the level's reference circuit uses, plus the cells of its chapter.

**When it appears.** The Parts button is not on the palette in the first levels. It opens at the first level whose reference circuit uses a cell other than the plain integrate-and-fire cell. That is **Adaptation** (the eighth level of the Invertebrates chapter, after the touch reflex and the salt-seeking worm), whose reference uses an adaptive exponential cell. It stays open once you are that far. The sandbox always shows it. The rule is in `godot/ui/lab/lab_unlocks.gd`.

**Which cards are open.** A card is usable when your build can run it and its chapter is open. Locked cards show `UNLOCKS IN <chapter>`. Cards your build cannot run show `NOT IN THIS BUILD`. Two exceptions: a model that the current level's own reference circuit uses is always pickable, and the sandbox opens everything. The table below lists the chapter that opens each card. The Bestiary and Theory wings are open from the start, so the crab cell and the rate and mean-field cards are open as soon as the Parts button is.

> **Note:** The inspector of a selected neuron has a model stepper that lists only models you can use. If a model is not in that list, pick its card in the Parts bin first.

## All cards at a glance

| Card | NEST / NESTML model | Family | Opens in | Weight unit |
|---|---|---|---|---|
| Leaky integrate-and-fire (current synapses) | `iaf_psc_alpha` | Integrate-and-fire | Invertebrates | pA |
| Integrate-and-fire, conductance synapses | `iaf_cond_exp` | Integrate-and-fire | Spinal cord | nS |
| Adapting integrate-and-fire | `iaf_cond_exp_sfa_rr` | Integrate-and-fire | Spinal cord | nS |
| Multi-timescale adaptive threshold | `mat2_psc_exp` | Integrate-and-fire | Spinal cord | pA |
| Integrate-and-fire, delta synapses | `iaf_psc_delta` | Integrate-and-fire | Rate models | mV |
| Adaptive exponential I&F, alpha | `aeif_cond_alpha` | Adaptive exponential | Invertebrates | nS |
| Adaptive exponential I&F, exponential | `aeif_cond_exp` | Adaptive exponential | Spinal cord | nS |
| Izhikevich RS, IB, CH, FS | `izhikevich` | Phenomenological | Invertebrates | mV |
| Izhikevich LTS | `izhikevich` | Phenomenological | Spinal cord | mV |
| Izhikevich TC, RZ | `izhikevich` | Phenomenological | Breathing | mV |
| Hodgkin-Huxley (squid axon) | `hh_psc_alpha` | Hodgkin-Huxley | Invertebrates | pA |
| Hodgkin-Huxley (Traub) | `hh_cond_exp_traub` | Hodgkin-Huxley | Spinal cord | nS |
| Hodgkin-Huxley with gap junctions (olive) | `hh_psc_alpha_gap` | Hodgkin-Huxley | Heart | pA |
| Traub Hodgkin-Huxley with gap junctions | `hh_cond_beta_gap_traub` | Hodgkin-Huxley | Heart | nS |
| Hill-Tononi thalamo-cortical neuron | `ht_neuron` | Multi-channel | Breathing | x g_peak |
| Generalised integrate-and-fire (GIF) | `gif_cond_exp` | Stochastic / fitted | Spinal cord | nS |
| Stochastic point-process neuron | `pp_psc_delta` | Stochastic / fitted | Heart | mV |
| Parrot (relay) neuron | `parrot_neuron` | Relay | Invertebrates | none |
| Rate unit (threshold-linear) | `threshold_lin_rate_ipn` | Rate models | Rate models | none |
| Rate unit (sigmoid) | `tanh_rate_ipn` | Rate models | Rate models | none |
| Mean-field unit (Siegert) | `siegert_neuron` | Rate models | Rate models | none |
| Sinoatrial node pacemaker | `sa_node_pacemaker` | Custom (NESTML) | Heart | nS |
| Bursting pacemaker (preBotzinger) | `butera_burster` | Custom (NESTML) | Breathing | nS |
| Stomatogastric neuron | `stg_cell` | Custom (NESTML) | Crab (open from start) | nS |
| Interstitial cell of Cajal | `icc_slow_wave` | Custom (NESTML) | Autonomic | pA |

The column "Weight unit" is the unit of a synaptic weight onto that cell. A weight is how strong one connection is (see [synapses and modulators](synapses-and-modulators.html)). The cell models use different units because NEST models them differently: current-based cells take picoamps, conductance-based cells take nanosiemens, delta-synapse cells take a voltage jump in millivolts.

> **Note:** Each card also lists a default input weight. It is the weight at which one incoming spike gives a small voltage step (about 1 to 2 mV) at rest. The engine measured it for each cell. The lab uses it when you draw a new connection.

## Words used on this page

- **Membrane potential (V_m):** the voltage across the cell's membrane, in millivolts. A spike is a brief jump in it.
- **Threshold:** the voltage at which the cell fires.
- **Rheobase:** the smallest steady current that makes a cell fire. For the plain integrate-and-fire cell at its defaults it is 375 pA.
- **Adaptation:** firing that slows down under a steady input.
- **Conductance-based synapse:** a synapse that opens ion channels. Its effect depends on the membrane voltage.
- **Current-based synapse:** a synapse that injects a fixed current, whatever the voltage.
- **f-I curve:** the cell's firing rate (f) plotted against a steady input current (I).
- **Class 1 / class 2:** class 1 cells fire slowly at first and speed up smoothly as the current grows. Class 2 cells jump straight to a clear rate.

## Integrate-and-fire cells

These cells treat the membrane like a leaky capacitor. Input charges it, it leaks back to rest, and when the voltage reaches threshold the cell fires and resets. They are cheap and easy to reason about.

### Leaky integrate-and-fire (`iaf_psc_alpha`)

The default cell of the game. It fires regularly, has no adaptation, and its rate rises smoothly with input after a threshold current (class 1). Synaptic input is a current with an alpha-function shape: it rises over a couple of milliseconds, then decays.

| Parameter | Unit | Default | Slider range | Meaning |
|---|---|---|---|---|
| `tau_m` | ms | 10 | 2 to 100 | Membrane time constant: how slowly the cell forgets its input |
| `C_m` | pF | 250 | 50 to 1000 | Membrane capacitance |
| `V_th` | mV | -55 | -60 to -40 | Spike threshold |
| `V_reset` | mV | -70 | -80 to -55 | Voltage right after a spike |
| `t_ref` | ms | 2 | 0 to 10 | Refractory period: dead time after a spike |
| `tau_syn_ex` | ms | 2 | 0.1 to 20 | Excitatory current time constant |
| `tau_syn_in` | ms | 2 | 0.1 to 20 | Inhibitory current time constant |
| `I_e` | pA | 0 | -500 to 1000 | Constant input current |

Default input weight: 80 pA. Presets in the lab: **Default**, **Fast integrator** (`tau_m` 6 ms, a coincidence detector) and **Slow integrator** (`tau_m` 30 ms, sums inputs over time).
Use it for: anything where the details do not matter. Sensory relays, interneurons and motor neurons in most early levels.
Used by: nearly every reference circuit, including all the first micro-puzzles (Threshold, Excitation, Convergence, Inhibition, Subtraction), the touch reflex, the worm, the Aplysia and fly levels, and the attractor-memory levels.

### Integrate-and-fire with conductance synapses (`iaf_cond_exp`)

Like the plain cell, but synapses open ion channels. An input then depends on the voltage and shunts the cell: inhibition near rest mostly clamps the voltage instead of pushing it down.

| Parameter | Unit | Default | Meaning |
|---|---|---|---|
| `g_L` | nS | 16.67 | Leak conductance (`tau_m` = `C_m` / `g_L`) |
| `C_m` | pF | 250 | Membrane capacitance |
| `V_th` | mV | -55 | Spike threshold |
| `V_reset` | mV | -60 | Reset voltage |
| `t_ref` | ms | 2 | Refractory period |
| `E_ex` | mV | 0 | Excitatory reversal potential |
| `E_in` | mV | -85 | Inhibitory reversal potential |
| `tau_syn_ex` | ms | 0.2 | Excitatory conductance decay |
| `tau_syn_in` | ms | 2 | Inhibitory conductance decay |
| `I_e` | pA | 0 | Constant input current |

Default input weight: 20 nS. Use it when shunting inhibition matters, or to build a model like a cortical cell in a busy network. No reference circuit uses it; it is there for the sandbox and your own designs.

### Adapting integrate-and-fire (`iaf_cond_exp_sfa_rr`)

A conductance-based cell with two spike-triggered potassium conductances. One is slow and makes firing slow down under steady input (spike-frequency adaptation, SFA). One is fast and makes the cell harder to fire right after a spike (relative refractoriness, RR). Its card is excitatory by default.

| Parameter | Unit | Default | Meaning |
|---|---|---|---|
| `q_sfa` | nS | 14.48 | Adaptation conductance added at each spike (0 = none) |
| `tau_sfa` | ms | 110 | Adaptation decay |
| `q_rr` | nS | 3214 | Fast refractory conductance added at each spike |
| `tau_rr` | ms | 1.97 | Refractory conductance decay |
| `g_L` | nS | 28.95 | Leak conductance |
| `C_m` | pF | 289.5 | Membrane capacitance |
| `V_th` | mV | -57 | Spike threshold |
| `I_e` | pA | 0 | Constant input current |

Default input weight: 4 nS. Use it for regular-spiking cortical pyramidal cells with adaptation over about 100 ms. Not used by a reference circuit.

### Multi-timescale adaptive threshold (`mat2_psc_exp`)

An integrate-and-fire cell whose *threshold* jumps at every spike and relaxes back with two time constants (10 and 200 ms). This gives adaptation and bursts, matches recorded cells well, and is cheap.

| Parameter | Unit | Default | Meaning |
|---|---|---|---|
| `omega` | mV | -51 | Resting threshold |
| `alpha_1` | mV | 37 | Fast threshold jump per spike |
| `alpha_2` | mV | 2 | Slow threshold jump per spike |
| `tau_1` | ms | 10 | Fast threshold decay |
| `tau_2` | ms | 200 | Slow threshold decay |
| `tau_m` | ms | 5 | Membrane time constant |
| `C_m` | pF | 100 | Membrane capacitance |
| `I_e` | pA | 0 | Constant input current |

Default input weight: 150 pA. Not used by a reference circuit.

### Integrate-and-fire with delta synapses (`iaf_psc_delta`)

Synaptic input is an instantaneous jump in the membrane voltage, so the **weight of a connection is the size of the voltage step in millivolts**. This is the cell that the Siegert mean-field formula describes exactly. It is the spiking partner of the mean-field unit below.

| Parameter | Unit | Default | Meaning |
|---|---|---|---|
| `tau_m` | ms | 10 | Membrane time constant |
| `C_m` | pF | 250 | Membrane capacitance |
| `t_ref` | ms | 2 | Refractory period |
| `E_L` | mV | -70 | Resting potential (set 0 to read other voltages from rest) |
| `V_th` | mV | -55 | Spike threshold |
| `V_reset` | mV | -70 | Reset voltage |
| `I_e` | pA | 0 | Constant input current |

Default input weight: 1 mV. Used by **The network and its mean field** (Theory wing, [campaign-theory](campaign-theory.html)).

## Adaptive exponential cells

### Adaptive exponential integrate-and-fire (`aeif_cond_alpha`, `aeif_cond_exp`)

An integrate-and-fire cell with a realistic sharp exponential spike upswing and a slow adaptation current `w`. Depending on `a`, `b` and the reset, one equation gives regular firing, adaptation, bursting or irregular firing. The two cards differ only in the synapse shape: alpha-function conductances (`_alpha`) or exponentially decaying ones (`_exp`).

| Parameter | Unit | Default | Meaning |
|---|---|---|---|
| `a` | nS | 4 | Sub-threshold adaptation: how strongly `w` follows the voltage |
| `b` | pA | 80.5 | Spike-triggered jump of `w` |
| `tau_w` | ms | 144 | Adaptation time constant |
| `Delta_T` | mV | 2 | Sharpness of the spike upswing (small = sharp) |
| `V_th` | mV | -50.4 | Exponential threshold |
| `V_reset` | mV | -60 | Reset voltage |
| `g_L` | nS | 30 | Leak conductance |
| `C_m` | pF | 281 | Membrane capacitance |
| `I_e` | pA | 0 | Constant input current |

Default input weight: 8 nS (alpha) or 10 nS (exponential). Set `a` and `b` to 0 for a fast-spiking, inhibitory-like cell.

Presets for `aeif_cond_alpha` in the lab (after Naud et al. 2008): **NEST default** (adapting), **Tonic** (`a` 2, `b` 0, `tau_w` 30, `V_reset` -58: regular firing), **Adapting** (`a` 2, `b` 60, `tau_w` 500, `V_reset` -58: slows down under input) and **Bursting** (`a` -0.5, `b` 7, `tau_w` 5, `V_reset` -46).

Use it for: neurons that must adapt, burst or rebound. It is the workhorse for rhythms, since adaptation and rebound are what make half-centre oscillators work.
Used by: **Adaptation**, **Half-centre**, **Rebound after inhibition** and **The heartbeat** (leech), the lamprey swimmer, and the spinal-cord reference circuits (stretch reflex, Renshaw, posture, reach, spinal sprint).

## Izhikevich cells

### Izhikevich (`izhikevich`, seven cards)

A two-equation cell with many firing patterns. Four numbers (a, b, c, d) pick the pattern. It is cheap and very flexible. Its voltage and recovery variable are drawn on the phase plane in the Theory wing's first chapter.

| Parameter | Unit | Meaning |
|---|---|---|
| `a` | 1/ms | Recovery speed: how fast the slow variable follows the voltage |
| `b` | none | Sensitivity of the recovery variable to sub-threshold voltage (resonance) |
| `c` | mV | Reset voltage after a spike |
| `d` | none | Extra recovery kick after each spike (large = strong adaptation or bursting) |
| `I_e` | pA | Constant input current (the model's own units, about 1 per mV/ms) |

One incoming spike steps the membrane voltage by exactly the weight, so the default input weight of 2 means a 2 mV step. The weight unit is mV.

| Card | a | b | c (mV) | d | Pattern |
|---|---|---|---|---|---|
| RS, regular spiking | 0.02 | 0.2 | -65 | 8 | A train that slows down under steady input. The classic excitatory cortical cell |
| IB, intrinsically bursting | 0.02 | 0.2 | -55 | 4 | A burst, then regular spiking |
| CH, chattering | 0.02 | 0.2 | -50 | 2 | Fast bursts of 3 to 4 spikes with short pauses |
| FS, fast spiking | 0.1 | 0.2 | -65 | 2 | Fast, no adaptation. An inhibitory interneuron |
| LTS, low-threshold spiking | 0.02 | 0.25 | -65 | 2 | Fires at low input, adapts moderately, tends to rebound |
| TC, thalamo-cortical | 0.02 | 0.25 | -65 | 0.05 | Tonic at rest. For burst mode, give a negative offset (about -5 pA) first, then a step |
| RZ, resonator | 0.1 | 0.26 | -65 | 2 | Responds best to input at its own preferred frequency |

The lab's Izhikevich preset stepper offers four of these (regular spiking, intrinsic bursting, chattering, fast spiking). The other three come from their Parts-bin cards.

Used by: **Where firing starts**, **Only the change counts**, **A neuron that remembers a pulse** (Theory), **A cell that bursts** (leech) and **Rhythm and gating** (breathing micro-puzzle).

## Hodgkin-Huxley cells

These are conductance models of the action potential, with real channels. They are the most detailed and the most expensive cells in the bin. They have a sharp threshold and jump straight to fast firing (class 2). None of the reference circuits uses a plain Hodgkin-Huxley cell, so they are free-play parts for the sandbox.

### Hodgkin-Huxley, squid axon (`hh_psc_alpha`)

Sodium, potassium and leak channels with the original kinetics. Silent below a sharp threshold; it can fire a rebound spike after a hyperpolarising pulse.

| Parameter | Unit | Default | Meaning |
|---|---|---|---|
| `g_Na` | nS | 12000 | Maximum sodium conductance (upstroke) |
| `g_K` | nS | 3600 | Maximum potassium conductance (repolarisation) |
| `g_L` | nS | 30 | Leak conductance |
| `E_Na` | mV | 50 | Sodium reversal potential |
| `E_K` | mV | -77 | Potassium reversal potential |
| `C_m` | pF | 100 | Membrane capacitance |
| `t_ref` | ms | 2 | Refractory period used to count spikes |
| `I_e` | pA | 0 | Constant input current |

Default input weight: 250 pA (the lab's fallback for this model is 400 pA).

### Hodgkin-Huxley, Traub (`hh_cond_exp_traub`)

A Traub-Miles cell with fast sodium, delayed-rectifier potassium and exponential conductance synapses. The card sets `V_T` to -50 mV so it is quiet at rest. (NEST's own default of -63 mV fires by itself at about 14 Hz.)

| Parameter | Unit | Default | Meaning |
|---|---|---|---|
| `V_T` | mV | -50 | Voltage offset of the channel kinetics (lower = more excitable) |
| `g_Na` | nS | 20000 | Sodium conductance |
| `g_K` | nS | 6000 | Potassium conductance |
| `g_L` | nS | 10 | Leak conductance |
| `C_m` | pF | 200 | Membrane capacitance |
| `tau_syn_ex` | ms | 5 | Excitatory conductance decay |
| `tau_syn_in` | ms | 10 | Inhibitory conductance decay |
| `I_e` | pA | 0 | Constant input current |

Default input weight: 2 nS (one spike gives a 1.9 mV step at rest). Examples: hippocampal CA3 pyramidal cells.

### Gap-junction cells (`hh_psc_alpha_gap`, `hh_cond_beta_gap_traub`)

These two accept **gap junctions**: electrical coupling between neighbouring cells that synchronises them. They are the only cells that do. Chemical synapses onto them can be excitatory or inhibitory as usual. See [synapses and modulators](synapses-and-modulators.html#gap-junctions).

`hh_psc_alpha_gap` is a Hodgkin-Huxley cell fitted to inferior-olive neurons, with extra Kv1 and persistent sodium channels.

| Parameter | Unit | Default |
|---|---|---|
| `g_Na` | nS | 4500 |
| `g_Kv3` | nS | 9000 |
| `g_Kv1` | nS | 9 |
| `g_L` | nS | 10 |
| `C_m` | pF | 40 |
| `I_e` | pA | 0 |

Default input weight: 90 pA.

`hh_cond_beta_gap_traub` is the Traub-Miles cell with beta-function conductance synapses.

| Parameter | Unit | Default |
|---|---|---|
| `V_T` | mV | -50 |
| `g_Na` | nS | 20000 |
| `g_K` | nS | 6000 |
| `g_L` | nS | 10 |
| `C_m` | pF | 200 |
| `I_e` | pA | 0 |

Default input weight: 1 nS (a 1.2 mV step).

## Multi-channel and stochastic cells

### Hill-Tononi neuron (`ht_neuron`)

A point neuron with four intrinsic currents (persistent sodium, depolarisation-activated potassium, low-threshold calcium T and the h-current) and AMPA, NMDA, GABA-A and GABA-B synapses, each on its own **receptor port**. It switches between tonic and burst firing depending on its state, which gives sleep-like slow oscillations in networks.

| Parameter | Unit | Default | Meaning |
|---|---|---|---|
| `g_peak_T` | nS | 1 | Low-threshold calcium current (burst mode) |
| `g_peak_h` | nS | 1 | h-current (rhythmicity and rebound) |
| `g_peak_NaP` | nS | 1 | Persistent sodium |
| `g_peak_KNa` | nS | 1 | Sodium-dependent potassium (slow adaptation) |
| `g_KL` | nS | 1 | Potassium leak (sets resting level) |
| `tau_m` | ms | 16 | Membrane time constant |
| `theta_eq` | mV | -51 | Resting spike threshold |

Weight unit: multiples of the peak conductance. Weight 1 on the AMPA port gives a 1 mV step. Synapses sit on receptor ports (AMPA is port 1), so inhibition needs the right port. This cell is for experts and the sandbox.

### Generalised integrate-and-fire (`gif_cond_exp`)

An integrate-and-fire cell with a fuzzy threshold. The chance of firing rises exponentially once the voltage passes threshold, so spike times are irregular. Spike-triggered currents and a moving threshold can be fitted to match recorded cells.

| Parameter | Unit | Default | Meaning |
|---|---|---|---|
| `lambda_0` | Hz | 1 | Firing probability density at threshold |
| `Delta_V` | mV | 0.5 | Threshold softness (large = noisier) |
| `V_T_star` | mV | -35 | Threshold voltage |
| `g_L` | nS | 4 | Leak conductance |
| `C_m` | pF | 80 | Membrane capacitance |
| `V_reset` | mV | -55 | Reset voltage |
| `t_ref` | ms | 4 | Absolute refractory period |
| `I_e` | pA | 0 | Constant input current |

Default input weight: 3 nS.

### Stochastic point-process neuron (`pp_psc_delta`)

Fires at random, with a rate that grows exponentially with the membrane potential (escape noise), followed by a dead time. Trains look Poisson-like, like cells in the living brain. Input is an instantaneous voltage jump.

| Parameter | Unit | Default | Meaning |
|---|---|---|---|
| `c_1` | 1/(s mV) | 0 | Linear coefficient of the rate-voltage relation |
| `c_2` | 1/s | 1.238 | Base rate coefficient |
| `c_3` | 1/mV | 0.25 | Exponential steepness |
| `dead_time` | ms | 1 | Silent period after each spike |
| `tau_m` | ms | 10 | Membrane time constant |
| `C_m` | pF | 250 | Membrane capacitance |
| `I_e` | pA | 0 | Constant input current |

Default input weight: 1 mV. Use it as a noisy source of spikes or a sensory afferent with realistic variability.

## Relay and population cells

### Parrot neuron (`parrot_neuron`)

Repeats every spike it receives, one for one, with no membrane dynamics. It has no parameters. The weight on its input does not matter as long as it is above zero; the sign of the connections it makes decides whether it excites or inhibits downstream.

Use it to fan one input out to many targets, as a relay station, or to turn a stimulation device into a real neuron that can take plastic synapses.
Used by: **Renshaw cell** (as part of the limb circuit) and the three tuning levels (**A cell tuned by its inputs**, **Sharpening a tuning curve**, **Reading a direction from a crowd**), where the afferents are parrot cells driven by tuned Poisson input.

### Rate unit, threshold-linear (`threshold_lin_rate_ipn`)

A whole population as one number, its firing rate. It has no spikes, no raster and no membrane voltage. The rate follows its input with a time constant `tau`. The summed input from other units is rectified at a threshold and capped at a ceiling.

Connect rate units with rate connections (the weight is the gain of one unit onto another, negative for inhibition) and drive them with a **rate input** (see [inputs and outputs](inputs-and-outputs.html)).

| Parameter | Unit | Default | Meaning |
|---|---|---|---|
| `tau` | ms | 10 | How fast the rate follows its input |
| `mu` | none | 0 | Constant drive added to the rate (a drive input writes it) |
| `sigma` | none | 0 | Noise on the rate (0 = deterministic) |
| `g` | none | 1 | Gain of the rectified input |
| `theta` | none | 0 | Threshold of the input |
| `alpha` | none | 1 | Ceiling of the rectified input |

The card's preset sets `sigma` to 0 and `alpha` to 1. Rates are in arbitrary units.
Used by: **Two populations that make a rhythm** and **Two populations, one winner**.

### Rate unit, sigmoid (`tanh_rate_ipn`)

The same idea, but the summed input goes through a hyperbolic tangent, so the rate saturates both ways, between -1 and 1 (read as activity relative to a baseline). Parameters: `tau` 10 ms, `mu` 0, `sigma` 0, `g` 1 (steepness), `theta` 0. No reference circuit uses it.

### Mean-field unit, Siegert (`siegert_neuron`)

The mean-field stand-in for a population of integrate-and-fire cells with delta synapses. Its rate comes from the Siegert formula, applied to the mean and the variance of the input it gets from other mean-field units. A connection between two of them carries two numbers: J, the size of one voltage step in mV, and K, how many such inputs each cell gets. A unit with no input of its own, whose `mean` is set to the external rate, stands for the external drive.

| Parameter | Unit | Default | Meaning |
|---|---|---|---|
| `tau` | ms | 1 | How fast the rate relaxes to the Siegert value (a pseudo-time of the iteration, not a membrane constant) |
| `tau_m` | ms | 5 | Membrane time constant of the cells it stands for |
| `tau_syn` | ms | 0 | Synaptic time constant (0 for delta synapses) |
| `t_ref` | ms | 2 | Refractory period of those cells |
| `mean` | 1/s | 0 | Constant input rate (for a driving unit, the external rate) |
| `theta` | mV | 15 | Threshold relative to rest |
| `V_reset` | mV | 0 | Reset relative to rest |

Used by: **The network and its mean field** (Theory wing). See [campaign-theory](campaign-theory.html).

## Custom NESTML cells

These four cells were written for the game in NESTML. Each has an exponential synaptic input with an `excitatory` and an `inhibitory` spike port, so a positive weight excites and a negative weight inhibits. Each has a `continuous` current port (the `dc` inputs) and the parameter `I_e`. Their sources are in `models/nestml/`. They are checked against PyNEST spike for spike, but they are not validated against recordings. See [what is real](what-is-real.html) for the honest account of each.

### Sinoatrial node pacemaker (`sa_node_pacemaker`)

A reduced model of the heart's pacemaker cell. It fires on its own, with no input, through the funny current I_f (HCN channels), T- and L-type calcium, and a delayed-rectifier potassium. **One spike is one heartbeat.** Acetylcholine (the vagus) slows it; noradrenaline (the sympathetic nerves) speeds it up. A large depolarising current stops it (depolarisation block).

| Parameter | Unit | Default | Slider range | Meaning |
|---|---|---|---|---|
| `ne` | a.u. | 0 | 0 to 1 | Noradrenaline level: raises I_f and I_CaL (speeds up) |
| `ach` | a.u. | 0 | 0 to 1 | Acetylcholine level: opens I_KACh and shifts I_f (slows down) |
| `g_f` | nS | 0.34 | 0.1 to 1 | Funny-current (HCN) conductance |
| `g_CaL` | nS | 3.53 | 1 to 6 | L-type calcium conductance |
| `g_KACh` | nS | 0.03 | 0 to 0.2 | Muscarinic potassium conductance at full ACh |
| `C_m` | pF | 32 | 10 to 100 | Membrane capacitance |
| `I_e` | pA | 0 | -10 to 10 | Constant input current |

Default input weight: 0.2 nS (about a 2 mV step; the reference coupling between node cells is 2 nS). The cell follows the circuit-wide ACh and NE levels with no extra setup (see [synapses and modulators](synapses-and-modulators.html#neuromodulation)).
Used by: **Pacemaking**, **Coupling**, **Neuromodulation**, the heart tasks, the baroreflex, fight or flight and the autonomic boss.

> **Note:** The card text says the cell "fires on its own at about 70 beats per minute". The model document (`docs/models/sa_node_pacemaker.md`) says about 100 beats per minute with no autonomic input, and 60 to 70 with resting vagal tone (ACh about 0.5). The levels that use the cell hold resting vagal tone, which is why they show the lower rate.

### Bursting pacemaker (`butera_burster`)

A persistent-sodium burster: slow inactivation of the persistent sodium current makes the cell burst rhythmically with no synaptic input. It is the kernel of the brainstem's respiratory rhythm generator (the preBotzinger complex). More tonic drive makes the bursts faster and finally tonic. One spike is one action potential in a burst. The equations and parameters are those of Butera, Rinzel and Smith (1999), model 1.

| Parameter | Unit | Default | Meaning |
|---|---|---|---|
| `g_NaP` | nS | 2.8 | Persistent sodium conductance (burst drive) |
| `g_L` | nS | 2.8 | Leak conductance (sets the burst regime) |
| `g_tonic` | nS | 0.3 | Tonic excitatory conductance: speeds the bursts up |
| `C_m` | pF | 21 | Membrane capacitance |
| `I_e` | pA | 0 | Constant input current |
| `ne` | a.u. | 0 | Noradrenaline (adds excitatory conductance) |
| `ach` | a.u. | 0 | Acetylcholine (adds excitatory conductance) |

Default input weight: 0.05 nS. The level's reference circuit uses 0.03 to 0.09 nS. The `ne` and `ach` effect is a game choice, not a result of the paper.
Used by: **Breathing rhythm**. The brainstem burster is excitatory.

### Stomatogastric neuron (`stg_cell`)

A single-compartment conductance model with the currents of the crab stomatogastric ganglion: fast sodium and delayed-rectifier potassium for spikes, two calcium currents (T-type and slow), a calcium-activated potassium current for slow bursts, an A-type potassium current, and the hyperpolarisation-activated H current.

> **Warning:** A new cell from the parts bin has **only the spike currents** (sodium, delayed rectifier, A-type, leak) and is silent. It does not burst and does not rebound until you give it the calcium currents, the calcium-activated potassium current (bursts) or the H current (rebound). The crab levels teach exactly this.

| Parameter | Unit | Card default | Parts-bin cell | Meaning |
|---|---|---|---|---|
| `g_Na` | nS | 62800 | 62800 | Fast sodium (spikes) |
| `g_Kd` | nS | 62800 | 31400 | Delayed-rectifier potassium (repolarisation) |
| `g_CaT` | nS | 1570 | 0 | Transient calcium (rebound, burst start) |
| `g_CaS` | nS | 3768 | 0 | Slow calcium (burst plateau) |
| `g_A` | nS | 31400 | 6280 | A-type potassium (delays firing after inhibition) |
| `g_KCa` | nS | 3140 | 0 | Calcium-activated potassium (ends bursts; sets the period) |
| `g_H` | nS | 6.28 | 0 | H current (rebound) |
| `g_L` | nS | 6.28 | 6.28 | Leak |
| `I_e` | pA | 0 | 0 | Constant input current |

The "Card default" column is what the card lists. The "Parts-bin cell" column is the preset a new cell actually gets. Default input weight: 20 nS of a spike-triggered synaptic conductance. This cell has no `ne` or `ach` parameters: the pyloric rhythm's modulator is a `dc` input. The card suggests inhibitory use, because pyloric followers are inhibited by the pacemaker.
Used by: **A pacemaker kernel**, **Phase from rebound**, **The pyloric rhythm** and its diagnosis case.

### Interstitial cell of Cajal (`icc_slow_wave`)

A two-variable (FitzHugh-Nagumo type) oscillator for the gut's pacemaker cells: slow plateau-like waves about three times per minute that set the rhythm of peristalsis. **It emits one spike at each wave onset.** ACh speeds it up, NE slows it.

| Parameter | Unit | Default | Meaning |
|---|---|---|---|
| `drive` | none | 0.5 | Baseline excitation (0.5 gives about 3 waves per minute) |
| `tau_w` | ms | 6000 | Recovery time constant (sets the wave period) |
| `tau_x` | ms | 600 | Excitation time constant (sets the wave onset speed) |
| `V_base` | mV | -70 | Trough of the slow wave |
| `V_amp` | mV | 25 | Amplitude of the slow wave |
| `I_e` | pA | 0 | Constant input current (100 pA shifts the excitation by 1) |
| `ach` | a.u. | 0 | Acetylcholine (depolarises, speeds up) |
| `ne` | a.u. | 0 | Noradrenaline (hyperpolarises, slows down) |

Default input weight: 200 pA (a one-unit step of the excitation; the reference coupling is 400 pA). Its card is excitatory: it drives smooth muscle through coupling. This is a phenomenological oscillator, not a channel model.
Used by: **Peristalsis**.

## Which cell for which job

| You want | Try |
|---|---|
| A cell whose details do not matter | `iaf_psc_alpha` |
| A cell that slows down under steady input | `aeif_cond_alpha` with the Adapting preset, or `izhikevich` RS |
| A cell that bursts | `izhikevich` IB or CH, or `aeif_cond_alpha` Bursting |
| A cell that fires after inhibition ends (rebound) | `aeif_cond_alpha`, `stg_cell` with H current, `hh_psc_alpha` |
| A pacemaker that fires with no input | `sa_node_pacemaker`, `butera_burster`, `stg_cell` with calcium currents, `icc_slow_wave` |
| A fast inhibitory interneuron | `izhikevich` FS |
| A one-to-one relay or fan-out | `parrot_neuron` |
| Irregular, Poisson-like firing | `pp_psc_delta` or `gif_cond_exp` |
| Electrical coupling | `hh_psc_alpha_gap` or `hh_cond_beta_gap_traub` |
| A population as one number | `threshold_lin_rate_ipn`, `tanh_rate_ipn` |
| The mean-field prediction for a spiking network | `siegert_neuron` next to `iaf_psc_delta` |

## Notes on what the levels use

- Most reference circuits use `iaf_psc_alpha`. Rhythm, spinal and Bestiary circuits add `aeif_cond_alpha`, and some spinal circuits and the lamprey use `iaf_cond_alpha`.
- `iaf_cond_alpha` (integrate-and-fire with alpha-shaped conductance synapses) is used by seven reference circuits (stretch reflex, Renshaw, posture, reach, spinal sprint, lamprey and the shaky-arm diagnosis case) but has **no card in the parts bin**. In those levels you can inspect it, but you cannot add more of it from the bin. Use `iaf_cond_exp` or `aeif_cond_alpha` for similar cells.
- Weights have limits: the circuit format refuses `|weight|` above 500 nS onto a conductance-based cell and above 10000 pA onto a current-based cell. The reference circuits stay below 60 nS and 1500 pA. See [a circuit is data](circuit-is-data.html).
- The f-I plots and voltage traces are measured by the engine with the `spike-catalog` tool and shipped as `catalog/thumbs.json`. If the file is missing, the cards show their text only. See [spike-run and tools](spike-run.html).

## Related pages

- [Synapses and modulators](synapses-and-modulators.html): how cells talk to each other.
- [Inputs and outputs](inputs-and-outputs.html): how a level feeds and reads a circuit.
- [Make it yours](make-it-yours.html): adding your own cell with NESTML.
- [What is real](what-is-real.html): which cells are NEST's own and which are re-fits.
