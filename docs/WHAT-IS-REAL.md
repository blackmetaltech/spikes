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
| `stg_cell` | Crab stomatogastric neuron (Na, Kd, CaT, CaS, A, KCa, H, leak; calcium dynamics) | **NESTML re-fit**. The gating forms (Boltzmann steady states, voltage-dependent time constants) follow the Prinz et al. 2003 family, written from memory of those papers and **not checked against them**; the maximal conductances are game values in the ranges of Prinz et al. 2004, **not their published parameter sets**. Not validated against recordings. | `docs/models/stg_cell.md` |
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

### Bestiary chapters (0.5): worm, leech, sea slug

Spiking cells everywhere. **The worm and the leech use spiking model cells, but the real cells of these circuits signal in a graded way.** Most neurons of the C. elegans touch circuit have no sodium action potentials and pass signals as smooth voltage changes; the leech heart interneurons inhibit each other partly through graded, voltage-dependent transmitter release (spike-mediated release dominates in the canonical oscillation). NEST has no graded synapse, so these levels approximate every cell with a spiking one (`iaf_psc_alpha`, `aeif_cond_alpha`) and every synapse with a spike-triggered one. Aplysia sensory and motor neurons do spike.

| Level | Cells | Mostly literature | Tuned, illustrative or assumed |
|---|---|---|---|
| `w_latch`, `w_switch` | NEST `iaf_psc_alpha`, 2-3 cells | A reversal that outlasts a touch and the forward/backward exclusion (Chalfie et al. 1985) | All weights; the memory is recurrent excitation, which is one of several real mechanisms |
| `worm_touch` | NEST `iaf_psc_alpha`, 17 cells | **The wiring.** Synapse counts of the touch circuit from Cook et al. 2019, read through the OpenWorm ConnectomeToolbox (`cect` 0.3.5, MIT; the data keep the terms of the paper). Pathways from Chalfie et al. 1985 | Weight = count x 25 pA; every chemical synapse is assumed excitatory (the data have no sign); gap junctions are modelled as excitatory links; the AVA/AVB mutual inhibition is **hand-added** (it is not a connection in the data); the excitability of each cell class is tuned; strong connections of the data that are not on the Chalfie pathway are left out (`tools/connectome/worm.py` lists them) |
| `l_burst`, `l_rebound` | NEST `izhikevich` (chattering), `aeif_cond_alpha` | Heart interneurons burst and rebound from inhibition (Nadim et al. 1995; Olsen et al. 1995) | Generic bursters and rebounders, not the leech cell: its currents (Ih, low-threshold Ca) are not modelled; the rebound comes from sub-threshold adaptation |
| `leech_heart` | NEST `aeif_cond_alpha`, 4 cells | Two reciprocally inhibitory interneurons alternate in bursts (Hill et al. 2001, 2002) | **Time is compressed:** the cycle is about two seconds so that a check takes a second; the real cycle is much slower. Weights, adaptation constants and the band are game values; the intersegmental coordination is not modelled |
| `a_depress`, `a_facilitate`, `aplysia_gill` | NEST `iaf_psc_alpha` with `tsodyks2_synapse` | Habituation is depression of the sensory-motor synapse; sensitisation is serotonin from facilitating interneurons (Castellucci et al. 1970; Byrne et al. 1978; Hawkins et al. 2006) | Time compressed by a large factor (seconds instead of minutes); serotonin is the engine's NE channel; the cAMP cascade is replaced by a direct change of the synapse (its release probability U, or its weight); depression is the generic Tsodyks-Markram form, not a fit to Aplysia data |

Diagnosis cases `dx_worm`, `dx_leech`, `dx_aplysia` are fictional laboratory animals with one missing connection in the model circuit.

### Bestiary chapters B (0.5): fly compass, fly mushroom body, crab

