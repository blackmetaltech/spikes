# Changelog

## [0.1.0] - 2026-11-16 — First circuit

The first release: wire spiking neurons into circuits that make a body do something.

### Added
- **NEST built in.** The NEST simulator runs inside the game through a C++ engine. It runs in closed loop: every 1 ms the game sets sensor inputs, runs NEST and reads the spikes back, at about 3 µs of overhead per step. Results are deterministic, checked with golden spike checksums.
- **Circuit files.** A circuit is a versioned JSON file using NEST model and parameter names. The same file runs in the game, in the `spike-run` command and in PyNEST, with spike-for-spike parity.
- **Three tasks**, each with a reference circuit, lesions that fail the way the biology predicts, and plain-language verdicts:
  - **Reflex arc:** detect a touch through the noise, by convergence.
  - **Lamprey swimmer:** a half-centre oscillator with crossed inhibition and adaptation.
  - **Worm chemotaxis:** steer by the difference between the ASEL and ASER sensory neurons.
- **Lab.** Build a circuit with NEST models and presets, connect neurons with weight, delay and synapse model, and edit neuron parameters in an advanced panel. Add noise inputs. Watch the live world, the spike raster, voltage traces and output rates, and get a score.
  - **TEST x5** runs your circuit on 5 seeds.
  - You earn up to 3 stars: pass, pass on all 5 seeds, and stay under the neuron budget.
  - An end-of-episode card suggests what to try next.
  - Progress is saved, tasks are ordered and unlock in turn, and circuits load and save as JSON.
- **Analysis:**
  - **Motif census:** all 13 three-node connection patterns, numbered as in Milo et al., against degree-preserving random rewiring. It works on populations or on individual neurons, and warns when the random comparison is too weak to mean anything.
  - **Firing statistics:** firing rates, ISI CV, Golomb χ and PSTH.
- **`spike` Python package.** It builds the same network in PyNEST, with motif and statistics tools and an example notebook.
- **Layouts.** Desktop and phone.

### Known gaps
- Desktop Linux build only. Windows, macOS and Android packaging are not done yet.
- NEST is built without OpenMP, so it runs on one thread.
- There is no audio.
- The editor does not offer plastic synapses (STDP, Tsodyks). Circuits that already use them still load.
- On a phone, the tasks screen needs scrolling.
