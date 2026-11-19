# Bestiary research B: six canonical circuits

Fact sheets for the Bestiary chapters (locust, zebrafish, barn owl, zebra finch, cat, rat). Each sheet: cells, wiring, scorable numbers, minimum circuit, NEST fit, gaps, lesions, citations.

**Evidence tags.** `[V]` = read in the PubMed abstract during this research (PMID in the citation table). `[M]` = from memory of the literature, order of magnitude only, **not verified**: re-check before it goes into player-facing text. Counts marked `~` are game-scale or literature estimates, not anatomy to copy.

**Engine facts that constrain every sheet** (read from `/home/user/spike`):

| Fact | Source | Consequence |
|---|---|---|
| `inputs[].kind` is only `dc` or `poisson`; anything else is a load error | `engine/src/circuit.cpp:577` | `spike_generator`, `sinusoidal_poisson_generator`, `ac_generator` are **not reachable from circuit JSON**. Needs an engine change, or the task drives the Poisson rate each 1 ms slice (rate = f(t)). |
| `resolution_ms` must be in [0.001, 10]; edge delay must be >= resolution | `circuit.cpp` | 1 us is the engine floor. |
| Engine does not set `tics_per_ms` | `engine.cpp` | NEST default 1000 tics/ms, so 0.001 ms is the smallest grid. Finer needs `tics_per_ms` set together with `resolution` (NEST refuses one without the other, `simulation_manager.cpp:293`). |
| Models in the build include `sinusoidal_poisson_generator`, `spike_generator`, `inhomogeneous_poisson_generator`, `gamma_sup_generator`, `pulsepacket_generator`, `iaf_psc_alpha_ps`, `iaf_psc_exp_ps`, `hh_cond_beta_gap_traub`, `hh_psc_alpha_gap`, `aeif_cond_alpha`, `aeif_cond_alpha_multisynapse`, `iaf_cond_alpha_mc`, `cm_default`, `ht_neuron`, `izhikevich`, `glif_cond`, `bernoulli_synapse`, `parrot_neuron` | `third_party/nest-build/models/models.cpp` | Models exist in the kernel; the circuit-JSON limits above are the real bottleneck. |
| Gap junctions: both sides must be `hh_psc_alpha_gap` or `hh_cond_beta_gap_traub`, no delay, symmetric, weight in nS > 0 | `CIRCUIT-FORMAT.md` | Usable for the Mauthner club-ending analogue; NEST waveform relaxation makes it slower. |
| Cond-based weights <= 500 nS, current-based <= 10000 pA | `CIRCUIT-FORMAT.md` | Fine for all sheets. |
| Existing repo circuits to reuse | `docs/circuits/{m_half_centre,m_feedforward,m_timed_bursts,renshaw,stretch_reflex,m_reciprocal,m_threshold}.md` | Cat CPG and Renshaw already exist as motifs; the Bestiary chapter should extend, not duplicate. |

---

## 1. Locust: LGMD / DCMD looming detector

### Cells and roles

| Cell / layer | Role | Count (game) | Count (animal) |
|---|---|---|---|
| Photoreceptor/medulla "P" units | Edge-sensitive: fire to any luminance change as an edge crosses them | ~250 `[V]` in Rind & Bramwell model; 20-50 enough for a game | thousands of ommatidia `[M]` |
| Excitatory (E) retinotopic afferents | Feed the LGMD dendritic fan a; excitation grows with edge velocity | 1 per P unit | |
| Lateral inhibitory (I) units | Spread inhibition sideways (excitation must "win the race"; `[V]` Rind & Bramwell 1996) | 1 per P unit | |
| Feedforward inhibitory pathway to LGMD (dendritic field b/c, large-field) | Fires when many P units are excited in a short time; truncates the response at end of approach `[V]` | 1-4 units | |
| LGMD | Wide-field looming detector, the integrator | 1 | 1 per optic lobe |
| DCMD | 1:1 spike relay of LGMD down the nerve cord to jump/flight motor circuits | 1 | 1 per side |

### Connectivity

| Pre | Post | Sign | Notes |
|---|---|---|---|
| P (retinotopic) | LGMD fan a (excitatory) | + | Many to one; convergence. Excitation = edge velocity-driven, size-sensitive `[V]` (Gabbiani 1999: motion- and size-sensitive inputs) |
| P | lateral I units (neighbours) | - | Local lateral inhibition, ~1 ms spread; selectivity for approach vs recede is lost if removed `[V]` |
| P (population) | feedforward I -> LGMD | - | Postsynaptic (onto LGMD dendrite) inhibition dominates; multiplication is in the neuron `[V]` Gabbiani 2002 |
| LGMD | DCMD | + (1:1) | Reliable relay `[M]` |

### Scorable behaviour

| Metric | Value | Tag |
|---|---|---|
| Peak DCMD rate occurs a fixed delay delta after the object reaches an angular threshold theta_thres | delta 15-35 ms; theta_thres 15-40 deg per animal, independent of object size and speed | `[V]` Gabbiani 1999 |
| Rate law | f(t) ~ psi(t-delta) * exp(-alpha * theta(t-delta)), alpha = 1/tan(theta_thres/2) | `[V]` |
| Approach vs recession | Strong preference for approach; two peaks (onset + build-up) for approaching disks >= 5 deg; single onset peak for recession | `[V]` Rind & Simmons 1992 |
| Width-of-selectivity | Wide-field background motion suppresses the response; low-contrast approach still selective for dark-on-light | `[V]` Rind & Simmons 1992 |
| Game score ideas | (a) peak time relative to l/v collision time (theta_thres match); (b) peak-rate ratio approach:recede (> 3:1); (c) size/speed invariance of theta_thres across 3 l/v values (scatter < 20%); (d) false alarm to translating edge at constant speed | game design |
| Looming-to-escape latency | Jump decision after DCMD peak, tens of ms | `[M]` |

### Minimum circuit

