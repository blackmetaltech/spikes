# v0.1 requirements: First circuit

Ground rules:
- Stay green on `cargo test --workspace`, `pytest`, and `scripts/smoke.sh`.
- Use real units and cite the source of every parameter.
- Do not commit; the lead reviews and commits.

| Item | Status |
|---|---|
| A. spike-core engine and golden traces | Not started |
| B. Tasks (bodies, sensors, scores) | Not started |
| C. Analysis and export | Not started |
| D. spike-py | Not started |
| E. Godot game (spike-gd, editor, scopes, tasks) | Not started |
| F. Release | Not started |

## A. Engine (`crates/spike-core`)
- **Neurons:** LIF (with refractory period), Izhikevich (all 20 published patterns available as presets) and AdEx. Store them in structure-of-arrays layout per population.
- **Synapses:**
  - conductance-based, using exponential or difference-of-exponentials kinetics;
  - receptor presets AMPA, NMDA (with Jahr–Stevens Mg block), GABA_A and GABA_B;
  - integer-step delays delivered through a ring buffer;
  - gap junctions;
  - Dale's law as an optional check.
- **Plasticity:** pair-based STDP (additive and multiplicative), using traces.
- **Inputs:** Poisson sources, current steps, current ramps, and sensor channels driven by a task.
- **Integration:** fixed `dt` (default 0.1 ms). Use exponential Euler for linear parts and RK2 for Izhikevich and AdEx, documented. RNG is PCG32 with a seed per run.
- **Recording:** spikes (neuron, step), sampled state variables and a decimated voltage trace.
- **Validation tests:**
  - the LIF f–I curve against its analytic formula;
  - Izhikevich regular spiking, fast spiking, bursting and chattering reproduce the published spike counts and timing within tolerance;
  - the STDP window shape;
  - NMDA Mg block values.
- **Golden traces:** `tests/golden/*.json` hold a circuit, a seed, step counts and a spike-raster checksum every 1000 steps. Regenerate them only with `GOLDEN_UPDATE=1`.
- **Benchmark:** `cargo bench` with 10k neurons, 1M synapses and 1 s of simulated time. Report the real-time factor.

## B. Tasks (`crates/spike-core/src/task`)
A task supplies a body, its sensors (which drive input channels), its motor outputs (which read spike rates from output neurons), a world step and a score. Each task needs one reference circuit that passes it, and that circuit is part of the golden traces.
1. **Reflex arc:** a touch sensor drives a withdrawal motor neuron. Score: response latency, and no false triggers under noise.
2. **Lamprey swimming rhythm:** two half-centres with mutual inhibition plus tonic drive. The body is a chain of segments that produces a travelling wave. Score: oscillation frequency in the 1–10 Hz band, a stable left/right phase near 180° and forward speed.
3. **Worm chemotaxis:** a 2D plate with a Gaussian salt gradient. The worm reads concentration at its head over time, using an ASE-like on/off sensor pair, and steers with dorsal/ventral turning neurons. Score: time to the peak and average distance to it.

## C. Analysis and export
- **Motif census:** count all 13 connected 3-node directed motifs. Compare against N degree-preserving random rewirings and report z-scores. Feed-forward loops, mutual inhibition and similar named motifs are labelled.
- **Statistics:** firing rate, ISI CV, a population synchrony index and a raster PSTH.
- **Brian2 export:** write a Python script that runs the same circuit in Brian2. CI checks that it is generated and that it is syntactically valid Python. If Brian2 is installed, it also checks that the rates match within tolerance.

## D. spike-py (`crates/spike-py`, maturin)
- `spike.load(json)`, `.run(ms)`, `.spikes()` and `.trace(id)` return numpy arrays.
- `spike.motifs(circuit)` and `spike.export_brian2(circuit)`.
- pytest covers each function, plus an example notebook in `examples/`.

## E. Game (`godot/`, `crates/spike-gd`)
- Same project shape as Mugshot and gigawatt: code-built UI through `Kit`, static classes, no autoloads, and `SP_*` env hooks (`SP_SMOKE`, `SP_SHOT`, `SP_SCREEN`). The UI shell (Kit, Layout, Settings, Pad, Touch, Fonts) is copied from `/home/user/mugshot-mayhem/godot` and re-themed.
- **Look:** a dark lab bench in a "living circuit" style.
  - Neurons are glowing soma discs that flash on spikes; excitatory ones are warm, inhibitory ones cool.
  - Synapses are curved axons with travelling pulse dots that respect the delays.
  - There are no human figures. Bodies (worm, lamprey) are simple shaded vector shapes.
- **Screens:**
  - **Title.**
  - **Task select:** the three tasks, each with a brief that explains the biology in plain words.
  - **Lab:**
    - an editor to place neurons, choose a model and preset, drag synapses, and edit receptor, weight and delay;
    - a live arena view of the task;
    - a scope dock with a raster, voltage traces and a rate meter;
    - play, pause and speed controls;
    - the score.
  - **Analysis panel:** the motif census with z-scores, plus statistics.
  - **Settings.**
- **Sound:** spikes become clicks, optional and rate-limited, with pitch set by neuron.
- **Tests:**
  - a headless smoke run;
  - GDExtension load;
  - each task's reference circuit passes in-engine;
  - screenshots at 1920×1080 and 844×390.

## F. Release
README, CHANGELOG, screenshots, persona critique (a novice, a neuroscientist, a gamer) and tag 0.1.0.
