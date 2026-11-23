# Diagnosis case: dx_a_worm (`circuits/dx_a_worm.json`)

Task `dx_a_worm` (`engine/src/tasks/dx_a_worm.cpp`, chapter "a_worm", order 994, diagnosis, off the spine, hangs off `a_path`). World: the head touch of `a_path`. Generator: `python3 godot/circuits/tools/gen_atlas_worm.py`.

## The case
A model worm does not back up when its head is touched. All cells and connections are present; the OUTPUTS of one interneuron class carry nothing (weight 0).

**Answer: `pop:PVC`.** Candidates: `pop:AVA`, `pop:AVB`, `pop:AVD`, `pop:PVC` (lesion-lab ids: silence the class).

## Recording signature (printed by `tasks_atlas_worm_test`, mean Hz per cell over the touch and the 300 ms after, circuit seeds 1 to 3 and 101 to 105)
In all four lesions the touch cells fire (58 to 87 Hz). AVB fires clearly (20.7 to 47.9 Hz) when AVA, AVB or AVD is the silenced class and stays at 0.7 to 5.7 Hz only when PVC is: in this model AVB depends on PVC for its drive. A scope on AVB tells the answer from the other three on all eight seeds. In the answer's patient the backward and forward motor neurons, AVA and AVD are silent and PVC fires (20 to 28 Hz). Caution: in one run (AVB silenced, seed 101) the backward motor neurons are not fully silent (2.9 Hz), so "all four lesions give silent motor neurons" holds on seven of eight seeds; the case does not rely on it.

## Scoring
The diagnosis rules of `dx_common.h` (100, minus 15 per wrong guess and 3 per probe beyond the first three, floor 25). The case is fictional.
