---
title: Building from source and CI
section: Under the hood
order: 85
summary: Build the game, engine and Python tools from the repository, run the tests, make a release zip, and see how CI does it.
---

# Building from source and CI

Most players should download the release zip (see [getting started](getting-started.html)). Build from source to change the game, add a level or a cell (see [make it yours](make-it-yours.html)), use PyNEST, or check the tests yourself.

## What you need

| Tool | Version | Why |
|---|---|---|
| C++ compiler | C++20 | The engine. |
| CMake | 3.24 or newer | The build system. |
| Ninja or Make | any | CMake's backend. |
| GSL | the GNU Scientific Library, with the **static** `libgsl.a` | NEST uses it. The engine links it statically. On Ubuntu, `libgsl-dev`. |
| Python 3 with venv | 3.10 or newer for the `spike` package | NESTML generation, tools, Python tests. |
| Godot | 4.7.2 | The game. Needed to run it and to export. |

Pinned versions, so spikes are the same everywhere:

| Pin | Where | Value in this repository |
|---|---|---|
| NEST | `scripts/build-nest.sh` (`NEST_REF`) | a fixed commit, `7e9111711d4a...` |
| NESTML | `scripts/build-nestml.sh` (`NESTML_VERSION`) | 9.0.1 |
| godot-cpp | `scripts/build-gd.sh` (`GODOT_CPP_TAG`) | `godot-4.5-stable` |
| Godot | `Dockerfile` (`GODOT_VERSION`) | 4.7.2 |

The released Linux build needs glibc 2.39 or newer (Ubuntu 24.04 or newer). macOS (Apple Silicon) and Windows (x86_64) are built on GitHub runners with `scripts/build-macos.sh` and `scripts/build-windows.sh`. [docs/BUILD.md](../../docs/BUILD.md) has the details.

## The easy way: Docker

On a clean Ubuntu 24.04 machine:

```sh
docker build -t spike-build .
docker run --rm -v "$PWD":/src spike-build scripts/build-all.sh --tests
docker run --rm -v "$PWD":/src spike-build scripts/package.sh
```

The image holds the toolchain: Godot, the pinned NEST (static and PyNEST), the NESTML venv and godot-cpp. A cold build takes about 1.5 hours, then it is cached. The second command builds the release extension, `spike-run`, the engine and the tests. The third writes `build/spike-linux-x86_64.zip`. CI publishes the same image to `ghcr.io/blackmetaltech/spike-build`, so you can pull it instead (see below).

## Without Docker

`scripts/build-all.sh [--tests] [--debug] [--pynest]` runs these steps in order. Each is incremental.

1. **NEST.** `scripts/build-nest.sh` builds static libraries into `third_party/nest-build`. Add `--pynest` to also build and install NEST and PyNEST into `third_party/nest-install` (needs python3, cython and numpy). Use the same `SPIKE_OPENMP` value for both, so the engine and PyNEST agree. The default is off, which means one thread.
2. **NESTML cells.** `scripts/build-nestml.sh` makes a venv with the pinned NESTML and writes C++ into `third_party/nestml-gen`. Without it, every level that uses a custom cell (the Heart, Breathing, Peristalsis and crab levels, and some micro-puzzles) cannot load. Add `--pynest` for the PyNEST loader library.
3. **The extension and runner.** `scripts/build-gd.sh release` (or `debug`). It builds the engine in its own tree (`build/gd-release`) and, on first use, clones godot-cpp at the pinned tag. It copies `libspike_gd_<mode>.so` and `spike-run` to `godot/bin` (not tracked by git), where the extension looks for the runner that Check uses. `build-all.sh` always builds the release one, and adds the debug one with `--debug`.
4. **Engine tests.** With `--tests`, `build-all.sh` runs `cmake -S . -B build -DSPIKE_BUILD_TESTS=ON -DSPIKE_BUILD_GDEXT=OFF` and builds the whole engine tree: the tests, `spike-catalog` and a second `spike-run` at `build/engine/spike-run`. You can run the same by hand: `cmake -S . -B build && cmake --build build -j`.

