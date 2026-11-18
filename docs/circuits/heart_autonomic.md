# Reference circuit: autonomic control of heart rate (`circuits/heart_autonomic.json`)

Task `heart_autonomic` (engine/src/tasks/heart_autonomic.cpp, chapter "heart", view "heart", order 210). The SA node's own rate is about 100/min with no autonomic input (as after blockade);
vagal acetylcholine (ACh) slows it, sympathetic noradrenaline (NE) speeds it up. The player builds both autonomic branches as two-neuron chains
(preganglionic cell -> postganglionic ganglion cell that releases the transmitter) and the pacemaker, and makes the heart follow a target
schedule.

## Task
| time | target heart rate |
|---|---|
| 0-20 s | 65 bpm (rest: vagal tone) |
| 20-30 s | ramp up to 120 bpm (exercise starts) |
| 30-55 s | 120 bpm |
| 55-70 s | ramp down to 65 bpm (recovery) |
| 70-85 s | 65 bpm |

**The three sensors are stand-ins, not sensors.** `exercise_cue` stands for central command and muscle afferents; `hr_too_low` / `hr_too_high` hand the circuit the sign of its own heart-rate error (a controller given its error), in place of the baroreceptor and arterial-stretch afferents that would report it. The brief says so.

Sensors (Poisson rates in Hz, set every slice): `exercise_cue` = 100 Hz x workload (central command; feed-forward, `(T-65)/55`), `hr_too_low` =
100 Hz x clamp((T - HR)/40 bpm, 0, 1) and `hr_too_high` = the same for the other sign (feedback; HR is the beat-based rate low-passed with tau 1 s).
Motors: `sa_beat` (SA spikes, each a beat), `vagal` (kind `modulator`, ACh, from the cardiac ganglion cells) and `sympathetic` (kind `modulator`, NE,
from the stellate ganglion cells). The task reads `Engine::modulator("ACh")` / `("NE")`; the circuit's `modulation` table maps them onto the SA cells.

**Score**: `value = 100 clamp(1 - mae/15 bpm, 0, 1)` with `mae` the mean |HR - target| over 8-85 s (HR sampled every 100 ms from the beat
intervals; a pause counts as a miss). **Pass**: `mae` <= 6 bpm, mean rest rate (12-20 s) within 65 +- 4, mean exercise rate (40-55 s) within 120 +- 8,
mean recovery rate (75-85 s) within 65 +- 4. Extra parts: `peak_hr`, `max_error_bpm`, `rest_rmssd_ms` (HRV at rest), `ach_rest`, `ne_peak`, `map_peak_mmHg`.
In the heart body the NE level sets contractility (`sym_cardiac = 0.2 + 0.25 (NE - 0.2)`) and active muscle vasodilates (`sym_vascular = 0.2 - 0.1 w`):
MAP peaks at about 104 mmHg in exercise.

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
| VG (6) | `iaf_psc_alpha` | intracardiac ganglion cells, 1:1 nicotinic relay (one-to-one, 1500 pA); release ACh: `vagal`, gain 0.012 per spike, tau 500 ms |
| IML (6) | `iaf_psc_alpha`, `I_e` 368 pA (subthreshold at rest) | sympathetic preganglionic neurons (intermediolateral column) |
| SG (6) | `iaf_psc_alpha` | stellate / cardiac sympathetic postganglionic neurons, 1:1 relay; release NE: `sympathetic`, gain 0.0031, tau 1500 ms |
| SA (1) | `sa_node_pacemaker`, `V_m` -60 mV | the pacemaker; `modulation`: ACh -> `ach`, NE -> `ne` (gain 1) |

Synapses (static, delay 1 ms, pA): CC -> NA -8, HIGH -> NA +10, LOW -> NA -6, CC -> IML +12, LOW -> IML +10, HIGH -> IML -25. The two ganglionic relays are
one-to-one with 1500 pA, so that every preganglionic spike fires its ganglion cell (as at a nicotinic ganglionic synapse). They have to be: an
all-to-all 300 pA relay worked only when the presynaptic cells happened to fire in bunches, and failed on some seeds.

