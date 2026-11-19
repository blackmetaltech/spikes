# Changelog

## [Unreleased] - 0.6 Theory wing, chapter 3: attractor memory

- Three levels: `h_complete` (a partial cue recalls the whole pattern), `h_two` (two patterns, no blended state), `h_learn` (the pattern is written by `stdp_synapse` during an exposure phase and recalled after, with a check that the weights really grew and that nothing was recalled before).
- Lab: a pattern view (cells lit by their own spikes, the cued ones ringed), a plastic-synapse editor (learning rate, weakening, ceiling) in `h_learn`.
- `Scripted` (task scaffold) gained `tick` and named neuron sets (`Meas::sets`, `active_in`) for levels that need to know which cells a sensor reaches.

## [Unreleased] - 0.6 Theory wing, chapter 2: rate models and mean field

- Three levels: `r_oscillate` (Wilson-Cowan pair), `r_choose` (winner take all with a held choice), `r_meanfield` (a spiking E-I network and its siegert mean-field prediction, scored on agreement at three drive levels).
- Rate units in the lab: parts-bin cards (`threshold_lin_rate_ipn`, `tanh_rate_ipn`, `siegert_neuron`, `iaf_psc_delta`), rate connections with no delay, a mean-field projection with J and K, rate-readout channels, a rate-strip raster instead of spikes for a circuit of rate units, a rates chart as the level view.
- Catalog probe for rate cards (steady rate against drive); `test_catalog` knows them. Lab: random sparse wiring (fixed in-degree up to 100) in the rates and memory chapters.

## [Unreleased] - 0.6 Theory wing, chapter 1: dynamics

