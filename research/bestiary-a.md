# Bestiary research, set A: six canonical circuits

Research for v0.5 "Bestiary". One fact sheet per circuit, then the connectome-access check.

Conventions
- **[V]**: confirmed from the PubMed record (title, authors, journal, PMID, DOI read via the PubMed MCP tools, 2026-11-18). Abstract-level facts only.
- **[mem]**: from the author's background knowledge, NOT checked against a source in this session. Treat numbers as approximate and re-check before putting them in a task's pass criteria.
- Sections 1-6 follow one template: cells, wiring, dynamics and scores, minimum circuit, NEST fit and gaps, lesions, citations.
- Engine facts (what `gap_junction`, `modulation`, `stdp_dopamine_synapse` accept) come from `docs/CIRCUIT-FORMAT.md`.

## 0. Summary tables

| # | Species / circuit | Core claim a game can score | Minimum cells | Hardest NEST gap |
|---|---|---|---|---|
| 1 | C. elegans touch withdrawal | Anterior touch gives reversal (AVA/A-motor), posterior touch gives forward acceleration (PVC/AVB/B-motor) | ~14 | Cells are largely non-spiking (graded); no graded chemical synapse |
| 2 | Leech heartbeat CPG | Two mutually inhibitory HN cells burst in antiphase, period about 6-12 s | 2 (4-6 for fidelity) | Graded Ca-mediated IPSP plus slow Ih/low-threshold Ca currents |
| 3 | Aplysia gill-siphon withdrawal | Depression on repeat touch; tail-shock sensitization and dishabituation | 4-5 | Neuromodulator that changes a synapse parameter (U, weight) |
| 4 | STG pyloric rhythm | Triphasic PD, LP, PY at about 1 Hz; many parameter sets give the same pattern | 3 (AB/PD, LP, PY) | Multi-conductance bursting cells (CaT, CaS, A, H, KCa) not built in |
| 5 | Drosophila mushroom body | Sparse KC code, DAN-gated depression of KC to MBON, APL gain control | ~40-100 scaled | Three-factor rule with no postsynaptic spike; sign |
| 6 | Drosophila central complex | Ring attractor bump holds in the dark and integrates turning | ~25-40 | None in NEST; engine lacks a bump-angle readout |

Models by role (NEST 3.x names, all pass straight through the circuit JSON)

| Need | Candidate | Note |
|---|---|---|
| Plain spiking cell | `iaf_psc_alpha`, `iaf_cond_alpha`, `iaf_cond_exp` | Cheapest; use for sensory, relay, KC |
| Adaptation, bursting | `aeif_cond_alpha`, `aeif_cond_exp` | `a`, `b`, `tau_w`, `V_reset` give tonic, adapting, bursting regimes (Brette and Gerstner 2005, already in CITATIONS.md) |
| Rebound burst (Ih, low-threshold Ca, persistent Na) | `ht_neuron` | Has I_h, I_T, I_NaP; receptors AMPA, NMDA, GABA_A, GABA_B on separate ports. Best built-in for half-centre rebound |
| Hodgkin-Huxley | `hh_cond_exp_traub`, `hh_psc_alpha` | Na and K only |
| Gap-junction cells | `hh_psc_alpha_gap`, `hh_cond_beta_gap_traub` | The only models the engine accepts for `gap_junction` |
| Bursting card in repo | `butera_burster`, `icc_slow_wave` (NESTML) | See `docs/models/`; marked unavailable until the module is built |
| Short-term depression | `tsodyks2_synapse`, `quantal_stp_synapse` | Engine rules in CIRCUIT-FORMAT |
| Dopamine-gated plasticity | `stdp_dopamine_synapse` + `dopamine_source` | Engine builds `volume_transmitter` |
| Rate or graded units | `rate_neuron_ipn`, `siegert_neuron` with `rate_connection_instantaneous` | NEST-native, but not accepted by the current circuit loader (`[mem]`, check loader) |

---

## 1. C. elegans: touch withdrawal and the forward/backward command switch

### Cells and roles

| Cell (L/R pairs unless noted) | Role | Neurotransmitter [mem] |
|---|---|---|
| ALML, ALMR, AVM (anterior); PLML, PLMR (posterior) | Gentle-touch mechanoreceptors (six in all) [V] | Glutamate |
| AVD | Command interneuron, anterior-touch pathway to reversal [V] | Glutamate |
| AVA (also AVE) | Backward command interneurons; drive A-type motor neurons (DA, VA) | Glutamate |
| AVB (also PVC) | Forward command interneurons; drive B-type motor neurons (DB, VB) | Glutamate (PVC), gap junctions to B-type |
| PVC | Posterior-touch pathway to forward motion [V] | Glutamate |
| DA, VA (A-type); DB, VB (B-type) | Motor neurons, backward and forward body bending | ACh |
| DD, VD (D-type) | GABA cross-inhibition of antagonist muscles | GABA |

Chalfie et al. 1985 [V]: six touch receptors, five pairs of interneurons, 69 motor neurons; **two** anterior-touch pathways (AVD, and AVB added as the animal matures) and **one** posterior pathway (PVC); the three anterior touch cells become coupled by gap junctions with maturation.

### Connectivity

| Edge | Sign | Approx. strength | Source |
|---|---|---|---|
| ALM, AVM to AVD, AVB, (PVC for ALM) | + (glutamatergic, mostly) | few to tens of EM synapses per pair [mem] | Chalfie 1985 [V] for the wiring pattern; weights from Cook 2019 |
| PLM to PVC, AVA | + | same | Chalfie 1985 [V] |
| ALM <-> AVM, ALM L <-> ALM R | gap junction (electrical) | small | Chalfie 1985 [V] |
| AVD to AVA, AVA to A-motor | + | AVA to A-motor is strong (tens of synapses) [mem] | Cook 2019 |
| PVC to AVB, AVB to B-motor | + | strong [mem] | Cook 2019 |
| AVA/AVD vs AVB/PVC | Mutual functional antagonism; direct sign and strength should be read off the connectome, not assumed | | [mem] |

