# Building, testing and the command line

## Build

Requirements: a C++20 compiler, CMake 3.24+, Ninja or Make, GSL (`libgsl-dev`, **static** `libgsl.a` is linked), Python 3 with venv, and for the game Godot 4.7.2.
Pinned: NEST commit (`scripts/build-nest.sh`), NESTML 9.0.1 (`scripts/build-nestml.sh`), godot-cpp `godot-4.5-stable` (`scripts/build-gd.sh`), Godot 4.7.2 (`Dockerfile`).

**One command, clean Ubuntu 24.04 (what CI does):**
```
docker build -t spike-build .
docker run --rm -v "$PWD":/src spike-build scripts/build-all.sh --tests   # NEST, NESTML, engine, release extension, spike-run, tests
docker run --rm -v "$PWD":/src spike-build scripts/package.sh             # build/spike-linux-x86_64.zip, smoke-tested
```

**Without Docker**, `scripts/build-all.sh [--tests] [--debug] [--pynest]` runs the steps below in order; each is incremental:

1. **NEST** (pinned commit, static libraries for the engine):
   ```
   scripts/build-nest.sh             # engine libs -> third_party/nest-build
   scripts/build-nest.sh --pynest    # also build and install NEST + PyNEST -> third_party/nest-install
   ```
   `--pynest` needs python3, cython and numpy. Use the same `SPIKE_OPENMP` value for both (default OFF) so the engine and PyNEST agree.
2. **NESTML models:** `scripts/build-nestml.sh` (venv with the pinned NESTML in `third_party/nestml-venv`). Without it Heart, Breathing and Peristalsis cannot load.
3. **Engine and tests:**
   ```
   cmake -S . -B build && cmake --build build -j
   ```
4. **GDExtension and runner:** `scripts/build-gd.sh` (debug) or `scripts/build-gd.sh release`. It clones godot-cpp at the pinned tag on first use and copies `libspike_gd_<mode>.so` **and `spike-run`** to `godot/bin` (git-ignored), where the extension looks for the runner behind CHECK. GSL is linked statically (`-DSPIKE_STATIC_GSL=OFF` for the shared library); the extension and `spike-run` have `$ORIGIN` rpath.

### Package a release

`scripts/package.sh` builds the release extension, exports the Linux preset from `godot/export_presets.cfg` (needs the Godot 4.7.2 editor and its export templates), and writes `build/spike-linux-x86_64.zip`: `Spike.x86_64`, `Spike.pck` (carries `res://circuits` and `res://catalog`), `libspike_gd_release.so`, `spike-run`, plus loose `circuits/` and `catalog/` that `spike-run` and the histogram viewer read. It then extracts the zip into an empty directory, scrubs the environment (no `spike-run` on `PATH`, no `SPIKE_*`), and runs the exported binary headless: the full `SP_SMOKE` test, `tests/check_export.gd` (CHECK scores a reference circuit through the bundled `spike-run`), and the bundled `spike-run` alone. It targets glibc 2.39 / Ubuntu 24.04 or newer; build in an older image to run on older systems.

### Windows

Not built yet. The export preset ("Windows x86_64") and the `.gdextension` entries exist; what is missing is the binaries: NEST, GSL (static) and godot-cpp must be cross-built (MinGW-w64 on the Ubuntu image, or a `windows-latest` CI runner with MSYS2), and the POSIX parts of the headless runner (`realpath`, `dladdr`, fork/exec in `gdext/src/bridge.cpp`, `/` path handling in `spike_run.cpp`) need Windows ports. Until then `scripts/package.sh` is Linux only.

## Run the game

```
godot --path godot
```
(or open `godot/` in the Godot editor). Choose Play, pick a level on the campaign map and press Build: you get an empty bench with the level's sensor and motor cards. Add a neuron, drag from a sensor card to it and from it to a motor card, set the weights, and press Run or Check. The lab starts with only the essentials (Run / Pause, Reset, Check, Brief, the add-neuron palette and Connect); the speed, difficulty, Test x5, lesions, analysis and files are in the More menu, and some tools appear only once the campaign reaches a level that needs them. The reference circuit opens after you pass a level, or at any time in Guided mode.

The look is the "Cajal Plate" identity ([docs/IDENTITY.md](docs/IDENTITY.md)): ink and Golgi stain on paper. Screenshots: `scripts/shots.sh` writes them to `docs/screenshots/`.

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

`build/engine/spike-run --task <id> [--circuit file.json] [--seed n | --seeds 1,2,3] [--lesion pre>post*k] [--require any|all]` scores a circuit on a task headless, with the same loop and seeding as the game (a seed also reseeds the circuit), and prints the score JSON. Without `--circuit` it uses the task's reference circuit (found through `$SPIKE_CIRCUITS_DIR`, or a `circuits/` or `godot/circuits/` folder above the working directory or the executable). `--lesion` scales every projection `pre`->`post` by `k` (0 deletes it). `spike-run --list-tasks` prints the task ids. The exit status is 3 when `--require` fails, which is how CI and `scripts/smoke.sh` check that every reference circuit passes.

## Custom cells and the parts bin
- `scripts/build-nestml.sh [--pynest]` generates the NESTML cells (`models/nestml/*.nestml`: SA-node pacemaker, Butera burster, ICC slow wave) into `third_party/nestml-gen/`, which CMake compiles into libspike. Without it those models are unavailable and their tests skip; a circuit that uses them then fails to load. See `docs/models/`. For PyNEST, build with `--pynest` too.
- `build/engine/spike-catalog` probes every parts-bin model and writes `catalog/thumbs.json` (f–I points and V_m traces) for the game's model cards.
- Body physics (muscle, joint, heart, lungs, eye, gut) is documented in `docs/body/`.
- New tasks register themselves: add `engine/src/tasks/<id>.cpp` ending with `SPIKE_REGISTER_TASK( <id>, <order>, "<circuit>.json", make_<id> )`; the file stem must equal the id.
