# Diagnosis case: dx_a_worm (`circuits/dx_a_worm.json`)

Task `dx_a_worm` (`engine/src/tasks/dx_a_worm.cpp`, chapter "a_worm", order 994, diagnosis, off the spine, hangs off `a_path`). World: the head touch of `a_path`. Generator: `python3 godot/circuits/tools/gen_atlas_worm.py`.

## The case
A model worm does not back up when its head is touched. All cells and connections are present; the OUTPUTS of one interneuron class carry nothing (weight 0).

**Answer: `pop:AVD`.** Candidates: `pop:AVA`, `pop:AVB`, `pop:AVD`, `pop:PVC` (lesion-lab ids: silence the class).

## Recording signature (`tools/connectome/worm_dx.py`, mean Hz per cell over the touch and the 300 ms after, circuit seeds 1 to 3 and 101 to 105, research/atlas-a.md 4b)
In all four lesions the touch cells fire (60 to 79 Hz) and the backward motor neurons stay silent. AVD fires (10 to 20 Hz) only when it is the silenced class; with AVA, AVB or PVC silenced it stays at 5 Hz or below. A scope on AVD tells the answer from the other three; AVA, AVB and PVC fire in the answer's patient. `tasks_atlas_worm_test` checks it.

## Scoring
The diagnosis rules of `dx_common.h` (100, minus 15 per wrong guess and 3 per probe beyond the first three, floor 25). The case is fictional.
