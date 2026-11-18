# Micro-puzzle: intrinsic pacemaking (`circuits/m_pacemaking.json`)

Task `m_pacemaking` (engine/src/tasks/m_pacemaking.cpp), concept 9, chapter heart, order 198 (before `m_coupling` at 199 and `heart_pacemaker` at 200). Uses: threshold.
(Concept 9 sits before concept 8 in the campaign: a player must know a cell can beat on its own before two such cells are coupled.)

## The puzzle
No sensors at all, ACh and NE held at 0. Wire a cell to `beat` so that it fires by itself, 50-140 times a minute (per cell), with beat intervals varying by at most 15 %, over 0.5-4 s.

## Reference (1 neuron)
```
N1 (sa_node_pacemaker) --> beat
```
103 beats per minute (interval 580 ms), CV 0.15 %.

## Why it works
Only an intrinsic oscillator passes. A plain `iaf_psc_alpha` cell is silent at 0 pA; with a bias current over its 375 pA rheobase it fires at 22 Hz or more (380 pA: 1320 beats per minute);
`izhikevich` with a bias of 4 pA fires at 429 per minute. The SA node cell fires at 103 per minute with no wiring. The lab opens a heart task on the SA card (its default part comes from the
reference circuit), so the first build usually passes.

## Novice path
| run | build | result |
|---|---|---|
| 1 | the SA node cell the lab opens the level on, on `beat` | pass |

**1 run** with the lab's default part. A player who swaps in a plain cell first: it stays silent ("a pacemaker cell fires by itself"), with a 380 pA bias it fires at 1320 per minute ("far too fast"), then the SA node cell passes: 3 runs. Several uncoupled SA cells on `beat` fail on irregularity (merged trains).

## Modes
Rate 40-160 / 50-140 / 50-140 per minute; CV <= 25 % / 15 % / 10 %. There is no stimulus, so no noise.

## What breaks it
Cell silenced; the SA cell replaced by a plain integrate-and-fire cell.

Hints: (1) pacemaking is rhythmic firing with no input; (2) the SA node pacemaker card; (3) one SA cell on `beat`, nothing else.
Metric: `interval_cv_pct`.

Credits: DiFrancesco D (1993) Annu Rev Physiol 55:455; Jose AD, Collison D (1970) Cardiovasc Res 4:160.

## Default part and recipe (v0.3 fix round)
* **`default_model()`:** `sa_node_pacemaker` - the card the lab's parts picker opens this level on: the cell type the reference circuit and every verdict assume.
* **`recipe()`:** One SA node cell wired to 'beat', nothing else: it beats about 100 times a minute on its own. No sensor, no input weight, no bias current.
  The brief keeps the concept; the numbers live here, so the lab can show them by mode (Guided inline, Standard behind SHOW RECIPE at the cost of the third star, Researcher never).
* **Novice path with the lab's defaults** (`tasks_micro_a_test`): first build made of the default part and the default weights, then one change per verdict: **1 run** (gate: at most 8).

## Honesty contract (v0.4)
v0.4: the lab now opens on a plain integrate-and-fire cell (`default_model`), which stays silent: finding the SA node card in the parts bin is the puzzle. Mechanism check: no bias current (I_e) on the cell and no projection onto it
(a driven cell is not a pacemaker). Cheap rows: plain cell, plain cell + I_e 380, SA cell pushed by another cell, adaptive cell + bias, an SA cell with a bias current. (The reference has no weights, so "weights 0 / extreme" is the reference itself and is not a row.)
