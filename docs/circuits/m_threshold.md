# Micro-puzzle: threshold (`circuits/m_threshold.json`)

Task `m_threshold` (engine/src/tasks/m_threshold.cpp), concept 1 (threshold and rheobase, `docs/DESIGN-0.3.md` section 3), chapter invertebrates,
campaign order 7 (before `m_excitation`, `m_convergence`, then `reflex` at 10). Level kind `micro`, on the spine.

## The puzzle
A dc sensor `pulse` gives a small blip (level 0.6, 800-1400 ms) and a big step (level 1.0, 2200-3600 ms) in a 4 s episode. One cell must fire on the
step (at least 8 spikes) and not on the blip or anywhere else. The current the cell gets is level x sensor weight (pA).

## Reference (1 neuron)
```
pulse (dc, 450 pA/unit) --> N1 (iaf_psc_alpha) --> spike
```
At 450 pA the step fires the cell at about 48 Hz (62-72 spikes, first spike 18-22 ms after the onset) and the blip (270 pA) is under the 375 pA rheobase.

## Why it works
* The rheobase of an `iaf_psc_alpha` cell is (V_th - E_L) / R = 15 mV / 40 MOhm = 375 pA, so with the sensor weight W the cell fires on the step iff
  W > 375 and stays quiet on the blip iff 0.6 W < 375, i.e. **375 < W < 625 pA**. Measured: 400 and 600 pass 6 of 6 seeds, 700 fails (the blip fires).
* The lab's first-connect dc weight is 300 pA, so the first build is silent: the verdict says "the step current stayed below its rheobase".

## Novice path (headless harness, lab defaults, no recipe read)
| run | build | result |
|---|---|---|
| 1 | pulse > cell > spike, weight 300 (the lab's start) | "never fired ... below its rheobase" |
| 2 | weight 450 | pass |

**2 runs.** A player who guesses 1000 gets "fired on the small blip" and needs a third.

## Modes
| | blip level | noise | step spikes | blip / stray spikes |
|---|---|---|---|---|
| guided | 0.5 (window 375-750 pA) | none | >= 5 | <= 1 |
| standard | 0.6 (375-625 pA) | 4 % per ms, 2 % per episode | >= 8 | 0 |
| researcher | 0.72 (375-520 pA) | x2 | >= 8 | 0 |

## What breaks it (`engine/tests/tasks_micro_a_test.cpp`)
Cell silenced; pulse weight x0.5 (225 pA, under the rheobase); pulse weight x1.6 (720 pA, the blip fires too).

Hints: (1) a neuron fires only when input pushes it past threshold, rheobase 375 pA; (2) the weight of `pulse`; (3) wire it up and set the weight to 400-600 pA.
Metric: `latency_ms` of the first step spike (lower is better).

Credits: Lapicque L (1907) J Physiol Pathol Gen 9:620; Brunel N, van Rossum MCW (2007) Biol Cybern 97:337; Hodgkin AL (1948) J Physiol 107:165.

## Default part and recipe (v0.3 fix round)
* **`default_model()`:** `iaf_psc_alpha` - the card the lab's parts picker opens this level on: the cell type the reference circuit and every verdict assume.
* **`recipe()`:** Wire pulse to one integrate-and-fire cell and the cell to 'spike'. The cell's rheobase is 375 pA, so set the pulse weight between about 400 and 600 pA: at the lab's start of 300 even the step (level 1.0) stays under it, and above 625 the blip (level 0.6) crosses it too.
  The brief keeps the concept; the numbers live here, so the lab can show them by mode (Guided inline, Standard behind SHOW RECIPE at the cost of the third star, Researcher never).
* **Novice path with the lab's defaults** (`tasks_micro_a_test`): first build made of the default part and the default weights, then one change per verdict: **2 runs** (gate: at most 8).
