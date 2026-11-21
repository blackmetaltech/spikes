---
title: How to read the lab
section: Start here
order: 3
summary: Every panel of the building screen: the bench, sensor and motor cards, the inspector, the play bar, the raster, traces, gauges and the body view.
---

# How to read the lab

The lab is the screen where you build and run a circuit. You reach it with the Build button on a level card, or from the Sandbox. This page names every part of it and says what to read off each.

![The lab on a task, with the parts labelled](img/lab-annotated.png)

## The layout at a glance

On a desktop window the lab has two columns under a bar.

| Area | Where | What it is |
|---|---|---|
| Top bar | Top | Back, the level title, Run or Pause, Reset, Check, Hint, Brief, Test x5, a read-out, and More. |
| Palette | Above the bench | Buttons that add parts: the parts bin, + Excitatory, + Inhibitory, + Noise, Connect. |
| Bench (circuit editor) | Left, large | Your neurons and connections, with sensor cards on the left edge and motor cards on the right edge. |
| Inspector | Left, below the bench | Details of whatever you selected: the brief, a neuron group, a connection, a sensor or a motor. |
| World | Right, top | The body or world the circuit drives, a timeline of events, and the score card. |
| Scopes | Right, bottom | The spike raster, voltage traces, and rate gauges. |

In a narrow window (under 1180 px wide) or on a phone, the same parts sit behind three tabs: Circuit, World and Scopes. The inspector becomes a sheet at the bottom of the screen on a phone. See [settings and performance](settings-and-performance.html) for the layout setting.

## The bench

The bench is drawn like an engineering sheet of graph paper. Everything on it is live: spikes light up cells and travel along connections as small ink beads.

| Thing | How it looks | Meaning |
|---|---|---|
| Neuron group | A round disc with dendrites | One or more neurons of the same model. Sienna means excitatory (its spikes push other cells toward firing). Prussian blue means inhibitory (its spikes push other cells away from firing). |
| Connection (axon) | A line with an arrowhead | An excitatory connection. A flat bar at the end means inhibitory. The beads move at a slowed-down version of the synaptic delay. |
| Sensor card | A tab on the left edge, with a SENSOR chip | An input the level drives: a touch, a salt level, a pulse, a pressure. You wire it to neurons. |
| Motor card | A tab on the right edge, with a MOTOR chip | An output the level reads: the mean firing rate of the neurons you connect. |

The brief's legend chips (EXCITATORY, INHIBITORY, SENSOR, MOTOR) explain the colours.

**Hints and verdicts on the bench.** After a failed run, the part of the circuit the verdict is about glows red. A tier-2 hint outlines a part in amber. See [stars, par and hints](stars-and-hints.html).

### Building on the bench

| To do this | Do this |
|---|---|
| Add a neuron | Press **+ Excitatory** (or **+ Inhibitory**, once unlocked). The new group appears on the bench. |
| Pick the cell model | Press the **Parts** button on the palette to choose the next model, or select a group and use its model stepper. See [the parts bin](parts-bin.html). |
| Move a neuron | Drag its disc from the middle. |
| Connect two neurons | Drag from the rim of the source disc onto the target. Or press **Connect**, then click the source, then the target. |
| Wire a sensor | Drag from the right edge of its card onto a neuron. |
| Wire a motor | Drag from a neuron's rim onto the motor card. |
| Select | Click a disc, a line or a card. |
| Delete | Select it, then press Delete or Backspace, or the Delete button in the inspector. Right-clicking a part also deletes it. |
| Fit the view | More, then **Fit**. |

On a level, the sensor and motor cards of the task cannot be deleted. Cards you add yourself (a noise input) can.

## The inspector

The inspector changes with what you select.

### The brief

With nothing selected the inspector shows the level's brief. It has a TO BUILD part (what to wire), a TO PASS part (the pass rule in plain words), and a RECIPE part. In Guided mode the recipe is printed. In Standard it sits behind **Show recipe**, which costs the third star. In Researcher mode there is none. Press **More...** to read a long brief in full.

The brief also states the star rules: 1 star for a pass, 2 for passing on 5 seeds, 3 for the same with the par number of neurons or fewer.

### A neuron group

| Control | What it does |
|---|---|
| Model stepper | Changes the cell model. A preset stepper next to it picks a ready-made parameter set. |
| **Trace** | Opens (or closes) a voltage trace for this group's first cell. At most four traces are open at once. |
| **E / I** | Flips the group between excitatory and inhibitory. All its outgoing weights change sign. |
| **Delete** | Removes the group. |
| neurons in group | 1, 2, 3, 4, 6, 8, 10, 12 or 20 cells of this model, drawn as one disc. |
| **Advanced** | Shows the model's parameters as sliders with units, plus rows for how the group responds to neuromodulators. See [synapses and modulators](synapses-and-modulators.html). |
| Reset parameters | Returns to the preset. |

Selecting a group also lets you listen: only its spikes click.

### A connection

| Control | What it does |
|---|---|
| weight | The strength, with a slider, a typed box and a unit that matches the target cell (pA for current-based cells, nS for conductance-based). Excitatory is positive and inhibitory negative on the circuit file. |
| delay | How long the spike takes to arrive, in ms. |
| spread | A percentage. Each synapse of the axon gets its own weight, varying by this much around the value. The spread is fixed by the circuit seed, and Test x5 redraws it for every seed. A circuit that only works with identical weights fails. |
| synapse type | Static, or a plastic or depressing type on levels that use them. |
| wiring | For a group of more than one cell onto a group of more than one: every pair, neuron k to k, or random few inputs (in the levels that offer it). A shift stepper offsets neuron k to k plus an offset. |
| E / I and Delete | As above. |

