# Seed histograms (synthetic)

`catalog/histograms/<task>.json` holds the distributions behind the post-solve histograms (docs/DESIGN-0.3.md section 6, `godot/ui/histogram_view.gd`). Metrics: `neurons`, `energy_hz` (spikes per second per neuron, over the whole episode) and the task's own quality metric(s) as `Task::metrics()` names them (e.g. `latency_ms`, `reach_ms`). Format: `{"task", "metrics": {name: {"unit", "lower_is_better", "values": [...]}}}`; the extra keys `source` and `samples` are ignored by the game.

**These are generated, not real players.** Until opt-in uploads exist (0.4 Share) they are a plausible shape for "where you stand", nothing more. The shapes follow from how the generator varies one author-built circuit, so:

- the minimum of `neurons` is the reference circuit's own count (or a little under it), and the spread above it is mostly the idle and redundant cells the generator adds; real players will be both leaner and messier;
- `energy_hz` and quality are spread around the reference's values, and cannot show a clever solution that is far from the reference design;
- levels with only a few dozen samples have a lumpy, not a smooth, distribution.

## Method

`tools/histograms/gen_histograms.py` (Python 3, stdlib only, spike-run as the engine). Per level (every task in `circuits/*.json` except `dx_*`; the level is found through `meta.task`):

1. Start from the reference circuit. Candidate 0 is the unchanged reference; every other candidate is a random variant of it (own RNG, `Random("<seed>:<task>:<index>")`, global `--seed` 20260101), so a run is reproducible given the same engine build.
2. A variant applies, each with its own probability, and with a magnitude `t = U(0,1) * strength`:
   - **Population sizes.** Populations with role `neuron` that no input or output pins by index, and that no sparse `edges` projection wires, are resized by a log-normal factor (sd 0.45 t, slightly biased smaller). `one_to_one` partners resize together. Weights from a resized pre population are rescaled by `factor^-c` (c random in 0.6-1.05) so the total drive stays roughly constant; dense `edges` projections are regenerated at the new size from the original weights. Sensor and motor populations keep their size (the skeleton).
   - **Weights.** A global factor (log-normal sd 0.12 t) times a per-projection factor (sd 0.25 t). With probability 0.5, about 60% of projections get a `weight_cv` spread of 0.05-0.45 (rule projections become equivalent explicit `edges` first).
   - **Neuron parameters.** Half of the populations get 1-3 parameters nudged (Gaussian, sd 4% of the slider range, 1.5% for `I_e`), clamped and snapped to the lab's slider ranges and steps (`ADVANCED` in `godot/game/circuits.gd`). Models without a slider table get a 6% t multiplicative jitter on their parameters. Modulator output gain and tau are jittered too.
   - **Redundant or idle neurons.** 1-4 additions: isolated idle cells, cells that listen to an existing population and fire for nothing, or a parallel clone of a free population (same inputs and targets, output weight split between original and clone).
   - The circuit seed is randomised.
3. Each candidate is scored with `spike-run --task <id> --circuit <file> --seeds <s> --require any` (standard mode, default variant) on seed 1, then, only if seed 1 passed, on seed 2. **A sample is kept only if both seeds pass** (stricter than `--require any`).
4. A kept sample records `neurons` (engine count, sensors and motors included), `energy_hz` = mean over the two seeds of spikes / neurons / episode seconds (`rate_hz_per_neuron`), and the mean over the two seeds of each task metric.
5. Candidates run in batches of 8 on 4 workers. After each batch the variation `strength` adapts to the pass rate (below 25% shrinks it, above 70% grows it, range 0.25-2), so slow levels do not waste hours on circuits that fail.
6. A level stops at 150 passing samples, or after the wall budget (150 s) once it has 40, or at 3x the budget, whichever comes first. Slow levels therefore end with fewer than 40.

Rerun: `python3 tools/histograms/gen_histograms.py [--tasks a,b] [--budget s] [--target n] [--min n] [--bin path/to/spike-run]` (`--probe` only times the references). It overwrites the files of the levels it runs. About 2 h on 4 cores for everything.

## Counts and caveats

Per level: see the table in the generator's last run, summarised here.

| level | samples | note |
|---|---|---|
| all `m_*` micros | 115-157 | m_rhythm 115 (slower); degenerate quality metrics, see below |
| reflex | 152 | |
| worm | 60 | |
| lamprey | 41 | |
| stretch_reflex | 43 | |
| renshaw | 41 | |
| posture | 42 | |
| reach | 34 | ~30% of variants pass |
| heart_pacemaker | 40 | |
| heart_autonomic | 11 | 17 s per run, narrow |
| baroreflex | 42 | |
| breathing | 25 | |
| pupil | 90 | |
| peristalsis | 31 | |
| fight_or_flight | 16 | narrow plateau (about half of variants fail) |
| boss_spinal_sprint | 19 | 18 s per seed; ~40% pass |
| boss_autonomic_exam | 18 | about 28% pass even at low strength |

Quality metrics that are binary in practice (all identical): `m_reciprocal.extensor_vs_rest`, `m_recruitment.force_error`, `m_wave.seg2_vs_tone` (always 0), `boss_autonomic_exam.conditions_met` (always 19). The histogram view shows a single spike there; consider hiding those metrics. `m_pacemaking`, `m_coupling` and `m_excitation` are nearly constant too.
- Constant quality metrics (a single bar) are removed from the files after generation, so the histogram screen shows only axes that vary.
