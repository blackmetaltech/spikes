# Reference circuit: sinoatrial pacemaker cluster (`circuits/heart_pacemaker.json`)

Task `heart_pacemaker` (engine/src/tasks/heart_pacemaker.cpp, chapter "heart", view "heart", order 200). The SA node is the heart's
pacemaker: a patch of cells that fire spontaneously, each spike one beat. The player places `sa_node_pacemaker` cells
(`docs/models/sa_node_pacemaker.md`), reads their spikes out as `sa_beat`, and the task feeds every spike to the heart body
(`spike::body::Heart`, `docs/body/heart.md`) as a beat request. The heart merges requests closer than its 250 ms refractory
period into the first one, so the first cell of a cluster to fire starts the beat, and a cluster that cannot agree on one rhythm
produces extra, irregular beats. The task applies a resting vagal tone: it holds the engine's ACh level at 0.5 and NE at 0 (`Engine::set_modulator`), and the SA cells follow the circuit-wide ACh / NE, so a cell that beats at about 100/min on its own (the intrinsic rate after autonomic blockade) sits at 65-70/min here, as in a resting human. (The heart body's own `vagal` / `sym_*` inputs only shape the pressure readouts; the rate is the cells'.)

**Cell heterogeneity (seeded).** At the first step the task gives every cell of the `sa_beat` pool its own membrane capacitance, `C_m` = 32 pF x (1 + e), e uniform in +-12 % (`set_neuron_param`; a player's own `C_m` on those cells is overridden). Node cells differ in size and so in intrinsic rate. Identical clones therefore drift apart, and only coupling makes the cluster agree.

## Score
Analysis window 6-30 s (the first 6 s are the settling of the cells onto their limit cycle). From the beat times of the heart:
`hr_bpm` = 60000 / mean interval, `cv` = sd / mean of the intervals, `sdnn_ms` and `rmssd_ms` from `analysis::hrv_analyze` (Task Force
1996), and `synchrony` = Golomb (2007) chi of the cluster's spike counts in 40 ms bins (0 = asynchronous, 1 = all cells spike together;
only for a motor pool of 2 or more cells).

* **Pass**: 60 <= hr <= 80 bpm **and** beat-interval CV < 5 %, at the end of the episode (30 s).
* **Value**: `100 (0.5 rate_term + 0.5 cv_term) (0.75 + 0.25 sync_term)`. `rate_term` is 1 inside 60-80 and falls linearly to 0 at 20 bpm
  outside; `cv_term` is 1 up to 2 % and 0 at 15 %; `sync_term = clamp((chi - 0.3)/0.6, 0, 1)` for a cluster, 0 for one cell. A single
  cell is not a node (see the mechanism check below: no pass, value capped at 49), a tightly coupled cluster scores about 98, and several uncoupled cells (identical clones included) fail.
* **Verdict** names the failure: not wired, no beats, too few beats (pauses or arrest), bradycardia, tachycardia (and whether the cells
  are out of step), irregular rhythm.

## Wiring (7 neurons)
```
SA_c (3 cells, I_e -4 pA)  <-- 6 nS excitatory, 1 ms, all to all -->  SA_p (4 cells, I_e 0 pA)       outputs: sa_beat = all 7 cells
```
| Population | Model | Role |
|---|---|---|
| SA_c (3) | `sa_node_pacemaker`, `I_e` -4 pA, `V_m` -65 mV | "central" cells: intrinsically slower |
| SA_p (4) | `sa_node_pacemaker`, `I_e` 0 pA, `V_m` -45 mV | "peripheral" cells: intrinsically faster |