~20 P units (a ring or a 1-D expanding edge, 2 edges moving apart) -> LGMD (one `aeif_cond_alpha` or `iaf_cond_alpha`), each P drives E with delay 1-2 ms, and a **fast feedforward inhibitory unit** driven by the summed P activity inhibits the LGMD with a delay of ~2-4 ms. Add lateral inhibition between adjacent P/E units. The edge pair is generated by the task (input = Poisson rate vs position and time). Without the feedforward inhibition the LGMD does not turn off at end of approach and does not sharpen to theta_thres.

### NEST fit

| Need | NEST | Note |
|---|---|---|
| P units, E, I | `iaf_cond_alpha` / `iaf_psc_alpha` | Edge sensors = driven Poisson (rate step when an edge passes). Fits the existing `poisson` input plus a task that sets per-sensor rate. |
| LGMD with spike-frequency adaptation and exponentiating active conductance | `aeif_cond_alpha` (`a`, `b` adaptation; `Delta_T` gives exponential spike onset) | Delta_T ~ 2-4 mV approximates the near-exponential rate/voltage mapping `[V]` Gabbiani 2002. Dendritic multiplication itself needs a compartment model, see gaps. |
| Looming stimulus | Task-computed rates | `inhomogeneous_poisson_generator` is in the kernel but not an input kind; task writes the rate per 1 ms slice. |

### Gaps

- **Dendritic multiplication** (log-subtract-exp inside the LGMD): point neurons can only approximate. `cm_default` (compartmental) or `iaf_cond_alpha_mc` exist in the kernel; neither is wired to the circuit format.
- Spatial "dendritic fans" and the three field geometry: no spatial model; retinotopy must be an indexing convention in the task.
- No built-in angular-looming stimulus; the task code must generate it.

### Diagnosis lesions

| Lesion | Observable symptom |
|---|---|
| Feedforward inhibitory unit deleted (or its weight zeroed) | LGMD keeps firing past the end of approach; theta_thres no longer constant, response to recession no longer truncated `[V]` (Rind & Bramwell: eliminating the loop prolongs the response) |
| Lateral inhibition removed | Loss of approach/recede selectivity; high response to a translating edge at constant speed `[V]` |
| Excitatory E gain too high / threshold shifted | Early peak (theta_thres too small), jump fires too soon, false alarms on slow translation |

---

## 2. Zebrafish larva: Mauthner-cell C-start

### Cells and roles

| Cell | Role | Count (game) | Count (animal) |
|---|---|---|---|
| Auditory/vestibular afferents (VIII nerve, otic vesicle hair cells) | Sensory input to the M-cell lateral dendrite; "club endings" are mixed electrical + chemical `[M]` | 4-8 | |
| Trigeminal (head-tactile) afferents | Alternative input; wins before ~75 hpf `[V]` Kohashi 2012 | 2-4 | |
| Mauthner cell (M-cell), left and right | Command neuron. One spike -> contralateral C-bend. Sets the escape threshold `[V]` Korn & Faber 2005 | 2 (1 per side) | 1 pair |
| Feedforward inhibitory neurons (PHP / "passive hyperpolarising potential" cells) | Driven by the same afferents; inhibit the M-cell (and set threshold, habituation) `[V]` Takahashi 2002; Marsden & Granato 2015 | 2 per side | |
| Commissural inhibition (M-cell -> contralateral M-cell reciprocal inhibition, via commissural interneurons "CoLo" and the "reciprocal" pathway) | Ensures only **one** M-cell fires: picks one side `[V]` Takahashi 2002 ("reciprocal inhibition between bilaterally opposed M-cells") | 1-2 per side | |
| Recurrent inhibition | M-cell axon collateral drives an ipsilateral inhibitory neuron that shunts the M-cell after its own spike `[V]` Takahashi 2002 | 1 per side | |
| Spinal motoneurons / contralateral trunk muscle | Receive M-cell axon via CoLo/primary MN; drive C-bend | 4 per side | |
| Homologues MiD2cm, MiD3cm | Add long-latency (non-M) escapes when M-cell is ablated or head-tactile input used `[V]` Kohashi & Oda 2008 | optional | |

### Connectivity

| Pre | Post | Sign | Notes |
|---|---|---|---|
| VIII afferent (ipsilateral) | M-cell ipsilateral (lateral dendrite) | + mixed electrical/chemical | Strong, 1-2 ms, sets 3-5 ms latency `[V]` |
| VIII afferent | Feedforward inhibitory neuron -> **contralateral** M-cell | - (glycinergic) | `[V]` Takahashi 2002: feedforward suppression of the contralateral M-cell |
| M-cell | contralateral M-cell via commissural inhibitory interneuron | - | Reciprocal inhibition `[V]` |
| M-cell | recurrent inhibitory interneuron -> same M-cell | - | `[V]` |
| M-cell axon | contralateral trunk motor neurons (via spinal interneurons/CoLo) | + | Crosses midline: contralateral bend `[V]` |

### Scorable behaviour

| Metric | Value | Tag |
|---|---|---|
| Short-latency escape (SLC) latency | Minimum onset ~5 ms at 45-59 hpf, ~3 ms after 75 hpf | `[V]` Kohashi 2012 |
| M-cell spike count per escape | Single spike (Ca response = one AP) | `[V]` Kohashi & Oda 2008 |
| Direction | Contralateral to the stimulated ear; left/right must be exclusive | `[V]` |
| Without M-cell | Escape-like behaviour still occurs, at longer latency (LLC) | `[V]` Gahtan 2002; Kohashi 2012 |
| Habituation | Startle probability falls with repeated stimulation; depression of lateral-dendrite Ca, plus feedforward inhibition drive | `[V]` Marsden & Granato 2015 |
| Game score ideas | (a) latency from stimulus to M spike < 5 ms; (b) P(both sides fire) < 1% under bilateral input; (c) P(M fires) vs stimulus strength sigmoid with threshold; (d) habituation index over 10 repeats | game design |

