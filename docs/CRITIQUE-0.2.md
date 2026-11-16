# v0.2 persona critique: Body systems

Judge's review of release 0.2 at commit `267fef9` (tree as of 2026-11-16). No code was changed for this review.

**How it was checked**
- Build already done in `build/` (ctest 28/28, smoke OK, as handed over). Re-ran `ctest -R "nestml_parity|python_pytest"`: both pass (80 pytest, NESTML spike parity against PyNEST in `/tmp/nest-install`).
- **Seeds.** Ran every new reference circuit on seeds 1-12 with `build/engine/tasks_circuits_test run <task> circuits/<task>.json 1,...,12` (it reseeds the circuit as `1000*seed+1` and the task with `seed`), and the lab's own TEST x5 (`SP_TASK=<t> SP_REF=1 SP_TESTNOW=1 godot --headless --path godot`, seeds 101-105, circuit seed fixed).
- **Novice harness.** A headless GDScript (`godot --headless --path godot -s novice.gd`) that builds circuits only with what the lab offers: the lab's task skeleton (`lab.gd:_skeleton`, copied verbatim), `Circuits.add_card` with the parts-bin card, `Circuits.add_projection` (default weight and 2 ms delay), the editor's first-connect input weight (`editor_view.gd:414-416`), `Circuits.set_n`, `Telemetry.sanitized`, then `task.step(sim, 1.0)` for the whole episode exactly as `Telemetry.advance` does. Results are quoted inline.
- **Screenshots** (xvfb, opengl3, the `scripts/shots.sh` mechanism) of every chapter map, every real task's body view, the empty skeletons, the parts picker, lesion lab, end card and sandbox, at 1920x1080 and 844x390: `docs/screenshots/critique-0.2/` (56 files). I looked at every one.

Severity: **P0** blocks the release. **P1** should be fixed for 0.2. **P2** can wait.

---

## 1. Novice (can they progress through the campaign from the briefs and the parts bin?)

**Short answer.**
- **heart_pacemaker:** yes, trivially. One SA-node card wired to `sa_beat` passes (85). Five *uncoupled* identical SA cells score 100, more than the reference (see S1).
- **stretch_reflex:** yes, but only by accident. The brief's wiring with editor defaults fails (75, "add reciprocal inhibition"), and the "give both pools tonic drive" instruction cannot work, because the lab makes `drive` a 1 Hz Poisson source. A sweep of 24 UI-reachable variants found passes (up to 100), but on a jagged landscape where neighbours fail.
- **heart_autonomic, baroreflex, fight_or_flight:** **no.** Nothing the editor can express can pass them, and the campaign dead-ends there.

### P0-1. Three of the eleven tasks cannot be built in the lab, and the campaign dead-ends at the Heart chapter
- **Evidence:**
  - `heart_autonomic`, `baroreflex` and `fight_or_flight` read the heart's drive from `Engine::modulator("ACh"/"NE")` (`heart_autonomic.cpp:17-21,128-129`). That needs (a) outputs of kind `"modulator"` and (b) a circuit `modulation` table that maps ACh/NE onto the SA cell's `ach`/`ne` (`docs/CIRCUIT-FORMAT.md` "Neuromodulation").
  - The lab cannot make either. `_skeleton` creates every motor as `{"kind": "rate"}` (`lab.gd:189`). The channel inspector has no kind, modulator, gain or tau control (`lab.gd:694-720`). Nothing in `godot/` writes `"modulation"` (grep finds only reads in `telemetry.gd:198-199`).
  - Harness, heart_autonomic built as the brief says (SA cell → `sa_beat`; vagal cell on `hr_too_high` → `vagal`; sympathetic cell on `hr_too_low` + `exercise_cue` → `sympathetic`): **value 0**, `ach_rest=0.000 ne_peak=0.000`, verdict "Resting tachycardia ... is the vagal branch working?". The verdict sends the player looking for a fix the editor does not offer.
  - Gating: a chapter opens when half of the previous chapter's tasks are passed, and nodes unlock in order (`campaign.gd:131-151`). Heart has 3 nodes, so Breathing needs `heart_pacemaker` **and** `heart_autonomic`. Breathing and Autonomic (4 tasks, 2 new cell types) are therefore unreachable without the loophole in G2.
- **Fix:**
  - Lab: the output inspector gets a kind switch (`rate` | `modulator NE/ACh/DA` with gain and tau, defaults 0.05 / 1000 ms). The skeleton takes each channel's kind from the task. Best: add `Task::channel_kinds()` (add-only, `task.h`), or copy the kinds from the reference circuit's channels (`Campaign.ref_path`).
  - Lab: add a "modulation" row to the neuron inspector ("responds to NE / ACh: param, gain"). Or have the engine apply a **per-model default table** when a circuit has none (REQ A literally asks for "a documented table" per model: SA `ach`/`ne` gain 1, Butera `ne`/`ach`, ICC `ach`/`ne`). The engine fix is preferred: `engine/src/engine.cpp` modulation setup, `engine/src/catalog/catalog.cpp` (the card owns its default rows), `docs/CIRCUIT-FORMAT.md`, and the mirror in `python/spike/circuit.py`.
  - Re-run the harness until heart_autonomic, baroreflex and fight_or_flight pass from an empty bench. Add that to smoke (C1).

### P1-N1. The lab gives the new tasks the wrong sensor kinds: "tonic drive" is a 1 Hz Poisson train
- **Evidence:**
  - `SENSOR_KIND := {"lamprey": "dc"}` (`lab.gd:10`), so every other sensor is created as `poisson` (`lab.gd:187-188`).
  - The spinal tasks set `drive` to 1.0 every slice, meaning "dc, scaled by the weight" (`stretch_reflex.cpp:9`). As a Poisson input that is **1 spike/s**.
  - Screenshot `sk-stretch_reflex-1920x1080.png`: the inspector says "drive: sensor, driven by the task, poisson". All spinal references use `dc` for `ag_Ia`/`ant_Ia`/`ag_II`/`drive`/`go`/`target`. The stretch-reflex doc explains why Poisson Ia afferents make the limb tremble.
  - Harness: the brief's wiring on 20-cell iaf pools scores 75 ("antagonist suppressed 0%"). The same circuit with `drive` as dc 300 pA scores 79.9, but the UI cannot do that.
  - With `aeif_cond_alpha` motoneurons (the card a novice would pick for "motoneuron") both pools stay **silent** (value 15) with or without the change.
