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

## Default part and recipe (v0.3 fix round)
* **`default_model()`:** `iaf_psc_alpha` - the card the lab's parts picker opens this level on: the cell type the reference circuit and every verdict assume.
* **`recipe()`:** alarm onto a command group of 2 cells. The command group onto heart at about 600 pA, onto pupil and onto sweat at about 280 pA (1 cell each); these weights scale inversely with the size of the command group: about 1200 pA in all onto the heart and about 560 pA onto pupil and sweat, so 1200 and 560 from a single command cell. alarm also onto an inhibitory group of 2 cells that projects onto gut at about -160 pA. tone onto gut only.
  The brief keeps the concept; the numbers live here, so the lab can show them by mode (Guided inline, Standard behind SHOW RECIPE at the cost of the third star, Researcher never).
* **Novice path with the lab's defaults** (`tasks_micro_b_test`): first build made of the default part and the default weights, then one change per verdict: **4 runs** (gate: at most 8).

## v0.4: the honesty contract (docs/REQ-0.4.md)
- **Mechanism check.** Before the behaviour, the task reads the circuit structure (`Engine::structure()`, captured when the run starts) and requires a command group (not an effector) driven by `alarm` that projects excitatorily onto at least two of heart, pupil and sweat, and an inhibitory route from `alarm` to the gut. A circuit that lacks it fails with a verdict naming the missing mechanism, and its value is capped at 40.
- **No numbers in verdicts or hint tiers 1-3.** Failed verdicts and the three hint tiers describe the observed behaviour and the direction to change; the numbers are only in `recipe()`. Pass messages still report observed values.
- **Negative tests.** `engine/tests/cheap_rows_b.inc` (run by `tasks_cheap_test`): empty bench, one cell with the lab's first-connect weights, one tonic cell with every weight 0, the reference with every weight 0, the reference with every weight at an extreme, and the reference minus the taught mechanism, each must fail with the mechanism named.
- **Non-circular novice.** Brief and lab defaults only, at most 8 runs: solved in 4 runs (heart 300 / 600 with the gentle links at x1 and x0.5).
