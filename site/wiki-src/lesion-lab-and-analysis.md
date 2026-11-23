---
title: The lesion lab and analysis tools
section: Playing
order: 13
summary: Cut and silence parts of a passed circuit, and read it with motif counts, burst and heart-rate-variability measures, phase response, energy, reverse correlation and the phase plane.
---

# The lesion lab and analysis tools

After you pass a level, take your circuit apart and measure it. What does each part do, and what kind of circuit is this?

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

A lesion is a removal. Remove a part of a passed circuit and see what it loses. Neuroscientists do the same: damage a region, see which behaviour goes.

**When it opens.** After a pass, on loading the reference circuit, or on a diagnosis case (button: LESION LAB). Before that, Lesions shows: "The lesion lab opens after a pass: pass the level, then cut and silence parts to see what each one does."

### Three kinds of lesion

| Lesion | What it does | How |
|---|---|---|
| Cut an axon | Sets that connection to nothing. | Click the axon. |
| Silence a population | The group stops firing. | Click the group. |
| Cut a sensor | Disconnects a sensor card from its targets. | Click the sensor card. |

Click again to restore. Lesioned parts are marked on the bench, and the live view restarts at once on the lesioned circuit. A sensor card wired to nothing cannot be lesioned. The bench is not editable here. With the keyboard, Enter on the bench cursor cuts or silences.

### Re-running on seeds

The live view shows one run. Use the buttons to measure.

| Button | What it does |
|---|---|
| **Run lesion** | Re-runs the episode on seeds 1, 2 and 3 with the current lesions, reseeding the world and the circuit. The table shows the mean score and range. Needs at least one lesion. A progress line shows the row and seed (Cancel stops it). |
| **Sweep all singles** | Tries each single lesion in turn and adds a row for each. |
| **Clear set** | Removes all lesion marks. |
| **Exit** | Leaves the lesion lab. |

Each row is one lesion set: a PASS or FAIL chip, the verdict, the mean score, its change from the intact circuit, and the range over seeds. The intact circuit is on top, then the most damaging lesion first. Some rows carry an amber or grey line saying what the lesion means in the body, from the level's lesion table. Amber means an exact match with that table.

If the intact circuit itself fails on some seeds, the table says: "The intact circuit fails on some seeds: a few points between rows are noise, not the lesion."

### Export

**Copy Markdown** and **Copy CSV** put the table on the clipboard. **Save files** writes a `.md` and a `.csv` into the `lesions` folder of the game's data folder (see [settings and performance](settings-and-performance.html#saves-backups-and-logs)) and says where.

### A lesion that helps

A lesion can raise the score. You get a toast: "Surprise: a lesion that helps! With <lesion> the score went UP by N points. Something that part does is working against the task: why?" That part costs more than it earns.

### Use in diagnosis

On a diagnosis case, a cut tried or a lesion re-run counts as a probe. See [diagnosis cases](diagnosis-cases.html).

### Command line

The experiment also runs headless: `spike-run --task <id> --circuit file.json --lesion pre>post*k` scales every connection from `pre` to `post` by `k`. See [spike-run](spike-run.html).

## The Analysis screen

Open it from More, then **Analysis** (from the Spinal cord chapter on), or from the end card. The **tab** stepper picks one of seven tabs: 3-node, 4-node, HRV, PRC, bursts, energy and STA. A side panel gives live spike statistics: the mean rate, the coefficient of variation of the intervals (ISI CV) and a synchrony measure (Golomb chi). A bottom panel shows the population rate for the last 4 seconds in 40 ms bins.

Some tabs need a recording. **RECORD** re-runs the circuit in a separate engine for 10 s, 30 s, 60 s or the full episode and keeps every spike. The **population** stepper picks the group to analyse. It starts on the group behind the level's first motor output (the sinoatrial node in the heart levels, the pre-Botzinger cells in the breathing level).

### Motif census (3-node and 4-node)

A motif is a small wiring pattern, like "A drives B and C, and B drives C" (a feed-forward loop). Real brain networks have some motifs more often than a random network with the same number of connections per cell. The census counts them in your circuit and compares with random networks.

