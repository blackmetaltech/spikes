# v0.1 persona critique: First circuit

Judge's review of release 0.1 at commit `7cb0a5d` (tree as of 2026-11-16). No code was changed for this review.

**How it was checked**
- Built the engine and the GDExtension: `cmake --build build -j4`, then `scripts/build-gd.sh`.
- `ctest --test-dir build`: 9 of 9 pass. `pytest python`: 29 pass, and the parity test really runs, against `/tmp/nest-install`. `scripts/smoke.sh`: SMOKE OK.
- Took screenshots with xvfb and the opengl3 driver into `docs/screenshots/critique/`, and looked at every one.
- Ran a headless GDScript harness (`godot --headless --path godot -s harness.gd`) that drives `SpikeSim` and `SpikeTask` exactly as the lab does. With it I built the circuits a novice would build, using the editor's own defaults (`Circuits.add_neuron`, `add_projection`, input weight 120 pA, `all_to_all`, delay 2 ms), and probed the scoring edge cases. The repro snippets are inline below.

Severity: **P0** blocks the release. **P1** should be fixed for 0.1. **P2** can wait.

---

## 1. Curious novice (no neuroscience; can they pass from the brief alone?)

**Short answer.**
- **Reflex:** yes, after trial and error.
- **Worm:** yes, but only after wiring every channel and by luck with the weights.
- **Lamprey:** no. Nothing the editor can express worked.
- On the way, the very first edit leaves the lab broken and the screen gives no explanation.

### P0-1. The circuit will not run until every sensor and every motor is wired, and the lab hides the failure on first open
- **Evidence:**
  - Harness: the task skeleton plus one neuron, which is exactly what the lab sends after the first `+ EXCITATORY`, comes back with `{"error": "inputs[0].targets: must select at least one neuron"}`. The same thing happens for any output with no neurons (`outputs[1].neurons: must select at least one neuron`).
  - Root cause: `engine/src/circuit.cpp:298-299` and `:330-331` reject empty selectors. `godot/ui/screens/lab.gd:123-124` creates every channel with `targets: []` / `neurons: []`.
  - The lab is then in a broken state (`docs/screenshots/critique/novice-unwired-1920x1080.png`). `Telemetry.setup` returns at `godot/game/telemetry.gd:57`, before it creates the task (`:68-71`). The world panel therefore says *"Free run: no task selected. Pick a task to see its world here."* (`godot/ui/task_view.gd:100`) while the title says *Reflex arc*. The score card says NO TASK and the sensor cards read INPUT instead of SENSOR.
  - On first open, for example when coming back to a half-wired circuit kept in `lab_keep` or `last_<task>.json`, no banner appears either. `_start()` → `_reload()` runs before `build()` has created `banner`, so `_banner()` returns silently (`lab.gd:44`, `:141`, `:151-153`).
  - For the worm, all four channels (ASEL, ASER, turn_left, turn_right) must be connected before a single spike is simulated.
- **Fix (pick one, engine-side preferred):**
  - (a) Engine: accept empty `targets` and `neurons`. Still create the device but skip `nest::connect`; an empty output reads 0. Files: `engine/src/circuit.cpp:298,330`, `engine/src/engine.cpp:321-337`, the mirror in `python/spike/circuit.py:160-170`, `docs/CIRCUIT-FORMAT.md`, and a test in `engine/tests/test_engine.cpp`.
  - (b) Game-only: `Telemetry.setup` sends a sanitised copy (unwired channels dropped), and `Engine::set_input` stops treating an unknown channel as sticky (see P2-C4).
  - Either way:
    - `telemetry.gd` must create `task` before loading the engine, so a rejected circuit still shows the task's world and brief;
    - `lab.gd` must defer `_banner` until `banner` exists (store the message and apply it at the end of `build()`);
    - the score-card verdict for an unwired task should name what is missing ("connect *touch* to a neuron", "connect a neuron to *withdraw*").

### P1-N1. Default weights produce silence, and nothing says why
- **Evidence:**
  - Harness: touch → N1 → withdraw with the editor's default input weight (120 pA) scores **0**, with "Missed 6 of 6 pokes". The same circuit at 300 pA scores **94 and passes**.
  - The same happens for touch → N1 → N2 → withdraw with the default 250 pA projection: 0.
  - Screenshot `novice-wired-1920x1080.png`: 5 spikes in 6 s and a `withdraw 1 Hz` gauge. The only hint is "In progress: 0 of 6 pokes answered".
  - The default comes from `lab.gd:123` (`120.0` for poisson inputs).
