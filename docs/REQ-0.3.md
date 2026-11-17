# v0.3 requirements: Learning curve

The design is in `docs/DESIGN-0.3.md`; read it first.

**Contract.** The v0.3 additions in `engine/include/spike/task.h` are:
- `level_kind`;
- the `concepts_*` lists;
- `on_spine`;
- `set_mode` / `mode`;
- `variants`;
- `hints`;
- `metrics`;
- `next_event_ms`;
- `diagnosis`;
- `notebook`.

These additions are add-only.

**Ground rules** are the same as in 0.2:
- Use real models and cite sources.
- Every level has a reference circuit that passes, and lesions that fail.
- Results are deterministic.
- Each agent tests its own work and does not commit.
- Tasks self-register with `SPIKE_REGISTER_TASK`, and the file stem must equal the task id.

**Campaign order keys:**

| Chapter | Order keys |
|---|---|
| invertebrates | 1–39 |
| spinal | 100–139 |
| heart | 200–239 |
| breathing | 300–339 |
| autonomic | 400–439 |

Micro-puzzles take the keys just before the task that uses them. Bosses take x30–x39, and diagnosis cases x40 and above.

| Item | Owner | Status |
|---|---|---|
| A. Micro-puzzles for concepts 1–10 | micro-A | Not started |
| B. Micro-puzzles for concepts 11–19 | micro-B | Not started |
| C. Boss scenarios (spinal sprint, autonomic exam) and 6 diagnosis cases | boss/diag | Not started |
| D. The 14 existing tasks: modes, variants, hints, metrics, notebook, concepts; the deferred 0.2 fixes | tasks | Not started |
| E. Engine: weight spread, instant check, Python mirror | engine | Not started |
| F. Game progression: map spine and branches, concept unlock, notebook, telemetry, modes setting | game-prog | Not started |
| G. Lab play: hint ladder, verdict highlight, histograms, diagnosis mode, instant check, skip to event, celebration, surprise | game-lab | Not started |
| H. Audio: generator, GameAudio, spike sonification | audio | Not started |
| I. Histogram seed data (offline solution sweeps), persona critique, release | lead | Not started |
