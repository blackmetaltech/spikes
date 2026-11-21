---
title: The sandbox
section: Playing
order: 14
summary: The free bench with no task and every part unlocked: what it has, what it lacks and what to use it for.
---

# The sandbox

The sandbox is the lab with no level. There is no brief, no score, no stars and no pass rule. Every part is unlocked. You build anything and read it off the scopes.

Open it from the title screen (**II. Laboratory**) or from the campaign map (**Sandbox**).

## What you get

| Feature | In the sandbox |
|---|---|
| Neuron models | Every model in the parts bin that the build can run. See [the parts bin](parts-bin.html). |
| Inhibitory neurons, noise inputs | Always available. |
| Analysis, Load, Save | Always available. |
| Sensors and motors | None. A level provides them. In the sandbox you drive cells with noise inputs and read the scopes. |
| Check, Hint, Test x5, Brief | Not shown. There is nothing to score. |
| Lesion lab | Not shown. It needs a level to score against. |
| Speed, Fit | In the More menu, as in a level. |

## The noise input

Press **+ Noise** to add an input device. It is a Poisson spike generator (random spike times at a mean rate). It starts at 30 spikes per second with a weight of half the default for the integrate-and-fire cell. Select it to change:

- the **kind**: Poisson spikes (a random train) or DC current (a steady push),
- its **weight**, in the unit of the target cell (pA or nS), and
- its **rate** (Hz) or **value** (pA).

Wire it to a neuron the same way as any sensor: drag from the right edge of its card onto the neuron. You can add several. The inspector has a **Remove input** button.

## The sandbox panel

The world panel is replaced by a small panel with three parts.

| Part | What it shows |
|---|---|
| NEUROMODULATORS | Three sliders, **NE** (noradrenaline), **ACh** (acetylcholine) and **DA** (dopamine), from 0 (none) to 1 (strong). They set the global level of each modulator. Cells whose parameters respond to a modulator follow it. Only shown when this build can set them. See [synapses and modulators](synapses-and-modulators.html). |
| ENERGY | The network firing rate in spikes per second, and the ATP it costs. Roughly 1e8 to 1e9 ATP per spike (Attwell and Laughlin 2001; Lennie 2003), so read it as an order of magnitude. |
| Rate bars | The network rate over the last seconds. |

The raster, traces and gauges are the same as in a level. See [how to read the lab](how-to-read-the-lab.html).

## Saving and loading

In the sandbox, **More**, then **Save** writes your circuit as a `.json` file. **Load** reads one. A circuit made in the sandbox is the same plain file the game, the command line and the Python package all read. See [a circuit is data](circuit-is-data.html) and [using your circuit in Python](python.html).

## What to use it for

- **Try a part.** Put a neuron of each model under the same noise input and compare their raster and trace. The parts bin cards show the same thing as thumbnails (an f-I curve and a voltage trace).
- **Build an oscillator.** Two cells with mutual inhibition and adaptation, or a bursting cell, with no task to satisfy.
- **Test an idea before a level.** For example, how many inputs it takes a cell to fire on coincidence.
- **Read the analysis tools on your own circuit.** The motif census, burst analysis and spike statistics all work on a free circuit. The tabs that re-run the circuit on a task (HRV, PRC, bursts, STA) are made for levels and may have nothing to record in the sandbox. See [the lesion lab and analysis tools](lesion-lab-and-analysis.html).
- **Export a circuit to Python.** Build here, save, and run it in PyNEST.

> **Note:** The sandbox has no clock limit and no episode. The run continues until you pause it. The simulation runs in model time at the speed you pick.

## Related pages

- [The parts bin](parts-bin.html)
- [Controls and keys](controls.html)
- [Make it yours](make-it-yours.html)
