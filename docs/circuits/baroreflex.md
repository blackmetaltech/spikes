# Reference circuit: baroreflex (`circuits/baroreflex.json`)

Task `baroreflex` (engine/src/tasks/baroreflex.cpp, chapter "heart", view "dashboard", order 220). The player builds the brainstem arterial baroreflex:
baroreceptor afferents -> nucleus of the solitary tract (NTS) -> nucleus ambiguus (cardiac vagal) and caudal ventrolateral medulla (CVLM) which inhibits the
tonically active rostral ventrolateral medulla (RVLM, sympathetic vasomotor drive) -> sympathetic preganglionic -> ganglion cells. The body is the heart and
circulation module (`docs/body/heart.md`).

## Task
| time | event |
|---|---|
| 0-15 s | supine |
| 15 s | stand up: blood pools in the legs (up to 220 ml, tau 6 s), venous return and MAP fall |
| 60 s | lie down: the pooled blood returns |
| 80-110 s | haemorrhage, 10 ml/s, 300 ml lost and not replaced |
| 140 s | end |

Sensor `baroreceptor`: Poisson rate in Hz set every slice to the heart's 1 s-averaged baroreceptor afferent rate (`100 Hz sigmoid((P + 0.04 dP/dt - 100)/20)`:
about 40 Hz at MAP 93, 20 Hz at 73, 15 at 64). Motors: `sa_beat`, `vagal` (kind `modulator`, ACh) and `sympathetic` (kind `modulator`, NE). The task reads the engine's
ACh and NE levels. The SA cell's rate comes from the cell (the `modulation` table maps NE and ACh onto its `ne` / `ach`, so both branches change the heart rate);
the sympathetic level also sets contractility and vascular tone through `sym = clamp(0.2 + 0.5 (NE - 0.2), 0, 1)` (0.2 = the heart module's resting tone at NE 0.2). A silent
sympathetic output is therefore not neutral: with NE 0 resting MAP is about 63 mmHg, as in a sympathectomised subject.

**Score**: MAP = `Heart::map_smooth`, sampled every 100 ms from 10 s. `value = 100 x` fraction of samples in 80-100 mmHg. **Pass**: that fraction >= 0.85, each of the three
segments (stand 15-60 s, lie 60-80 s, haemorrhage 80-140 s) back inside the band for 3 s running within 15 s of its start (`settle_*_s`), and >= 80 % of the last 10 s of each
segment in band (`tail_*`). Parts also give `map_min_mmHg`, `map_max_mmHg`, `hr_mean_bpm`.

## Wiring (47 neurons)
```
baroreceptor (Poisson) --1600 pA--> AFF (6) --280--> NTS (8) --30--> NA (4, I_e 372) --1:1 1500--> VG (4) --> vagal (ACh) --> SA ("ach" x1)
                                                     |
                                                     +--200--> CVLM (6) --(-140)--> RVLM (6, I_e 380) --1:1 1500--> IML (6) --1:1 1500--> SG (6) --> sympathetic (NE) --> SA ("ne" x0.25)
```
| Population | Model | Role |
|---|---|---|
| AFF (6) | `iaf_psc_alpha` | baroreceptor afferents (nodose ganglion), 1:1 relay of the Poisson input (rate out = rate in) |
| NTS (8) | `iaf_psc_alpha` | second-order neurons: threshold-linear in the afferent rate (about 1.8 Hz per Hz above 17 Hz) |
| NA (4), VG (4) | `iaf_psc_alpha` | nucleus ambiguus cardiac vagal preganglionic neurons and cardiac ganglion cells; ACh gain 0.012 per spike, tau 500 ms, SA `ach` gain 1 |
| CVLM (6) | `iaf_psc_alpha` | GABAergic depressor area, copies the NTS rate |
| RVLM (6) | `iaf_psc_alpha`, `I_e` 380 pA | pacemaker-like presympathetic cells, 22 Hz when unopposed |
| IML (6), SG (6) | `iaf_psc_alpha` | sympathetic preganglionic and ganglion cells, one-to-one relays; NE gain 0.0076 per spike, tau 1000 ms, SA `ne` gain 0.25 |
| SA (1) | `sa_node_pacemaker`, `V_m` -60 mV | the pacemaker |

## Why it works
* **A threshold-linear chain**: afferent rate -> NTS -> CVLM -> inhibition of RVLM. With the CVLM weight at -140 pA the RVLM rate falls from 22 Hz (no afferent input) to 4.4 Hz
  at the resting afferent rate (41 Hz) and to 0 above about 65 Hz. NE (released with tau 1 s from six cells) is 0.2 at rest and about 1 with no afferent drive; the slope
  is about -0.04 NE per mmHg, which with the heart module's sympathetic pressure gain (about 180 mmHg per unit of NE, 90 per unit of the halved drive map below) gives a loop gain of about 2 (the open-loop standing and haemorrhage drops of 20
  and 30 mmHg become 7 and 10).
* **The relays must be secure**: the RVLM -> IML -> SG and NA -> VG stages are one-to-one with 1500 pA (a ganglionic synapse is a secure relay). With all-to-all 300 pA the
  relay worked only when the presynaptic cells happened to fire in bunches, and one seed lost all sympathetic output.
