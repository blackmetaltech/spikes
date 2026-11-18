# Reference circuit: Rhythm and sensory gating (`circuits/m_rhythm.json`)

Micro-puzzle `m_rhythm` (`engine/src/tasks/m_rhythm.cpp`, chapter "breathing", order 299), concept 17 Rhythm plus sensory gating, placed just before `breathing (300)`
(docs/DESIGN-0.3.md section 3). A fish-tank puzzle: one new idea, a 1-12 neuron circuit, a 2-5 s episode, a dashboard with the readouts above.
Generator: `python3 circuits/tools/gen_micro_b.py m_rhythm`.

## World
5 s. `chemo` is a dc sensor: level 1.0 (rest) and, after a 200 ms ramp from 2400 + J ms, level 1.6 (the air is turning stale). One motor channel, `phrenic`, read from a single bursting cell (the detector counts runs of two or more spikes less than 25 ms apart; a pool of several identical cells fires out of step and is refused). The tank's rhythm is about ten times faster than a breath, so that a run fits in five seconds.

## Reference circuit
```
chemo (dc, 5 pA per unit) --> PBC (1 Izhikevich cell, chattering: a 0.02 b 0.2 c -50 d 2) --> phrenic
```
1 neuron. The cell bursts for a drive of about 4 to 9 (burst period 140 ms at I = 4, 58 ms at I = 9); below about 3.5 it is silent and above about 11 the bursts fuse into a steady train. The lab's 300 pA per unit would fuse them at once, so the sensor weight is the only knob. Generator: `python3 circuits/tools/gen_micro_b.py m_rhythm`. (The 5 s episode rules out `butera_burster`, whose burst period is 1.1-5 s; it is the cell of the big task.)

## Score
| criterion | limit |
|---|---|
| one cell | `phrenic` reads exactly 1 cell |
| rhythm | in [800, s] ms at least 5 bursts of 2 spikes or more, at 5-14 bursts/s (guided 3.7-19, researcher 6.3-11.2) |
| gating | after the rise (from s + 500 ms) at least 1.4 x the rest rate and at most 3.5 x, still bursting (guided 1.2 x, researcher 1.65 x) |

Value 100 when all three hold, else 85 x the credit. The quality metric is the gating ratio (higher is better).

## Measured (standard mode, reference circuit)
| Seed | rest bursts/s | CO2 bursts/s | ratio | spikes per burst | Score |
|---|---|---|---|---|---|
| 1 | 9.4 | 15.5 | 1.64 | 4.0 | 100 |
| 2 | 9.8 | 16.0 | 1.63 | 4.0 | 100 |
| 3 | 9.9 | 15.8 | 1.60 | 4.0 | 100 |
| 4 | 9.9 | 15.8 | 1.60 | 4.0 | 100 |
| 5 | 9.9 | 15.8 | 1.59 | 4.0 | 100 |
| 6 | 9.8 | 15.5 | 1.58 | 4.0 | 100 |

All six seeds pass (`spike-run --task m_rhythm --seeds 1,2,3,4,5,6`). Guided mode: 3/3 seeds pass; researcher mode: 3/3.

## From an empty bench
Run 1: the Izhikevich chattering part as one cell, `chemo` at its first-connect 300 pA: `fires a steady train ... lower the chemo weight to about 5`. Run 2: weight 5 passes (the window is about 4 to 5.5: 6 gives only x1.36, 7 is above 14 bursts/s at rest).

Lesions that must fail (tested in `engine/tests/tasks_micro_b_test.cpp`): Sensor weight 60 x too high (steady firing); weight 0.2 x (silent).

## Biology
Smith JC, Ellenberger HH, Ballanyi K, Richter DW, Feldman JL (1991) Pre-Botzinger complex: a brainstem region that may generate respiratory rhythm in mammals. Science 254:726-729. Guyenet PG, Stornetta RL, Bayliss DA (2010) Central respiratory chemoreception. J Comp Neurol 518:3883-3906 (RTN neurons add drive that raises respiratory frequency). Izhikevich EM (2003) IEEE Trans Neural Netw 14:1569-1572 (the chattering cell).

## Default part and recipe (v0.3 fix round)
* **`default_model()`:** `izhikevich_CH` - the card the lab's parts picker opens this level on: the cell type the reference circuit and every verdict assume.
* **`recipe()`:** One Izhikevich chattering cell (n = 1) read out on phrenic, with chemo wired to it. This cell bursts for a drive of about 4 to 9 pA, so set the chemo weight to about 5 pA per unit: 5 pA at rest bursts about 10 times a second, 8 pA after the CO2 rise about 15. Any higher weight, such as 300, makes the cell fire continuously.
  The brief keeps the concept; the numbers live here, so the lab can show them by mode (Guided inline, Standard behind SHOW RECIPE at the cost of the third star, Researcher never).
* **Novice path with the lab's defaults** (`tasks_micro_b_test`): first build made of the default part and the default weights, then one change per verdict: **1 run** (gate: at most 8).

## v0.4: the honesty contract (docs/REQ-0.4.md)
- **Mechanism check.** Before the behaviour, the task reads the circuit structure (`Engine::structure()`, captured when the run starts) and requires the cell read out on `phrenic` has an intrinsic burst mechanism (Izhikevich, adaptive exponential, Hodgkin-Huxley or similar; an integrate-and-fire cell cannot burst) and `chemo` reaches it. The lab no longer pre-sets the weight of `chemo`: the first connection has the plain default weight and the right one is for the player to find. A circuit that lacks it fails with a verdict naming the missing mechanism, and its value is capped at 40.
- **No numbers in verdicts or hint tiers 1-3.** Failed verdicts and the three hint tiers describe the observed behaviour and the direction to change; the numbers are only in `recipe()`. Pass messages still report observed values.
- **Negative tests.** `engine/tests/cheap_rows_b.inc` (run by `tasks_cheap_test`): empty bench, one cell with the lab's first-connect weights, one tonic cell with every weight 0, the reference with every weight 0, the reference with every weight at an extreme, and the reference minus the taught mechanism, each must fail with the mechanism named.
- **Non-circular novice.** Brief and lab defaults only, at most 8 runs: solved in 7 runs (halving the sensor weight from 300 down to 4.7).