| Level | Cells | Mostly literature | Tuned, illustrative or assumed |
|---|---|---|---|
| `c_ring`, `c_shift`, `fly_compass` | NEST `iaf_psc_alpha`, 20 to 52 cells (16 compass cells E-PG, 4 Delta7, 16 + 16 P-EN) | A single bump on a ring of compass cells, held by recurrent excitation and competition, that follows self-motion in the dark; left and right P-EN cells shift the bump one way each (Seelig & Jayaraman 2015; Turner-Evans et al. 2017; Green et al. 2017) | **After Turner-Evans/Hulse cell types, not synapse-level data.** The ring is a ring-attractor model: the E-PG to E-PG excitation is a direct stand-in for what is largely indirect in the animal; all weights, the slow excitatory synapse (15 ms), the bias, and the 16-wedge ring are game values. The turn signal is a pair of dc inputs (a tonic level on both sides, more on the side of the turn). Tracking is only good for turns of about 90 to 170 degrees per second: a discrete ring has a dead zone for slow turns. Time is the real one. The heading is scored with a population vector of the ring cells |
| `k_sparse`, `k_depress`, `fly_mb` | NEST `iaf_psc_alpha`, 18 to 231 cells; `stdp_dopamine_synapse` | Odours are sparse patterns on Kenyon cells, each KC with a few random inputs, held sparse by feedback inhibition from the APL (Aso et al. 2014a; Papadopoulou et al. 2011: the locust analogue is a nonspiking neuron); odour followed by dopamine depresses the active KC to MBON synapses, odour-specifically (Hige et al. 2015; Aso et al. 2014b) | **After the published cell types, not synapse-level data.** The APL is a spiking group. Four PN inputs per KC, 200 KCs (not about 2000), six-glomerulus odours, the odour patterns, all weights, and the time scale (a pairing of 0.4 s) are game values. The three-factor rule is NEST `stdp_dopamine_synapse` with `A_plus < 0` and `A_minus > 0` (NEST subtracts A_minus for anti-causal pairs), driven by the output cells' own baseline firing: the real rule needs no postsynaptic spike. DAN, TSN and the avoidance MBON are generic cells; one compartment only; valence is not modelled beyond one MBON group |
| `s_pacemaker`, `s_phase`, `crab_pyloric` | NESTML `stg_cell`, 1 to 6 cells | The pyloric rhythm: an AB/PD pacemaker kernel and LP and PY followers that are inhibited and fire by rebound, in the order PD, LP, PY, about once a second; many different parameter sets give the same pattern (Marder & Bucher 2001; Prinz et al. 2004) | **Not the published parameter sets.** The cell is a conductance-based single compartment with the usual currents and **kinetic forms written from memory** of Prinz 2003; the AB/PD electrical coupling is **approximated by fast reciprocal excitation** (the engine couples electrically only the tonic gap-junction cell models); the graded glutamatergic synapses are spike-triggered inhibitory conductances; cell counts (1 AB, 2 PD, 1 LP, 2 PY), conductances, synaptic strengths and the pass bands are game values. `crab_pyloric.json` and `crab_pyloric_b.json` are two hand-picked parameter sets found by a random search over the follower conductances and the synapses; both pass (degeneracy), a sweep is not shipped |

Diagnosis cases `dx_compass`, `dx_mb_apl`, `dx_mb_dan`, `dx_crab` are fictional laboratory animals with one missing connection in the model circuit (`dx_crab` is built on the second parameter set).

### Theory wing (0.6): dynamics

| Level | Cells | Mostly literature | Tuned, illustrative or assumed |
|---|---|---|---|
| `d_class`, `d_phasic`, `d_switch` | NEST `izhikevich` (the player may use `aeif_cond_exp` / `aeif_cond_alpha`), 1 cell | The equations are NEST's own. Class 1 and class 2 excitability (Hodgkin 1948), the 2-D reset models and their firing patterns (Izhikevich 2003), phasic responses from a stable resting state (Rothman & Manis 2003 for the biology), reset-induced bistability in 2-D reset models (Touboul & Brette 2008 analyse how the reset interacts with the equations) | **The phase plane is for a constant current and ignores the synaptic conductances of the aeif cells.** The nullclines and fixed points are computed from the cell's live parameters by `engine/src/analysis/theory.cpp` and checked against the NEST cell settling on the fixed point (`test_theory`). Class II is decided by the resonance of the resting state just before it is lost (about 15 Hz or more): this is a game criterion, the classes shade into each other and the default regular-spiking cell lies near a Bogdanov-Takens point. The staircase of `d_class` is scaled to the cell's own onset current. The reference parameter sets, the 50 ms pulses, the windows and the pass counts are tuned in silico. Persistent firing of real cells is channel-based; the level shows the model mechanism only |

