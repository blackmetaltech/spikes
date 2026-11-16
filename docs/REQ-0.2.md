# v0.2 requirements: Body systems (vertebrate circuits, more neuron types, muscles, autonomic control)

0.1 proved the loop: real NEST neurons in a closed loop with a body and a score. 0.2 widens the game. It adds many more cell types, real effectors (muscle and heart), neuromodulation, and a campaign of circuits from vertebrate physiology. Each circuit can be built, broken and analysed.

## Ground rules
These are the same as 0.1:
- Keep the contracts in `engine.h`, `task.h` and `CIRCUIT-FORMAT.md`, and only add to them.
- Use NEST models where they exist.
- Every task has a reference circuit that passes and lesions that fail.
- Use real units and cite sources in `docs/circuits/<id>.md`.
- Keep everything deterministic, with golden traces.
- Do not commit.

## A. Neuron parts bin (engine + game)
- **Catalogue** (`engine/src/catalog/*`, `godot` parts bin). Every model is a card with a plain-language description, its key parameters with units, a typical f–I or voltage trace thumbnail rendered live by the engine, and its biological examples. The NEST models are:
  - integrate-and-fire: `iaf_psc_alpha`, `iaf_cond_exp`, `iaf_cond_exp_sfa_rr` (adapting);
  - adaptive exponential: `aeif_cond_alpha` / `aeif_cond_exp`;
  - `izhikevich` with all named presets (RS, IB, CH, FS, LTS, TC, RZ, and so on);
  - Hodgkin–Huxley: `hh_psc_alpha` and `hh_cond_exp_traub`;
  - `ht_neuron` (thalamocortical);
  - `gif_cond_exp` (spike-triggered adaptation);
  - `mat2_psc_exp`;
  - `pp_psc_delta` (stochastic);
  - `parrot_neuron` (relay).
- **Custom cell types through NESTML**, compiled into the static build. NESTML (pip `nestml` 9.x) generates C++ from `models/nestml/*.nestml` at build time, as a NEST extension module linked statically and registered at engine init. Each model is validated against its paper figure in a test. The models are:
  1. **SA-node pacemaker cell**: a reduced sinoatrial model with funny current I_f (HCN), T- and L-type Ca and delayed rectifier K. It fires spontaneously at about 70/min. ACh (via I_KACh, and by reducing I_f) slows it, and noradrenaline (by raising I_f and I_CaL) speeds it up. Source: a reduced Demir- or Maltsev-style model, with the source documented.
  2. **Bursting pacemaker**: a persistent-sodium burster (Butera, Rinzel & Smith 1999, model 1) for the preBötzinger respiratory rhythm.
  3. **Smooth-muscle / interstitial cell of Cajal slow-wave oscillator**: a simple two-variable model for the gut.
  4. **Muscle spindle Ia/II afferent**: a stretch- and velocity-sensitive transducer (a Mileusnic-style simplification) driven by task sensor channels. It can be built from NEST generators if that is simpler; document the choice.
