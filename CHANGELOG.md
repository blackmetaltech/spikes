# Changelog

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
