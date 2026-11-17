# Reference circuit: Feed-forward plus feedback (`circuits/m_feedforward.json`)

Micro-puzzle `m_feedforward` (`engine/src/tasks/m_feedforward.cpp`, chapter "heart", order 207), concept 15 Feed-forward plus feedback, placed just before `heart_autonomic (210)`
(docs/DESIGN-0.3.md section 3). A fish-tank puzzle: one new idea, a 1-12 neuron circuit, a 2-5 s episode, a dashboard with the readouts above.
Generator: `python3 circuits/tools/gen_micro_b.py m_feedforward`.

## World
4.9 s, a plain-C++ plant like `m_negative_feedback` but with a slow effector: `tau_p dy/dt = u - y - d(t)`, tau_p = 250 ms, u the pool `effort`'s rate over 80 Hz low-passed with tau_e = 200 ms. A workload d = 0.5 pulls y down on [b + 250, b + 1300) with b = 1100 + J ms, announced by the sensor `cue` from b (a quarter second ahead; dc level 0.4, 120 pA); a second workload on [3500 + J, 4700 + J) has no cue. `tone` (dc 2.0) and `level` (3 x y, delayed 60 ms, 2 % noise) as before.

## Reference circuit
```
tone  --> EFF (2 cells) --> effort
cue   --(450 pA/unit)--> EFF                 (feed-forward: raises the effort before the workload)
level --> INH (3 cells, inhibitory) --(-300 pA)--> EFF      (feedback)
```
5 neurons. Feedback alone lets y fall by about 0.30 before the slow effector catches up; the cue's extra drive (+0.2 to +0.45 of effort) cuts the dip to 0.12-0.17. Feed-forward alone cannot hold the level at all (the baseline is 0.97): feedback is needed for the unannounced workload. Generator: `python3 circuits/tools/gen_micro_b.py m_feedforward`.

## Score
| criterion | limit (standard) |
|---|---|
| baseline [800, b] | abs(y - 0.5) <= 0.10 |
| warned workload | peak abs(y - 0.5) over [b, b + 1300] <= 0.23 (feedback alone: 0.28-0.34) |
| warned, end | mean y over the last 300 ms within 0.10 of 0.5 |
| unwarned, end | mean y over [4200 + J, 4700 + J] within 0.10 of 0.5 |

Guided widens the bands (band 0.112, peak 0.31), researcher tightens them (band 0.093, peak 0.18). Value 100 when all four hold, else 85 x the credit.

## Measured (standard mode, reference circuit)
| Seed | baseline | warned dip | warned end | unwarned end | Score |
|---|---|---|---|---|---|
| 1 | 0.46 | 0.16 | 0.50 | 0.42 | 100 |
| 2 | 0.45 | 0.12 | 0.47 | 0.42 | 100 |
| 3 | 0.43 | 0.17 | 0.49 | 0.42 | 100 |
| 4 | 0.44 | 0.15 | 0.50 | 0.42 | 100 |
| 5 | 0.44 | 0.15 | 0.49 | 0.42 | 100 |
| 6 | 0.44 | 0.14 | 0.47 | 0.42 | 100 |

All six seeds pass (`spike-run --task m_feedforward --seeds 1,2,3,4,5,6`). Guided mode: 3/3 seeds pass; researcher mode: 3/3.

## From an empty bench
Run 1: feedback only (cue not wired, so the lab blanks the pass; with `cue` parked on any cell the verdict is `the announced workload still pulls the level 0.3 away`). Run 2: `cue` onto the effort cells at the first-connect 300 pA with the default feedback weight: the baseline is still off. Run 3: feedback at -300 pA from 3 cells (brief, tier 3) passes.

Lesions that must fail (tested in `engine/tests/tasks_micro_b_test.cpp`): The feedback cut (feed-forward only: the level never settles); the cue cut (feedback only: the dip of the announced workload is 0.3).

## Biology
Krogh A, Lindhard J (1913) The regulation of respiration and circulation during the initial stages of muscular work. J Physiol 47:112-136: the cardiovascular and respiratory response starts within seconds of exercise, too fast for chemical feedback, so a command from the motor system drives it (central command); reflex feedback then trims it. The tank's plant and time constants are game values.

## Default part and recipe (v0.3 fix round)
* **`default_model()`:** `iaf_psc_alpha` - the card the lab's parts picker opens this level on: the cell type the reference circuit and every verdict assume.
* **`recipe()`:** tone onto 2 effort cells. level onto 3 inhibitory cells that project onto the effort cells at about -300 pA (the sensor gives 900 pA per unit of level). cue onto the effort cells (the first connection's weight of 300 pA per unit is right, 150 is too little).
  The brief keeps the concept; the numbers live here, so the lab can show them by mode (Guided inline, Standard behind SHOW RECIPE at the cost of the third star, Researcher never).
* **Novice path with the lab's defaults** (`tasks_micro_b_test`): first build made of the default part and the default weights, then one change per verdict: **2 runs** (gate: at most 8).
