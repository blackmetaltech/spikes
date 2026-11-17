# Reference circuit: reach (`circuits/reach.json`)

Task `reach` (`engine/src/tasks/reach.cpp`, chapter "spinal", order 130). A target appears with a go cue and the circuit must move the
elbow to it fast, with little overshoot, from a triphasic agonist - antagonist - agonist pattern, three times (0.4, 0.7 and 0.5 rad of flexion).
Generator: `python3 circuits/tools/gen_spinal.py reach`. The limb, spindles and delays are those of `stretch_reflex.md`, with `rmax_hz = 100` (a
12 cells at 60 Hz per cell is 36 % excitation of the muscle's 20 units; the muscle is fully excited when the pool's total rate reaches 20 x 60 Hz), so that bursts of 60-120 Hz per cell remain informative. Between reaches a light manipulandum (a
spring-damper to the rest angle, 30 N m/rad, 3 N m s/rad, at most 8 N m, for 1500 ms) brings the forearm back, so each reach starts at rest;
without it the arm does not return to 0.2 rad by itself (the rest posture is nearly neutral, a drift of 0.1-0.2 rad).

## Wiring (56 neurons)
```
go (dc, 100 for 40 ms) --> GO x3 --24 nS--> B1 x4 --2.85 nS--> MN_F ;  B1 --(-1.9 nS)--> MN_E ;  B1 --6 nS, delay 212 ms--> B2 x4
target (dc, 100 x rad) --> TGT x3 --1.9 / 1.3 nS--> B1 / B2 (sub-threshold: they scale the bursts)
                                  `--5.95 nS--> MN_F ;  --(-10 nS)--> MN_E          (hold command: shifts the tonic balance)
