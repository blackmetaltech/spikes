# v0.3 persona critique: Learning curve

Judge's review of release 0.3 at commit `2a457af` (tree of 2026-11-17, plus the untracked `catalog/histograms/` and `tools/histograms/` that the lead is still filling). No code was changed for this review.

**How it was checked**
- Build as handed over in `build/` (ctest 34/34, smoke all good, 41 levels in `spike-run --list-tasks`).
- **Level facts.** A headless GDScript (`godot --headless --path godot -s dump.gd`) dumped `Campaign.info`, `Campaign.notebook`, `hints()`, `channel_kinds()`, `Campaign.nodes`, `unlocked` and `lock_reason` for every level from a fresh save.
- **Novice harness.** A Python bench builder that writes circuits exactly as the lab does (`Circuits.add_card`, `add_projection` with the catalogue weight and 2 ms delay, dc sensors at the first-connect 300 pA, Poisson sensors at `input_weight`, outputs with the task's kinds) and scores them with `build/engine/spike-run --task <id> --circuit <file> --seeds ...` (the instant check's own command line). The novice reads only the brief and the verdict, starts from the lab's defaults, and changes one thing per run the way the verdict says. Lab defaults were read from the running game (`Circuits.weight_of`: iaf 80 pA, aeif 8 nS, SA 0.2 nS, icc 200; first-connect dc 300, Poisson onto iaf 280). Run counts are for seed 1 (the CHECK seed); robustness on seeds 1-5.
- **Timing.** Every reference run headless and sequentially with `spike-run --task <id> --seed 1` (this is exactly what CHECK runs). In-game wall time per run measured with a headless driver that opens the real lab (`main.tscn`, `SP_SCREEN=lab SP_TASK=<id> SP_REF=1`, default speed, `fps_cap=60`, the default 9 ms stepping budget, no input) and reports when the episode ends; a software-GL (xvfb) run is reported separately because llvmpipe frame cost dominates there.
- **Screenshots.** 54 shots (27 screens x 1920x1080 and 844x390) with the `scripts/shots.sh` mechanism and the v0.3 `SP_*` hooks (`SP_HINT`, `SP_FAILS`, `SP_VERDICT`, `SP_HISTO`, `SP_CELEB`, `SP_CHECK`, `SP_SKIP`, `SP_DXPICK`/`SP_DXSUBMIT`, `SP_VARIANT`, `SP_MODE`) from a fresh user-data dir: `docs/screenshots/critique-0.3/`. I looked at every one; the ones quoted are named inline.

Severity: **P0** blocks the release. **P1** should be fixed for 0.3. **P2** can wait.

---

## Gate-by-gate (DESIGN-0.3 section 10, plus the section 5-9 commitments the gates depend on)

| Gate / commitment | Result | Evidence |
|---|---|---|
| **Micro-puzzles** solvable by the novice harness without recipe or hints in <= 8 runs | **FAIL as shipped** (PASS only with a part the lab does not select) | With `iaf_psc_alpha` cells all 19 pass in 1-5 runs (table in section 1). The lab opens 14 of the 19 micros on a different part (`_default_part`, N1): `m_excitation` is then not passable by the knobs the verdict names (echo <= 52 % at the 160 nS slider maximum), and `m_threshold`, `m_wave`, `m_feedforward`, `m_neuromodulation`, `m_recruitment` give false verdicts. Also, micro-B briefs carry the recipe numbers, so "without recipe" is not what was measured. |
| **Spine tasks** solvable in Standard with tier-1/2 hints only | **FAIL** | `renshaw`: the brief's recipe and its own tier-3 hint score 0/5 (N2). `reach`: the brief's starting point 0/6 in Standard and Guided; the spread control the brief tells you to use does not exist (N3). `posture`: the brief's recipe 0/5, and its verdict points the wrong way (N4). |
| **Long runs**: no level with a median wall time per run above 45 s; micros <= 10 s | **FAIL** (micros PASS) | Micros run in 3-5 s. Measured in the real lab at default speed: breathing 116 s (80 s before the idle cap), boss_spinal_sprint 63 s, boss_autonomic_exam 53 s, baroreflex 50 s, so 6 levels with the dx cases that reuse them (G2). The default speed asks for <= 25 s; the lab delivers 1.3-2.8x where it asks for 5-10x. |
| **Unfinished levels**: no level where a novice is stuck with no next action | **FAIL** | `renshaw` (verdict, tier 2 and tier 3 all describe the circuit already built), `reach` (no editor control reaches a pass), `m_excitation` on the default part. Because these are spine levels, Heart, Breathing and Autonomic never open (G1). |
| **Physiology**: every concept card has a correct, cited fact; physiologist signs off | **FAIL** | Concept glossary entries carry no citation; five notebook entries misstate or mis-cite (T3); one diagnosis distractor is physiologically wrong (S3). Not signed off. |
| Local telemetry: time per level, run count, hint tier, quits, edits before first pass | **FAIL** (records, but wrongly) | Every visit is logged as two visits and one phantom quit; runs are only counted on RESET (C3). |
| Modes (Guided / Standard / Researcher) as in section 5 | **FAIL** | The recipe is in every brief in every mode although the in-game text says "behind a button" / "No recipe"; Researcher does not require 5 seeds (V3). |
| Hint ladder, verdict highlight | PASS (desktop) / FAIL (phone) | Works and reads well on desktop (`lab-m_threshold-hint3-1920x1080.png`); on a phone tier 3 is cut off (M1). |
| Challenge variants | **FAIL** | 14 of 28 variants are `half_neurons`, which any passing circuit completes (V2). |
| Post-solve histograms | PARTIAL (in progress) | 4 of 35 levels have data; distributions are perturbations of the reference, so the brief's own recipe beats 100 % (V1). |
| Diagnosis cases | **FAIL** as puzzles | In all six cases the answer is the only candidate whose wire is absent from the drawn circuit, and the lesion-lab advice is inverted (S1, S2). |
| Instant check under 2 s | **FAIL** on 13 levels | Table in G2. |
| Audio (spikes, heart, breath) | PARTIAL | Works and is rate-limited; silent above 5x, so the breathing task (10x default) never breathes and baroreflex never beats (C4). |
| Chapter unlock: spine-based, two chapters always open | PARTIAL | Spine rule implemented; "two open" holds only at the start (G5). |
| References pass | PASS | All 41 pass seed 1 (dx with the answer submitted); both bosses pass seeds 1-5 in Standard and Researcher. |