- **Fix:**
  - Default the poisson input weight to about 300 pA for `iaf_psc_alpha` targets (`lab.gd:123`; better, scale it by `Circuits.WEIGHT` of the target model when the input is first connected, in `editor_view.gd:389-395`).
  - Add diagnostic verdicts in the task `score()` methods when a motor channel stayed silent or saturated. For example, in `engine/src/tasks/reflex.cpp:242-252`: "withdraw never fired: increase the weights into it". Do the same in `lamprey.cpp:139-149` and `worm.cpp:163-168`.

### P1-N2. The lamprey is unsolvable with what the editor exposes
- **Evidence:**
  - Harness: I built the textbook half-centre the brief describes in 24 variants: two `aeif_cond_alpha` cells (Adapting and NEST-default presets), drive into both, reciprocal inhibition, each cell on one motor, drive 300/600/1000 pA, inhibition 20/60 nS, symmetric or 20 % asymmetric. **None** pass. The symmetric variants report "No rhythm: left and right motor rates never alternate". The asymmetric ones lock into winner-take-all ("Rhythm at 0 Hz").
  - Cause: the network has no noise and is fully deterministic, so identical cells stay in lock-step forever. The reference circuit breaks symmetry with a hidden `V_m` -62 vs -70.6 mV (`docs/circuits/lamprey.md`, wiring table) and uses tuned `a`, `b`, `tau_w`, `tau_syn_*`.
  - The inspector cannot edit any neuron parameter, only pick a preset (`lab.gd:324-344`). There is no noise source in the palette either.
- **Fix:**
  - Give each neuron a small seeded random initial `V_m`. Do it in the engine (`engine/src/engine.cpp` build step (1): draw `V_m` from `U(E_L, V_th)` from the circuit seed when the population has no explicit `V_m`, and mirror it in `python/spike/circuit.py`). Or do it in the editor (`Circuits.add_neuron` writes `V_m` jittered by population index). Either way the novice's symmetric half-centre can then break symmetry.
  - Add a "noise" input kind (poisson background) to the palette.
  - Add an "Advanced" fold in the neuron inspector with the 3-5 key NEST parameters per model (`tau_w`, `b`, `a`, `I_e`, `tau_syn_ex`/`tau_syn_in`, `V_m`), in `lab.gd:_inspect_pop` with presets in `godot/game/circuits.gd:11-15`.
  - Re-test with the harness until a hand-built two-cell half-centre passes.

### P1-N3. Worm: the intuitive wiring fails and the winning wiring is a single neuron
- **Evidence:**
  - Harness: "ASER (worse) → N1 → turn_left, ASEL (better) → N2 → turn_right", which is what the brief's wording invites, **fails** at 120, 400 and 1000 pA (mean distance 6-7.8 mm).
  - A single cell, ASER → N1 → turn_left at 1000 pA, **passes in 34.9 s, as fast as the 12-neuron reference** (29-35 s, `docs/circuits/worm.md`).
- **Fix:** rewrite the worm brief (`engine/src/tasks/worm.cpp:52-56`) to say what "turning" means: turning away from a bad heading in one direction, a pirouette, is enough. Mention that the turn rate depends on the left-minus-right difference. The difficulty side of this is covered in P1-G2.

### P2-N4.
- Empty-bench hint says "Add a neuron below", but the buttons are above, and the text overlaps the channel cards (`editor_view.gd:288-289`, `novice-skeleton-844x390.png`).
- The Delete key never reaches the editor because of `focus_mode = FOCUS_NONE` at `editor_view.gd:25`, so the handler at `:356` is dead.
- The selected-axon weight label is drawn underneath neuron discs (`editor_view.gd:200-210` runs before the neuron loop at `:237`; visible in `docs/screenshots/lab-edit-1920x1080.png`).

---

## 2. Computational neuroscientist (models, units, task physics, scoring, motif census)

**What is good:**
- NEST models and parameter names are used throughout.
- Units are correct in the docs.
- The reference circuits are documented with lesions that fail and sources.
- The parity test is real, and the Milo ids and canonical classes check out: I verified ids 6/12/14/36/38/46/74/78/98/102/108/110/238 against the 9-bit encoding.
- The triad counting visits each triad once (`analysis.cpp:177-194`).

