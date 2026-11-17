# Reference circuit: Negative feedback (`circuits/m_negative_feedback.json`)

Micro-puzzle `m_negative_feedback` (`engine/src/tasks/m_negative_feedback.cpp`, chapter "spinal", order 119), concept 14 Negative feedback control, placed just before `posture (120)`
(docs/DESIGN-0.3.md section 3). A fish-tank puzzle: one new idea, a 1-12 neuron circuit, a 2-5 s episode, a dashboard with the readouts above.
Generator: `python3 circuits/tools/gen_micro_b.py m_negative_feedback`.

## World
3.6 s, a plain-C++ plant. A level y (joint angle, blood pressure) obeys `tau dy/dt = u - y - d(t)` with tau = 200 ms and u the pool `effort`'s rate over 80 Hz, low-passed with 60 ms. The target is y = 0.5. A push d pulls y down by 0.3 for 1 s from b = 1100 + J ms, and pushes it up by 0.3 for 1 s from b + 1400. `tone` (dc level 2.0, 600 pA: effort 1.06) would carry y to about 1.0 without feedback. `level` (dc) reports 3 x y, delayed by 20 ms (2 % noise): 450 pA at the target. One motor channel, `effort`.

## Reference circuit
```
tone  --> EFF (2 cells) --> effort
level --> INH (3 cells, inhibitory) --(-300 pA)--> EFF
```
5 neurons. Each unit of level adds 900 pA to the inhibitory cells, so the loop gain is about 5 and the error is the open-loop error over 6. Generator: `python3 circuits/tools/gen_micro_b.py m_negative_feedback`.

## Score
y averaged over [700, b] (baseline), [b + 700, b + 1000] (push down) and [b + 2100, b + 2400] (push up) must each be within 0.10 of 0.5 (guided 0.135, researcher 0.08). Value 100 when all three hold, else 85 x the credit.

## Measured (standard mode, reference circuit)
| Seed | baseline | during push down | during push up | mean abs error | Score |
|---|---|---|---|---|---|
| 1 | 0.52 | 0.44 | 0.56 | 0.055 | 100 |
| 2 | 0.52 | 0.44 | 0.57 | 0.052 | 100 |
| 3 | 0.52 | 0.44 | 0.57 | 0.054 | 100 |
| 4 | 0.52 | 0.43 | 0.57 | 0.058 | 100 |
| 5 | 0.52 | 0.43 | 0.56 | 0.054 | 100 |
| 6 | 0.51 | 0.44 | 0.57 | 0.051 | 100 |

All six seeds pass (`spike-run --task m_negative_feedback --seeds 1,2,3,4,5,6`). Guided mode: 3/3 seeds pass; researcher mode: 3/3.

## From an empty bench
Run 1: `level` wired straight onto the effort cells (positive feedback) or not at all: the level sits near 1.0. Run 2: through an inhibitory cell at the default -80 pA: still 0.7. Run 3: 3 cells at -300 pA (brief, tier 3) passes. 6 cells at -150 pA also passes.

Lesions that must fail (tested in `engine/tests/tasks_micro_b_test.cpp`): The inhibitory link cut (open loop: level 1.0); the link with the wrong sign (excitatory: positive feedback, the level runs away).

## Biology
Cannon WB (1929) Organization for physiological homeostasis. Physiol Rev 9:399-431. The stretch reflex, the baroreflex and the pupil light reflex all have the same elements: a sensor, a comparison with a set point, and an effector that opposes the difference.
