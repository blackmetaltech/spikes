# Atlas level: a_predict, a prediction bet (`circuits/a_predict.json` = `circuits/a_predict_given.json`)

Task `a_predict` (`engine/src/tasks/a_predict.cpp`, chapter "a_worm", `level_kind` "diagnosis", order 993, off the spine, hangs off `a_path`). The circuit is the given touch circuit of `a_path` (same locked wiring and digests, every class at scale 1, nothing to edit).

## The bet
Which one interneuron class can be silenced without losing the backward response? Five candidates: AVA, AVB, AVD, AVE, PVC. The player commits first (`Diagnosis::bet` is true; the commit is sent with `set_variant`, see DESIGN-0.5.md "Answer strings"). The TASK then silences the committed class itself (before the first slice it sets the weight of every connection out of that class to 0, as a lesion), touches the head, and the verdict says what happened. The game edits no circuit.

## What passes
A commit naming one of the five classes, and the run shows that the worm still backs up. The class for which the response survives is measured, not stored: on circuit seeds 1 to 3 and 101 to 105 exactly one of the five commits passes, AVE (`tasks_atlas_worm_test`; silencing AVA, AVB, AVD or PVC leaves the backward motor neurons silent, research/atlas-a.md 4b). No commit: not passed, the verdict asks for the bet (exit 3 in `spike-run`). Value: 100 minus 15 per wrong commit (the W of `cand@N:W`) and 3 per instrument read beyond the budget of three (floor 25). An edited circuit (a class not at scale 1) is read as an edited circuit, not as a lesion.

## Honesty
Verdicts say "in this model". The notebook cites Chalfie et al. 1985 for the animal's touch circuit and says the model differs.
