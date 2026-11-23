# Spike: roadmap after 0.5

Three new wings, one release each. Each wing brings **one new way to play**, **one new body or world** and **one new instrument**, so the game grows in kind and not only in level count.

| Release | Wing | New way to play | New world | New instrument | Detail |
|---|---|---|---|---|---|
| 0.6 | **The Atlas** (connectomics) | Tune, don't build: real wiring is given, the player assigns signs and gains, predicts lesions, tests wiring against chance | Real wiring of the worm and the fly | Wiring matrix, null-model bench, state-space view | [ROADMAP-CONNECTOME.md](ROADMAP-CONNECTOME.md) |
| 0.7 | **Memory** (hippocampus, working memory) | Day and night: what the circuit learns in one phase must still work in the next | A rat on a track and in an arena | Place-field map, theta-phase plot, replay raster | [ROADMAP-MEMORY.md](ROADMAP-MEMORY.md) |
| 0.8 | **Senses and movement** | Replicate the figure: the target is the shape of a classic result | Stimulus screen, two ears, a looming object, a singing syrinx | Receptive-field mapper, state-space view on motor cortex | [ROADMAP-SENSORIMOTOR.md](ROADMAP-SENSORIMOTOR.md) |

Population dynamics is an instrument, not a wing: the **state-space view** (population activity projected on its leading components, with the trajectory drawn live) lands in 0.6 with the fly compass and is reused in 0.7 (replay, working memory) and 0.8 (motor cortex, the eye-position integrator).

## The line that does not move
The honesty contract of 0.4 holds for every new level: the mechanism is checked through the circuit's structure, verdicts and hint tiers 1 to 3 carry no numbers, every level has cheap circuits that must fail and lesions that must break it, references pass seeds 101 to 105 in all three modes, quick implies full, and every citation is verified (`tools/citations/check.py`) before it reaches player text.

Two rules are added for the new wings:
- **Contested mechanisms are checked as phenomena.** Where the field has several live explanations (phase precession, grid cells, replay content), the level scores the phenomenon, accepts any circuit that produces it honestly, and the notebook names the competing accounts. The level never says "this is how the brain does it".
- **Real data carries its provenance.** Every wiring excerpt names its dataset, version, terms of use, the extraction script and what was simplified, on the level card and the What-is-real page. Only the small extracted tables are committed.

Scope ends where circuits end: working memory and decision-making are in; language and reasoning are out, because no agreed circuit exists to check a player's answer against.

## New ways to play (shared by the wings)
| Mode | What the player does | Why it teaches | First used |
|---|---|---|---|
| **Tune, don't build** | The wiring is real and locked. The player sets what the data does not give: the sign of each cell type, a gain, a time constant | This is what connectome-constrained modelling is | Atlas, fly compass |
| **Prediction bet** | Before a lesion or a stimulus, the player commits to an outcome; stars reward a correct, committed prediction over trial and error | Hypothesis before experiment | Atlas, worm |
| **Null-model duel** | Is this motif count, this wiring pattern, real or chance? The player picks the shuffle that keeps the right things fixed | Statistics of wiring; what a null model is | Atlas, worm and mushroom body |
| **Experiment budget** | A fixed number of recordings, silencings and activations | Experimental design. Extends the probe scoring of diagnosis cases | Atlas |
| **Day and night** | One run with phases; plastic weights carry over | Learning, consolidation | Memory |
| **Replicate the figure** | The target is the shape of a published result (a tuning curve, a place field that shifts backwards) | Reading papers; the result as a behaviour | Senses and movement |
| **State space as playfield** | Steer a trajectory to a region, hold it on a ring or a line | Population dynamics | Atlas compass, Memory, motor cortex |

## How each iteration runs
One iteration is one chapter (2 or 3 micro-puzzles, one task, 1 or 2 diagnosis cases) or one piece of shared machinery. The order inside an iteration:

1. **Research note** in `research/`: cells, wiring, the scorable behaviour, the minimum circuit, what NEST can and cannot express, lesions, sources with PMID or DOI.
2. **Engine and data** (add-only API): new inputs, outputs, bodies, extraction scripts; tests.
3. **Levels**: task code, reference circuits, cheap rows, lesions, hint ladders, notebook entries, catalogue entries.
4. **Lab**: body view, the instrument, map plate, level cards; `godot/tests/interaction_graph.json` and `docs/INTERACTION-GRAPH.md` updated in the same commit as any control.
5. **Playtest** by the scripted novice (brief only) and a held-out-seed run of the references.
6. **Docs**: status table in the wing's roadmap, What-is-real, wiki page, CHANGELOG.
7. **CI** green in Docker, then merge. A wing is released when its last iteration merges.

Implementation is done by small agents in their own worktrees; direction, review, and art that needs a sense of geometry stay with the lead. At most two or three agents run at once.

## Before 0.6: debts carried from 0.5.0 (0.5.3)
| Debt | Plan |
|---|---|
| No buildable escape oscillator for the leech heart | an h-current NESTML cell; then a tested escape reference |
| Stretch reflex is too hard from the brief alone (the scripted novice fails it) | strengthen the hint ladder, not the check |
| Spinal boss reference passes 90% of unseen seeds in Researcher | widen the reference's margin; no criterion changes |
| Parts cards do not expose some parameters references set (`E_L`) | expose them |
| Python package lacks the Theory kinds | add `rate_drive`, `tuned_poisson`, `rate_value`, rate populations |

These are small and independent; they run beside the first Atlas iteration.

## Order and status
| # | Iteration | Release | Status |
|---|---|---|---|
| A1 | Atlas groundwork: data extraction, scale benchmark, locked-wiring levels | 0.6 | next |
| A2 | Worm: reading wiring | 0.6 | planned |
| A3 | Fly compass from real wiring; state-space view | 0.6 | planned |
| A4 | Fly mushroom body from real wiring | 0.6 | planned |
| A5 | Same wiring, different dynamics; the feeding circuit boss | 0.6 | planned |
| M1 to M5 | Memory wing | 0.7 | planned |
| S1 to S7 | Senses and movement wing | 0.8 | planned |