---

## 1. Novice (fresh save, first chapter path, only what the UI shows)

**Short answer.** The micro-puzzles are well made: short, one idea each, with verdicts that usually name the knob and the size of the change. A novice with the right cell passes every one in 1-5 runs. But the lab hands the novice the wrong cell on 14 of the 19 micros, the first chapter's reflex task can be passed with one neuron, and the Spinal spine has two levels that cannot be passed from the brief, so the campaign ends after 16 levels (Invertebrates, the first four Spinal levels and `dx_clonus`).

### P0-N1. The lab opens almost every micro-puzzle on the wrong part, and the verdicts then teach false numbers
- **Evidence:**
  - `lab.gd:_default_part` takes the first card of `_task_cards()` that is not `iaf_psc_alpha`. For a reference that uses only `iaf_psc_alpha` that is the first card of the chapter: `aeif_cond_alpha` in Invertebrates, `iaf_cond_exp` in Spinal, `pp_psc_delta` in Heart, `icc_slow_wave` (an oscillator) in Autonomic, and the SA pacemaker for `m_neuromodulation`'s vagal cell.
  - Screenshots: `lab-m_threshold-empty-1920x1080.png` shows "PARTS: aeif_cond_alpha" on the very first level; `lab-m_recruitment-empty-1920x1080.png` shows "PARTS: iaf_cond_exp". The same happens on `reflex` (`lab-reflex-hint1-standard-0fails-1920x1080.png`) whose recipe is in pA for iaf cells.
  - Every micro brief, verdict and hint is written for `iaf_psc_alpha` (rheobase 375 pA, 80 pA synapses), and so is the novice path in `tasks_micro_a_test.cpp:429-501` ("the lab's defaults (iaf_psc_alpha synapse 80 pA...)"). The gate was measured against a lab that does not exist.
  - Harness with the part the lab actually gives:
    - `m_threshold`: 300, 400, 500 pA all "The cell never fired... rheobase (about 375 pA for this cell)... Raise the weight"; 600 "barely above threshold"; 700 passes. Five runs, and the verdict's 375 pA is false for that cell (aeif rheobase is about 620 pA).
    - `m_excitation`: the source cell never fires at 300 (600 pA < 620); with the tap raised the echo answers at most 52 % even at the 160 nS slider maximum. **Not passable with the knobs the verdict names**; nothing tells the player to change the part.
    - `m_wave` on `icc_slow_wave`: "Segment 0 is silent at rest... Wire 'tone' to all three segments" when tone is wired. `m_neuromodulation` with an SA cell as the vagal cell: "Make a cell fire when the vagus comes on and wire it to 'vagal'" when it is. `m_feedforward` on `pp_psc_delta`, `m_recruitment` on `iaf_cond_exp` with the brief's own bias logic: fail.
    - `reflex` recipe built on the default aeif part (link 140, now nS): `withdraw` never fires, 0/3.
- **Fix:** `godot/ui/screens/lab.gd:_default_part`: start on the reference's first model, `iaf_psc_alpha` included; only fall back to a chapter card when the reference has none. Make the micro tests build with what `_default_part` returns (or assert it is `iaf_psc_alpha` for the iaf micros) so the gate measures the lab. Verdicts that quote a rheobase or a weight should read the cell model the output is wired to (or say "for the default integrate-and-fire cell").

### P0-N2. `renshaw` (spine) cannot be passed from its brief, its verdict or its tier-3 hint
- **Evidence:**
  - The brief's recipe built literally (stretch-reflex circuit with drive 200; `noise` (Poisson, weight 100) > DESC (2 cells) > both pools at 40; Renshaw groups RF, RE as stated): **0/5**, values 89-100 with `passed=false`. Verdict: "Your motor pools hardly answer the common input (a volley adds only 121 spikes/s; the reference cord shows about 1100)... Send the noise through shared fibres to both pools." That is what was built. Tier 2: "route it through shared fibres". Tier 3: "noise (weight 100) > DESC (2 cells) > both pools at 40 pA" (the same circuit).
  - The same circuit without Renshaw cells also fails the volley gate, so the 0.3 change ("Renshaw control measured on the player's own circuit", `232965e`) is what broke the recipe: the reference uses `parrot_neuron` DESC cells and spread `edges`, which the brief does not mention. Variants (noise weight 280, DESC > MN 160, parrot DESC) either still fail the volley gate or smother the reflex. Guided: also 0/3.
  - The score ring shows **100** while the verdict says not passed.
- **Fix:** re-derive the brief recipe and the tier-2/3 hints against the 0.3 scoring (`engine/src/tasks/renshaw.cpp`, `docs/circuits/renshaw.md`), probably DESC as `parrot_neuron` with the weight the volley test needs; cap the value below the pass bar when a gate part fails; add the brief's recipe as a novice case to `engine/tests/tasks_spinal_test.cpp` (there is none for the 14 tasks in 0.3).

### P0-N3. `reach` (spine) is still not passable from an empty bench, and its brief sends the player to a control that does not exist
- **Evidence:**
  - Brief: "Give the links onto the pools some weight spread (the lab's spread control; ...a spread (CV) near 0.3 to 0.5)". `grep -ri spread godot/` finds no such control; `weight_cv` exists only in the engine (`docs/CIRCUIT-FORMAT.md` "Weight spread"), and only on `edges` projections: `spike-run` rejects `weight_cv` on the `all_to_all` projections the lab makes ("weight spread needs an 'edges' projection").
  - The brief's starting point built literally: Standard **0/6** (31.5, 0.3, 13.2, 11.7, 4.4, 0.0), Guided **0/6** (same values; verdicts "2 of 3 reaches never settle", "creeps 0.10 rad before go").
  - `reach` and `boss_spinal_sprint` are on the Spinal spine, which gates Heart (G1).
