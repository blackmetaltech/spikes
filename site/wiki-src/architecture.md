---
title: Architecture
section: Under the hood
order: 84
summary: How the game, the engine, NEST and the Python package fit together, and what happens in each millisecond of a run.
---

# Architecture

This page is a tour of how Spike is built, for people who want to read or change the code. You do not need it to play.

## The big picture

```
 NEST (pinned commit, linked statically)     the simulator: neuron and synapse models
   + NESTML cells compiled in                custom cells (heart, breathing, gut, crab)
        |
 engine/   C++20  ->  libspike
   circuit.cpp    circuit JSON  ->  NEST nodes and connections
   engine.cpp     the closed loop: set inputs, step NEST, read spikes
   tasks/         one file per level: world, score, hints, notebook
   body/          muscle, joint, heart, lungs, eye, gut (plain C++, no NEST)
   analysis/      motifs, bursts, HRV, PRC, phase plane, spike-triggered average
   catalog/       the parts-bin cards and their measured thumbnails
   spike_run.cpp  the command line (headless runner and scorer)
        |
 gdext/    GDExtension (godot-cpp)  ->  SpikeSim, SpikeTask
        |
 godot/    the game (Godot 4.7): code-built UI, levels in godot/circuits,
           parts catalogue data in godot/catalog

 python/   the spike package: the same circuit JSON in PyNEST, plus analysis
```

Each box has a job and little else. The engine knows nothing about Godot. The game knows nothing about NEST except through the engine. The Python package shares only the circuit file and the analysis maths.

## NEST, built in

NEST is the neural simulator behind most published spiking-network research. Spike pins one commit of it (`NEST_REF` in `scripts/build-nest.sh`) and builds it as static libraries, so the game is one self-contained program with no Python at run time. The game uses NEST's own models, names and units. That is the point of the project: when you wire two `iaf_psc_alpha` cells, a researcher gets the same behaviour from the same file in PyNEST. See [what is real](what-is-real.html).

Custom cells that NEST does not ship are written in NESTML and compiled in. See [make it yours](make-it-yours.html).

GSL (the GNU Scientific Library, which NEST uses to integrate some models) is linked statically. NEST is built without OpenMP by default, so it runs on one thread.

## The engine

`libspike` is the C++ library. The header `engine/include/spike/engine.h` is the contract shared by the game, the tasks and the tests. It is kept stable, and new features are only added.

What it does:

- **Loads a circuit.** `circuit.cpp` reads the JSON, validates every field (unknown keys, weight limits, synapse rules) and creates NEST nodes. See [a circuit is data](circuit-is-data.html).
- **Steps in a closed loop.** The engine advances NEST in slices and exchanges data with the world between slices.
- **Reports structure.** `Engine::structure()` lists populations and projections with their connection counts, mean weights and signs, read from the live NEST connections. `has_path`, `has_loop` and `has_loop_via` answer "is there a path from A to B?" and "is there a feedback loop through this group?". Tasks use these to check that a circuit uses the mechanism a level teaches. See `docs/ENGINE-STRUCTURE.md`.
- **Reads and sets.** Spikes (`drain_spikes`), output rates and counts, input levels, neuromodulator levels, single weights (this is what a lesion does) and cell state.

## The closed loop, millisecond by millisecond

A level is a conversation between the world and the circuit. Every 1 ms of model time the task does this:

1. **Read the world.** Where is the limb? How high is the blood pressure? What is the salt concentration at the worm's head? The task turns that into sensor levels.
2. **Set the inputs.** The task sets the level of each sensor channel in the engine (a current in pA or a rate in Hz).
3. **Step NEST.** The engine runs the simulator for that 1 ms. NEST itself uses a finer step, 0.1 ms in the game's circuits, so spike times are accurate to 0.1 ms.
4. **Read the spikes.** The engine counts spikes of each output channel in that slice.
5. **Move the world.** The task converts output rates into muscle drive, vagal tone, breathing effort or similar. The body model advances one step.
6. **Score.** The task updates its running score.

The loop is deterministic for a given NEST build, thread count and seed. The tests check it with golden spike checksums (a hash over every spike), per build rather than across compilers. The overhead of one engine step is measured at about 3 microseconds in the first release's notes.

