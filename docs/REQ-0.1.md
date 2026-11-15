# v0.1 requirements: First circuit (on NEST)

**Ground rules (all agents)**
- The contracts are `engine/include/spike/engine.h`, `engine/include/spike/task.h` and `docs/CIRCUIT-FORMAT.md`. Don't change a signature; add new things instead. If a contract is wrong, say so in your report.
- NEST is the simulator: use its models and parameter names, and never re-implement neuron dynamics.
  - Build it with `scripts/build-nest.sh`. The static libs land in `third_party/nest-build/{models,nestkernel,libnestutil}/*.a`. Headers are in `third_party/nest-simulator/{nestkernel,libnestutil,models}` plus the generated headers in `third_party/nest-build/{nestkernel,libnestutil}`.
  - Link with `-Wl,--start-group <libs> -Wl,--end-group -lgsl -lgslcblas`.
  - `engine/tests/feasibility_loop.cpp` is a working example. Note that `slice_nc` is 1-based, and recorder parameters are locked between `prepare` and `cleanup`.
- Use real units and cite sources for the reference circuits' parameters.
- Do not commit; the lead reviews and commits. Stay inside your own directories.
- Keep these green: `cmake --build build && ctest --test-dir build`, `pytest python`, and `scripts/smoke.sh`.

| Item | Owner | Status |
|---|---|---|
| A. Engine: libspike, ring-buffer backend, circuit loader, golden traces | engine agent | Not started |
| B. Tasks: reflex, lamprey, worm, with reference circuits | tasks agent | Not started |
| C. Analysis: C++ motif census and stats, plus the Python package and PyNEST parity | analysis agent | Not started |
| D. Game: godot-cpp extension and Godot UI (editor, scopes, task view) | game agent | Not started |
| E. Release | lead | Not started |

## A. Engine (`engine/src`, `engine/CMakeLists.txt`, top-level `CMakeLists.txt`, `engine/tests`)
- Implement `engine.h` on the NEST C++ API.
- **Ring-buffer backend:** a custom `nest::RecordingBackend` registered under the name `"spike_ring"`. It pushes `(node_id, step)` into a lock-free single-producer ring buffer. Every neuron in the circuit connects to one spike_recorder with `record_to: spike_ring`.
- **Circuit loader:** nlohmann/json, header-only and vendored in `third_party/` or fetched by CMake. It validates against CIRCUIT-FORMAT.md and gives a clear error per field.
- **Inputs and outputs:** inputs are devices whose amplitude or rate changes through `set_node_status` between steps. Outputs give the spike count during the last `step` divided by the step length.
- **Voltage:** watching a neuron attaches a multimeter recording `V_m` into a small ring of its own.
- **Tests:**
  - load, error paths and closed loop;
  - determinism: the same seed gives the same checksum twice, and a different seed gives a different one;
  - golden traces in `engine/tests/golden/*.json`, holding a circuit file, the ms to run and the expected checksum and spike count. Regenerate them with `GOLDEN_UPDATE=1`. Cover at least: one `iaf_psc_alpha` under DC, a balanced E/I 1k network, and an `aeif_cond_alpha` half-centre pair;
  - a benchmark giving the closed-loop overhead per 1 ms slice for the 1k network. It must be under 50 µs when it isn't simulating.
- Expose the library as CMake target `spike::engine`, built position-independent so the GDExtension can link it.

## B. Tasks (`engine/src/tasks/*`, `circuits/*.json`, `engine/tests/tasks_*`)
Implement `task.h` for these three tasks. Each one ships a reference circuit in `circuits/` that passes it, and a test that runs it and checks the pass. The world physics is simple, deterministic and documented in the source header.
1. **`reflex`:** a touch sensor (Poisson input whose rate is driven by touch events at seeded random times) drives a withdrawal motor channel. Score: latency from touch to motor burst, misses, and false triggers under background noise.
2. **`lamprey`:** two half-centres, each with mutual inhibition, plus a tonic drive input.
   - Body: about 10 segments. Each segment's bend angle follows (left − right) motor rate with a phase lag down the chain.
   - Thrust follows a simplified reactive (Lighthill-style) model of the travelling wave.
   - Score: frequency in 1–10 Hz, left/right phase near 180° and forward speed.