Totals [mem]: hermaphrodite 302 neurons, on the order of 6000 chemical synapses, 900 gap junctions, 1500 neuromuscular junctions (White 1986 lineage; Cook 2019 [V] updates it, exact figures in its matrices).

### Key dynamics and scoring

| Observable | Expected (qualitative [mem], numbers to be set by the task) | Possible score |
|---|---|---|
| Anterior touch gives reversal | AVA/A-motor activity up, AVB/B-motor activity down | Reversal probability, latency, reversal duration |
| Posterior touch gives forward acceleration | PVC and AVB up | Forward speed gain |
| Mutual exclusion of the two states | AVA-active and AVB-active windows rarely overlap | Overlap fraction, as in `m_half_centre` |
| Many neurons are non-spiking and use graded potentials [mem] | Slow plateau-like responses | Output = smoothed voltage or rate over a window |
| Habituation to repeated taps (Rankin lab; not checked here) | Response decays with repetition | Optional extension via `tsodyks2_synapse` |

### Minimum circuit that reproduces the switch (about 14 cells)

```
ALM, AVM  (touch, input A) --+--> AVD --> AVA --> A-motor  ----> backward
PLM       (touch, input P) --+--> PVC --> AVB --> B-motor  ----> forward
AVA <-inhib-> AVB   (reciprocal; makes it a switch)
ALM <-gap-> AVM (optional)
```
Counts: 3 anterior touch + 2 PLM + 2 AVD + 2 AVA + 2 AVB + 2 PVC + 2 motor groups (collapse DA/VA, DB/VB to one population each). Collapsing L/R halves to 7 populations of n=1-2 is fine for a first chapter.

### NEST fit

| Part | Model | Comment |
|---|---|---|
| Touch cells | `iaf_psc_alpha` driven by `poisson` input | Real cells are graded; a poisson-driven spiking proxy works for scoring |
| Command interneurons | `aeif_cond_alpha` (adapting) or `iaf_cond_exp` | AVA/AVB flip with plateau-like persistence; `aeif` with small `b` approximates |
| Reciprocal AVA/AVB inhibition | `static_synapse`, negative weight | Same pattern as `m_half_centre` |
| Gap junctions (ALM-AVM, AVA-motor) | `gap_junction` on `hh_psc_alpha_gap` | Forces HH cells everywhere gap junctions are used |
| Habituation extension | `tsodyks2_synapse` | |
| **Gap** | No graded (non-spiking) chemical synapse in NEST; `rate_neuron_ipn` with `rate_connection_instantaneous` is the nearest native model but cannot mix with spiking cells in the current JSON loader. NESTML could define a graded synapse. |

### Candidate lesions

| Lesion | Observable symptom | Basis |
|---|---|---|
| Cut or ablate AVD/AVA branch (or silence AVA) | Anterior touch no longer produces reversal; forward drive intact | Chalfie 1985 laser ablations [V] |
| Ablate PVC (or cut PVC to AVB) | Posterior touch no longer produces forward acceleration | Chalfie 1985 [V] |
| Sign error: AVA to AVB inhibition removed | Both command states fire together; motor output co-contracts, no net movement | Mirrors `m_half_centre` overlap failure |

### Citations

| Ref | PMID | DOI | Status |
|---|---|---|---|
| Chalfie M, Sulston JE, White JG, Southgate E, Thomson JN, Brenner S (1985) The neural circuit for touch sensitivity in Caenorhabditis elegans. J Neurosci 5:956-964 | 3981252 | 10.1523/JNEUROSCI.05-04-00956.1985 | [V] |
| Cook SJ et al. (2019) Whole-animal connectomes of both Caenorhabditis elegans sexes. Nature 571:63-71 | 31270481 | 10.1038/s41586-019-1352-7 | [V] |
| Gray JM, Hill JJ, Bargmann CI (2005) A circuit for navigation in Caenorhabditis elegans. PNAS 102:3184-3191 | 15689400 | 10.1073/pnas.0409009101 | [V] (reversal and turn circuit; AIB, AIY) |
| White JG, Southgate E, Thomson JN, Brenner S (1986) The structure of the nervous system of C. elegans. Phil Trans R Soc B 314:1-340 | not searched | not looked up | UNVERIFIED (old; may not be in PubMed) |

---

## 2. Leech: heartbeat central pattern generator (half-centre oscillator)

### Cells and roles

| Cell | Role | Note |
|---|---|---|
| HN(3), HN(4) pairs in midbody ganglia G3 and G4 | Oscillator interneurons: in each segmental oscillator two cells inhibit each other reciprocally [V: "each oscillator consists of two reciprocally inhibitory oscillator interneurons along with coordinating interneuron fibers"] | Which HN numbers are the oscillator is from memory [mem] |
| HN(1), HN(2) (G1, G2 somata), HN(5-7) | Coordinating and switch interneurons; their fibres link G3 and G4 oscillators [V for coordinating role] | |
| Heart motor neurons (HE) | Receive HN inhibition; produce the peristaltic vs synchronous heart constriction pattern [mem] | |

Hill 2002 [V]: the timing network is two segmental oscillators (G3, G4) coupled by inhibitory coordinating interneurons; the faster oscillator leads and sets the period.

