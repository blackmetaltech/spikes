# NESTML custom cell models

| Model | Source | Spike means | Doc |
|---|---|---|---|
| `sa_node_pacemaker` | reduced Maltsev-Lakatta / Demir / Kurata rabbit SA node cell | one heartbeat (upstroke through 0 mV) | [sa_node_pacemaker.md](sa_node_pacemaker.md) |
| `butera_burster` | Butera, Rinzel & Smith 1999, model 1 | one action potential in a preBotzinger burst | [butera_burster.md](butera_burster.md) |
| `icc_slow_wave` | FitzHugh-Nagumo oscillator in gut units | one slow-wave onset | [icc_slow_wave.md](icc_slow_wave.md) |
| `stg_cell` | crab stomatogastric neuron after Prinz et al. 2003 (kinetics from memory, game conductances) | one action potential in a burst | [stg_cell.md](stg_cell.md) |

Sources are `models/nestml/*.nestml`. All three have exponential synaptic input (an `excitatory` and an `inhibitory` spike port, so a positive circuit weight excites and a negative one inhibits), a `continuous` current port (`dc` inputs) and the parameter `I_e`. `ach` and `ne` are plain parameters. In a circuit they are driven by the modulation table (`{"modulator":"NE","pop":"N","param":"ne","gain":1.0}`) or by `set_neuron_param`; PyNEST uses `SetStatus`.

## How it is built

```
scripts/build-nestml.sh            # NESTML 9.x: models/nestml/*.nestml -> third_party/nestml-gen/*.cpp
cmake -S . -B build && cmake --build build -j     # engine/CMakeLists.txt compiles them into libspike
scripts/build-nestml.sh --pynest   # optional: libspike_nestml_py.so for PyNEST (parity)
```

* NESTML 9.0.1 targets NEST 3.10-dev through `NESTExtensionInterface`. That interface is only reachable through `nest.Install()`, which needs ltdl, and ltdl is off. `scripts/nestml_generate.py` keeps the per-neuron sources and drops NESTML's module class.
* `engine/src/modules/nestml_models.cpp` includes the generated headers and calls each `register_<model>()`. `Engine::Impl::build()` calls `spike::register_nestml_models()` after `ResetKernel` and after `set_kernel_status`. NEST drops every registered model on a reset and on a thread-count change, and with ltdl off nothing re-registers them.
* Without the generated code, libspike builds a stub and `test_nestml_*` skip (exit 77).

## NEST / NESTML incompatibilities worked around (`scripts/nestml_generate.py`)

1. `generate_target` always runs `NESTTools.detect_nest_version()`, which imports Python `nest`. The script pins the version to `"main"` (NEST `3.10.0-post0.dev0`) instead.
2. NESTML's builder wants to run `cmake` plus `nest-config` and install. It is replaced by a no-op, and CMake compiles the sources.
3. Generated headers contain `#ifndef HAVE_LIBLTDL #error ...`. The script removes that guard. The neuron code does not use ltdl.
4. NESTML 9 syntax: `exc_spikes <- excitatory spike` takes no unit (use an `internals` `unit_psg nS`); the variable names `a` and `h` collide with unit names and warn.

## Validation

`ctest -R nestml` runs `test_nestml_sa_node`, `test_nestml_butera`, `test_nestml_icc` and `test_nestml_parity` (engine versus PyNEST, spike for spike).
