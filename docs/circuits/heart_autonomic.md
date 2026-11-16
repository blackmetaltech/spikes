# Reference circuit: autonomic control of heart rate (`circuits/heart_autonomic.json`)

Task `heart_autonomic` (engine/src/tasks/heart_autonomic.cpp, chapter "heart", view "heart", order 210). The SA node's own rate is about 68/min;
vagal acetylcholine (ACh) slows it, sympathetic noradrenaline (NE) speeds it up. The player builds both autonomic branches as two-neuron chains
(preganglionic cell -> postganglionic ganglion cell that releases the transmitter) and the pacemaker, and makes the heart follow a target
schedule.

## Task
| time | target heart rate |
|---|---|
| 0-20 s | 62 bpm (rest: vagal tone) |
| 20-30 s | ramp up to 100 bpm (exercise starts) |
| 30-55 s | 100 bpm |
| 55-70 s | ramp down to 62 bpm (recovery) |
| 70-85 s | 62 bpm |

Sensors (Poisson rates in Hz, set every slice): `exercise_cue` = 100 Hz x workload (central command; feed-forward, `(T-62)/38`), `hr_too_low` =
100 Hz x clamp((T - HR)/30 bpm, 0, 1) and `hr_too_high` = the same for the other sign (feedback; HR is the beat-based rate low-passed with tau 1 s).
Motors: `sa_beat` (SA spikes, each a beat), `vagal` (kind `modulator`, ACh, from the cardiac ganglion cells) and `sympathetic` (kind `modulator`, NE,
from the stellate ganglion cells). The task reads `Engine::modulator("ACh")` / `("NE")`; the circuit's `modulation` table maps them onto the SA cells.

**Score**: `value = 100 clamp(1 - mae/15 bpm, 0, 1)` with `mae` the mean |HR - target| over 8-85 s (HR sampled every 100 ms from the beat
intervals; a pause counts as a miss). **Pass**: `mae` <= 6 bpm, mean rest rate (12-20 s) within 62 +- 4, mean exercise rate (40-55 s) within 100 +- 8,
mean recovery rate (75-85 s) within 62 +- 4. Extra parts: `peak_hr`, `max_error_bpm`, `rest_rmssd_ms` (HRV at rest), `ach_rest`, `ne_peak`, `map_peak_mmHg`.
In the heart body the NE level sets contractility (`sym_cardiac = 0.2 + 0.25 (NE - 0.2)`) and active muscle vasodilates (`sym_vascular = 0.2 - 0.1 w`):
MAP peaks at about 105 mmHg in exercise.

## Wiring (37 neurons)
```
exercise_cue --> CC (4) --inh--> NA (6, tonic) --1:1--> VG (6) --> vagal (ACh, tau 0.5 s) --> SA cell
                  |                  ^ exc
                  | exc          HIGH (4) <-- hr_too_high
                  v                  |inh                    SA --> sa_beat
               IML (6, tonic) <--exc-- CC, LOW (4) <-- hr_too_low;  inh <-- HIGH
                  --1:1--> SG (6) --> sympathetic (NE, tau 1.5 s) --> SA cell
```
| Population | Model | Role |
|---|---|---|
| CC, LOW, HIGH (4 each) | `iaf_psc_alpha` | sensor relay cells (input weight 800 pA); central command and the two error signs |
| NA (6) | `iaf_psc_alpha`, `I_e` 376 pA | nucleus ambiguus / DMV cardiac vagal preganglionic neurons, about 10 Hz of tone |
| VG (6) | `iaf_psc_alpha` | intracardiac ganglion cells, 1:1 nicotinic relay (one-to-one, 1500 pA); release ACh: `vagal`, gain 0.01 per spike, tau 500 ms |
| IML (6) | `iaf_psc_alpha`, `I_e` 368 pA (subthreshold at rest) | sympathetic preganglionic neurons (intermediolateral column) |
| SG (6) | `iaf_psc_alpha` | stellate / cardiac sympathetic postganglionic neurons, 1:1 relay; release NE: `sympathetic`, gain 0.0035, tau 1500 ms |
| SA (1) | `sa_node_pacemaker`, `V_m` -60 mV | the pacemaker; `modulation`: ACh -> `ach`, NE -> `ne` (gain 1) |