### Theory wing (0.6): rate models and mean field

| Level | Cells | Mostly literature | Tuned, illustrative or assumed |
|---|---|---|---|
| `r_oscillate`, `r_choose` | NEST `threshold_lin_rate_ipn`, 2 units | The excitatory-inhibitory population pair and its oscillation, and competition between populations (Wilson & Cowan 1972) | **Not the original equations.** The units are NEST's threshold-linear rate neurons with a ceiling, not the sigmoid with refractoriness of the paper; rates are in arbitrary units; the drive enters the unit's rate directly (NEST's `mu`), not through the nonlinearity. All weights, time constants and bands are tuned in silico |
| `r_meanfield` | NEST `iaf_psc_delta` (200 + 50 cells) and `siegert_neuron` (3 units) | The Siegert rate formula for integrate-and-fire neurons with delta synapses and its use as a pseudo-time rate network (Amit & Brunel 1997; Fourcaud & Brunel 2002; Hahne et al. 2017). The mapping drift = tau_m K J and diffusion = tau_m K J^2 is NEST's own (`brunel_siegert_nest.py`) | **The network is small on purpose** (200 excitatory cells with 60 inputs each, not thousands): the prediction still agrees within a few percent in the reference, and fails in the cheap circuits that change J or K. The recurrent weights are chosen so that the loop matters; the drive levels, the agreement band and the 450 ms measurement windows are game values. A diffusion projection takes J and K as separate numbers; NEST has only the derived drift and diffusion factors |

### Theory wing (0.6): attractor memory

| Level | Cells | Mostly literature | Tuned, illustrative or assumed |
|---|---|---|---|
| `h_complete`, `h_two`, `h_learn` | NEST `iaf_psc_alpha`, 8 cells in 2 groups; `stdp_synapse` in `h_learn` | Content-addressable memory in a network with symmetric connections and its spurious mixtures (Hopfield 1982); stable, selective activity in networks of integrate-and-fire cells (Amit & Brunel 1997); Hebbian storage by spike-timing-dependent plasticity (Bi & Poo 1998; Song, Miller & Abbott 2000) | **A pattern is a group of cells, not a +1 / -1 vector, and the weight matrix is block-structured by construction, not built by the outer-product rule: this is the simplest honest reading of a stored pattern with 8 cells.** Persistent activity is at 100 Hz or more because the network has no balanced inhibition or adaptation; real delay activity is 10 to 40 Hz. The inhibition between patterns is built in (NEST `stdp_synapse` is excitatory only, so it cannot be learned). The STDP learning rate, the weights, the exposure and the cue are game values; one second of exposure stands for minutes of learning |

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
- **Diagnosis circuits** (`circuits/dx_*.json`): generated from the reference circuit by `circuits/tools/gen_boss_dx.py` (and, for the Bestiary, `circuits/tools/gen_bestiary_a.py`) with the answer's weights set to zero.
- **`circuits/worm_touch.json`**: generated by `tools/connectome/worm.py` from the `cect` package (OpenWorm ConnectomeToolbox, Cook 2019 hermaphrodite data); the dataset name, version and every simplification are in the file's `meta`.
- **Theory wing reference circuits**: `circuits/tools/gen_theory.py`.
- **Other Bestiary reference circuits**: `circuits/tools/gen_bestiary_a.py` (worm, leech, sea slug) and `circuits/tools/gen_bestiary_b.py` (fly compass, fly mushroom body, crab, and their diagnosis patients).
- **`docs/CITATIONS.md`**: generated from `catalog/citations.json` by `tools/citations/check.py --write-md`.

## What this page does not claim

- That any circuit weight is a physiological measurement.
- That any body module reproduces a particular patient or subject.
- That NESTML cells match published recordings.
- That a level's pass band is clinically meaningful.