- **Neuromodulation**:
  - noradrenaline (NE), acetylcholine (ACh) and dopamine (DA) concentrations as engine-level channels (`set_modulator(name, conc)`), mapped onto per-model parameters through a documented table (for example, NE raises the pacemaker's I_f);
  - a new `"modulator"` output kind for tasks that release them, from spikes of a named population;
  - `volume_transmitter` plus `stdp_dopamine_synapse` for reward learning.
- **Synapses in the editor**: `static`, `stdp`, `stdp_dopamine`, `tsodyks2` (short-term plasticity), `gap_junction` (with `hh_psc_alpha_gap` or `hh_cond_beta_gap_traub`), and `quantal_stp`.

## B. Effectors and bodies (`engine/src/body/*`)
These are shared physics modules that tasks compose. They are deterministic and documented.
- **Muscle**: a Hill-type muscle (force–length, force–velocity) with twitch summation into tetanus from motor-unit spike trains. Motor units have Henneman size-principle recruitment (small, slow units are recruited first). It produces force, length and fatigue.
- **Joint**: a single-joint limb with an agonist/antagonist pair, inertia, gravity, a load and joint limits.
- **Heart and circulation**: an SA-node rate that drives a beat event, stroke volume, and a 2-element Windkessel giving arterial pressure, with baroreceptor stretch sensing. Sympathetic input raises heart rate, contractility and vascular resistance. Vagal input lowers heart rate.
- **Lungs**: a breath-volume model driven by inspiratory motor output, with blood CO₂ or pH chemoreception feeding back.
- **Eye**: pupil diameter set by the sphincter (parasympathetic) and the dilator (sympathetic), with light on the retina.
- **Gut segment**: a chain of smooth-muscle segments for a peristaltic wave moving a bolus.

## C. Campaign tasks (the "Body systems" chapter)
Each task has a reference circuit, at least 2 lesions that fail with the biologically expected failure, a brief, and a score. They are grouped into organs and unlock in order.

| id | Circuit | Pass when |
|---|---|---|
| `heart_pacemaker` | an SA-node cell (or a coupled cluster through gap junctions) | rate 60–80 bpm and stable (beat-interval CV < 5%); cluster synchrony |
| `heart_autonomic` | pacemaker plus vagal (ACh) and sympathetic (NE) inputs | track a target heart rate through rest, exercise and recovery phases |
| `baroreflex` | baroreceptor → NTS → nucleus ambiguus (vagal) and CVLM ⊣ RVLM (sympathetic) → heart and vessels | hold mean arterial pressure 90±10 mmHg through a standing-up (orthostatic) drop and a haemorrhage |
| `stretch_reflex` | Ia afferent → α motor neuron (monosynaptic), with reciprocal Ia inhibition of the antagonist | resist an imposed stretch quickly without oscillating |
| `renshaw` | add Renshaw recurrent inhibition | stabilise motor-neuron output and reduce force tremor |
| `posture` | stretch reflex plus gamma drive, keeping a joint upright under gravity and random pushes | angle error within the bound |
| `reach` | agonist/antagonist burst pattern (triphasic) | move to the target angle in time with little overshoot |
| `breathing` | a preBötzinger burster network → phrenic motor neurons → lungs, plus CO₂ chemoreflex | breathe at 12–20/min and raise ventilation when CO₂ rises |
| `pupil` | light reflex: retina → pretectum → Edinger–Westphal → ciliary ganglion → sphincter; sympathetic dilator for darkness or fright | pupil tracks light steps with the right latency and both directions work |
| `peristalsis` | an enteric circuit (ascending excitation, descending inhibition) over the gut chain | move the bolus to the end |
| `fight_or_flight` | a combined autonomic scenario: a threat cue raises sympathetic output (heart rate, blood pressure, pupil) and parasympathetic output recovers it | score each system's trajectory |

The 0.1 tasks become the "Invertebrates" chapter.

## D. Mechanics (game)
- **Campaign map.** Chapters (Invertebrates, Spinal cord, Heart, Breathing, Autonomic) are drawn as an anatomical map of node cards. Each card shows best score, stars (pass, efficient, elegant) and unlocks.
- **Budgets and stars.**
  - The neuron and synapse count is shown against par, and spikes per second are shown as an energy cost (ATP estimate).
  - Stars are awarded for pass, under par, and low energy.
  - A "biologically faithful" star is awarded when the player's circuit's motif census resembles the reference circuit's.
- **Perturbations.** Each task has a script of events (a push, standing up, exercise, a light flash, CO₂ rising), shown on a timeline under the world with the score's tolerance bands.
- **Lesion lab.** After a pass, you can cut any synapse or silence any population and watch the failure. This produces a lesion table, which is how neuroscience is actually done.
- **Sandbox.** All parts are available, with no task: just the scopes and the analysis.
- **Parts bin.** The catalogue cards from A. A new cell type unlocks when its chapter starts.
- **Modulator dials.** NE, ACh and DA appear as sliders in sandbox, or as task-driven gauges.
- **Phone layout.** The lab follows the 0.1 critique fixes.

## E. Analysis
- Phase-response curves for oscillators.
- Burst detection.
- Heart-rate variability (time domain, and an LF/HF power spectrum).
- A 4-node motif census (199 classes), which is optional and slower.
- The motif comparison between the player's circuit and the reference is used for the elegance star.
