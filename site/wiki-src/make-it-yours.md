---
title: Make it yours
section: Under the hood
order: 83
summary: Add your own cell model with NESTML, or write a new level, from the task contract to the tests every level must pass.
---

# Make it yours

Spike is open source and built to be extended. There are two common ways to add to it: a new neuron model, and a new level. Both need a build from source (see [building from source](building-from-source.html)), because cells and levels are compiled into the engine. You cannot drop a new cell into a downloaded game.

If all you want is a different circuit, you do not need any of this. Build it in the [lab](how-to-read-the-lab.html) or write a [circuit file](circuit-is-data.html).

## Part 1: a custom cell with NESTML

NESTML is a domain language for writing neuron models. A `.nestml` file holds the equations in a few readable lines; a tool turns it into C++ that NEST can run. Spike uses NESTML 9.0.1 for its custom cells.

The four cells that exist today are in `models/nestml/`:

| Cell | What it stands for |
|---|---|
| `sa_node_pacemaker` | A sinoatrial (heart pacemaker) cell. One spike is one heartbeat. |
| `butera_burster` | A breathing-rhythm bursting neuron from the brainstem. |
| `icc_slow_wave` | A gut pacemaker cell. A simplified oscillator in slow-wave units. |
| `stg_cell` | A crab stomatogastric neuron with several ion currents. |

Each has a page in `docs/models/` that says where the equations come from and how far they can be trusted. See [what is real](what-is-real.html).

### The steps

