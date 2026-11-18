# v0.4 requirements: Honest puzzles, shippable game

**Status: shipped in 0.4.0** except: Windows export, CI/Docker never run, CHECK over target on five levels, novice gaps (see CHANGELOG "Known gaps").

Source: `docs/REVIEW-HARSH.md`. 0.4 fixes the puzzle core and makes the game shippable. 0.5 will make diagnosis the second core loop. NEST stays as the runtime.

## Rules for every level (the "honesty contract")
1. **Mechanism, not just output.**
   - Every level checks that the circuit uses the mechanism the level teaches, through the circuit structure the task can see. Tasks get structure access (below).
   - A verdict says which part of the mechanism is missing.
   - For example: m_recurrent needs a motor → inhibitory → motor loop. heart_pacemaker needs coupling between pacemaker cells. reflex needs convergence (≥2 sensory cells onto a motor pool).
2. **No numbers in verdicts or tier-1/2 hints.**
   - Verdicts describe the observed behaviour and the direction to change ("too weak to reach threshold"), never the answer ("375 pA").
   - Only the recipe (Guided, or SHOW RECIPE) gives numbers, and tier-3 hints give a direction or a range shape, not the pass band.
3. **Negative tests.** Every level has a table of cheap or degenerate circuits that must FAIL, in `engine/tests/tasks_cheap_test.cpp`. The table includes:
   - an empty bench;
   - a single cell;
   - a tonic cell with all weights 0;
   - the reference circuit with the taught mechanism removed;
   - "max every weight";
   - each cheap circuit listed in REVIEW-HARSH's appendix.
4. **Novice tests that are not circular.** The scripted novice may read only the brief and verdicts and use lab defaults. It may not use recipes, hint tier 3, or knowledge of the reference. It searches with a fixed budget, for example a coarse weight grid over the structure the brief describes. Report the levels it cannot solve; don't hide them.
5. **Real data, honest labels.**
   - Every citation is checked against PubMed or Crossref, with a DOI stored next to it.
   - Wrong citations are fixed or removed.
   - Facts that don't match their task are rewritten.
   - Generated histograms are labelled "generated variants of the reference, not players".
   - A "What is real" page in-game and in the docs lists what is NEST, what is tuned and what is illustrative.

## Engine add-ons
- **Circuit structure for tasks:** `Engine::structure()` returns populations (model, size, E/I sign from outgoing weights), projections (pre, post, weight sign and magnitude, rule) and channels. It is add-only, and tasks query it in `score()`.
- **Faster CHECK:**
  - Run seeds in parallel processes.
  - Use a shorter "quick check" episode where the task allows it.
  - Target: under 3 s for every micro-puzzle and task, and under 8 s for bosses.

## Shipping
- **Bundled content:** levels and the catalogue live inside the Godot project (`res://circuits`, `res://catalog`). There is one source of truth, and `circuits/` at the repo root becomes a symlink or a build step.
- **Export:** export presets for Linux x86_64 and Windows. `spike-run` and the needed shared libraries (or static GSL) are bundled next to the extension. CHECK works from the exported build.
- **Release extension** built in CI.
- **Reproducible build:**
  - A Dockerfile (Ubuntu with build deps, NEST pinned and built static, NESTML pinned in a venv, godot-cpp pinned, Godot 4.7 headless).
  - `scripts/build-all.sh`.
  - `scripts/package.sh`, which produces a zip.
- **CI:** `.github/workflows/ci.yml` runs the Docker build, ctest, pytest, smoke and package.

## Cleanup
- README, PLAN and title texts match what ships.
- Mock levels are hidden from production.
- `lab.gd` is split into lab_controller, lab_layout and lab_inspector, with no behaviour change.
- The screenshots in git are pruned to a curated set, and the rest are gitignored.
- Lab defaults live in one place and are read by both the game and the C++ test harness (JSON).

## UI honesty (after the identity redesign lands)
- REFERENCE is hidden until the level is passed, or in Guided mode.
- The reference-derived lock is a hash of the circuit compared with the reference, not a flag stored in the file.
- Fewer controls before the first build: progressive disclosure.
