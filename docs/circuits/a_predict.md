# Atlas level: a_predict, a prediction bet (`circuits/a_predict.json`, given `circuits/a_predict_given.json`)

Task `a_predict` (`engine/src/tasks/a_predict.cpp`, chapter "a_worm", `level_kind` "atlas", order 993, spine). The given circuit is the one of `a_path` (same locked wiring, same digests).

## The bet
Which one interneuron class can be silenced without losing the backward response? Five options: AVA, AVB, AVD, AVE, PVC. The player commits first (`Diagnosis::bet` is true; the commit is sent with `set_variant`, see DESIGN-0.5.md "Answer strings"), then the lesion run sets that class's scale to 0, touches the head and the verdict says what happened.

## What passes
One class silenced (scale 0), no class half on, the commit (if any) names that class, and the run shows that the worm still backs up. The class for which the response survives is measured, not stored: on circuit seeds 1 to 3 and 101 to 105 exactly one of the five commits passes, AVE (`tasks_atlas_worm_test`; silencing AVA, AVB, AVD or PVC leaves the backward motor neurons silent, research/atlas-a.md 4b). With a matching commit the value is 100 minus 15 per wrong commit and 3 per instrument read beyond the budget of three (floor 25); with no commit the run is a control run that can pass and scores at most 60.

## Honesty
Verdicts say "in this model". The notebook cites Chalfie et al. 1985 for the animal's touch circuit and says the model differs.
