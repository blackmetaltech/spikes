# What is real here

Spike is **built on real neuron models; the bodies and the circuits around them are simplified**. This page says, layer by layer and level by level, what is the simulator's own maths, what was re-fitted, what was tuned until the level passed, what is illustrative, and what is generated. The in-game "What is real" screen should show the layer table below. Citations are in `docs/CITATIONS.md` (registry `catalog/citations.json`, checked by `tools/citations/check.py`).

Five labels are used throughout:

| Label | Meaning |
|---|---|
| **NEST** | The equations are NEST's own, unmodified (`iaf_psc_alpha`, `iaf_cond_alpha`, `aeif_cond_alpha`, `izhikevich`, `parrot_neuron`, gap junctions, Poisson input). Checked by golden checksums and PyNEST parity. |
| **NESTML re-fit** | A custom cell written in NESTML for this game, with the structure taken from a paper but the numbers adjusted by hand. Not validated against recordings. |
| **Tuned in silico** | A number chosen by sweeping or hill-climbing until the level passed (circuit weights, delays, gains, pass bands). It is a property of the game, not a measurement. |
| **Illustrative** | A coefficient or time constant picked so a body module behaves plausibly. Shapes are tested; values are not fitted to data. |
| **Generated** | Produced by a script, not by people or experiments (the post-solve histograms). |

## By layer

| Layer | What it is | Status | Where it is documented |
|---|---|---|---|
| Neuron dynamics, generic cells | `iaf_psc_alpha` (34 reference circuits), `iaf_cond_alpha` (7), `aeif_cond_alpha` (9), `izhikevich` (1), `parrot_neuron` (2) | **NEST**. The equations are the published ones (Rotter & Diesmann 1999, Brette & Gerstner 2005, Izhikevich 2003). The parameter values the levels use are chosen for the game. | `engine/src/catalog/catalog.cpp`, `docs/CIRCUIT-FORMAT.md` |
| `butera_burster` | Butera, Rinzel & Smith 1999, model 1, preBotzinger bursting pacemaker | **NESTML**, equations and parameters from the paper, single compartment. Checked qualitatively against the paper's activity map; not claimed to match it quantitatively. | `docs/models/butera_burster.md` |
| `sa_node_pacemaker` | Sinoatrial node cell | **NESTML re-fit**. Structure follows rabbit SA-node cell models (Demir 1994, Kurata 2002, Maltsev & Lakatta 2009), reduced to five currents, with conductances adjusted by hand scans (the prototype script was not kept) so the isolated cell beats at the human intrinsic rate (about 100/min; the rabbit cells beat at 150-200/min). Not validated. | `docs/models/sa_node_pacemaker.md` |
| `icc_slow_wave` | Gut pacemaker (interstitial cells of Cajal) | **NESTML, phenomenological**. A FitzHugh-Nagumo oscillator rescaled to slow-wave units. It is not a channel model; the real mechanism is cyclic Ca release and Ano1 / Cav3.2 currents. The modulator gains are heuristic. | `docs/models/icc_slow_wave.md` |
| Circuit size | 1-80 cells stand for pools of thousands | **Simplified**. Reference sizes: reflex 23 cells, posture 56, spinal boss 80. A cell is a representative of a population. | `circuits/*.json` |
| Synaptic weights, delays | Every connection weight in every reference circuit | **Tuned in silico** until the level passes (sweeps for posture, stretch_reflex, renshaw, breathing, worm; hill-climbing for reach). The qualitative signs and the wiring pattern follow the cited biology; the magnitudes do not. | `docs/circuits/*.md` |
| Neuromodulators | ACh, noradrenaline and similar | **Illustrative**. Global scalars with heuristic gains acting on a cell parameter. Real modulators act through receptor-specific cascades. | `docs/models/README.md`, `icc_slow_wave.md` |
| Body physics | Muscle, joint with spindles and tendon organs, heart and circulation, lungs and CO2, eye, gut (pure C++, no NEST) | **Structure from the literature, coefficients illustrative.** The shapes are tested (Hill force-velocity, size principle, Windkessel, Otis single-compartment lung, Naka-Rushton retina). The numbers are chosen so the module sits at a textbook resting value and behaves plausibly in a game. | `docs/body/*.md` |
| Efferent and afferent coupling | Pool firing rate to a 0..1 muscle or organ drive; organ state to a sensor firing rate | **Tuned in silico** (pool rate / reference rate; afferent gains are game values). | `docs/body/README.md` |
| Task worlds | The simulated experiment: the touch pokes, the salt gradient, the CO2 challenge, the stand-up | **Game design.** They follow the cited experiments in kind but are built for a 20-60 s puzzle. Pass bands and scoring are game values. | `docs/circuits/*.md` |
| Notebook notes, concept cards, lesion hints | The text that explains a finding | **Cited, with a few honest limits.** Every reference is in the registry. Where a level uses a real number for an illustrative purpose (the Ia afferent figures in the touch-reflex level are cat stretch-reflex data), the note says so. | `docs/CITATIONS.md` |
| Diagnosis cases | The six `dx_*` patients | **Fictional models of a real kind of case.** The lesion is a missing wire in a model circuit; the patient is invented. `dx_clonus` ("the shaky arm") is explicitly not clinical clonus. | `docs/circuits/dx_*.md` |
| Post-solve histograms | "Where you stand" | **Generated.** Random variants of the reference circuit, kept if they pass two seeds. They are not other players and not other people's solutions. The game card must say "generated variants of the reference circuit, not players". | `docs/HISTOGRAMS.md`, `tools/histograms/gen_histograms.py` |
| Hint tiers, recipes | The numbers that Guided mode shows | **Tuned in silico**; they are the reference circuit's values. | task `hints()` |