The two groups differ in intrinsic rate (on top of the task's per-cell size spread; the bias current stands for the node's centre-to-periphery gradient in cell size, I_f density and
capacitance) and in starting phase (`V_m` pinned, so the result does not depend on the engine's random initial draw). The coupling is an
excitatory synapse (6 nS, conductance, reversal 0 mV, 1 ms delay) in both directions. The `modulation` table maps the engine's `ACh` and `NE`
channels onto every cell's `ach` / `ne`, so the sandbox dials work. There are no sensors.

## Why it works
* **Alone the cells disagree.** With the task's size spread the cells differ by tens of percent in rate. Uncoupled, the heart gets all the rhythms interleaved: five uncoupled *identical* clones give 126-170 beats/min with CV 25-38 % and synchrony 0.43-0.47 (task value 0). Before this change the clones all started depolarised, fired together and scored 100 (0.2 critique, S1).
* **Coupling entrains them** to one rate (68-70.5/min, tone-slowed) with intervals constant to 0.06 %. Reference on seeds 1-10: value 97.3-99.0, hr 68.0-70.5, CV <= 0.06 %, synchrony 0.83-0.88. Entrainment of pacemaker cells of different intrinsic rate by coupling is the classical picture of how the node works (Jalife 1984; Michaels, Matyas & Jalife 1987).
* **A single cell** beats at 64-71/min depending on its size draw, CV 0.05 %, passes and scores 75 (no cluster bonus).
* The cells are deterministic limit cycles; the seed only moves their sizes (and the engine's initial V_m draw). (A Poisson "channel noise" input was tried and rejected: at 0.01-0.03 nS and 100 Hz it already knocks cells out of lock in some seeds.)

## Gap junctions
The node's coupling is electrical (connexin 43/45/40 gap junctions). The engine's `gap_junction` synapse needs `hh_psc_alpha_gap` or
`hh_cond_beta_gap_traub` cells on both sides, which fire at tens of hertz, not once a second. The NESTML SA cell has no gap-junction interface, so the
cluster uses a fast excitatory conductance as the stand-in (a pulse-coupled oscillator network rather than a continuously coupled one). Both entrain
cells of different intrinsic rate; the difference is that a pulse is delivered at the spike, where a gap junction acts through the whole cycle.

## What breaks it (tests in `engine/tests/tasks_heart_test.cpp`, seeds 1 and 2; clones on seeds 1-10)
| Lesion | Result | Expected biology |
|---|---|---|
| coupling removed (SA_c <-> SA_p weight x0) | 162 beats/min, CV 26 %, synchrony 0.36, value 0 | uncoupled node cells of different rates give no single rhythm (sinus node dysfunction, exit block) |
| five uncoupled identical clones (one population, no synapses) | 126-170 beats/min, CV 25-38 %, synchrony 0.43-0.47, value 0, all 10 seeds | the task makes the cells differ; coupling is what earns the score |
| I_CaL blocked by 60 % (`g_CaL` x0.4) | no beats at all | L-type Ca current makes the upstroke (nifedipine, Cav1.3 knockout) |
| I_CaT blocked by 30 % (`g_CaT` x0.7) | 56 beats/min, bradycardia | T-type Ca (Cav3.1) helps late diastolic depolarisation; Cav3.1-null mice are bradycardic (Mangoni et al. 2006). Full block silences this re-fit cell |
| I_f blocked (`g_f` 0) | 50 beats/min, bradycardia | ivabradine slows the node (15-20 % in humans; here the re-fit cell is more I_f dependent, -28 %) |
| extra tonic ACh 1.5 on top of the resting tone | 54 beats/min, bradycardia (gradual, no arrest) | muscarinic agonist, vagal overdrive |
| tonic NE 1.0 (`ne` param) | 107 beats/min, tachycardia | beta-adrenergic stimulation |

Credits: Jose AD, Collison D (1970) Cardiovasc Res 4:160 (intrinsic heart rate); Jalife J (1984) J Physiol 356:221; Michaels DC, Matyas EP, Jalife J (1987)
Circ Res 61:704; Mangoni ME et al. (2006) Circ Res 98:1422; Golomb D (2007) Scholarpedia 2(2):1347 (chi measure); Task Force of the ESC and NASPE (1996)
Circulation 93:1043 (HRV).

## v0.3: concepts, modes, variants (item D)

* **Concepts:** coupling, pacemaking (new). Level kind `task`, on the spine.
* **Modes** (guided / standard / researcher): Rate 55-85 / 60-80 / 62-78 per min, CV < 7 / 5 / 4 %; cell-size spread x0.6 / 1 / x1.25. The structure of the pass test never changes; the reference circuit passes every mode (researcher on seeds 1-5; `tasks_modes_test`).
* **Variants** (optional branches, never weaken the base bar): half_neurons (par 4); noise_x2 (24 % spread in C_m).
* **Hints:** three tiers from `score().parts` (concept, then the place: a channel or population id, then a partial recipe).
* **Quality metric:** `synchrony`. **Notebook:** Jose & Collison 1970; Michaels 1987.

## v0.4: mechanism check, quick check, negative tests
**Mechanism (Engine::structure()).** The cells `sa_beat` reads must be coupled to each other by excitatory projections: a group projecting to itself (at least 2 cells, not a one-to-one self-projection, which is only autapses) or one group projecting to another. A lone cell, cells with no coupling, coupling at weight 0 and coupling by inhibition are "no cluster": `passed` is false, the value is capped at 49 and the verdict says there is no cluster and what to add. (Before: one default cell passed 5/5 with 75 points; REVIEW-HARSH S1.) Cutting one direction of the coupling keeps the cluster (one group still drives the other).

**Quick check.** `quick_duration_ms()` = 20 s: the same rate / CV / synchrony rules over the window 6-20 s of the same run (the rhythm is stationary after the settling; 12 beats are still needed). Reference: 1.8 s -> 1.3 s wall for one seed on the dev machine.

**Not a cheap circuit here:** "every weight at the maximum" (the coupling at its extreme) still passes: a cluster coupled as strongly as possible is a correct answer to a coupling level. The negative table (`engine/tests/cheap_rows_c.inc`) holds the lone default cell (the appendix row), the reference's own cell alone, one cell wired to itself, five uncoupled clones, inhibitory coupling and the reference minus its coupling.

The verdicts and the tier 1-3 hints carry no parameter numbers (weights, biases, gains): those live in `recipe()` only (checked in the tests). Negative tests: `engine/tests/cheap_rows_c.inc` (run by `tasks_cheap_test`) and the mechanism cases in the level's own test file.