3. **`worm`:** a 2D plate with a Gaussian salt field, sensed at the head.
   - Sensors: ASEL is on-sensitive (fires when concentration rises) and ASER is off-sensitive (fires when it falls); both are derivative sensors.
   - Motors: `turn_left` and `turn_right` rates set the angular velocity, with constant forward speed.
   - Score: time to reach the peak and mean distance to it. Pass if it gets within 10% of the plate radius in time.
- `field()` returns the salt concentration so the game can draw the gradient.

## C. Analysis (`engine/src/analysis/*`, `engine/include/spike/analysis.h`, `python/`)
- **C++:**
  - Motif census: all 13 connected directed 3-node motifs, numbered as in Milo et al. 2002. Run it on the circuit's connection graph (from NEST `get_connections`), with an optional weight sign (E or I).
  - Null model: degree-preserving rewiring with a seed, giving z-scores.
  - Named labels: feed-forward loop, feedback loop, mutual (reciprocal) pair, fan-in, fan-out.
  - Stats: per-neuron firing rate, ISI CV, a population synchrony index (Golomb's χ), PSTH.
- **Python** (`python/spike/`, a pyproject package, with PyNEST taken from the build or installed):
  - `spike.build(circuit_json)` builds the identical network in PyNEST.
  - `spike.run`, `spike.motifs`, `spike.stats`.
  - A parity test runs the same circuit in the C++ engine (via a small CLI `spike-run` built by CMake that prints spikes as JSON) and in PyNEST with the same seed and thread count, and asserts identical spikes.
  - A notebook example.

## D. Game (`gdext/`, `godot/`, `scripts/smoke.sh`, `scripts/shots.sh`)
- **Extension:** a godot-cpp GDExtension (godot-cpp branch matching Godot 4.7) linking `spike::engine`. Class `SpikeSim`:
  - `load_circuit(json) -> bool`, `error()`;
  - `step(ms)`, `set_input(name, v)`;
  - `drain_spikes() -> PackedFloat64Array`, packed as neuron and t pairs;
  - `voltage(i)`, `watch(i)`, `output_rate(name)`;
  - `neuron_count()`, `stats() -> Dictionary`.
  - Class `SpikeTask` wraps `spike::Task`: `make(id)`, `reset(seed)`, `step(sim, ms)`, `body() -> Dictionary`, `score() -> Dictionary`, `brief()`, `field(x, y)`, `bounds()`.
- **Godot:** the shell already in `godot/` (copied from Mugshot: Kit, Layout, Settings, Pad, Touch). Keep it with code-built UI and `SP_*` env hooks.
- **Screens:**
  - **Title.**
  - **Tasks:** cards with a brief.
  - **Lab:**
    - a circuit editor: add a neuron from a NEST model list (iaf_psc_alpha, aeif_cond_alpha, izhikevich, hh_psc_alpha) with presets, drag to connect, and an edge inspector for weight, delay and synapse model;
    - a live task view drawing the body and the field;
    - a scope dock with a scrolling raster, 1–4 voltage traces and output-rate gauges;
    - run, pause and speed controls;
    - a score card;
    - load or save the circuit JSON, and load the reference circuit.
  - **Analysis:** motif census bars with z-scores, and stats.
  - **Settings, Credits.**
- **Look:** a dark lab bench.
  - Neurons are glowing discs that flash on a spike; excitatory ones are amber and inhibitory ones cyan.
  - Axons are curved, with pulses travelling along them at the synaptic delay.
  - Bodies are clean vector shapes (worm, lamprey). No human figures.
- **Smoke:** builds the extension, boots every screen headless, loads each reference circuit, runs each task for 1 s, and fails on any SCRIPT ERROR.
- **Screenshots:** taken at 1920×1080 and 844×390 into `docs/screenshots/`. Look at them.