### Minimum circuit

2 afferent groups (L, R, 4 cells each) -> 2 M-cells (L, R, `aeif_cond_alpha` or `hh_cond_beta_gap_traub`) + 2 feedforward inhibitory cells (cross: afferent_L -> FFI -> M_R) + 2 commissural inhibitory cells (M_L -> CI -> M_R, and mirror) + a recurrent inhibitory cell. Output: M_L spike -> right motor pool and vice versa. Bilateral near-equal input is the hard case: whichever side leads by ~1 ms must win and silence the other.

### NEST fit

| Need | NEST | Note |
|---|---|---|
| M-cell | `hh_cond_beta_gap_traub` (spikes, no adaptation) | Fast, deterministic single spike; threshold by synaptic weight. |
| Club-ending electrical coupling afferent -> M | `gap_junction` between two `hh_cond_beta_gap_traub` | No delay, symmetric: afferents would be back-driven by the M-cell, which is not biologically one-way (club endings rectify `[M]`). Alternative: a chemical synapse with `delay = resolution` and large weight. |
| Mixed synapse | Gap junction **and** a parallel `static_synapse` | Both can be in the circuit. |
| Latency in ms | `resolution_ms` 0.05-0.1 is enough for 3-5 ms latencies (3 ms = 30-60 steps) | Fine. |
| Feedforward inhibition | `static_synapse` with negative weight, delay 0.5-1 ms | Plain `m_feedforward` motif. |
| Habituation | `tsodyks2_synapse` on the afferent -> M and afferent -> FFI synapses (depression) | Already in the engine. |

### Gaps

- **Rectifying electrical synapse**: NEST gap junctions are ohmic and symmetric; no rectification (club endings are rectifying `[M]`). Use a chemical proxy for the one-way part.
- **Gap junctions cost**: they need waveform relaxation, so they slow the whole simulation; keep their number small (< 20).
- M-cell axon cap / extracellular "electrical inhibition" (field effect on the axon cap, `[M]`): no NEST model.
- Real M-cell is a giant (soma ~ 50 um, axon ~ 30 um): `hh_cond_beta_gap_traub` has no geometry; tune weights instead.

### Diagnosis lesions

| Lesion | Observable symptom |
|---|---|
| Commissural (reciprocal) inhibition cut | Under near-simultaneous bilateral input both M-cells fire -> bilateral muscle contraction, no C-bend (the fish "freezes" or straightens); P(both) > 20% |
| Feedforward inhibition cut | Lower threshold, no habituation, spontaneous escapes to weak sub-threshold stimuli `[V]` Marsden & Granato 2015 (increased FFI drives habituation) |
| M-cell removed / afferent from VIII cut | No short-latency escape; only slow (~10+ ms) long-latency escapes via MiD cells `[V]` Kohashi 2012, Gahtan 2002 |

---

## 3. Barn owl: ITD coincidence detection in nucleus laminaris

### Cells and roles

| Cell | Role | Count (game) | Count (animal) |
|---|---|---|---|
| Nucleus magnocellularis (NM), left and right | Phase-locked relay of auditory-nerve spikes (4-7.5 kHz range recorded `[V]`); axons are the delay lines | 8-40 per side | |
| Nucleus laminaris (NL) | Coincidence detectors; bipolar with very short dendrites `[V]`; fire for the ITD that cancels the axonal delay difference | 20-100 | |
| Superior olivary nucleus (SON) / inhibition | GABAergic feedback onto NL and NM; controls gain over level `[M]` | 1-2 | |
| ICx / space map target | Reads NL tonotopic ITD maps | outside circuit | |

### Connectivity and delays

| Pre | Post | Sign | Delay |
|---|---|---|---|
| NM ipsilateral axon (enters NL dorsally) | NL cell | + | Increases with dorsoventral depth |
| NM contralateral axon (enters NL ventrally) | NL cell | + | Decreases with depth: opposite gradient |
| Range of conduction time over NL depth | | | **160 us over 700 um** for all frequencies 4-7.5 kHz, matching the owl's ITD range `[V]` Carr & Konishi 1990 |
| Axon conduction velocity | | | 3-5 m/s with ~60 um internodes `[V]` |
| NL output | ICx | + | outside circuit |

The Jeffress model needs each NL cell to see the two NM trains offset by tau_i - tau_c; the NL cell at depth x peaks at ITD = (tau_c(x) - tau_i(x)).

### Scorable behaviour

| Metric | Value | Tag |
|---|---|---|
| NL response at best ITD | Spike count roughly 2x monaural; below monaural at worst ITD `[V]` Carr & Konishi 1990 | `[V]` |
| ITD tuning curve | Periodic at 1/f (cycle ambiguity: at 5 kHz, period 200 us); main peak half-width ~ 1/4 of a period (~50 us at 5 kHz) | `[M]` |
| ITD range of the map | about +-100 to +-200 us (owl head) | `[V]` 160 us conduction range; `[M]` head ITD range |
| Behavioural ITD acuity | ~ 10 us or better | `[M]` |
| Phase locking (vector strength) of NM | ~ 0.6-0.9 up to 7-9 kHz | `[M]` |
| Game score ideas | (a) best ITD of NL cell k equals designed ITD within +-10 us; (b) tuning half-width; (c) peak:trough ratio of spike count vs ITD (>= 2:1 as `[V]`); (d) monotonic map: best ITD varies smoothly with index | game design |

### Minimum circuit

2 x N NM input trains (N = 8), M NL neurons (M = 8-16), each NL cell gets 4-8 contralateral and 4-8 ipsilateral inputs with delays that ramp linearly across the array. A single carrier frequency (e.g. 3-5 kHz) is enough; tonotopy is the gap-filling extension (multiple frequency channels).

### Delay resolution: the key NEST problem

