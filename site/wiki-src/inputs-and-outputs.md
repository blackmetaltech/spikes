---
title: Inputs and outputs
section: Reference
order: 52
summary: The sensor and motor cards of a level: every input kind (dc, Poisson, rate drive, tuned Poisson, spike trains) and every output kind (rate, modulator, ring readout, rate value), with units and real examples.
---

# Inputs and outputs

A level does not give you a body to wire neurons into directly. It gives you **cards**. Sensor cards on one side feed the circuit. Motor cards on the other side are read by the level. Everything between them is yours.

This page explains every kind of card, the units it uses, and how to wire it. For the screen layout see [how to read the lab](how-to-read-the-lab.html). For what sits behind the motor cards (muscles, heart, lungs) see [bodies](bodies.html).

## How the cards work

**Sensor cards** (inputs) sit at one edge of the circuit editor. The level writes a value to each one, every millisecond. A sensor does nothing until you connect it to a neuron group. To connect: drag from the right edge of the sensor card onto a neuron, or press **Connect**, click the card, then click the neuron.

**Motor cards** (outputs) sit at the other edge. The level reads what your neurons do. To connect: drag from a neuron's rim onto the card, or use Connect mode in the other direction. Nothing is read until at least one neuron is wired to the card. An unwired motor reads 0, and the verdict says so.

Select a card and the inspector shows what it is connected to, its kind, and its settings. **Disconnect all** removes every connection of the card. A card the level provides cannot be deleted. An input you add yourself (a noise input) can.

**First-connect weight.** The first time you connect a sensor to a neuron, the lab picks a weight that makes that cell type fire. Many levels set their own start weight for a sensor, so the level can be solved without hunting for a working number. You can change it afterwards in the inspector. See [synapses and modulators](synapses-and-modulators.html) for weights.

## Numbered families

Some levels have many sensors or motors with a numbered name: `cue0` to `cue15`, `dist0` to `dist7`, `pn0` to `pn19`. They are a **family**. A family stands for a body part that has many places: the ring of the fly compass, the eight segments of the gut, the glomeruli of the fly's antennal lobe.

The inspector of any card in a family says: "cue0 to cue15 are a numbered family of 16. A group of 16 neurons wired to any one of them connects neuron k to sensor k." So you do not wire sixteen cards by hand. Either drag from one card onto a group of 16 neurons, or press the button **Wire all to \<group\>** (for sensors) or **Wire \<group\> (16) to all** (for motors). The button appears for each group in the circuit whose size equals the size of the family. Neuron k is connected to sensor k, or motor k.

The order matters. For a ring readout (below), the order you connect the neurons is the order around the ring.

## Input kinds

| Kind | Device in NEST | The level writes | Unit of the value | Where you meet it |
|---|---|---|---|---|
| `dc` | `dc_generator` | A steady current level | pA per unit of value, times the weight | Most micro-puzzles, the spinal limb, lamprey |
| `poisson` | `poisson_generator` | A spike rate | Hz | Touch, salt, light, odour and many other sensors |
| `rate_drive` | none (writes a parameter) | A drive level for rate units | arbitrary units, times the weight | The Rate models chapter |
| `tuned_poisson` | one `poisson_generator` per target | A stimulus angle | degrees | The Tuning chapter |
| `spike_generator` | `spike_generator` | A list of spike times | ms | Circuit files and Python only |
| `sinusoidal_poisson` | `sinusoidal_poisson_generator` | A rate that oscillates in time | Hz | Circuit files and Python only |

### DC input

A **DC** (direct current) input gives its target neurons a steady current. The value the level writes is multiplied by the weight (in pA per unit of value), so the current in picoamps is `value * weight`. It is smooth: no randomness, no spikes.

Use it for a graded signal such as a stretch, a pressure or a level. Many micro-puzzles use `dc`, because a clean signal makes the mechanism easy to read. The inspector shows the weight as "pA/unit". The slider goes up to 2000 pA/unit, whatever cell it drives: a conductance-based cell still takes a current.

Examples:

- **Spinal limb.** The sensors `ag_Ia`, `ant_Ia`, `ag_II` and `ant_II` are `dc` inputs. The task writes the receptor's firing rate in Hz of a muscle spindle, and the circuit's input weight turns each hertz into 4 pA. Receptor cells then fire regularly at about that rate. Real stretch receptors are very regular, so a smooth current is a fair stand-in. The first versions used Poisson trains, but their noise made the limb tremble.
- **Lamprey.** The sensor `drive` is a `dc` input. The brainstem command to swim is held at 1.0, the full command, and the weight sets how many picoamps that is.
- **Breathing.** The sensor `chemo` is a `dc` input in picoamps: the level adds the carotid-body rate and the central chemoreceptor rate and scales them. At rest it is about 13 Hz, which becomes about 51 pA. Five per cent CO2 is about three times that.