### Connectivity

| Edge | Sign | Notes |
|---|---|---|
| HN(L) to HN(R) in one segment, and back | Inhibitory | Graded (Ca-mediated) plus spike-mediated IPSP; during canonical oscillation the spike-mediated part dominates [V, Nadim 1995] |
| Oscillator HN to coordinating HN fibres | Inhibitory | Symmetric or asymmetric variants modelled [V, Hill 2002] |
| Coordinating HN to the other segment's oscillator | Inhibitory | Entrainment by "removal of inhibition" [V] |
| HN to heart motor neurons | Inhibitory | [mem] |

Counts [mem]: 7 bilateral HN pairs (G1-G7); oscillator core is 2 cells per segment. Six HNs make up the basic rhythm network in one ganglion [V, Hill 2001].

### Key dynamics and scoring

| Observable | Value | Source |
|---|---|---|
| Cycle period of the isolated timing network | about 6-12 s (roughly 10 s typical) | [mem] |
| Burst pattern | Alternating bursts, each cell bursts while the partner is silent, duty about 50% | [mem] |
| Mechanism | Inhibition hyperpolarises, activates Ih, removes inactivation of regenerative inward currents (fast Na, low-threshold Ca), cell escapes and inhibits partner | [V, Nadim 1995] |
| Two parameter regimes: "S-mode" (synaptic-current dominated) and "G-mode" (intrinsic-current dominated) | Maps the parameter space of the model | [V, Olsen 1995] |
| Entrainment | Driving one HN with alternating +/- current can lock the whole network over a range of periods | [V, Hill 2002; Masino and Calabrese 2002] |
| Period set by the faster segmental oscillator | Coupled oscillators with different periods lock to the faster | [V, Hill 2002] |

Scores: `period_ms` (already in the engine for `m_half_centre`), antiphase index, duty cycle, entrainment range.

### Minimum circuit

```
drive --> HN_L, HN_R   (2 cells, burst-capable, ht_neuron or aeif)
HN_L --(inhib)--> HN_R   HN_R --(inhib)--> HN_L
```
Fidelity tier: add 2 coordinating cells and a second pair (G4) with inhibitory coordinating links, 6 cells. Period locking to the faster side is the testable emergent property.

### NEST fit

| Part | Model | Comment |
|---|---|---|
| HN oscillator cell | `ht_neuron` (I_h, I_T, I_NaP) or `aeif_cond_alpha` (a, b, tau_w) | `ht_neuron` gives true rebound/release; `aeif` is the repo's existing path |
| Reciprocal inhibition | `static_synapse`, negative weight (spike-mediated component) | Same as `m_half_centre` |
| Period fidelity (seconds) | `aeif` `tau_w` several hundred ms to seconds | Existing reference gives about 150 ms cycle; the leech needs about 50x slower, so scale `tau_w` and `b` |
| **Gap** | Graded Ca-mediated IPSP (continuous release as function of presynaptic V_m below spike threshold) has no NEST synapse. Add NESTML cell with release proportional to sigmoid(V_m), or accept spike-only inhibition (documented to dominate anyway [V]). |

### Candidate lesions

| Lesion | Observable symptom |
|---|---|
| One reciprocal inhibitory link cut | Partner stops alternating; one cell fires tonically and the other is silenced or fires in-phase: `m_half_centre`-style overlap |
| Ih (hyperpolarisation-activated) conductance removed | Rebound after release is lost: period lengthens drastically or the rhythm stops (G-mode loss) [mem for exact effect size; mechanism V] |
| Coordinating link between G3 and G4 cut | Two independent oscillators with different periods: beat frequencies, no phase locking [V mechanism] |

### Citations

| Ref | PMID | DOI | Status |
|---|---|---|---|
| Nadim F, Olsen OH, De Schutter E, Calabrese RL (1995) Modeling the leech heartbeat elemental oscillator. I. J Comput Neurosci 2:215-235 | 8521288 | 10.1007/BF00961435 | [V] |
| Olsen OH, Nadim F, Calabrese RL (1995) ... II. Exploring the parameter space. J Comput Neurosci 2:237-257 | 8521289 | 10.1007/BF00961436 | [V] |
| Hill AA, Lu J, Masino MA, Olsen OH, Calabrese RL (2001) A model of a segmental oscillator in the leech heartbeat neuronal network. J Comput Neurosci 10:281-302 | 11443286 | 10.1023/a:1011216131638 | [V] |
| Hill AAV, Masino MA, Calabrese RL (2002) Model of intersegmental coordination in the leech heartbeat neuronal network. J Neurophysiol 87:1586-1602 | 11877528 | 10.1152/jn.00337.2001 | [V] |
| Masino MA, Calabrese RL (2002) A functional asymmetry in the leech heartbeat timing network ... J Neurosci 22:4418-4427 | 12040049 | 10.1523/JNEUROSCI.22-11-04418.2002 | [V] |

---

## 3. Aplysia: gill-and-siphon withdrawal, habituation and sensitization

### Cells and roles

| Cell | Role | Note |
|---|---|---|
| Siphon (LE) mechanosensory neurons, about 24 [mem] | Slowly adapting mechanoreceptors; discharge rate is a monotonic, roughly exponential function of force [V, Byrne 1978] | Do not themselves decrement at habituating rates [V, Byrne 1978] |
| Gill and siphon motor neurons (L7, L9, LDG1 and others) [mem] | Monosynaptic target; their EPSPs carry the reflex | |
| Excitatory interneurons L29, inhibitory L30 [mem] | Polysynaptic branch | |
| Facilitatory serotonergic neurons (tail pathway) [mem] | Release 5-HT on sensory terminals after a noxious tail shock; cAMP-PKA cascade | Short-, intermediate-, long-term phases [V, Hawkins 2006] |