- **Fix:** either (a) add the spread control to the link inspector and have the lab write `edges` (or make the engine accept `weight_cv` on rule projections, drawing per-edge weights after the connect) — `godot/ui/screens/lab.gd:_inspect_proj`, `godot/game/circuits.gd`, `engine/src/circuit.cpp`, `python/spike/circuit.py` for parity; or (b) take `reach` and the boss off the spine until a novice bench passes. Re-run a novice reach case in `tasks_spinal_test.cpp`.

### P1-N4. `posture` (spine): the brief's recipe fails 0/5 and the verdict points the wrong way
- **Evidence:** recipe built literally: 55.3, 56.2, 53.1, 49.7, 58.9, all fail; "The limb sags 0.17 rad... more flexor tone is needed". More flexor tone makes it worse (TONIC > MNF 240: sag 0.27; 320: 0.28; 480: 0.30); less is better (120: 2/3 pass). The verdict uses the magnitude of the offset and ignores its sign (`mean_err_rad -0.173`). Guided passes 3/3.
- **Fix:** `engine/src/tasks/posture.cpp` verdict by the sign of the offset; retune the brief's TONIC > MNF number (about 120) and test it.

### P1-N5. The reflex task passes with one neuron; the convergence lesson is not enforced
- **Evidence:** `touch > N1 > withdraw` (default 280, one iaf cell): Standard 5/8 seeds pass including seed 1 ("Clean reflex: all 6 pokes answered in 57 ms on average, 1 false withdrawals"); Guided 3/3. The brief says "one cell listening to one afferent cannot do it". Researcher 0/5.
- **Fix:** `engine/src/tasks/reflex.cpp`: no false withdrawals in Standard (or a latency bar a lone cell misses), and no "Clean" verdict with a false withdrawal.

### Novice runs to pass, micro-puzzles (iaf part; the novice changes what the verdict says)

| level | runs | what happened |
|---|---|---|
| m_threshold | 2 | 300 "never fired, rheobase 375" > 400 pass (5/5) |
| m_excitation | 5 | 80, 400, 1000 "never fired: raise a lot" (same text three times) > 1600 "too late by 3.4 ms" > delay 5 pass |
| m_convergence | 3 | 300, 200 "fired for b + c" > 150 pass (5/5) |
| m_inhibition | 2 | -80 "needs about 12 times" > -800 pass |
| m_subtraction | 2 | -80 "about 14 times" > -800 pass; -1600 correctly "too strong" |
| m_adaptation | 2-4 | raising b (verdict: "tau_w or b") never passes even at 200; tau_w 300 passes. The "Adapting" preset fails this puzzle (ratio 1.4) |
| m_half_centre | 1-2 | Adapting preset passes; the NEST-default aeif card is silent and the verdict says "needs the default 300 pA weight or more" when it is 300 |
| m_recruitment | 2 | biases pass |
| m_reciprocal | 2 | -80 > -300 pass |
| m_recurrent | 4 | brief recipe (2 Renshaw, 600 both ways) on one MN fails with "nothing holds the motoneurons back" although the loop halves the kick; 1000 fails the same way; 1600 passes |
| m_negative_feedback | 1-2 | recipe passes 5/5 |
| m_timed_bursts | 3 | brief's "about 300 pA" with 1-cell groups fails, the verdict repeats "about 300 pA"; 1000 passes |
| m_pacemaking | 1 | SA card is the default part here |
| m_coupling | 2 | 0.2 > 0.8 nS |
| m_feedforward | 1 | recipe; without the cue it fails, so the lesson holds |
| m_neuromodulation | 1 | |
| m_rhythm | 2 | 300 > 5 as the verdict says |
| m_wave | 2 | |
| m_divergent | 4-5 | brief numbers assume a 2-cell command group it does not state; the verdicts repeat "about 600 pA" / "about 280 pA" at those values |

First chapter path after the micros: `reflex`, `worm`, `lamprey` pass first time from the brief's recipe (5/5), because the recipe is in the brief. A lamprey built as the half-centre micro taught (1 cell a side, drive 300) is silent: "Both motor channels are silent... check that drive reaches your neurons" (P2: say the drive weight).

### P2-N6.
- Verdict numbers depend on a group size the brief never states (`m_recurrent`, `m_timed_bursts`, `m_divergent`): state the size, or give the number per cell.
- Wrong verdicts: `m_recurrent` "nothing holds the motoneurons back" whenever the first kick exceeds 27 Hz, even with a working loop (`m_recurrent.cpp`); `m_half_centre` "needs the default 300 pA weight or more" (`m_half_centre.cpp`).
- An empty bench scores 57 (`m_threshold`), 37 (`m_recruitment`); "never fired" scores 56.7 and 42.5 (`lab-m_threshold-empty-1920x1080.png`). Force 0 until something fires.
- The map card cuts the brief at 420 characters; on `m_threshold` that drops "the weight is your knob" (`map-fresh-1920x1080.png`).

---

## 2. Learner / teacher (one concept per level, order, notebook)

**What is good.** The concept graph is real: 19 micros, each on one idea, ordered so that every concept is introduced before a later level uses it (checked mechanically over all 41 levels, with the alias table). The notebook entries are short, mostly true and mostly cited to the primary paper (Lapicque, Katz-Miledi, Hartline, Henneman, Renshaw, Jalife, Loewi, Krogh-Lindhard, Smith 1991, Bayliss-Starling).

### P1-T1. Big tasks re-announce concepts the micros already taught as their "new idea"
- **Evidence:** `reflex` lists `concepts_new` = threshold, excitation, convergence (all three taught by the preceding micros); `worm` inhibition, subtraction; `lamprey` adaptation, half_centre; `stretch_reflex` recruitment, reciprocal_inhibition; `heart_autonomic` neuromodulation, feedforward_feedback; and so on. The map card shows them under "NEW IDEA". `pupil` introduces none.
- **Fix:** tasks list these under `concepts_used`; `concepts_new` only for what is genuinely new (the integration). One id per concept (`feedforward` vs `feedforward_feedback`, `travelling_waves` vs `travelling_wave`/`traveling_waves` are held together by `Campaign.CONCEPT_ALIAS`).

