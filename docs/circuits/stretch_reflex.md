# Reference circuit: stretch reflex (`circuits/stretch_reflex.json`)

Task `stretch_reflex` (`engine/src/tasks/stretch_reflex.cpp`, chapter "spinal", order 100, view "limb"). A tendon is tapped and the forearm
is pushed by four sudden loads; the circuit must answer with a spinal-latency reflex, cut the deflection, switch the opposing muscle
off and not ring. The generator is `circuits/tools/gen_spinal.py` (`python3 circuits/tools/gen_spinal.py stretch_reflex`).

## The body (shared by the four spinal tasks, `engine/src/tasks/spinal_limb.h`)
`body::Joint`: an elbow-like hinge with a flexor (agonist) and extensor (antagonist) Hill muscle (`docs/body/muscle.md`, `joint.md`),
supported (no gravity), inertia 0.1 kg m^2, viscosity 0.3 N m s/rad, no fatigue. Spindle Ia, II and Golgi Ib rates come from the joint
and go to the circuit's sensor channels (Hz, `dc` inputs) after a 10 ms conduction delay; the mean rate per neuron of each alpha
motoneuron pool, low-passed with tau 15 ms, times the number of cells, divided by 20 motor units x 50 Hz and delayed 10 ms is the muscle's pool excitation (a pool of 20 cells at 50 Hz is full
excitation; a single cell can drive only one unit's share). The `Muscle` then
recruits its 20 motor units by size (Henneman 1957), so size-principle recruitment is in the effector, not in the circuit.

## Wiring (58 neurons)
```
ag_Ia  (dc, Hz) --4 pA/Hz--> Ia_F x6 --5.0 nS--> MN_F x20 --> ag_motor
                                   `--2.5 nS--> IN_F x3 --(-6 nS)--> MN_E
ant_Ia (dc, Hz) --4 pA/Hz--> Ia_E x6 --5.0 nS--> MN_E x20 --> ant_motor
                                   `--2.5 nS--> IN_E x3 --(-6 nS)--> MN_F
drive  (dc, 1.0) --1050 pA--> MN_F, MN_E   (tonic descending drive)
```
| Population | Model | Role |
|---|---|---|
| Ia_F, Ia_E (6 each) | `iaf_psc_alpha`, tau_m 100 ms | primary spindle afferents. A dc "sensor" current of 4 pA per Hz makes each fibre fire regularly at about that rate with random phases (real Ia endings are very regular, CV about 0.1: Matthews 1972) |
| MN_F, MN_E (20 each) | `aeif_cond_alpha` (C 500 pF, g_L 50 nS, V_th -52 mV, b 40 pA, tau_w 100 ms) | alpha motoneurons: rheobase about 800 pA, about 100 Hz per nA, tau_m 10 ms, spike-triggered AHP (Eccles, Eccles & Lundberg 1957; Kernell 1965) |
| IN_F, IN_E (3 each) | `iaf_cond_alpha` | Ia inhibitory interneurons (Eccles, Eccles & Lundberg 1957) |

Ia -> MN weights are explicit edges with weights 5.0 x U(0.4, 1.6) nS (Ia EPSP sizes vary several-fold between motoneurons: Henneman &
Mendell 1981); the spread also desynchronises the pool, which otherwise fires in volleys at its own rate. Delays are 1 ms (monosynaptic).

