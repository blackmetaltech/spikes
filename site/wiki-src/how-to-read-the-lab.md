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
| Top bar | Top | Back, the level title, Run or Pause, Reset, Check, Hint, Brief, Test x5, Undo, Redo, a read-out, and More. Hint shows once hints are open, Test x5 once the level is passed (always in Researcher mode). |
| Palette | Above the bench | Buttons that add parts: the parts bin, + Excitatory, + Inhibitory, + Noise, Connect. |
| Bench (circuit editor) | Left, large | Your neurons and connections, with sensor cards on the left edge and motor cards on the right edge. |
| Inspector | Left, below the bench | Details of whatever you selected: the brief, a neuron group, a connection, a sensor or a motor. |
| World | Right, top | The body or world the circuit drives, a timeline of events, and the score card. |
| Scopes | Right, bottom | The spike raster, voltage traces, and rate gauges. |

In a narrow window (under 1180 px wide) or on a phone, the same parts sit behind three tabs: Circuit, World and Scopes. On a phone the inspector becomes a sheet at the bottom of the screen, Undo and Redo move to the palette row, and the More button reads Menu and also holds the status line, the parts bin, Brief and Hint. See [settings and performance](settings-and-performance.html) for the layout setting.

## The bench

The bench is drawn like an engineering sheet of graph paper. Everything on it is live: spikes light up cells and travel along connections as small ink beads.

| Thing | How it looks | Meaning |
|---|---|---|
| Neuron group | A round disc with dendrites | One or more neurons of the same model. Sienna means excitatory (its spikes push other cells toward firing). Prussian blue means inhibitory (its spikes push other cells away from firing). |
| Connection (axon) | A line with an arrowhead | An excitatory connection. A flat bar at the end means inhibitory. The beads move at a slowed-down version of the synaptic delay. |
| Sensor card | A tab on the left edge, with a SENSOR chip | An input the level drives: a touch, a salt level, a pulse, a pressure. You wire it to neurons. |
| Motor card | A tab on the right edge, with a MOTOR chip | An output the level reads: the mean firing rate of the neurons you connect. |
| Ensemble (large group) | A stained ellipse with the name and the count (for example KC ×200), a ring of sample cells on its rim | A group of more than twelve neurons. The ring shows a sample of its cells, which flash as they fire; the stain darkens with the group's firing. |
| Bus card | One card for a run of four or more numbered channels of one kind (cue0 to cue15 become "cue ×16"), with a small comb of bars | The bars are the channels' activity. Drag from the card's edge to a group to wire every channel to it (the same works from a group to a motor bus). Click the card to open it into one card per channel; click the head tab to close it again. |
| Zoom readout | A small box at the bottom left, such as 85% | How far the bench is zoomed. Click it to fit the circuit. A scale bar at the bottom right keeps its length true at every zoom. |

The brief's legend chips (EXCITATORY, INHIBITORY, SENSOR, MOTOR) explain the colours.

**Hints and verdicts on the bench.** After a failed run, the part of the circuit the verdict is about glows red. A tier-2 hint puts a pulsing outline on a part. See [stars, par and hints](stars-and-hints.html).

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
| Delete | Select it, then press Delete or Backspace, or the Delete button in the inspector. Right-clicking a part also deletes it. Undo brings it back. |
| Fit the view | Press F, click the zoom readout, or More, then **Fit**. |
| Zoom and pan | The wheel (or a pinch) zooms about the pointer, from 15% to 300%. Middle-drag, Space-drag or a finger drag on the empty bench pans. |
| Select several | Shift+click groups, or drag a box over the empty bench. Drag one of them to move them all; Delete deletes them all. |
| Copy and paste | Ctrl+C copies the selected groups with the connections between them; Ctrl+V pastes them where the pointer is, selected, as one Undo step; Ctrl+D duplicates. Links to sensor and motor cards are not copied (Import from a passed level brings them). |
| Undo a change | **Undo** on the play bar or Ctrl+Z; **Redo** or Ctrl+Shift+Z. Each add, wire, delete, slider change, drag, paste or loaded circuit is one step, up to 100. |
| Work without a mouse | Click the bench (or Tab onto it): a double-ring cursor walks the cards, groups and connections with Tab or the arrow keys (the D-pad on a gamepad). Enter (A) selects, W (X) starts a wire, Enter (A) on the target finishes it, Esc (B) cancels it, I (Y) opens the inspector, Shift+arrows nudge a group, and Delete removes the selection. See [keyboard and controls](controls.html). |