### P1-S1. Lamprey phase score is meaningless when one side is silent: a scoring loophole
- **Evidence:**
  - `lamprey.cpp:242-251` demodulates left and right independently and takes the phase difference. If `dr_` is constant, `atan2(sr, cr) = atan2(0, 0) = 0`, so the "phase" is just the left signal's absolute phase relative to t = 2.5 s. It is arbitrary, and can be 180.
  - Harness: a single bursting neuron on `left_motor` with a silent cell on `right_motor` reports `phase_deg 176.5`.
  - The same run shows the frequency detector counting spike ripple: `freq_hz 206.7`, because the hysteresis is 5 % of the swing (`lamprey.cpp:219`).
  - Only the bad frequency stopped that run from passing the anti-phase test.
- **Fix:**
  - Require both sides to modulate: the amplitude at `f` of each side above X Hz, or `min(swing_L, swing_R) > 0.3 * max`.
  - Report the phase only when both are coherent.
  - Raise the hysteresis to about 25 % and smooth before counting crossings.
  - Add the one-sided circuit to the lesion tests in `engine/tests/tasks_lesions.inc`.

### P1-S2. Motif z-scores mislead: ±∞ is drawn as +8, the census runs on neurons the player never drew, and nulls on dense graphs barely move
- **±∞ drawn as +8.** `godot/ui/screens/analysis.gd:86` maps every non-finite z (JSON `null`) to `+zmax` when count > 0, so **−∞ is drawn as "+8.0\*"**, and +∞ as a finite 8. Screenshot `analysis-1920x1080.png`: four classes pinned at "+8.0\*" for the lamprey. In `motif_zscores` (`analysis.cpp:276-279`), sd = 0 means "fully determined by the degree sequence". That is *undefined significance*, not infinite significance.
- **Neuron-level census.** The census is taken on the neuron graph from NEST (`analysis_nest.cpp`). The editor shows populations wired `all_to_all`, so most counts are artefacts of block expansion. Examples: "mutual pair co driving 60" and "fully connected 8" come from E_s recurrence, not from a designed motif. The half-centre's defining loop, E_L → CC_L ⊣ E_R → CC_R ⊣ E_L, is a 4-cycle and never appears.
- **Weak nulls on dense graphs.** With 10 swap *attempts* per edge (`analysis.h:68`) on small dense graphs, most attempts are rejected. The nulls stay close to the original, sd is tiny, and 10 of 13 classes are starred.
- **Fix:**
  - `analysis.gd:85-96`: draw non-finite values as "n/a (fixed by degrees)", greyed, with no bar.
  - Add a "graph: populations | neurons" toggle. The population graph is built from `circuit.projections` in GDScript, or by a new `motif_json_graph(edges_json)` in `gdext/src/spike_sim.cpp` and `bridge.cpp`.
  - Report the swap acceptance rate from `rewire()` (`analysis.cpp:203-244`) and warn under about 20 %.
  - Say in the header that "all" is sign-blind, and that Milo-style nulls conserving mutual edges are planned (P2).

### P1-S3. STDP and Tsodyks are offered in the edge inspector but silently wrong here
- **STDP.** `lab.gd:378` offers `stdp_synapse` with NEST's default `Wmax = 100`. Editor default weights are 250 pA for `iaf_psc_alpha` targets (`circuits.gd:18`), so the first potentiation clamps them to 100 pA. NEST's `facilitate()` (`third_party/nest-simulator/models/stdp_synapse.h:189-190`) maps `w/Wmax`, so an inhibitory (negative) weight is pushed through the same formula and potentiation can flip its sign or clamp it to +Wmax. The harness loads both without any error.
- **Tsodyks.** `tsodyks_synapse` assumes an exponential PSC whose `tau_psc` matches the neuron's. Its default 3 ms doesn't match `iaf_psc_alpha`'s alpha-shaped 2 ms synapse.
- **Fix:** for 0.1, remove both from `SYNAPSE_OPTS` (`lab.gd:378`) and `Circuits.SYNAPSES` (`circuits.gd:19`), since plasticity is the 0.2 theme. Or set `Wmax = 2·|w|·sign(w)` on selection and only allow Tsodyks onto `*_psc_exp` targets.

