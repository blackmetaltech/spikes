# Reference circuit: breathing (`circuits/breathing.json`)

Task `breathing` (engine/src/tasks/breathing.cpp, chapter "breathing", view `lungs`, order 300). A preBotzinger complex of bursting pacemaker cells drives a phrenic motor pool; the phrenic rate drives the lung model (`docs/body/lungs.md`); blood CO2 closes the loop through chemoreceptors. The episode is 150 s of game time (gas exchange runs 5x faster than physiology, `time_scale = 5`; steady states are unchanged).

## World and score
- **Sensors.** `drive`: a constant 200 Hz Poisson train (brainstem wakefulness drive, tiny weight; mainly gives every burster its own noise). `chemo`: a graded current in pA, `4 pA x (peripheral_hz + central_hz)` of the lung model: carotid body (0.7 + 0.45 (PaCO2 - 35) Hz, Lahiri & DeLaney 1975) plus central chemoreceptors (1.2 (PCO2_tissue - 38) Hz, Guyenet et al. 2010). Rest is about 13 Hz, 51 pA. The afferent population is summarised as one smooth drive, so it is a `dc` input.
- **Motor.** `phrenic`: mean rate per phrenic motoneuron. Drive `u = rate / 225 Hz` (clamped to 0-1) goes to the diaphragm: `Pmus = 20 cmH2O u`, 80 ms low-pass, passive expiration.
- **Perturbation (timeline).** 50-110 s: 5 % CO2 added to the inspired air (inspired PCO2 30 mmHg, 5 s ramps). With no chemoreflex PaCO2 would go to `30 + 863 VCO2/VA = 71 mmHg`.
- **Score.** Rest window 12-48 s: breaths per minute from breath completions, mean PaCO2. Challenge window 85-110 s: ventilation (sum of tidal volumes per minute). Pass: 12-20 breaths/min with at least 5 breaths, ventilation x1.6 or more, resting PaCO2 35-45 mmHg, peak PaCO2 52 mmHg or less, PaCO2 46 mmHg or less at 135-150 s. `value = 100 (0.25 rate + 0.30 ventilation + 0.30 PaCO2 control + 0.15 recovery)`, each a soft 0..1 score.

## Wiring (28 neurons, par 26)
```
chemo (dc, pA) -> RTN x6 --0.09 nS--> PBC x10 <-0.03 nS-> PBC (recurrent excitation)
                      \--8 pA--> PHR x8 --> phrenic
drive (Poisson) -> PBC          PBC --2.2 pA--> PHR      PBC --(-60 pA)--> EXP x4 --(-30 pA)--> PHR
```
| Population | Model | Role |
|---|---|---|
| RTN (6) | `iaf_psc_alpha`, I_e 335 pA | retrotrapezoid / NTS chemosensory relay: current-driven, rate rises with PaCO2 (about 8 Hz at rest, 35 Hz at +6 mmHg). Its two outputs are the "frequency" path (to the burst network) and the "depth" path (direct to the premotor/phrenic cells). Initial V_m is random, so the six cells are out of phase |
| PBC (10) | `butera_burster` (NESTML), g_NaP 2.8 nS, g_tonic 0.29 nS | preBotC persistent-sodium bursting pacemakers (Butera, Rinzel & Smith 1999, model 1; `docs/models/butera_burster.md`). Weak all-to-all excitatory coupling (0.03 nS) synchronises them into one population burst of about 0.6 s |
| EXP (4) | `iaf_psc_alpha`, I_e 430 pA, tau_syn_in 10 ms | expiratory / post-inspiratory cells: tonically active (37 Hz), they hold the phrenic pool hyperpolarised between breaths; the burst network inhibits them, which releases ("disinhibits") the phrenic pool during inspiration |
| PHR (8) | `iaf_psc_alpha`, I_e 380 pA, 30 ms synapses | phrenic premotor / motoneurons: silent between bursts, 60-80 Hz plateaus within them |