## Why it works
* **Rest**: NA fires at its tonic rate, VG releases ACh to about 0.59, IML is silent (NE ~ 0): the SA cell runs at 65/min, held down from its intrinsic ~100 by vagal tone (on the graded part of the cell's ACh curve, 0.5 -> 67, 0.7 -> 63 beats/min).
* **Exercise**: the cue withdraws vagal tone at once (CC inhibits NA: ACh falls towards 0, HR goes to the intrinsic ~100) and drives IML (NE rises over the 1.5 s release time constant to about 0.6: 120-125 beats/min). This is the textbook order, vagal withdrawal first and sympathetic activation second (Robinson et al. 1966; Rowell & O'Leary 1990). The feedback channels trim the feed-forward: `hr_too_low` adds to IML and subtracts from NA, `hr_too_high` does the reverse.
* **Recovery**: the cue falls with the target; ACh returns within seconds (fast vagal reactivation, Imai et al. 1994) while NE decays with its 1.5 s tau, so the heart is back at 65/min by 70-75 s. The asymmetric time constants (ACh 0.5 s, NE 1.5 s) are the autonomic time scales (Levy 1971).
* **Strong ACh**: the SA cell's ACh response is graded (58/min at ACh 1.0, 54 at 1.5, no cliff), so the vagal gain is forgiving, but ACh x2 still gives bradycardia.
* Results on seeds 1-10: value 77.8-81.7, `mae` 2.8-3.3 bpm, rest 65.0, exercise 124-125, recovery 65.0, peak 130-131, `ne_peak` 0.59-0.63, `ach_rest` 0.59. All pass.

## What breaks it (tests in `engine/tests/tasks_heart_test.cpp`, seeds 1 and 2)
| Lesion | Result | Expected biology |
|---|---|---|
| NA -> VG x0 (vagus cut) | rest 99 (target 65): "resting tachycardia", `ach_rest` 0, value 0 | atropine or vagotomy raises resting HR to the intrinsic rate (Jose & Collison 1970); vagal tone dominates the resting rate |
| IML -> SG x0 (sympathetic chain cut) | exercise only 99 bpm (vagal withdrawal alone), `ne_peak` 0, value 27 | beta-blockade / sympathectomy: the exercise rate is limited to the intrinsic rate |
| CC, LOW -> IML x0 | exercise 99 bpm | no drive to the sympathetic preganglionic cells |
| NA -> VG x3 | `ach_rest` 1.1, rest 58 bpm, recovery 58 | too much ACh: bradycardia |

Credits: Jose AD, Collison D (1970) Cardiovasc Res 4:160; Robinson BF, Epstein SE, Beiser GD, Braunwald E (1966) Circ Res 19:400; Levy MN (1971) Circ Res
29:437; Rowell LB, O'Leary DS (1990) J Appl Physiol 69:407; Imai K et al. (1994) J Am Coll Cardiol 24:1529; Task Force (1996) Circulation 93:1043.

## Limits
The schedule is rest 65 and exercise 120; a young adult reaches 170-190 in hard exercise, but the reduced SA cell's NE range ends at about 135/min (NE 1.0), so 120 is the realistic top the model allows. There is no accentuated antagonism. The error channels are handed to the circuit (see above), so this is a controller exercise, not a sensory one.

## v0.3: concepts, modes, variants (item D)

* **Concepts:** neuromodulation, feedforward_feedback (new). Level kind `task`, on the spine.
* **Modes** (guided / standard / researcher): Mean error / rest / exercise / recovery: 7.5 / 5 / 10 / 5, 6 / 4 / 8 / 4, 5.1 / 3.4 / 6.8 / 3.4 bpm. The structure of the pass test never changes; the reference circuit passes every mode (researcher on seeds 1-5; `tasks_modes_test`).
* **Variants** (optional branches, never weaken the base bar): half_neurons (par 23); tight_tracking (4 / 3 / 6 / 3 bpm).
* **Hints:** three tiers from `score().parts` (concept, then the place: a channel or population id, then a partial recipe).
* **Quality metric:** `mae_bpm`. **Notebook:** Jose & Collison 1970; Levy 1971.

## v0.4: mechanism check, quick check, negative tests
**Mechanism (Engine::structure()).** `vagal` and `sympathetic` must be modulator outputs (ACh / NE release) read from two different populations, and each branch must be reachable from the inputs: some path from `exercise_cue` / `hr_too_low` / `hr_too_high` to the vagal cells and some path to the sympathetic cells. A branch no input can reach is a fixed tone, not a controller; one pool serving both outputs cannot release two transmitters. The verdict names the missing branch; the value is capped at 49.

**Quick check.** `quick_duration_ms()` = 40 s: the same day compressed (rest until 13 s, ramp up 13-18 s, exercise until 26 s, ramp down 26-30 s, scored windows rest 8-13 s, exercise 22-26 s, recovery 35-40 s; the mean error counts from 6 s), same rules and pass bars. Reference: 6.2 s -> 2.8 s wall (mean error 3.8-4.2 bpm against the 6 bpm limit).

The verdicts and the tier 1-3 hints carry no parameter numbers (weights, biases, gains): those live in `recipe()` only (checked in the tests). Negative tests: `engine/tests/cheap_rows_c.inc` (run by `tasks_cheap_test`) and the mechanism cases in the level's own test file.