NEST `delay` is rounded to a multiple of the simulation resolution; delay >= resolution; spike times of grid-based neurons and generators are also snapped to the grid unless an off-grid/precise path is used. Table for a +-160 us range with a 5 kHz carrier (period 200 us):

| Resolution | Delay values across 0-160 us | Steps per carrier cycle | Viable? |
|---|---|---|---|
| 0.1 ms (game default) | 1-2 (0 or 100 us) | 2 | **No.** No ITD map is possible; carrier is aliased. |
| 0.05 ms | ~3-4 | 4 | No. |
| 0.01 ms (10 us) | ~16 | 20 | **Marginal.** Resolution matches behavioural acuity (10 us) at best; usable for a map with 8-16 delay slots at <= 4 kHz. Runs 10x slower than the default. |
| 0.001 ms (1 us) | ~160 | 200 | **Yes for a small circuit.** This is the engine floor (`circuit.cpp`: [0.001, 10]). Steps = 1000 per simulated ms; a 1 s trial = 1e6 steps. Fine for ~100-300 neurons; 1 ms task slices still work. |
| < 1 us | n/a | | Needs `tics_per_ms` raised together with `resolution` in NEST kernel params; the engine does not expose it. Not needed. |

Other timing issues, all real:

1. The engine only offers `poisson` and `dc` inputs. A Poisson train is **not phase-locked**, so it cannot carry ITD. Needed: `sinusoidal_poisson_generator` (phase-locked with vector strength capped near 0.5 because `ac <= dc` for a non-negative rate `[M]`, weaker than the real NM, ~0.7-0.9 `[M]`) or a `spike_generator` with pre-computed precise times (`precise_times` / `allow_offgrid_times` exist `[V]` in the NEST header). Both need a new input kind in `circuit.cpp` / `engine.cpp`.
2. The sinusoidal generator at 5 kHz needs resolution <= 0.01 ms to sample the rate waveform sensibly; 0.001 ms is clean.
3. Grid-based neurons (`iaf_psc_alpha`, `aeif_cond_alpha`) place their spikes on the grid; at 1 us that is acceptable. Off-grid precision needs `iaf_psc_alpha_ps` / `iaf_psc_exp_ps` (in the build) and `precise_times` generators.
4. Synaptic delay per connection = the ITD slot. All delays must be set per edge (`edges` projections carry `[pre, post, weight, delay]`: fine).
5. Cost: at 1 us, simulated time per wall second is ~100x worse than at 0.1 ms; budget trials of 20-50 ms (100-250 carrier cycles).

Recommendation: ship the owl chapter at `resolution_ms = 0.001`, <= 300 neurons, with a task-supplied phase-locked input (new `spike_generator`-based input kind). Do not attempt it at 0.1 ms.

### NEST fit

| Need | NEST | Note |
|---|---|---|
| NL coincidence cell | `iaf_psc_exp` / `iaf_cond_exp` with short tau_m (~0.2-0.5 ms) and short synaptic tau (~0.1-0.2 ms), threshold above one EPSP | Coincidence window ~ the tau. Real NL has very fast Kv1/low-threshold currents `[M]`; point neuron with small tau_m approximates. |
| Delay lines | Per-edge `delay` | See table above. |
| Phase-locked NM | `sinusoidal_poisson_generator` or `spike_generator` | Not in circuit JSON today. |
| NM relay | `parrot_neuron` | Used elsewhere in the engine. |
| Gain control by SON inhibition | `static_synapse` negative weight | Optional. |

### Gaps

- Input kind: no phase-locked generator in the circuit format (above).
- Real NL has dendritic length-dependent structure and ITD tuning sharpening by Kv1 currents: no NEST point-neuron equivalent without `cm_default`.
- Inhibition for microsecond ITD tuning is shown for the **gerbil MSO**, not the owl NL `[V]` Brand 2002; do not claim it for the owl.

### Diagnosis lesions

| Lesion | Observable symptom |
|---|---|
| One side's NM->NL delay gradient flattened (all delays equal) | All NL cells share one best ITD; map collapses; localisation maps to a single azimuth |
| Contralateral input silenced | NL spike count drops to monaural level; tuning curve flat; no ITD dependence `[V]` (monaural level) |
| NL threshold too low (coincidence window widened, tau_m too long) | Tuning curve broadens; peak:trough ratio < 1.5; phase ambiguity worsens |

---

## 4. Zebra finch: HVC(RA) sparse sequence / synfire chain

### Cells and roles

| Cell | Role | Count (game) | Count (animal) |
|---|---|---|---|
| HVC(RA) projection neurons | Burst once, at a single precise time in the song motif `[V]` Hahnloser 2002 | 100-500 | tens of thousands per hemisphere `[M]` |
| HVC(X) neurons | Project to Area X; also sparse; not needed for the core chain | 0-50 | |
| HVC interneurons | Fire densely during song; inhibit projection neurons | 20-50 | ~10-20% `[M]` |
| RA | Target; stereotyped sequences of bursts driven by HVC `[V]` | 20-50 | |
| RA to brainstem motor pool | Syringeal / respiratory output | outside | |

### Connectivity

| Pre | Post | Sign | Notes |
|---|---|---|---|
| HVC(RA) group g | HVC(RA) group g+1 | + | Synaptic chain: feed-forward, groups of ~5-20 cells `[V]` Long 2010: large rapid depolarization 5-10 ms before burst onset |
| HVC interneuron | HVC(RA) | - | Shapes burst timing and prevents runaway; population inhibition |
| HVC(RA) | HVC interneuron | + | |
| HVC(RA) | RA | + | Each RA cell is driven by the HVC groups active at that time `[V]` |

### Scorable behaviour