### P1-S4. A pass on one fixed seed is fragile, and the reflex hint is not what the score rewards
- **Evidence:**
  - Single-neuron reflex (touch → N1 → withdraw, 300 pA): passes on 7 of 8 seeds (scores 80-94; seed 6 misses a poke).
  - The game always evaluates the circuit's own seed (`telemetry.gd:95`), so the player can overfit one peak position (worm) or one set of poke times (reflex).
  - The reflex brief's hint ("make the motor cells need several inputs at once") is never needed. A one-cell arc scores 94 and the reference 100.
  - Part of the reason: the burst detector counts *pool* spikes (≥ 3 in 25 ms summed over the pool, `reflex.cpp:178-186`). A 1-cell pool is judged by the same absolute count as a 30-cell pool, contrary to the per-neuron intent stated in `task_util.h:62-64`.
- **Fix:**
  - Make "pass" an evaluation over N seeds (for example 5): a "TEST" button in `lab.gd` that runs headless episodes in a loop. `Telemetry` already has `reset(seed)`.
  - Scale the reflex burst threshold to `max(3, 0.5 * pool size)` (`reflex.cpp:132,186`).
  - Raise background noise or add noise bursts ("brushes") so that convergence is actually needed. Update `docs/circuits/reflex.md`.

### P2-S5. Smaller scientific notes
- **Voltage traces.** The scopes poll `V_m` once per 1 ms step through `get_node_status` (`engine.cpp:564-569`, `:399-411`); there is no multimeter, contrary to REQ A ("watching a neuron attaches a multimeter"). `iaf` spikes are therefore invisible on the scope (the reset happens within the ms). Use a multimeter at resolution into the ring, as REQ specifies.
- **Current-based inhibition.** −1500 pA on `iaf_psc_alpha` (worm AIY ⊣ AIB) drives V_m to about −120 mV, as seen on the scope axis in `lab-worm-1920x1080.png`. Current-based synapses have no reversal potential. Note this in `docs/circuits/worm.md`, or use `iaf_cond_alpha` for the inhibited cells.
- **Golomb χ** is computed on 1 ms binary spike-count bins (`analysis.h:98`, `telemetry.gd:191-195`). That underestimates synchrony; state the bin, or smooth (Golomb 2007 uses V or filtered rates).
- **Lamprey thrust.** `T = ½ m v_tail² (1 − U/c)` is a toy, not Lighthill's ½ m (w² − U² h′²). The header says "simplified"; also say so in `docs/circuits/lamprey.md` and avoid citing it as Lighthill's model.
- **Worm biology.** `docs/circuits/worm.md` presents klinotaxis plus pirouettes, but the reference only turns one way (TL unused): it is klinokinesis/pirouette only. Say so.
- **Reflex doc.** `docs/circuits/reflex.md` line 1 says "within 150 ms" while the pass bar is a mean of 60 ms. Explain that 150 ms is the hit window and 60 ms the pass bar.
- **Verdict formatting.** Lamprey uses `o.precision(2)`, which prints "Rhythm at 2.1e+02 Hz" and "Rhythm at 0 Hz is outside…" when no crossings are found (`lamprey.cpp:138-142`). Use `std::fixed` and a separate "no alternation" message. In the worm verdict, "closest approach measured as mean distance" (`worm.cpp:166`) conflates two numbers; report the actual closest approach.

---

## 3. Gamer (fun, feedback, progression, goals, juice)

**What is good:**
- The look is strong: glowing discs, comet pulses, a living lamprey and a worm trail. See `lab-worm-1920x1080.png` and `lab-lamprey-world-844x390.png`.
- The score ring with its verdict sentence is clear.
- The task cards are attractive.

### P1-G1. Progression is dead: best scores are never recorded
- `Progress.record()` (`godot/game/progress.gd:18`) has **no caller** (checked with `grep -rn Progress.record godot`). Every task card reads "NOT TRIED" forever, even after a pass (`tasks-1920x1080.png`).
- **Fix:** call `Progress.record(task_id, score.value, score.passed)` when `tele.finished` flips true, and when `passed` first becomes true (`telemetry.gd:123,139`, or `lab.gd:_process`). Don't record runs started from REFERENCE (track `app.lab_reference` and a "circuit edited" flag in `lab.gd`).

