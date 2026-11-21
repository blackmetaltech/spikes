---
title: Building from source and CI
section: Under the hood
order: 85
summary: How to build the game, the engine and the Python tools from the repository, run the tests, make a release zip, and how continuous integration does it.
---

# Building from source and CI

Most players should download the release zip (see [getting started](getting-started.html)). Build from source if you want to change the game, add a level or a cell (see [make it yours](make-it-yours.html)), use PyNEST, or check the tests yourself.

## What you need

| Tool | Version | Why |
|---|---|---|
| C++ compiler | C++20 | The engine. |
| CMake | 3.24 or newer | The build system. |
| Ninja or Make | any | CMake's backend. |
| GSL | the GNU Scientific Library, with the **static** `libgsl.a` | NEST uses it; the engine links it statically. On Ubuntu, `libgsl-dev`. |
| Python 3 with venv | 3.10 or newer for the `spike` package | NESTML generation, tools, Python tests. |
| Godot | 4.7.2 | The game. Needed to run it and to export. |

Pinned versions, so that spikes are the same everywhere:

| Pin | Where | Value in this repository |
|---|---|---|
| NEST | `scripts/build-nest.sh` (`NEST_REF`) | a fixed commit, `7e9111711d4a...` |
| NESTML | `scripts/build-nestml.sh` (`NESTML_VERSION`) | 9.0.1 |
| godot-cpp | `scripts/build-gd.sh` (`GODOT_CPP_TAG`) | `godot-4.5-stable` |
| Godot | `Dockerfile` (`GODOT_VERSION`) | 4.7.2 |

The released Linux build needs glibc 2.39 or newer (Ubuntu 24.04 or newer). Windows is not built yet: the export preset exists, but NEST, GSL and godot-cpp must be cross-built and some POSIX-only code (`realpath`, `dladdr`, fork and exec in `gdext/src/bridge.cpp`) needs a Windows port.

## The easy way: Docker

On a clean Ubuntu 24.04 machine:

```sh
docker build -t spike-build .
docker run --rm -v "$PWD":/src spike-build scripts/build-all.sh --tests
docker run --rm -v "$PWD":/src spike-build scripts/package.sh
```

The image holds the toolchain: Godot, the pinned NEST (static and PyNEST), the NESTML venv and godot-cpp. A cold build takes about 1.5 hours, then it is cached. The second command builds the engine, the release extension, `spike-run` and the tests. The third writes `build/spike-linux-x86_64.zip`.

## Without Docker

`scripts/build-all.sh [--tests] [--debug] [--pynest]` runs these steps in order. Each is incremental.

1. **NEST.** `scripts/build-nest.sh` builds static libraries into `third_party/nest-build`. Add `--pynest` to also build and install NEST and PyNEST into `third_party/nest-install` (needs python3, cython and numpy). Use the same `SPIKE_OPENMP` value for both, so the engine and PyNEST agree. The default is off, which means one thread.
2. **NESTML cells.** `scripts/build-nestml.sh` makes a venv with the pinned NESTML and writes C++ into `third_party/nestml-gen`. Without it, Heart, Breathing and Peristalsis cannot load, because they use custom cells. Add `--pynest` for the PyNEST loader library.
3. **Engine and tests.** `cmake -S . -B build && cmake --build build -j`.
4. **The extension and runner.** `scripts/build-gd.sh` (debug) or `scripts/build-gd.sh release`. On first use it clones godot-cpp at the pinned tag. It copies `libspike_gd_<mode>.so` and `spike-run` to `godot/bin` (not tracked by git), where the extension looks for the runner that Check uses.

Then run the game:

```sh
godot --path godot
```

or open the `godot/` folder in the Godot editor.

> **Note:** `build-all.sh --tests` configures the engine tree with the tests on and the extension off. The extension is built in its own tree (`build/gd-release`, `build/gd-debug`) by `build-gd.sh`.

## Package a release

`scripts/package.sh` builds the release extension, exports the Linux preset from `godot/export_presets.cfg` (it needs the Godot 4.7.2 editor and its export templates), and writes `build/spike-linux-x86_64.zip`. The zip holds:

- `Spike.x86_64`, the game;
- `Spike.pck`, which carries `res://circuits` and `res://catalog`;
- `libspike_gd_release.so`, the extension;
- `spike-run`, the headless runner;
- loose `circuits/` and `catalog/` folders that `spike-run` and the histogram viewer read.

