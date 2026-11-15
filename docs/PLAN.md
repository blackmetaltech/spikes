# SPIKE: grow a nervous system

Spike is a game and a research tool. You wire biologically realistic spiking neurons into circuits that do real jobs: steer a worm up a smell gradient, swim a lamprey, dodge a looming shadow, localise a sound. You score them, analyse them and share them as motifs.

## Shape

```
 NEST (third_party/nest-simulator, pinned)  ← the simulator: every NEST neuron/synapse model, NESTML later
   built static, no MPI, Python only at build time
 engine/  (C++20, CMake → libspike)
   ├─ engine.h            the contract: load circuit JSON → closed-loop step / set_input / drain_spikes
   ├─ ring-buffer recording backend (custom NEST RecordingBackend)
   ├─ circuit.cpp         circuit JSON (docs/CIRCUIT-FORMAT.md) → nest::create / connect
   ├─ tasks/              bodies + worlds + scores (reflex, lamprey, worm …), deterministic
   ├─ analysis/           motif census (3-node, z-scores vs degree-preserving nulls), rates, CV, synchrony
   └─ tests/              unit, validation and golden spike-checksum traces
 godot/   Godot 4.7 game, code-built UI (Kit/Layout/Settings/Pad/Touch from Mugshot)
 gdext/   godot-cpp GDExtension linking libspike → class SpikeSim
 python/  `spike` package: the same circuit JSON → PyNEST, plus analysis (the research-tool side)
```

## Principles
- **Use NEST models, don't reinvent them.** Use the names and units NEST uses. The tasks, the editor and the analysis are what we add.
- **A circuit is data.** It is a versioned JSON file. The game builds it with the C++ engine and the Python package builds the identical network with PyNEST. A parity test checks that the spikes match.
- **Deterministic.** Pin the NEST version, set the seed and thread count, and check golden spike checksums.
- **Gigawatt-style process.** Releases are small MVPs, then themed improvements. Each release has a requirements doc with a status table, tests, screenshots, a persona critique and a CHANGELOG entry.

## Releases
| Ver | Theme | Done when |
|---|---|---|
| 0.1 | **First circuit** | NEST embedded, the engine and ring buffer, circuit JSON, 3 tasks (reflex arc, lamprey half-centre swimmer, worm chemotaxis), a Godot editor with raster and trace scopes, the 3-node motif census, the Python package with a PyNEST parity test |
| 0.2 | **Learning** | stdp_dopamine_synapse reward tasks (fly looming escape, a conditioning box), Tsodyks STP |
| 0.3 | **Discovery** | sweeps and evolutionary motif search, a motif library, 4-node census |
| 0.4 | **Connectomes** | C. elegans (OpenWorm) import, NeuroML, NESTML custom models |
| 0.5 | **Maps of space** | head-direction ring attractor, grid cells, Jeffress sound localisation |
| 0.6 | **Share** | circuit packs, replays, the Claude lab assistant |
