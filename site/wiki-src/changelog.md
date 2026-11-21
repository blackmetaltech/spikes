---
title: Changelog highlights
section: Under the hood
order: 87
summary: What changed in each version of Spike, in plain words and newest first, with the gaps the project says remain.
---

# Changelog highlights

This is a player's summary of `CHANGELOG.md` in the repository. It is newest first. Versions marked "in progress" are not released: they are on the main branch and in source builds. The game's title screen shows the version of the build you run.

> **Note:** The numbering is uneven. The game project file says "0.5 in progress", while the changelog already has entries for the 0.6 Theory wing under "Unreleased". The Python package says 0.4.0. Treat the dates and tags on the [releases page](https://github.com/ixaxaar/spikes/releases) as the final word.

## In progress: the Theory wing (0.6)

A new wing of the campaign, open from the start like the Bestiary, built on what a computational-neuroscience course teaches. See [the Theory wing](campaign-theory.html).

- **Chapter 1, Dynamics.** Where firing starts (class I and class II cells), a cell that reports only change, and one neuron that remembers a pulse. The lab gets a phase-plane view: nullclines, fixed points, the reset and a live trajectory.
- **Chapter 2, Rate models and mean field.** Two populations that oscillate (a Wilson-Cowan pair), two that compete with a held choice, and a spiking network compared with its mean-field prediction. The lab gets rate units, rate connections, a mean-field link with its own J and K numbers, and a rate strip instead of a spike raster for circuits of rate units.
- **Chapter 3, Attractor memory.** A partial cue recalls a whole pattern; two patterns without blends; a pattern written by spike-timing-dependent plasticity. The lab gets a pattern view and a plastic-synapse editor.
- **Chapter 4, Tuning and decoding.** A tuned cell built from its inputs, tuning sharpened by competition, and a direction read from a crowd of cells with the population vector. The lab gets a tuning view with a stimulus dial. The Analysis screen gets a spike-triggered-average tab.
- **Under the hood.** New input and output kinds, rate neurons and `siegert_neuron` as populations, and new command-line options (`--sample`, `--set`, `--phase`). See [a circuit is data](circuit-is-data.html) and [spike-run](spike-run.html).
- New citations, checked against PubMed.

## In progress: 0.5 playtest fixes

A review of the new levels with novice-style circuits led to a round of fixes.

- **The reference lock** only flags a circuit as a copy of the reference when it truly equals the reference file, or when you pressed Reference. Before, round-number circuits you built yourself could be wrongly flagged and lose their stars.
- **Structure checks** that were too strict were loosened: a latch can hold itself with one cell exciting itself, and the worm level accepts turning only one way.
- **Check** now runs the short episode. The two boss levels got quick versions of about 5 seconds each.
- **No numbers in verdicts and hints**, for every level, pass or fail, all three tiers. A test scans every one.
- **Verdicts follow the facts.** Several verdicts that said the opposite of what the circuit did were corrected (the leech burster, the lamprey, the Aplysia levels, the leech rebound).
- **Gauges** are all fed. A missing readout shows a dash rather than a green 0.0.
- **Lab and map:** two rows of chapter tabs (Body and Bestiary) and a scrolling strip on phones; the pass rule is visible in the brief; slider limits come from each level's needs; a verdict asking for more strength is not shown when a slider is at its end; notifications queue one at a time; hints open after two different missed circuits; diagnosis cases offer Reveal answer after several wrong picks.
- Some difficulty settings were adjusted: the half-centre level's default drive, the Aplysia lab defaults, and the leech rebound brief.

## In progress: 0.5 Bestiary, step 3 (fly compass, fly mushroom body, crab)

Thirteen levels in three more chapters. See [the Bestiary](campaign-bestiary.html).

- **Fly compass:** a bump of activity on a ring of cells that holds the heading and follows turns in the dark, plus a diagnosis case.
- **Fly mushroom body:** a sparse odour code, and a dopamine-gated depression that teaches the fly to avoid an odour, plus two diagnosis cases.
- **Crab:** a conductance-based pacemaker kernel and rebound followers making the pyloric rhythm, plus a diagnosis case.
- A new custom cell, `stg_cell` (a stomatogastric neuron), with its own parts-bin card.
- The lab gets a ring readout, ring-closing wiring, random sparse wiring, a dopamine-gated synapse with source and learning, and a compass dial.
- Honesty notes: the compass and mushroom body follow published cell types, not synapse-level data; the crab cell's kinetics are written from memory of the published forms and its parameter sets are not the published ones. See [what is real](what-is-real.html).
- A half-wired circuit (a sensor or motor left unconnected) no longer passes a scripted level.

## In progress: 0.5 Bestiary, step 2 (worm, leech, sea slug)

Twelve levels in three new chapters, a wing that is open from the start and never gates the body chapters.

- **Worm:** hold a reversal, choose forward or backward, and a touch circuit wired from real synapse counts of the C. elegans connectome (read through OpenWorm's ConnectomeToolbox).
- **Leech:** a bursting cell, rebound after inhibition, and the two-cell heartbeat.
- **Sea slug (Aplysia):** a reflex that gets used to a touch, one that a tail shock sensitises, and the gill-withdrawal reflex that combines them.
- Each chapter has a diagnosis case.
- The lab offers depressing synapses and a modulation row on a synapse for the Aplysia levels.
- What is real: worm and leech cells are spiking stand-ins for graded cells; the leech cycle is time-compressed.
- Known gaps at the time: no plate art for the Bestiary map; deleting a connection in the lab did not renumber a synapse modulation row; the citation checker still reported unverified references in the research sheets.

## 0.4.0 (2026-11-18): Honest puzzles, shippable game

A response to a harsh review.

- **Mechanism checks on all 41 levels** of the time. The verdict names the missing part, for example no coupling between pacemaker cells, or a baroreflex with only one limb.
- **Cheap circuits that must fail:** an empty bench, a single cell, zero weights, the reference without its mechanism and more, for every level. See [contributing](contributing.html).
- **Recipes kept apart:** verdicts and hints carry no numbers.
- **Faster Check:** seeds run in parallel and most levels have a shorter quick episode.
- **The Cajal Plate look:** paper and ink, sienna and Prussian blue, new fonts, atlas-plate maps.
- **UI honesty:** the reference is hidden until passed (or in Guided mode); the histograms say they are generated variants; an in-game What is real page; tools appear in the lab as the campaign needs them.
- **Citations checked** against PubMed and Crossref; wrong ones fixed.
- **Shipping:** levels and catalogue inside the game, a Linux export, `spike-run` bundled, a build script, a Dockerfile and CI.
- **Fixed:** NEST used to abort on huge conductances. Weights are now limited when a circuit loads, and a failed step stops the run with a message.
- Known gaps then: some levels had no novice test; Check was slow on several levels; no Windows build; the Linux build needs glibc 2.39 or newer; 34 citations (books and older papers) unverified; two levels (`m_reciprocal` and `m_wave`) nearly trivial.

## 0.3.0 (2026-11-17): Learning curve

The campaign became a ladder. See [the body chapters](campaign-body.html).

- **19 micro-puzzles**, each isolating one idea with 1 to 4 cells and a 3 to 5 second episode, placed just before the task that needs it.
- **Two boss levels:** the spinal sprint, and the autonomic exam (a whole day).
- **Six diagnosis cases.** See [diagnosis cases](diagnosis-cases.html).
- **Three modes:** Guided, Standard and Researcher. See [modes](modes.html).
- **The hint ladder:** the idea, then the place, then a partial recipe. See [stars, par and the hint ladder](stars-and-hints.html).
- **Challenge variants** for every task, such as half the neurons or double noise.
- **Post-solve histograms** against generated variants, not other players.
- **The lab notebook** with a cited finding unlocked by each pass, and a concept glossary. See [the notebook](notebook.html).
- **Sound:** spike clicks, heartbeat, breath, muscle, gut and interface sounds, with four volume sliders.
- **Feel:** a first-pass celebration, an instant Check, skip to the next event, and a default speed from measured throughput.
- **Map:** a main path with branches; a chapter opens when the previous chapter's main path, or the one two before it, is passed.
- An opt-in local play log.
- Per-connection weight spread; asynchronous headless scoring; a new level contract in the engine.
- Changed: levels open on the cell type their puzzle assumes; the reflex level needs convergence; the Renshaw level measures your own circuit; the posture verdict reads which way the limb is off.
- Known gaps then: reach was off the main path because no novice circuit passed it reliably; histograms were generated, not real players; the sounds had not had a listening pass.

## 0.2.0 (2026-11-16): Body systems

The game widened from three small circuits to a campaign of eleven body-system tasks.

- **Five chapters on an anatomical map:** Invertebrates, Spinal cord, Heart, Breathing, Autonomic.
- **Body physics:** muscle, a single-joint limb, heart and circulation, lungs with CO2 sensing, eye and pupil, a gut chain. See [bodies](bodies.html).
- **The parts bin** with every NEST neuron model as a card, with a live thumbnail. See [the parts bin](parts-bin.html).
- **Custom cells:** the sinoatrial pacemaker, the respiratory burster, the gut slow-wave cell.
- **Neuromodulators** NE, ACh and DA, with sandbox dials. See [synapses and modulators](synapses-and-modulators.html).
- **The lesion lab, the sandbox and in-game analysis** (motifs, heart-rate variability, phase-response curves, bursts, energy). See [the lesion lab and analysis tools](lesion-lab-and-analysis.html) and [the sandbox](sandbox.html).
- **Stars:** 1 for a pass, 2 for all five test seeds, 3 within the neuron par and the energy budget.
- **`spike-run --task`** to score a circuit headless.
- Known gaps then: reach was hard from an empty bench; the editor offered only static synapses; some cell and sensor defaults were generic.

## 0.1.0 (2026-11-16): First circuit

- NEST built into the game through a C++ engine, run in a 1 ms closed loop.
- Circuit files as versioned JSON, with spike-for-spike parity with PyNEST.
- Three levels: the reflex arc, the lamprey swimmer and worm chemotaxis.
- The lab: models and presets, connections with weight, delay and synapse model, noise inputs, a live world, a raster, voltage traces, outputs, Test x5, three stars and a save and load.
- The three-node motif census and firing statistics, and the `spike` Python package.
- Desktop and phone layouts.
- Known gaps then: Linux only, one NEST thread, no sound, no plastic synapses in the editor.

## Gaps that the project still lists

- Linux only; no Windows or macOS build yet.
- NEST runs on one thread by default.
- Some citations are unverified (books and older papers).
- The reference circuits and their numbers are tuned, not measured; see [what is real](what-is-real.html).
- Histograms are generated variants, not other players.