### Poisson input

A **Poisson** input gives its target neurons random spikes at a given average rate. "Poisson" means the spikes are independent: knowing when the last one came tells you nothing about the next. Every neuron the input is wired to gets **its own independent train**, so two neurons on the same sensor hear different chatter. That is the reason convergence (many inputs onto one cell) averages noise away.

The weight is the size of one spike's effect, in the unit of the target cell (pA, nS or mV). The value is a rate in hertz. In the lab the inspector's rate slider goes from 0 to 400 Hz for inputs you add yourself.

Examples:

- **Touch reflex.** `touch` is a Poisson input. The receptor chatters at about 150 Hz in the background, and at about 400 Hz while something pokes the skin. A poke is only 2.7 times the background, which is why one cell alone cannot tell a poke from a noisy stretch of chatter.
- **Worm.** The sensors `ASEL` and `ASER` are Poisson inputs: a 5 Hz background, with salt change adding up to 400 Hz.
- **Pupil.** `retina` is a Poisson rate from the eye model (5 to 100 Hz per fibre, times four fibres). `threat` is 0 normally and 250 Hz during the fright cue.
- **Breathing.** `drive` is a constant Poisson rate of 200 Hz into whatever cells you wire.

You can also add a free noise input yourself with **+ Noise** (the button opens at the first level that needs it, and is always open in the sandbox). It starts as a Poisson input at 30 Hz with a weight of half the usual first-connect weight. Use it for background drive, or to test whether a circuit survives chatter. In the inspector, a stepper switches it between **Poisson spikes** and **DC current**.

### Rate drive

A **rate drive** feeds rate units (population rates, no spikes). The value the level writes, times the weight, is added to the drive of each unit it reaches. The drive is the unit's `mu` parameter. For a mean-field (Siegert) unit it is `mean`, the external rate in spikes per second. The weight slider goes from 0 to 5.

Only rate populations may be targets. Spiking inputs on rate populations are refused when the circuit loads, and rate drives on spiking cells likewise.

Example: **Two populations that make a rhythm** drives its excitatory and inhibitory units with a `drive`. **Two populations, one winner** has two drives, `a_in` and `b_in`.

> **Note:** The drive is added to the unit's rate directly, not through its nonlinearity. This differs from the original Wilson-Cowan equations, where the stimulus goes through the sigmoid. A rate can read above 1, and inhibition cannot cancel the drive. See [what is real](what-is-real.html).

### Tuned Poisson input

A **tuned** input says: "the stimulus is at this angle", and each cell it reaches fires at a rate that depends on how far its **preferred angle** is from the stimulus. A cell's preferred angle is the one it responds to most. Cell k of N has the preferred angle `period * k / N`. The rate follows a bell-shaped (von Mises) curve:

`rate = base + (peak - base) * exp(kappa * (cos(2 pi (angle - preferred) / period) - 1))` in Hz

The settings of the curve are fixed by the level: `rate_max` (default 50 Hz), `rate_base` (default 0), `kappa` (default 2, higher is narrower) and the period (360 degrees for a direction, 180 for an orientation). You do not edit them. The level sets the stimulus angle, in degrees.

Used by the three Tuning levels. The cells it reaches are usually parrot relays, so the tuned input stays outside the cell models. See [campaign-theory](campaign-theory.html).

### Spike trains and sinusoidal Poisson

Two more kinds exist for circuit files, Python and `spike-run`. They are not part of the lab's free-input menu.

- `spike_generator` emits spikes at the exact times you list, in milliseconds. Times must sit on the resolution grid (usually 0.1 ms), be above 0 and be sorted.
- `sinusoidal_poisson` has a rate `max(0, rate + amplitude * sin(2 pi freq t + phase))`. Use it for an oscillating drive such as a theta rhythm. The frequency must be below the Nyquist limit of the resolution.

See [a circuit is data](circuit-is-data.html).

## Output kinds

| Kind | What it reads | Unit | Where you meet it |
|---|---|---|---|
| `rate` | The firing of the connected spiking neurons | Hz | Nearly every level |
| `modulator` | The same spikes, turned into a neuromodulator level | concentration, arbitrary units | Heart levels, baroreflex, fight or flight, Aplysia |
| `population_vector` | The direction of a bump on a ring of cells | degrees | The fly compass, the direction decoder |
| `rate_value` | The rate of connected rate units | arbitrary units (spikes per second for a mean-field unit) | The Rate models chapter |

### Rate readout