## By level

"Cells" are the reference circuit's cell models. "Body" is the body module the level runs (all are illustrative in their coefficients). Every level's weights, delays and pass bands are tuned in silico; the column "Mostly literature" names what in the level is a literature value rather than a game value.

### Micro-puzzles (a few cells; no body module)

| Level | Cells | Mostly literature | Tuned or illustrative |
|---|---|---|---|
| `m_threshold` | NEST `iaf_psc_alpha` | The idea of rheobase (Lapicque 1907; Hodgkin 1948) | Cell parameters, pulse widths |
| `m_excitation` | NEST `iaf_psc_alpha` | A few ms of synaptic delay (Katz & Miledi 1965) | Weights and delays |
| `m_convergence` | NEST `iaf_psc_alpha` | About 0.1 mV per Ia afferent against about 10 mV to fire (Mendell & Henneman 1971) | Number of inputs, weights |
| `m_inhibition` | NEST `iaf_psc_alpha` | Lateral inhibition (Hartline, Wagner & Ratliff 1956) | Weights |
| `m_subtraction` | NEST `iaf_psc_alpha` | ON/OFF salt neurons ASEL/ASER (Suzuki 2008) | Rates and weights |
| `m_adaptation` | NEST `aeif_cond_alpha` | Regular-spiking adaptation (Connors & Gutnick 1990) | a, b, tau_w for the game |
| `m_half_centre` | NEST `aeif_cond_alpha` | The half-centre idea (Brown 1911; Grillner 2003) | Weights, adaptation |
| `m_coupling` | NESTML re-fit `sa_node_pacemaker` | Mutual entrainment of SA cells (Jalife 1984; Michaels 1987) | Gap-junction strength |
| `m_pacemaking` | NESTML re-fit `sa_node_pacemaker` | Intrinsic heart rate about 100/min (Jose & Collison 1970) | All channel conductances |
| `m_neuromodulation` | NEST + NESTML re-fit | Vagusstoff (Loewi 1921) | Modulator gain |
| `m_recruitment` | NEST `iaf_psc_alpha` | The size principle (Henneman 1957, 1965) | The tank gets the ordering from a bias current, a shortcut for input resistance (said in the level) |
| `m_reciprocal` | NEST `iaf_psc_alpha` | Reciprocal innervation (Sherrington 1906; Lloyd 1946) | Weights |
| `m_recurrent` | NEST `iaf_psc_alpha` | The Renshaw loop (Renshaw 1941; Eccles, Fatt & Koketsu 1954) | Weights |
| `m_rhythm` | NEST `izhikevich` | The preBotzinger rhythm (Smith 1991) | Izhikevich parameters |
| `m_timed_bursts` | NEST `iaf_psc_alpha` | The triphasic EMG pattern (Hallett 1975) | Delays and weights |
| `m_feedforward` | NEST `iaf_psc_alpha` | Central command at exercise onset (Krogh & Lindhard 1913) | The tank's plant and time constants are game values |
| `m_negative_feedback` | NEST `iaf_psc_alpha` | Feedback as homeostasis (Cannon 1929) | Plant and gains |
| `m_divergent` | NEST `iaf_psc_alpha` | Sympathetic discharge as a unit (Cannon 1915) | Fan-out sizes |
| `m_wave` | NEST `iaf_psc_alpha` | The law of the intestine (Bayliss & Starling 1899) | Delays |