### P1-T2. Diagnosis branches hang off the wrong levels
- **Evidence:** `dx_achalasia` (a gut case using travelling waves) hangs off `pupil` and unlocks before `m_wave`/`peristalsis` teach the idea (`map-autonomic-unlocked-1920x1080.png`: the achalasia pin sits on the face). `dx_vagal` hangs off `m_neuromodulation`, before `heart_autonomic`, whose circuit it is. Cause: `Campaign._parent_of` guesses by concept overlap; the dx tasks declare no parent.
- **Fix:** give `Diagnosis` (or the dx spec) the underlying task as parent (`DxSpec.inner` already is it) and have `campaign.gd:_parent_of` use it.

### P1-T3. Notebook facts that are wrong or mis-cited (the physiology gate)
- `reflex` "Convergence beats noise": Castellucci & Kandel 1974 (PNAS 71:5004) is the quantal analysis of habituation in the gill-withdrawal reflex; it does not show interneurons summing weak afferents to beat noise. Use a convergence source (e.g. the Mendell-Henneman entry already used by `m_convergence`) or rewrite the claim.
- `breathing` "Bursting makes the rhythm" cites Feldman & Del Negro 2006, a review that argues against the pacemaker hypothesis (rhythm survives block of the persistent sodium current). State it as one hypothesis (group pacemaker vs pacemaker) or change the title.
- `worm` cites "Suzuki H et al. (2008) Neuron 57:1"; the same paper is Nature 454:114 in `m_subtraction`.
- `m_negative_feedback`: Cannon coined "homeostasis" in 1926 (the 1929 review develops it), not "in the 1930s".
- `boss_autonomic_exam` note title "Central command turns the baroreflex up, not off" contradicts both its text (resetting) and the brief ("the central signals must also turn the reflex down (CVLM)").
- Concept glossary entries (`Campaign.CONCEPTS`, Notebook > Concepts) carry no citation at all; the gate asks for a cited fact on every concept card.
- **Fix:** the task files' `notebook()` / `DxSpec.note` (`engine/src/tasks/reflex.cpp`, `breathing.cpp`, `worm.cpp`, `m_negative_feedback.cpp`, `boss_autonomic_exam.cpp`) and a citation column for `godot/game/campaign.gd:CONCEPTS`.

### P2-T4.
- Speculative claims stated as fact: `m_recurrent` ("helps synchronise the pool", "as in clonus when it fails"); `posture` ("Gamma-quiet spindles are nearly silent": spindles keep a length-dependent resting discharge).
- `dx_apnoea` symptoms put the hypoventilation at rest while its own note says CCHS patients "breathe adequately when awake"; "Ondine's curse" is dated, use CCHS.
- `m_adaptation`: the parts bin's "Adapting" preset does not pass the adaptation puzzle (ratio 1.4); either rename the preset or make it pass.

---

## 3. Puzzle veteran (Zachtronics fan)

**What is good.** The grammar is right for the genre: open toolset, acceptance criteria, measured not prescribed; three axes per level; pin-a-target; variants as optional branches; CHECK in 0.1 s on a micro (`lab-m_threshold-check-1920x1080.png`). The histogram card reads well when it has data (`lab-m_threshold-histo-844x390.png`).

### P1-V1. Histograms: sparse, and seeded from perturbations of the reference, so they measure nothing
- **Evidence:** `catalog/histograms/` has 4 files (`m_threshold`, `m_wave`, `reflex`, `baroreflex`) of 35 levels; the rest show "No distribution for this task yet". The generator (`tools/histograms/gen_histograms.py`) perturbs the reference circuit, so `reflex` neurons run 22-48 with the median at the reference's own 23 (`lab-reflex-histo-1920x1080.png`: the reference sits on the median of all three axes): the brief's own 16-neuron recipe (and the 1-neuron pass of N5) beats 100 %. Quality axes are degenerate: `m_wave seg2_vs_tone` is 0 in all 61 samples; `baroreflex worst_settle_s` median 0. The third star is still par plus energy, not "beat the median on any axis" (section 6).
- **Fix:** seed from structurally different solutions (the novice harness benches, minimal circuits, the recipe), choose quality metrics with spread, label the distribution's source on the card, and finish all levels before release (`tools/histograms/gen_histograms.py`, `catalog/histograms/*`, the tasks' `metrics()`).

### P1-V2. Half of the challenge variants are free
- **Evidence:** 14 of 28 variants are `half_neurons` (one on each of the 14 tasks). The variant only halves `par_neurons` (`task_util.h:168 variant_par`); neither the task's score nor the lab checks the neuron count. `lab.gd:_record` records a variant pass on `passed` alone. `spike-run --task reflex --variant half_neurons` on the 23-neuron reference: passed.
- **Fix:** enforce it: the lab records a `half_neurons` pass only when `Circuits.neuron_count <= par`, or the engine's `score()` fails it (it knows the circuit size through the engine). Replace some with the design's own examples (lesioned body, "only Izhikevich cells", 2x noise).

### P1-V3. Researcher mode is Standard with the hints hidden
- **Evidence:** the recipe is in every 0.2 task's brief in all modes (13 of 14 briefs contain "A recipe that works"; micro-B briefs carry the numbers too), while the mode blurbs say "The recipe is behind a button" and "No recipe" (`progress.gd:21-23`, `settings-game-1920x1080.png`). Researcher does not require 5 seeds (`Progress.record` takes one passing run). `boss_spinal_sprint` scores identically in Standard and Researcher on seeds 1-5 (77.8, 77.4, 77.2, 78.1, 79.6).
- **Fix:** split each brief into brief + recipe (`Task::brief()` plus a `recipe()` or a marker the lab can cut at), show the recipe behind a button in Standard (costing the efficiency star) and never in Researcher; record a Researcher pass only from TEST x5; scale noise with the mode in every task (`micro_b.h`, `boss_*.cpp`).

### P2-V4.
- Surprise detection covers only "an unintended rhythm"; "a lesion that does something unexpected" is not there.
- Live f-I preview and the weight-slider latency preview (section 8) are not implemented.
- No epilogue sandbox or codex reveal per chapter; Invertebrates, Heart and Breathing have no boss (scope said two bosses, the interest curve says one per chapter).

---

## 4. Physiologist (bosses and diagnosis cases)

