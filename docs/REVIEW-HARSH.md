# Spike 0.3: a harsh review

Reviewed at `d31d895` (2026-11-18). I changed no project files except this one and the new screenshots in `docs/screenshots/review/`.

**What I ran**
- `ctest --test-dir build -j4`: 34/34 pass, 10 min 47 s wall time, 32 CPU-minutes.
- `pytest python/tests`: 94 pass.
- About 230 headless `spike-run --task` scorings of hand-made cheap circuits (scripts in the session scratchpad; the circuits are described inline below).
- 13 screenshots through xvfb and OpenGL with the `SP_*` hooks, all of which I looked at.
- 24 of the 70 distinct citations checked against PubMed.
- File and line evidence is given inline.

Severity: **S1** damages the product's core promise, **S2** is a real problem a user will hit, **S3** is debt or polish.

---

## Executive verdict

Spike is a well-documented, heavily tested **tuning toy wearing the clothes of a Zachtronics game and a research tool, and it is not yet either**. The core loop is: read a 150-250 word paragraph, drop one to three cells, then move a slider until the verdict (which tells you the number) turns green. That is not puzzle design. It is a guided calibration exercise, and the verdict text does most of the thinking. The scoring checks outputs, never mechanism, and I found cheap circuits that pass levels without the concept the level teaches:
- one tonic neuron with every input disconnected passes "Recurrent inhibition" at 100/100, in Researcher mode too, while the verdict praises "the Renshaw loop";
- one default pacemaker card, with no wiring at all, three-stars the Heart chapter's first big task.

The "realism" is real only at the level that matters least: NEST's point-neuron integrators. Everything the player actually sees is phenomenological or hand-tuned:
- the circuits are 1-80 cells standing in for pools of thousands;
- the bodies use "illustrative" coefficients (`HR = 105 + 80·sym − 85·vagal`);
- the heart cell is a hand-scanned re-fit;
- the histograms are random perturbations of the reference, shown as "other solutions";
- the citations were "written from memory" (`docs/body/README.md:34`). Two of the 24 I checked are wrong.

NEST itself is a liability at this scale: an 80-neuron network runs at about 2x real time headless, the build needs NEST and NESTML from source plus a `/tmp` fallback, and nothing has ever been exported. The engineering discipline (tests, critique loop, honest internal docs) is far above average for a 3-day, 65-commit, 42 k-line agent build. But the product question, "why would anyone play this for more than 20 minutes?", is unanswered, and the visual identity is a recoloured copy of gigawatt's dashboard.

---

## Top 10 problems, ranked by damage