### Invertebrate chapter

| Level | Cells | Body or world | Mostly literature | Tuned or illustrative |
|---|---|---|---|---|
| `reflex` | NEST `iaf_psc_alpha`, 23 cells | Poisson touch afferent | Convergence arithmetic from cat Ia data (a stretch-reflex measurement used for illustration); the circuit layout follows Aplysia withdrawal | The 150 Hz resting rate (real cutaneous afferents are mostly silent at rest), weights, scoring |
| `worm` | NEST `iaf_psc_alpha`, 24 cells | Kinematic worm in a salt gradient | ASEL/ASER roles (Suzuki 2008; Thiele 2009), AIY/AIB (Iino & Yoshida 2009; Gray 2005; Kunitomo 2013) | Wiring is a reduced reading of the connectome, weights by closed-loop sweeps, worm kinematics |
| `lamprey` | NEST `iaf_cond_alpha` and `aeif_cond_alpha`, 18 cells | Toy swimmer (hydrodynamics inspired by Lighthill 1960, not his formula) | Fictive swimming 0.3-10 Hz, antiphase left/right, head-to-tail lag (Wallen & Williams 1984; Grillner 2003) | Cell count, weights, the swimmer |

### Spinal chapter

| Level | Cells | Body | Mostly literature | Tuned or illustrative |
|---|---|---|---|---|
| `stretch_reflex` | NEST mix, 58 cells | Joint with spindles and tendon organs, muscle | Monosynaptic Ia reflex, reciprocal inhibition (Lloyd 1943; Eccles 1957), latencies (Matthews 1991) | All gains; spindle and GTO coefficients |
| `renshaw` | NEST mix, 54 cells | Joint and muscle | Renshaw cells (Renshaw 1941; Hultborn 1971, 2006), common drive and tremor (Farmer 1993; Elble & Koller 1990) | Loop gain from closed-loop sweeps |
| `posture` | NEST mix, 56 cells | Joint and muscle | Servo and stiffness regulation (Merton 1953; Nichols & Houk 1976; Houk 1979) | Weights found by sweeping |
| `reach` | NEST mix, 56 cells | Joint and muscle | Triphasic burst (Hallett 1975; Berardelli 1996), equilibrium-point idea (Feldman 1986) | About 14 weights found by hand and hill-climbing |
| `boss_spinal_sprint` | NEST mix, 80 cells | Joint and muscle | The above, combined | The combination is tuned |

### Heart, breathing, autonomic and body-system chapters