Synapses (static, delay 1 ms, pA): CC -> NA -8, HIGH -> NA +6, LOW -> NA -6, CC -> IML +14, LOW -> IML +10, HIGH -> IML -14. The two ganglionic relays are
one-to-one with 1500 pA, so that every preganglionic spike fires its ganglion cell (as at a nicotinic ganglionic synapse). They have to be: an
all-to-all 300 pA relay worked only when the presynaptic cells happened to fire in bunches, and failed on some seeds.

## Why it works
* **Rest**: NA fires at its tonic rate, VG releases ACh to about 0.49, IML is silent (NE ~ 0): the SA cell runs at 62.7/min. ACh in 0.4-0.9 sits on the flat
  part of the cell's ACh curve (61-63/min), so rest does not depend on the exact tone.
* **Exercise**: the cue withdraws vagal tone at once (CC inhibits NA: ACh falls from 0.49 to about 0 as the ramp proceeds, HR goes to the intrinsic 68) and drives IML (NE rises over
  the 1.5 s release time constant to 0.75: 100 beats/min). This is the textbook order, vagal withdrawal first and sympathetic activation second
  (Robinson et al. 1966; Rowell & O'Leary 1990). The feedback channels trim the feed-forward: `hr_too_low` adds to IML and subtracts from NA,
  `hr_too_high` does the reverse.
* **Recovery**: the cue falls with the target; ACh returns within seconds (fast vagal reactivation, Imai et al. 1994) while NE decays with its 1.5 s tau,
  so the heart returns to 62.7/min by 70-75 s. The asymmetric time constants (ACh 0.5 s, NE 1.5 s) are the autonomic time scales (Levy 1971).
* **The arrest range**: ACh above about 0.9 stops the cell (a cliff, `docs/models/sa_node_pacemaker.md`), so the vagal gain has to be moderate: with the first
  tuning (gain 0.02, ACh 1.0 at rest) recovery overshoot gave 30 bpm pauses.
* Results on seeds 1-4: value 85-86, `mae` 2.05-2.22 bpm, rest 62.7, exercise 99.4-100, recovery 62.7, peak 101, `ne_peak` 0.76-0.78, `ach_rest` 0.49.

## What breaks it (tests in `engine/tests/tasks_heart_test.cpp`, seeds 1 and 2)
| Lesion | Result | Expected biology |
|---|---|---|
| NA -> VG x0 (vagus cut) | rest 68.0 (target 62): "resting tachycardia", `ach_rest` 0, exercise still reached | atropine or vagotomy raises resting HR to the intrinsic rate (Jose & Collison 1970) |
| IML -> SG x0 (sympathetic chain cut) | exercise only 68 bpm (vagal withdrawal alone), `ne_peak` 0, value 0 | beta-blockade / sympathectomy: the exercise rate is limited to the intrinsic rate |
| CC, LOW -> IML x0 | exercise 68 bpm | no drive to the sympathetic preganglionic cells |
| NA -> VG x3 | `ach_rest` 0.86, pauses, rest 54-57 bpm, recovery down to 57-62 | too much ACh: the node pauses (vagal arrest, bradycardia) |
| CC -> IML x0 alone (not in the CTest) | exercise 86 bpm, `ne_peak` 0.33, fails | the error feedback alone raises NE, but a proportional loop keeps a steady-state error |
| CC -> NA x0 alone (not in the CTest) | still passes (value 78): `hr_too_low` inhibits NA instead, only slower | redundancy of the two routes to vagal withdrawal |

Credits: Jose AD, Collison D (1970) Cardiovasc Res 4:160; Robinson BF, Epstein SE, Beiser GD, Braunwald E (1966) Circ Res 19:400; Levy MN (1971) Circ Res
29:437; Rowell LB, O'Leary DS (1990) J Appl Physiol 69:407; Imai K et al. (1994) J Am Coll Cardiol 24:1529; Task Force (1996) Circulation 93:1043.

## Limits
The reduced SA cell's ACh effect is small (68 -> 61/min at ACh 0.9, then arrest), while a human's vagal range is much larger; the schedule therefore uses a
resting rate of 62 and a peak of 100 (the model's NE range ends at about 111/min), not 50-60 and 170. There is no accentuated antagonism.