The script then unpacks the zip into an empty folder, scrubs the environment (no `spike-run` on `PATH`, no `SPIKE_*` variables), and runs the exported game headless: the full `SP_SMOKE` test, a test that Check scores a reference circuit through the bundled `spike-run`, and more. A release that cannot run on its own does not leave the script.

## Tests

```sh
ctest --test-dir build         # engine, tasks, goldens, UI graph, Python tests
scripts/smoke.sh               # headless Godot smoke test
scripts/ui_graph_test.sh       # walk the UI interaction graph
GOLDEN_UPDATE=1 build/engine/test_golden   # deliberately regenerate golden checksums
```

Each file in `engine/tests/*.cpp` becomes one ctest test. What the families check:

| Family | What it checks |
|---|---|
| `test_golden`, `test_determinism` | Golden spike checksums for fixed circuits (tied to the pinned NEST, the resolution and one thread); the same seed gives the same checksum, a different seed does not, and slicing a run does not change spikes. |
| `test_engine`, `test_circuit_spread`, `test_synapses`, `test_synapse_modulation`, `test_modulators`, `test_spike_inputs`, `test_population_vector`, `test_structure`, `test_theory` | The loader and the engine: weight spread, synapse models, neuromodulators, input kinds, structure queries, phase plane. |
| `test_body_*`, `tasks_physics_test` | The body models (muscle, joint, heart, lungs, eye, gut), and each task's world physics, with a stand-in engine and no NEST. |
| `test_nestml_*`, `test_nestml_parity` | The custom cells, and spike parity between the engine and PyNEST. They skip (exit code 77) when NESTML code or PyNEST is missing. |
| `tasks_circuits_test` | Every level's reference circuit passes on several seeds, and lesioned copies fail the way the biology predicts. |
| `tasks_cheap_test_a` to `_f` | The honesty tables: cheap circuits that must fail on every level (see [contributing](contributing.html)). One ctest entry per rows file, so none exceeds the time limit. |
| `tasks_*_test` (micro, spinal, heart, autonomic, boss and diagnosis, bestiary, fixes, gauges, modes) | Each chapter's levels: modes, hints, variants, notebook entries, diagnosis answers, gauges. |
| `test_instant` | The instant check: `spike-run --task` honours mode and variant, reports hints, is deterministic and is fast. |
| `test_catalog` | The parts-bin cards and their measured thumbnails. |
| `analysis_*_test` | Motifs, bursts, HRV, PRC, null models, and agreement with the Python versions. |
| `interaction_graph_static`, `ui_graph` | The UI graph's JSON and diagram are in sync; the headless game presses every control. `ui_graph` skips without Godot or the extension. |
| `python_pytest` | The Python package. Sets `SPIKE_REQUIRE_NEST=1`, so a missing PyNEST fails instead of skipping. |

The ctest time limit per test is 1500 s (1800 s for `ui_graph`). `scripts/ui_graph_test.sh` exports the game to a temporary folder and runs the exported binary, because the system Godot of some distributions cannot load the extension.

## Run the game's screenshots

`scripts/shots.sh` writes screenshots to `docs/screenshots/`.

## Continuous integration

Two GitHub Actions workflows in `.github/workflows/`:

- **`toolchain.yml`** builds the Docker image and pushes it to `ghcr.io/ixaxaar/spike-build:<tag>`. The tag is a hash of `Dockerfile`, `scripts/prebuilt.sh`, `build-nest.sh`, `build-nestml.sh` and `build-gd.sh` (`scripts/toolchain-tag.sh`). It runs when those files change on master, or by hand, and does nothing if the tag already exists.
- **`ci.yml`** pulls that image and runs the project steps inside it: build (engine, extension, tests, PyNEST), `ctest`, `pytest`, the headless smoke test, and packaging with a smoke test of the zip, then uploads the zip as an artifact. The run takes about 15 to 25 minutes, because NEST is already in the image.

Budget rules: pushes that touch only `docs/**`, `research/**` or Markdown files do not run CI; runs are on master only, and other branches run through their pull request; a newer push cancels an older run.

If you bump a pin, push, wait for the toolchain workflow, then re-run `ci` if it failed because the image was missing. `scripts/ci-local.sh [checkout-dir]` runs the same steps locally in the image.

## Related pages

[Architecture](architecture.html), [spike-run](spike-run.html), [Use your circuit in Python](python.html), [contributing](contributing.html), [the changelog](changelog.html).