The default motor card. The level counts the spikes of the connected neurons. Most levels use the mean rate **per neuron**, in hertz, often smoothed over a few tens of milliseconds, and then convert it into a drive for a body part. The conversions are explicit and are part of each level's design. For example:

- Breathing: the phrenic motor rate, divided by a reference rate, gives the inspiratory drive from 0 to 1.
- Pupil: the sphincter and dilator pools are divided by 40 Hz to give each muscle's drive from 0 to 1.
- Spinal limb: the pool's **total** rate (neurons times the mean rate) divided by what 20 motor units can use at their peak gives the muscle's drive. So the pool **size** matters. One motor neuron at its top rate drives one twentieth of the muscle. Every spinal design has to decide how many cells to use.
- Reflex: spikes of the withdraw pool are counted in a 25 ms window. A burst is a window with enough spikes (at least 3, or half the pool if it has more than 6 cells).

> **Note:** The engine can give the **summed** pool rate or the **per neuron** mean. A level says which it uses in its brief and in the pass rule. If a level is judged on the mean rate, adding cells does not raise it. If it is judged on a total, adding cells does.

A rate card over rate units reads 0 (rate units have no spikes): use a rate value card instead.

### Modulator outputs

A **modulator output** turns spikes into a chemical level. The cells it reads release a neuromodulator, and the level reads the concentration instead of the rate.

In the lab, a motor card of such a level has a stepper with **Rate readout** and **Releases modulator**. For a level that reads ACh (the vagus nerve) or NE (the sympathetic nerves) the card warns you: "This task reads ACh from this channel: switch it to 'releases modulator'." After the switch you set:

| Setting | Unit | Range | Start |
|---|---|---|---|
| Modulator | NE, ACh or DA | | The one the level wants |
| Gain | concentration per spike | 0.001 to 0.5 | 0.05 |
| Tau | ms | 50 to 5000 | 1000 |