| # | Sev | Problem | Evidence (short) |
|---|---|---|---|
| 1 | S1 | **The core loop is slider-hunting against a verdict that tells you the answer.** No structural puzzle, no design space, nothing to be clever about. | 7 of 19 micros have **zero projections** in their reference (`m_threshold`, `m_convergence`, `m_adaptation`, `m_pacemaking`, `m_rhythm`, `m_recruitment`, `m_neuromodulation`). The `m_threshold` failure verdict prints "rheobase (about 375 pA)", and tier 3 says "between about 400 and 600 pA". The pass band I measured is 400-600 pA, 5/5 seeds. The "novice harness" follows verdict text step by step (`tasks_micro_a_test.cpp:462-536`). |
| 2 | S1 | **Scores check outputs, not mechanism, so lessons can be skipped.** | `m_recurrent`: 1 iaf cell, `I_e` 450 pA, both inputs at weight 0, no Renshaw cell: **5/5, 100.0, Researcher too**. Verdict: "the Renshaw loop is a gain control". `heart_pacemaker`: 1 default `sa_node_pacemaker`, no synapses: **5/5 pass**; par 7 and energy 3 Hz give 3 stars. No test anywhere asserts that a degenerate circuit fails. |
| 3 | S1 | **The "scientifically realistic" claim is oversold.** NEST cells are real; everything the player sees and is scored on is tuned, illustrative or synthetic. | README "The simulation is real". `docs/body/README.md:32-34`: "tuned so that the model... behaves plausibly in a game... References were written from memory". The SA cell is "a re-fit... not validated" (`docs/models/sa_node_pacemaker.md`). The chemoreflex gain is "a game value" (`docs/circuits/breathing.md:28`). Efferents are pool rate / 60 Hz. |
| 4 | S1 | **Citations are wrong, and "post-solve histograms" are mislabeled.** | `boss_autonomic_exam.cpp:373` "Raven PB, Fadel PJ, Ogoh S (2006) J Physiol 574:85" is an AMPK review by Ramamurthy & Ronnett; the real paper is Exp Physiol 91:37. "Guyenet PG, Stornetta RL, Bayliss DA (2010) Respir Physiol Neurobiol 173:244" is Guyenet & Mulkey. The histogram card says "Your run against other solutions of this task" (`lab.gd:1496`), with no "generated" label. The empty state claims "developer and AI solutions", but the data is random perturbations of the reference (`tools/histograms/gen_histograms.py:2-5`). |
| 5 | S2 | **Nobody outside this container can build or ship it.** | No CI (`.github` absent), no git remote, no `export_presets.cfg`. Godot reads `res://../circuits` and `res://../catalog`, which are outside the project, so a `.pck` export loses every level (`circuits.gd:104`, `catalog.gd:39`). CHECK needs `spike-run` next to the `.so`, and `godot/bin` has none. `libspike_gd.so` links the system `libgsl.so.27`. PyNEST lives in `/tmp/nest-install`, found only by a hard-coded fallback (`conftest.py:19`, `build-nestml.sh:29`); the README's `third_party/nest-install` does not exist. The release `.so` (17:18 on 11-17) predates the 0.3 engine commit `0341810` (17:43). NESTML is unpinned (`nestml>=9,<10`); without it Heart, Breathing and Peristalsis silently fail to load. |
| 6 | S2 | **UI overload and generic look.** | The lab shows 26 controls in three rows before the bench (`review/lab-first-1920x1080.png`). REFERENCE (the solution) sits next to BUILD on the very first level. A 10-line brief wall (`review/lab-reflex-empty-1920x1080.png`). Palette, layout, all-caps button bars and the "instrument dashboard" are gigawatt's (`gigawatt/godot/ui/kit.gd:5-24` vs `spike/godot/ui/kit.gd:6-18`: navy, cyan, amber, green, red). The boss "world" is four gauges, not a body (`review/boss-1920x1080.png`). |
| 7 | S2 | **NEST is the wrong engine for an 80-neuron game.** | `boss_spinal_sprint` (80 cells, 40 s) takes 15.8 s headless, so CHECK is not instant. `breathing` (28 cells) takes 10.4 s and `reach` 5.1 s. One engine per process (`engine.cpp:1-2,719`) forces CHECK, the catalogue and the histograms into child processes. The 1 ms closed loop, `prepare()` re-runs and a 163 MB `nest-build` for a few dozen leaky integrators. |
| 8 | S2 | **Content is bespoke C++ per level; no level format, no editor, no community.** | 41 hand-written task files of 150-470 lines, each with its own scoring, verdict strings and hard-coded numbers (`engine/src/tasks/*.cpp`). Adding a level means C++ and a rebuild. Opus Magnum and SpaceChem live on shared solutions and user puzzles; Spike cannot have either. |
| 9 | S2 | **The tests prove that references pass, not that the game is sound.** | Every task test is "reference passes, lesion fails, scripted novice passes". Not one asserts that a cheap circuit fails (see #2). The smoke test still passes diagnosis cases by submitting the answer (`smoke.sh:39-43`), and accepts "any of seeds 1-3" because "references have narrow plateaus". Lab constants are duplicated in C++ (`micro_lab.h:30-37` vs `circuits.gd:53-62`). The suite costs 32 CPU-minutes. |
| 10 | S3 | **Agent-churn debt and stale docs.** | README says "Version 0.2 has eleven tasks" and the 0.2 unlock rule. `PLAN.md`'s release table (0.2 "STDP reward tasks", 0.3 "evolutionary motif search") has nothing to do with what shipped. The title promises "dodge a shadow"; no looming task exists (`title.gd:25`). The "Adapting" preset `tau_w` 500 lies outside its own slider range of 5-400 (`circuits.gd:13` vs `:54`). There are 217 GDScript lines over 200 chars; `lab.gd` is 1593 lines, 357 of them `;`-chained. Mock levels ship in production (`campaign.gd:13-14,66,177`). There are 48 MB of screenshots in the repo. |

---

## 1. Is it fun?

**Verdict: no, not past 20 minutes for anyone but a neuroscience student who has been assigned it.**

### F1 (S1). The core loop's weakest point is the moment of insight, because there isn't one
- **What the loop is.** Zachtronics games give you a small, orthogonal instruction set and a goal with **many structurally different solutions**. The pleasure is finding one, then a better one, and seeing it on a histogram of real players. Spike gives you a continuous parameter space and a verdict that names the parameter and its value.
- **First level.** `m_threshold` is "set one number between 400 and 600". My sweep, scored on 5 seeds:

  | pulse weight (pA) | passes (of 5) | score |
  |---|---|---|
  | 300 | 0 | 0 |
  | 380 | 3 | 71 |
  | 400-600 | 5 | 100 |
  | 620 | 2 | 74 |
  | 640+ | 0 | 57 |

  The failure verdict already prints the rheobase. Tier 1 of the hint prints it again (`spike-run` output, `hints[0]`).
- **Inhibition.** The hint's tier 3 says "weight at about -1000 pA... roughly 12 times larger" (`review/hint-1920x1080.png`). The novice script in `tasks_micro_a_test.cpp:476-479` is literally "-80 → -1000 as the verdict said".
- **What this teaches.** It teaches reading, not reasoning. Opus Magnum's equivalent would be a dialog box telling you to rotate the arm 60°.
- **Fix.** Make the verdicts *qualitative and diagnostic* ("the cell fired on the blip"), never "raise to X". Move numbers to tier 3 only. Most importantly, design levels whose **structure** is the puzzle:
  - a fixed parts budget of fixed-weight synapses (weights from a small discrete set: weak, medium, strong);
  - a fixed cell palette;
  - a target behaviour that admits several topologies.

  Discrete weights alone would turn this from calibration into a puzzle.

### F2 (S1). There is no design space to optimise
- **Par is trivial.** The third star is "neurons ≤ par and energy ≤ 1.6 × reference". Par is the reference's own size, and a single cell beats it on several levels (#2).
- **The histograms add nothing.** They come from perturbing the reference, so the reference sits on the median of every axis (`review/histo-1920x1080.png`: "you: 23 neurons, median 23; better than 60%"). There is no frontier to chase.
- **Fix.**
  - Pick metrics where real trade-offs exist, for example latency versus false alarms versus neuron count in `reflex`, or settling time versus overshoot in `baroreflex`.
  - Seed the histograms from structurally different solutions (minimal, redundant, alternative topologies), label them honestly, and plan real uploads.

### F3 (S2). Watching is the payoff, and the bodies are too small to pay
- **What works.** The worm and lamprey on the map (`review/map-fresh-1920x1080.png`) and the eye in `dx_horner` (`review/dx-1920x1080.png`) are the best things on screen.
- **What doesn't.** The autonomic boss, the climax of the game, shows four dial gauges and a smeared V_m trace (`review/boss-1920x1080.png`). Kerbal works because the rocket *is* the feedback: it wobbles, it explodes, it is funny. Here the "body" is a 600×200 px inset beside a raster.
- **Fix.** Make the body the main view and the circuit the inset when the run plays. Give failure a physical, funny consequence: the arm flops, the worm swims away from the salt, the lamprey spins.

### F4 (S2). Session shape
- Micros are 4 s episodes with a 0.4 s CHECK, which is good.
- The big tasks are 20-150 s episodes. The bosses take 9-19 s to CHECK headless (measured: `boss_spinal_sprint` 15.8 s, `breathing` 10.4 s), with a "CHECKING…" toast in between. Iteration on the levels that should be most interesting is the slowest.
- **Comparison.** SpaceChem's rule is that a solution test takes seconds and shows *where* it failed. Spike's long tasks fail in a paragraph of verdict at the end.

### F5. Honest comparisons
| Game | What it does that Spike does not |
|---|---|
| Opus Magnum / SpaceChem | Discrete parts, many solutions, real-player histograms, GIF export, the solution *is* the spectacle |
| Kerbal | Physical consequence, emergent failure comedy, a sandbox people stay in for 100 h |
| Foldit / Eterna | Real science, with player solutions that matter |
| Educational sims (PhET, Neuronify, BrainFacts) | Neuronify does "drag neurons, watch spikes" on a phone in an afternoon, with no NEST |

Spike's unique asset is *closed-loop bodies driven by spiking circuits plus lesion-based diagnosis*. That is genuinely uncommon, and it is buried under tuning.

---

## 2. Scientific honesty

### H1 (S1). Where "realistic" holds and where it doesn't

| Layer | Status | Evidence |
|---|---|---|
| Neuron dynamics | Real NEST models (iaf, aeif, izhikevich) | Golden checksums, PyNEST parity |
| NESTML cells | Hand re-fits, explicitly not validated | `docs/models/sa_node_pacemaker.md` ("a throwaway Python prototype, not kept"); ICC is FitzHugh-Nagumo, "phenomenological, not a channel model" |
| Circuit sizes | 1-80 cells for pools of 10³-10⁵ | `circuits/*.json` (reference sizes: reflex 23, posture 56, boss 80) |
| Synaptic weights | Tuned until the task passes | `docs/circuits/posture.md:45` ("found by sweeping"), `reach.md:77` ("hill-climbing") |
| Body physics | Lumped, coefficients "illustrative" | Heart rate is linear in sym and vagal drives (`docs/body/heart.md`); `lungs.md:26`, `muscle.md:38`, `eye.md:9`, `joint.md:24` |
| Efferent coupling | Pool rate / 60 Hz to a 0..1 drive | `docs/body/README.md` |
| Modulators | Global scalars with heuristic gains | `docs/models/icc_slow_wave.md:20` ("heuristic... not fitted") |
| Histograms | Synthetic | `tools/histograms/gen_histograms.py` |
| Citations | "Written from memory" | `docs/body/README.md:34` |

The internal docs are admirably candid. The **player-facing** text is not:
- README: "The simulation is real";
- the CHANGELOG: "Every pass unlocks a true, cited finding";
- the title: "biologically realistic".

**Fix.** Put a one-screen "What is real here" page in the game (Credits or Notebook) with that table. Change "realistic" to "built on real neuron models; bodies are simplified".

### H2 (S1). Citation spot check (PubMed)
I checked 24 of the 70 distinct citation strings. 22 are correct, including:
- Mendell & Henneman 1971;
- Jose & Collison 1970;
- Michaels, Matyas & Jalife 1987;
- Grillner 2003;
- Smith 1991;
- Feldman & Del Negro 2006;
- Suzuki 2008;
- Thiele 2009;
- Williams & Baker 2009, which is apt.

Two are wrong:
- **`engine/src/tasks/boss_autonomic_exam.cpp:373`** "Raven PB, Fadel PJ, Ogoh S (2006) J Physiol 574:85". According to PubMed, J Physiol 574:85-93 is Ramamurthy & Ronnett, "Developing a head for energy sensing: AMPK…" ([DOI](https://doi.org/10.1113/jphysiol.2006.110122)). The intended paper is Raven, Fadel & Ogoh, "Arterial baroreflex resetting during exercise: a current perspective", *Exp Physiol* 91:37-49 ([DOI](https://doi.org/10.1113/expphysiol.2005.032250)).
- **"Guyenet PG, Stornetta RL, Bayliss DA (2010) Respir Physiol Neurobiol 173:244"** (CCHS case). According to PubMed, that article is Guyenet & **Mulkey**, "Retrotrapezoid nucleus and parafacial respiratory group" ([DOI](https://doi.org/10.1016/j.resp.2010.02.005)). Guyenet, Stornetta & Bayliss 2010 is *J Comp Neurol* 518:3883, which is cited correctly elsewhere. The two were conflated.

Also checked: Williams & Baker 2009 ([DOI](https://doi.org/10.1523/JNEUROSCI.0272-09.2009)) and Goyal & Chaudhury 2010 ([DOI](https://doi.org/10.1053/j.gastro.2010.08.013)), the latter an editorial comment rather than primary evidence.

Two wrong out of 24 is a ~8% error rate on a feature sold as "true, cited". **Fix:** run every citation through a DOI resolver in CI, store DOIs instead of free text, and show the DOI in the notebook.

### H3 (S2). Fact-to-task mismatches and invented premises
- **`reflex`.**
  - The brief is a cutaneous *touch* withdrawal (polysynaptic, flexor-reflex afferents), but the notebook explains it with Ia afferent → motoneuron EPSPs (Mendell & Henneman), which is the monosynaptic *stretch* reflex.
  - The premise "a touch receptor chatters… about 150 spikes/s" in the background is not physiological. Cutaneous low-threshold mechanoreceptors are near-silent at rest.
- **`dx_clonus`.**
  - It still presents a 10 Hz postural tremor from isolated Renshaw loss (`dx_clonus.cpp:21,34`). Clinical clonus is a 5-8 Hz stretch-evoked oscillation from upper-motor-neuron hyperexcitability.
  - The case is a fictional disease with a real clinical name. Rename it ("the shaky arm") or model a real cause.
- **`m_recurrent`.** The level passes without a Renshaw loop, and the verdict tells the player that the loop they did not build "is a gain control" (#2). The game is asserting false facts about the player's own circuit.

### H4 (S2). Synthetic histograms presented as other people's solutions
- `lab.gd:1496`: "Your run against other solutions of this task… Beat the median."
- The empty-state text says the data is "generated offline from developer and AI solutions". That is also untrue: it is random perturbation of the reference (`gen_histograms.py:2-5`).
- `HISTOGRAMS.md` is honest. The game screen is not.
- **Fix:** label the card "Generated variants of the reference circuit (not players)".

---

## 3. Learning design

### L1 (S1). The ladder teaches you to follow instructions
- Briefs explain the concept well (the micro briefs are the best writing in the project).
- Then three channels hand over the numbers:
  - the failure verdict ("about 12 times its present weight", "rheobase about 375 pA");
  - hint tier 1, which often includes the number;
  - the recipe (Guided: inline; Standard: one button).
- The "novice passes in 1-5 runs" gate is **circular**: the same agents wrote the verdicts and the scripted novice, who obeys the verdicts (`tasks_micro_a_test.cpp:453-536`). The gate measures whether the verdicts are internally consistent, not whether a human learns.
- `m_pacemaking` is passed by the default card with zero actions (`tasks_micro_a_test.cpp:521`: "the part the lab opens on… true").
- **Fix.**
  - Make transfer the test: after each micro, a "same concept, new numbers" variant where the verdict says only *what* went wrong (fired on the blip, too late, never), never *how much*.
  - Run five real novices with the play log on and measure runs-to-pass and quit points. The play log exists; there is no human data in the repo.

### L2 (S1). Mechanism is never checked, so the concept can be skipped (see #2)
- **Fix:** each task declares a *structural* precondition, checked on the circuit graph, plus a *causal* check, both as an add-only `Task::requires()`:
  - structural, for example "a projection from the motor pool to an inhibitory population that projects back";
  - causal: re-run with the student's own key projection lesioned, and the score must drop.

  Renshaw already does the causal check on volleys. Generalise it.
- Add negative tests per level: a tonic cell, a single default card, and all inputs at weight 0 must fail.

### L3 (S2). The reference solution is one click away on every level, in every mode
- A REFERENCE button sits on the map card next to BUILD (`tasks.gd:171`) and in the lab (`lab.gd:396`), including in Researcher mode.
- The "reference-derived" lock is a flag inside the circuit's own `meta` (`lab.gd:272-277`). LOAD from disk never sets it (`lab.gd:529`), so loading `circuits/reflex_arc.json` from the file dialog gives full stars. Opening the reference, noting its weights, then EMPTY BENCH and retyping also gives full stars.
- **Fix:** hide REFERENCE until the level is passed (or in Guided only), and fingerprint the topology rather than trust a flag.

### L4 (S3). What's good
The concept graph is real (19 one-idea micros, ordered, mechanically checked). The lesion lab is the best teaching tool here: "cut it and see" is exactly how physiology is learned. The diagnosis cases are the strongest learning design in the project. Build the game around them (see the last section).

---

## 4. UX/UI and visual identity

### U1 (S2). Information overload
- **Desktop lab.** It shows 26 controls before the bench (13 in the top row, 7 in the second, 6 in the third: `review/lab-first-1920x1080.png`). A first-time player on level 1 sees all of these:
  - TEST x5, LESIONS, ANALYSIS, LOAD, SAVE, EMPTY BENCH, REFERENCE;
  - a mode stepper, CHECK, SKIP, HINT, RESULTS;
  - a parts picker, + EXCITATORY, + INHIBITORY, + NOISE, CONNECT, FIT.
- **The first level needs four of those controls**: one neuron, two drags and a slider.
- **Briefs are walls of text.** `reflex` is 10 lines of 17 px text (`review/lab-reflex-empty-1920x1080.png`) and the boss brief is 9 lines with a scrollbar (`review/boss-1920x1080.png`).
- **Read-outs aren't explained.** The results strip has 7-10 chips ("burst threshold 3", "false triggers 0", "latency 0 ms") on an unbuilt circuit. The raster tick labels repeat values ("-0.5 s -0.5 s -0.4 s -0.4 s").
- **Fix.** Progressive disclosure: level 1 shows the bench, one slider, PLAY and the gauge. Unlock buttons as concepts need them (SAVE and LOAD in Sandbox; LESIONS after the first pass; ANALYSIS in chapter 2). Cut briefs to two sentences plus "more…".

### U2 (S2). Phone (844×390)
- The lab opens with no brief and no CHECK or HINT visible (`review/lab-first-844x390.png`).
- The hint card covers the sensors it tells you to wire and is clipped at tier 2 (`review/hint-844x390.png`).
- Chapter tabs read "SPINE", which collides with "spine" (main path) in the unlock text, and "LUNGS" for a chapter called Breathing elsewhere.
- The map's first node is jammed into the bottom-left corner on both layouts (`review/map-fresh-*.png`).
- Neither the phone nor any other platform is actually built (desktop Linux only, per the CHANGELOG), so the phone layout is untested speculation.

### U3 (S2). Visual identity: a recoloured clone
- `godot/ui/kit.gd` copies gigawatt's design language and lightly recolours it:

  | | gigawatt | Spike |
  |---|---|---|
  | Background | `BG 0.035,0.05,0.075` | `BENCH #070b13` |
  | Accents | `CY 0.3,0.82,1` / `AM 1,0.72,0.2` / `GR` / `RD` | `CYAN #4fd1ff` / `AMBER #ffb347` / `GREEN` / `RED` |

  Same rounded navy panels, same uppercase pill buttons in rows, same "instrument dashboard" (`gigawatt/docs/screenshots/design.png` vs `review/lab-first-1920x1080.png`).
- `wrap_guard.gd` is byte-identical and `touch_scroll.gd` differs by 3 lines. PLAN.md says the shell came from Mugshot.
- The result looks like a generic dark sci-fi admin panel, not a neuroscience game. Nothing on screen says "living tissue".
- **Fix.** Commit to a visual metaphor that is native to the subject:
  - a dissecting-microscope bench, with ink-on-paper Cajal-style neurons, stained tissue colours and a light background;
  - or oscilloscope phosphor taken seriously, with the CRT, the grain and the audio click as the identity.

  Draw the body large and alive. Drop the copied pill-button bars.

> **Note:** while this review was being written (09:07 on 2026-11-18), an uncommitted change to `godot/ui/kit.gd` began a "Cajal Plate" identity: paper, ink, Golgi sienna and Prussian blue, the direction proposed above. This review and its screenshots cover the committed `d31d895`. Token names are kept and only values change, so the layout and button-bar density problems in U1 remain.

### U4 (S3). Specific glitches seen
- "Pass with 1 neurons or fewer" (map card, `review/map-fresh-1920x1080.png`).
- Population labels overlap neurons in dense circuits: "NA ×4" over the VG column, "SAD ×4" hidden (`review/boss-1920x1080.png`), and "IN_F ×2", "TONIC" and "B2" collide (`review/stretch-1920x1080.png`).
- The V_m scope on the bosses is a saturated yellow smear (`review/boss`, `review/stretch`).
- The diagnosis view shows the lesioned target group dark (SCG in `review/dx-1920x1080.png`). The "probing" is "find the dark column", which is fine for the first case and trivial by the sixth.

---

## 5. Engineering

### E1 (S2). Architecture
- The C++ engine contract (`engine/include/spike/engine.h`, add-only) is clean. The body modules have no NEST dependency and are deterministic (splitmix64, no `std::random` distributions). These are good decisions.
- **One engine per process** (`engine.cpp:1-2,719`) is NEST's singleton leaking into the product:
  - the game cannot score a variant while the live run plays without spawning `spike-run` (`gdext/src/bridge.cpp:258-290,461-470`);
  - the catalogue probes each model in its own process;
  - the histogram generator is a process farm.
- The runner search order still includes `./spike-run`, `../build/engine/spike-run` and finally **`$PATH`** (`bridge.cpp:280-287`). That means "whatever binary named spike-run is first", with a possibly mismatched circuit-format version.
- **Fix:** ship `spike-run` inside `godot/bin`, check `--version` and refuse a mismatch, and drop the `PATH` fallback in release builds.

### E2 (S2). NEST is the wrong dependency for the game (keep it for the research side)
- The largest level is 80 neurons. Measured headless (contended host):

  | level | episode | headless run |
  |---|---|---|
  | `boss_spinal_sprint` | 40 s | 15.8 s |
  | `breathing` | 150 s | 10.4 s |
  | `reach` | 13 s | 5.1 s |

- At this size, a purpose-built exact integrator for iaf, aeif and izhikevich (or the same NESTML models generated to plain C++) runs well over 100× real time, multi-instance, on any platform including web and Android.
- NEST costs:
  - a 163 MB build tree and a pinned commit cloned from GitHub;
  - GSL linked dynamically;
  - one thread;
  - no web or mobile;
  - the singleton.
- **Fix:** keep the circuit JSON and PyNEST parity as the research export ("your circuit runs in NEST"). Run the game on a small embedded simulator validated spike-for-spike against NEST in CI.

### E3 (S2). Determinism claims
- **Claimed:** "Results are deterministic, checked with golden spike checksums".
- **What is tested:** same-process, same-binary equality and slicing invariance (`engine/tests/test_determinism.cpp:27-36`), plus three checksum goldens (`engine/tests/golden/*.json`).
- **What is not:**
  - checksums are FNV over floating-point-derived spike steps, so they will differ across compilers, `-ffast-math`, libm and CPU features. No second platform has ever run them.
  - nothing tests that the live lab run and CHECK agree. The lab's run is driven by frame-time slicing in GDScript (`telemetry.gd`); CHECK runs in another process.
- **Fix:** a test that drives the GDExtension stepping path and `spike-run` on the same circuit and seed and compares scores. Call the goldens "per-build".

### E4 (S2). Test quality
- **Tests that test themselves.**
  - The smoke diagnosis check greps the answer out of the C++ source and submits it (`scripts/smoke.sh:39-43`).
  - The novice harness hard-codes the lab's defaults in C++ (`engine/tests/micro_lab.h:30-37`), duplicating `godot/game/circuits.gd:53-62` with no cross-check. It has already drifted: the "Adapting" preset's `tau_w` 500 is outside the GDScript slider maximum of 400 (`circuits.gd:13`, `:54`).
  - `ENERGY_HZ` per task lives in GDScript (`progress.gd:31-33`), not in the engine that owns `par_neurons`.
- **Flaky by design.** The smoke test passes a reference "on at least one of seeds 1-3… references have narrow plateaus" (`smoke.sh:37`). If the reference only passes on some seeds, the level is fragile for players too.
- **No negative tests.** Nothing asserts that a trivially wrong circuit fails, which is why #2 exists.
- **Cost.** `tasks_boss_dx_test` 647 s, `tasks_autonomic_test` 498 s, `tasks_heart_test` 392 s, `tasks_spinal_test` 309 s, `tasks_circuits_test` 309 s. The suite is 32 CPU-minutes, effectively a nightly job, with no CI to run it.
- **Fix:** a per-level "cheese suite" (empty, tonic, single default card, reference minus its key projection), all of which must fail. A seed-robustness gate: the reference passes on 9 of 10 seeds. Generate `micro_lab.h` from `circuits.gd` (or both from one JSON).

### E5 (S3). Agent-churn debt
- **History:** 65 commits and 42 k lines in 3 days, about 8 agents per release working in parallel.
- **Stale docs:**
  - `README.md` paragraph 1 (0.2, "eleven tasks"), the campaign section ("Chapters open when half…") and "no audio";
  - `PLAN.md` release table and shape (Mugshot, "dodge a looming shadow, localise a sound");
  - `REQ-0.1.md:67`.
- **Dead or dev code in production:** `godot/game/mock_task.gd` (131 lines) and the `SP_MOCK` / `SP_MOCK03` synthetic levels in `campaign.gd`, 19 references in all.
- **Style:** 217 GDScript lines over 200 chars and 26 over 300 (maximum 494, `lab.gd`). `lab.gd` is 1593 lines with 357 `;` chains: a god-object screen with editor, run control, diagnosis, hints, histograms, celebration and file I/O.
- **Duplicated vocabularies:** concept ids in `tasks_modes_test.cpp:21-23`, `campaign.gd:CONCEPTS` and `CONCEPT_ALIAS` (which exists only to glue `travelling_wave`, `traveling_waves` and `feedforward` vs `feedforward_feedback`).
- **Repo weight:** 48 MB of screenshots are committed, and the critique folders are git-ignored but still on disk.
- **Fix:**
  - split `lab.gd` into `LabRun`, `LabEditorPanel`, `LabDiagnosis` and `LabOverlays`;
  - one `concepts.json` read by both engine and game;
  - move the mocks behind a debug feature tag;
  - delete or rewrite `PLAN.md`;
  - Git LFS or an external host for the screenshots.

### E6 (S2). Build and packaging: can anyone else build and ship it? No.
- **No CI and no remote.** README line 76 claims "how CI… check[s]".
- **No Godot export preset.**
  - Reference circuits, the catalogue and the histograms are read from `res://../circuits` and `res://../catalog` (`circuits.gd:104`, `catalog.gd:39`, `histogram_view.gd:33`): outside the project, so they are absent from any export.
  - `godot/bin` has no `spike-run`, so CHECK in an export fails.
  - `libspike_gd_release.so` predates the 0.3 engine changes, so a release export would run an older engine.
  - It depends on the host's `libgsl.so.27`.
- **Undocumented PyNEST location.** It lives in `/tmp/nest-install` here. The tests pass only because `python/tests/conftest.py:19` and `scripts/build-nestml.sh:29` hard-code `/tmp`, and the documented `third_party/nest-install` is absent.
- **NESTML:** `pip install "nestml>=9,<10"` from PyPI at build time, generated C++ git-ignored. A NESTML minor release can change generated code silently. Without it, `heart_*`, `breathing`, `peristalsis`, `m_pacemaking`, `m_coupling` and all six dx/boss levels using SA, Butera or ICC cells fail to load.
- **Fix:**
  - a `Dockerfile` (or Nix flake) that builds NEST, NESTML (pinned exact version, generated code vendored) and the engine;
  - GitHub Actions running `ctest -L fast`, smoke and a nightly full suite;
  - `export_presets.cfg` plus a `scripts/package.sh` that copies `circuits/`, `catalog/` and `spike-run` into the export and bundles or statically links GSL;
  - then try the Windows and macOS builds, which NEST makes painful (another reason for E2).

### E7 (S3). Performance
- The headless real-time factors are poor for the size (E2), and the 1 ms closed loop calls NEST `run` 1000 times per model second.
- The critique measured the in-game delivered speed at 1.3-2.8× on long levels. The 0.3 fix ("stepping share up to 70% of frame time") was not re-measured in the CHANGELOG with numbers. Verify it on real hardware with a GPU and on llvmpipe.

---

## 6. Product

### P1. Who is it for?
| Audience | Fit today | Why |
|---|---|---|
| Puzzle gamers (Zachtronics fans) | Poor | No structural puzzle, numbers given away, synthetic histograms, no sharing |
| High-school biology | Poor | Text-heavy, pA and nS units, desktop Linux only |
| Undergrad physiology or neuroscience course | **Plausible** | Concept ladder, lesion lab, diagnosis cases, notebook. Teachers need a build that runs on lab PCs |
| Computational neuroscience researchers | Weak | The Python side has no tasks or bodies ("There are no tasks or body modules in Python", `python/README.md:75`). Parity covers open-loop spikes of 8 toy circuits, none of them game circuits. A researcher would just use PyNEST |

### P2. The pitch that works
"**House M.D. for the nervous system.** A patient's circuit is broken. Probe it, lesion it, diagnose it, then rebuild it."

The six diagnosis cases are the only content where the player *reasons* (which cells are silent, what does cutting X do) rather than calibrates. That is the notable, non-forgettable game here. It is also what physiology teachers want.

### P3. What makes it forgettable
- Another dark dashboard.
- A paragraph per level.
- A slider and a gauge.
- "Realistic" claims undermined by tuned numbers.
- A Linux-only build no one can install.

---

## What I'd cut / what I'd double down on

**Cut**
- **Embedding NEST in the game runtime.** Replace it with a small validated simulator. Keep the NEST and PyNEST export for credibility.
- **The REFERENCE button before a pass**, and number-giving verdicts.
- **Researcher, Guided and Standard as three modes.** One mode plus optional hints is enough for the current audience size.
- **Synthetic histograms.** Remove them until they are honest and varied, or label them plainly.
- **Half the big tasks.** Keep the ones with a strong body visual (lamprey, worm, limb, eye, heart). Cut or merge the dashboard-only ones (`heart_autonomic`, `fight_or_flight` as a gauge wall).
- **`PLAN.md`, the mock levels and the 48 MB of committed screenshots.**
- **The phone layout,** until there is a phone build.

**Double down**
- **Diagnosis as the core loop.** 30 cases, procedurally varied lesions, a patient chart, clinical vignettes reviewed by a physiologist, scored on probes used (fewer probes = better) with real-player histograms. Rebuild after diagnosing.
- **Structural puzzles with discrete parts:**
  - weights from {weak, strong}, fixed cell types, a fixed part budget;
  - mechanism checks (structural plus causal lesion) so the concept cannot be skipped;
  - negative "cheese" tests per level.
- **The body as the spectacle.** A big, animated, funny-when-it-fails body view. GIF export of a run, which is Opus Magnum's growth engine.
- **The lesion lab** as the universal verb: every level ends with "now cut something and predict what happens".
- **Honesty as a feature.** A "What is real" page, DOIs on every notebook card, CI-checked citations, a physiologist sign-off. That is a differentiator in edu-games, where most are vague.
- **A shippable build:** Docker, CI, export presets and one Windows build. Then put it in front of five students and five puzzle players and read the play logs before writing another level.

---

### Appendix: the cheap-circuit measurements (reproducible)

| Level | Circuit | Result (seeds 1-5) |
|---|---|---|
| `m_recurrent` | 1 `iaf_psc_alpha`, `I_e` 450 pA, `drive` and `kick` at weight 0, no projections | 5/5, 100.0 (also `--mode researcher` 5/5) |
| `m_recurrent` | same, with `I_e` 380, 600 or 900 | pass (seed 1) |
| `heart_pacemaker` | 1 `sa_node_pacemaker` (catalogue default), no projections | 5/5, 75.0, 64-71 bpm; par 7, energy budget 3 Hz → 3 stars possible |
| `m_threshold` | the reference, pulse weight swept | pass band 400-600 pA (5/5); 380 and 620 pass 3/5 and 2/5 |
| All 41 levels | 1 silent cell, all inputs at 0 | no passes; partial scores up to 48.8 (`boss_autonomic_exam`), 42.5 (`m_convergence`, `m_subtraction`) |
| All 41 levels | 1 tonic cell (`I_e` 380, 450, 600, 900), inputs at 0 | only `m_recurrent` passes |

Screenshots for this review: `docs/screenshots/review/` (title, map-fresh, lab-first, lab-reflex-empty, hint, histo, boss, dx, stretch, notebook; phone: lab-first, map-fresh, hint).
