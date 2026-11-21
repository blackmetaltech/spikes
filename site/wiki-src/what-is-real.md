---
title: What is real
section: Reference
order: 58
summary: What is NEST's own maths, what was re-fitted, what was tuned until a level passed, what is illustrative, and what is generated.
---

# What is real

Spike is built on real neuron models. The bodies and the circuits around them are simplified. This page says which parts are the simulator's own maths and which are game design. It follows the repository's page `docs/WHAT-IS-REAL.md` and the in-game screen of the same name (title screen, **III. What is real**). Where this page and the game differ, trust the game.

## Five labels

| Label | Meaning |
|---|---|
| **NEST** | The equations are NEST's own, unmodified. Checked by golden checksums and a parity test against PyNEST. |
| **NESTML re-fit** | A custom cell written in the NESTML language for this game, with its structure taken from a paper and its numbers adjusted by hand. Not validated against recordings. |
| **Tuned in silico** | A number chosen by sweeping or hill-climbing until the level passed: circuit weights, delays, gains, pass bands. It is a property of the game, not a measurement. |
| **Illustrative** | A coefficient or time constant picked so a body behaves plausibly. Shapes are tested. Values are not fitted to data. |
| **Generated** | Produced by a script, not by people or experiments (the histograms). |

NEST is the simulator used by neuroscientists to run large spiking networks. Spike links it into the game, so a neuron in the game is a NEST neuron.

## By layer

| Layer | Status |
|---|---|
| Generic cells (`iaf_psc_alpha`, `iaf_cond_alpha`, `aeif_cond_alpha`, `izhikevich`, `parrot_neuron`, gap junctions, Poisson input) | **NEST.** The equations are the published ones. The parameter values the levels use are chosen for the game. |
| `butera_burster` (breathing pacemaker) | **NESTML**, equations and parameters from Butera, Rinzel and Smith 1999 (model 1). Single compartment. Checked against the paper qualitatively, not quantitatively. |
| `sa_node_pacemaker` (heart pacemaker) | **NESTML re-fit.** Structure follows rabbit sinoatrial cell models, reduced to five currents. Conductances were adjusted by hand so that the isolated cell beats at the human intrinsic rate (about 100 per minute). Not validated. |
| `stg_cell` (crab neuron) | **NESTML re-fit.** The gating forms follow the Prinz et al. 2003 family but were written from memory and not checked against them. The conductances are game values, not the published sets. |
| `icc_slow_wave` (gut pacemaker) | **NESTML, phenomenological.** A FitzHugh-Nagumo oscillator rescaled to slow-wave units. It is not a channel model. |
| Circuit size | **Simplified.** One to 80 cells stand for pools of thousands. A cell is a representative of a population. |
| Weights and delays | **Tuned in silico.** The signs and the wiring pattern follow the cited biology. The magnitudes do not. |
| Neuromodulators | **Illustrative.** Global scalars with heuristic gains. Real modulators act through receptor-specific cascades. |
| Body physics | **Structure from the literature, illustrative coefficients.** Hill force-velocity, the size principle, a Windkessel for blood pressure, a single-compartment lung, a Naka-Rushton retina. Numbers are chosen so the module sits at a textbook resting value. See [the bodies](bodies.html). |
| Task worlds | **Game design.** They follow the cited experiments in kind but are built for a short puzzle. Pass bands and scoring are game values. |
| Notebook notes, concept cards | **Cited**, with honest limits. Where a level uses a real number for another purpose, the note says so. |
| Diagnosis cases | **Fictional models of a real kind of case.** The lesion is a missing wire in a model circuit. |
| Histograms | **Generated.** See below. |
| Hint tiers, recipes | **Tuned in silico**: they are the reference circuit's values. |

## The Bestiary: honest limits

**Worm and leech cells are spiking stand-ins for graded cells.** Most neurons of the *C. elegans* touch circuit have no sodium action potentials and pass signals as smooth voltage changes. Leech heart interneurons inhibit each other partly through graded release. NEST has no graded synapse, so these levels use spiking cells and spike-triggered synapses.

