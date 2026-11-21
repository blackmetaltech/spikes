---
title: The lesion lab and analysis tools
section: Playing
order: 13
summary: Cut and silence parts of a passed circuit, and read it with motif counts, burst and heart-rate-variability measures, phase response, energy, reverse correlation and the phase plane.
---

# The lesion lab and analysis tools

After you pass a level, Spike gives you tools to take your circuit apart and measure it. They answer two questions: what does each part do, and what kind of circuit is this.

| Tool | Where | Question it answers |
|---|---|---|
| Lesion lab | More, then Lesions (or the end card) | What breaks if this part is removed? |
| Motif census | Analysis, 3-node and 4-node tabs | Which small wiring patterns does my circuit have, more or less than chance? |
| Burst analysis | Analysis, bursts tab | How does this cell group burst? |
| HRV | Analysis, HRV tab | How regular is the beat? |
| PRC | Analysis, PRC tab | How does a push shift the next beat? |
| Energy | Analysis, energy tab | What does the circuit cost in spikes? |
| STA | Analysis, STA tab | What input pattern makes this cell fire? |
| Phase plane | The world panel of the Dynamics levels | Why does a single neuron behave like this? |
| Results | More, Results (after a pass) | How does my circuit compare with generated variants? |

## The lesion lab

A lesion is a removal. The lesion lab lets you remove a part of a passed circuit and see what the circuit loses. This is the experiment real neuroscientists do: damage a region, see which behaviour goes.

**When it opens.** After you pass the level, or when you load the reference circuit, or on a diagnosis case (where the button is LESION LAB). Before that, pressing Lesions shows: "The lesion lab opens after a pass: pass the level, then cut and silence parts to see what each one does."

### Three kinds of lesion

| Lesion | What it does | How |
|---|---|---|
| Cut an axon | Sets that connection to nothing. | Click the axon. |
| Silence a population | The group stops firing. | Click the group. |
| Cut a sensor | Disconnects a sensor card from its targets. | Click the sensor card. |

Click again to restore it. Lesioned parts are marked on the bench. The live view restarts at once on the lesioned circuit, so you see the effect as it happens. A sensor card that is wired to nothing cannot be lesioned. In the lesion lab the bench is not editable; with the keyboard, the bench cursor's Enter cuts or silences.

### Re-running on seeds

The live view shows one run. To measure, use the buttons.

| Button | What it does |
|---|---|
| **Run lesion** | Re-runs the whole episode on three seeds (1, 2, 3) with the current lesions. The world and the circuit are reseeded. The table shows the mean score and range. It needs at least one lesion, and a progress line shows which row and seed is running (Cancel stops it). |
| **Sweep all singles** | Tries every single lesion one at a time, and adds a row for each. |
| **Clear set** | Removes all lesion marks. |
| **Exit** | Leaves the lesion lab. |

The table has one row per lesion set, with a PASS or FAIL chip, the verdict, the mean score, its change from the intact circuit, and the range over seeds. The intact circuit is on top, then the most damaging lesion first. Some rows carry a line in amber or grey: what the lesion means in the body, taken from the level's lesion table. A line is amber when the lesion matches the level's biological lesion table exactly.

If the intact circuit itself fails on some seeds, the table says: "The intact circuit fails on some seeds: a few points between rows are noise, not the lesion."

### Export

**Copy Markdown** and **Copy CSV** put the table on the clipboard. **Save files** writes a `.md` and a `.csv` into the `lesions` folder of the game's data folder (see [settings and performance](settings-and-performance.html#saves-backups-and-logs)) and says where.

### A lesion that helps

Sometimes a lesion raises the score. The game shows a toast: "Surprise: a lesion that helps! With <lesion> the score went UP by N points. Something that part does is working against the task: why?" That is the game pointing at a part of your circuit that costs more than it earns.

### Use in diagnosis

On a diagnosis case, a cut tried or a lesion re-run counts as a probe. See [diagnosis cases](diagnosis-cases.html).

### Command line

The same experiment runs headless: `spike-run --task <id> --circuit file.json --lesion pre>post*k` scales every connection from `pre` to `post` by `k`. See [spike-run](spike-run.html).

## The Analysis screen

Open it from More, then **Analysis** (from the Spinal cord chapter on), or from the end card. It has seven tabs, chosen by the **tab** stepper at the top: 3-node, 4-node, HRV, PRC, bursts, energy and STA. Beside the tabs a panel of spike statistics for the live run shows the mean rate, the coefficient of variation of the intervals (ISI CV) and a synchrony measure (Golomb chi). A panel at the bottom shows the population rate for the last 4 seconds in 40 ms bins.

Some tabs need a recording. **RECORD** re-runs the circuit on the task in a separate engine for 10 s, 30 s, 60 s or the full episode, and keeps every spike. The **population** stepper picks which group's spikes are analysed. It starts on the group behind the level's first motor output (the sinoatrial node in the heart levels, the pre-Botzinger cells in the breathing level).