On a big circuit the bench opens fitted to show every group and its name. If the window is too small for the circuit and its cards, the groups are kept in view and the cards may be a pan away.

On a level, the sensor and motor cards of the task cannot be deleted. Cards you add yourself (a noise input) can. On a diagnosis case the bench is read-only.

## The inspector

The inspector changes with what you select.

### The brief

With nothing selected the inspector shows the level's brief. A long brief is cut short, and two parts of it stay in view: TO BUILD (the sentences that say what to wire) and TO PASS (the pass rule in plain words). Press **More...** in the brief to read it in full. Below them comes a RECIPE part. In Guided mode the recipe is printed. In Standard it sits behind **Show recipe**, which costs the third star. In Researcher mode there is none. The colour chips (EXCITATORY, INHIBITORY, SENSOR, MOTOR) explain the bench.

What each star needs is on the level's card on the campaign map (see [stars, par and hints](stars-and-hints.html)).

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

A sensor shows what it is wired to, its input kind (Poisson spikes or DC current, where you can choose), its weight, and a **Remove input** button if you added it. A motor shows what feeds it, and its readout kind where the level offers a choice (rate, modulator or ring). **Disconnect all** removes every connection of a card. On a numbered family of channels (such as `limb0` to `limb7`) there is a **Wire all** button ("Wire <group> (n) to all" on a motor): a group with the same number of cells connects cell k to channel k. See [inputs and outputs](inputs-and-outputs.html).

## The play bar

The bar across the top has these buttons.

| Button | Meaning |
|---|---|
| Back | Leaves the lab. Your circuit is saved automatically for the level. |
| **Run / Pause** | Starts and stops the simulation, at the speed set in More. The lab starts running when it opens. When an episode has finished, Run plays it again. |
| **Reset** | Restarts the episode with the circuit you have. |
| **Undo / Redo** | Takes back the last change to the circuit, or puts it back (Ctrl+Z, Ctrl+Shift+Z). Grey when there is nothing to undo. On a phone they sit on the palette row. |
| **Check** | Scores the circuit headless on five random worlds (seeds 101 to 105), in the background, using the level's quick episode. It does not need the live run to finish, and its button reads "Checking..." meanwhile. Not on a diagnosis case. See [the core loop](core-loop.html). |
| **Hint** | Appears once hints are open: at once in Guided mode, after two missed runs in Standard, never in Researcher. See [stars, par and hints](stars-and-hints.html). |
| **Brief** (or **Case** on a diagnosis) | Brings the brief back into the inspector. |
| **Test x5** | After a pass (always in Researcher mode): re-runs the full episode on the same five worlds as Check, with a progress card. |
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
| Solution slots | Save to A, B, C; Load A, B, C | Three saved circuits per level, kept across restarts. A line under the buttons says what each slot holds ("A: 7 neurons   B: empty"). Save overwrites the slot without asking. Load appears for a filled slot, and asks first when the bench has work on it; the bench it replaces is kept in the bench history. A slot made from the reference circuit stays marked as one. |
| Level | Test x5, Results, Reference, Empty bench, Restore previous bench | Results shows your circuit against generated variants (after a pass). Reference loads the level's reference circuit (after a pass, or in Guided mode). Empty bench starts again. Restore previous bench appears once the bench history holds something (see below). Reference, Empty bench, Load and a slot ask first when the bench has work on it. |
| Tools | Lesions | The lesion lab (after a pass); reads Leave lesions while it is open. See [the lesion lab](lesion-lab-and-analysis.html). |
| Tools | Analysis | The analysis screen (from the second chapter). |
| Tools | Load, Save | Read or write a circuit file (from the second chapter). |
| Tools | Copy circuit as JSON, Paste circuit JSON | Put the whole circuit on the clipboard, or replace the bench with a circuit from the clipboard (it asks first, keeps your bench in the history, and refuses a circuit that fails the checks below). |
| Tools | Import from a passed level | Lists your own saved bench and slots of every level you have passed (benches built from a reference are left out, and the list says how many). The pick is pasted as one selected group, with its links to sensors and motors that this level also has; links to ones it does not have are dropped, and a note names them. Esc closes the list. |

