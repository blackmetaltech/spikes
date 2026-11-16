# Reference circuit: fight or flight (`circuits/fight_or_flight.json`)

Task `fight_or_flight` (engine/src/tasks/fight_or_flight.cpp, chapter "autonomic", view `dashboard`, order 420). One threat cue, four organs: heart (Heart body, SA-node cell), blood pressure (Windkessel), pupil (Eye) and ventilation (Lungs). The episode is 100 s: rest to 20 s, threat 20-50 s (`threat` Poisson 250 Hz, 0.5 s ramp), safe 50-100 s.

## World and score
- **Motors.** `sa_beat`: spikes of the SA cell (`sa_node_pacemaker`) are the heartbeats. `sympathetic` and `vagal` are *modulator* outputs from the stellate-ganglion and cardiac vagal cells (NE gain 0.0015 per spike, tau 1.5 s; ACh gain 0.007, tau 0.5 s); the circuit's `modulation` table maps the engine's NE and ACh onto the SA cell's `ne` and `ach` (`docs/CIRCUIT-FORMAT.md`). `sym_vascular` (Hz / 100) sets vessel resistance, contractility is `0.2 + 0.2 NE/(NE + 0.3)`. `dilator` and `sphincter` (Hz / 40 each) drive the iris at a fixed 30 cd/m2. `vent` (Hz / 40, smoothed 1 s) sets the rate (`10 + 12 d` /min) and depth (`0.18 + 0.2 d`) of a simple respiratory pattern generator that drives the Lungs (the CPG itself is the `breathing` task). All rates are low-passed before any clamp.
- **Readouts for the dashboard.** `hr_bpm, beat, map_mmhg, sys_mmhg, dia_mmhg, stroke_ml, pupil_mm, ventilation, rr_per_min, volume, co2_mmhg, NE, ACh, DA, sympathetic_hz, parasympathetic_hz, threat`; the bands (`hr_bpm 55-80`, `map_mmhg 80-105`, `pupil_mm 3-6`, `ventilation 4-9`) are the resting targets the trajectories must leave during the threat and come back to.
- **Score.** Per system (heart rate, mean arterial pressure, pupil diameter, ventilation): (1) the rest mean over 12-20 s lies in its band, (2) the threat mean over 40-50 s exceeds rest by 25 bpm, 12 mmHg, 1.2 mm and x1.4, (3) the mean over 90-100 s is back within 8 bpm, 8 mmHg, 0.6 mm and x1.25 of rest. Twelve conditions in all, each also a soft 0..1 score: `value = 100 x mean`. Pass: all twelve.

## Wiring (45 neurons, par 45)
```
threat -> HYP x4 --120 pA--> RVLM x4 --60--> SG x6  => NE   (modulator, SA ne)  \
                                       \--30--> SV x4  => sym_vascular           } sympathetic
                                        \--60--> SCG x4 => dilator              /
          HYP --(-150)--> NA x4 --60--> VG x6 => ACh (modulator, SA ach)  \ parasympathetic: tone,
          HYP --(-150)--> EW x4 --60--> CG x4 => sphincter                /  withdrawn by the threat
          HYP --100--> VRG x4 => vent
SA x1: sa_node_pacemaker, NE -> ne, ACh -> ach (modulation table)
```
| Population | I_e (pA) | Role / numbers |
|---|---|---|
| HYP (4) | 100 | hypothalamic defence area: silent at rest, about 45 Hz during the cue (tau_syn_in 10 ms) |
| RVLM (4) | 330 | rostral ventrolateral medulla premotor cells: silent at rest, about 46 Hz during the cue |
| SG (6), SV (4), SCG (4) | 340, 377, 377 | sympathetic ganglion cells for the heart (silent at rest: NE about 0), vessels (about 18 Hz of resting tone, sym 0.18) and iris dilator (18 Hz tone) |
| NA (4), VG (6) | 377, 340 | nucleus ambiguus and cardiac vagal ganglion: 13-18 Hz tone, ACh about 0.37 at rest, 0 during the threat |
| EW (4), CG (4) | 377, 340 | Edinger-Westphal and ciliary ganglion: tonic sphincter drive, withdrawn by the cue |
| VRG (4) | 377 | ventral respiratory group: 18 Hz tone, about 54 Hz during the cue |
| SA (1) | | `sa_node_pacemaker`: 63 bpm with ACh 0.37 and NE 0 |

