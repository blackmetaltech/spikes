# Micro-puzzle: mutual inhibition and the half-centre (`circuits/m_half_centre.json`)

Task `m_half_centre` (engine/src/tasks/m_half_centre.cpp), concept 7, chapter invertebrates, order 29 (just before `lamprey`). Uses: inhibition, adaptation.

## The puzzle
A tonic dc `drive` (level 2.0, 600 pA at the lab's weight) excites a left and a right cell. Over 1-5 s, in 25 ms bins, the sides must alternate (>= 6 switches between a left bin
and a right bin), fire together in at most 15 % of the active bins and neither side may win (the weaker side >= 25 % of the spikes). At t = 0 the task starts the left cells at
-50 mV and the right ones at -75 mV (the command reaches the left side a little earlier), so identical cells do not start in perfect step on any seed.

## Reference (2 neurons)
```
drive (dc, 300 pA/unit) --> L, R (aeif_cond_alpha, Adapting preset: a 0, b 60, tau_w 100, V_reset -58; both inhibitory)
L --(-8 nS)--> R      R --(-8 nS)--> L      L --> left      R --> right
```
The two sides alternate spike for spike at about 7 Hz (54 switches in 4 s, a cycle of about 150 ms), 0 % overlap.

## Why it works
Mutual inhibition makes each side silence the other, adaptation (b, tau_w) tires the active side so the other can take over. With plain integrate-and-fire cells and strong
inhibition (-1600 pA) the left side wins for good (verdict: "only the left side fires ... use adapting cells"); with excitatory links both fire together (88 % overlap).
Without the start-up asymmetry the reference alternates only on some seeds: identical single cells can lock in step (3 of 6 seeds in a first version of this task).

## Novice path
| run | build | result |
|---|---|---|
| 1 | two aeif cells of the part the lab opens on (NEST default), both left excitatory (+8 nS), drive at the lab's 300 pA | "Nothing fired ... the NEST-default adaptive cell needs about 620 pA" (its rheobase; the drive is 2 x the weight) |
| 2 | drive weight 400 pA | "both sides fire together ... check that both cells are inhibitory" |
| 3 | both set inhibitory (-8 nS) | pass (330-420 pA of drive weight pass 5 of 5 seeds; 450 does not on every seed) |

**3 runs** with the default part. The other routes: the Adapting preset (fires at the lab's own 300 pA) with both links left excitatory then set inhibitory: 2 runs; plain cells with strong inhibition fail with "use adapting cells", then the Adapting preset passes: 2 runs.

## Modes
Switches needed 4 / 6 / 8; overlap <= 25 % / 15 % / 10 %; noise x0 / x1 / x2.

## What breaks it
One inhibitory link cut (overlap 40-80 %); both cut; Tonic preset (no adaptation: both sides fire in step, 100 % overlap).

Hints: (1) mutual inhibition plus a way for the active side to tire; (2) two inhibitory cells, adapting aeif; (3) the full recipe at default weights (the NEST-default aeif cell needs a drive weight of about 400 pA, the Adapting preset the lab's 300).
Metric: `period_ms` (cycle period, lower = faster).

Credits: Brown TG (1911) Proc R Soc B 84:308; Grillner S (2003) Nat Rev Neurosci 4:573.

## Default part and recipe (v0.3 fix round)
* **`default_model()`:** `aeif_cond_alpha` - the card the lab's parts picker opens this level on: the cell type the reference circuit and every verdict assume.
* **`recipe()`:** drive > L and drive > R (the default sensor weight 300 is right for the Adapting preset). L and R: one aeif cell each, Adapting preset, both inhibitory. L > R and R > L at the default weight (-8 nS), L > 'left' and R > 'right'. Each side then bursts for a few hundred milliseconds and gives way to the other. With the NEST-default aeif cell the drive has to be raised to about 400 pA of weight (its rheobase is about 620 pA).
  The brief keeps the concept; the numbers live here, so the lab can show them by mode (Guided inline, Standard behind SHOW RECIPE at the cost of the third star, Researcher never).
* **Novice path with the lab's defaults** (`tasks_micro_a_test`): first build made of the default part and the default weights, then one change per verdict: **3 runs** (gate: at most 8).
