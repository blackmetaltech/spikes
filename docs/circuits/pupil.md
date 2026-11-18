# Reference circuit: pupil light reflex (`circuits/pupil.json`)

Task `pupil` (engine/src/tasks/pupil.cpp, chapter "autonomic", view `eye`, order 400). The pupil is a two-muscle servo (`docs/body/eye.md`): the parasympathetic sphincter closes it, the sympathetic dilator opens it, and the retina measures the light that gets through, so a small pupil lowers its own signal. The episode is 40 s.

## World and score
- **Light (timeline).** 1 cd/m2 (dim) until 4 s, a flash of 200 cd/m2 from 4 s to 11 s, dim again 11-20 s, then 30 cd/m2. A fright cue (`threat`, Poisson 250 Hz) runs 30-34 s at the medium light.
- **Sensors.** `retina`: Poisson rate `4 x retinal_hz` of the eye model (retinal illuminance in trolands = luminance x pupil area, Naka-Rushton transfer, 30 ms latency: 5-100 Hz per fibre). `threat`: 0 or 250 Hz.
- **Motors.** `sphincter` = ciliary-ganglion pool and `dilator` = superior-cervical-ganglion pool: mean Hz per neuron / 40 Hz, smoothed over 20 ms before the clamp, then into the Eye. `D = 2 + 6 clamp(0.5 + 0.5 (dilator - sphincter))` mm, sphincter 130/500 ms, dilator 800/300 ms, 40 ms excitation-contraction delay.
- **Score.** Constriction = dim - bright diameter, redilation = dim again - bright; latency = time after a step until the diameter has moved 20 % of the way from its value just before the step (mean of the preceding 200 ms) to the new plateau and stays there for 50 ms (a crossing within 50 ms of the step is noise, not a reflex; the old baseline was the 1 s mean, which noise had already crossed on seed 8, giving "1 ms"); fright = dilation during the cue at medium light. Pass: constriction 2.5 mm or more with a bright-light pupil of 4.5 mm or less (direction and amplitude), redilation 2.5 mm or more and 6 mm or more in the dark (both directions work), constriction latency 150-350 ms (human 200-250 ms), fright dilation 0.8 mm or more. `value` is the mean of five soft scores.

## Wiring (30 neurons, par 30)
```
retina --250 pA--> RGC x6 --100 pA, 25 ms--> PON x4 --60 pA, 15 ms--> EW x4 --80 pA, 25 ms--> CG x4 --> sphincter
                                              |  \-(-100 pA)-> IML x4 --100 pA--> SCG x4 --> dilator
threat --250 pA--> HYP x4 --60 pA--> IML          HYP --(-40 pA)--> EW
```
| Population | Role (I_e) |
|---|---|
| RGC (6) | retinal ganglion cells (incl. melanopsin) with a 100 pA bias: rate 15 Hz in the dark to 50 Hz in bright light, graded |
| PON (4) | olivary pretectal nucleus, bias 225 pA: a threshold just above the dark level, so it is silent in the dark and fires with illuminance |
| EW (4), CG (4) | Edinger-Westphal preganglionic neurons and ciliary-ganglion cells: bias 340 pA (just below firing); the two relays pass the PON rate on almost one-to-one, so the sphincter drive follows the pretectal signal. Delays 15 and 25 ms |
| IML (4) | intermediolateral column (C8-T2) sympathetic preganglionic cells: tonically firing (410 pA, about 30 Hz), inhibited by PON (light switches the sympathetic side off), excited by HYP |
| SCG (4) | superior cervical ganglion: 1:1 relay to the dilator |
| HYP (4) | hypothalamic alarm (bias 100 pA, silent until the cue): excites IML, inhibits EW (arousal inhibits the Edinger-Westphal nucleus) |

