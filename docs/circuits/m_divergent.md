# Reference circuit: Divergent command (`circuits/m_divergent.json`)

Micro-puzzle `m_divergent` (`engine/src/tasks/m_divergent.cpp`, chapter "autonomic", order 419), concept 19 Divergent command, placed just before `fight_or_flight (420)`
(docs/DESIGN-0.3.md section 3). A fish-tank puzzle: one new idea, a 1-12 neuron circuit, a 2-5 s episode, a dashboard with the readouts above.
Generator: `python3 circuits/tools/gen_micro_b.py m_divergent`.

## World
3.2 s. `alarm` (dc level 3.0 = 900 pA) is on for 1.2 s from 900 + J ms; `tone` (dc 1.4) holds the gut at about 40 spikes/s (digestion). Four effector pools: `heart`, `pupil`, `sweat` (silent at rest) and `gut`.

## Reference circuit
```
alarm --> CMD (2 cells) --(600 pA)--> HEART --> heart
                        --(280 pA)--> PUPIL --> pupil
                        --(280 pA)--> SWEAT --> sweat
alarm --> INH (2, inhibitory) --(-160 pA)--> GUT --> gut        tone --> GUT
```
8 neurons. The alarm sensor has one weight (900 pA, 135 spikes/s on every cell it touches), so the different strengths are made at the synapses of the command group: the heart needs more than 735 pA of drive for 100 spikes/s, the pupil and sweat about 380-520 pA for 15-80. Generator: `python3 circuits/tools/gen_micro_b.py m_divergent`.

## Score
| criterion | limit |
|---|---|
| rest | heart, pupil, sweat <= 3 spikes/s; gut >= 8 |
| excite | heart >= 100 spikes/s (guided 80, researcher 118); pupil and sweat in 15-80 (guided 12-100, researcher 19-64) |
| inhibit | gut <= 0.3 x its rest rate (guided 0.4, researcher 0.24) |
| together | the three excited pools start within 100 ms of each other and within 200 ms of the alarm |
| recovery | after 2400 + J ms heart, pupil, sweat <= 5 spikes/s; gut >= 0.6 x rest |

Value 100 when all five hold, else 85 x the credit.

## Measured (standard mode, reference circuit)
| Seed | heart Hz | pupil Hz | sweat Hz | gut rest Hz | gut alarm Hz | onset spread ms | Score |
|---|---|---|---|---|---|---|---|
| 1 | 134 | 45 | 45 | 42 | 0 | 19 | 100 |
| 2 | 134 | 45 | 45 | 42 | 0 | 20 | 100 |
| 3 | 134 | 45 | 45 | 40 | 0 | 20 | 100 |
| 4 | 134 | 45 | 45 | 42 | 0 | 20 | 100 |
| 5 | 134 | 44 | 44 | 42 | 0 | 20 | 100 |
| 6 | 134 | 45 | 45 | 42 | 0 | 20 | 100 |

All six seeds pass (`spike-run --task m_divergent --seeds 1,2,3,4,5,6`). Guided mode: 3/3 seeds pass; researcher mode: 3/3.

## From an empty bench
Run 1: the alarm straight onto the three excitatory pools and one inhibitory cell: the pupil fires 135 spikes/s (verdict: the same weight cannot serve the heart and the pupil). Run 2: a command group at the default weights: the heart stays silent. Run 3: heart 600, pupil and sweat 280 (brief, tier 3) with one inhibitory cell onto the gut passes (the alarm drives that cell at 135 spikes/s, enough to cancel the gut's 45 pA of surplus drive at the default -80 pA).

Lesions that must fail (tested in `engine/tests/tasks_micro_b_test.cpp`): The inhibitory cells cut from the gut (digestion continues); the command group cut from the heart; all effectors at the heart's weight (pupil and sweat too strong).

## Biology
Cannon WB (1915) Bodily Changes in Pain, Hunger, Fear and Rage. D. Appleton, New York: the sympathetic division discharges as a unit in fear or rage, speeding the heart, dilating the pupils, driving sweat and stopping digestion. A sympathetic preganglionic neuron contacts many postganglionic neurons, so one command reaches many organs (Janig W (2006) The Integrative Action of the Autonomic Nervous System. Cambridge University Press).
