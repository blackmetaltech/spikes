# Reference circuit: Recurrent inhibition (Renshaw loop) (`circuits/m_recurrent.json`)

Micro-puzzle `m_recurrent` (`engine/src/tasks/m_recurrent.cpp`, chapter "spinal", order 109), concept 13 Recurrent inhibition, placed just before `renshaw (110)`
(docs/DESIGN-0.3.md section 3). A fish-tank puzzle: one new idea, a 1-12 neuron circuit, a 2-5 s episode, a dashboard with the readouts above.
Generator: `python3 circuits/tools/gen_micro_b.py m_recurrent`.

## World
3.1 s. `drive` (dc level 1.5, 450 pA) makes the motoneurons fire at about 50 spikes/s. `kick` (dc, the extra input of a descending volley) adds level 1.0 (300 pA) for 1 s from b = 1000 + J ms and level 2.0 (600 pA) for the next second. An unprotected `iaf_psc_alpha` cell goes 50 -> 110 -> 155 spikes/s (+60, +105). Standard noise: 3 % white per slice on both levels. One motor channel, `mn`.

## Reference circuit
```
drive, kick --> MN (4 cells) --> mn
MN --(800 pA)--> RC (2 cells, inhibitory) --(-800 pA)--> MN
```
6 neurons. The Renshaw cells fire only if the loop is strong (the default 80 pA is far too weak: 4 cells at 50 spikes/s give a RC cell about 100 pA of mean drive against a 375 pA rheobase). The task cannot see the wiring, so it measures the effect: the rise of the rate with the kicks. Generator: `python3 circuits/tools/gen_micro_b.py m_recurrent`.

## Score
| window | criterion |
|---|---|
| base 600 ms..b | rate >= 15 spikes/s |
| first kick, last 500 ms | rise over the base rate <= 27 spikes/s (45 % of the unbraked 60; guided x1.35, researcher x0.8) |
| double kick, last 500 ms | rise <= 42 spikes/s (40 % of the unbraked 105) |

Value 100 when all three hold, else 85 x the credit. Any feedback that makes the output itself oppose extra input passes: a feed-forward brake driven by `drive` alone would not stop the `kick`, which is the point of the second sensor.

## Measured (standard mode, reference circuit)
| Seed | base Hz | kick Hz | double kick Hz | rise 1 | rise 2 | Score |
|---|---|---|---|---|---|---|
| 1 | 21 | 34 | 42 | 13 | 21 | 100 |
| 2 | 20 | 34 | 42 | 14 | 22 | 100 |
| 3 | 23 | 34 | 42 | 11 | 19 | 100 |
| 4 | 21 | 34 | 42 | 13 | 21 | 100 |
| 5 | 24 | 34 | 42 | 10 | 18 | 100 |
| 6 | 20 | 34 | 42 | 14 | 22 | 100 |

All six seeds pass (`spike-run --task m_recurrent --seeds 1,2,3,4,5,6`). Guided mode: 3/3 seeds pass; researcher mode: 3/3.

## From an empty bench
Run 1: `drive` and `kick` onto the motoneurons: the first kick adds 60 spikes/s (verdict). Run 2: Renshaw cells at the default weights: the verdict still says the loop is too weak (about 55 spikes/s over base). Run 3: 600 / -600 pA (the brief and tier 3) passes. With 4 Renshaw cells 300 pA is already enough.

Lesions that must fail (tested in `engine/tests/tasks_micro_b_test.cpp`): The Renshaw cells no longer inhibit the motoneurons; the motoneurons no longer drive the Renshaw cells; the loop at the lab's default weight (80 pA).

## Biology
Renshaw B (1941) Influence of discharge of motoneurons upon excitation of neighboring motoneurons. J Neurophysiol 4:167-183. Eccles JC, Fatt P, Koketsu K (1954) Cholinergic and inhibitory synapses in a pathway from motor-axon collaterals to motoneurones. J Physiol 126:524-562. The loop limits the firing rate of motoneurons and acts as a gain control on the output (it is not a gain control on the input: it cannot tell where extra drive came from).

## Default part and recipe (v0.3 fix round)
* **`default_model()`:** `iaf_psc_alpha` - the card the lab's parts picker opens this level on: the cell type the reference circuit and every verdict assume.
* **`recipe()`:** A group of 4 motoneurons (drive and kick onto it at the first-connection weight, 300 pA per unit) and 2 Renshaw cells set to inhibitory. The loop weights of about 600 pA both ways (motoneurons onto Renshaw, Renshaw back at -600) work for these sizes: the weight needed scales inversely with the number of cells that sum. With a single motoneuron and 2 Renshaw cells about 1600 pA both ways is needed.
  The brief keeps the concept; the numbers live here, so the lab can show them by mode (Guided inline, Standard behind SHOW RECIPE at the cost of the third star, Researcher never).
* **Novice path with the lab's defaults** (`tasks_micro_b_test`): first build made of the default part and the default weights, then one change per verdict: **4 runs** (gate: at most 8).