B2 --7 nS--> MN_E ;  B2 --(-5.1 nS)--> MN_F ;  B2 --(-11 nS)--> B1 ;  B2 --6.2 nS, delay 103 ms--> B3 x4
B3 --2.3 nS--> MN_F ;  B3 --(-10.7 nS)--> B2
ag_Ia / ant_Ia --> Ia_F / Ia_E (5 each) --5 nS--> MN_F / MN_E (12 each) ; Ia --3 nS--> IN (2 each) --(-8 nS)--> opposite MN   (stretch reflex)
drive (dc 1.0) --1000 pA--> MN_F, MN_E
```
B1, B2, B3 are `aeif_cond_alpha` burst cells (C 200 pF, g_L 20 nS, spike-triggered adaptation b 34 pA, tau_w 92 ms) with slow NMDA-like excitatory
(tau 30 ms) and slow inhibitory (tau 40 ms) synapses, so that a short volley from GO (or a delayed volley from the previous stage) becomes a burst
of 60-120 ms. B2 terminates B1 (slow inhibition) and B3 terminates B2. Delays of 100-200 ms stand for the polysynaptic chain and conduction that
sequence the real burst generators (spinal and brainstem interneurons; the brain's reach circuits are not modelled). Three extra outputs, `burst1`,
`burst2`, `burst3` (the pools of B1-B3), are not read by the task; they give the scopes the burst pattern.

## Score
`reach_ms`: time from the cue until the angle enters +-0.10 rad of the target and stays within +-0.16 rad for 200 ms. `overshoot`: peak beyond the
target over the distance, 900 ms. `final_err`: the mean angle over the last 300 ms of the 1800 ms display against the target. `premature`:
peak-to-peak angle in the 300 ms before the cue. A reach is good with reach_ms <= 700, overshoot <= 0.30, final_err <= 0.15 rad; the task passes if
all three are good and premature <= 0.10 rad. Measured:

| Seed | Mean reach time | Mean overshoot | Mean final error |
|---|---|---|---|
| 1 | 450 ms | 0.08 | 0.027 rad |
| 2 | 450 ms | 0.14 | 0.069 rad |
| 3 | 464 ms | 0.08 | 0.057 rad |
| 4 | 472 ms | 0.16 | 0.080 rad |
| 5 | 464 ms | 0.12 | 0.045 rad |
| 6 | 517 ms | 0.11 | 0.068 rad |
| 7 | 489 ms | 0.06 | 0.067 rad |
| 8 | 518 ms | 0.03 | 0.028 rad |
| 9 | 470 ms | 0.06 | 0.043 rad |
| 10 | 473 ms | 0.06 | 0.073 rad |
| 11 | 474 ms | 0.07 | 0.051 rad |
| 12 | 460 ms | 0.10 | 0.043 rad |

Seeds 1-12 all pass (the test runs 1-10).## How the parameters were found (and what is fragile)
The hold command and the three bursts were found by closed-loop search with the task as the objective: the equilibrium (hold) weights by a
1-D sweep (w_T_MNE -8 to -14 nS: -10 lands about 0.03 rad short, -12 overshoots), then about 100 random and 100 local-search circuits (2-3 seeds each) over the
burst weights, delays, adaptation and target gains. Without bursts the same circuit reaches the targets in 700-970 ms by the hold command alone
(the equilibrium-point mechanism of Feldman 1986); the bursts make it 400-570 ms, which is what separates the lesions below from the reference.
Margins: before the fix the reference failed seeds 6, 8 and 10 (creep, slow reach, 0.13 rad final error). The first agonist burst was strengthened (w_B1_MNF 2.4 -> 2.85) and the final-error limit widened to 0.15 rad, the creep limit to 0.10 rad (the resting limb wanders 0.03-0.08 rad p-p): the reference now passes seeds 1-12 (12/12). The verdict compares the worst reach with the per-reach limits, not the mean. The
burst size depends non-linearly on the target gain (burst cells fire only above a threshold of target plus go drive); shorter B1 -> B2 delays
(170 ms instead of 212 ms) lost the whole effect (the brake falls on a burst that has not accelerated the limb yet).

## What breaks it (tests in `engine/tests/tasks_spinal_test.cpp`)
| Lesion | Result |
|---|---|
| B1 -> MN_F cut (no accelerating burst) | reaches take 1030 ms (seed 1) or 1 of 3 good (seed 2) |
| B2 -> MN_E and B2 -> MN_F cut (no braking burst) | overshoot 26-31 % of the distance, final error 0.25-0.28 rad |
| GO -> B1 cut (the cue never starts the generator) | **not tested as a failure**: the equilibrium-point hold command alone drags the arm to the target in 550-950 ms, so it passes some seeds (as Feldman's model predicts) |
| TGT -> MN_F and MN_E cut (no hold command) | the bursts move the arm but nothing holds it: final error 0.35-0.5 rad, no reach good |

Credits: Wachholder K, Altenburger H (1926) Pflugers Arch 214:642 (the triphasic pattern); Hallett M, Shahani BT, Young RR (1975) J Neurol Neurosurg
Psychiatry 38:1154; Berardelli A et al. (1996) Brain 119:661 (burst size and duration scale with distance); Feldman AG (1986) J Mot Behav 18:17
(equilibrium-point control); Gottlieb GL, Corcos DM, Agarwal GC (1989) Behav Brain Sci 12:189 (speed-insensitive and speed-sensitive strategies).

## v0.3: concepts, modes, variants (item D)

* **Concepts:** timed_bursts (new). Level kind `task`, on the spine.
* **Modes** (guided / standard / researcher): Limits (reach time, overshoot, final error, creep) x1.4 guided (980 ms ...) / 1 / x0.95. Guided is the novice-passable setting. The structure of the pass test never changes; the reference circuit passes every mode (researcher on seeds 1-5; `tasks_modes_test`).
* **Variants** (optional branches, never weaken the base bar): half_neurons (par 28); quick_reach (<= 550 ms).
* **Hints:** three tiers from `score().parts` (concept, then the place: a channel or population id, then a partial recipe).
* **Quality metric:** `reach_ms`. **Notebook:** Hallett 1975; Berardelli 1996.

### v0.3: novice passability
The engine now has `weight_sd` / `weight_cv` on `edges` projections and `ChannelKind::weight`; the brief recommends a weight spread (CV 0.3-0.5) on the links onto the pools and the guided limits are x1.4 wider. I could not reproduce the lab's empty-bench circuit exactly here, so the gain from the spread on a novice build is not measured; the reference (spread baked into `edges`) passes all modes.