On a level the More menu ends with a line about what opens later ("More tools open as the campaign needs them: ..."). Load, Save, the JSON copy and paste, Import and Analysis open at the Spinal cord chapter; inhibitory neurons, noise inputs and the parts bin open at the first level whose reference needs them. The [sandbox](sandbox.html) has everything.

> **Note:** A circuit that is a copy of a level's reference circuit earns no stars and does not unlock anything. The end card says so. Press Empty bench and build your own.

### Undo, redo and the bench history

There are two safety nets.

- **Undo and Redo** (the play bar, Ctrl+Z, Ctrl+Shift+Z or Ctrl+Y) step through your edits on the bench. Each add, wire, delete, slider change, drag, paste or loaded circuit is one step, and 100 steps are kept.
- **Restore previous bench** (More, Level group) keeps you from losing a circuit to a button. Whenever Reference, Empty bench, Load, a slot, Paste circuit JSON or the tutorial replaces a bench that has neurons, the old bench is saved into the level's history (`circuits/<level>/history`, the newest ten). Restore brings the newest one back and puts the bench you had into the history in its place, so pressing it twice swaps back. A bench identical to the newest entry, an empty bench and a copy of the reference are not kept. The history is per level, and not offered on a diagnosis case.

## The raster

The raster is a scrolling chart of spikes. Each row is one neuron, grouped by population. Each tick is one spike: sienna for an excitatory cell, blue for an inhibitory one. The "now" edge is on the right. The time window is chosen from the cells' own rhythm (about three cycles wide, automatic). Click the window label in the strip's top right corner to cycle through 500 ms, 1 s, 2 s, 4 s and back to automatic.

What to read: a dense block of ticks is a burst. Rows that tick together are in sync. A row that never ticks is silent. Each group keeps one label in a fixed column at the left and at most twelve rows (a larger group is sampled evenly); above 40 cells or 8 groups the raster draws one rate-heat row per group instead. In a circuit made only of rate units the raster becomes a heat strip, one row per unit, with colour showing the rate.

## The traces

Up to four membrane-voltage recorders. Each is a pen line on its own strip of chart paper, with a stroke at every spike. The voltage comes from NEST at 1 ms. NEST models reset the voltage at threshold, so the stroke marks the spike itself.

To open one, select a group and press **Trace** (a fifth trace replaces the oldest). What to read: how far the voltage rises before a spike, how fast it recovers, and whether it sits just under threshold. The voltage axis is held to -90 to +40 mV; a model with no voltage floor that sinks below it is clipped and the strip says so. Click the window label in the strip's top right corner to cycle the window (automatic, 500 ms, 1 s, 2 s, 4 s, 10 s). A level that teaches the phase plane shows a phase-plane view in the world panel instead. See [the Theory wing](campaign-theory.html).

## The gauges

One horizontal gauge per motor channel. A bar fills with hatching up to the channel's rate in Hz, and the number sits at the end. The value is a 500 ms mean, and once the episode has ended it is the mean over the whole run. A pale hatched rail that reads "measuring..." means the level has no value yet. Gauges for neuromodulator levels (noradrenaline, acetylcholine, dopamine) show the current level as read-only dials in tasks that use them. In the sandbox they are sliders. See [the sandbox](sandbox.html).

## The world and the body view

The world panel draws what your circuit is driving. The bodies are described on [the bodies page](bodies.html): a limb with a muscle and a spindle, a heart with blood pressure, lungs with carbon dioxide, an eye, a gut, a swimming lamprey, and several special views (a phase plane, a tuning dial, a pattern grid, rate traces). Every Bestiary level, and its diagnosis case, shows its animal as a **specimen**: an engraving with live parts drawn on it from your running circuit only. The worm crawls forward or backs up, the leech's two heart tubes beat with a pen record of the bursts, the sea slug's gill withdraws with the motor cell, the crab ganglion lights its cells above a record of the three-phase rhythm, the fly's mushroom body shows its Kenyon cells as a field of points, and the compass is a ring of sixteen wedges with the bump as a needle. Nothing on a specimen moves unless the simulation moves it. In a short window the gauges stay and the captions drop.

Under the world there may be a **timeline**. It shows the level's scripted events as labelled spans (a push, standing up, exercise, a flash, CO2 rising), and below it one plot per judged trace. Each plot has a shaded band, the range the score accepts. The line is red where it leaves the band. A cursor moves at the model time.

## The score card

The score card sits under the world. It reads like a rubber stamp.

