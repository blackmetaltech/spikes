# Reference circuit: Travelling wave and polarity (`circuits/m_wave.json`)

Micro-puzzle `m_wave` (`engine/src/tasks/m_wave.cpp`, chapter "autonomic", order 409), concept 18 Travelling waves and polarity, placed just before `peristalsis (410)`
(docs/DESIGN-0.3.md section 3). A fish-tank puzzle: one new idea, a 1-12 neuron circuit, a 2-5 s episode, a dashboard with the readouts above.
Generator: `python3 circuits/tools/gen_micro_b.py m_wave`.

## World
3.2 s. A tube of three segments (0 oral, 1, 2 aboral), each with a motor pool `seg0`, `seg1`, `seg2`. `tone` (dc 1.4 = 420 pA) holds each about 40 spikes/s. A bolus presses segment 1 on [700 + J, 1500 + J) ms (sensor `dist1`, dc level 2.0), then segment 2 on [1700 + J, 2500 + J) (`dist2`).

## Reference circuit
```
tone  --> SEG0, SEG1, SEG2 (2 cells each)       seg0, seg1, seg2 read out
dist1 --> SEG0                                  (ascending excitation)
dist1 --> IN (2, inhibitory) --(-160 pA)--> SEG2 (descending inhibition)
dist2 --> SEG1
```
8 neurons. Generator: `python3 circuits/tools/gen_micro_b.py m_wave`.

## Score
| criterion | limit |
|---|---|
| tone | all three segments >= 8 spikes/s at rest |
| ascending | bolus at 1: seg0 >= tone + 15 spikes/s |
| descending | bolus at 1: seg2 <= 0.3 x its tone (guided 0.4, researcher 0.24) |
| travel | bolus at 2: seg1 >= tone + 15 and seg0 <= tone + 8 |
| recovery | after 2700 + J ms every segment within 0.6-1.35 of its tone |

Value 100 when all five hold, else 85 x the credit.

## Measured (standard mode, reference circuit)
| Seed | seg0 tone | seg0 bolus 1 | seg2 tone | seg2 bolus 1 | seg1 bolus 2 | Score |
|---|---|---|---|---|---|---|
| 1 | 40 | 152 | 40 | 0 | 152 | 100 |
| 2 | 41 | 152 | 40 | 0 | 152 | 100 |
| 3 | 40 | 150 | 41 | 0 | 151 | 100 |
| 4 | 41 | 151 | 43 | 0 | 152 | 100 |
| 5 | 40 | 150 | 43 | 0 | 150 | 100 |
| 6 | 41 | 151 | 41 | 0 | 151 | 100 |

All six seeds pass (`spike-run --task m_wave --seeds 1,2,3,4,5,6`). Guided mode: 3/3 seeds pass; researcher mode: 3/3.

## From an empty bench
Run 1: tone onto the three segments, `dist1` onto seg0, `dist2` onto seg1: seg2 does not relax (verdict: inhibition must run down the tube). Run 2: one inhibitory cell at the default weight takes seg2 only to 70 %. Run 3: 2 cells (or -160 pA) passes.

Lesions that must fail (tested in `engine/tests/tasks_micro_b_test.cpp`): The inhibitory cells cut from seg2; `dist1` no longer reaches seg0 (the segment above the bolus does not contract).

## Biology
Bayliss WM, Starling EH (1899) The movements and innervation of the small intestine. J Physiol 24:99-143: "the law of the intestine", contraction above a stimulated point and inhibition below it. The enteric nervous system implements it with ascending excitatory (cholinergic) and descending inhibitory (nitrergic) interneurons (Furness JB (2006) The Enteric Nervous System, Blackwell). The big task `peristalsis` has the same chains with 8 segments.

## Default part and recipe (v0.3 fix round)
* **`default_model()`:** `iaf_psc_alpha` - the card the lab's parts picker opens this level on: the cell type the reference circuit and every verdict assume.
* **`recipe()`:** tone onto 3 groups (seg0, seg1, seg2; the first connection sets 300 pA per unit, which is right). dist1 onto seg0 and onto an inhibitory group (2 cells) that projects onto seg2 at about -160 pA; dist2 onto seg1.
  The brief keeps the concept; the numbers live here, so the lab can show them by mode (Guided inline, Standard behind SHOW RECIPE at the cost of the third star, Researcher never).
* **Novice path with the lab's defaults** (`tasks_micro_b_test`): first build made of the default part and the default weights, then one change per verdict: **2 runs** (gate: at most 8).

## v0.4: the honesty contract (docs/REQ-0.4.md)
- **Mechanism check.** Before the behaviour, the task reads the circuit structure (`Engine::structure()`, captured when the run starts) and requires ascending excitation (`dist1` excites the group read out on seg0) and descending inhibition (`dist1` reaches seg2 through an inhibitory interneuron). A circuit that lacks it fails with a verdict naming the missing mechanism, and its value is capped at 40.
- **No numbers in verdicts or hint tiers 1-3.** Failed verdicts and the three hint tiers describe the observed behaviour and the direction to change; the numbers are only in `recipe()`. Pass messages still report observed values.
- **Negative tests.** `engine/tests/cheap_rows_b.inc` (run by `tasks_cheap_test`): empty bench, one cell with the lab's first-connect weights, one tonic cell with every weight 0, the reference with every weight 0, the reference with every weight at an extreme, and the reference minus the taught mechanism, each must fail with the mechanism named.
- **Non-circular novice.** Brief and lab defaults only, at most 8 runs: solved in 1 run (the default inhibitory weight is enough).