## Why it works
* **Static map.** Dark: PON, EW silent; IML about 30 Hz so the dilator drive is 0.8 and D is 7.1 mm. Bright light: PON about 37 Hz, sphincter drive 0.8-0.9, IML silenced, D = 2.6-2.8 mm. The loop is negative: a constricting pupil lowers the retinal rate (70 to 65 Hz) so it settles instead of oscillating. Intermediate luminances give intermediate diameters.
* **Latency.** 30 ms retina + 25 + 15 + 25 ms transmission (retinopretectal, pretecto-EW, preganglionic-ganglionic) + 40 ms muscle delay + the rise of the sphincter (130 ms time constant) puts the 10 % point at 172-184 ms (human 200-250 ms; the body-module test with a 150 ms arc delay gives 228 ms, so the game's arc is a little faster than a human's). Redilation starts about 190-220 ms after the light goes off and is slower overall (dilator tau 800 ms), as in humans.
* **Sympathetic arm.** The fright cue gives about +3 mm at the medium light (HYP excites IML and removes the EW drive), which the light reflex alone cannot do.
* **Parameter provenance.** The relays (I_e just below the 375 pA rheobase of `iaf_psc_alpha`, weights 60-100 pA) were set open loop against the Poisson retina input, then the PON threshold and gains in closed loop with the task; the timing is the knob for latency. Margin, 10 seeds (1-10; also 11, 12, 101-105): the reference passes 10/10. Constriction 3.95-4.67 mm, constriction latency 186-226 ms (101: 161), redilation latency 251-318 ms, redilation 4.05-4.51 mm, fright +2.8-3.2 mm.

## What breaks it (tests in `engine/tests/tasks_autonomic_test.cpp`)
| Lesion | Result |
|---|---|
| PON -> EW x0 (Edinger-Westphal denervated) | sphincter never fires; the pupil only gets as small as the sympathetic withdrawal allows (5.0 mm, a mid-dilated, poorly reacting pupil, as in an oculomotor palsy), constriction 2.2 mm |
| EW -> CG x0 (ciliary ganglion cut) | the same, one synapse later |
| IML -> SCG x0 (sympathetic chain cut, Horner-like) | dilator never fires; the pupil only reopens to 5.0 mm in the dark (needs 6), fright adds only +1.7 mm (sphincter withdrawal) |
| HYP -> IML and HYP -> EW x0 (alarm cut) | the light reflex is intact (4.4 mm) but fright dilation is 0 |
| RGC -> PON x0 (no retinal input to the pretectum) | no light reflex at all: constriction 0.0 mm, pupil fixed at 7.2 mm |

## Limits
No hippus, no consensual reflex (one eye), no near response, no steady luminance adaptation of the retina beyond the Naka-Rushton curve, and the sympathetic chain is lumped into one cell type (IML, SCG).

Credits: Gamlin PDR (2006) Prog Brain Res 151:379; McDougal DH, Gamlin PD (2015) Compr Physiol 5:439 (pathways, latency); Ellis CJ (1981) Br J Ophthalmol 65:754 and Loewenfeld IE (1993) *The Pupil* (time courses); Steinhauer SR, Hakerem G (1992) Ann N Y Acad Sci 658:182; Watson AB, Yellott JI (2012) J Vis 12(10):12 (diameter range); Naka KI, Rushton WAH (1966) J Physiol 185:536.

## v0.3: concepts, modes, variants (item D)

* **Concepts:** none new; excitation, inhibition, reciprocal_inhibition used. Level kind `task`, on the spine.
* **Modes** (guided / standard / researcher): Amplitudes 2.0 / 2.5 / 2.94 mm, latency window 120-400 / 150-350 / 160-330 ms, fright 0.64 / 0.8 / 0.94 mm; fright cue x1.2 / 1 / x0.9. The structure of the pass test never changes; the reference circuit passes every mode (researcher on seeds 1-5; `tasks_modes_test`).
* **Variants** (optional branches, never weaken the base bar): half_neurons (par 15); weak_retina (3 fibres).
* **Hints:** three tiers from `score().parts` (concept, then the place: a channel or population id, then a partial recipe).
* **Quality metric:** `constriction_latency_ms`. **Notebook:** McDougal & Gamlin 2015.

## v0.4: mechanism check, quick check, negative tests
**Mechanism (Engine::structure()).** Two pathways onto two pools: an excitatory path from `retina` to the cells `sphincter` reads (the parasympathetic reflex arc) and one from `threat` to the cells `dilator` reads (the sympathetic arm), and the two output pools must be different cells (one pool read by both is one muscle). The verdict names the missing pathway or says the outputs read the same cells; the value is capped at 49.

**Quick check.** None needed: the full episode (40 s) already takes 1.4 s (`quick_duration_ms()` = the full episode).

The verdicts and the tier 1-3 hints carry no parameter numbers (weights, biases, gains): those live in `recipe()` only (checked in the tests). Negative tests: `engine/tests/cheap_rows_c.inc` (run by `tasks_cheap_test`) and the mechanism cases in the level's own test file.