### Connectivity

| Edge | Sign | Dynamics |
|---|---|---|
| Siphon SN to gill/siphon MN | + (excitatory) | Low-frequency depression with repeated stimulation; heterosynaptic facilitation after a strong stimulus to another pathway [V, Castellucci 1970] |
| Siphon SN to L29 to MN | + then + | Polysynaptic, also modulated [mem] |
| Tail SN to facilitatory interneuron to SN terminal | + modulatory (5-HT) | Raises release, broadens spike [mem] |

Castellucci 1970 [V]: reduction of the reflex to monosynaptic components; both habituation and dishabituation explained in part, perhaps entirely, by changes in efficacy of specific excitatory synapses.

### Key dynamics and scoring

| Observable | Quantity | Source |
|---|---|---|
| Habituation | MN response (EPSP, spikes, or gill contraction) falls over repeated touches; recovers over minutes; faster and deeper at higher repetition rates | [V qualitatively]; exact percentages [mem] |
| Sensor stability | Sensory firing does not decrement; depression is at the synapse | [V, Byrne 1978] |
| Dishabituation | A strong stimulus elsewhere restores the depressed response | [V, Castellucci 1970] |
| Sensitization | Tail shock enhances the reflex for minutes (short-term) up to days (long-term, after repeated spaced trials) | [V, Hawkins 2006 for the phases]; time constants [mem] |

Scores: response ratio last/first (habituation index), recovery fraction after rest, facilitation ratio after shock, and spontaneous-vs-evoked discrimination.

### Minimum circuit (4-5 cells)

```
touch (poisson) --> SN --(tsodyks2, depressing)--> MN --> withdrawal (rate output)
tail shock (poisson) --> FN (facilitatory) --(modulation of SN-MN gain)--> same synapse
```

### NEST fit

| Part | Model | Comment |
|---|---|---|
| SN, MN | `iaf_cond_exp` or `aeif_cond_alpha` | Adapting MN ok |
| Habituation | `tsodyks2_synapse` (depression: `tau_fac` = 0, `U` about 0.3-0.7, `tau_rec` seconds to tens of seconds) | Engine rules in CIRCUIT-FORMAT; weight times U is the first PSP |
| Stochastic release | `quantal_stp_synapse` | Gives trial-to-trial variance |
| Sensitization | Closest: scale post cell with the `modulation` table, or `Engine::set_weight` on the SN-MN projection driven by a program | **Gap**: no NEST synapse whose `U` or weight is gated by a neuromodulator concentration. The `modulation` table targets neuron parameters only (CIRCUIT-FORMAT). Options: extend the engine so a modulator row can target a projection parameter; or use a NESTML synapse |
| Long-term sensitization | `stdp_dopamine_synapse` is the nearest persistent-weight plastic model | Not mechanistically 5-HT/cAMP; label as an abstraction |

### Candidate lesions

| Lesion | Observable symptom |
|---|---|
| Depressing synapse replaced by static | No habituation: response stays constant across repeated touches |
| Facilitatory pathway cut (or its modulation gain zero) | Tail shock no longer sensitizes or dishabituates; habituation intact |
| SN to MN weight too low or MN threshold raised | Reflex absent at all intensities (distinguish from habituation: it is absent on the first trial) |

### Citations

| Ref | PMID | DOI | Status |
|---|---|---|---|
| Castellucci V, Pinsker H, Kupfermann I, Kandel ER (1970) Neuronal mechanisms of habituation and dishabituation of the gill-withdrawal reflex in Aplysia. Science 167:1745-1748 | 5416543 | 10.1126/science.167.3926.1745 | [V] |
| Byrne JH, Castellucci VF, Carew TJ, Kandel ER (1978) Stimulus-response relations and stability of mechanoreceptor and motor neurons mediating defensive gill-withdrawal reflex in Aplysia. J Neurophysiol 41:402-417 | 650274 | 10.1152/jn.1978.41.2.402 | [V] |
| Hawkins RD, Kandel ER, Bailey CH (2006) Molecular mechanisms of memory storage in Aplysia. Biol Bull 210:174-191 | 16801493 | 10.2307/4134556 | [V] |

---

## 4. Stomatogastric ganglion: pyloric rhythm and degeneracy

### Cells and roles (crab or lobster; Cancer, Homarus) [mem unless marked]

| Cell | Count | Role |
|---|---|---|
| AB (anterior burster) | 1 | Intrinsic pacemaker; electrically coupled to PD |
| PD (pyloric dilator) | 2 | Pacemaker kernel with AB; fire together |
| LP (lateral pyloric) | 1 | Follower; reciprocal inhibition with PD |
| PY (pyloric) | about 5 | Followers; fire after LP |
| VD, IC | 1 each | Followers, omitted from the 3-cell model |

### Connectivity (3-cell model of Prinz 2004 [V])

| Edge | Sign | Type | Note |
|---|---|---|---|
| AB <-> PD | + (electrical) | gap junction | AB/PD treated as one unit in the model |
| AB/PD to LP, AB/PD to PY | Inhibitory | Glutamatergic (graded and spike-mediated) plus a slower cholinergic component from PD [mem] | |
| LP to PD | Inhibitory | Glutamatergic | Feedback to the pacemaker |
| LP to PY, PY to LP | Inhibitory | Glutamatergic | |

The 3-cell model of Prinz 2004 [V] was simulated for "more than 20 million versions" with different synapse strengths and neuron properties.

### Key dynamics and scoring