| Metric | Value | Tag |
|---|---|---|
| Burst number per HVC(RA) cell per motif | One `[V]` | `[V]` |
| Burst timing precision across renditions | Sub-millisecond to ~1 ms jitter | `[M]` |
| Burst shape | ~ 4-6 spikes in ~ 5-10 ms | `[M]`; `[V]` for ~10 ms underlying depolarisation (Long 2010) |
| Chain propagation | 5-10 ms from onset of depolarization to burst `[V]`; group-to-group step ~ 5-10 ms | `[V]` |
| Motif length | ~ 0.5-1 s | `[M]` |
| Game score ideas | (a) number of distinct groups activated in order (sequence length); (b) burst jitter SD across 10 trials; (c) every cell fires once (sparseness); (d) chain completes under 20% noise (robustness); (e) ordered ranking of first-spike times (Kendall tau = 1) | game design |

### Minimum circuit

K = 20-40 groups x 5 `aeif_cond_alpha` cells in a feedforward chain (each cell in group g gets from every cell in group g-1, weight chosen so that ~3 of 5 coincident inputs fire it), 5-10 global inhibitory interneurons (all-to-all from groups, weight small), a starter input (a brief Poisson/dc pulse onto group 0). Group-to-group delay 2-5 ms yields a total chain time of K x 3 ms (a 60-120 ms mini-motif). Optional: one RA readout per few groups.

### NEST fit

| Need | NEST | Note |
|---|---|---|
| Chain cell with burst-like response | `aeif_cond_alpha` with a depolarising plateau (large `b`, `a` small) or `izhikevich` bursting preset | Calcium plateau ~10 ms `[V]` Long 2010: model it by a slow synapse (alpha tau ~ 5 ms) or a short-lived I_e. |
| Synfire connectivity | `all_to_all` between consecutive populations, or `fixed_indegree` | Supported in format. |
| Timing | Per-edge `delay` 1-5 ms; resolution 0.1 ms is fine | |
| Sequence start | `dc` pulse or Poisson burst via task | |
| Learning the chain | `stdp_synapse` | Already allowed (excitatory only). |

### Gaps

- **Dendritic Ca spike** that propagates the chain with high precision `[V]`: needs a compartment model (`cm_default`, `iaf_cond_alpha_mc`), not in the format.
- Competing models (subthreshold-timer / "ramping") are not distinguishable in NEST without structured noise; the Bestiary can only represent the chain model.
- Realistic scale (~10^4 cells) is too big for the game; the chain at 100-500 cells is a caricature.

### Diagnosis lesions

| Lesion | Observable symptom |
|---|---|
| One group's outgoing weights zeroed (broken chain link) | Sequence halts at that group; downstream groups silent; RA sequence truncated at a fixed time |
| Inhibitory interneurons removed | Chain activation spreads (several groups active at once), bursts per cell > 1, sequence smears |
| Synaptic weights too strong (above critical) / too weak | Weak: chain dies after a few groups; strong: whole chain fires at once (no temporal order) |

---

## 5a. Cat: spinal locomotor CPG (Graham Brown half-centre, Ia / Ib / Renshaw)

Existing repo circuits already cover the motifs: `m_half_centre.md`, `renshaw.md`, `stretch_reflex.md`, `reflex.md`. This sheet adds what the Bestiary chapter needs on top.

### Cells and roles

| Cell | Role | Count (game) |
|---|---|---|
| Flexor and extensor rhythm-generator (RG) interneurons | Half-centres: reciprocal inhibition + adaptation or persistent Na gives alternation `[V]` Rybak 2006 (RG level) | 2-5 per side |
| Pattern-formation (PF) interneurons | Second level; shape motoneuron drive, can fail ("deletion") without resetting the RG phase `[V]` Rybak 2006; McCrea & Rybak 2008 | 2-5 per side |
| Flexor and extensor motoneurons | Output | 8-16 per pool |
| Ia inhibitory interneurons | Reciprocal inhibition of the antagonist motoneurons | 2 per side |
| Ib interneurons | Golgi-tendon-organ afferent (load) feedback; in locomotion gate extensor phase (stance to swing) `[M]` | 2 per side |
| Renshaw cells | Recurrent inhibition of own motoneurons and Ia interneurons | 3 per pool |
| Ia / Ib afferents | Sensory feedback; adjust phase durations | 5 each |

### Connectivity

| Pre | Post | Sign |
|---|---|---|
| RG_F <-> RG_E | each other | - (reciprocal) |
| RG_F / RG_E | PF_F / PF_E | + |
| PF | motoneurons (same pool) | + |
| Motoneuron | Renshaw (same pool) | + (collateral) |
| Renshaw | same pool MN, Ia interneuron, antagonist Renshaw | - |
| Ia afferent | agonist MN | +; Ia interneuron -> antagonist MN -; |
| Ib afferent | Ib interneuron -> agonist MN (-), + to the half-centre of the antagonist (swing trigger) `[M]` | |

### Scorable behaviour

| Metric | Value | Tag |
|---|---|---|
| Alternation | Flexor/extensor bursts in antiphase | `[V]` |
| Step period | ~ 0.5-1.5 s across speeds (decerebrate cat) | `[M]` |
| Speed control | Period change is mostly in the extensor phase; flexor phase nearly constant | `[M]` |
| Period stability under deletion | Period and phase are maintained across spontaneous deletions of motoneuron activity | `[V]` Rybak 2006; McCrea & Rybak 2008 |
| Game score ideas | (a) phase-lag flexor vs extensor = 0.5 +- 0.05 cycles; (b) CV of period < 10%; (c) period tunable 3:1 by tonic drive; (d) recovers after a 1-cycle motoneuron silencing without phase shift | game design |

### Minimum circuit

Two cells (`aeif_cond_alpha`, `a` ~ 4 nS, `b` ~ 80 pA) with reciprocal inhibition (-30 nS, 2 ms) and tonic dc drive: this is exactly `m_half_centre`. Add PF and MN layers (one cell each side) to demonstrate deletions; add Ia/Ib/Renshaw via `renshaw.md`.

### NEST fit

