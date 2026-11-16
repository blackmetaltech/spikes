# Reference circuit: lamprey half-centre swimmer (`circuits/lamprey_cpg.json`)

Task `lamprey` (engine/src/tasks/lamprey.cpp): a 10-segment body whose segment bends follow (left - right) motor rate with
a 36 ms lag per segment; a toy reactive-thrust model (inspired by, not equal to, Lighthill 1960). Pass: BOTH sides modulate (peak-to-peak >= 2 Hz
each and the weaker >= 0.3 of the stronger, also at the swim frequency), 1-10 Hz, left/right within 40 degrees of 180,
speed >= 0.25 BL/s. Frequency uses the motor rates smoothed an extra 40 ms and 25 % hysteresis, so spike ripple is not a rhythm;
phase is only reported when both sides are coherent. Verdicts say when a motor channel never fired or is not wired.

## Biology
The lamprey spinal cord generates the swimming rhythm locally (fictive swimming 0.3-10 Hz; Wallen & Williams 1984).
Each half-cord has glutamatergic excitatory interneurons (E) that excite everything on their own side and
glycinergic commissural interneurons (CC) that cross and inhibit the other side; reciprocal inhibition plus slow
spike-frequency adaptation make a **half-centre oscillator**, left and right bursting in antiphase
(Grillner 2003; Buchanan 2001; Wang & Rinzel 1992 for the general mechanism; Marder & Calabrese 1996; Ekeberg 1993
for a neuromechanical lamprey model). A tonic reticulospinal drive switches the oscillator on (Grillner 2003).

## Wiring (18 neurons, 9 per side)
```
drive (dc, 1 per unit) --1050 pA--> E_L, E_R                        (E = 4 x aeif_cond_alpha per side)
E_s --0.9 nS--> E_s (recurrent, no autapses)      E_s --6 nS--> CC_s (2 x aeif_cond_alpha)      E_s --2 nS--> MN_s (3 x iaf_cond_alpha)
CC_s --(-19 nS)--> E_other                        CC_s --(-28 nS)--> MN_other
```
| Population | Model | Parameters changed from NEST defaults |
|---|---|---|
| E_L / E_R | `aeif_cond_alpha` (Brette & Gerstner 2005) | a 8.4 nS, b 75 pA, tau_w 320 ms (slow adaptation, burst termination), tau_syn_ex 12 ms (NMDA-like), tau_syn_in 20 ms (glycinergic), V_m(0) -62 mV (L) / -70.6 mV (R) (pinned) |
| CC_L / CC_R | `aeif_cond_alpha` | tau_syn_ex 3.3 ms, V_m(0) pinned at -70.6 mV (E_L) |
| MN_L / MN_R | `iaf_cond_alpha` | tau_syn_ex 4.4 ms, V_m(0) pinned at -70 mV (E_L) |

Delays: 1 ms within a side, 2 ms across the midline. The `drive` input is a `dc_generator`; the task sets its amplitude
to 1.0 and the input weight (1050 pA) is the drive strength, so a player can scale the swim command by changing that weight.

## Why it works
* Both E pools get the same tonic drive (1050 pA, rheobase of `aeif_cond_alpha` is about 0.5 nA) and are mutually
  inhibited through CC. The initial V_m asymmetry (the circuit is noise free) makes one side win first.
* The winner bursts (about 50-125 ms, up to ~80 Hz), its adaptation current w (b = 75 pA per spike, tau_w = 320 ms)
  accumulates, and its CC cells also fire, silencing the other side. When w has weakened the winner enough, the
  suppressed side escapes: the usual "release/escape" half-centre switching of Wang & Rinzel 1992.
* Result: about 2.5 Hz, left/right at 179-180 degrees, mean motor-neuron rate 15 Hz per cell, 1.3 BL/s.
* **Parameter provenance.** Neuron constants are Brette & Gerstner's defaults except adaptation and synaptic time
  constants (set in the physiological ranges: adaptation 100-500 ms, NMDA-like 10-100 ms, glycinergic IPSC
  10-20 ms). The weights and the adaptation were found by random search then local refinement (a few hundred simulations,
  scored with the task itself); the pass region is wide in the drive: 90 % to 140 % of 1050 pA all pass at ~2.5 Hz,
  below 80 % the rhythm becomes irregular and fails the phase test.

## What breaks it (tests in `engine/tests/tasks_circuits_test.cpp`)
| Lesion | Result |
|---|---|
| Delete both CC -> E projections (no reciprocal inhibition) | the two sides fire together: phase 7 degrees, fail |
| Delete one crossing | phase 26 degrees, fail |
| E -> CC x0 (CC silent) | in phase, fail |
| E -> MN x0 | no motor output, fail |
| E_R -> MN_R x0 (or E_L -> MN_L) | one-sided rhythm: the silent side is named, no phase, fail |
| CC -> E x0.3 | still passes (3 Hz, 180 degrees): the half-centre tolerates weak inhibition |

Credits: Brette R, Gerstner W (2005) J Neurophysiol 94:3637; Wang X-J, Rinzel J (1992) Neural Comput 4:84; Marder E,
Calabrese RL (1996) Physiol Rev 76:687; Grillner S (2003) Nat Rev Neurosci 4:573; Buchanan JT (2001) Prog Neurobiol
63:441; Wallen P, Williams TL (1984) J Physiol 347:225; Ekeberg O (1993) Biol Cybern 69:363; Lighthill MJ (1960)
J Fluid Mech 9:305.

The thrust model is a toy (T = 1/2 m v_tail^2 (1 - U/c)), not Lighthill's large-amplitude theory. Reference score 99.8 on 5 seeds (the network is noise free, so seeds give identical results).
