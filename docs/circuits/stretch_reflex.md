# Reference circuit: stretch reflex (`circuits/stretch_reflex.json`)

Task `stretch_reflex` (`engine/src/tasks/stretch_reflex.cpp`, chapter "spinal", order 100, view "limb"). A tendon is tapped and the forearm
is pushed by four sudden loads; the circuit must answer with a spinal-latency reflex, cut the deflection, switch the opposing muscle
off and not ring. The generator is `circuits/tools/gen_spinal.py` (`python3 circuits/tools/gen_spinal.py stretch_reflex`).

## The body (shared by the four spinal tasks, `engine/src/tasks/spinal_limb.h`)
`body::Joint`: an elbow-like hinge with a flexor (agonist) and extensor (antagonist) Hill muscle (`docs/body/muscle.md`, `joint.md`),
supported (no gravity), inertia 0.1 kg m^2, viscosity 0.3 N m s/rad, no fatigue. Spindle Ia, II and Golgi Ib rates come from the joint
and go to the circuit's sensor channels (Hz, `dc` inputs) after a 10 ms conduction delay; the mean rate per neuron of each alpha
motoneuron pool, low-passed with tau 15 ms, divided by 50 Hz and delayed 10 ms is the muscle's pool excitation. The `Muscle` then
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
Pass: latency <= 50 ms, resistance >= 0.35, antagonist suppression >= 0.25, no clonus. Measured on seeds 1-3:

| Seed | Value | Latency | Resistance | Antagonist suppression | Ringing (rad p-p) |
|---|---|---|---|---|---|
| 1 | 90.1 | 23 ms | 0.58 | 0.30 | 0.026 |
| 2 | 95.6 | 24 ms | 0.68 | 0.41 | 0.013 |
| 3 | 89.4 | 24 ms | 0.70 | 0.29 | 0.024 |

## Why it works, and what the numbers came from
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
| Ia -> IN cut (both pools) | the antagonist keeps firing (suppression 0.05-0.14), co-contraction: resistance 0.28-0.34 (< 0.35), "add reciprocal Ia inhibition" |
| IN -> MN cut | same as above |
| Ia -> MN x4 | hyper-reflexia: the pools saturate (77-82 Hz), resistance 0.00, suppression 0.07-0.12 |

Credits: Sherrington CS (1906) *The Integrative Action of the Nervous System*; Liddell EGT, Sherrington CS (1924) Proc R Soc B 96:212;
Lloyd DPC (1943) J Neurophysiol 6:111; Eccles JC, Eccles RM, Lundberg A (1957) J Physiol 137:22; Henneman E (1957) Science 126:1345;
Kernell D (1965) Acta Physiol Scand 65:65; Matthews PBC (1972) *Mammalian Muscle Receptors and their Central Actions*; Henneman E, Mendell
LM (1981) in *Handbook of Physiology: The Nervous System II*; Matthews PBC (1991) Trends Neurosci 14:87 (the human stretch reflex, its latencies and the motor cortex).