| Observable | Value | Source |
|---|---|---|
| Cycle period | about 1 s (0.5-2 s range, temperature dependent) | [mem] |
| Triphasic pattern: PD bursts first, LP next, PY last | Burst onset phases near PD 0, LP about 0.3-0.5, PY about 0.6-0.7 (period normalised) | [mem] |
| Burst duty cycles | Roughly 0.2-0.4 | [mem] |
| Degeneracy | Virtually indistinguishable network activity from widely disparate parameter sets; many combinations of synaptic strengths and intrinsic properties give appropriate output | [V, Prinz 2004] |
| Single-cell degeneracy | 1.7 million single-compartment models over 8 maximal conductances; activity classes silent, spiking, bursting, irregular | [V, Prinz 2003] |
| Consequences | Animal-to-animal variability in parameters; robust under perturbation [V implication] | |

Scores: period within a band, phase order and phase windows, burst count, then "number of distinct circuits that pass" in a sandbox sweep (degeneracy score).

### Minimum circuit (3 populations)

```
AB/PD (burster, n=1-2, gap-coupled) --inhib--> LP, PY
LP --inhib--> PD, LP --inhib--> PY, PY --inhib--> LP
```
Sizes: PD 2 (or AB 1 + PD 2 gap-coupled for fidelity), LP 1, PY 3-5.

### NEST fit

| Part | Model | Comment |
|---|---|---|
| Pacemaker AB/PD | `butera_burster` (NESTML, persistent Na burster) as the repo's nearest bursting card; or `aeif_cond_alpha` with bursting `a`, `b`, `V_reset` | Neither has the true AB conductance set |
| AB-PD electrical coupling | `gap_junction` on `hh_psc_alpha_gap` or `hh_cond_beta_gap_traub` | Allowed by the engine; but those HH cells fire tonically, they do not burst |
| LP, PY followers | `ht_neuron` (post-inhibitory rebound via I_h, I_T), `aeif_cond_alpha` | LP rebound from PD inhibition is what sets its phase |
| Inhibitory synapses | `static_synapse` negative weight, delay 1-3 ms (the engine bounds |w| for `_cond_` models) | Graded component missing |
| **Gap** | No built-in CaT, CaS, A, H, KCa, proctolin/ modulator-dependent conductances. Degeneracy sweeps (millions of parameter combinations) need a NESTML Liu-type model and an offline sweep, not live play. |

Game-design option: Offer 3 conductance-style sliders on an `aeif` burster plus synapse weights, record the pass count over a random draw (a Prinz-style "how many solutions" counter). Parameter values are the player's. The claim is degeneracy, not any specific number.

### Candidate lesions

| Lesion | Observable symptom |
|---|---|
| LP to PD feedback synapse removed | Rhythm persists (pacemaker intact) but PD and LP overlap or LP phase moves; period slightly shortened [mem] |
| AB/PD gap junction cut (or PD detached from AB) | Pacemaker kernel breaks: irregular or two competing rhythms; PD and AB lose lock |
| One follower's intrinsic conductance (H or rebound current) set to zero | LP never rebounds: LP and PY fail to fire in phase order, or one conductance loss is compensated elsewhere (test of degeneracy) |

### Citations

| Ref | PMID | DOI | Status |
|---|---|---|---|
| Prinz AA, Bucher D, Marder E (2004) Similar network activity from disparate circuit parameters. Nat Neurosci 7:1345-1352 | 15558066 | 10.1038/nn1352 | [V] |
| Prinz AA, Billimoria CP, Marder E (2003) Alternative to hand-tuning conductance-based models. J Neurophysiol 90:3998-4015 | 12944532 | 10.1152/jn.00641.2003 | [V] |
| Marder E, Bucher D (2001) Central pattern generators and the control of rhythmic movements. Curr Biol 11:R986-996 | 11728329 | 10.1016/s0960-9822(01)00581-4 | [V] |

---

## 5. Drosophila mushroom body: PN to KC to MBON, APL, DAN learning

### Cells and roles

| Cell type | Count (one hemisphere) | Role |
|---|---|---|
| Olfactory projection neurons (PN) | about 150 uniglomerular [mem] | Excitatory (ACh) input from antennal lobe glomeruli |
| Kenyon cells (KC) | about 2000 [V, Aso 2014a] | Sparse odor code; excitatory (ACh) |
| MBON | 34 neurons of 21 types [V, Aso 2014b] | Output; segregated dendrites tile 15 compartments [V, Aso 2014a] |
| DAN | 20 types, each to one or at most two compartments [V, Aso 2014a] | Teaching signal (reward or punishment) |
| APL | 1 per hemisphere [mem] | Giant GABAergic neuron, feedback inhibition on KCs; sets sparseness |

The locust equivalent of APL is a nonspiking, graded-release giant interneuron whose negative-feedback loop keeps KC output sparse over a wide range of input [V, Papadopoulou 2011, locust, not Drosophila]. KC activity is sparse, about 5-10% of KCs per odor [mem].

### Connectivity

| Edge | Sign | Counts, rule | Source |
|---|---|---|---|
| PN to KC | + | Each KC has about 5-7 claws, each claw from a different PN, random across glomeruli [mem]; so KC in-degree about 5-7 | [mem] |
| KC to APL | + | Dense | [mem] |
| APL to KC | Inhibitory (GABA) | Dense, global | [mem] |
| KC to MBON | + (cholinergic) | Each MBON samples many KCs along a compartment: thousands of KC inputs | [V, Aso 2014a] qualitative |
| DAN to KC-MBON synapse (presynaptic KC terminal) | Modulatory | One DAN type per compartment; convergent with the MBON dendrite [V, Aso 2014a] | |
| MBON to MBON, MBON to DAN | Mixed; three MBON types form a feedforward network in the lobes [V] | | |