## What the numbers do (3 seeds, same to within noise)
rest -> threat -> recovery: HR 63.2 -> 97 -> 63.2 bpm (rest ACh 0.37; during the cue ACh 0 and NE 0.5); MAP 88 -> 129 -> 88 mmHg; pupil 4.3 -> 7.5 -> 4.1 mm; ventilation 7.0 -> 16.4 -> 7.0 L/min (x2.3). Heart rate first rises by vagal withdrawal and then with NE (the NE pool has a 1.5 s time constant); recovery is vagal reactivation (ACh tone returns in about 0.5 s) and slower NE washout. Scores 99 (value weights 0.2 rest, 0.6 response, 0.2 recovery per system; failing lesions score 76-87).

## Why this wiring
* The SA cell's response is the NESTML one: ACh 0.37 slows 68 to about 63 bpm, NE 0.3-0.5 with ACh 0 gives 92-100 bpm (`docs/models/sa_node_pacemaker.md`); the stellate ganglion is *silent* at rest so the resting heart is under vagal control and the sympathetic response is a clear step. Vessels and iris carry their own sympathetic tone (SV, SCG at 18 Hz).
* The cue acts on both sides of the autonomic balance: excitation of RVLM and VRG, and *inhibition* of the vagal (NA) and Edinger-Westphal (EW) tone. A hypothalamus that only excited the sympathetic side would still pass the heart and pressure bands but give a smaller HR and pupil response (see lesions below).
* **Pressure.** The body model gives a MAP rise mainly through cardiac output (HR x stroke volume) plus resistance; the vasomotor pool is deliberately modest (30 pA from RVLM) so MAP stays under 150 mmHg (the test asserts that).

## What breaks it (tests in `engine/tests/tasks_autonomic_test.cpp`)
| Lesion | Result |
|---|---|
| HYP -> RVLM x0 | no sympathetic outflow: heart rate rises only from vagal withdrawal (63 -> 68 bpm), MAP +3 mmHg; the pupil still opens by 1.9 mm through Edinger-Westphal withdrawal: fails |
| RVLM -> SG x0 | no NE on the pacemaker: HR +4.8 bpm, below 25: fails (pressure and pupil still respond) |
| HYP -> VRG x0 | ventilation unchanged (x1.0): fails |
| RVLM -> SCG and HYP -> EW x0 | pupil fixed (4.3 mm), the rest of the response intact: fails |
Milder cuts do not fail the task and show up as smaller responses: HYP -> NA x0 (vagal not withdrawn: HR 92 instead of 97 bpm), HYP -> EW x0 alone (pupil 5.6 mm instead of 7.5), RVLM -> SV x0 (MAP 112 instead of 129).

## Limits
No baroreflex in this circuit (the baroreflex task is separate), so pressure is not regulated; no metabolic CO2 production change with the fight, so PaCO2 falls with hyperventilation but nothing feeds back on it; the premotor and ganglion levels of each sympathetic branch are lumped (RVLM -> ganglion cells). The recovery is passive (decay and vagal rebound), not an active parasympathetic overshoot.

Credits: Cannon WB (1915) *Bodily Changes in Pain, Hunger, Fear and Rage*; Hilton SM (1982) J Auton Nerv Syst 7:3 (defence area); Dampney RAL (1994) Physiol Rev 74:323 (central cardiovascular pathways); Robinson BF, Epstein SE, Beiser GD, Braunwald E (1966) J Appl Physiol 21:1655 (vagal then sympathetic components of the heart rate response); Steinhauer SR, Hakerem G (1992) Ann N Y Acad Sci 658:182 (arousal inhibits the EW nucleus); Demir SS et al. (1994) Am J Physiol 266:C832 (SA-node cell family).