- **Fix:** derive input kinds from the task (the same `channel_kinds()` as P0-1) or from the reference circuit's inputs, in `lab.gd:_skeleton`. Let the channel inspector switch dc/poisson for task sensors. Assert in smoke that every skeleton channel kind matches the reference's (C1).

### P1-N2. Default weights for the new cards are off by one to two orders of magnitude
- **Evidence:**
  - Every NESTML card and several NEST cards report `input_weight 1.0` with an empty unit (`catalog.cpp:33`; dumped through `SpikeTask.catalog_json()`: `sa_node_pacemaker`, `butera_burster`, `icc_slow_wave`, `ht_neuron`, `pp_psc_delta`, `parrot_neuron`, `hh_cond_*`).
  - `Circuits.weight_of` falls back to that (`circuits.gd:30-33`).
  - The breathing reference uses 0.005 nS drive and about 0.09 nS coupling onto Butera cells. The editor gives 1.0.
  - Harness, breathing with the brief's wiring and defaults: one burster → phrenic never fires (value 0). Ten coupled bursters → **168 breaths/min** (value 30).
  - The game's own `WEIGHT` table (iaf 250 pA, aeif 20 nS) and the catalogue's (iaf 80 pA, aeif 8 nS) disagree.
- **Fix:** give every card a measured default weight and unit, chosen as the weight that makes one presynaptic spike train at 20 Hz move the cell noticeably. Have `spike-catalog` compute it and write it into `catalog/thumbs.json`. Use one table: drop `Circuits.WEIGHT` in favour of the catalogue (`godot/game/circuits.gd:18,30-33`, `engine/src/catalog/catalog.cpp`).

### P1-N3. The parts picker hides the chapter's own cell type at the bottom
- **Evidence:**
  - Every lab opens with `PARTS: iaf_psc_alpha` (`lab.gd:28`).
  - The picker sorts cards by chapter (`catalog.gd:cards()`), so in the heart lab the SA-node card the brief asks for is card 22 of 24. That is about 3 desktop screens down, and about 12 phone screens down, two cards per row (`parts-pick-1920x1080.png`, `parts-pick-844x390.png`).
- **Fix:** in the lab's picker (`lab.gd:_open_parts`, `godot/ui/parts_bin.gd`), show "For this task" first: the models the reference circuit uses. Default `part_card` to the reference's first non-iaf model.

### P1-N4. Peristalsis asks for 32 drag-wirings before anything is scored
- **Evidence:**
  - 8 sensors plus 24 motors (`ach0-7`, `no0-7`, `icc0-7`). `Wiring` requires every one (`task_util.h:99-122`), and `Telemetry._apply_issue` blanks the pass until then.
  - `sk-peristalsis-1920x1080.png`: 16 channel cards spill off the bench, and the empty-bench hint is drawn over them. On a phone (`sk-peristalsis-844x390.png`) only 8 cards are visible.
  - Fight or flight has 7 motors.
- **Fix:**
  - Engine: make `icc*` and `no*` optional, as the spinal tasks already do for `ag_Ib`/`ag_II` (`peristalsis.cpp:65-66,106`).
  - Editor: "segment" wiring. Wire a population of n = 8 to `ach*` in one drag (neuron k → channel k), in `editor_view.gd:_make_link`.

### P2-N5.
- The campaign card cuts the brief at 420 characters (220 on phone) (`tasks.gd:129`). For heart_pacemaker that drops the 60-80 bpm target and the coupling hint (`ch-heart-1920x1080.png`).
- An empty stretch-reflex bench shows a score of **15**, because "no ringing" gets its 15 % (`stretch_reflex.cpp:115-118`). Force the value to 0 while `!wiring_.ok()`.
- Verdict grammar: "No reflex to the tendon taps in the either pool" (`stretch_reflex.cpp:137`).

---

## 2. Physiologist / computational neuroscientist (models, physics, scoring, references, lesion claims)

**What is good.**
- The docs are unusually candid: "This is a re-fit, not a copy" for the SA cell, "phenomenological" for ICC, "game value" for the chemoreflex gain, the posture doc's own seed 4-8 table, and the honest "partial" lesion in peristalsis.
- The heart module is a sound 2-element Windkessel with volume conservation.
- The Renshaw task's control-curve idea (judging tremor against the same-rate control) is a genuinely good piece of experimental design.

### P1-S1. The SA-node task rewards clones, not entrainment: five uncoupled identical cells score 100
- **Evidence (harness):**
  - one SA cell: 85, pass;
  - one population of 5, **no synapses**: **100**, synchrony 1.0;
  - two populations with the reference's I_e (−3.5 / +2.5 pA), uncoupled: 14.3, which is the intended lesson;
  - the reference: 99.9.
- **Cause:**
  - The seeded initial `V_m` is drawn from U(E_L, V_th) (`engine.cpp:269-290`). For `sa_node_pacemaker`, E_L = −30 mV and V_th = 0 mV (`sa_node_pacemaker.nestml:87,107`). Every clone starts depolarised, and all fire within 20 ms (`spike-run`, 3 cells: first spikes at 4.7, 12.3 and 20.1 ms, then 965.5, 966.5 and 967.4 ms).
  - The other gates start identical, so the cluster stays in lock-step inside the 40 ms chi bins.
  - The brief's "real node cells are not identical" is not enforced.
- **Fix:**
  - Engine (`engine.cpp` V_m draw): jitter only for models with a real resting potential, or draw from a model-declared range. The catalogue card can declare `vm_init_range`, e.g. SA [−74, −45] mV.
  - Task (`heart_pacemaker.cpp`): add per-cell heterogeneity itself (a seeded ±3 pA bias spread through `set_neuron_param`), so an uncoupled cluster fails and coupling is what earns the score.
  - Update `docs/circuits/heart_pacemaker.md` and add an uncoupled-clones lesion to `tasks_heart_test.cpp`.

### P1-S2. Narrow passing plateaus: four references fail on seeds the docs don't mention, and stretch_reflex fails the lab's own TEST x5
Seeds 1-12, `tasks_circuits_test run`. Wall time is for the whole episode in C++.

