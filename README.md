# Spike

Spike is a game about building small spiking neural circuits and watching them behave. You wire neurons together in an editor, drive them from sensors, read motor outputs, and a task (a reflex arc, a worm that must find food, a lamprey swimming central pattern generator) scores what the circuit does. The simulation is real: the [NEST](https://www.nest-simulator.org/) kernel runs inside the game (C++ API, no Python at runtime), with NEST model and parameter names throughout (mV, ms, pA, nS, Hz).

A circuit is one JSON file ([docs/CIRCUIT-FORMAT.md](docs/CIRCUIT-FORMAT.md)). The game, the C++ engine and the Python package read the same file, so a circuit built in the game runs unchanged in PyNEST, with identical spikes (the parity test checks this).

## Layout

| Path | What |
|---|---|
| `engine/` | C++ engine (`libspike`): loads a circuit, closed-loop `step()`, tasks, motif analysis, `spike-run` CLI, tests |
| `gdext/` | Godot GDExtension (`SpikeSim`, `SpikeTask`) over the engine |
| `godot/` | The game (Godot 4.5+) |
| `python/` | `spike` package: build circuits in PyNEST, motif census, spike statistics ([python/README.md](python/README.md)) |
| `circuits/`, `docs/` | Example circuits, format spec, plan, requirements |
| `scripts/` | `build-nest.sh`, `build-gd.sh`, `smoke.sh`, `shots.sh` |

## Build

Requirements: a C++20 compiler, CMake 3.24+, GSL, and (for the game) Godot 4.5+ and scons for godot-cpp.

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
