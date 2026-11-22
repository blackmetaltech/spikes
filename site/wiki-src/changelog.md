---
title: Changelog highlights
section: Under the hood
order: 87
summary: What changed in each version of Spike, in plain words and newest first, with the gaps the project says remain.
---

# Changelog highlights

This is a player's summary of `CHANGELOG.md` in the repository. It is newest first. The game's title screen shows the version of the build you run.

## 0.5.0 (2026-11-22): the Bestiary, the Theory wing, and stars that mean something

The campaign now has 79 levels in three wings. There is no separate 0.6: the Theory wing ships in 0.5.

**New levels**
- **The Bestiary** has six chapters, one per model organism: the worm (*C. elegans*, wired from the real touch circuit), the leech heartbeat, the sea slug (*Aplysia*), the fly compass, the fly mushroom body and the crab pyloric rhythm. That is 18 levels and 7 diagnosis cases. See [the Bestiary](campaign-bestiary.html).
- **The Theory wing** has four chapters of three levels: dynamics (a phase-plane view), rate models and mean field, attractor memory, and tuning and decoding (with a spike-triggered-average tab in Analysis). See [the Theory wing](campaign-theory.html).
- **A new micro-puzzle, Tonic drive** (a bias current), after Adaptation: 20 micro-puzzles now.
- **Diagnosis is scored on probes.** The cases track the probes you used and your wrong guesses, the order of the candidates does not hint at the answer, and no finding names the answer. See [diagnosis cases](diagnosis-cases.html).

**New in the game**
- **Plate 0**, an interactive tutorial, and a mode picker on the first run.
- **A rewritten editor:** undo and redo, zoom and pan, multi-select, copy and paste, keyboard and gamepad building, three solution slots per level, JSON copy and paste, and an import from a level you have passed. See [controls](controls.html).
- **Stars that mean something.** CHECK runs five worlds. One star is a pass, two is all five worlds, three is a per-level challenge that is written on the level card. Guided mode stops at two stars. A Researcher pass earns a seal. A diagnosis case's third star needs the right answer after a probe. See [stars and hints](stars-and-hints.html).
- **Gated wings and Needs chips.** The Bestiary and Theory open once you pass the Invertebrates spine; a few Bestiary chapters wait for an earlier level; each level card lists what it needs. Posture is now a branch, like Reach.
- **A score stamp** with the criteria and a Details panel, a map wing selector with a Continue button, and larger, more careful body drawings.
- **Safer saves:** atomic writes, backups, a bench history you can restore, save export and import, Reset progress and a log folder. A damaged file is set aside instead of crashing the game. The engine refuses hostile or enormous circuits with a plain message.

**Under the hood:** the engine has safety limits and a fuzz test, the UI has an interaction graph and headless tests, CI uses a prebuilt toolchain image, and this website and wiki are built from the repository. New circuit-file input and output kinds support the Theory wing; the Python package does not read them yet. See [a circuit is data](circuit-is-data.html) and [spike-run](spike-run.html).

**Gaps the project states:** the leech heartbeat level still needs a rework of its escape-versus-release concept; the spinal boss reference passes about 80 percent of held-out seeds; `stretch_reflex`'s quick CHECK misses 1 or 2 of 35 test seeds; nothing yet re-runs a circuit with the mechanism removed to prove the mechanism matters; the fly connectome excerpt is not real synapse data; the Python package refuses the Theory input and population kinds with a plain message; the phone layout has rough spots; there is no Windows build.

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