| Need | NEST |
|---|---|
| Half-centre with escape/release | `aeif_cond_alpha` (adaptation `a`, `b`) or `hh_cond_exp_traub` |
| Persistent sodium rhythm | `butera_burster` (NESTML, if compiled) for the respiratory analogue |
| Reciprocal inhibition, delays | `static_synapse` negative weight, delay 1-3 ms |
| Sensory feedback | Task-computed Ia/Ib rates via `poisson` inputs; body model supplies the load |

### Gaps

- No NEST Golgi tendon organ or spindle model (provided by the game body, not by NEST).
- Mixed flexor/extensor "unit burst generators" and multi-joint coordination: no template; build with populations.
- Speed-dependent gait transitions need modulated drive, not covered by NEST.

### Diagnosis lesions

| Lesion | Observable symptom |
|---|---|
| Reciprocal inhibition cut on one side | Both half-centres fire together (co-contraction) or one side dominates; no alternation |
| Renshaw cells removed | Motoneuron rates and tremor increase (already in `renshaw` task); bursts longer; phase less clean |
| Ib (load) feedback cut | Swing fails to start at end of stance, or period jitter rises; the step does not adapt to load |

---

## 5b. Cat: V1 simple-cell orientation selectivity

### Cells and roles

| Cell | Role | Count (game) |
|---|---|---|
| LGN ON and OFF centre-surround relay cells | Thalamic input; their receptive fields are arranged in a line along the preferred orientation (Hubel & Wiesel 1962) | 8-16 ON + 8-16 OFF |
| Simple cell (layer 4) | Orientation-selective, ON and OFF subregions | 1-10 |
| Inhibitory interneurons (push-pull) | ON-inhibition in OFF subregions and vice versa; gives contrast invariance `[V]` Ferster & Miller 2000 | 4-10 |
| Recurrent excitatory pool (for recurrent models) | Sharpens tuning via cortical amplification | 20-100 |

### Two competing explanations

| Model | Mechanism | Predicts |
|---|---|---|
| Feedforward (Hubel & Wiesel) | Thalamic inputs aligned along a line; excitation sums | Tuning broad; width changes with contrast unless push-pull inhibition is added `[V]` |
| Feedforward + push-pull inhibition | As above plus inhibition in anti-phase | Contrast-invariant orientation tuning `[V]` |
| Recurrent / feedback (ring models) | Weak thalamic bias amplified by recurrent excitation + broad inhibition | Sharp tuning, orientation-selective even for weakly tuned input; response time course and contrast invariance from network gain `[V]` (discussed in Ferster & Miller 2000) |
| Normalisation | Division by pooled cortical activity | Contrast invariance |

### Scorable behaviour

| Metric | Value | Tag |
|---|---|---|
| Preferred-orientation selectivity | Tuning half-width at half-height ~ 15-30 deg | `[M]` |
| Orientation selectivity index (OSI) | 0.3-0.8 | `[M]` |
| Contrast invariance | Tuning width roughly constant over contrast 10-100% | `[V]` (as the property the models must explain) |
| Feedforward alone fails | Thalamic arrangement "cannot explain the invariance of orientation tuning to changes in stimulus contrast" | `[V]` Ferster & Miller 2000 |
| Game score ideas | (a) OSI > 0.5; (b) half-width at 20% vs 80% contrast within 25%; (c) response at orthogonal orientation < 20% of preferred | game design |

### Minimum circuit

Feedforward: 6 ON + 6 OFF LGN `parrot_neuron`/`iaf_psc_alpha` cells whose "receptive fields" are indexed in a 1-D strip; stimulus orientation theta gives each LGN cell a Poisson rate = baseline + gain * overlap(theta); all excitatory to 1 simple cell; threshold gives tuning. Add 2-4 push-pull inhibitory cells (OFF-driven inhibition of ON-driven zones) for contrast invariance. Recurrent variant: a ring of 16-32 E cells (orientation index) with Mexican-hat coupling plus inhibition.

### NEST fit

| Need | NEST |
|---|---|
| LGN drive by orientation | `poisson` inputs with task-computed rates (supported); a drifting grating needs time-varying rates, which the task sets each slice |
| Simple cell, interneurons | `iaf_cond_alpha` / `aeif_cond_alpha` |
| Ring recurrent | `iaf_cond_alpha` ring with `fixed_indegree` plus distance-dependent weights via `edges` |
| Contrast gain | Rate scale of the Poisson inputs |
| Push-pull | Negative-weight static synapses |

### Gaps

- No spatial receptive-field filtering (Gabor) in NEST: the task must precompute LGN rates from the stimulus.
- No pinwheel / map geometry; no cortical columns.
- Subtle: distinguishing feedforward from recurrent needs a task with time-resolved response and contrast ramps; the score cannot rely on tuning width alone.

### Diagnosis lesions

| Lesion | Observable symptom |
|---|---|
| Push-pull (anti-phase) inhibition removed | Orientation tuning width grows with contrast; contrast-invariance index falls `[V]` (Ferster & Miller) |
| Misaligned LGN subunits (one ON subunit off the line) | Preferred orientation shifts; OSI drops |
| Recurrent excitation too strong | Tuning too sharp at high contrast but untuned input amplified: unselective bursting; or persistent firing after the stimulus is removed |

---

## 6. Rat: place cells, theta phase precession, CA1 theta (minimal)

### Cells and roles

| Cell | Role | Count (game) |
|---|---|---|
| Medial-septum pacemaker / theta drive | Rhythmic GABAergic/cholinergic input at 7-12 Hz `[V]` O'Keefe & Recce 1993 (theta 7-12 Hz) | 1 source |
| CA1 pyramidal place cell | Fires in a place field; spike phase precesses | 10-30 |
| CA1 interneuron (basket) | Peak firing ~60 deg before the pyramidal peak `[V]` Skaggs 1996 | 2-5 |
| Entorhinal / CA3 input | Ramping, spatially tuned excitation | task-driven |

### Scorable behaviour