1. **Write the model.** Add `models/nestml/my_cell.nestml`. Follow an existing file. The repository's cells share conventions: an `excitatory` and an `inhibitory` spike input port (so a positive circuit weight excites and a negative one inhibits), a continuous current port for `dc` inputs, and a parameter `I_e` for a constant current. Neuromodulator levels `ach` and `ne` are plain parameters; the [modulation table](circuit-is-data.html) drives them. NESTML 9 syntax has quirks: a spike port takes no unit (declare an internal `unit_psg nS` instead), and variable names `a` and `h` collide with unit names.
2. **Generate the C++.** Run `scripts/build-nestml.sh`. It makes a virtual environment with the pinned NESTML, then runs `scripts/nestml_generate.py` over every `.nestml` file in `models/nestml`, and writes C++ into `third_party/nestml-gen/`. The script patches around some incompatibilities between NESTML 9 and the pinned NEST (it fixes the NEST version label, replaces NESTML's own build step, and removes a `libltdl` guard the neuron code does not need). It also injects a step-count and minimum-step guard and a non-finite check into every cell's integration loop, so a strong input ends the run with an error and not a stall. The engine caps the weights and constant currents a custom cell accepts for the same reason (see [a circuit is data](circuit-is-data.html#limits-and-errors)); a new cell inherits those caps, and a circuit that needs more is refused with a message.
3. **Rebuild the engine.** `cmake -S . -B build && cmake --build build -j`. CMake globs the generated sources and compiles them into the engine. It warns if a `.nestml` file is newer than the generated code. The models register themselves after every NEST reset, in `engine/src/modules/nestml_models.cpp`.
4. **Add a parts-bin card.** The cards are defined in C++ in `engine/src/catalog/catalog.cpp`. A card gives the model's title, a family ("Custom (NESTML)"), a plain summary, real-world examples, a source line, the key parameters with units and ranges, the default input weight and its unit, whether it suits excitatory or inhibitory use, and the drive levels for its thumbnail. Copy the `stg_cell` card and edit it. Slow oscillators set `run_ms` and `max_ms` so the probe sees at least 10 cycles.
5. **Measure the thumbnails.** `build/engine/spike-catalog` runs every card in its own process and writes `catalog/thumbs.json` (the f-I curve and a voltage trace). The game reads that file for the card pictures.
6. **Python parity.** `scripts/build-nestml.sh --pynest` builds the loader library for PyNEST. `models/nestml/test_parity.py` compares the engine with PyNEST spike for spike; add your model to it. See [Use your circuit in Python](python.html).
7. **Write a test** next to the existing ones: `engine/tests/test_nestml_*.cpp` (for example `test_nestml_sa_node.cpp`). These tests skip, with exit code 77, when the NESTML code was not generated.
8. **Label it honestly.** Add it to `docs/WHAT-IS-REAL.md` as a NEST model, a NESTML re-fit or phenomenological, and say what was tuned. The project's rule is that nothing is called real unless it is. See [contributing](contributing.html).

> **Note:** A new cell appears in the sandbox parts bin once its card exists. Which levels offer it is controlled by the campaign: the lab unlocks the parts bin and cell types as the player reaches levels whose reference circuit uses them.

## Part 2: a new level

A level has a world, a score, a reference circuit and a lesson. In the code it is a **task**: one C++ file in `engine/src/tasks/`.

### The contract

The base class is `spike::Task` in `engine/include/spike/task.h`. The important pieces:

| Method | What it provides |
|---|---|
| `id()`, `title()`, `brief()` | The level name and the plain-language brief. The brief says what to build and how it is scored, but gives no numbers that give the answer away. |
| `sensor_channels()`, `motor_channels()` | The names of the inputs the task drives and outputs it reads. A circuit must declare them. |
| `channel_kinds()` | How each channel is declared (`dc`, `poisson`, `rate`, ...) and a default sensor weight, so the lab can build a correct empty bench. A level of the Theory wing may declare `rate_drive` or `tuned_poisson` inputs and `rate_value` outputs. |
| `duration_ms()`, `quick_duration_ms()` | The full episode, and a shorter one that Check uses by the same rules. |
| `reset(seed)`, `step(engine, dt_ms)` | The world. `reset` builds a deterministic world from the seed. `step` reads the world into engine inputs, steps the engine 1 ms, reads outputs, and advances the world. |
| `score()` | A value from 0 to 100, a pass flag, a verdict sentence and named parts. |
| `body()` and `view()` | What the game draws: a limb, heart, lungs, eye, gut, a dashboard or the generic polyline. See [bodies](bodies.html). |
| `chapter()`, `level_kind()`, `on_spine()` | Where it sits: a chapter id, `micro`, `task`, `boss` or `diagnosis`, and whether it gates progress. Branches never gate. |
| `concepts_new()`, `concepts_used()` | The ideas it teaches and relies on. These feed the glossary and the "know first" chips. |
| `par_neurons()`, `challenge()` | The neuron count for the par, and the optional goal for the third star (a neuron par, sometimes an energy ceiling or "no modulators"), shown as text on the level card. |
| `timeline()`, `bands()`, `modulators()` | Scripted events, target bands for the timeline plot and the neuromodulators the task reads. |
| `set_mode()`, `variants()`, `set_variant()` | Difficulty modes and challenge variants. A mode changes tolerance and noise, never structure. |
| `hints()` | Three tiers: the idea, the place (a population or channel id to outline), a partial recipe. |
| `recipe()` | The concrete numbers, kept apart from the brief so the lab can show them by mode. |
| `metrics()` | A quality number for the post-solve histograms. |
| `notebook()` | The cited fact unlocked on first pass. |
| `diagnosis()` | For diagnosis cases only: the circuit, symptoms, candidates and the answer. |

### Registering

End the file with the macro, with the id equal to the file name:

```cpp
SPIKE_REGISTER_TASK( my_level, 301, "my_level.json", make_my_level )
```

The number sorts the campaign, and the engine's registry order is the play order within a chapter. By habit the bands are about 10 to 30 for Invertebrates, 100s Spinal cord, 200s Heart, 300s Breathing, 400s Autonomic, 500s the Bestiary and 600s Theory, and a micro-puzzle takes a number just below the level it prepares. Which levels open when is decided in `godot/game/campaign.gd` (a wing opens after the Invertebrates spine, and a few chapters wait for a named level). CMake finds the file by globbing and generates the code that keeps its registration. The file stem must equal the id.

### The steps

1. **Write the task** in `engine/src/tasks/my_level.cpp`. Many levels use scaffolds: `micro_a.h` and `micro_b.h` for micro-puzzles, `spinal_limb.h` for the limb levels, `bestiary_*.h`, `theory.h`, and `dx_common.h` for diagnosis cases.
2. **Check the mechanism, not only the output.** The task asks the engine for the circuit's structure (`Engine::structure()`, `has_path`, `has_loop`, `has_loop_via`; see `docs/ENGINE-STRUCTURE.md`) and the verdict names the missing part. A circuit that hits the numbers by a cheap trick must fail.
3. **Build a reference circuit** that passes, and save it as `godot/circuits/my_level.json`. The repository's `circuits` folder is a link to it. Several circuits are generated by scripts in `circuits/tools/`.
4. **Write the cheap rows.** This is the most important test. In `engine/tests/cheap_rows_*.inc`, add rows that list circuits that must fail:

   ```cpp
   CHEAP_ROW( "my_level", "empty bench", cheap::empty( "my_level" ) );
   CHEAP_ROW( "my_level", "one cell, tonic 450 pA", cheap::tonic( "my_level", 450 ) );
   CHEAP_ROW( "my_level", "reference without the loop",
              cheap::from_ref( "my_level", cheap::cut( "MN", "RC" ) ) );
   CHEAP_ROW_V( "my_level", "no inhibition", "inhibitory", /* circuit */ );
   ```

   Every level has at least these: the empty bench, a single cell, a tonic cell with all weights 0, the reference with every weight 0, with every weight extreme, and the reference minus its mechanism. `CHEAP_ROW_V` also requires the verdict to contain a word, which pins that the verdict names the missing mechanism. A row passes when the task does not pass on seeds 1 and 2 and says something. The test also checks that the reference passes the same harness, so a broken harness cannot "fail" everything. A scan checks that verdicts and hint tiers contain no digits.
5. **Write the notebook entry and the glossary concept.** Cite real papers. The concept text lives in `godot/game/campaign.gd` (the `CONCEPTS` table). Run the citation checker (below).
6. **Put it on the map.** Engine order sets play order. `campaign.gd` has a `PLANNED` table of map positions per chapter (levels not listed are placed automatically) and `DX_PARENT` for diagnosis cases. If the third-star challenge should include an energy ceiling, add the level to `ENERGY_HZ` in `godot/game/progress.gd` (1.6 times what its reference circuit spends, from `rate_hz_per_neuron` in `spike-run --task <id>`); levels without an entry use 40 spikes per second per cell for the Analysis screen only, and earn nothing from it.
7. **Document it** in `docs/circuits/my_level.md` and `docs/WHAT-IS-REAL.md`.
8. **Run the tests**: the cheap rows, `tasks_circuits_test`, `tasks_quick_full_test` (the reference must pass the quick and the full episode in every mode), `spike-run --task my_level --seeds 1,2,3 --require all`, the smoke test, and the interaction-graph test if the UI changed.

### Making the rows

The cheap-row builders (`empty`, `skeleton`, `single_cell`, `tonic`, `from_ref`, `zero_weights`, `max_weights`) and the mutators (`cut`, `silence`, `scale_input`, `set_params`, `set_model`, `set_delay`, and so on) are documented at the top of `engine/tests/tasks_cheap_test.cpp`. A hand-built circuit can be made with the fluent `Bench` class. For tuning, `tasks_cheap_test run <task> <circuit.json> [seed]` and `tasks_circuits_test run <task> <circuit.json>` score one circuit and print the parts.

## The citation rule

Every reference shown in the game must be in `catalog/citations.json`, with a DOI where one exists. `python3 tools/citations/check.py` checks offline: registry sanity, references in the source that are missing from the registry, and known wrong strings. `--online` asks Crossref to confirm each DOI against the title, first author, year, volume and page. `--write-md` regenerates `docs/CITATIONS.md`. See [contributing](contributing.html).

## The interaction-graph rule

If your level changes the UI (a new screen, button or control), you must update `godot/tests/interaction_graph.json` in the same commit, run `tools/graph/render.py` to regenerate the diagram, and run `scripts/ui_graph_test.sh`. The test fails if a visible control is not listed. Details are in [architecture](architecture.html).

## Histograms

After a pass, the game shows "where you stand" against **generated variants of the reference circuit**, not other players. `tools/histograms/gen_histograms.py` makes the files for your level: it varies the reference, scores each variant on two seeds and keeps those that pass.

## Related pages

[Architecture](architecture.html), [contributing](contributing.html), [stars, par and the hint ladder](stars-and-hints.html), [what is real](what-is-real.html).
