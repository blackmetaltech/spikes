# Reference circuit: Recruitment (size principle) (`circuits/m_recruitment.json`)

Micro-puzzle `m_recruitment` (`engine/src/tasks/m_recruitment.cpp`, chapter "spinal", order 98), concept 11 Recruitment, placed just before `stretch_reflex (100)`
(docs/DESIGN-0.3.md section 3). A fish-tank puzzle: one new idea, a 1-12 neuron circuit, a 2-5 s episode, a dashboard with the readouts above.
Generator: `python3 circuits/tools/gen_micro_b.py m_recruitment`.

## World
Effort steps of 900 ms from t = 500 + J ms (J = U(0, 100) ms from the seed): light (command level 1.2), medium (2.2), maximal (3.2), then rest; 4 s in all.
`command` is a dc sensor, so the current into a cell is the sensor weight (lab default 300 pA per unit) times the level: 360, 660, 960 pA.
Three motor pools are the units of one muscle, smallest first (`mu_small`, `mu_mid`, `mu_large`). A pool's force is `cap * clamp(rate / 20 Hz, 0, 1)` with caps 0.10, 0.30, 0.60 (small units are weak, tetanic fusion saturates a unit near 20 Hz) and the muscle's force is the sum. The staircase the task wants is 0.10 / 0.40 / 1.00 (0 at rest). Standard noise: 2 % white on the level.

## Reference circuit
```
command (dc, 300 pA/unit) --> MU_S (1 cell, I_e +100 pA) --> mu_small
                           --> MU_M (1 cell, I_e 0)      --> mu_mid
                           --> MU_L (1 cell, I_e -350 pA) --> mu_large
```
No projections: the three thresholds are the whole circuit (`iaf_psc_alpha`, rheobase 375 pA). At 360 pA only MU_S crosses (360 + 100), at 660 pA MU_S and MU_M (660 - 350 = 310 is below rheobase for MU_L), at 960 pA all three (610). Generator: `python3 circuits/tools/gen_micro_b.py m_recruitment`.

## Score
Windows are the last 600 ms of each step; the rest windows are 100-400 ms and the last 600 ms of the episode.

| criterion | limit (standard) |
|---|---|
| rest | force <= 0.04 |
| light / medium / maximal | `abs(force - target) <= 0.09` (guided x1.35, researcher x0.8) |

Value is 100 when all four hold, else 85 x the credit of the four. The verdict names the pool that fired too early or stayed silent.

## Measured (standard mode, reference circuit)
| Seed | force light | force medium | force maximal | mu_large Hz at medium | Score |
|---|---|---|---|---|---|
| 1 | 0.10 | 0.40 | 1.00 | 0 | 100 |
| 2 | 0.10 | 0.40 | 1.00 | 0 | 100 |
| 3 | 0.10 | 0.40 | 1.00 | 0 | 100 |
| 4 | 0.10 | 0.40 | 1.00 | 0 | 100 |
| 5 | 0.10 | 0.40 | 1.00 | 0 | 100 |
| 6 | 0.10 | 0.40 | 1.00 | 0 | 100 |

All six seeds pass (`spike-run --task m_recruitment --seeds 1,2,3,4,5,6`). Guided mode: 3/3 seeds pass; researcher mode: 3/3.

## From an empty bench
Empty bench: three groups, `command` onto each, each group read out on its own channel. With every bias at 0 nothing fires at 360 pA and everything fires at 660 pA (verdict: `mu_small stayed silent at light effort`). Raise mu_small's bias; then the medium effort shows `mu_large fired at medium effort`: lower its bias until 660 pA no longer reaches rheobase (below about -285 pA). Tier 3 gives +100 / 0 / -350.

Lesions that must fail (tested in `engine/tests/tasks_micro_b_test.cpp`): Every bias at 0 (the three pools switch on together); `command` cut from the large pool (force never reaches 1.00).

## Biology
Henneman E, Somjen G, Carpenter DO (1965) Functional significance of cell size in spinal motoneurons. J Neurophysiol 28:560-580: the threshold of a motoneuron to a synaptic input is inversely related to its size, so units are recruited from small and weak to large and strong, always in the same order. The reason is input resistance: the same synaptic current depolarises a small cell more. The tank gets the same ordering from the bias current (a shortcut for input resistance); the game's concept card says so.

## Default part and recipe (v0.3 fix round)
* **`default_model()`:** `iaf_psc_alpha` - the card the lab's parts picker opens this level on: the cell type the reference circuit and every verdict assume.
* **`recipe()`:** A cell's rheobase is 375 pA and the command delivers 360, 660 and 960 pA (weight 300 pA per unit times the level 1.2 / 2.2 / 3.2). Three groups, each driven by 'command' and read out on its own channel. Give the small pool a bias of about +100 pA, the mid pool none, the large pool about -350 pA: then 360 pA recruits only the small pool, 660 pA the mid pool too, 960 pA all three.
  The brief keeps the concept; the numbers live here, so the lab can show them by mode (Guided inline, Standard behind SHOW RECIPE at the cost of the third star, Researcher never).
* **Novice path with the lab's defaults** (`tasks_micro_b_test`): first build made of the default part and the default weights, then one change per verdict: **5 runs** (gate: at most 8).