Each spike adds the gain to a pool. The pool decays with time constant tau. The inspector shows the steady-state level for 20 spikes per second. See [synapses and modulators](synapses-and-modulators.html#neuromodulation) for what the level does afterwards.

Example: **Autonomic heart rate** has three motor cards. `sa_beat` is a rate readout of the pacemaker cells. `vagal` and `sympathetic` are modulator outputs of two *different* groups of release cells, because a controller needs two branches that push in opposite directions.

### Ring readout (population vector)

A **ring readout** reads a bump of activity on a ring of cells and turns it into an angle. The cells are laid out in order: cell k of N has the preferred angle `360 * k / N` degrees, cell 0 at the top. After every step the engine adds up one arrow per spike, each pointing at its cell's angle. The direction of the sum is the bump angle. The length of the sum, divided by the number of spikes, is the **strength** from 0 to 1. A strength of 1 means all spikes sit at one angle. A tidy bump of a half-rectified cosine shape gives about 0.79. A flat or cancelling ring gives 0. With no spikes in a step, there is no direction.

You must connect the **whole ring group, in order**. The order you wire the cells decides the angles. A group wired in reverse reads as a mirror image. A cell can appear only once.

Used by the fly compass levels, and by **Reading a direction from a crowd**. In the lab the **Rate readout / Ring readout** stepper appears on such a card. The world panel draws it as a dial: the ink needle is the true heading, the cyan needle is the bump (its length is how sharp the bump is), and a mark on the rim is the landmark while the lights are on.

### Rate value

A **rate value** reads the mean activity of rate units, in arbitrary units. For a Siegert unit it is the rate in spikes per second. There is nothing to choose in the inspector. The card says: "Rate readout: the mean rate of the rate units connected to this channel."

Used by **Two populations that make a rhythm**, **Two populations, one winner** and **The network and its mean field**. These levels draw their motor channels as scrolling traces on one shared scale.

## Every level's channels

Names from the reference circuits (the level may add more sensors than a reference uses). Families are shown with their size in brackets.

| Level | Sensors (kind) | Motors (kind) |
|---|---|---|
| Threshold | `pulse` (dc) | `spike` (rate) |
| Excitation: weight and delay | `tap` (dc) | `source`, `echo` (rate) |
| Convergence | `in_a`, `in_b`, `in_c` (dc) | `out` (rate) |
| Reflex arc | `touch` (Poisson) | `withdraw` (rate) |
| Inhibition | `drive`, `stop` (dc) | `out` (rate) |
| Subtraction | `plus`, `minus` (dc) | `diff` (rate) |
| Worm chemotaxis | `ASEL`, `ASER` (Poisson) | `turn_left`, `turn_right` (rate) |
| Adaptation | `step` (dc) | `spikes` (rate) |
| Half-centre | `drive` (dc) | `left`, `right` (rate) |
| Lamprey swimmer | `drive` (dc) | `left_motor`, `right_motor` (rate) |
| Recruitment | `command` (dc) | `mu_small`, `mu_mid`, `mu_large` (rate) |
| Reciprocal inhibition | `tone`, `ia_flexor` (dc) | `flexor`, `extensor` (rate) |
| Stretch reflex | `ag_Ia`, `ant_Ia`, `drive` (dc) | `ag_motor`, `ant_motor` (rate) |
| Renshaw cell | `ag_Ia`, `ant_Ia`, `drive` (dc), `noise` (Poisson) | `ag_motor`, `ant_motor` (rate) |
| Posture | `ag_Ia`, `ant_Ia`, `ag_II`, `ant_II`, `drive` (dc) | `ag_motor`, `ant_motor`, `gamma_motor` (rate) |
| Reach | `go`, `target`, `ag_Ia`, `ant_Ia`, `drive` (dc) | `ag_motor`, `ant_motor`, `burst0` to `burst2` (rate) |
| Pacemaking, Coupling | none | `beat` or `a`, `b` (rate) |
| Sinoatrial pacemaker | none | `sa_beat` (rate) |
| Neuromodulation | `vagus` (dc) | `beat` (rate), `vagal` (modulator) |
| Autonomic heart rate | `exercise_cue`, `hr_too_low`, `hr_too_high` (Poisson) | `sa_beat` (rate), `vagal`, `sympathetic` (modulator) |
| Baroreflex | `baroreceptor` (Poisson) | `sa_beat` (rate), `vagal`, `sympathetic` (modulator) |
| Breathing rhythm | `drive` (Poisson), `chemo` (dc) | `phrenic` (rate) |
| Pupil light reflex | `retina`, `threat` (Poisson) | `sphincter`, `dilator` (rate) |
| Peristalsis | `dist0` to `dist7` (Poisson) | `ach0` to `ach7`, `no0` to `no7`, `icc0` to `icc7` (rate) |
| Fight or flight | `threat` (Poisson) | `sa_beat`, `sym_vascular`, `dilator`, `sphincter`, `vent` (rate), `sympathetic`, `vagal` (modulator) |
| Worm chapter | `touch`, `stop`, `push_rev`, `push_fwd`, `touch_head`, `touch_tail` (Poisson) | `reverse`, `forward` (rate) |
| Leech chapter | `drive` (dc), `inhibit` (Poisson) | `burst`, `rebound`, `left`, `right` (rate) |
| Aplysia chapter | `touch`, `shock` (Poisson) | `gill` (rate), `serotonin` (modulator) |
| Fly compass chapter | `cue0` to `cue15` (Poisson), `turn_left`, `turn_right` (dc) | `heading` (ring readout) |
| Fly mushroom body chapter | `pn0` to `pn19`, `punish`, `odor_a`, `odor_b` (Poisson) | `kc`, `avoid` (rate) |
| Crab chapter | `modulator` (dc), `pd` (Poisson) | `ab`, `pd`, `lp`, `py` (rate) |
| Dynamics chapter | `current`, `hold`, `set`, `reset` (dc) | `spikes` (rate) |
| Rate models chapter | `drive`, `a_in`, `b_in`, `ext_mf` (rate drive), `ext` (Poisson) | `e`, `i`, `a`, `b`, `mf` (rate value), `net` (rate) |
| Attractor memory chapter | `cue`, `cue_a`, `cue_b`, `show` (Poisson) | `memory`, `a`, `b` (rate) |
| Tuning chapter | `stim` (tuned Poisson) | `resp` (rate), `heading` (ring readout) |

Not every sensor on this list is used by every level of its chapter: levels share a family of circuits. The brief of a level names the cards it provides.

## Reading the cards: a few habits

1. **Read the card kind in the inspector first.** Is it `dc` (a level in pA), `Poisson` (a rate in Hz), or a ring? It tells you the unit of the weight.
2. **Wire the whole path.** A sensor must reach neurons, and neurons must reach a motor card. A level that checks the mechanism looks at the wiring, and its verdict names a sensor or motor that is not connected.
3. **Use groups, not single cells.** A group of 10 cells on a Poisson sensor hears 10 independent trains. Averaging them is the point of many levels.
4. **Check the unit when you copy a weight.** A weight that works on an `iaf_psc_alpha` cell (pA) is a huge push on a conductance cell (nS).

## Related pages

- [How to read the lab](how-to-read-the-lab.html): where the cards sit on screen.
- [Bodies](bodies.html): what the motor cards drive.
- [Synapses and modulators](synapses-and-modulators.html): weights, delays, and neuromodulator levels.
- [A circuit is data](circuit-is-data.html): the `inputs` and `outputs` keys of the circuit file.