The bodies are not NEST. The muscle, joint, heart and circulation, lungs, eye and gut are small deterministic C++ models in `engine/src/body/`, documented in `docs/body/`. See [bodies](bodies.html). The same state also feeds the picture the game draws.

## Tasks

Each level is one C++ file in `engine/src/tasks/` that registers itself with `SPIKE_REGISTER_TASK(id, order, circuit, factory)`. It defines the world, the scoring, the verdicts and the three-tier hints, the notebook entry, the challenge variants and the difficulty modes. The registry order is the campaign play order. See [make it yours](make-it-yours.html) for the contract and the tests every level must pass. There are 78 registered levels in this build.

## The GDExtension

`gdext/` is a Godot extension built with godot-cpp. It exposes two classes:

- `SpikeSim`: a circuit loaded into the engine, with run, step, spikes, inputs, modulators, analysis calls (bursts, HRV, PRC, motifs, spike-triggered average) and so on.
- `SpikeTask`: a level, as the game sees it: its brief, channels, timeline, bands, hints, notebook entry and scoring, plus headless scoring (it starts a `spike-run` child process in a worker thread).

The engine can host only one NEST instance per process. That is why the Check button runs `spike-run` as a separate process, and why seeds can run in parallel child processes.

## The game

`godot/` is a Godot 4.7 project. Almost all of the user interface is built in code, not in scenes.

| Folder | What is in it |
|---|---|
| `godot/main.gd` | The app shell: screens, back stack, layout, input. |
| `godot/game/` | Game logic with no drawing: the campaign (chapters, concepts, unlock rules), progress and stars, settings, circuits (load, save, edit, the reference fingerprint), the lesion helper, audio, the play log. |
| `godot/ui/` | The screens (title, campaign map, lab, parts bin, notebook, settings, analysis, what is real, credits), the lab's pieces (editor, raster, traces, gauges, body views, inspector, hint panel, lesion lab) and a small widget kit. |
| `godot/circuits/` | The reference circuits, one per level. |
| `godot/catalog/` | The citation registry, the card thumbnails and the generated histograms. |
| `godot/tests/` | The interaction graph and its test. |

Layouts adapt: desktop, handheld and phone. The look is the "Cajal Plate" identity (`docs/IDENTITY.md`): India ink and Golgi stain on paper, sienna for excitation and Prussian blue for inhibition, and a second cue (an arrowhead for excitatory and a bar for inhibitory) so colour is never the only signal. See [settings and performance](settings-and-performance.html).

## The Python package

`python/spike/` builds the same circuit in PyNEST and analyses spikes. It does not run tasks. Parity tests check that its spikes equal the engine's for the same circuit. See [Use your circuit in Python](python.html).

## Data flow of a Check

1. You press Check in the [lab](how-to-read-the-lab.html).
2. The game writes your circuit to a private temporary file and calls `SpikeTask.score_headless_quick_async`.
3. A worker thread starts `spike-run --task <id> --quick` with your circuit, mode and variant, pinned to the circuit seed you are watching.
4. `spike-run` loads the circuit into a fresh engine, runs the task's quick episode, and prints a score as JSON.
5. The game polls the job and shows the verdict. If the verdict asks for more strength while a weight is already at the end of its slider, the game swaps it for a note that more strength is not the answer.

The headless score and a live run agree because they use the same task code and the same seed rules. See [the core loop](core-loop.html) and [spike-run](spike-run.html).

## Tests that guard the structure

The engine tests include golden spike traces, the closed loop, body physics, the cheap-circuit tables, every task's reference circuit, and parity with PyNEST. The UI has a graph test that walks every screen and control. See [building from source](building-from-source.html).

> **Note:** The interaction-graph rule: any UI change updates `godot/tests/interaction_graph.json` in the same commit. A test starts the real game headless and presses every control, and fails if a visible control is not listed. This is why the game's controls are documented in one place. See [controls](controls.html).

## Related pages

[A circuit is data](circuit-is-data.html), [make it yours](make-it-yours.html), [building from source](building-from-source.html), [contributing](contributing.html).