### P1-G2. Difficulty curve: trivial, trivial, impossible
- Reflex and worm pass with one neuron (sections 1 and 2); lamprey cannot be built in the editor (P1-N2). There is no goal beyond "pass": no par, no stars, no neuron budget.
- **Fix:**
  - Add 3 stars per task: pass, pass on 5 seeds, pass within a neuron or synapse budget. Or a "score ≥ 90" tier, shown on the card and score card (`tasks.gd:45-49`, `score_card.gd`).
  - Order the tasks reflex → worm → lamprey and lock the lamprey behind a reflex pass.
  - Fix P1-N2 so the lamprey is hard rather than impossible.

### P1-G3. The end of an episode is a non-event
- At `tele.finished` the sim just freezes. Nothing celebrates a pass, says what to try next, or offers to re-run, and "PASSED" mid-run looks the same as at the end.
- **Fix:** add an end-of-episode overlay in `lab.gd` showing score, pass/fail, the best score, and RUN AGAIN / NEXT TASK / ANALYSIS. Add a short glow pulse on the score ring when it first passes (`score_card.gd`).

### P2-G4.
- **No audio.** The Audio settings tab controls nothing (`settings.gd:103`; no `AudioStream` anywhere). A per-spike click from the selected population, like an electrophysiology audio monitor, would be the most "real" juice available.
- **Every slider nudge reloads the engine and restarts the episode** (`lab.gd:155-160`), so tuning the worm costs 30+ s of model time per try. Use `SpikeSim.set_weight` for weight and delay edits, which already exists (`engine.cpp:633`), and restart only on structural edits.
- **Speed.** The default is 1x for a 120 s worm episode; default the worm to 10x.

---

## 4. Phone player at 844×390

### P1-M1. You can watch on a phone but not build
- **Inspector off-screen.** Selecting an axon pushes the inspector, with the weight and delay sliders, below the bottom edge. Only its header "CC_L > E_R inhibitory synapse" is visible (`lab-lamprey-edit-844x390.png`).
- **Toolbars eat the screen.** The top bar, tab row, palette and second palette row take about 190 of the 390 px, leaving about 190 px of editor (`lab-lamprey-844x390.png`).
- **Fix:**
  - On phone, show the inspector as a bottom sheet or overlay over the editor, with its own scroll, when something is selected (`lab.gd:64-65,288`).
  - Collapse BRIEF/REFERENCE/LOAD/SAVE/ANALYSIS into one "⋯" menu (`lab.gd:215-218`).
  - Merge the model and preset steppers into one "+ NEURON" sheet (`lab.gd:204-219`).

### P2-M2.
- **Channel cards overlap.** The kicker and the name collide ("SENSOR" over "drive", "MOTOR" over "left_motor") because the card is 52 px × 0.85 and the text is laid out for desktop (`editor_view.gd:48-49,307-309`; `lab-lamprey-844x390.png`).
- **Tasks screen.** BUILD/REFERENCE sit below the fold and need a scroll, and the motor chips are cut (`tasks-844x390.png`). Use a horizontal pager, or drop the icon height to 50 on phone (`tasks.gd:35`).
- **Analysis.** The per-neuron table disappears ("+ 18 more neurons") and the chart subtitle is clipped (`analysis-844x390.png`).
- **File dialog.** The LOAD/SAVE `FileDialog` is 900×560, larger than the screen (`lab.gd:83`); use `popup_centered_clamped`.

---

## 5. Researcher using `python/`

### P1-R1. No README, and PyNEST setup is not reproducible
- **No documentation.** There is no `README.md` anywhere: not in the repo root, not in `python/`. There is also no CHANGELOG, though `docs/PLAN.md` requires one.
- **PyNEST setup.** The parity test and the notebook look for PyNEST in `third_party/nest-install/...` (`python/tests/conftest.py:16-21`, notebook cell 1). But `scripts/build-nest.sh:13` only runs `make models nestkernel nestutil`: it never installs PyNEST. Parity passes in this container only because a PyNEST of the same version happens to exist at `/tmp/nest-install`.
- **Silent skip.** On a fresh machine the parity test **skips silently** (`requires_nest`, `conftest.py:62`), and `pytest python` stays green.
- **Fix:**
  - Add `scripts/build-nest.sh --pynest` (`make install` into `third_party/nest-install` with Python on, from the same pinned commit).
  - Add `SPIKE_REQUIRE_NEST=1`, which turns the skip into a failure, set in `python/CMakeLists.txt` for ctest.
  - Write `README.md` (top level: what it is, build, run, test, screenshots) and `python/README.md` (install, PyNEST path, `build` / `run` / `motifs` / `stats` example, parity guarantees and limits: same commit, same OpenMP setting, `threads`).

