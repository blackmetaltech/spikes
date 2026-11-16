# Reference circuit: Renshaw recurrent inhibition (`circuits/renshaw.json`)

Task `renshaw` (`engine/src/tasks/renshaw.cpp`, chapter "spinal", order 110). The stretch-reflex circuit plus Renshaw cells. The cord is
driven by a noisy 8-12 Hz descending input shared by both motoneuron pools; the task scores how much the recurrent inhibition cuts the
resulting force tremor, and checks that the reflex still works. Generator: `python3 circuits/tools/gen_spinal.py renshaw`.
The limb, spindles, delays and muscle are those of `stretch_reflex.md`; the only change is that a pool at 70 Hz per cell is fully excited
(`rmax_hz = 70`), so that 25-45 Hz stays on the steep part of the muscle's force curve.

## Wiring (54 neurons)
```
ag_Ia / ant_Ia --> Ia_F / Ia_E (5 each) --6 nS--> MN_F / MN_E (16 each) --> ag_motor / ant_motor
                                              `--3 nS--> IN_F / IN_E (2 each) --(-9 nS)--> opposite MN
MN_F --3 nS--> RC_F (3) --(-10 nS)--> MN_F ;  RC_F --(-3 nS)--> IN_F ;  RC_F --(-3 nS)--> RC_E     (and the mirror image for the extensor)
noise (poisson, Hz) --> DESC (2 parrot fibres) --2 nS x U(0.2, 1.8)--> every MN_F and MN_E (common input)
drive (dc 1.0) --1000 pA--> MN_F, MN_E
```
Renshaw cells (`iaf_cond_alpha`, fast) are excited by the collaterals of their own pool's motoneurons (cholinergic, Eccles, Fatt & Koketsu
1954), inhibit those motoneurons (glycinergic, Renshaw 1941), inhibit the Ia inhibitory interneurons of the same muscle (Hultborn, Jankowska &
Lindstrom 1971) and the Renshaw cells of the antagonist (Ryall 1970). RC rate in the reference is 65-80 Hz at a motoneuron rate of 30 Hz.

## Score
`tremor_pct` is the lock-in amplitude of the summed muscle force at the three drive frequencies (8.5, 10, 11.5 Hz; a whole number of cycles
fit the 6 s hold), root-sum-square, over the mean force. (Spike noise of the pool drops out; a plain high-pass rms was dominated by it and
RC could not change it. Force is summed over both muscles because common input moves both in phase: in the net joint torque it cancels.)
`improvement = 1 - tremor / tremor_ref(rate)`, where `tremor_ref` is a measured **control curve**: the same reference circuit with the
Renshaw cells cut, at tonic drives of 450-1000 pA (mean of seeds 1-3):

| pool rate (Hz) | 16.6 | 18.5 | 21.8 | 24.1 | 27.0 | 30.1 | 33.1 | 36.5 | 39.6 | 42.6 | 45.5 | 48.0 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| tremor (%) | 10.0 | 9.1 | 6.1 | 5.8 | 5.7 | 5.1 | 4.3 | 3.5 | 2.9 | 2.2 | 1.5 | 1.1 |

(Tremor falls with rate because the muscle's force curve flattens, so judging against a fixed number would reward any lesion that raises
the rate: removing Renshaw inhibition raises it from 30 to 48 Hz, which alone would cut the tremor to 1.1 %. Judged against the control
curve at the same rate, the cut circuit scores no improvement.) Pass: improvement >= 0.25, stretch resistance >= 0.30, no ringing, pools
firing >= 5 Hz. Seeds 1-3:

| Seed | Value | Pool rate | Tremor | Control at this rate | Improvement | Resistance |
|---|---|---|---|---|---|---|
| 1 | 100 | 30.3 Hz | 1.89 % | 5.04 % | 0.63 | 0.66 |
| 2 | 100 | 30.8 Hz | 2.26 % | 4.90 % | 0.54 | 0.64 |
| 3 | 100 | 32.0 Hz | 1.52 % | 4.60 % | 0.67 | 0.57 |

## Parameter provenance
* Recurrent loop gain from a closed-loop sweep of w_MN_RC (0.5-6 nS), w_RC_MN (-1 to -20 nS) and drive (650-1200 pA): weak settings
  (RC -> MN of -1 to -2 nS) cut the tremor only 10-15 % at equal rate; -10 nS with 3 nS MN -> RC cut it 55-67 % while the motoneuron rate fell
  only from 36 to 30 Hz (the drive was raised to 1000 pA to compensate). Stronger loops (-20 nS) reduce the pool rate further and start to
  smother the reflex (resistance 0.33).
* The common source is two parrot fibres at 400 Hz (modulated +-60 % at three frequencies by the task) with 2 nS x U(0.2, 1.8) edges; one
  fibre with 4 nS gave similar tremor; four fibres of 1 nS gave much less common variance.

## What breaks it (tests in `engine/tests/tasks_spinal_test.cpp`)
| Lesion | Result |
|---|---|
| MN -> RC cut (Renshaw cells never driven) | pool rate 48 Hz, tremor 0.96-1.1 % against a control of 1.1 %: improvement 0.004-0.14 |
| RC -> MN cut (Renshaw cells fire, cannot inhibit) | pool rate 58 Hz, saturated: improvement 0, stretch resistance 0.25-0.27 |

Credits: Renshaw B (1941) J Neurophysiol 4:167; Eccles JC, Fatt P, Koketsu K (1954) J Physiol 126:524; Ryall RW (1970) J Neurophysiol 33:257;
Hultborn H, Jankowska E, Lindstrom S (1971) J Physiol 215:591, 613; Hultborn H (2006) Prog Neurobiol 78:215 (spinal reflexes: mechanisms and concepts); Farmer SF et al. (1993)
J Physiol 470:127 (common drive to motor units); Elble RJ, Koller WC (1990) *Tremor* (the 8-12 Hz central drive).