### Key dynamics and scoring

| Observable | Value | Source |
|---|---|---|
| Sparse KC response | Few percent of KCs per odor, odor-specific subset | [mem]; sparse code [V, Hige 2015] |
| Learning rule | Odor (KC activity) followed by DAN activation depresses that odor's KC to MBON synapse; **order matters** (odor then DAN, not DAN then odor) | [V, Hige 2015] |
| Odor specificity and generalisation | Overlap between sparse KC representations of two odors defines how far the depression transfers | [V, Hige 2015] |
| Compartment specificity | Dopamine acts only within its own compartment | [V, Hige 2015] |
| Valence | MBON ensemble collectively represents valence; activation of MBON types drives repulsion or attraction by type | [V, Aso 2014b] |
| Size of depression | Tens of percent of the EPSP [mem] | check Hige 2015 |

Scores: KC sparseness (fraction active), odor discriminability (overlap of KC sets), post-training MBON response ratio for CS+ vs CS-, preference index, generalisation to a similar odor.

### Minimum circuit (about 40-100 cells, scaled)

```
PN (n=10-20) --sparse random (indegree 3-6)--> KC (n=100-200)
KC --> APL (n=1)  --inhib--> KC            (gain control)
KC --(stdp_dopamine_synapse)--> MBON (n=1-2; approach and avoid)
DAN (n=1-2) --> volume transmitter (dopamine_source for KC->MBON)
```
Scale-down: 2000 KC to 200 and 150 PN to 20 keeps indegree about 5 and sparseness about 5-10% via APL.

### NEST fit

| Part | Model | Comment |
|---|---|---|
| PN, KC | `iaf_cond_alpha` or `iaf_psc_alpha` (KC high threshold) | Sparse code needs a high threshold plus inhibition |
| APL | `iaf_cond_alpha` with `fixed_indegree` (all to one, one to all) | NEST lacks a graded APL; spiking APL with fast GABA works as a proxy |
| PN to KC random wiring | `fixed_indegree`, `indegree` 5-6, per-KC random (engine supports) | |
| KC to MBON plasticity | `stdp_dopamine_synapse` with `dopamine_source: DAN` | Engine builds `volume_transmitter` and an engine-wide DA level |
| Alternative | `static_synapse` plus offline weight update by task code | Safer for a first chapter |
| **Gaps** | (1) `stdp_dopamine_synapse` needs pre and post spikes to build its eligibility trace; the real rule is presynaptic (KC) activity times DAN, with no postsynaptic spike term. Workaround: MBON fires at baseline so pairings exist. (2) The real rule is **depression**; `stdp_dopamine_synapse` is built as dopamine-gated potentiation, so sign (negative `A_plus`/`A_minus`, or reversing the dopamine baseline `b`) must be tested [mem, not tested]. (3) The engine's weight rules for `stdp_dopamine_synapse` require `weight` > 0 and `Wmax`: good for excitatory KC-MBON, fine. (4) Compartment-specific DA: one `dopamine_source` per projection, fine (each projection has its own transmitter). |

### Candidate lesions

| Lesion | Observable symptom |
|---|---|
| APL removed or its weight zeroed | KC activity density rises (dozens of percent): odor codes overlap, generalisation increases, learning no longer odor-specific |
| DAN silenced (or DAN to synapse link cut) | Odor-shock pairing leaves the MBON response unchanged: no learning, naive responses intact |
| KC to MBON link cut for one MBON class | That valence channel is missing: preference index becomes one-sided; or MBON never responds to odors |

### Citations

| Ref | PMID | DOI | Status |
|---|---|---|---|
| Aso Y et al. (2014) The neuronal architecture of the mushroom body provides a logic for associative learning. eLife 3:e04577 | 25535793 | 10.7554/eLife.04577 | [V] |
| Aso Y et al. (2014) Mushroom body output neurons encode valence and guide memory-based action selection in Drosophila. eLife 3:e04580 | 25535794 | 10.7554/eLife.04580 | [V] |
| Hige T, Aso Y, Modi MN, Rubin GM, Turner GC (2015) Heterosynaptic plasticity underlies aversive olfactory learning in Drosophila. Neuron 88:985-998 | 26637800 | 10.1016/j.neuron.2015.11.003 | [V] |
| Papadopoulou M, Cassenaer S, Nowotny T, Laurent G (2011) Normalization for sparse encoding of odors by a wide-field interneuron. Science 332:721-725 | 21551062 | 10.1126/science.1201835 | [V] (locust; APL analogue) |
| Li F et al. (2020) The connectome of the adult Drosophila mushroom body (hemibrain). eLife | not searched | not looked up | UNVERIFIED |
| Turner GC, Bazhenov M, Laurent G (2008) Olfactory representations by Drosophila mushroom body neurons. J Neurophysiol | not searched | not looked up | UNVERIFIED |

---

## 6. Drosophila central complex: head-direction ring attractor

### Cells and roles

| Cell type | Approx. count (hemisphere) [mem] | Role |
|---|---|---|
| E-PG (compass neurons) | about 40-50 | Dendrites tile the 16 ellipsoid-body (EB) wedges; axons to protocerebral bridge (PB) glomeruli. Carry the heading bump [V, Seelig 2015: population in a toroidal EB shows ring-attractor features] |
| P-EN (shifting neurons) | about 40 | Conjunctive heading and angular velocity; excited by turns in one direction; mirror-symmetric left and right subtypes [V, Turner-Evans 2017; Green 2017] |
| Delta7 (PB local neurons) | about 40 | Inhibition across the PB with a separation of about half the ring; implements global competition [mem] |
| Ring neurons (R) | many | Visual and landmark input to the EB, GABAergic [mem] |
| Angular-velocity input | | Self-motion (turn) signal reaches P-EN [V] |