### P2-R2.
- **No tasks in Python and no task CLI.** A researcher cannot score a circuit or sweep parameters outside Godot; `spike-run` runs free circuits only. Add `spike-run --task <id> [--seed n]` printing the score JSON (`engine/src/spike_run.cpp`), or pybind the task library.
- **`run(drain=True)` is quadratic.** It re-reads and lexsorts the whole event history on every call (`python/spike/circuit.py:187-207`); keep an offset into `n_events`.
- **Python skips validation.** `build()` skips the C++ loader's validation (unknown keys, empty selectors, delay < resolution), so a file can build in PyNEST and be rejected by the game. Share one JSON Schema file and validate it in both.

---

## 6. Code reviewer (`engine/src`, `gdext/src`)

No memory-safety or threading defects were found. All NEST calls happen on Godot's main thread, and the SPSC ring has one ring per NEST thread with correct acquire/release ordering. NEST is used correctly: one engine instance, reset per load, re-prepare when a `poisson_generator` rate changes because the rate is applied in `pre_run_hook`. That re-prepare costs about 83 ms of wall time per 5 s of worm.

### P1-C1. Engine errors after `step()` are never surfaced
- `Engine::step` records failures in `error()` and returns (`engine.cpp:530-533`, `:571-574`). Neither `SpikeTask::step` (`gdext/src/spike_task.cpp:265-269`) nor `Telemetry.advance` (`telemetry.gd:119-120`) checks it.
- Examples: a circuit with `resolution_ms` 0.3 makes every 1 ms slice fail, yet the task world keeps advancing and scoring a frozen network. A NEST exception mid-run (kernel in an inconsistent state) leaves the game "running".
- **Fix:**
  - Make `SpikeTask.step` return a bool, or add `SpikeSim.last_step_ok()`, and have `Telemetry` stop and set `error` so `lab.gd` shows the banner.
  - Have the circuit loader reject a resolution that does not divide 1 ms when the circuit is used by a task.

### P2-C2. `bridge.cpp:355-359` builds error JSON by hand
- It escapes only quotes and backslashes. A NEST message with a newline produces invalid JSON; the chart then sits on "computing…" forever (`analysis.gd:48-50,66`).
- **Fix:** use nlohmann::json to build the string.

### P2-C3. Ring overflow on long `step()` calls
- `Engine::step(ms)` pops the ring once, after `nest::run(ms)` (`engine.cpp:559-563`). A single call longer than about 131 k spikes per thread (`ring_backend.h:55`), for example `step(20000)` on the 1 k network, drops spikes. It only shows up in `dropped()`.
- **Fix:** split `step()` internally into runs of at most 100 ms (identical spikes), or document the limit in `engine.h`.

### P2-C4. `Engine::set_input` error is sticky
- An unknown channel sets `error` permanently (`engine.cpp:509`), and tasks call `set_input` every slice. A circuit without `ASER` therefore reports `set_input: unknown input channel 'ASER'` for the rest of the run.
- Nothing checks that a circuit declares the task's channels, although `task.h:37-38` says it must.
- **Fix:** validate the channels in `Telemetry.setup` (show a banner listing missing channels), and keep a separate `warnings()` list.

### P2-C5. The `SpikeSim` constructor can throw through Godot
- `shared_engine()` constructs `spike::Engine`, and `nest::init_nest` can throw, inside `SpikeSim::SpikeSim()` (`spike_sim.cpp:88-94`). That is a C++ exception across the GDExtension boundary, which terminates the process.
- **Fix:** wrap it in try/catch and set an error state.

### P2-C6. Undocumented env hooks
- `godot/main.gd:4-7` lists the `SP_*` hooks but omits `SP_LABTAB` (`lab.gd:80`, used by `shots.sh`), `SP_CIRCUIT` (`lab.gd:99`), `SP_SIM_UNTIL` and `SP_BUDGET_US`.