- The 3-node tab counts the 13 connected classes of three-node patterns (Milo et al. 2002).
- The 4-node tab counts the 199 connected classes of four-node patterns. It lists the ten with the strongest score. If the work estimate is too big, it refuses and says so.
- **z-score**: the distance, in standard deviations, of the real count from the mean of random networks that keep every cell's in- and out-degree. A z beyond 2 (null standard deviation at least 0.5) is significant. Sienna means more than chance, slate grey means fewer (blue is kept for inhibition).
- **graph**: the nodes are populations or single neurons. **edges**: all, excitatory only, or inhibitory only. **nulls**: how many random networks, 20, 100 or 500.
- The tab needs at least three (or four) populations.

### Bursts

A burst is a group of spikes close together with silent gaps around it. The tab finds them in the chosen population. A burst starts when an interval between spikes drops under 170 ms, ends when one goes over 170 ms, has at least 3 spikes and 10 ms, and is at least 200 ms from the last one. It lists the number of bursts, the share of spikes inside bursts, the mean burst duration, spikes per burst, the burst period and the mean interval inside a burst. Use it on the leech, crab and breathing levels.

### HRV: heart-rate variability

HRV measures how much the time between beats varies. A healthy resting heart is not a metronome. Each spike of the population counts as a beat (spikes within 150 ms are one beat). The tab reports:

| Measure | Meaning |
|---|---|
| SDNN | The standard deviation of the beat-to-beat intervals. |
| RMSSD | The root mean square of successive differences. It reads fast, beat-to-beat change. |
| pNN50 | The share of successive differences larger than 50 ms. |
| LF/HF | The ratio of slow to fast oscillation in the beat intervals (a Lomb-Scargle spectrum). |

It draws a tachogram (interval against beat number). It needs at least three beats, and nine for the spectrum. The time-domain definitions follow the 1996 Task Force. Use it on the heart levels.

### PRC: phase-response curve

Push an oscillator at different moments in its cycle and the next beat shifts by different amounts. That shift against the phase of the push is the phase-response curve. The tab plots the shift in cycles for each of the level's scripted pushes (its timeline events). Positive means the next beat came early (a phase advance). Dots are single pushes. The line is the mean per phase bin. A task with no scripted push says so: "This task has no scripted perturbation to measure a phase response to." The baroreflex, autonomic heart rate and pupil levels have them.

### Energy

The tab shows the mean rate per neuron, the level's budget, the number of neurons, the number of spikes, and the ATP cost (about 1e8 to 1e9 ATP per spike; Attwell and Laughlin 2001; Lennie 2003). A bar compares you with the budget. It says "Energy: within budget" or, on a level whose challenge has an energy ceiling, "over budget, no third star" (on other levels the bar shows a reference ceiling that earns nothing). A list shows the rate of each population. See [stars, par and hints](stars-and-hints.html).

### STA: spike-triggered average

The tab drives the first cell of the chosen population with Gaussian white-noise current (random, no pattern) for the length you chose (10 s is usually plenty, at most 20 s). It averages the current before each spike, minus the average of the whole current. The result is the cell's linear filter: a bump just before time zero means the cell responds to the recent past. A grey band shows what chance gives with that many spikes. It needs a spiking cell, and refuses a population of rate units. The method is reverse correlation (Chichilnisky 2001). The Theory wing's [tuning and decoding](campaign-theory.html) chapter introduces it.

## The phase plane

In the Dynamics levels of the Theory wing, the world panel draws the **phase plane** of the single cell: its voltage V against its recovery variable (U or w). It shows the two nullclines (the curves where V or the recovery variable stop changing), the fixed points (where they cross), the reset, and the cell's live trajectory. It is computed from the cell's live parameters and the current you give it, and holds for a constant current. See [the Theory wing](campaign-theory.html).

## Results: your circuit against variants

After a pass, **Results** opens a card: "Your circuit against generated variants". It plots your neurons, energy and a quality measure against a few dozen to a few hundred generated variants of the reference circuit. Pin a target to chase. The card says on how many of the three axes you beat the median.

> **Warning:** The variants are not other players. A script changed the reference circuit at random and kept every variant that passed on two seeds. They give a rough picture of where you stand, nothing more. They cannot show a clever solution far from the reference.

## Related pages

- [The core loop](core-loop.html)
- [Diagnosis cases](diagnosis-cases.html)
- [Glossary](glossary.html)