### Connectivity (computational essence) [V for shift loop; counts [mem]]

| Edge | Sign | Function |
|---|---|---|
| E-PG to P-EN | + | Heading copy sent into the shifting populations |
| P-EN to E-PG | + | Returns heading shifted by one step clockwise or anticlockwise: "recurrent loops with an angular shift" [V, Turner-Evans 2017] |
| Delta7 and ring-neuron inhibition | Inhibitory | Global competition; keeps a single bump [mem] |
| Landmark (R) to E-PG | Inhibitory/learned | Anchors the bump to the visual scene [V qualitatively, Seelig 2015] |

### Key dynamics and scoring

| Observable | Value | Source |
|---|---|---|
| Single activity bump on the EB ring | One localised packet; moves with the fly's heading | [V, Seelig 2015] |
| Tracking | Follows visual landmarks; in darkness follows self-motion; angular path integration | [V, Seelig 2015] |
| Persistence | Bump maintained with no visual or self-motion cues (persistent activity, a short-term memory) | [V, Seelig 2015] |
| Shifting | P-EN activity shifts the bump in proportion to angular velocity; silencing shifting neurons breaks tracking in the dark, stimulating them induces predictable shifts | [V, Green 2017] |
| Bump width, gain, drift | Numbers not checked | [mem] |

Scores: circular error between decoded bump angle and true heading; gain (bump angular speed / true turning rate); drift in the dark (deg/s); bump persistence time with input off; number of bumps.

### Minimum circuit (about 25-40 cells)

```
E-PG  ring of N=16 (n=16)                 recurrent local excitation + global inhibition
P-EN-L ring of 16, P-EN-R ring of 16      (or 8+8 for a coarse 8-wedge ring)
E-PG[i] --+--> P-EN-L[i], P-EN-R[i]
P-EN-L[i] --+--> E-PG[i-1]     P-EN-R[i] --+--> E-PG[i+1]       (shift loop)
Delta7 (n=1-4) <-- E-PG, --inhib--> E-PG, P-EN (global competition)
turn input: dc or poisson to P-EN-L (CCW) or P-EN-R (CW)
heading cue input: step current into one E-PG wedge
```
Smallest: 8 E-PG + 8 + 8 P-EN + 1 inhibitory = 25 cells.

### NEST fit

| Part | Model | Comment |
|---|---|---|
| E-PG, P-EN | `iaf_cond_alpha` / `iaf_psc_alpha` | Rate-coded bump emerges from recurrent excitation and inhibition |
| Ring wiring | `edges` projections (`[pre, post, weight, delay]`) with weight `+/- k` offset | Engine supports; pure `rule` connections do not offer ring offsets |
| Weight spread (heterogeneity) | `weight_cv` (CIRCUIT-FORMAT v0.3) | A natural "ring attractor only works with identical weights" difficulty control |
| Inhibition | `static_synapse` negative weight | |
| Persistent bump | Needs recurrent gain near 1; NMDA-like slow synapses help: `iaf_cond_alpha_mc`, `ht_neuron` (NMDA port) or `aeif_cond_beta_multisynapse` | |
| **Gap** | No NEST gap for the network itself. Engine gap: output kinds are `rate` and `modulator` over a set of neurons (CIRCUIT-FORMAT). A bump angle needs a population-vector readout, i.e. a task-side metric, not an output kind. |

### Candidate lesions

| Lesion | Observable symptom |
|---|---|
| P-EN-L (one shift direction) silenced | Bump moves only one way: turning in the other direction is not integrated; heading error grows with one-sided turns |
| Delta7 / global inhibition removed | Bump broadens, then multiple bumps or saturation of the whole ring |
| Shift loop sign or offset wrong (P-EN to E-PG offset 0 or 2) | Bump does not move, or moves at the wrong rate/direction (gain far from 1) [mechanism V, Turner-Evans 2017] |
| E-PG recurrent gain too low | Bump decays in darkness: no persistence |

### Citations

| Ref | PMID | DOI | Status |
|---|---|---|---|
| Seelig JD, Jayaraman V (2015) Neural dynamics for landmark orientation and angular path integration. Nature 521:186-191 | 25971509 | 10.1038/nature14446 | [V] |
| Turner-Evans D et al. (2017) Angular velocity integration in a fly heading circuit. eLife 6:e23496 | 28530551 | 10.7554/eLife.23496 | [V] |
| Green J, Adachi A, Shah KK, Hirokawa JD, Magani PS, Maimon G (2017) A neural circuit architecture for angular integration in Drosophila. Nature 546:101-106 | 28538731 | 10.1038/nature22343 | [V] |
| Hulse BK et al. (2021) A connectome of the Drosophila central complex reveals network motifs suitable for flexible navigation and context-dependent action selection. eLife | not searched | not looked up | UNVERIFIED |

---

## 7. Connectome data check (run 2026-11-18 from this machine)

Method: `curl -I` or small GET through the session proxy, 12-20 s timeout, nothing larger than a listing page fetched. "Blocked" means the proxy refused the CONNECT tunnel (HTTP 403), i.e. the host is not on the allowlist.

### Reachability