| Task | Fails on | Pass rate | Episode / wall |
|---|---|---|---|
| heart_pacemaker | none (99.9 throughout) | 12/12 | 30 s / 2.3 s |
| heart_autonomic | none (84-86) | 12/12 | 85 s / 7 s |
| baroreflex | none (98.6-100) | 12/12 | 140 s / 5 s |
| stretch_reflex | **6, 12** (49, "no reflex to taps in the flexor/extensor pool", while resistance 0.52 / 0.46) | 10/12 | 12.5 s / 3.1 s |
| renshaw | none (94-100) | 12/12 | 13.5 s / 2.9 s |
| posture | **7, 9, 11** (sag 0.29 / 0.16 / 0.17 rad) | 9/12 | 13.5 s / 2.6 s |
| reach | **6, 8, 10** (creep before go; final error) | 9/12 | 13.3 s / 5.4 s |
| breathing | none | 12/12 | 150 s / 12.5 s |
| pupil | **8** ("constriction latency 1.0 ms", see S3) | 11/12 | 40 s / 1.7 s |
| peristalsis | none | 12/12 | 40 s / 7.5 s |
| fight_or_flight | none (99.1-99.9) | 12/12 | 100 s / 7.5 s |

- The lab's TEST x5 on the references (seeds 101-105, circuit seed fixed):
  - stretch_reflex **3 of 5** (49.0 on seed 104), so the reference itself cannot earn the second star;
  - posture, reach and pupil 5 of 5.
  - With the circuit reseeded as well: posture 3/5, **reach 2/5**, stretch 4/5, pupil 4/5.
- Known before this review: reach seed 6 and posture seed 7. New here:
  - stretch_reflex 6 and 12 (and 104);
  - posture 9 and 11 (and 103, 105);
  - reach 8 and 10 (and 101-103);
  - pupil 8 (and 101).
- `docs/circuits/stretch_reflex.md` shows only seeds 1-3. `reach.md` says "seeds 4 and 5 pass, seed 6 fails".
- **Fix:**
  - Re-tune for margin: stretch_reflex's tap detector (S3), posture's tone balance, reach's hold weights and creep.
  - Make the CTests check 10 seeds, not 3 (`engine/tests/tasks_spinal_test.cpp`, `tasks_autonomic_test.cpp`).
  - Publish the 10-seed table in each `docs/circuits/<task>.md`. Until the margins are fixed, say plainly that the reference passes N/10.

### P1-S3. Measurement artefacts decide passes, and the verdicts then give the wrong advice
- **Pupil latency of 1 ms** (seed 8):
  - `latency()` measures from the 3-4 s *mean* diameter and returns the first sample past 10 % (`pupil.cpp:209-217`). Pupil noise at the instant of the step is already past the threshold.
  - Fix: take the baseline as the value at t0 (or its 200 ms mean just before t0), require the crossing to persist for 50 ms, and treat < 50 ms as "no response yet".