## Score (see the header of `stretch_reflex.cpp`)
Twelve **tendon taps** (3 ms Ia volley, alternately on each muscle) give a PSTH of the tapped pool; the first response bin plus the 10 ms
efferent conduction is the **latency** (23-24 ms here; the spinal range is 25-50 ms for long human limb nerves, Matthews 1991).
Four **imposed stretches** (+-2.5 N m, 350 ms, alternating direction) are compared with an **open-loop copy** of the limb that holds
its drive frozen, giving the **resistance** (1 - peak deflection / open-loop deflection). **Reciprocal inhibition** is the fall of the
antagonist pool's rate in 20-120 ms after the stretch starts. A sustained 0.08 rad oscillation is clonus.
Pass: latency <= 50 ms, resistance >= 0.30, antagonist suppression >= 0.20, no clonus. The tap protocol is 12 taps per muscle (24 taps) with
a pooled PSTH threshold; the verdict never says to wire what is already wired: when the taps are not resolved it says what was measured and whether the
pool responds to a stretch. Measured on seeds 1-10 (reference passes 10/10, and 12/12 on seeds 1-12, 5/5 on the lab's seeds 101-105):

| Seed | Latency | Resistance | Antagonist suppression |
|---|---|---|---|
| 1 | 23 ms | 0.60 | 0.39 |
| 2 | 24 ms | 0.56 | 0.40 |
| 3 | 24 ms | 0.56 | 0.38 |
| 4 | 24 ms | 0.70 | 0.26 |
| 5 | 24 ms | 0.61 | 0.34 |
| 6 | 24 ms | 0.64 | 0.32 |
| 7 | 24 ms | 0.41 | 0.26 |
| 8 | 24 ms | 0.50 | 0.34 |
| 9 | 24 ms | 0.36 | 0.25 |
| 10 | 24 ms | 0.64 | 0.35 |
| 11 | 24 ms | 0.55 | 0.32 |
| 12 | 25 ms | 0.59 | 0.30 |

Seeds 1-12 (the test runs 1-10); the tightest values are suppression 0.25 (seed 9, limit 0.20) and resistance 0.36 (limit 0.30). The lab's seeds 101-105 pass too (suppression 0.21-0.32, resistance 0.42-0.56).## Why it works, and what the numbers came from
* **Loop gain is the whole story.** The loop is spindle (velocity-sensitive, 0.6 power) -> 10 ms -> motoneurons -> 15 ms smoothing -> twitch
  (30-90 ms) -> limb. With Ia -> MN at 5 nS (x6 fibres) the pool rises about 8 Hz per 40 Hz of extra Ia, enough to halve the deflection and
  stay stable. Doubling the gain saturates the pools (tonic rate already 30 Hz of a 50 Hz excitation scale) and the reflex vanishes; x4
  makes it worse. Tested by closed-loop sweeps of w_Ia_MN (2-10 nS), w_IN_MN (-3 to -9 nS) and the drive (850-1100 pA).
* **Regular afferents.** Poisson Ia input made the motoneuron pool noisy enough (pool spike noise multiplied by the spindle's velocity
  derivative) to give a 5 Hz limb tremor of 0.2-0.4 rad even at low gain; regular afferents remove that without changing the reflex.
* **Push-pull.** Stretching the flexor raises Ia_F (more IN_F, more inhibition of MN_E) and unloads the extensor spindle (less Ia_E,
  less IN_E, so MN_F is released). Both halves add; the antagonist's own Ia (autogenic unloading) also contributes to the fall of its rate.

## What breaks it (tests in `engine/tests/tasks_spinal_test.cpp`, 2 seeds each)
| Lesion | Result |
|---|---|
| Ia -> MN cut (both pools) | no reproducible response to the taps in either pool (seeds 1, 2); the limb is floppy and wanders (0.17-0.19 rad p-p) |
| Ia -> IN cut (both pools) | the antagonist keeps firing (suppression 0.05-0.14), co-contraction: resistance 0.28-0.34 (< 0.30 on most seeds), "add reciprocal Ia inhibition" |
| IN -> MN cut | same as above |
| Ia -> MN x4 | **not clinical hyper-reflexia**: the motoneuron pools saturate against the muscle's full-excitation rate (a model ceiling), so the reflex is lost (resistance 0.00). Real hyper-reflexia would raise resistance and give clonus; this model cannot show that at x4. It is kept as "gain past the model's saturation" |

Credits: Sherrington CS (1906) *The Integrative Action of the Nervous System*; Liddell EGT, Sherrington CS (1924) Proc R Soc B 96:212;
Lloyd DPC (1943) J Neurophysiol 6:111; Eccles JC, Eccles RM, Lundberg A (1957) J Physiol 137:22; Henneman E (1957) Science 126:1345;
Kernell D (1965) Acta Physiol Scand 65:65; Matthews PBC (1972) *Mammalian Muscle Receptors and their Central Actions*; Henneman E, Mendell
LM (1981) in *Handbook of Physiology: The Nervous System II*; Matthews PBC (1991) Trends Neurosci 14:87 (the human stretch reflex, its latencies and the motor cortex).

## v0.3: concepts, modes, variants (item D)

* **Concepts:** none new (recruitment and reciprocal_inhibition are taught by the micros and used here). Level kind `task`, on the spine.
* **Modes** (guided / standard / researcher): Latency <= 62 / 50 / 42.5 ms, resistance >= 0.24 / 0.30 / 0.35, antagonist suppression >= 0.16 / 0.20 / 0.24. The structure of the pass test never changes; the reference circuit passes every mode (researcher on seeds 1-5; `tasks_modes_test`).
* **Variants** (optional branches, never weaken the base bar): half_neurons (par 26); heavy_load (3.5 N m pushes).
* **Hints:** three tiers from `score().parts` (concept, then the place: a channel or population id, then a partial recipe).
* **Quality metric:** `latency_ms`. **Notebook:** Lloyd 1943; Eccles 1957.

### v0.3 fix round: recipe() and the novice bench
The numbers moved out of the brief into `Task::recipe()` (Guided shows it inline, Standard behind SHOW RECIPE, Researcher never); `default_model()` is `iaf_psc_alpha`, the cell the recipe assumes.
The recipe was re-measured on a bench built as the lab builds one (`novice_bench` in `engine/tests/tasks_spinal_test.cpp`: default part, delay 2 ms, all-to-all links, recipe numbers only): drive 320 (was 280), Ia > MN 240 with a weight spread (CV) of 0.3 (was 320, no spread), INs unchanged. The old numbers passed 4 of 6 seeds, the new ones 10 of 10 (seeds 1-10, Standard; worst margin: antagonist suppression 0.21 against 0.20, resistance 0.31 against 0.30). Without the spread the pool is recruited all at once, so some seeds co-contract; with it the reflex is graded. More drive (340) fails 7 of 10 seeds: both pools fight.
The test checks the bench on seeds 1-5.

## v0.4: the honesty contract (docs/REQ-0.4.md)
- **Mechanism check.** Before the behaviour, the task reads the circuit structure (`Engine::structure()`, captured when the run starts) and requires each Ia afferent group excites its own motoneuron pool through at most one projection (a monosynaptic reflex), and each Ia reaches the opposite pool through an inhibitory interneuron (reciprocal inhibition). A circuit that lacks it fails with a verdict naming the missing mechanism, and its value is capped at 40.
- **No numbers in verdicts or hint tiers 1-3.** Failed verdicts and the three hint tiers describe the observed behaviour and the direction to change; the numbers are only in `recipe()`. Pass messages still report observed values.
- **Negative tests.** `engine/tests/cheap_rows_b.inc` (run by `tasks_cheap_test`): empty bench, one cell with the lab's first-connect weights, one tonic cell with every weight 0, the reference with every weight 0, the reference with every weight at an extreme, and the reference minus the taught mechanism, each must fail with the mechanism named.
- **Non-circular novice.** Brief and lab defaults only, at most 8 runs: solved in 8 runs (sensors straight onto the pools, an inhibitory group each side; coarse grid of drive, Ia weight, inhibition).
- **Speed.** Quick check (`Task::quick_duration_ms()`, 8.5 s instead of 13.5 s): the taps and the first two stretches with the second one's ringing window; the same rules (two stretches judged, a full run judges four).
