# Reference circuit: Reciprocal inhibition (`circuits/m_reciprocal.json`)

Micro-puzzle `m_reciprocal` (`engine/src/tasks/m_reciprocal.cpp`, chapter "spinal", order 99), concept 12 Reciprocal inhibition, placed just before `stretch_reflex (100)`
(docs/DESIGN-0.3.md section 3). A fish-tank puzzle: one new idea, a 1-12 neuron circuit, a 2-5 s episode, a dashboard with the readouts above.
Generator: `python3 circuits/tools/gen_micro_b.py m_reciprocal`.

## World
3.2 s. `tone` (dc level 1.4, 420 pA at the lab's 300 pA per unit) holds both motor pools just above rheobase (about 40 spikes/s). The flexor is stretched on [800 + J, 1800 + J) ms (J = U(0, 100) ms from the seed): the spindle afferent `ia_flexor` (dc level 2.0) switches on. `flexor` and `extensor` are the pools' rates; the dashboard shows a joint angle of 0.6 deg per Hz of difference.

## Reference circuit
```
tone      --> MN_F (2) --> flexor            tone --> MN_E (2) --> extensor
ia_flexor --> MN_F (2)        (monosynaptic stretch reflex)
ia_flexor --> IN (2, inhibitory) --(-160 pA)--> MN_E        (Ia inhibitory interneuron)
```
All cells `iaf_psc_alpha`, 6 neurons. The extensor has only 45 pA above rheobase, so one cell at the default -80 pA cuts its rate to 70 % and 2 cells silence it. Generator: `python3 circuits/tools/gen_micro_b.py m_reciprocal`.

## Score
| window | criterion |
|---|---|
| baseline 300-700 ms | flexor and extensor >= 8 spikes/s (both hold tone) |
| stretch 1000+J..1800+J | flexor >= baseline + 15 spikes/s; extensor <= 0.3 x baseline (guided 0.4, researcher 0.24) |
| recovery 2200+J..3200 | extensor >= 0.6 x baseline |

Value 100 when all hold, else 85 x the credit of the four.

## Measured (standard mode, reference circuit)
| Seed | flexor rest Hz | flexor stretch Hz | extensor rest Hz | extensor stretch Hz | extensor after Hz | Score |
|---|---|---|---|---|---|---|
| 1 | 40 | 151 | 40 | 0 | 41 | 100 |
| 2 | 41 | 151 | 41 | 0 | 42 | 100 |
| 3 | 41 | 151 | 41 | 0 | 41 | 100 |
| 4 | 41 | 151 | 41 | 0 | 41 | 100 |
| 5 | 40 | 151 | 40 | 0 | 41 | 100 |
| 6 | 41 | 151 | 41 | 0 | 41 | 100 |

All six seeds pass (`spike-run --task m_reciprocal --seeds 1,2,3,4,5,6`). Guided mode: 3/3 seeds pass; researcher mode: 3/3.

## From an empty bench
Run 1: tone onto both pools, `ia_flexor` onto the flexor only: the extensor does not relax (verdict). Run 2: an interneuron group set to inhibitory between `ia_flexor` and the extensor; one cell at -80 pA gets the extensor down only to 70 %. Run 3: 2 cells (or -160 pA) passes.

Lesions that must fail (tested in `engine/tests/tasks_micro_b_test.cpp`): The interneuron cut from the extensor (no relaxation); `ia_flexor` cut from its own motoneurons (no stretch reflex).

## Biology
Sherrington CS (1906) The Integrative Action of the Nervous System: reciprocal innervation, the antagonist relaxes when the agonist is excited. Lloyd DPC (1946) Integrative pattern of excitation and inhibition in two-neuron reflex arcs. J Neurophysiol 9:439-444 (the disynaptic inhibition of antagonist motoneurons by group Ia afferents).

## Default part and recipe (v0.3 fix round)
* **`default_model()`:** `iaf_psc_alpha` - the card the lab's parts picker opens this level on: the cell type the reference circuit and every verdict assume.
* **`recipe()`:** tone onto a flexor group and an extensor group (2 cells each; the first connection sets 300 pA per unit, which is right). ia_flexor onto the flexor group (the stretch adds 600 pA) and onto an interneuron group of 2 cells set to inhibitory, which projects to the extensor group at the default weight (-80 pA); raise it to about -160 pA if the extensor does not fall silent.
  The brief keeps the concept; the numbers live here, so the lab can show them by mode (Guided inline, Standard behind SHOW RECIPE at the cost of the third star, Researcher never).
* **Novice path with the lab's defaults** (`tasks_micro_b_test`): first build made of the default part and the default weights, then one change per verdict: **2 runs** (gate: at most 8).

## v0.4: the honesty contract (docs/REQ-0.4.md)
- **Mechanism check.** Before the behaviour, the task reads the circuit structure (`Engine::structure()`, captured when the run starts) and requires an inhibitory interneuron (a cell that is neither motor pool) on the path from `ia_flexor` to the extensor pool (a path of net inhibitory sign), and `ia_flexor` also excites the flexor pool. A circuit that lacks it fails with a verdict naming the missing mechanism, and its value is capped at 40.
- **No numbers in verdicts or hint tiers 1-3.** Failed verdicts and the three hint tiers describe the observed behaviour and the direction to change; the numbers are only in `recipe()`. Pass messages still report observed values.
- **Negative tests.** `engine/tests/cheap_rows_b.inc` (run by `tasks_cheap_test`): empty bench, one cell with the lab's first-connect weights, one tonic cell with every weight 0, the reference with every weight 0, the reference with every weight at an extreme, and the reference minus the taught mechanism, each must fail with the mechanism named.
- **Non-circular novice.** Brief and lab defaults only, at most 8 runs: solved in 1 run (the default inhibitory weight is enough once the interneuron is set to inhibitory).
