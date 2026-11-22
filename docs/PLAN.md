# Spike: plan

Spike is a puzzle game about wiring small spiking neural circuits that run a simplified body, and a research side (a Python package) that runs the same circuit files in PyNEST. This page describes what ships today and where the next releases go. What is NEST, what is tuned and what is illustrative is in [WHAT-IS-REAL.md](WHAT-IS-REAL.md).

## Shape

```
 NEST (pinned commit, built static)      the simulator: NEST neuron and synapse models; NESTML cells compiled in
 engine/  C++20, CMake -> libspike
   engine.h          the contract: load circuit JSON -> closed-loop step / set_input / drain_spikes / structure
   circuit.cpp       circuit JSON (docs/CIRCUIT-FORMAT.md) -> NEST nodes and connections
   tasks/            79 levels: worlds, bodies and scores; each registers itself (SPIKE_REGISTER_TASK)
   body/             muscle, joint, heart and circulation, lungs, eye, gut (no NEST, deterministic)
   analysis/         motif census (3- and 4-node), rates, CV, synchrony, bursts, HRV, PRC
   spike-run         headless scorer and runner (Check, CI, histograms)
 gdext/   godot-cpp GDExtension over libspike: SpikeSim, SpikeTask
 godot/   Godot 4.7 game, code-built UI in the "Cajal Plate" identity (docs/IDENTITY.md); levels and catalogue in godot/circuits and godot/catalog
 python/  `spike` package: the same circuit JSON -> PyNEST, plus analysis
```

## Principles
- **Use NEST models, do not reinvent them.** NEST's names and units throughout. The levels, the editor and the analysis are what Spike adds.
- **A circuit is data.** One versioned JSON file, read by the game, the engine and the Python package. A parity test checks PyNEST spikes against the engine on the parity circuits.
- **Deterministic per build.** Pinned NEST, seeds everywhere, golden spike checksums (per build, not across compilers).
- **Say what is real.** Tuned numbers are called tuned, illustrative bodies illustrative, generated data generated, in the game and in the docs.
- **Small releases.** Each has a requirements doc with a status table, tests, screenshots and a CHANGELOG entry.

## What ships (0.5.0)
- **Campaign:** 79 levels in three wings. The **body** wing has five chapters on hand-inked atlas pages (Invertebrates, Spinal cord, Heart, Breathing, Autonomic): 20 micro-puzzles, 14 tasks, 2 bosses and 6 diagnosis cases, with challenge variants, diagnosis cases and two optional tasks (posture, reach) as branches. The **Bestiary** has six chapters (worm, leech, sea slug, fly compass, fly mushroom body, crab): 18 levels and 7 diagnosis cases. The **Theory** wing has four chapters (dynamics, rate models, attractor memory, tuning and decoding): 12 levels. The wings open after the chapter I spine; inside the Bestiary three chapters wait for an earlier level or chapter. Guided, Standard and Researcher modes; a three-tier hint ladder; a lab notebook of cited findings and a concept glossary; an interactive tutorial (Plate 0) and a mode picker on the first run.
- **Lab:** the bench, parts bin (NEST and NESTML cells), live raster, voltage traces, body views and gauges; instant headless Check; Test x5 over five seeds; a lesion lab with multi-seed re-runs and export; analysis (motifs, HRV, PRC, bursts, energy, spike-triggered average); an editor with undo and redo, zoom and pan, multi-select, copy and paste, keyboard and gamepad building, three solution slots and import from a passed level; an atomic save layer with bench history, backups and save export and import. Progressive disclosure: a first level shows Run / Pause, Reset, Check, Brief, More and the add-neuron palette; the rest is in More or opens with the level that needs it.
- **Honesty in the UI:** the reference circuit is hidden until a level is passed (or in Guided mode); the reference lock is a structural fingerprint computed by the game, not a flag in the file; the post-solve histograms say they are generated variants of the reference circuit, not players; a "What is real" page in the game.
- **Identity:** "Cajal Plate": paper, India ink, Golgi sienna for excitation, Prussian blue for inhibition; Fraunces, Atkinson Hyperlegible and IBM Plex Mono.
- **Shipping:** levels and catalogue inside the Godot project, a Linux export preset, `spike-run` bundled next to the extension, static GSL, Dockerfile, `scripts/build-all.sh`, `scripts/package.sh`. Desktop Linux only. Synthetic mock levels exist for development only (debug builds with `SP_MOCK`); release exports leave them out.

## Releases
| Ver | Theme | Status |
|---|---|---|
| 0.1 | **First circuit**: NEST embedded, circuit JSON, three tasks (reflex arc, lamprey swimmer, worm chemotaxis), editor and scopes, 3-node motif census, Python package with PyNEST parity | shipped |
| 0.2 | **Body systems**: five-chapter campaign of eleven tasks, body modules, NESTML cells, neuromodulators, parts bin, lesion lab | shipped |
| 0.3 | **Learning curve**: micro-puzzles, bosses, diagnosis cases, modes, hints, variants, notebook, sound, instant check | shipped |
| 0.4 | **Honest puzzles, shippable game** (docs/REQ-0.4.md): every level checks the mechanism it teaches through the circuit structure; verdicts and early hints carry no numbers; a table of cheap circuits that must fail; faster Check; verified citations; the UI honesty and cleanup items above; export and CI | shipped |
| 0.5 | **The Bestiary, the Theory wing and honest stars**: six model-organism chapters, four theory chapters, diagnosis scored on probes, the interaction-graph tests, the website and wiki, two enhancement waves (engine safety limits, cheat-proof scoring, atomic saves, a rewritten editor, the tutorial, wing gating, star rules that mean something) | shipped as 0.5.0 (see the CHANGELOG for known gaps) |
| 0.5.1 | Debts from 0.5.0: an h-current cell and a tested escape oscillator for the leech, stronger hints for the stretch reflex, wider margin for the spinal boss reference, parts cards exposing every parameter a reference sets, the Python package catching up with the Theory kinds | planned ([ROADMAP.md](ROADMAP.md)) |
| 0.6 | **The Atlas** (connectomics): real worm and fly wiring, locked-wiring levels where the player assigns signs and gains, prediction bets, null models, a wiring matrix and a state-space view | planned ([ROADMAP-CONNECTOME.md](ROADMAP-CONNECTOME.md)) |
| 0.7 | **Memory**: a rat on a track and in an arena; place and path integration, dentate and CA3, theta and sequences, replay across a day and a night, working memory | planned ([ROADMAP-MEMORY.md](ROADMAP-MEMORY.md)) |
| 0.8 | **Senses and movement**: retina, fly motion vision, barn-owl sound localisation, looming and escape, cerebellum, basal ganglia, the eye-position integrator, birdsong, motor cortex | planned ([ROADMAP-SENSORIMOTOR.md](ROADMAP-SENSORIMOTOR.md)) |
| later | A counterfactual mechanism check, phone layout, sharing circuits, real-player histograms (opt-in uploads), Windows build, a lighter embedded simulator validated against NEST | ideas |