## Why it works
* **Rhythm.** At g_tonic 0.29 nS an isolated Butera cell bursts every 4-5 s. Excitatory coupling makes the 10 cells fire one common burst (no coupling: the bursts smear out and the phrenic output is no longer a breath). Open-loop (fixed chemo current), the period falls from 4.2 s at the resting 51 pA to 2.6 s at 90 pA and 1.9 s at 140 pA, i.e. 14 to 32 breaths per minute. Drive to the burster is the only thing that sets the rate, as in the paper's g_tonic-e map.
* **Depth.** The burst gets shorter as drive rises (the cell moves toward tonic firing), so burst-network output alone would give smaller breaths at high rates. The chemo relay also excites the phrenic cells directly (8 pA, 0.09 nS to PBC), which raises the burst height from 60 to 80 Hz. Minute ventilation (rate x depth) roughly doubles for a +6 mmHg PaCO2 change.
* **Loop.** At rest the loop settles at PaCO2 about 39 mmHg and 13-17 breaths/min (VE 6.5-7 L/min). With 5 % CO2 inspired ventilation rises 1.9-2.3x and PaCO2 peaks at 49-50 mmHg (a real subject: a 3-6 mmHg rise); it returns to 39 mmHg afterwards.
* **Parameter provenance.** Butera parameters are the paper's. The rest (weights, I_e, the 225 Hz phrenic reference, 4 pA/Hz chemo gain) were tuned open-loop first (burst period vs chemo current with the lungs removed) and then closed-loop with the task. The chemoreflex gain is a game value.
* **Seeds.** The circuit's seed reseeds the initial membrane potentials and the drive noise; the reference passes 10/10 seeds (1-10): rest rate 13.3 /min throughout, ventilation ratios 2.14-2.29, PaCO2 peak 48.7-49.3 mmHg.
* **Building it as a novice.** Default weights of 1.0 are far too strong for this chapter: one burster alone with no coupling never drives the phrenic pool, and ten bursters coupled at 1 nS fire 168 breaths/min. The brief therefore gives the working recipe (units: burster synapses in nS, iaf in pA): ~10 `butera_burster` (g_NaP 2.8, g_tonic 0.29 nS) coupled at ~0.03 nS, `drive` at 0.005, phrenic iaf cells with I_e ~380 pA excited at ~2 pA by PBC, tonic expiratory cells (I_e ~430 pA) that PBC inhibits (-60) and that inhibit PHR (-30), `chemo` through relay cells to PBC (0.09 nS, frequency) and PHR (8 pA, depth). The verdicts say which way to move: rate over 20 -> weaken coupling or drive; under 12 or few breaths -> raise g_tonic (0.30 nS = a burst every 4.9 s, 0.35 = 2.6 s); phrenic silent -> phrenic I_e and the PBC->PHR weight. A bare PBC -> PHR circuit (iaf phrenic cells without I_e) does not fire, which is why the recipe asks for the standing current.

## What breaks it (tests in `engine/tests/tasks_autonomic_test.cpp`)
| Lesion | Result |
|---|---|
| RTN -> PBC and RTN -> PHR x0 (chemoreceptor afferent cut) | resting rate 8-10 /min, ventilation x1.1-1.2 under CO2, PaCO2 peak 80-87 mmHg: no CO2 response |
| PBC -> PBC x0 (bursters uncoupled) | the cells burst out of phase; ventilation x1.05, PaCO2 peak 82 mmHg |
| PBC -> EXP x0 (no disinhibition) | phrenic pool never fires: apnoea, PaCO2 above 120 mmHg |
| EXP -> PHR x0 (no expiratory silence) | phrenic fires tonically: 45 /min of shallow breaths, no ventilation |

The task also says plainly when the sensors are not connected ("sensor 'chemo' is not connected to any neuron").

## Limits
No O2 sensing or hypoxic drive, no Hering-Breuer stretch feedback, no active expiration. The chemoreceptor signal is a graded current rather than spikes. The preBotC here is a network of identical bursters with excitatory coupling only; the inhibitory interneurons that real preBotC networks have (Janczewski et al. 2013) are not modelled inside it, expiratory inhibition lives in the EXP cells.

Credits: Smith JC, Ellenberger HH, Ballanyi K, Richter DW, Feldman JL (1991) Science 254:726; Butera RJ, Rinzel J, Smith JC (1999) J Neurophysiol 82:382; Feldman JL, Del Negro CA (2006) Nat Rev Neurosci 7:232; Guyenet PG, Stornetta RL, Bayliss DA (2010) Respir Physiol Neurobiol 173:244; Lahiri S, DeLaney RG (1975) Respir Physiol 24:249; Janczewski WA et al. (2013) J Neurosci 33:3045; Grodins FS et al. (1967) J Appl Physiol 22:260 (gas exchange).