### Motif census (3-node and 4-node)

A motif is a small wiring pattern of a few cells, like "A drives B and C, and B drives C" (a feed-forward loop). Real brain networks have some motifs more often than a random network with the same number of connections per cell. The census counts them in your circuit and compares with random networks.

- The 3-node tab counts the 13 connected classes of three-node patterns (Milo et al. 2002).
- The 4-node tab counts the 199 connected classes of four-node patterns. It lists the ten with the strongest score. If the work estimate is too big it refuses with a message.
- **z-score**: how many standard deviations the real count is from the mean of random networks that keep every cell's in- and out-degree. A z beyond 2 (and a null standard deviation of at least 0.5) is called significant. Sienna means more than chance, slate grey means fewer (blue is kept for inhibition).
- **graph**: the nodes are populations or single neurons. **edges**: all, excitatory only, or inhibitory only. **nulls**: how many random networks, 20, 100 or 500.
- You need at least three (or four) populations for the tab to work.

### Bursts

A burst is a group of spikes close together with silent gaps around it. The tab finds bursts in the chosen population. A burst starts when an interval between spikes drops under 170 ms, ends when one goes over 170 ms, has at least 3 spikes and 10 ms, and is at least 200 ms from the last one. The tab lists the number of bursts, the share of spikes inside bursts, the mean burst duration, spikes per burst, the burst period and the mean interval inside a burst. Use it on the leech, crab and breathing levels, where the rhythm is made of bursts.

### HRV: heart-rate variability

HRV measures how much the time between beats varies from beat to beat. A healthy resting heart is not a metronome. The tab treats each spike of the population as a beat (spikes within 150 ms count as one beat). It reports:

| Measure | Meaning |
|---|---|
| SDNN | The standard deviation of the beat-to-beat intervals. |
| RMSSD | The root mean square of successive differences. It reads fast, beat-to-beat change. |
| pNN50 | The share of successive differences larger than 50 ms. |
| LF/HF | The ratio of slow to fast oscillation in the beat intervals (a Lomb-Scargle spectrum). |

A tachogram (interval against beat number) is drawn. You need at least three beats, and at least nine for the spectrum. The time-domain definitions follow the 1996 Task Force. Use it on the heart levels.

### PRC: phase-response curve

If you push an oscillator at different moments in its cycle, the next beat comes earlier or later by different amounts. The curve of that shift against the phase of the push is the phase-response curve. The tab takes the level's scripted pushes (its timeline events) and plots, for each, the shift in cycles. Positive means the next beat came early (a phase advance). Dots are single pushes, and the line is the mean per phase bin. A task with no scripted push says so: "This task has no scripted perturbation to measure a phase response to." The baroreflex, autonomic heart rate and pupil levels have them.

### Energy

The mean rate per neuron, the level's budget, the number of neurons, the number of spikes, and the ATP cost (about 1e8 to 1e9 ATP per spike; Attwell and Laughlin 2001; Lennie 2003). A bar compares you with the budget. The tab says "Energy: within budget" or, on a level whose challenge has an energy ceiling, "over budget, no third star" (on other levels the bar shows a reference ceiling that earns nothing). A list shows the rate of each population. See [stars, par and hints](stars-and-hints.html).

### STA: spike-triggered average

The tab drives the first cell of the chosen population with Gaussian white-noise current (random, with no pattern) for the length you chose (10 s is usually plenty, at most 20 s). It then averages the current that came before each spike, minus the average of the whole current. The result is the cell's linear filter: a bump just before time zero means the cell responds to the recent past. A grey band shows what chance gives with that many spikes. It needs a spiking cell: a population of rate units is refused. The method is reverse correlation (Chichilnisky 2001). The Theory wing's [tuning and decoding](campaign-theory.html) chapter introduces it.

## The phase plane

In the Dynamics levels of the Theory wing, the world panel draws the **phase plane** of the single cell: its voltage V against its recovery variable (U or w), with the two nullclines (the curves where V or the recovery variable stop changing), the fixed points (where they cross), the reset, and the live trajectory of the cell. It is computed from the cell's live parameters and the current you give it. It holds for a constant current. See [the Theory wing](campaign-theory.html).

## Results: your circuit against variants

After a pass, **Results** opens a card: "Your circuit against generated variants". It plots the neurons, the energy and a quality measure of your circuit against a few dozen to a few hundred generated variants of the reference circuit. You can pin a target to chase. It says how many of the three axes you beat the median on.

> **Warning:** The variants are not other players. A script changed the reference circuit at random and kept every variant that passed on two seeds. They are a rough picture of "where you stand", nothing more. They cannot show a clever solution far from the reference.

## Related pages

- [The core loop](core-loop.html)
- [Diagnosis cases](diagnosis-cases.html)
- [Glossary](glossary.html)