**What is good.** The bosses are real integrations and their references are robust (spinal sprint 77-80 on seeds 1-5, autonomic exam 100 on 1-5 with HR 69 rest / 76 standing / 113 exercise / 115 threat and MAP 89 standing, all plausible). The diagnosis vignettes are clinically literate (a smoker's Horner pointing at a preganglionic lesion, neurogenic orthostatic hypotension with an intact afferent arm, vagotomy at the intrinsic rate), and each distractor has an authored, quantitative reason why it does not fit.

### P1-S1. Every diagnosis answer is readable from the wiring diagram; no probing is needed
- **Evidence:** the patient circuit is the reference minus the answer's projections (`dx_common.h` header). For all six cases, the answer is the only candidate whose projections are absent; every distractor's wire is drawn (dx_clonus: MN>RC absent, 5 others present; dx_vagal NA>VG; dx_orthostatic RVLM>IML; dx_apnoea RTN>PBC+RTN>PHR; dx_horner IML>SCG; dx_achalasia DIN>IMN). The lesion lab cannot even select the answer (there is no axon to click), and `spike-run --lesion MN_F>RC_F*0` warns "matched no projection".
- **Fix:** keep the lesioned projection in the file with its weight scaled to 0 (or 0.05) so every candidate is drawn and cuttable (`circuits/tools/gen_boss_dx.py`, `circuits/dx_*.json`), and draw a zero-weight axon like any other.

### P1-S2. The lesion-lab advice in the brief, the panel and the tier-3 hint is inverted
- **Evidence:** brief: "a cut you add that makes no difference was not the missing one"; tier 3: "Cut the one you suspect in the lesion lab: if the symptom does not change, it was not the missing wire"; panel after a wrong answer: "Probe again: cut the candidate in the lesion lab and compare". In the patient, cutting the missing wire changes nothing by construction, and so does cutting anything downstream of a silent cell: on `dx_clonus` seed 1 the patient run, the run with RC>MN cut and the run with RC>IN cut are identical (tremor 0.96 %, pool 47.89 Hz, value 45.85). Followed literally, the tier-3 advice rejects the correct answer. The authored "why not" text (`DxSpec.explain`) is never shown in the panel (`lab-dx_clonus-wrong-1920x1080.png`).
- **Fix:** the reasoning that works is "which cells are silent and which fire" (the explain texts already argue that way): rewrite the brief and tier 3 to it (`dx_common.h`), show `explain[picked]` after a wrong submission (`godot/ui/diagnosis_panel.gd`; the verdict already carries it), and show the plain-language labels on the candidate buttons, not only "The pathways RC_F to MN_F...".

### P1-S3. `dx_horner`: the central (first-order) distractor is physiologically wrong
- **Evidence:** "HYP>IML+HYP>EW ... the pupil would still open fully in the dark (7 mm)". A lesion of the hypothalamospinal pathway (first-order neuron, e.g. lateral medullary infarct) is a classic cause of Horner syndrome; the case's own notebook says interrupting any of the three neurons gives it. In the model IML fires from its bias, so the hypothalamic input is not needed for tone.
- **Fix:** make the distractor a lesion that truly spares the dilator in the dark (e.g. HYP>EW only, which removes the fright response), or give IML its tone from HYP in the pupil model and accept two right answers (`engine/src/tasks/dx_horner.cpp`, `pupil.cpp`).

### P2-S4.
- `dx_clonus` presents a 10 Hz postural tremor; clonus is a 5-8 Hz stretch-evoked oscillation from stretch-reflex hyperexcitability. Rename the case (it is "the shaky arm" on screen) or change the symptoms.
- `dx_orthostatic` symptoms say "the brainstem cells that should answer are firing", which nearly states the answer; with 3 of 5 candidates ruled out by "pressure high, not low", the case is a coin toss.
- Diagnosis cards show "Pass on 5 random seeds (TEST x5)" stars that mean "first guess right" (`map-autonomic-unlocked-1920x1080.png`); brute force (at most 6 submits, REVEAL after one) is free.
- Smoke's "dx reference passes" submits the answer as the variant: a tautology. Test instead that each distractor applied to the healthy reference gives the numbers its `explain` text quotes.

---

## 5. Gamer (pacing, juice, progression, session length)

### P0-G1. The campaign dead-ends in Spinal: Heart, Breathing and Autonomic never open
- **Evidence:** `Campaign.chapter_open` opens chapter i when chapter i-1's spine is passed (first two open from the start). The Spinal spine contains `renshaw` (N2) and `reach` (N3), neither passable from the brief, and `boss_spinal_sprint`, which needs both. Fresh save dump: Heart, Breathing, Autonomic locked, `lock_reason` "Pass the Spinal cord spine first". Spine nodes open in order, so a novice stuck on `renshaw` also never sees `m_negative_feedback`, `posture`, `m_timed_bursts`, `reach` or the spinal boss. In all 25 of 41 levels stay locked (5 in Spinal, the 15 spine levels of Heart, Breathing and Autonomic including the autonomic boss, and 5 diagnosis cases), plus every later part card (SA node, Butera, ICC) and their variants.
- **Fix:** N2 and N3; until they land, gate chapter i on chapter i-2 (G5) so a stuck chapter does not end the game.

### P1-G2. Pacing: four long levels (six with their diagnosis twins) take 50-116 s per run, the instant check takes up to 19 s, and long runs slow down when you stop touching the mouse
- **Instant check** (`spike-run --task <id> --seed 1`, sequential, this machine): boss_spinal_sprint 18.8 s, breathing 13.0, boss_autonomic_exam 9.1, fight_or_flight 7.5, peristalsis 7.4, heart_autonomic 6.8, reach 5.5, baroreflex 5.0, renshaw 3.7, stretch_reflex 3.6, posture 2.6, heart_pacemaker 2.3, worm 2.2; pupil 1.6, lamprey 0.7, reflex 0.4; every micro <= 0.4 s. 13 levels miss "under 2 s"; the toast says "CHECKING..." with no progress.
- **Live run, wall time per episode** (real lab, default speed, 60 fps, 9 ms stepping budget, no input): table below. `lab.gd:_default_speed` picks 5x or 10x so a run should take about 25 s, but `Telemetry.advance` only steps 9 ms of each frame and does GDScript work per 1 ms slice, so the lab delivers 1.3-2.8x on the long levels. The spinal boss is also slow in the engine itself (2.1x real time headless).
- On the software renderer (xvfb, llvmpipe) the lab reaches 0.1x on the spinal boss: 10.8 s of model time in 150 s (`lab-boss_spinal` run). That is a CPU-rendering artefact, but it is what a laptop without a GPU driver sees.
- `main.gd:101` drops the frame cap to 30 fps after 60 s without input (`Settings.idle`), and the lab steps a fixed 9 ms per frame, so a player who watches a long run halves its speed.
- **Fix:** step in larger slices (or several slices per GDScript call: a `step_many` binding) and give the run a wall-clock share instead of a fixed 9 ms per frame; do not apply the idle frame cap while a run is playing; choose the default speed from the measured delivered speed, and use SKIP-to-event by default on the quiet stretches of breathing and baroreflex; show progress in the CHECK toast and shorten the boss checks (`godot/game/telemetry.gd`, `godot/ui/screens/lab.gd:_default_speed`, `main.gd:101`, `gdext/src/spike_sim.cpp`).

| level | episode | default speed | wall time per run | delivered speed | instant check |
|---|---|---|---|---|---|
| m_threshold (micros alike) | 4 s | 1x | 4.1 s | 1.0x | 0.4 s |
| reflex | 20 s | 1x | 20.2 s | 1.0x | 0.4 s |
| reach | 13.3 s | 1x | 15.3 s | 0.9x | 5.5 s |
| renshaw | 16.5 s | 1x | 18.1 s | 0.9x | 3.7 s |
| peristalsis | 40 s | 2x | 27.7 s | 1.4x | 7.4 s |
| worm | 120 s | 5x | 32.0 s | 3.7x | 2.2 s |
| heart_autonomic | 85 s | 5x | 35.3 s | 2.4x | 6.8 s |
| fight_or_flight | 100 s | 5x | 41.1 s | 2.4x | 7.5 s |
| **baroreflex** (and dx_orthostatic) | 140 s | 10x | **49.9 s** | 2.8x | 5.0 s |
| **boss_autonomic_exam** | 120 s | 5x | **52.8 s** | 2.3x | 9.1 s |
| **boss_spinal_sprint** | 40 s | 2x | **63.5 s** | 0.6x | 18.8 s |
| **breathing** (and dx_apnoea) | 150 s | 10x | **116.4 s** (about 80 s before the idle cap) | 1.3x (1.9x at 60 fps) | 13.0 s |

Headless Godot (no render cost), `fps_cap=60`, no input, the reference circuit, default speed. A real GPU adds render time on top, so these are lower bounds. The engine alone runs breathing at 11.5x and baroreflex at 28x; the lab delivers 1.9x and 2.8x, so the limit is the lab's per-frame budget and the GDScript work per 1 ms slice, not NEST.

### P1-G3. Juice works but is undercut in places
- The first-pass celebration (slow motion, camera push, trail, notebook card) works; the notebook card is drawn under the end card and its citation is clipped (`lab-m_threshold-celeb-1920x1080.png`).
- Body sounds stop above 5x (`telemetry.gd:292-294`); the default speed is 10x for `breathing` (150 s) and `baroreflex` (140 s), so the breathing task is silent and the baroreflex has no heartbeat. `twitch` and `gut` cues exist but nothing calls them.
- The score ring reads 100 on a failing `renshaw` run and on the running `m_threshold` episode before it ends (`lab-m_threshold-hint3-1920x1080.png`).
- **Fix:** `godot/ui/celebration.gd` (z-order), `godot/game/telemetry.gd` (play body sounds at any speed, rate-limited, or at least at the task's default), `godot/ui/body_views.gd` (call twitch/gut from the limb and gut views), `godot/ui/score_card.gd` (no 100 before `passed`).

### P2-G4. Session length estimate
- Micros: 1-5 runs of a 3-5 s episode plus editing: 2-5 min each, about 1-1.5 h for 19. Tasks with the recipe in the brief: 1-2 runs, 5-15 min each. Bosses: 30-60 min.
- Critical path (35 spine levels) with recipes visible: about 5-8 h, not the 12-15 h of section 9; with variants, diagnosis and optimisation perhaps 12-15 h. Natural stopping points every 20-40 min hold inside a chapter (micro, micro, task).
- As shipped, the session ends at `renshaw`, the 15th spine level (about 2-3 h in).

### P2-G5. "Two chapters always open" is only true at the start
- After Invertebrates is done only Spinal is in play (`campaign.gd:chapter_open`). Open chapter i when chapter i-2's spine is passed.

---

## 6. Phone 844x390

### P1-M1. The hint card cannot be read past tier 2
- **Evidence:** `lab-m_threshold-hint3-844x390.png`: tier 3 is below the screen edge; `HintPanel` has no scroll container and is anchored at the top of the overlay.
- **Fix:** wrap the tiers in a `ScrollContainer` sized to the overlay (`godot/ui/hint_panel.gd`, `lab.gd:_hint_render`).

### P2-M2.
- CHECK, SKIP, HINT, RESULTS and the mode stepper are only in MENU on a phone (`lab-m_threshold-empty-844x390.png`); the "Hints unlocked: the HINT button now gives a nudge" toast points at a button that is not on screen. Put HINT and CHECK in the phone top bar.
- Campaign map: the first micro's label is drawn over the reflex pin and the node sits on the map corner (`map-fresh-844x390.png`); micro pins have no label until selected.
- The diagnosis sheet works (`lab-dx_clonus-844x390.png`), but the circuit behind it is a sliver; a HIDE CASE / SHOW CIRCUIT toggle exists, good.

---

## 7. Code reviewer (0.3 additions)

### P1-C1. Instant check (`gdext/src/bridge.cpp:headless_start`)
- The runner is found relative to the current directory (`./spike-run`, `build/engine/spike-run`, `../build/engine/spike-run`, ...) then on `PATH`; the game never calls `SpikeTask.set_headless_runner`. CHECK therefore works only when Godot is started from the repo root or `godot/`; from a desktop launcher or an export it fails ("Check failed"), and a binary named `spike-run` earlier on `PATH` is executed. It also prefers `build/engine/spike-run` (the CMake build) over the `build/gd-debug` one that smoke builds, so a stale CLI can score a newer circuit format.
- The circuit is written to a predictable `/tmp/spike_hl_<pid>_<n>.json` with `std::ofstream` (no `O_EXCL`): a local user can pre-create a symlink there. Use `mkstemp` in `user://` or `$XDG_RUNTIME_DIR`.
- The lab passes the circuit seed as the task seed (`lab.gd:1300`), and `spike-run` reseeds the circuit as `1000 * seed + circuit seed`, so CHECK scores a differently seeded circuit than the live run it sits next to; they can disagree, and a passing CHECK records a pass (`_record_check`).
- No cancel: a job whose lab was closed is never polled and stays in `g_jobs` until exit; `Cleanup` then joins it, so quitting during a boss check blocks for up to ~19 s.
- **Fix:** resolve the runner from the extension's own location (`godot/bin/` next to the `.so`) or `OS.get_executable_path()`, set it once from `main.gd`; secure temp file; pass the live run's seeds through (or add `--circuit-seed` to `spike-run`); add `headless_cancel` and call it from `lab.gd:_exit_tree`.

### P1-C2. Per-task sensor weights are dropped by the game
- `Task::ChannelKind::weight` (the deferred 0.2 item, set by tasks: stretch_reflex Ia 3, reach target 2, renshaw noise 100, worm 1000, lamprey 600) reaches GDScript through `SpikeTask.channel_kinds()` but `Circuits._absorb_one` keeps only `kind`, the skeleton uses `input_weight("iaf_psc_alpha", kind)` and the editor's first connect uses `Circuits.input_weight(model, kind)` (300 dc / 3.5 x synapse Poisson). The briefs still have to say "set the Ia weight to 3, not the first-connect 300".
- **Fix:** keep `weight` in `godot/game/circuits.gd:task_channel_kinds` and use it in `lab.gd:_skeleton` and `editor_view.gd:549-551`.

### P1-C3. Play log (`godot/game/playlog.gd` and its callers)
- Every visit is logged twice: `tasks.gd:163` calls `Playlog.level_start` and `lab.gd:188` calls it again, which closes the first as a quit. Measured: one visit with one edit gives `visits 2, quits 2`. `_set_variant` does the same.
- `runs` counts only RESET presses (`lab.gd:465`), not episodes that end, the automatic re-run after each edit, CHECK or TEST, so "runs before first pass" is near zero.
- `edit` fires on every slider tick (`_on_structure` from `_val_row`), and every 25 events rewrites the whole JSON (up to 4000 events) during a drag.
- Privacy is fine: off by default, local only, no names or paths in the data, CLEAR and EXPORT work.
- **Fix:** one `level_start` (the lab's); count a run when an episode ends or a CHECK/TEST finishes; coalesce edits per structure change (on the 0.3 s reload, not per tick).

### P2-C4. Audio (`godot/game/audio.gd`)
- The limiter is sound (per-population 12 ms gap, 110 clicks/s bucket of 14, density-dependent gain) and tested. But spikes are drained once per frame and stamped with the wall clock, so above 1x every population clicks at most once per frame: a frame-rate buzz unrelated to spike timing. Stamp clicks with model time scaled to wall time, or schedule them across the frame.
- Body sounds are cut above 5x (G3).

### P2-C5. Diagnosis through `set_variant`
- Overloading the variant slot works (`DxTask::set_variant`, `lab.gd:_dx_submit` reads `score()` at once), but it means `Telemetry.configure` resets the answer on every reload, the answer travels to GDScript in `diagnosis().answer`, and the CHECK button has to be hidden because `word()` rejects `>`/`+`. A `submit_diagnosis(id)` method (add-only) would be clearer.

### Performance
- Headless real-time factors (sequential): micros 10-40x, reflex 50x, lamprey 13x, boss_spinal_sprint 2.1x, peristalsis 5.4x, reach 2.4x, renshaw 4.5x, breathing 11.5x. The spinal tasks are the slow ones (large aeif pools with dense `edges`).

---

## Release process (lead)
- **P1:** `CHANGELOG.md` has no 0.3 entry; `README.md` still describes 0.2 ("eleven tasks", "chapters open when half of the previous one is passed").
- **P1:** `docs/REQ-0.3.md` status table still says "Not started" for every item; record done / changed / deferred (live f-I preview, epilogue sandboxes, per-chapter bosses, surprise on lesions, third-star rule).
- **P1:** No test builds the 0.2/0.3 task briefs' recipes any more; two of them (renshaw, posture) rotted with the 0.3 scoring changes and nobody noticed. Put a novice bench per spine task into the task tests, built with the lab's `_default_part` and `channel_kinds`.

---

## Verdict

**Not ready to ship.**

The content work is strong: 19 genuinely one-idea micro-puzzles with good verdicts, two real boss integrations with robust references, six clinically literate diagnosis cases, a hint ladder that reads well, histograms and a celebration that feel like the genre, cited notebook entries, audio with a proper limiter, and 41 references that pass.

What blocks the release is the path a novice actually walks:
- the lab hands them the wrong cell on 14 of 19 micros, so the first level's verdict quotes a false rheobase and `m_excitation` cannot be passed with the knobs it names (N1);
- two Spinal spine levels cannot be passed from their briefs (renshaw's recipe and hints fail the new volley gate; reach needs a spread control that does not exist), so Heart, Breathing and Autonomic never open (N2, N3, G1);
- the diagnosis cases are solved by looking for the missing arrow, and their lesion-lab advice is backwards (S1, S2);
- the modes do not do what their own text says, half the variants are free, and the play log that the gates rely on double-counts every visit (V2, V3, C3).
- the long levels run 50-116 s per episode in the real lab although the default speed asks for 25 s, and the instant check takes up to 19 s (G2).

Fix the P0s and the P1 list; then rebuild the novice harness against the lab's real defaults (part, sensor weights) for every spine level, re-run it in Standard with tier-1/2 hints, and 0.3 is a release.

### P0 / P1 grouped by the files a fix touches (for parallel agents)

| Agent | Files it owns | Items |
|---|---|---|
| **A. Lab** | `godot/ui/screens/lab.gd`, `godot/game/circuits.gd`, `godot/ui/editor_view.gd` | **P0-N1** `_default_part` = the reference's first model (iaf included; today only m_adaptation, m_half_centre, m_pacemaking, m_coupling and m_rhythm open on the right part); **P0-N3(a)** link-inspector spread control writing `edges`/`weight_cv`; **P1-C2** keep `ChannelKind.weight` in `task_channel_kinds`, use it in `_skeleton` and the first connect; **P1-V2** record a `half_neurons` pass only within par; **P1-V3** recipe behind a button (Standard) / hidden (Researcher), Researcher pass only from TEST x5; **P1-C1** pass the live seeds to CHECK, cancel the job in `_exit_tree`; **P1-C3** single `level_start`, run counting, edit coalescing (with I) |
| **B. Engine core and gdext** | `engine/src/circuit.cpp`, `engine/src/spike_run.cpp`, `gdext/src/bridge.{h,cpp}`, `gdext/src/spike_task.{h,cpp}`, `python/spike/circuit.py` | **P0-N3(a)** `weight_cv` on rule projections (with Python parity) if A does not write `edges`; **P1-C1** runner resolved from the extension's location, `mkstemp`, `--circuit-seed`, `headless_cancel`; optional `submit_diagnosis` binding (C5); faster CHECK (G2) |
| **C. Spinal tasks** | `engine/src/tasks/renshaw.cpp`, `posture.cpp`, `reach.cpp`, `boss_spinal_sprint.cpp`, `engine/tests/tasks_spinal_test.cpp`, `docs/circuits/{renshaw,posture,reach,boss_spinal_sprint}.md` | **P0-N2** renshaw brief recipe and tier 2/3 re-derived for the volley gate, value capped when a gate fails; **P0-N3(b)** reach novice bench that passes (after A/B), or off the spine; **P1-N4** posture verdict by sign, TONIC number; novice bench per spine task in the test |
| **D. Invertebrate and micro tasks** | `engine/src/tasks/reflex.cpp`, `worm.cpp`, `m_*.cpp`, `micro_a.h`, `micro_b.h`, `engine/tests/tasks_micro_{a,b}_test.cpp` | **P1-N5** reflex: one cell must not pass Standard; **P1-T1** concepts_new only for new ideas; **P1-T3** reflex and worm notes; **P1-V3** brief/recipe split, mode noise in micro_b; micro tests built with the lab's real default part; P2 verdicts (m_recurrent, m_half_centre, group sizes, empty-bench score) |
| **E. Diagnosis and bosses** | `engine/src/tasks/dx_*.cpp`, `dx_common.h`, `boss_autonomic_exam.cpp`, `circuits/tools/gen_boss_dx.py`, `circuits/dx_*.json`, `godot/ui/diagnosis_panel.gd`, `godot/game/campaign.gd` (`_parent_of` only) | **P1-S1** keep the answer's projection at weight 0 so every candidate is drawn and cuttable; **P1-S2** rewrite the lesion-lab advice (brief, tier 3, panel), show `explain[picked]` and the plain labels; **P1-S3** dx_horner distractor; **P1-T2** dx parent = the inner task; boss note title (T3) |
| **F. Other task notes and glossary** | `engine/src/tasks/breathing.cpp`, `m_negative_feedback.cpp`, `godot/game/campaign.gd` (`CONCEPTS`), `godot/ui/screens/notebook.gd` | **P1-T3** breathing, Cannon, citations on concept cards |
| **G. Campaign and progression** | `godot/game/campaign.gd` (`chapter_open`, `gate_chapter`), `godot/game/progress.gd`, `godot/ui/screens/tasks.gd` | **P0-G1** interim: open chapter i on chapter i-2's spine (G5); **P1-V3** mode blurbs that match behaviour; star text for diagnosis cards; **P1-C3** remove the `level_start` in `tasks.gd:163` |
| **H. Feel, audio, pacing, phone** | `godot/game/telemetry.gd`, `godot/game/audio.gd`, `godot/ui/celebration.gd`, `godot/ui/score_card.gd`, `godot/ui/hint_panel.gd`, `godot/main.gd`, `godot/game/settings.gd` | **P1-G2** larger or batched slices, a wall-clock stepping share, no idle cap during a run, default speed from the delivered speed, CHECK progress; **P1-G3** body sounds at the default speed, celebration z-order, no 100 before pass; **P1-M1** scrollable hint card; phone HINT/CHECK in the top bar (M2) |
| **I. Histograms, docs, process (lead)** | `tools/histograms/gen_histograms.py`, `catalog/histograms/*`, `README.md`, `CHANGELOG.md`, `docs/REQ-0.3.md`, `scripts/smoke.sh` | **P1-V1** all levels, structurally varied seeds, non-degenerate quality metrics; README, CHANGELOG 0.3.0, REQ status; smoke: a novice bench per spine level instead of "dx passes with the answer" |

**Coordination notes**
- **A and B** meet at P0-N3: decide whether the lab writes `edges` with spread or the engine accepts `weight_cv` on `all_to_all` (then Python must follow for parity). C cannot finish reach until that lands.
- **A and D** meet at P0-N1: once `_default_part` changes, D's micro tests must build with the same rule (assert it), so the gate measures the lab.
- **A, G and I** all touch the play log path: G deletes the map's `level_start`, A owns the lab's calls, I owns `playlog.gd`.
- **E** owns `campaign.gd:_parent_of` only; **G** owns the rest of `campaign.gd`.

## Lead decisions (fix round)
- **Reach spread.** The engine accepts `weight_cv` and `weight_sd` on rule projections, seeded per edge, with Python parity (group B). The lab adds a spread slider to the edge inspector (group A).
- **Default part.** The lab opens each level on the cell type that the level's reference circuit and its verdicts assume. Every task declares this through a new add-only method, `Task::default_model()`. The micro tests build on the same default (groups A and D).
- **Unlocking.**
  - A chapter opens when the previous chapter's spine is passed, or when the chapter two before it is passed. That way one stuck task never blocks the game.
  - A spine task that a novice cannot pass is moved to a branch until it can be passed. This applies to reach.
- **Diagnosis.** The answer's wire stays in the patient circuit at weight 0, so every candidate is drawn and can be cut or probed.
- **Recipes by mode.**
  - Guided shows the recipe in the brief.
  - Standard puts it behind a SHOW RECIPE button, and using it costs the third star.
  - Researcher has no recipe, and a pass requires TEST x5.