| Level | Cells | Body | Mostly literature | Tuned or illustrative |
|---|---|---|---|---|
| `heart_pacemaker` | NESTML re-fit `sa_node_pacemaker`, 7 cells | Heart | Coupled pacemaker cells (Jalife 1984; Michaels 1987); intrinsic rate (Jose & Collison 1970) | Cell conductances (re-fit), coupling |
| `heart_autonomic` | NEST + NESTML re-fit, 37 cells | Heart | Intrinsic rate, vagal and sympathetic control, nonlinear interaction (Jose & Collison 1970; Robinson 1966; Levy 1971; Rowell & O'Leary 1990) | Heart rate is linear in the two drives; gains illustrative |
| `baroreflex` | NEST + NESTML re-fit, 47 cells | Heart and baroreceptor | Baroreflex pathway (Kirchheim 1976; Dampney 1994; Guyenet 2006) | Pressure model, set point and gains |
| `breathing` | NESTML `butera_burster` + NEST, 28 cells | Lungs and CO2 | Pre-Botzinger rhythm (Smith 1991; Butera 1999; Feldman & Del Negro 2006), RTN chemoreception (Guyenet & Mulkey 2010) | Chemoreflex gain is a game value; weights open- then closed-loop |
| `pupil` | NEST `iaf_psc_alpha`, 30 cells | Eye | Light-reflex pathway and latency (Gamlin 2006; McDougal & Gamlin 2015; Ellis 1981) | Time constants illustrative; fixed luminance |
| `peristalsis` | NEST + NESTML `icc_slow_wave`, 49 cells | Gut | Law of the intestine and enteric reflex (Bayliss & Starling 1899; Costa 2000; Furness 2012), ICC pacemakers (Sanders 1996) | The ICC cell is phenomenological; rates and gains are game values |
| `fight_or_flight` | NEST + NESTML re-fit, 45 cells | Heart, lungs, eye | Defence reaction (Cannon 1915; Hilton 1982; Dampney 1994) | All gains |
| `boss_autonomic_exam` | NEST + NESTML re-fit, 75 cells | Heart, lungs, eye | Baroreflex resetting in exercise (Raven, Fadel & Ogoh 2006; Rowell & O'Leary 1990) | Lumped sympathetic chain, no light reflex, no muscle pump |

### Diagnosis cases (fictional patients, model circuits)

| Level | Underlying task | Real pathology it echoes | Honest limit |
|---|---|---|---|
| `dx_clonus` ("the shaky arm") | `renshaw` | Reduced recurrent inhibition has been proposed as one contributor to spasticity | Isolated Renshaw loss is not a clinical cause of clonus; the patient shows excess 10 Hz tremor, not clonus (Hidler & Rymer 1999 model real clonus as stretch-reflex instability) |
| `dx_vagal` | `heart_autonomic` | Vagal withdrawal and resting tachycardia | Model heart, lumped drives |
| `dx_orthostatic` | `baroreflex` | Neurogenic orthostatic hypotension (Freeman 2011) | The model's heart rate still rises through vagal withdrawal; real neurogenic cases have a fixed pulse |
| `dx_apnoea` | `breathing` | Congenital central hypoventilation (PHOX2B; Amiel 2003; Weese-Mayer 2010) | One removed wire stands for a genetic syndrome |
| `dx_horner` | `pupil` | Sympathetic chain interruption (Kanagalingam & Miller 2015) | Three-neuron chain collapsed |
| `dx_achalasia` | `peristalsis` | Loss of nitrergic inhibitory neurons (Pandolfino & Gawron 2015; Goyal & Chaudhury 2010 is an editorial) | Gut segments reduced to chains |

## What is generated, and by what

- **Histograms** (`godot/catalog/histograms/*.json`): `tools/histograms/gen_histograms.py`. Variants of the reference circuit, not players (`docs/HISTOGRAMS.md`).
- **Audio** (`tools/audio`): synthesised, not recorded.
- **NESTML C++** (`third_party/nestml-gen`): generated from `models/nestml/*.nestml` by `scripts/build-nestml.sh`.
- **Diagnosis circuits** (`circuits/dx_*.json`): generated from the reference circuit by `circuits/tools/gen_boss_dx.py` with the answer's weights set to zero.
- **`docs/CITATIONS.md`**: generated from `catalog/citations.json` by `tools/citations/check.py --write-md`.

## What this page does not claim

- That any circuit weight is a physiological measurement.
- That any body module reproduces a particular patient or subject.
- That NESTML cells match published recordings.
- That a level's pass band is clinically meaningful.