### A sensor or motor

A sensor shows what it is wired to, its input kind (Poisson spikes or DC current, where you can choose), its weight, and a **Remove input** button if you added it. A motor shows what feeds it. On a numbered family of channels (such as `limb0` to `limb7`) there is a **Wire all** button ("Wire <group> (n) to all" on a motor): a group with the same number of cells connects cell k to channel k. See [inputs and outputs](inputs-and-outputs.html).

## The play bar

The bar across the top has these buttons.

| Button | Meaning |
|---|---|
| Back | Leaves the lab. Your circuit is saved automatically for the level. |
| **Run / Pause** | Starts and stops the simulation. The simulation runs in model time (real biology time), at the speed set in More. |
| **Reset** | Restarts the episode with the circuit you have. |
| **Check** | Scores the circuit headless, on the level's own seed, right now. It runs the level's quick episode. It does not need the live run to finish. See [the core loop](core-loop.html). |
| **Hint** | Appears once hints are open. See [stars, par and hints](stars-and-hints.html). |
| **Brief** (or **Case** on a diagnosis) | Brings the brief back into the inspector. |
| **Test x5** | After a pass (always in Researcher mode): re-runs the level on five other random worlds. |
| read-out | A short line of status text on the right. |
| **More** | The menu below. |

### The More menu

| Group | Item | What it does |
|---|---|---|
| Run | Speed | 0.25x, 0.5x, 1x, 2x, 5x, 10x, 20x or 50x. |
| Run | Skip | Jumps to just before the level's next scripted event (a touch, a push, a flash). |
| Run | Fit | Fits the circuit into the bench. |
| Level | Mode | Guided, Standard or Researcher. See [modes](modes.html). |
| Level | Variant | The base task, or one of its challenge variants. |
| Level | Test x5, Results, Reference, Empty bench | Results shows your circuit against generated variants (after a pass). Reference loads the level's reference circuit (after a pass, or in Guided mode). Empty bench starts again. |
| Tools | Lesions | The lesion lab (after a pass). See [the lesion lab](lesion-lab-and-analysis.html). |
| Tools | Analysis | The analysis screen (from the second chapter). |
| Tools | Load, Save | Read or write a circuit file (from the second chapter). |

> **Note:** A circuit that is a copy of a level's reference circuit earns no stars and does not unlock anything. The end card says so. Press Empty bench and build your own.

## The raster

The raster is a scrolling chart of spikes. Each row is one neuron, grouped by population. Each tick is one spike: sienna for an excitatory cell, blue for an inhibitory one. The "now" edge is on the right. The time window is chosen from the cells' own rhythm (about three cycles wide, automatic). Click the window label in the strip's top right corner to cycle through 500 ms, 1 s, 2 s, 4 s and back to automatic.

What to read: a dense block of ticks is a burst. Rows that tick together are in sync. A row that never ticks is silent. In a circuit made only of rate units the raster becomes a heat strip, one row per unit, with colour showing the rate.

## The traces

Up to four membrane-voltage recorders. Each is a pen line on its own strip of chart paper, with a stroke at every spike. The voltage comes from NEST at 1 ms. NEST models reset the voltage at threshold, so the stroke marks the spike itself.

To open one, select a group and press **Trace**. What to read: how far the voltage rises before a spike, how fast it recovers, and whether it sits just under threshold. Click the window label in the strip's top right corner to cycle the window (automatic, 500 ms, 1 s, 2 s, 4 s, 10 s). A level that teaches the phase plane shows a phase-plane view in the world panel instead. See [the Theory wing](campaign-theory.html).

## The gauges

One horizontal gauge per motor channel. A bar fills with hatching up to the channel's rate in Hz, and the number sits at the end. A dash means the level has no reading yet. Gauges for neuromodulator levels (noradrenaline, acetylcholine, dopamine) show the current level as read-only dials in tasks that use them. In the sandbox they are sliders. See [the sandbox](sandbox.html).

## The world and the body view

The world panel draws what your circuit is driving. The bodies are described on [the bodies page](bodies.html): a limb with a muscle and a spindle, a heart with blood pressure, lungs with carbon dioxide, an eye, a gut, a swimming lamprey, a crawling worm, and several special views (a compass dial, a mushroom body, a tuning dial, a pattern grid).

Under the world there may be a **timeline**. It shows the level's scripted events as labelled spans (a push, standing up, exercise, a flash, CO2 rising), and below it one plot per judged trace. Each plot has a shaded band, the range the score accepts. The line is red where it leaves the band. A cursor moves at the model time.

## The score card

The score card sits under the world. It has a needle gauge from 0 to 100 and a number. When a run passes, the number becomes a stamp: PASSED. Below it are a status line, the verdict sentence and the score parts (chips with numbers that name each thing the score is made of). The glowing chip, if any, is the part the verdict is about.

## The end card

At the end of an episode a card appears. It shows Passed or Not passed (or "n of 5 seeds passed" after Test x5), the score, your best, a sentence about what next, the energy cost, and buttons: Run again, Test x5, Results, a Hint button, Next task, Lesion lab, Analysis and Close. On a pass of a level for the first time you also see the FIRST PASS stamp and a toast for the notebook entry.

![The end card after a pass, with stars and the next step](img/end-card.png)

## Related pages

- [The core loop](core-loop.html)
- [Controls and keys](controls.html)
- [The glossary](glossary.html)
