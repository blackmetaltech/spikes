# SPIKE: grow a nervous system

Spike is a game and a research tool. You wire biologically realistic spiking neurons into circuits that do real jobs: steer a worm up a smell gradient, swim a lamprey, dodge a looming shadow, localise a sound. You score them, analyse them and share them as motifs.

## Shape

```
 spike-core (Rust crate, deterministic)
   ├─ neurons: LIF, Izhikevich, AdEx (later HH)
   ├─ synapses: exp/alpha conductance (AMPA, NMDA+Mg block, GABA_A, GABA_B), delays, gap junctions
   ├─ plasticity: STDP, Tsodyks–Markram STP, three-factor (dopamine) rules
   ├─ bodies + tasks: worm, lamprey, fly, owl …  (sensors in, motor out, a score)
   ├─ analysis: motif census (3/4-node, z-scores vs degree-preserving nulls), rates, CV, synchrony, spectra
   ├─ io: circuit JSON (versioned schema), export to Brian2 / NEST / NeuroML
   └─ golden traces: fixed seeds → checksummed spike rasters (any change to dynamics must regenerate them on purpose)
 ├─ spike-gd   (GDExtension via godot-rust/gdext) → the game
 ├─ spike-py   (PyO3 / maturin)                    → pip + Jupyter, the research side
 └─ spike-wasm (later)                             → web demo
 Godot 4.7 game (godot/): code-built UI (Kit, Layout, Settings, Pad, Touch), circuit editor, task arena, raster/trace scopes
 Claude sidecar (later): explains circuits, proposes experiments. Always optional and mocked in tests.
```

## Principles
- **Real models, real units.** Use mV, ms, nS and pA, with parameters from the literature and the source cited in code. Every model has a validation test against a known result: the Izhikevich firing patterns, the LIF f–I curve against its analytic formula, and the STDP window shape.
- **Deterministic.** There is one integrator (fixed dt, 0.1 ms by default; exact integration where it is linear) and one seeded RNG (PCG). Results are the same on every platform.
- **A circuit is data.** It is a versioned JSON file that the game, the Python package and the exporters all read, so a circuit made in the game runs in Brian2.
- **Fast enough.** Aim for 10k neurons and 1M synapses in real time on a laptop CPU. Use structure-of-arrays layout, event-driven synapse delivery and a ring buffer for delays.
- **Gigawatt-style process.** Releases are small MVPs, then themed improvements. Each release has a requirements doc with a status table, tests, screenshots, a persona critique and a CHANGELOG entry.

## Releases
| Ver | Theme | Done when |
|---|---|---|
| 0.1 | **First circuit** | core (LIF, Izhikevich, conductance synapses, delays, STDP), circuit JSON, golden traces, the Python package, a Godot editor with raster and trace scopes, 3 tasks (reflex arc, lamprey swimming rhythm, worm chemotaxis), 3-node motif census, Brian2 export |
| 0.2 | **Learning** | three-factor dopamine learning, STP, reward tasks (fly looming escape, a conditioning box) |
| 0.3 | **Discovery** | parameter sweeps and evolutionary motif search, a motif library, 4-node census |
| 0.4 | **Connectomes** | C. elegans import (OpenWorm), NeuroML import and export, NEST export |
| 0.5 | **Maps of space** | head-direction ring attractor, grid cells, Jeffress sound localisation |
| 0.6 | **Share** | circuit packs, replays, a web demo (WASM), the Claude lab assistant |