- New campaign wing "Theory" (third tab row, open from the start): `docs/DESIGN-THEORY.md`. First chapter, dynamics: `d_class` (class I and II, f-I onset), `d_phasic` (a stable fixed point that ends firing), `d_switch` (one neuron, two stable states, set and reset pulses).
- New lab view: the V-w phase plane (nullclines from the cell's live parameters and current, fixed points, reset, live trajectory).
- Engine, add-only: `Engine::neuron_state`, `input_weight`, `input_value`, `output_value`; input kinds `rate_drive`, `tuned_poisson`; output kind `rate_value`; rate neurons and `siegert_neuron` as populations with `rate_connection_*` and `diffusion_connection` (J, K) projections; `spike/theory.h` (phase plane, onset type, spike-triggered average); `spike-run --sample/--set/--phase`. See `docs/CIRCUIT-FORMAT.md`.
- New citations (checked against PubMed): Ermentrout 1996, Rothman & Manis 2003, Touboul & Brette 2008, Naud 2008 and those of the later chapters.

## [Unreleased] - 0.5 playtest fixes (research/PLAYTEST-0.5.md)

- **Reference lock:** a circuit counts as a copy of the reference only when it equals the reference file exactly (structure, editor positions, provenance, every number within a relative 1e-6) or the REFERENCE button filled the bench; hand-built circuits with round numbers are never flagged.
- **Structure checks:** `w_latch` and `worm_touch` accept a self-excitation loop (autapse); `worm` accepts turning one way (the other turn channel may stay unconnected), in the engine and in the lab's wiring check. Test: `tasks_fixes_test`.
- **CHECK** runs the quick episode (`score_headless_quick_async`); the spinal boss has a compressed quick schedule (all three reaches, a push in each hold; about 5 s), the autonomic boss's quick day is about 5 s, worm and lamprey under 1 s.
- **No numbers in verdicts and hints (tiers 1-3)** for every task, pass or fail, including the "(The run itself: ...)" suffixes; the digit scan in `tasks_cheap_test` covers every task (names of the task's own channels and the formula CO2 are not numbers). Wiring messages name a numbered channel family once.
- **Verdicts follow the parts:** `l_burst`, `lamprey`, `a_facilitate`, `aplysia_gill`, `l_rebound`.
- **Gauges** are all fed (`tasks_gauges_test`); a missing readout shows a dash, not 0.0 in green. Sentinel -1 shows a dash.
- **Lab/campaign:** two-row chapter tabs (Body / Bestiary) and a scrolling strip on a phone; pass rule in the visible part of the brief; slider caps from the level's reference (twice the largest weight) and typed values; a verdict that asks for more strength is not shown at a slider's cap; toasts queue (one at a time, none over the result modal); hints open after two different missed circuits; "rhythm by accident" only for circuits with a feedback loop and a task without a periodic stimulus; serotonin label on the Aplysia modulator dial; diagnosis: Reveal answer after two wrong picks, a generic "no diagnosis" verdict, `dx_aplysia` findings no longer name the answer, `dx_leech` patient robust on every seed; title shows the project version (0.5.0-dev).
- **Difficulty:** `m_half_centre` default drive 400 fires the plain adaptive cell; the Aplysia lab defaults (U 0.1, gain 3, scale 4) are in the workable range; `l_rebound` brief names the steady drive.

## [Unreleased] - 0.5 Bestiary, step 3: fly compass, fly mushroom body, crab

- Thirteen levels in three more chapters of the Bestiary wing: fly compass (`c_ring`, `c_shift`, `fly_compass`, `dx_compass`), fly mushroom body (`k_sparse`, `k_depress`, `fly_mb`, `dx_mb_apl`, `dx_mb_dan`), crab (`s_pacemaker`, `s_phase`, `crab_pyloric`, `dx_crab`). Notes: `docs/circuits/bestiary_b.md`.
- New NESTML cell `stg_cell` (Na, Kd, CaT, CaS, A, KCa, H, leak and calcium; after the Liu/Prinz model family, game conductances), with a parts-bin card.
- Engine, add-only: `ProjectionInfo::dopamine_source`, `Engine::output_spiked`. Lab: ring readout output kind, ring-closing segment wiring, random sparse wiring, dopamine-gated synapse with source and learning, compass dial.
- The compass is wired after Turner-Evans/Hulse cell types, not synapse-level data; the mushroom body after the published cell types; STG kinetics are from memory of the published forms and the parameter sets are not the published ones (`docs/WHAT-IS-REAL.md`).
- Scripted levels: a half-wired circuit (a sensor or motor not connected) no longer passes.

## [Unreleased] - 0.5 Bestiary, step 2: worm, leech, sea slug

- Twelve levels in three new campaign chapters (wing "Bestiary", open from the start, never gating the body chapters): worm (`w_latch`, `w_switch`, `worm_touch`, `dx_worm`), leech (`l_burst`, `l_rebound`, `leech_heart`, `dx_leech`), aplysia (`a_depress`, `a_facilitate`, `aplysia_gill`, `dx_aplysia`). Design: `docs/DESIGN-0.5.md`; notes: `docs/circuits/bestiary_a.md`.
- `worm_touch` is wired from the real touch circuit (Cook et al. 2019 synapse counts through the OpenWorm ConnectomeToolbox `cect`, `tools/connectome/worm.py`).
- Engine, add-only: `Structure::modulation` and `ChannelStructure::modulator`. Lab: the Aplysia levels offer depressing synapses and a modulation row on a synapse.
- What is real: worm and leech cells are spiking stand-ins for graded cells; the leech cycle is time-compressed.
- Known gaps: no plate art for the Bestiary map; deleting a connection in the lab does not renumber a synapse modulation row; `tools/citations/check.py` still reports the unverified references of the research sheets `research/bestiary-*.md`.

## [0.4.0] - 2026-11-18 — Honest puzzles, shippable game

0.4 answers the harsh review (`docs/REVIEW-HARSH.md`, requirements in `docs/REQ-0.4.md`): every level now checks the mechanism it teaches, and the game builds, exports and runs CHECK from a zip.

### Added
- **Mechanism checks on all 41 levels.** Tasks read the circuit structure (`Engine::structure()`, `has_path`, `has_loop`, `has_loop_via`) and the verdict names the missing part, e.g. no coupling between pacemaker cells, or a baroreflex without both limbs.
- **Cheap circuits that must fail:** empty bench, single cell, zero weights, reference without the mechanism, max every weight, and the review's cheap circuits, for every level (`tasks_cheap_test_{a,b,c}`). Diagnosis cases test that every wrong and unknown answer fails.
- **Recipes kept apart.** Verdicts and hint tiers 1-3 carry no numbers (a test scans them); numbers live only in `recipe()`.
- **Faster CHECK:** seeds run in parallel processes and most tasks have a shorter quick episode (`Task::set_quick`).
- **Cajal Plate identity** (`docs/IDENTITY.md`): paper and ink, Golgi sienna and Prussian blue, Fraunces / Atkinson Hyperlegible / IBM Plex Mono, atlas-plate maps. No Godot splash.
- **UI honesty:** reference hidden until passed (or Guided), reference lock is a structural fingerprint, histograms labelled as generated variants, an in-game "What is real" page, progressive disclosure in the lab.
- **Citations** checked against PubMed / Crossref with DOIs (`catalog/citations.json`, `tools/citations/check.py`); wrong ones fixed.
- **Shipping:** levels and catalogue inside the Godot project, Linux export preset, `spike-run` bundled, static GSL, `scripts/build-all.sh`, `scripts/package.sh`, Dockerfile and CI workflow.

### Fixed
- NEST aborted on huge conductances. Weights are now limited at load (500 nS / 10000 pA) and a failed step latches a fault instead of crashing.

### Known gaps
- A brief-only scripted novice cannot solve posture and reach; the spinal boss and seven heart/autonomic levels (which need bias currents) have no novice test yet.
- CHECK is over the 3 s target for breathing (6.8 s), peristalsis (4.5 s), baroreflex (3-4 s), reach (3.2 s) and renshaw (about 3 s).
- The Dockerfile and CI workflow have never run; no Windows build; the Linux build needs glibc >= 2.39.
- 34 citations (books, older papers) are unverified; the online checker mode is untested.
- m_reciprocal and m_wave are nearly trivial. Some heart-map leader lines cross the drawing; population labels overlap on dense circuits.

## [0.3.0] - 2026-11-17 — Learning curve

0.3 turns the campaign into a learning ladder (design in `docs/DESIGN-0.3.md`): one new idea per level, a hint for when you are stuck, and a reason to come back after three stars.

### Added
- **19 micro-puzzles.** Each one isolates a single concept, from threshold, excitation and convergence to divergent command. They use 1-4 neurons and run for 3-5 s, and they come just before the task that needs that concept. A scripted beginner, using the lab's own default cell and weights, passes each one in 1-5 runs.
- **Two boss levels.** The spinal sprint combines posture, pushes, reaches and Renshaw cells. The autonomic exam is a whole day (rest, standing, exercise, a threat, recovery) with heart, blood pressure, pupil and breathing all scored.
- **Six diagnosis cases:** clonus, vagotomy, orthostatic hypotension, CCHS, Horner's syndrome and achalasia. You get the patient's circuit with a hidden lesion and its symptoms. You probe it with the scopes and the lesion lab, pick the lesion, and get a "why not" explanation for every wrong guess.
- **Difficulty modes.** Modes change tolerance, noise and perturbation strength, never the circuit's structure.
  - **Guided:** the recipe is shown in the brief, and hints are free.
  - **Standard:** SHOW RECIPE caps that level at 2 stars, and hints unlock after 2 failed runs.
  - **Researcher:** no recipe, no hints, and a pass only counts from TEST x5.
- **Hint ladder.** Tier 1 names the concept, tier 2 outlines the faulty part of your circuit, tier 3 gives a partial recipe. After a failed run, the guilty element glows.
- **Challenge variants.** There are two per task, for example half the neurons, double noise, a heavier load, a stronger haemorrhage or a fainter threat.
- **Post-solve histograms** of neurons, energy and quality, with your own target pinned on top. They are seeded from generated passing variants (see `docs/HISTOGRAMS.md`), not from other players.
- **Lab notebook.** Every pass unlocks a true, cited finding, and a concept glossary with citations sits alongside.
- **Sound.**
  - Spike clicks like a real electrophysiology monitor (warm for excitatory cells, cool for inhibitory ones, rate-limited).
  - Heartbeat, breath, muscle twitches, gut sounds and UI sounds.
  - Four volume sliders.
- **Feel.** A first-pass celebration, surprise toasts, instant CHECK (headless scoring in a few seconds), skip to the next event, and a default speed set from the measured throughput.
- **Campaign.**
  - The map shows a main path plus branches.
  - A chapter opens when the previous chapter's main path, or the one two before it, is passed.
  - Concept chips show what each level introduces and what it uses.
  - Opt-in local play log, exportable as JSON.
- **Engine.**
  - Seeded per-connection weight spread (`weight_cv`, `weight_sd`) on any projection, with PyNEST parity.
  - Async headless scoring with seed pinning and cancel.
  - Per-task default sensor weights, default cell type and recipe.
  - New task contract: level kind, concepts, modes, variants, hints, metrics, pacing, diagnosis and notebook.

### Changed
- Every level opens on the cell type its puzzle and verdicts assume. Briefs explain the concept only; the numbers live in the recipe.
- **Reflex:** the task now needs convergence, so a single cell cannot pass it.
- **Renshaw:** it measures your own circuit's response to input volleys, so removing the Renshaw cells fails.
- **Posture:** the verdict reads which way the limb is off.
- **Big tasks:** they list the concepts their micro-puzzles taught as used, not new.
- **Adapting preset:** retuned so it actually adapts (tau_w 500 ms).

### Known gaps
- **Reach is a branch, not on the main path.** No beginner circuit has passed it reliably yet; the best is 3 of 6 seeds.
- **Histograms are generated, not real players.** The slow levels (bosses, fight-or-flight, autonomic heart rate) have only 11-19 samples.
- **Renshaw loophole:** a circuit that lowers tremor by another route through the noise path could still pass.
- **Unverified glossary citations:** several were written from memory and still need a physiologist's check.
- **Untested on hardware:** surprise toasts have not fired in testing, and the new sounds have not had a listening pass.
- **Builds:** a shipped build must put `spike-run` next to the extension for CHECK to work. Desktop Linux only, and NEST runs on one thread.

## [0.2.0] - 2026-11-16 — Body systems

The game widens from three small circuits to a campaign of eleven vertebrate-physiology tasks, with real effectors, new cell types and neuromodulators.

### Added
- **Campaign.** Five chapters on an anatomical map: Invertebrates (the three 0.1 tasks), Spinal cord, Heart, Breathing, Autonomic. A chapter opens when half of the one before it is passed, and the tasks inside a chapter unlock in order.

  | Chapter | Tasks |
  |---|---|
  | Invertebrates | `reflex`, `worm`, `lamprey` |
  | Spinal cord | `stretch_reflex`, `renshaw`, `posture`, `reach` |
  | Heart | `heart_pacemaker`, `heart_autonomic`, `baroreflex` |
  | Breathing | `breathing` |
  | Autonomic | `pupil`, `peristalsis`, `fight_or_flight` |

  Each task has a reference circuit, lesions that fail the way the biology predicts, a timeline of scripted perturbations with score bands, and a body view (limb, heart, lungs, eye, gut or dashboard).
- **Body physics** in `engine/src/body/`, documented in `docs/body/`: Hill-type muscle, a single-joint limb, heart and circulation with baroreceptors, lungs with CO2 chemoreception, eye and pupil, a peristaltic gut chain.
- **Parts bin.** Every NEST neuron model as a card with its key parameters, units and a live f-I or V_m thumbnail from the engine (`catalog/thumbs.json`, written by `spike-catalog`). Parts unlock with the chapters.
- **NESTML cells**, compiled into the static build (`scripts/build-nestml.sh`): SA-node pacemaker, Butera respiratory burster, ICC slow-wave oscillator. A spike-parity test checks them against PyNEST.
- **Neuromodulators** NE, ACh and DA as engine channels (`Engine::set_modulator`, `modulator`), a `modulation` table in the circuit format, per-model default modulation for the NESTML cells, and a `"modulator"` output kind that releases them from spikes. Sandbox dials work on any circuit.
- **Lesion lab.** After a pass, cut synapses or silence populations on a re-run and get a lesion table (CSV and Markdown export).
- **Sandbox.** All parts, no task, scopes and analysis.
- **Analysis in the game:** 4-node motif census (199 classes, with a work-limit guard that refuses graphs too big to randomise in reasonable time), heart-rate variability (SDNN, RMSSD, pNN50, LF/HF), phase-response curves from the task's scripted perturbations, burst detection, and an energy readout. HRV, PRC and bursts re-run the circuit on the task and keep every spike (RECORD). The same analyses are in Python (`spike.hrv`, `spike.prc`, `spike.bursts`, the 4-node census in `spike.motifs`).
- **Stars.** 1 pass, 2 pass on all 5 TEST seeds, 3 the same within the neuron par and the energy budget (mean spikes per second per neuron, `Progress.ENERGY_HZ`; the budget is 1.6 x what the reference circuit spends). TEST x5 reseeds the world and the circuit and records the mean score, not the best. A circuit that is an edited copy of a reference circuit is "reference-derived" and earns no stars.
- **`spike-run --task <id>`** scores a circuit headless, with the game's own loop: `spike-run --task heart_pacemaker --seeds 1,2,3 [--circuit file.json] [--lesion pre>post*k] [--require any|all]` prints the score JSON (value, passed, verdict, parts, neurons, spikes, rate per neuron) and exits non-zero when the `--require` condition fails. `spike-run --list-tasks` lists the task ids. `scripts/smoke.sh` uses it to check every reference circuit.
- **Smoke test.** For every real task: the empty bench's channel kinds equal the reference circuit's, a minimal circuit moves the task's modulators, the real body view has its readouts, and the reference passes headless.
- **GDExtension.** `SpikeSim` binds `set_modulator`, `modulator`, `modulator_names`, `bursts_json`, `prc_json`, `prc_experiment_json`, `hrv_json`, `motif4_json`, `motif4_json_edges`; `SpikeTask` binds `channel_kinds()`.

### Changed
- The game needs Godot 4.7 (the extension declares compatibility from 4.5).
- The `Telemetry` modulator dial calls `SpikeSim.set_modulator` directly; the fallbacks that created a `SpikeTask` per frame are gone.
- Task order on the campaign map comes from the engine's registry; the map only keeps positions.

### Known gaps
- **Reach is hard from an empty bench.** A beginner-built circuit passes about 2 of 6 seeds. The reference relies on spread synapse weights and cell parameters the editor cannot set yet. Planned for 0.3: a weight-spread control.
- **Renshaw does not enforce its lesson.** A stretch-reflex circuit with no Renshaw cells can still pass, because the control tremor curve was measured on the reference, not on the player's own circuit.
- **Sensor weights.** The first connection from a dc sensor uses one generic weight. A per-task default is planned.
Also deferred to 0.3 (see `docs/REQ-0.2.md`):
- **Editor synapses:** the editor offers `static_synapse` only. STDP, dopamine STDP, Tsodyks, gap junctions and quantal STP still load from circuit files but cannot be added in the game.
- **"Biologically faithful" star** (motif similarity to the reference): not implemented. The 3- and 4-node census is in the game, the comparison is not.
- **Muscle from motor-unit spike trains:** the tasks drive the muscle from pool rates, as `docs/body/muscle.md` describes.
- A researcher cannot build tasks in Python; score circuits with `spike-run --task`.
- Desktop Linux only, one NEST thread, no audio (unchanged from 0.1).

## [0.1.0] - 2026-11-16 — First circuit

The first release: wire spiking neurons into circuits that make a body do something.

### Added
- **NEST built in.** The NEST simulator runs inside the game through a C++ engine. It runs in closed loop: every 1 ms the game sets sensor inputs, runs NEST and reads the spikes back, at about 3 µs of overhead per step. Results are deterministic, checked with golden spike checksums.
- **Circuit files.** A circuit is a versioned JSON file using NEST model and parameter names. The same file runs in the game, in the `spike-run` command and in PyNEST, with spike-for-spike parity.
- **Three tasks**, each with a reference circuit, lesions that fail the way the biology predicts, and plain-language verdicts:
  - **Reflex arc:** detect a touch through the noise, by convergence.
  - **Lamprey swimmer:** a half-centre oscillator with crossed inhibition and adaptation.
  - **Worm chemotaxis:** steer by the difference between the ASEL and ASER sensory neurons.
- **Lab.** Build a circuit with NEST models and presets, connect neurons with weight, delay and synapse model, and edit neuron parameters in an advanced panel. Add noise inputs. Watch the live world, the spike raster, voltage traces and output rates, and get a score.
  - **TEST x5** runs your circuit on 5 seeds.
  - You earn up to 3 stars: pass, pass on all 5 seeds, and stay under the neuron budget.
  - An end-of-episode card suggests what to try next.
  - Progress is saved, tasks are ordered and unlock in turn, and circuits load and save as JSON.
- **Analysis:**
  - **Motif census:** all 13 three-node connection patterns, numbered as in Milo et al., against degree-preserving random rewiring. It works on populations or on individual neurons, and warns when the random comparison is too weak to mean anything.
  - **Firing statistics:** firing rates, ISI CV, Golomb χ and PSTH.
- **`spike` Python package.** It builds the same network in PyNEST, with motif and statistics tools and an example notebook.
- **Layouts.** Desktop and phone.

### Known gaps
- Desktop Linux build only. Windows, macOS and Android packaging are not done yet.
- NEST is built without OpenMP, so it runs on one thread.
- There is no audio.
- The editor does not offer plastic synapses (STDP, Tsodyks). Circuits that already use them still load.
- On a phone, the tasks screen needs scrolling.