| Metric | Value | Tag |
|---|---|---|
| Theta frequency | 7-12 Hz | `[V]` |
| Phase precession total | 100-355 deg through the field (cell-dependent) | `[V]` O'Keefe & Recce 1993 |
| Mechanism in data | Cell's intrinsic burst frequency exceeds LFP theta | `[V]` O'Keefe & Recce |
| Entry phase | First spikes 90-120 deg after peak population phase, near the inhibition minimum | `[V]` Skaggs 1996 |
| Shape | Phase advance typically accelerating, not linear | `[V]` Skaggs 1996 |
| Population compression | Place-field sequences replayed within a theta cycle at up to 10:1 | `[V]` Skaggs 1996 |
| Population modulation depth | ~50% for CA1 pyramidal population | `[V]` Skaggs 1996 |
| Game score ideas | (a) circular-linear regression slope of spike phase vs position (cycles per field, target -0.5 to -1); (b) correlation coefficient; (c) theta-locking of interneurons (vector strength); (d) in-field vs out-of-field rate ratio > 5 | game design |

### Minimum circuit (dual oscillator form)

One CA1 pyramidal cell (`iaf_cond_alpha`) receives (i) a ramping spatial excitation (rate grows then falls as the rat crosses the field: task-set Poisson rate or `dc`) and (ii) a **rhythmic inhibitory/oscillating drive at theta** (shared with an interneuron). Because the ramping excitation grows through the field and the cell's intrinsic rhythm is slightly faster than theta, the first spike occurs late in the theta cycle and moves earlier as the ramp grows. Add one interneuron driven by the same theta, projecting to the pyramidal cell (Skaggs: interneuron peak ~60 deg earlier than the pyramidal peak).

### NEST fit

| Need | NEST | Circuit-JSON? |
|---|---|---|
| Theta drive | `sinusoidal_poisson_generator` (7-10 Hz, `ac` <= `dc`) or `ac_generator` into the cell (pA) | **No**: only `dc` / `poisson` inputs. Workaround: task sets the Poisson `rate` each 1 ms slice to rate(t) = r0(1 + m cos(2 pi f t)); resolution 0.1 ms is fine. |
| Precise theta timing | `spike_generator` (a 10 Hz train, one spike per cycle) -> parrot -> inhibitory synapse | Not in JSON; as above |
| Ramp spatial drive | Task-set `dc` or Poisson rate per slice | Yes |
| Place cell | `iaf_cond_alpha`, `aeif_cond_alpha` | Yes |
| Resolution | 0.1 ms; theta period 100-140 ms | Cheap |

### Gaps

- NEST has **no native phase-precession mechanism**: it must be built from two oscillators / ramp-plus-oscillation, with the slope set by the frequency difference. It will not emerge from a single point neuron without an input ramp.
- Dendritic computations in CA1 (the dendritic-oscillation models) need compartments (`cm_default`).
- Full CA3-CA1 theta generation (septum, interneurons, I_h) is beyond "minimal"; Buzsaki 2002 describes the sources `[V]` (entorhinal input, CA3 Schaffer collaterals, Ca currents in dendrites).

### Diagnosis lesions

| Lesion | Observable symptom |
|---|---|
| Theta drive removed (septal lesion) | No rhythmic firing; spikes uncorrelated with a reference rhythm; place field rate may remain; phase slope undefined |
| Ramp excitation flattened (no spatial tuning) | No place field; rate flat across position; phase constant |
| Interneuron / inhibition removed | Pyramidal firing no longer restricted to one theta phase band; weak modulation depth; precession range wider or lost |

---

## Citations (verified with PubMed MCP tools, 2026-11-18)

"Verified" = PMID, title, authors, journal, year, volume/pages and DOI read from the PubMed record via `get_article_metadata`.