* **Why 0.5 on the NE-to-drive map**: the heart module's pressure is very sensitive to sympathetic drive (180 mmHg per unit of NE; the 0.5 map makes it 90 per unit of NE change in `sym`), and spike noise in the release pool is amplified by a closed
  loop. With a gain of 1 the amplified spike noise of the NE pool (a swing of 0.2 in NE is 36 mmHg) made MAP swing between 74 and 106 mmHg on seed 1; with 0.5 the pressure stays within 82-98 mmHg.
* **Results (seeds 1-10)**: value 100 (in band 100 %), MAP 82-99 mmHg, mean HR 75.5 beats/min (rest about 70, up on standing and in the haemorrhage). The SA cell's intrinsic rate is about 100/min, so the vagal limb now matters.
* The vagal limb holds the heart rate down: a vagal cut (NA -> VG x0) raises the mean HR from 75.5 to 109 beats/min (it was only +2.5 with the old 68/min cell) and pressure is still regulated (value 99.7), because pressure control is mainly sympathetic.

## What breaks it (tests in `engine/tests/tasks_heart_test.cpp`, seeds 1 and 2)
| Lesion | Result | Expected biology |
|---|---|---|
| AFF -> NTS x0 (sino-aortic denervation) | CVLM silent, RVLM unopposed: MAP 129-185 mmHg (mean 155), HR 122, never in band | baroreceptor denervation removes the tonic depressor drive; pressure is high and unregulated (Cowley, Liard & Guyton 1973: labile pressure with chronic denervation; the acute response is hypertension) |
| RVLM -> IML x0 (spinal cord / sympathetic cut) | NE 0, mean MAP 68 mmHg and down to 56 on standing, value 1.5 | sympathectomy: no vascular tone; orthostatic hypotension |
| IML -> SG x0 (ganglionic block, hexamethonium-like) | same as the line above | |
| NTS -> CVLM x0 | RVLM never inhibited: MAP 121-171 mmHg | loss of the depressor pathway (CVLM lesion raises pressure; Dampney 1994) |
| NTS -> CVLM x0.5 (reflex gain halved) | pressure regulated too high: mean 103, max 112 mmHg, 29 % in band | blunted baroreflex gain, as in hypertension |

Credits: Kirchheim HR (1976) Physiol Rev 56:100; Dampney RAL (1994) Physiol Rev 74:323; Guyenet PG (2006) Nat Rev Neurosci 7:335; Cowley AW Jr, Liard JF, Guyton AC (1973) Circ Res 32:564;
baroreceptor coding in the heart module: Chapleau & Abboud 1987 and Mahdi et al. 2013, as cited in `docs/body/heart.md`.

## Limits
Sino-aortic denervation in real animals gives high mean pressure for a day or two and then a normal mean with large variability; the acute hypertension is what the task shows (the
variability would need the central set point to adapt). The loop includes no respiratory sinus arrhythmia, no chemoreflex and no cardiopulmonary (volume) receptors, which in
real haemorrhage add to the response.

## v0.3: concepts, modes, variants (item D)

* **Concepts:** none new; negative_feedback, neuromodulation, inhibition, excitation used. Level kind `task`, on the spine.
* **Modes** (guided / standard / researcher): In-band fraction 0.75 / 0.85 / 0.9, settling 20 / 15 / 12 s, tails 0.7 / 0.8 / 0.85; haemorrhage rate x0.8 / 1 / x1.2. The structure of the pass test never changes; the reference circuit passes every mode (researcher on seeds 1-5; `tasks_modes_test`).
* **Variants** (optional branches, never weaken the base bar): half_neurons (par 24); severe_haemorrhage (x1.15).
* **Hints:** three tiers from `score().parts` (concept, then the place: a channel or population id, then a partial recipe).
* **Quality metric:** `worst_settle_s`. **Notebook:** Kirchheim 1976; Guyenet 2006.

## v0.4: mechanism check, quick check, negative tests
**Mechanism (Engine::structure()).** A relay population that the baroreceptor input reaches (the NTS) must feed both limbs: the vagal limb (an excitatory path to the cells `vagal` reads) and the sympathetic limb (a net-inhibitory path to the cells `sympathetic` reads: relay -> CVLM, CVLM -| RVLM, RVLM -> ..., so that high pressure turns sympathetic output down). The verdict names the missing limb ("the vagal limb is missing", "the sympathetic limb is missing", "neither limb is wired", "the limbs start from different relays"); the value is capped at 49. The vagal limb does not matter for holding pressure (the run can be fine), but it is half of the reflex.

**Quick check.** `quick_duration_ms()` = 94 s: the same day shortened (stand up at 10 s, lie down at 32 s, the same 300 ml haemorrhage at the same rate from 52 s to 82 s, end 94 s, first scored sample 8 s); every stretch stays longer than the 15 s settling limit, same rules. Reference: 4.8 s -> 2.9 s wall.

The verdicts and the tier 1-3 hints carry no parameter numbers (weights, biases, gains): those live in `recipe()` only (checked in the tests). Negative tests: `engine/tests/cheap_rows_c.inc` (run by `tasks_cheap_test`) and the mechanism cases in the level's own test file.