- **Stretch-reflex taps:**
  - Seeds 6 and 12 fail with "No reflex to the tendon taps in the flexor pool... Wire each Ia pool to its own motoneurons", on the unmodified reference, with 52 % resistance.
  - The PSTH criterion ("above baseline + max(4 sqrt(b/6), 1.2) and 20 % of the pool's cells", `stretch_reflex.cpp` header) is brittle at 6 taps.
  - Fix: use 12 taps per muscle or a pooled threshold, and only blame the wiring when the Ia → MN path does not exist (the task can see the circuit's connectivity through `Engine`).
- **Reach verdict:**
  - Seed 6 says "Ends 0.07 rad from the target (limit 0.12)". It prints the mean final error against the per-reach limit, while one reach failed (`reach.cpp:114,221`).
  - Fix: report the worst reach.

### P1-S4. Misleading physiology in claims and lesion tables
- **"Ia → MN ×4: hyper-reflexia, resistance 0.00"** (`stretch_reflex.md:61`, `tasks_spinal_test.cpp:195`).
  - Clinical hyper-reflexia means *more* stretch resistance and clonus. The model's zero comes from the motoneuron pools saturating against `rmax_hz = 50` (`spinal_limb.h:36,94`).
  - That is a model ceiling, not the lesion's biology. Relabel it ("gain past the model's saturation, the reflex is lost"), or raise `rmax` so that ×4 gives clonus, which is the textbook outcome.
- **The SA cell's "intrinsic" rate is the human resting rate.**
  - The cell beats 68/min alone (`sa_node_pacemaker.md`), the rate a human reaches *with* vagal tone; the intrinsic rate is about 105 (Jose & Collison 1970, cited in `heart.md`).
  - As a result the vagal brake is nearly absent: the baroreflex doc says a vagal cut raises HR by only about 2.5 bpm. That contradicts heart_autonomic's own header, "vagal tone dominates the resting heart rate", and the brief's "cutting the vagus should show up as resting tachycardia".
  - Fix: re-fit the cell to about 100/min at ACh 0, with resting ACh bringing it to 60-70. Or state the limitation in the brief and the doc.
- **heart_pacemaker:** "Autonomic tone is held at rest (sympathetic 0.2, vagal 0.6)" (`heart_pacemaker.cpp:7`, `.md`). With `sa_rate_hz = 0` the vagal input acts on nothing; the cell has no ACh. Delete the sentence or apply the tone.
- **renshaw:**
  - `improvement` is judged against a control curve measured on *the reference circuit with its Renshaw cells cut* (`renshaw.md` table).
  - A player's circuit with other pool sizes, models or weights is judged against a curve that was not measured for it. The lesson ("RC cuts tremor") becomes "match our pool's tremor-vs-rate curve".
  - Fix: compute the control on the player's own circuit (a second, Renshaw-silenced copy run inside the task, as stretch_reflex already does with its open-loop limb), or at least say so in the brief.
- **heart_autonomic** gives the circuit the *error signal* (`hr_too_low`, `hr_too_high`, `heart_autonomic.cpp:10-13`). That is a controller handed its error, not a sensor. Say in the brief that these stand in for the baroreceptor and central command.

### P1-S5. The muscle sees a pool *rate*, so one motoneuron drives a whole muscle and pool size is meaningless
- **Evidence:**
  - Excitation = (mean rate *per neuron* of the pool, low-passed 15 ms) / 50 Hz (`spinal_limb.h:94-95`), and the muscle then makes its own regular unit spike trains (`docs/body/muscle.md` "Rate mode").
  - So one motoneuron at 50 Hz is full excitation of 20 motor units. Motoneuron spike timing and synchrony never reach the muscle, which undercuts the Renshaw story (decorrelation of motor units).
  - The "pass with ≤ par neurons" star counts neurons that do no work.
  - REQ B asks for "twitch summation ... from motor-unit spike trains".
- **Fix:** use the muscle's existing spike mode (`MuscleInput::spikes[i]`): map pool neuron k to motor unit k, ordered by recruitment threshold (size principle in the circuit: give neurons rising rheobase). Or scale excitation by pool size / 20, so a single neuron can only drive its unit. That touches `spinal_limb.h` and the four spinal references (regenerate with `circuits/tools/gen_spinal.py`).

### P1-S6. "Validated against its paper figure" (REQ A) is not met, and the catalogue thumbnails mislead for slow cells
- **Evidence:**
  - All three NESTML docs say no figure was digitised: SA "not claimed to match a paper figure", Butera "the test asserts ordering and range relations, not exact paper numbers", ICC "check direction and a ±5 % window".
  - That is honest, but REQ A asks for figure validation.
  - The f-I thumbnails come from 1 s windows (`docs/CIRCUIT-FORMAT.md` "Parts bin"). For a 1 Hz pacemaker that gives 0.8, 1.2, 1.0, 1.0, 1.2, 1.2 Hz from −5 to +4 pA (`catalog/thumbs.json`), non-monotonic and quantised. The doc's own measurement is 58.8 → 74.7/min over −3 → +3 pA. ICC: 0.02-0.07 Hz.
- **Fix:**
  - Butera: digitise Butera 1999 Fig. 2-3 (period and burst duration against g_tonic) and test against it in `engine/tests/test_nestml_butera.cpp`. SA: compare with the Kurata 2002 rate-vs-ACh and rate-vs-ISO curves, or relabel the requirement as "behavioural, not figure" in `REQ-0.2.md`.
  - Catalogue: measure f-I over ≥ 10 cycles, with a card-declared window (`engine/src/catalog/catalog.cpp`, `spike-catalog`), and show units on the axes (`godot/ui/parts_bin.gd`).

### P1-S7. The lesion lab reports a single seed, on references whose plateaus are narrow
- `Lesion.Runner` re-runs "for the circuit's own seed" (`lesion.gd:6-7`). With posture failing intact on 3/12 seeds, a "FAIL" row can be seed noise, and the table has no intact variability to compare against.
- **Fix:** run every lesion row on 3-5 seeds (intact included), show mean ± range and k/n passed, and mark rows whose intact baseline itself fails on some seeds (`godot/game/lesion.gd` Runner and `markdown`/`csv`).

### P1-S8. The ECG is synthetic and aliased: a sinus rhythm with 0 % variation is drawn as an irregular trace
- `body_views.gd:106-157`: the trace is a fixed P-QRS-T template whose phase is integrated backwards from the `hr_bpm` history, at 2 px per sample.
  - A QRS 7 ms wide, sampled at about 23 ms per sample, aliases into spikes of random height (`w-heart_pacemaker-1920x1080.png`, `w-heart_autonomic-1920x1080.png`, `overlay-heart-1920x1080.png`: CV 0.0 % but an irregular-looking ECG).
  - Pauses and extra beats from an unlocked cluster are not shown, because `hr_bpm` is smoothed.
- **Fix:** draw complexes at the actual beat times. Expose a ring of beat times as a readout (`last_beat_ms` exists) or from `Telemetry` hist, and render each QRS analytically at its pixel, not by sampling.

### P2-S9.
- `baroreflex.md` gives the sympathetic pressure gain as both "90 mmHg per unit" and "180 mmHg per unit".
- `breathing.md`: under 5 % CO2, PaCO2 rises about 10 mmHg against 3-6 in humans. It is acknowledged, but the pass bar (< 52 mmHg) is set to let it through. Tighten to < 48 once the chemoreflex gain is raised.
- Pupil: the pass window for constriction latency is 120-450 ms against a human 200-250 ms. The reference sits at 167-184 ms; narrow it to 150-350.
- ICC: the model's slow-wave waveform is unused. The gut takes a fixed half-sine plateau triggered by the ICC spike (`peristalsis.md` "Limits"), so the ICC is a spike clock.
- The 0.1 note on current-based inhibition applies again: all autonomic relays are `iaf_psc_alpha`.

---

## 3. Gamer (pacing, feedback, juice, lesion lab)

**What is good.**
- The lab is a beautiful instrument: glowing pools, travelling pulses, the event timeline with tolerance bands, and gauges with live history (`w-fight_or_flight-1920x1080.png`, `w-baroreflex-1920x1080.png`).
- The pupil's eye view (`w-pupil-1920x1080.png`) is the best body view: legible, reactive, and shows the light.
- The end card has clear next steps, with energy in ATP.

### P1-G1. The campaign stalls at Heart (P0-1). Past it, the only way on is to edit a reference
- `_on_structure()` clears the "from reference" flag on **any** edit (`lab.gd:223-225`), and passes then record progress (`lab.gd:427,430`).
- Nudging one weight slider of the REFERENCE circuit therefore earns the pass and the next unlock. TEST x5 then gives star 2, and star 3 when the reference is at or under par: heart_pacemaker 7/7, heart_autonomic 37/45, baroreflex 47/48, pupil 30/30, peristalsis 49/49, fight_or_flight 45/45.
- TEST also records the **maximum** of the 5 scores as "best" (`lab.gd:545`).
- **Fix:**
  - Keep a structural fingerprint of the reference (populations, projections, weights rounded to 10 %). Award stars only when the circuit differs from it beyond trivial edits, or mark "derived from reference" runs and give them star 1 at most (`lab.gd:171-176,223-225`).
  - Record the mean of the TEST, not the max (`lab.gd:545`, `godot/game/progress.gd:record`).

### P1-G2. REQ D/E rewards are missing: no energy star, no "biologically faithful" star, no 0.2 analysis in the game, and no new synapses in the editor
- Stars are pass, 5 seeds and par (`progress.gd:3-4,stars_for`). REQ D asks for "pass, under par, low energy" plus a motif-similarity "biologically faithful" star. Grep finds no `faithful` or `elegan` anywhere in `godot/`.
- PRC, burst detection, HRV and the 4-node census exist in C++ and Python but are not bound in `gdext` (the `SpikeSim` bindings stop at `motif_json`/`spike_stats`). The analysis screen does not offer them, although HRV is the natural readout for three heart tasks.
- The editor offers only `static_synapse` (`circuits.gd:20`, `lab.gd:687`). REQ A asks for stdp, stdp_dopamine, tsodyks2, gap_junction (heart cluster!) and quantal_stp in the editor. No reference uses any of them; heart_pacemaker explains why gap junctions could not be used.
- **Fix:**
  - Either land these (gdext bindings for `hrv`/`bursts`/`prc`/`motif4`; an analysis tab per task kind; a motif-distance star from `analysis.cpp` against the reference; an energy star from `Telemetry.spike_rate_hz()` against the reference's rate; synapse choices in `circuits.gd:SYNAPSES`), or move them to 0.3 in `REQ-0.2.md` and `CHANGELOG.md`.
  - Shipping the REQ as written while these are silently absent is the problem.

### P1-G3. Long episodes at 1x, and instruments on the wrong timescale
- Episode lengths: breathing 150 s, baroreflex 140 s, fight_or_flight 100 s, heart_autonomic 85 s. Only the worm defaults to 10x (`lab.gd:168`).
- The scope and raster windows are fixed at 500 ms (`trace_view.gd`, `raster_view.gd`). For a 1 Hz SA node, 0.25 Hz breathing or a 0.2 Hz ICC, the membrane traces are flat lines and the raster is empty (`w-heart_pacemaker-1920x1080.png`: "SA_c" and "SA_p" flat; `w-breathing-1920x1080.png`: the Butera trace flat between bursts). The most important cells of the chapter are invisible on the instruments.
- **Fix:**
  - Default speed = clamp(duration / 30 s, 1, 10) (`lab.gd:_start`).
  - Make the scope window follow the slowest watched cell's ISI (≈ 3 cycles), or add a window stepper. The task can also suggest a window, e.g. `Task::scope_ms()` add-only, or simply from the `chapter`.

### P1-G4. Art: the chapter maps and three body views do not read as anatomy
Seen in the `ch-*` and `w-*` shots.
- **Heart chapter** (`ch-heart-1920x1080.png`): a **valentine heart** (the parametric 16 sin³ curve, `tasks.gd` organ "heart", and again in `body_views.gd:118-120`), with two sticks for vessels. There are no atria, so "the SA node in the right atrium" (the brief's first sentence) has nowhere to be.
  - Fix: draw a stylised 4-chamber frontal section (RA, RV, LA, LV, aorta, SVC, IVC). Put heart_pacemaker at the SA node at the SVC-RA junction, heart_autonomic on the vagus and the sympathetic cardiac nerves entering the base, and baroreflex at the carotid sinus and aortic arch, with the NTS in a brainstem inset.
  - In the body view, pulse the atria then the ventricles on each real beat (atrial-ventricular delay), with the SA node flashing.
- **Breathing** (`ch-breathing-1920x1080.png`, `w-breathing-*`): two symmetric ellipses with straight-line "bronchi" that cross the lung border, a diaphragm arc drawn *through* the lower lobes, and one node card over the carina. The task is about a brainstem rhythm and there is no brainstem.
  - Fix: asymmetric lungs (cardiac notch on the left, 3 vs 2 lobes), the diaphragm dome below the bases moving with volume, and a brainstem/medulla inset with preBötC and RTN, joined to the diaphragm by the phrenic nerve, where the node sits.
- **Spinal** (`ch-spinal-1920x1080.png`): a row of rounded rectangles reads as piano keys or a bar chart.
  - Fix: a lateral spine with intervertebral discs and the cord, plus a spinal-segment cross-section (dorsal and ventral horns) inset where the reflex arc nodes sit, and a limb.
- **Autonomic** (`ch-autonomic-1920x1080.png`): the eye, a large empty ellipse, the valentine heart again, and a sine "gut".
  - Fix: a body outline with the sympathetic chain and the vagus. Eye, heart and gut sit at their anatomical sites, joined by the two divisions (orange sympathetic, cyan parasympathetic), so the "two opposing nerves, one body" blurb is drawn.
- **On every map, the node cards (320 × 110 px) cover the organ they label.** Use small pins on the anatomy, with the card shown on hover or selection (`tasks.gd` node layout).
- **Limb view** (`w-stretch_reflex`, `w-posture`, `w-reach`):
  - The arm is a small capsule in a large empty panel, with no horizontal reference, gravity arrow, push arrows or target ghost. The muscles are flat bars labelled "agonist"/"antagonist" that collide on phone (`w-posture-844x390.png`).
  - The stretch reflex reads "**11.5 deg, target 0.0, off by 11.5**" in red at rest, because `LimbTaskBase::target_rad()` defaults to 0 (`spinal_limb.h:262`) while the rest angle is 0.2 rad.
  - Fix: override `target_rad()` in `stretch_reflex.cpp`/`renshaw.cpp`; in `body_views.gd` draw the upper arm fixed, a horizontal line, the target ghost, a load or gravity vector and push arrows from the timeline, and fusiform muscle bellies whose thickness follows activation.
- **Gut** (`w-peristalsis-1920x1080.png`): eight rounded squares with a white dot.
  - Fix: a tube whose wall constricts behind the bolus and relaxes ahead, with circular-muscle bands coloured by ACh/NO, so the "law of the intestine" is visible.

### P1-G5. The lesion lab works but does not reward
- `lesion-heart-1920x1080.png`: the rows are correct (intact 100, "cut SA_c > SA_p + cut SA_p > SA_c" FAIL 11, −89). But:
  - the live view freezes at t = 0 after RUN ("0 spikes, 1.0x of 20x");
  - nothing says what the result *means*;
  - there is no prediction step.
- The docs already contain an "Expected biology" column, e.g. "sinus node dysfunction, exit block", "Horner-like", "oculomotor palsy".
- **Fix:**
  - Before RUN, ask "what will happen?" with 3 choices taken from the score parts. After it, reveal the clinical correlate. Ship each reference's lesion table (pre/post → name, expected signature) as data, via a task method or `docs/circuits` JSON.
  - Animate the live view on the lesioned circuit while the row runs.
  - Count a "lesion bestiary" per chapter as a collectible (`godot/game/lesion.gd`, lesion section of `lab.gd`).

### P2-G6.
- Readout strips print raw keys and floats: "co l min 5.11", "beats 23.00", "cells 7.00", "blood lost ml 0.00". Units are doubled: "map mmHg (mmHg)", "hr bpm (bpm)".
- Timeline labels overlap: "upright up" in `w-baroreflex`.
- Score chips are cryptic: "settle haem s 0", "tail lie 0".
- Big references are illegible in the editor. `w-fight_or_flight-1920x1080.png`: HYP, RVLM, SG and SV are stacked on top of each other with unreadable labels. `w-peristalsis`: motors run off-screen, and FIT does not fit them.
- Add an auto-layout on load (layered by path from sensors to motors) in `editor_view.gd`.

---

## 4. Phone 844x390

The 0.1 fixes hold (the WORLD/CIRCUIT/SCOPES tabs and the MENU), and the world views fit (`w-fight_or_flight-844x390.png` is good).

### P1-M1. Building a v0.2 circuit on a phone is not practical
- **Parts picker** (`parts-pick-844x390.png`): two cards per row, each about 900 px tall, so the chapter's cell sits about 12 screens down (see N3).
- **Peristalsis skeleton** (`sk-peristalsis-844x390.png`): 32 channel cards, 8 visible at a time; the hint text is drawn over them.
- **Lesion lab** (`lesion-heart-844x390.png`): the sheet covers about 75 % of the editor. The circuit is about 70 px tall, with labels overlapping ("SA_c ×8 p ×4"), and the table shows one row.
- **Fix:**
  - Phone parts picker as a compact list (title, family and an f-I sparkline), with "For this task" on top.
  - The lesion sheet collapses to a one-line bar with RUN/SWEEP while picking, and expands for the table.
  - Segment wiring (N4).

### P2-M2.
- Heart world: the "beats / min" label is clipped ("beats / mi"), and the readout strip overlaps ("beats 24.00blood lost ml") (`w-heart_pacemaker-844x390.png`).
- Lock reasons are truncated on map cards: "Pass Autonomic he", "Pass Sinoatrial pac" (`ch-heart-844x390.png`).

---

## 5. Researcher (`python/`, PyNEST parity, docs)

### P1-R1. 0.2 is undocumented where a researcher looks
- `CHANGELOG.md` has only 0.1.0.
- `README.md` still says Godot 4.5+ (the build is 4.7) and lists 3 tasks. Its "Custom cells" section is four bullets, with no list of the 11 tasks, chapters, body modules or modulators.
- `python/README.md` documents none of `spike.bursts`, `spike.hrv`, `spike.prc`, the 4-node census in `motifs.py`, `Network.set_modulator`, or the NESTML registration.
- `circuit.py:312-322` registers the NESTML cells "if the PyNEST loader library is built; **skip silently otherwise**". A researcher who loads `heart_pacemaker.json` gets an unknown-model error with no hint why. That repeats the 0.1 silent-skip problem.
- **Fix:**
  - Write the 0.2.0 CHANGELOG entry, with the known gaps (P0-1, G2) stated.
  - README: task and chapter table, body modules, NESTML build step.
  - python README sections for the new modules.
  - `_register_nestml` warns (or raises, under `SPIKE_REQUIRE_NEST`) when the circuit uses a NESTML model that is not registered.

### P1-R2. Still no way to score a circuit outside Godot
- `spike-run` has no `--task` (usage: `spike-run.cpp:18`, 0.1 P2-R2), and Python has no tasks or body.
- The only scorer is the tests' "tuning aid" mode (`tasks_circuits_test run <task> <circuit> [seeds] [pre>post*k]`). It works, and it is what this review used, but it is undocumented outside the test source.
- The lesion tables in `docs/circuits` cannot be reproduced from Python, and a parameter sweep means writing C++.
- **Fix:** promote the tuning aid to `spike-run --task <id> [--seeds 1,2,3] [--lesion pre>post*k]`, printing the score JSON (`engine/src/spike_run.cpp`), and document it in the README. A pybind11 `spike._tasks` module can follow.

### P2-R3.
- The parity fixtures (`python/tests/test_parity.py` CASES) cover gap junctions, STP, dopamine STDP and the single-cell NESTML runs, but none of the 0.2 reference circuits.
- Add `heart_pacemaker.json` (NESTML in a network, modulation table, explicit edges) and `breathing.json` as free-run parity cases. They have no modulator outputs, which Python does not simulate (documented).

---

## 6. Code reviewer (engine, NESTML, registry, body, gdext, smoke, perf)

**What is good.**
- The task registry is clean: self-registering through `SPIKE_REGISTER_TASK`, with a linker anchor so static-library objects are not dropped, and sorted by `order`.
- Body modules are small, deterministic, documented and unit-tested (6 `test_body_*`).
- Modulation is applied between steps only, with change detection (`engine.cpp:540-575`).
- Release pools decay exactly from each spike's own time.
- The NESTML module is statically linked, with a real PyNEST spike-parity test.
- `step()` is chunked into 100 ms runs (0.1 P2-C3 fixed).

### P1-C1. Smoke never builds a v0.2 circuit from the skeleton, so P0-1 and N1 slipped through
- `_lab_checks` opens every task **with its reference** and adds one iaf neuron (`main.gd:241-258`).
- Body views are asserted only on the synthetic `MockTask` (`main.gd:306-320`), so a real task's view is never checked.
- Nothing compares the skeleton's channels with what the task reads.
- **Fix:** in `_v02_checks`:
  - for every task, build `_skeleton`, assert that each channel's kind equals the reference circuit's kind (dc/poisson/rate/modulator);
  - build a minimal circuit with the editor API, run 2 s, and assert that `modulator()` moves for tasks whose `modulators()` is not empty;
  - open the real body view of each task for 1 s and assert there are no "Waiting for ..." placeholders.

### P1-C2. The TEST x5 seeds only the world, not the circuit, and is free on deterministic tasks
- `_test_step` calls `task.reset(TEST_SEEDS[i])` but loads the circuit with its own seed (`lab.gd:520-524`), so NEST noise and initial `V_m` are identical across the five runs.
- `heart_pacemaker` and `heart_autonomic` worlds are documented as "deterministic, no randomness", so TEST x5 is five identical runs and star 2 is automatic.
- `tasks_circuits_test` reseeds both (`circuit["seed"] = 1000 * seed + ...`), so the tests and the game disagree: posture is 5/5 in the lab and 3/5 with reseeding.
- **Fix:** set `circuit.seed = 1000 * TEST_SEEDS[i] + circuit.seed` in `_test_step`, as the tests do.

### P2-C3.
- **Modulator plumbing hedges.**
  - `Telemetry.modulators_available` and `_push_mod` probe for `SpikeSim.set_modulator` (absent) and fall back to `ClassDB.instantiate("SpikeTask").call(...)`, creating a new `SpikeTask` on every call, every frame, from `modulator_level` (`telemetry.gd:186-200`). The comment says "once the modulator engine work lands"; it has landed.
  - Bind `set_modulator` and `modulator` on `SpikeSim` (`gdext/src/spike_sim.cpp`) and delete the fallbacks.
- **Re-prepare per slice.** Every Poisson sensor that changes rate triggers `nest::cleanup()` and `nest::prepare()` for that 1 ms slice (`engine.cpp:783-796`). The new tasks drive 1-8 Poisson sensors that change every slice (baroreceptor, pupil retina, peristalsis `dist*`, heart_autonomic errors), and the DA volume-transmitter rate does the same (`engine.cpp:566-574`). It is fine today (below), but it is the dominant cost; consider `inhomogeneous_poisson_generator` fed ahead, or a `dc`-to-parrot design.
- **Dead compatibility code.** `catalog.gd` keeps three generations of thumbs formats and a built-in card table (`catalog.gd:12-30,188-214`), now that the engine always supplies `catalog_json`. `Campaign.info` probes `has_method` for each v0.2 field (`campaign.gd:70-82`). Remove them.
- **Catalogue units.** Several cards have an empty `weight_unit` (see N2).
- **Two task orders.** The engine registry (`order`) and `Campaign.PLANNED` positions both define the order; a mismatch only shows on the map. Derive the order from the engine and keep only the positions in GDScript.

### Performance
No task is too slow for 1x. The C++ headless wall time per episode (table in S2) gives these headroom factors:

| Task | Faster than real time | Note |
|---|---|---|
| reach | 2.5x | the tightest; the lab shows "2.3x of 5x" (`w-reach`) |
| stretch_reflex | 4x | |
| renshaw | 4.7x | |
| posture | 5x | |
| peristalsis | 5x | |
| breathing | 12x | |
| heart_autonomic | 12x | |
| heart_pacemaker | 13x | |
| fight_or_flight | 13x | |
| pupil | 23x | |
| baroreflex | 28x | |

- Under xvfb with software GL the lab reached 3-8x when asked for 5-20x.
- A 5-seed TEST of breathing is about 63 s of pure simulation, before the per-frame budget.
- Users will hit the speed ceiling on the spinal tasks above 2-5x. Profile the reach circuit (56 neurons, aeif and dc inputs; likely the 12 explicit-edge projections and the sub-stepped limb).

---

## Release process (lead)

- **P1:** `CHANGELOG.md` has no 0.2 entry (see R1).
- **P1:** `REQ-0.2.md` has no status table. Several requirement items are not delivered or are delivered differently:
  - editor synapses;
  - energy and faithful stars;
  - analysis in the game;
  - figure validation;
  - the spindle card (`Catalog.UNLOCK` lists `spindle_afferent`, but the catalogue has no such card; the spindle lives in `spinal_limb.h`, which is fine if documented);
  - "muscle from motor-unit spike trains".
- State each one: done, changed (and why), or moved to 0.3.

---

## Verdict

**Not ready to ship.**

The engine work is strong:
- NESTML cells statically linked, with real PyNEST parity;
- a clean task registry;
- documented, tested body physics;
- eleven reference circuits that pass, mostly robustly;
- candid docs.

What blocks the release is the game:
- three of the eleven tasks cannot be built with the editor, because there are no modulator outputs and no modulation table, and the campaign cannot get past the Heart chapter without "editing" a reference circuit (P0-1, G1);
- around that: wrong sensor kinds and default weights in the lab skeleton (N1, N2);
- a pacemaker score that rewards clones (S1);
- references that fail on 1-3 of 12 seeds, including the lab's own TEST for stretch_reflex (S2, S3);
- several physiology claims that describe model artefacts as biology (S4, S5);
- chapter art that does not read as anatomy, the valentine heart above all (G4).

Fix P0-1 and the P1 list. Then re-run the novice harness until every task passes from an empty bench using only editor controls, re-run the 12-seed sweep until every reference passes at least 11/12 (or the docs say otherwise), and 0.2 is a release.

### P0 / P1 grouped by the files a fix touches (for parallel agents)

| Agent | Files it owns | Items |
|---|---|---|
| **A. Lab editor** | `godot/ui/screens/lab.gd`, `godot/ui/editor_view.gd`, `godot/game/circuits.gd`, `godot/ui/parts_bin.gd` | P0-1 output kind switch (rate/modulator, gain, tau) and modulation rows in the inspector; N1 skeleton kinds from the task or reference, dc/poisson switch; N2 use the catalogue weights (drop `WEIGHT`); N3 "For this task" parts first, default part; N4 segment wiring (n → channel k); G1 reference fingerprint, record mean of TEST; G3 default speed by duration; C2 reseed the circuit in TEST; M1 phone picker list and collapsible lesion sheet; G5 the lesion UI part (prediction step, live view during run) |
| **B. Engine core and catalogue** | `engine/src/engine.cpp`, `engine/include/spike/engine.h`, `engine/include/spike/task.h` (add-only), `engine/src/catalog/catalog.cpp`, `engine/include/spike/catalog.h`, `catalog/thumbs.json` (regenerated), `docs/CIRCUIT-FORMAT.md`, `engine/tests/test_engine.cpp`, `engine/tests/test_catalog.cpp` | P0-1 per-model default modulation table (and/or `Task::channel_kinds()` add-only); S1 model-declared initial-V_m range; N2 measured default weights and units; S6 f-I over ≥ 10 cycles |
| **C. Python mirror** | `python/spike/circuit.py`, `python/tests/*`, `python/README.md` | mirror B's default modulation and V_m range (keep parity); R1 NESTML warning and README for bursts/hrv/prc/census4; R3 parity cases for heart_pacemaker and breathing |
| **D. Heart tasks** | `engine/src/tasks/heart_pacemaker.cpp`, `heart_autonomic.cpp`, `baroreflex.cpp`, `engine/tests/tasks_heart_test.cpp`, `docs/circuits/heart_*.md`, `docs/circuits/baroreflex.md`, `models/nestml/sa_node_pacemaker.nestml`, `docs/models/sa_node_pacemaker.md`, `engine/tests/test_nestml_sa_node.cpp`, `circuits/heart_*.json`, `circuits/baroreflex.json`, `circuits/fight_or_flight.json` | S1 per-cell heterogeneity and the uncoupled-clones lesion test; S4 SA intrinsic rate re-fit or relabel, delete the "tone held at rest" claim, label the error sensors; S6 figure check for SA (or relabel); 10-seed tables |
| **E. Spinal tasks** | `engine/src/tasks/spinal_limb.h`, `stretch_reflex.cpp`, `renshaw.cpp`, `posture.cpp`, `reach.cpp`, `engine/tests/tasks_spinal_test.cpp`, `circuits/tools/gen_spinal.py`, `circuits/{stretch_reflex,renshaw,posture,reach}.json`, `docs/circuits/{stretch_reflex,renshaw,posture,reach}.md`, `docs/body/muscle.md` | S2 margins and 10-seed tests; S3 tap detector, wiring-aware verdict, reach worst-reach verdict; S4 hyper-reflexia relabel or `rmax`, Renshaw control on the player's circuit; S5 spike-mode muscle or pool-size scaling; G4 `target_rad()` override for stretch/renshaw; N5 zero value when unwired, verdict grammar |
| **F. Autonomic and breathing tasks** | `engine/src/tasks/pupil.cpp`, `peristalsis.cpp`, `breathing.cpp`, `fight_or_flight.cpp`, `engine/tests/tasks_autonomic_test.cpp`, `docs/circuits/{pupil,peristalsis,breathing,fight_or_flight}.md`, `engine/tests/test_nestml_butera.cpp`, `docs/models/butera_burster.md` | S3 pupil latency baseline and persistence; N4 optional `icc*`/`no*` channels; S2 10-seed tests; S6 Butera figure check |
| **G. Art and views** | `godot/ui/screens/tasks.gd`, `godot/ui/body_views.gd`, `godot/ui/timeline_view.gd`, `godot/ui/score_card.gd`, `godot/ui/trace_view.gd`, `godot/ui/raster_view.gd` | G4 anatomical chapter maps (heart 4-chamber with SA node, lungs with brainstem inset, spine and cord, autonomic body with the two divisions), node pins instead of covering cards, limb, gut and heart body views; S8 ECG from real beat times; G3 scope and raster window by timescale; G6 readout labels and units, timeline label overlap; M2 phone clipping |
| **H. Lesion logic** | `godot/game/lesion.gd` (with the lesion data per task under `docs/circuits` or a task method from D/E/F) | S7 multi-seed lesion rows with intact variability; G5 expected-biology reveal data |
| **I. Smoke and process** | `godot/main.gd`, `scripts/smoke.sh`, `scripts/shots.sh`, `engine/src/spike_run.cpp`, `README.md`, `CHANGELOG.md`, `docs/REQ-0.2.md` | C1 skeleton, modulator and real-view smoke checks; R2 `spike-run --task/--seeds/--lesion`; R1 CHANGELOG 0.2.0 and README; lead REQ status table (G2 decision: land or defer) |
| **J. Campaign and stars** | `godot/game/campaign.gd`, `godot/game/progress.gd`, `gdext/src/spike_sim.{h,cpp}`, `gdext/src/bridge.cpp` (add-only), `godot/ui/screens/analysis.gd` | G2 energy and faithful stars, HRV/PRC/bursts/motif4 bindings and analysis tabs (or the REQ deferral); C3 `set_modulator`/`modulator` on `SpikeSim` (then `telemetry.gd` cleanup by A or J, agree first) |

**Coordination notes:**
- **A and B** meet at P0-1. Agree first whether the engine applies default modulation rows (then the lab needs only the output kind switch) or the lab writes the rows. The engine route also fixes the sandbox dials for player-built circuits.
- **B and C** must land together, or parity breaks: the modulation defaults and the V_m range.
- **D, E and F** each regenerate their own `circuits/*.json`. Nobody else touches those files.
- **A and H** both touch the lesion lab. H owns `lesion.gd` and the Runner API (multi-seed); A owns the lesion UI in `lab.gd`. Agree on `Runner.begin(task, circuit, sets, seeds)` first.
- **G and E** share the `target_rad()` fix: E adds the override, G draws the ghost.
- **J and A** share `telemetry.gd`. Whoever lands the `SpikeSim` modulator binding also deletes the fallbacks in `telemetry.gd:186-200`; nobody else edits that block.

## Lead decisions (fix round)
- **P0-1 route:** the engine applies per-model default modulation. Any population whose model has `ach`/`ne` parameters (the NESTML cells) follows the circuit-wide ACh/NE levels automatically unless the circuit's `modulation` table overrides it. The lab adds an output-kind switch (rate / modulator with gain and tau) so players can make vagal and sympathetic release pools. The sandbox dials then work on any circuit.
- **G2:** the energy star and the 4-node, HRV, PRC and burst analysis panels land in 0.2. The "biologically faithful" motif star and plastic synapses in the editor are deferred to 0.3, and REQ-0.2 records this.
- **Stars:** TEST x5 reseeds the circuit and records the mean, not the max. Any edit to a reference circuit marks it as "reference-derived", and a reference-derived circuit earns no stars.