| # | Circuit | Citation | PMID | DOI | Status |
|---|---|---|---|---|---|
| 1 | Locust | Gabbiani F, Krapp HG, Laurent G (1999). Computation of object approach by a wide-field, motion-sensitive neuron. J Neurosci 19:1122-41 | 9920674 | 10.1523/JNEUROSCI.19-03-01122.1999 | verified |
| 2 | Locust | Gabbiani F, Krapp HG, Koch C, Laurent G (2002). Multiplicative computation in a visual neuron sensitive to looming. Nature 420:320-4 | 12447440 | 10.1038/nature01190 | verified |
| 3 | Locust | Rind FC, Simmons PJ (1992). Orthopteran DCMD neuron: a reevaluation of responses to moving objects. I. J Neurophysiol 68:1654-66 | 1479436 | 10.1152/jn.1992.68.5.1654 | verified |
| 4 | Locust | Rind FC, Bramwell DI (1996). Neural network based on the input organization of an identified neuron signaling impending collision. J Neurophysiol 75:967-85 | 8867110 | 10.1152/jn.1996.75.3.967 | verified |
| 5 | Locust | Fotowat H, Gabbiani F (2011). Collision detection as a model for sensory-motor integration. Annu Rev Neurosci 34:1-19 | 21391815 | 10.1146/annurev-neuro-061010-113632 | verified (review) |
| 6 | Zebrafish | Korn H, Faber DS (2005). The Mauthner cell half a century later: a neurobiological model for decision-making? Neuron 47:13-28 | 15996545 | 10.1016/j.neuron.2005.05.019 | verified |
| 7 | Zebrafish | Takahashi M, Narushima M, Oda Y (2002). In vivo imaging of functional inhibitory networks on the Mauthner cell of larval zebrafish. J Neurosci 22:3929-38 | 12019312 | 10.1523/JNEUROSCI.22-10-03929.2002 | verified |
| 8 | Zebrafish | Kohashi T, Nakata N, Oda Y (2012). Effective sensory modality activating an escape triggering neuron switches during early development in zebrafish. J Neurosci 32:5810-20 | 22539843 | 10.1523/JNEUROSCI.6169-11.2012 | verified |
| 9 | Zebrafish | Kohashi T, Oda Y (2008). Initiation of Mauthner- or non-Mauthner-mediated fast escape evoked by different modes of sensory input. J Neurosci 28:10641-53 | 18923040 | 10.1523/JNEUROSCI.1435-08.2008 | verified |
| 10 | Zebrafish | Marsden KC, Granato M (2015). In vivo Ca2+ imaging reveals that decreased dendritic excitability drives startle habituation. Cell Rep 13:1733-40 | 26655893 | 10.1016/j.celrep.2015.10.060 | verified |
| 11 | Zebrafish | Gahtan E, Sankrithi N, Campos JB, O'Malley DM (2002). Evidence for a widespread brain stem escape network in larval zebrafish. J Neurophysiol 87:608-14 | 11784774 | 10.1152/jn.00596.2001 | verified |
| 12 | Owl | Carr CE, Konishi M (1990). A circuit for detection of interaural time differences in the brain stem of the barn owl. J Neurosci 10:3227-46 | 2213141 | 10.1523/JNEUROSCI.10-10-03227.1990 | verified |
| 13 | Owl (contrast: gerbil MSO) | Brand A, Behrend O, Marquardt T, McAlpine D, Grothe B (2002). Precise inhibition is essential for microsecond interaural time difference coding. Nature 417:543-7 | 12037566 | 10.1038/417543a | verified; **gerbil, not owl** |
| 14 | Finch | Hahnloser RHR, Kozhevnikov AA, Fee MS (2002). An ultra-sparse code underlies the generation of neural sequences in a songbird. Nature 419:65-70 | 12214232 | 10.1038/nature00974 | verified |
| 15 | Finch | Long MA, Jin DZ, Fee MS (2010). Support for a synaptic chain model of neuronal sequence generation. Nature 468:394-9 | 20972420 | 10.1038/nature09514 | verified |
| 16 | Cat CPG | Rybak IA, Shevtsova NA, Lafreniere-Roula M, McCrea DA (2006). Modelling spinal circuitry involved in locomotor pattern generation: insights from deletions during fictive locomotion. J Physiol 577:617-39 | 17008376 | 10.1113/jphysiol.2006.118703 | verified |
| 17 | Cat CPG | McCrea DA, Rybak IA (2008). Organization of mammalian locomotor rhythm and pattern generation. Brain Res Rev 57:134-46 | 17936363 | 10.1016/j.brainresrev.2007.08.006 | verified |
| 18 | Cat V1 | Hubel DH, Wiesel TN (1962). Receptive fields, binocular interaction and functional architecture in the cat's visual cortex. J Physiol 160:106-54 | 14449617 | 10.1113/jphysiol.1962.sp006837 | verified (no abstract in PubMed) |
| 19 | Cat V1 | Ferster D, Miller KD (2000). Neural mechanisms of orientation selectivity in the visual cortex. Annu Rev Neurosci 23:441-71 | 10845071 | 10.1146/annurev.neuro.23.1.441 | verified |
| 20 | Rat | O'Keefe J, Recce ML (1993). Phase relationship between hippocampal place units and the EEG theta rhythm. Hippocampus 3:317-30 | 8353611 | 10.1002/hipo.450030307 | verified |
| 21 | Rat | Skaggs WE, McNaughton BL, Wilson MA, Barnes CA (1996). Theta phase precession in hippocampal neuronal populations and the compression of temporal sequences. Hippocampus 6:149-72 | 8797016 | 10.1002/(SICI)1098-1063(1996)6:2<149::AID-HIPO6>3.0.CO;2-K | verified |
| 22 | Rat | Buzsaki G (2002). Theta oscillations in the hippocampus. Neuron 33:325-40 | 11832222 | 10.1016/s0896-6273(02)00586-x | verified (review) |

### Unverified or not found (do not cite without a check)

| Item | Status |
|---|---|
| Koyama M et al. (2016) eLife, "A circuit motif in the zebrafish hindbrain for a two alternative behavioral choice to turn left or right" | **Not found** in PubMed through the MCP tools (citation lookup returned no PMID; title searches returned nothing). It is probably the best source for the commissural left/right choice, but no PMID or DOI is given here. Verify manually (eLife is also in PubMed Central) before use. The commissural-inhibition claims above are cited to Takahashi 2002 and Korn & Faber 2005 instead. |
| Jeffress LA (1948) "A place theory of sound localization", J Comp Physiol Psychol | Not in PubMed (psychology journal). Cited by name only, no ID. |
| Graham Brown TG (1911/1914) half-centre papers | Pre-PubMed or not indexed. Cited by name only. |
| Every `[M]` number in this file | From memory, not checked against a source in this session. Includes: owl ITD range and acuity, NM vector strength, HVC burst jitter, motif length, cell counts, cat step period, V1 half-widths and OSI, M-cell club-ending rectification. |

## Cross-cutting gap summary (for the engine owner)

| Gap | Affects | Fix |
|---|---|---|
| No `spike_generator` / `sinusoidal_poisson_generator` / `ac_generator` input kind | Owl (essential), rat theta, finch start pulse | Add input kinds in `circuit.cpp` + `engine.cpp`; or accept task-driven Poisson rate per slice (adequate for theta, **not** for owl) |
| Owl needs 1 us grid and phase-locked inputs | Owl | `resolution_ms = 0.001`, <= 300 neurons, short trials; 0.1 ms is unusable |
| No compartmental/dendritic models in the circuit format | LGMD multiplication, HVC Ca plateau, NL Kv1 sharpening, CA1 dendrites | Expose `cm_default` or accept point-neuron approximations |
| Gap junctions are ohmic, symmetric, slow (waveform relaxation) | Mauthner | Mixed proxy (chemical + small gap-junction count) |
| No Gabor/receptive-field front end | V1 | Task precomputes LGN rates |
| No phase-precession primitive | Rat | Ramp + theta two-input recipe |