- A ring holds the score, from 0 to 100, with the word SCORE, in red while the level is not passed. When the run passes, the ring turns green and says PASSED. A run that has not passed never reads 100. With nothing built it shows a dash and asks you to add neurons.
- A status chip (RUNNING, PAUSED, FINISHED: NOT PASSED, PASSED), the result of your last Check, and your best stars for the level.
- **Criteria rows** below: first the verdict sentence, then one row for each judged trace ("inside the green band" or "outside"), then each pass flag, then "Conditions met: n of m" where the level counts them. A tick means met, a cross means not met, a dot means still being measured. If there are more rows than fit, it says "+N more".
- **Details** (top right of the card) swaps the rows for the raw score parts, each under a human label with its unit, plus the neuron count against the par and the spike rate of the run. **Hide details** swaps back. The part the failed verdict is about has a red outline.

## The end card

At the end of an episode a card appears. It shows a stamp with your score and "of 100" and PASSED or NOT PASSED (or "n of 5 worlds passed" after Test x5), your stars and best score, a sentence about what to do next, and buttons: **Next level** (the main button after a pass), Run again, Test x5, Results, a Hint button (when the run did not pass), Lesion lab, Analysis and Close. The energy cost lives in Results. A reference circuit shows "Reference circuit: practice only" instead of stars. On a pass of a level for the first time you also see the FIRST PASS stamp and a toast for the notebook entry.

![The end card after a pass, with stars and the next step](img/end-card.png)

## When the lab refuses a circuit

A circuit can come from Load, Paste circuit JSON, a slot, an autosave or Import. It is checked before it reaches the bench or the engine. A refusal never changes your bench.

**The game's checks** (shown as a banner such as "The clipboard does not hold a usable circuit: ..." or "Slot A could not be opened: ..."):

| Limit | Value |
|---|---|
| Neurons in one group | 1 to 400, a whole number |
| Neurons in all | 2000 |
| Connections (projections) | 20000, and 200000 individual edges |
| Sensors and motors | 500 each |
| Group name | text, up to 64 characters, no two alike |
| Nesting of the file | 24 levels |
| Numbers | finite (no NaN or infinity); a seed and a time step must be numbers, and the time step positive |

The messages say the first problem in plain words, for example "population E1: n must be a whole number from 1 to 400", "more than 2000 neurons in all", "not a circuit (the file is not JSON, or is cut short)" or "the file is empty". Damaged neuron positions are repaired by laying the groups out again, with a note.

**The engine's checks** (shown as "The engine rejected the circuit: ..." with the place in the file, such as `projections[3].synapse.weight`):

| Limit | Value |
|---|---|
| Synaptic weight onto a conductance-based cell | 500 nS |
| Synaptic weight onto a current-based cell | 10000 pA |
| Synaptic weight onto the special NESTML cells (the bursters and pacemakers) | 2000 nS |
| Constant current (a dc input's weight times its value, or a cell's bias) | 100000 pA; 2000 pA for a bias on a special cell; 1000 pA of dc input onto one |
| Synaptic delay | 1000 ms |
| Poisson or sinusoidal rate | 100000 Hz |
| Gap-junction conductance | 100 nS |
| Neurons in all | 20000 |
| Connections in all (estimated before anything is built) | 2 million |
| Nesting of the file | 32 levels |

A message reads, for example, "weight 600.000000 is outside the allowed range |w| <= 500 nS for aeif_cond_alpha" or "a constant current of ... pA is above the allowed ... pA". The lab's own sliders stop well inside these numbers, so you meet them mostly with a hand-edited file.

**While it runs**, the engine stops a circuit instead of hanging: "the simulation is not keeping up (... s of computing for ... ms of model time): a cell is stalling, usually from a very strong input", or "a membrane potential became NaN or infinite". The lab shows "The simulation stopped: ... Fix the circuit and press Reset." A Check that has not answered after 60 seconds is stopped with "Check timed out". If the game closed or stalled during a live run, that circuit reopens paused with a note.

**Damaged files.** If your saved circuit for a level cannot be opened, it is renamed `last_<level>.json.bad`, the banner names it, and the level opens on an empty bench. See [settings and performance](settings-and-performance.html#saves-backups-and-logs).

## Related pages

- [The core loop](core-loop.html)
- [Controls and keys](controls.html)
- [The glossary](glossary.html)