| Chapter | What to remember |
|---|---|
| Worm | The wiring of the touch circuit comes from published synapse counts (Cook et al. 2019, via the OpenWorm ConnectomeToolbox). Weight is the count times 25 pA. Every chemical synapse is assumed excitatory because the data have no sign. The mutual inhibition between the forward and backward command cells is added by hand. |
| Leech | **Time is compressed.** The cycle is about two seconds so a check takes a second. The real cycle is much slower. The coordination between segments is not modelled. |
| Sea slug | Time is compressed by a large factor. Serotonin is the engine's NE channel. The second-messenger cascade is replaced by a direct change of the synapse. Depression is the generic Tsodyks-Markram form, not a fit to Aplysia data. |
| Fly compass | After the published cell types, not synapse-level data. The ring is a ring-attractor model. All weights are game values. Tracking is good only for turns of about 90 to 170 degrees per second. |
| Fly mushroom body | After the published cell types. 200 Kenyon cells, not about 2000. Four inputs per cell. A pairing lasts 0.4 s. The learning rule is a NEST dopamine-gated synapse used with a sign that depresses. |
| Crab | Not the published parameter sets. The electrical coupling between the pacemaker cells is approximated by fast excitation. Two hand-picked parameter sets both pass (the "degeneracy" the chapter teaches). |

## The Theory wing: honest limits

| Chapter | What to remember |
|---|---|
| Dynamics | The phase plane holds for a constant current and ignores the synaptic conductances of the aeif cells. The class of a cell is a game criterion computed from its resting state. The one-neuron switch fires at about 150 Hz when on. Real persistent firing is 5 to 50 Hz and channel-based, which is not modelled. |
| Rate models | The units are NEST threshold-linear rate neurons with a ceiling, not the Wilson-Cowan sigmoid. Rates are in arbitrary units. |
| Mean field | The network is small (200 excitatory cells) and not very sparse. The prediction still agrees within a few percent, so the level does not show where mean field breaks down. |
| Memory | A pattern is a group of cells, not a plus-or-minus-one vector. Persistent activity is at 100 Hz or more because there is no balanced inhibition. Real delay activity is 10 to 40 Hz. One second of exposure stands for minutes of learning. |
| Tuning | The `tuned_poisson` input is an engine input kind, not a NEST model: it sets the rate of one independent Poisson train per afferent cell from a von Mises curve around its preferred angle. The tuning curves are given, not learned. Real afferents are not evenly spaced or Poisson. The population vector is one linear decoder, not the best one. |

## Diagnosis cases

Each case is a fictional animal or patient with one missing connection. They echo real conditions, with honest limits.

| Case | Real condition | Limit |
|---|---|---|
| The shaky arm | Reduced recurrent inhibition has been proposed as one contributor to spasticity. | Isolated Renshaw loss is not a clinical cause of clonus. The patient shows excess 10 Hz tremor. |
| The racing resting pulse | Vagal withdrawal and resting tachycardia. | Model heart, lumped drives. |
| Faint on standing | Neurogenic orthostatic hypotension. | The model's heart rate still rises through vagal withdrawal. Real neurogenic cases have a fixed pulse. |
| Stops breathing in sleep | Congenital central hypoventilation. | One removed wire stands for a genetic syndrome. |
| The small pupil | Interrupted sympathetic chain (Horner syndrome). | Three-neuron chain collapsed. |
| Food that will not pass | Loss of inhibitory neurons in achalasia. | Gut segments reduced to chains. |

## What is generated

- The histograms ("where you stand"): random variants of the reference circuit, kept if they pass two seeds. They are not other players. The game says so on the card.
- The audio: synthesised by a script.
- The compiled NESTML cells: generated from the model files.
- The diagnosis circuits: generated from the reference circuit with the answer's weights set to zero.
- The worm touch circuit: generated from the connectome package.
- The Theory and Bestiary reference circuits: generated by scripts in `circuits/tools` (the repository's `circuits` folder is a link to `godot/circuits`).
- The citation list in the docs: generated from the registry.

## What this does not claim

- That any circuit weight is a physiological measurement.
- That any body reproduces a particular patient or subject.
- That NESTML cells match published recordings.
- That a level's pass band is clinically meaningful.
- That the limits on weights, rates, delays and sizes the engine enforces (see [a circuit is data](circuit-is-data.html#limits-and-errors)) are physiological bounds. They are numerical safety rules: they keep NEST's integrators from diverging and keep a circuit file from hanging the game.

## How honesty is enforced in the game

- Every level checks the **mechanism** through the circuit's structure, so a cheap trick that hits the numbers fails.
- Verdicts and the three hint tiers carry no numbers. A test scans them.
- Each level has a table of cheap circuits that must fail (an empty bench, a single cell, zero weights, and more).
- Citations are checked against PubMed or Crossref with DOIs.

See [contributing and the honesty contract](contributing.html) and [the notebook](notebook.html).

## Related pages

- [Parts bin](parts-bin.html)
- [Bodies](bodies.html)
- [Glossary](glossary.html)