Then run the game:

```sh
godot --path godot
```

or open the `godot/` folder in the Godot editor.

> **Note:** `build-gd.sh` builds the extension in its own tree (`build/gd-release`, `build/gd-debug`). The engine tests are in `build/`.

## Package a release

`scripts/package.sh` builds the release extension, exports the Linux preset from `godot/export_presets.cfg` (it needs the Godot 4.7.2 editor and its export templates), and writes `build/spike-linux-x86_64.zip`. The zip holds:

- `Spike.x86_64`, the game;
- `Spike.pck`, which carries `res://circuits` and `res://catalog`;
- `libspike_gd_release.so`, the extension;
- `spike-run`, the headless runner;
- loose `circuits/` and `catalog/` folders that `spike-run` and the histogram viewer read;
- the repository `README.md`.

The script bakes the git commit into the build (the build id the title screen shows, with `-dirty` if the tree has changes). It refuses a zip in which `Spike.x86_64` or `spike-run` has lost its executable bit. It then unpacks the zip into an empty folder, scrubs the environment (no `spike-run` on `PATH`, no `SPIKE_*` variables), and runs the exported game headless: the full `SP_SMOKE` test, a test that Check scores a reference circuit through the bundled `spike-run`, and a run of the bundled `spike-run` on its own (`--list-tasks` and the reflex reference). It also checks that nothing links GSL dynamically. A release that cannot run on its own does not leave the script.

## Tests

```sh
ctest --test-dir build --output-on-failure -j"$(nproc)"   # everything below
scripts/smoke.sh               # headless Godot smoke test (CI and package.sh run it; it is not a ctest entry)
scripts/ui_graph_test.sh       # walk the UI interaction graph
GOLDEN_UPDATE=1 build/engine/test_golden   # deliberately regenerate golden checksums
```

Each file in `engine/tests/*.cpp` becomes one ctest test named after the file, except the two that are split. A test that needs something missing exits with 77, which ctest reports as skipped, not passed. What the families check:

