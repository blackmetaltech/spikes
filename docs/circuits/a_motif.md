# Atlas level: a_motif, a null-model bet (`circuits/a_motif.json` = `a_motif_given.json`)

Task `a_motif` (`engine/src/tasks/a_motif.cpp`, chapter "a_worm", `level_kind` "diagnosis" (see below), order 992, off the spine, hangs off `a_path`). Circuit: the whole chemical wiring of the hermaphrodite (`data/connectome/worm/whole.json`, Cook et al. 2019): 302 cells, 131 classes as populations, 3709 connections with their synapse counts, locked, no stimulus; four cell-type groups (sensory, interneuron, motor, other) with nothing free; cell names in `meta.cells`.

## The bet
Under which null model is the feed-forward loop more frequent than chance? `edges` (the same number of edges), `degree` (the same in- and out-degree of every cell), `blocks` (the same degrees and the same number of connections between the four cell-type blocks, the circuit's groups). The engine computes the three z-scores itself: `analysis::motif_zscores( g, NullModel, block_of, 100, seed 1 )` for the induced class 030T (self-connections are not part of the graph). The correct option is the one whose z is significant and positive; a test asserts that exactly one is, and that it is `edges` (research/atlas-a.md 4b and "Worm: wiring tables and numbers": positive against the same number of edges, negative against the two stricter shuffles).

No commit: not passed, the verdict asks for the bet (exit 3 in `spike-run`); the verdict after a commit says what the engine found for the committed null model. Value: 100 minus 15 per wrong commit and 3 per instrument read beyond the budget of three (floor 25).

## Why level_kind "diagnosis"
The level is decided by the committed answer, not by a world, and `tasks_circuits_test` caps every non-diagnosis reference at 60 cells (302 here). As a diagnosis-kind task it is skipped by that and by `tasks_quick_full_test`, exactly as the cases are. `a_predict` is a bet of the same kind.
