# Diagnosis case: dx_a_compass (`circuits/dx_a_compass.json`)

Task `dx_a_compass` (`engine/src/tasks/dx_a_compass.cpp`, chapter "a_compass", order 998, diagnosis, off the spine, hangs off `a_gain`). World: the two cues of `a_gain` with a reset between them (`atlas_compass.h`). Generator: `python3 godot/circuits/tools/gen_atlas_compass.py`.

## The case
A model fly whose ring does not settle into one bump at a cue. All cells and connections of the compass are present (not locked, no cards); the OUTPUTS of one cell type carry nothing (weight 0).

**Answer: `pop:D7`** (the Delta7 cells). Candidates: `pop:EPG`, `pop:PENa`, `pop:PENb`, `pop:PEG`, `pop:D7` (lesion-lab ids: silence the type).

## Recording signature (`tasks_atlas_compass_test`, mean Hz per cell, circuit seeds 1 to 40; research/atlas-a.md 4d)
Only two of the five lesions stop the ring holding a bump at both cues. Delta7 silenced: the compass cells fire at 234 to 256 Hz, the Delta7 cells themselves at 340 to 349 Hz. E-PG silenced: the P-EG and Delta7 cells and the first kind of P-EN cells are silent (0.0 Hz) and the compass cells fire at 32 to 73 Hz. P-EG, P-EN a or P-EN b silenced: the ring holds its bump at both cues on 40 of 40 seeds and the compass cells fire at 20 to 22 Hz late, as in the intact compass (21 to 22 Hz). So a scope on the compass cells (over-active) and one on the Delta7 cells (they fire hard but the ring is not held in check) tell the answer from every other candidate, on all 40 seeds. The three candidates that leave the ring working are there to be ruled out by what the recordings show.

## Scoring
The diagnosis rules of `dx_common.h` (100, minus 15 per wrong guess and 3 per probe beyond the first three, floor 25). The case is fictional.