| Family | What it checks |
|---|---|
| `test_golden`, `test_determinism` | Golden spike checksums for fixed circuits (tied to the pinned NEST, the resolution and one thread). The same seed gives the same checksum, a different seed does not, and slicing a run does not change spikes. |
| `test_engine`, `test_circuit_spread`, `test_synapses`, `test_synapse_modulation`, `test_modulators`, `test_spike_inputs`, `test_population_vector`, `test_structure`, `test_theory` | The loader and the engine: weight spread, synapse models, neuromodulators, input kinds, structure queries, phase plane, the Theory wing's rate units. |
| `test_hardening`, `fuzz_circuits`, `evidence_circuits` | Circuit loading and its limits: sizes, budgets, weight ceilings, error messages. `fuzz_circuits` feeds `spike-run` a corpus of about 230 hostile circuits from `engine/tests/fuzz` and requires each to end with status 0 or 1 inside a time and memory limit. `evidence_circuits` replays circuits from `engine/tests/fixtures` that once broke something. |
| `test_body_*`, `tasks_physics_test` | The body models (muscle, joint, heart, lungs, eye, gut), and each task's world physics, with a stand-in engine and no NEST. |
| `test_nestml_sa_node`, `_butera`, `_icc`, `test_nestml_parity` | The custom cells, and spike parity between the engine and PyNEST. They skip (exit code 77) when NESTML code or PyNEST is missing. |
| `tasks_circuits_test` | Every level's reference circuit passes on several seeds, and lesioned copies fail the way the biology predicts. |
| `tasks_cheap_test_a` to `_f` | The honesty tables: cheap circuits that must fail on every level (see [contributing](contributing.html)). One ctest entry per rows file, so none exceeds the time limit. |
| `tasks_quick_full_test_0` to `_3` | The quick and the full episode of every reference circuit, over several seeds in each mode, split in four so each part fits the time limit. |
| `tasks_*_test` (micro, spinal, heart, autonomic, boss and diagnosis, bestiary, theory, fixes, gauges, modes, honest, novice, diagnosis order, dx fixes, physics) | Each chapter's levels: modes, hints, variants, notebook entries, diagnosis answers and probes, gauges, and a scripted novice that may use only the brief and the verdicts. |
| `test_instant` | The instant check: `spike-run --task` honours mode and variant, reports hints, is deterministic and is fast. |
| `test_catalog` | The parts-bin cards and their measured thumbnails. |
| `analysis_*_test` | Motifs, bursts, HRV, PRC, null models, and agreement with the Python versions. |
| `interaction_graph_static`, `ui_graph` | The UI graph's JSON and diagram are in sync. The headless game presses every control. `ui_graph` skips without Godot or the extension. |
| `save_io` | The save layer (`godot/tests/test_save.gd`): atomic writes, backups, damaged, truncated or future saves, hostile circuit files, export and import, a read-only folder. Needs only Godot. |
| `godot_editor`, `tutorial`, `godot_polish` | The circuit editor (undo and redo, a large circuit's layout, clipboard, keyboard cursor), the first-run logic of Plate 0, and the lab's chrome logic. Need only Godot. |
| `art_specimens` | The lab's specimen layers match the plate drawings they are cut from. Skips without pycairo. |
| `python_pytest` | The Python package. Sets `SPIKE_REQUIRE_NEST=1`, so a missing PyNEST fails instead of skipping. |

The ctest time limit per test is 1500 s by default (3000 s for `tasks_quick_full_test`, 1800 s for `ui_graph`, less for the small Godot tests). The five tests that open the Godot project (`ui_graph`, `save_io`, `godot_editor`, `tutorial`, `godot_polish`) share a resource lock, so they run one after another even with `ctest -j`. `scripts/ui_graph_test.sh` exports the game to a temporary folder and runs the exported binary, because the system Godot of some distributions cannot load the extension.

## Screenshots and website media

`scripts/shots.sh` writes screenshots to `docs/screenshots/`. `scripts/site-media.sh` makes the website's stills and clips, and the wiki's figures (`site/wiki-src/img/`), from the exported game. It also needs `xvfb-run`, ffmpeg and ImageMagick. The website's derived assets (plates, fonts, the paper texture) come from `tools/site/assets.sh`.

## Continuous integration

Three GitHub Actions workflows in `.github/workflows/`:

- **`toolchain.yml`** builds the Docker image and pushes it to `ghcr.io/blackmetaltech/spike-build:<tag>`. The tag is a hash of `Dockerfile`, `scripts/prebuilt.sh`, `build-nest.sh`, `build-nestml.sh` and `build-gd.sh` (`scripts/toolchain-tag.sh`). It runs when those files change on master, or by hand, and does nothing if the tag already exists.
- **`ci.yml`** pulls that image and runs the project steps inside it: `build-all.sh --tests --debug --pynest` (engine, release and debug extension, tests, PyNEST), `ctest`, `pytest`, the headless smoke test, and packaging with a smoke test of the zip. It then uploads the zip as an artifact. A run takes about 15 to 25 minutes, because NEST is already in the image. If the image for the current pins has not been published, the job fails at once with instructions to run the toolchain workflow and re-run.
- **`pages.yml`** builds and deploys the website and this wiki to GitHub Pages. It runs `python3 tools/site/build.py --check` (which fails on a broken link, anchor or image) and publishes `site/`. It runs on pushes to master that touch `site/`, `tools/site/` or the workflow. See [architecture](architecture.html#the-website-and-this-wiki).

Budget rules: pushes that touch only `docs/**`, `research/**`, `site/**`, `tools/site/**` or Markdown files do not run CI. Runs are on master only, and other branches run through their pull request. A newer push cancels an older run.

If you bump a pin, push, wait for the toolchain workflow, then re-run `ci` if it failed because the image was missing. `scripts/ci-local.sh [checkout-dir]` runs the same steps locally in the image. Use a clean checkout for a faithful run.

## Related pages

[Architecture](architecture.html), [spike-run](spike-run.html), [Use your circuit in Python](python.html), [contributing](contributing.html), [the changelog](changelog.html).
