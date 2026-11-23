---
title: The sandbox
section: Playing
order: 14
summary: The free bench with no task and every part unlocked: what it has, what it lacks and what to use it for.
---

# The sandbox

The sandbox is the lab with no level. It has no brief, score, stars or pass rule, and every part is unlocked. Build anything and read it off the scopes.

Open it from the title screen (**II. Laboratory**) or the campaign map (**Sandbox**; on a phone the button reads **Lab**). Your bench saves as you edit and is there next time.

The editor is the same as in a level: undo and redo, zoom and pan, multi-select, copy and paste, and building from the keyboard or a gamepad. See [Controls and keys](controls.html) and [how to read the lab](how-to-read-the-lab.html).

## What you get

| Feature | In the sandbox |
|---|---|
| Neuron models | Every model in the parts bin that the build can run. See [the parts bin](parts-bin.html). |
| Inhibitory neurons, noise inputs | Always available. |
| Analysis, Load, Save | Always available. |
| Sensors and motors | None. A level provides them. Here you drive cells with noise inputs and read the scopes. |
| Check, Hint, Test x5, Brief, Skip, the mode picker | Not shown. Nothing to score. |
| Lesion lab | Not shown. It needs a level to score against. |
| Empty bench, Restore previous bench | Not shown. They belong to a level's bench. |
| Speed, Fit | In the More menu, as in a level. |
| Solution slots A, B and C | In the More menu. The sandbox has its own three slots, apart from every level's. |

## The noise input

Press **+ Noise** to add an input device: a Poisson spike generator (random spike times at a mean rate). It starts at 30 spikes per second with a weight of half the default for the integrate-and-fire cell. Select it to change:

- the **kind**: Poisson spikes (a random train) or DC current (a steady push),
- its **weight**, in the unit of the target cell (pA or nS), and
- its **rate** (Hz, up to 400) or **value** (pA, up to 1000).

Switching the kind resets the weight to the default for that kind.

Wire it as you would any sensor: drag from the right edge of its card onto the neuron. You can add several. The inspector has a **Remove input** button.

## The sandbox panel

A small panel with four parts replaces the world panel.

| Part | What it shows |
|---|---|
| NEUROMODULATORS | Three sliders, **NE** (noradrenaline), **ACh** (acetylcholine) and **DA** (dopamine), from 0 (none) to 1 (strong). They set each modulator's global level. Cells whose parameters respond to a modulator follow it. Shown only when this build can set them. See [synapses and modulators](synapses-and-modulators.html). |
| ENERGY | The network firing rate in spikes per second, and the ATP it costs. Roughly 1e8 to 1e9 ATP per spike (Attwell and Laughlin 2001; Lennie 2003), so read it as an order of magnitude. |
| Rate bars | Network spikes per 100 ms over the last 4 seconds. |
| Population rates | One bar for each group, its mean rate over the last second in spikes per second per neuron. |

The raster, traces and gauges are the same as in a level. See [how to read the lab](how-to-read-the-lab.html).

## Saving and loading

**More**, then **Save** writes your circuit as a `.json` file. **Load** reads one. The game, the command line and the Python package all read the same plain file. See [a circuit is data](circuit-is-data.html) and [using your circuit in Python](python.html).

More ways to keep or move a circuit, all in the More menu:

- **Save to A, B or C** puts the bench in a slot without asking. **Load A** and so on bring it back (after a confirmation if the bench has work on it). Slots are part of a save export.
- **Copy circuit as JSON** puts the whole circuit on the clipboard. **Paste circuit JSON** replaces the bench with one from the clipboard.
- **Import from a passed level** pastes your own circuit from a passed level (its saved bench or one of its slots) onto the bench as one selected group. Links to sensors and motors the sandbox lacks are dropped, and a note says which.

Loading or pasting checks the file first. A broken circuit never reaches the bench: a red banner gives the reason. [Settings and performance](settings-and-performance.html) covers where the game keeps saves and backups, the save export and import, and the log folder.

## Limits

The game checks a circuit when you load or paste it, and the engine checks it again when it is built. The limits are generous for anything built by hand.

| Limit | Value |
|---|---|
| Neurons in one group | 1 to 400 |
| Neurons in all | 2000 in the game (the engine itself allows 20000) |
| Connections | 20000 projections, 200000 listed edges |
| Inputs or outputs | 500 of each |
| Delay | At most 1000 ms |
| Weight onto a conductance-based cell (`_cond_` models) | At most 500 nS either way |
| Weight onto a current-based cell (`_psc_` and `_curr_` models) | At most 10000 pA either way |
| Weight onto the custom cells (`butera_burster`, `stg_cell`, `sa_node_pacemaker`, `icc_slow_wave`) | At most 2000 nS; a steady current onto one is at most 2000 pA |
| Input rates and currents | At most 100000 Hz and 100000 pA |

What you can see when a limit is hit:

- Loading a file gives "<file> could not be opened: <reason>", for example "more than 20000 neurons in all" (the engine's limit) or "population X: n must be a whole number from 1 to 20000".
- The engine shows a red banner "The engine rejected the circuit:" with the field's path and the allowed range, for example a weight above its ceiling.
- A circuit that goes numerically wrong while running stops with "The simulation stopped:" and the reason. Fix it, then press Reset. A cell never takes the game down.
- In a level, a Check that runs for 60 seconds is stopped with a "Check timed out" toast.

## What to use it for

- **Try a part.** Put a neuron of each model under the same noise input and compare raster and trace. The parts bin cards show the same as thumbnails (an f-I curve and a voltage trace).
- **Build an oscillator.** Two cells with mutual inhibition and adaptation, or a bursting cell, with no task to satisfy.
- **Test an idea before a level.** For example, how many inputs it takes a cell to fire on coincidence.
- **Read the analysis tools on your own circuit.** The motif census and the spike statistics work on a free circuit. The tabs that record a task run (HRV, PRC, bursts, STA) are made for levels and may have nothing to record here. See [the lesion lab and analysis tools](lesion-lab-and-analysis.html).
- **Export a circuit to Python.** Build here, save, and run it in PyNEST.

> **Note:** The sandbox has no clock limit and no episode. The run continues until you pause it, in model time at the speed you pick.

## Related pages

- [The parts bin](parts-bin.html)
- [Controls and keys](controls.html)
- [Settings and performance](settings-and-performance.html)
- [Make it yours](make-it-yours.html)
