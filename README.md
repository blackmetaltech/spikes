# Spike

Spike is a game about building small spiking neural circuits and watching them behave. You wire neurons together in an editor, drive them from sensors, read motor outputs, and a task scores what the circuit does. Version 0.2 has eleven tasks in five chapters, from a reflex arc and a swimming lamprey to a heartbeat, breathing and the fight-or-flight response. The simulation is real: the [NEST](https://www.nest-simulator.org/) kernel runs inside the game (C++ API, no Python at runtime), with NEST model and parameter names throughout (mV, ms, pA, nS, Hz).

A circuit is one JSON file ([docs/CIRCUIT-FORMAT.md](docs/CIRCUIT-FORMAT.md)). The game, the C++ engine and the Python package read the same file, so a circuit built in the game runs unchanged in PyNEST, with identical spikes (the parity test checks this).

## Layout

| Path | What |
|---|---|
| `engine/` | C++ engine (`libspike`): loads a circuit, closed-loop `step()`, tasks, body physics, parts catalogue, analysis (motifs, bursts, HRV, PRC), `spike-run` CLI, tests |
| `gdext/` | Godot GDExtension (`SpikeSim`, `SpikeTask`) over the engine |
| `godot/` | The game (Godot 4.7; the extension builds against godot-cpp 4.5 and declares compatibility from 4.5) |
| `python/` | `spike` package: build circuits in PyNEST, motif census, spike statistics ([python/README.md](python/README.md)) |
| `circuits/`, `docs/` | Example circuits, format spec, plan, requirements |
| `scripts/` | `build-nest.sh`, `build-gd.sh`, `smoke.sh`, `shots.sh` |

## Build

Requirements: a C++20 compiler, CMake 3.24+, GSL, and (for the game) Godot 4.7 and scons for godot-cpp.

1. **NEST** (pinned commit, static libraries for the engine):
   ```
   scripts/build-nest.sh             # engine libs -> third_party/nest-build
   scripts/build-nest.sh --pynest    # also build and install NEST + PyNEST -> third_party/nest-install
   ```
   `--pynest` needs python3, cython and numpy. Use the same `SPIKE_OPENMP` value for both (default OFF) so the engine and PyNEST agree.
2. **Engine and tests:**
   ```
   cmake -S . -B build && cmake --build build -j
   ```
3. **GDExtension:** `scripts/build-gd.sh` (debug) or `scripts/build-gd.sh release`. It clones godot-cpp on first use and copies the library to `godot/bin`.

## Run the game

```
godot --path godot
```
(or open `godot/` in the Godot editor). Pick a task, press BUILD, add neurons, wire the sensors and motors, and watch the score ring.

## Tests

```
ctest --test-dir build            # engine, tasks, goldens, and pytest for python/
scripts/smoke.sh                  # headless Godot smoke test
GOLDEN_UPDATE=1 build/engine/test_golden   # deliberately regenerate golden checksums
```
The `python_pytest` ctest sets `SPIKE_REQUIRE_NEST=1`, so a missing PyNEST fails instead of skipping. Plain `pytest python` skips the PyNEST tests when it is absent.

## Python quick start

```
pip install -e python[test]
export SPIKE_PYNEST=$PWD/third_party/nest-install/lib/python3.*/site-packages   # if not found automatically
```
```python
import spike
net = spike.build("python/examples/ei_net.json")
sp = spike.run(net, 1000.0)
```
See [python/README.md](python/README.md).

## Command line

`build/engine/spike-run circuit.json 1000` prints one JSON line per spike.

`build/engine/spike-run --task <id> [--circuit file.json] [--seed n | --seeds 1,2,3] [--lesion pre>post*k] [--require any|all]` scores a circuit on a task headless, with the same loop and seeding as the game (a seed also reseeds the circuit), and prints the score JSON. Without `--circuit` it uses the task's reference circuit (found through `$SPIKE_CIRCUITS_DIR`, or a `circuits/` folder above the working directory). `--lesion` scales every projection `pre`->`post` by `k` (0 deletes it). `spike-run --list-tasks` prints the task ids. The exit status is 3 when `--require` fails, which is how CI and `scripts/smoke.sh` check that every reference circuit passes.

## The campaign (0.2)

| Chapter | Task | The circuit and what scores it |
|---|---|---|
| Invertebrates | `reflex` | touch reflex: detect a poke through noise |
| | `worm` | salt-seeking worm: steer by the difference between two sensory neurons |
| | `lamprey` | swimming central pattern generator |
| Spinal cord | `stretch_reflex` | Ia afferent to motor neuron, resist an imposed stretch |
| | `renshaw` | recurrent inhibition steadies the motor output |
| | `posture` | hold a joint upright under gravity and pushes |
| | `reach` | triphasic agonist/antagonist burst to a target angle |
| Heart | `heart_pacemaker` | SA-node cells: a stable 60-80/min and synchrony |
| | `heart_autonomic` | vagal (ACh) and sympathetic (NE) release track a target heart rate |
| | `baroreflex` | hold arterial pressure through standing up and a haemorrhage |
| Breathing | `breathing` | Butera burster network drives the lungs, CO2 raises ventilation |
| Autonomic | `pupil` | light reflex with the right latency, both directions |
| | `peristalsis` | an enteric circuit moves a bolus along the gut |
| | `fight_or_flight` | a threat cue steers heart, pressure and pupil, then recovery |

Chapters open when half of the previous one is passed. Stars: 1 pass; 2 pass on all five TEST seeds (the lab reseeds world and circuit and records the mean); 3 the same within the task's neuron par and energy budget (spikes per second per neuron). An edited copy of a reference circuit earns no stars.

**Body modules** (`docs/body/`): Hill-type muscle, single-joint limb, heart with Windkessel circulation and baroreceptors, lungs with chemoreception, eye and pupil, gut chain.

**Modulators:** NE, ACh and DA are engine channels. NESTML cells follow them by default, any population can follow them through the circuit's `modulation` table, and an output of kind `modulator` releases them from spikes (`docs/CIRCUIT-FORMAT.md`). The sandbox has dials.

**Analysis in the game:** 3- and 4-node motif census, spike statistics, HRV, phase-response curves, bursts and energy. The same analyses are in the Python package.

## Custom cells and the parts bin (0.2)
- `scripts/build-nestml.sh [--pynest]` generates the NESTML cells (`models/nestml/*.nestml`: SA-node pacemaker, Butera burster, ICC slow wave) into `third_party/nestml-gen/`, which CMake compiles into libspike. Without it those models are unavailable and their tests skip; a circuit that uses them then fails to load. See `docs/models/`. For PyNEST, build with `--pynest` too.
- `build/engine/spike-catalog` probes every parts-bin model and writes `catalog/thumbs.json` (f–I points and V_m traces) for the game's model cards.
- Body physics (muscle, joint, heart, lungs, eye, gut) is documented in `docs/body/`.
- New tasks register themselves: add `engine/src/tasks/<id>.cpp` ending with `SPIKE_REGISTER_TASK( <id>, <order>, "<circuit>.json", make_<id> )`; the file stem must equal the id.