| Host / URL | Result | Meaning |
|---|---|---|
| wormwiring.org | **Blocked** (CONNECT 403) | Cook 2019 matrices at the source are not reachable |
| github.com, api.github.com, codeload (zip) | 403 JSON "GitHub access to this repository is not enabled for this session. Use add_repo" | Repo-gated, not host-blocked; `add_repo` for `openworm/c302` or `openworm/ConnectomeToolbox` would unlock it (not called) |
| raw.githubusercontent.com | **Reachable** (200) for `openworm/c302` README, LICENSE, `setup.py`; `openworm/ConnectomeToolbox` README, LICENSE | Single raw files can be pulled. Guessed paths for `openworm/CElegansNeuronTables` returned 404, so the real file names are unknown here, not a block |
| pypi.org (simple index) | **Reachable** for `cect`, `c302`, `owmeta-core`, `fafbseg`, `caveclient`, `navis`, `neuprint-python`, `nest-simulator` | Package metadata ok; `cect` 0.3.5 wheel is 13.6 MB, so not downloaded |
| codex.flywire.ai | **Blocked** (CONNECT 403) | |
| cave.flywire.ai, prod.flywire-daf.com, data.flywire.ai | **Blocked** | FlyWire annotation/CAVE servers and data host unreachable |
| neuprint.janelia.org, hemibrain-dvid.janelia.org, www.janelia.org | **Blocked** | |
| storage.googleapis.com | Reachable, but only buckets such as `flywire_v141_m630`, `flywire_neuropil_meshes` listed | Image segmentation and meshes, not synapse tables; useless for a 10-50 cell excerpt |
| zenodo.org, figshare.com, doi.org, ncbi.nlm.nih.gov, eutils, api.crossref.org, elifesciences.org, docs.nest-simulator.org, huggingface.co | **Blocked** | No DOI resolution or NEST docs from curl; PubMed works through the MCP tools instead |
| `import nest` in this sandbox | Not installed | NEST model names above are from background knowledge of NEST 3.x, not tested here |

### Source details

| Source | Format | Auth | Licence | Smallest practical subset |
|---|---|---|---|---|
| **Cook et al. 2019 / WormWiring** | Adjacency matrices (chemical and gap, hermaphrodite and male) as Excel/CSV supplement tables [mem] | None [mem] | Not stated in anything fetched; check the Nature SI and wormwiring.org terms (Nature: [V] it is a 2019 Nature paper; PMC copy PMC6889226 exists) | Open the matrix, keep rows and columns for the 14 touch cells (ALML/R, AVM, PLML/R, AVDL/R, AVAL/R, AVBL/R, PVCL/R) plus motor classes; kilobytes |
| **OpenWorm ConnectomeToolbox (`cect`)** | Python package bundling several published datasets (README: "multiple connectome datasets from different research groups"); per-dataset reader modules | None | **MIT** [V from LICENSE, (c) 2026 OpenWorm]; underlying datasets keep the original publication terms | `pip install cect` in a scratch venv outside this sandbox's proxy limits (13.6 MB), then select cells by name in pandas; or fetch one data file with raw.githubusercontent.com once its path is known. The exact reader API and file list were not inspected |
| **OpenWorm c302** | NeuroML2 generator with built-in worm data files | None | **MIT** [V from LICENSE, (c) 2024 OpenWorm] | Use for ready-made conductance-based worm cells; not needed for the touch circuit |
| **CElegansNeuronTables** | Spreadsheet of White 1986 / Varshney data [mem] | None | Unknown | Could not locate by guess (404); do not rely |
| **FlyWire (codex.flywire.ai)** | Codex bulk downloads (feather/CSV: neurons, classification, connections) and the CAVE API (`caveclient`, `fafbseg`) | Codex browse and downloads: free account [mem]; CAVE API needs a personal token | CC BY 4.0 [mem], check site | With `caveclient`: `synapse_query(pre_root_ids=[...])` for named cell ids, or filter the Codex connections table by `cell_type`; MB (KC, MBON, DAN, APL) and EB (E-PG, P-EN, Delta7) are cell-type labels. Both hosts are blocked here, so this must run off-sandbox |
| **neuPrint hemibrain** | neuPrint server (Neo4j) via `neuprint-python`: `NeuronCriteria(type='EPG')`, `fetch_adjacencies`, `fetch_simple_connections` | Token required (Google login to get one) [mem] | CC BY 4.0 [mem], check dataset page | `fetch_adjacencies(sources=NeuronCriteria(type='EPG'), targets=NeuronCriteria(type='PEN_a'))` returns a compact table; 10-50 cells is a few KB. Server blocked here |

Practical plan for the Bestiary build
1. Do extractions off-sandbox (developer machine) and commit only the small derived tables (cell, type, signed weight) under `research/data/` with the source, release and licence in a header.
2. For the worm, the cheapest route that stays inside this sandbox: `add_repo` `openworm/ConnectomeToolbox`, or request `pip install cect` (pypi reachable, wheel 13.6 MB).
3. FlyWire and hemibrain need an allowlist request for `codex.flywire.ai`/`cave.flywire.ai` and `neuprint.janelia.org` plus a user-supplied token (use `request_credential`), or an off-sandbox export.

---

## 8. Open items

| Item | Needed before |
|---|---|
| Numeric targets marked [mem] (periods, phases, depression sizes, cell counts) | Writing any task pass criteria |
| Verify the unverified refs: White 1986, Li 2020, Turner 2008, Hulse 2021 | Putting them in `catalog/citations.json` |
| Test negative `A_plus`/`A_minus` or reversed baseline in `stdp_dopamine_synapse` for MB depression | MB task design |
| Decide engine feature: modulator row targeting a projection parameter (Aplysia sensitization) | Aplysia task |
| Decide engine feature: population-vector (bump angle) metric | Central complex task |
| Check whether the circuit loader accepts rate-neuron models | Graded-cell approximation for C. elegans/leech |