### P2-C7. Budget check every 4 slices
- `Telemetry` checks its wall-time budget only every 4 slices (`telemetry.gd:125`). A pathological `aeif_cond_alpha` (huge drive) took 60 ms per slice in the harness, so frames take at least 240 ms. Check the budget after every slice whose duration exceeded `budget_us / 4`.

---

## Release process (lead)

- **P1:** `docs/REQ-0.1.md` still shows every item as "Not started".
- **P1:** there is no `CHANGELOG.md` and no README (see P1-R1). Both are required by the process in `docs/PLAN.md`.

---

## Verdict

**Not ready to ship as is; close, though.**

The engine, NEST integration, determinism, parity and reference circuits are solid, and the instruments look great.

What blocks the release is the novice's first five minutes:
- a half-wired circuit is rejected and the lab falsely says "Free run" (P0-1);
- the editor defaults give silence with no explanation (P1-N1);
- the lamprey cannot be built with the editor at all (P1-N2).

What should be fixed alongside:
- progression never records (P1-G1);
- the motif chart and lamprey scoring can mislead a scientist (P1-S1, P1-S2, P1-S3).

Fix P0-1 and the P1 list, re-run the novice harness until all three tasks can be passed from an empty bench with editor controls only, and 0.1 is a release.

### P0 / P1 grouped by the files a fix touches (for parallel agents)

| Agent | Files it owns | Items |
|---|---|---|
| **A. Engine core** | `engine/src/circuit.cpp`, `engine/src/engine.cpp`, `engine/include/spike/engine.h` (add-only), `engine/tests/test_engine.cpp`, `docs/CIRCUIT-FORMAT.md` | P0-1(a) accept empty `targets`/`neurons`; P1-N2 seeded random initial `V_m` when not given; P1-C1 a step-ok flag (add-only API) |
| **B. Python mirror** | `python/spike/circuit.py`, `python/tests/*`, `python/CMakeLists.txt`, `scripts/build-nest.sh`, `README.md`, `python/README.md` | P0-1 mirror the empty selectors and the `V_m` jitter (keeps parity); P1-R1 `--pynest`, `SPIKE_REQUIRE_NEST`, READMEs |
| **C. Tasks** | `engine/src/tasks/lamprey.cpp`, `reflex.cpp`, `worm.cpp`, `engine/tests/tasks_*`, `docs/circuits/*.md` | P1-S1 lamprey phase/frequency validity and a one-sided lesion test; P1-S4 reflex pool-scaled burst threshold and harder noise; P1-N1 silent-motor verdicts; P1-N3 worm brief |
| **D. Analysis** | `godot/ui/screens/analysis.gd`, `gdext/src/bridge.cpp`, `gdext/src/spike_sim.{h,cpp}` (add-only), `engine/src/analysis/analysis.cpp` | P1-S2 non-finite z display, population/neuron toggle, swap-acceptance warning |
| **E. Lab runtime** | `godot/game/telemetry.gd`, `godot/game/progress.gd`, `godot/ui/score_card.gd`, `godot/ui/screens/tasks.gd`, `gdext/src/spike_task.cpp` | P0-1(b) create the task before loading the engine, unwired-channel verdict; P1-C1 stop and report on engine error; P1-G1 `Progress.record`; P1-G2 stars and tiers on the cards and score card |
| **F. Lab UI** | `godot/ui/screens/lab.gd`, `godot/ui/editor_view.gd`, `godot/game/circuits.gd` | P0-1 deferred banner; P1-N1 input weight default by target model; P1-N2 advanced neuron parameters and a noise input; P1-S3 drop STDP/Tsodyks; P1-S4 TEST-on-5-seeds button; P1-G3 end-of-episode overlay; P1-M1 phone inspector sheet and collapsed toolbars |
| **Lead** | `docs/REQ-0.1.md`, `CHANGELOG.md` | status table, changelog |

**Coordination notes:**
- E and F both touch the reload path (`telemetry.gd` ↔ `lab.gd:_reload`). Agree first that `Telemetry.setup` always sets `task` and that `lab.gd` reads `tele.error` after `build()`.
- A and B must land together, or parity breaks.
- If A's option (a) for P0-1 is taken, E only needs the task-first and verdict parts.
