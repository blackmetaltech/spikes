# Atlas level: a_gain (`circuits/a_gain.json`, given `circuits/a_gain_given.json`)

Task `a_gain` (`engine/src/tasks/a_gain.cpp`, world in `atlas_compass.h`, chapter "a_compass", `level_kind` "atlas", order 997, spine). Generator: `python3 godot/circuits/tools/gen_atlas_compass.py`. Study: `research/atlas-a.md` 4d.

## The puzzle
The compass wiring with the signs that hold a bump (the reference of `a_signs`) is given and locked. One cell-type card has a free number: the scale of the Delta7 cells (`DELTA`), which multiplies every connection they make (stored magnitude x scale x the fixed sign -1), range 0.1 to 1.8 in steps of 0.05, given at 0.15. The other cards (E-PG, P-EN, P-EG) have nothing free. Two cues in turn, headings 4 and 12, a reset between them.

## What passes
The bump holds where it was put at both cues. Measured, not stored: on circuit seeds 1 to 40 the level passes on 40 of 40 at every step from 0.30 to 1.80 and at the reference value 1.03, and on 0 of 40 at 0.10, 0.15, 0.20 and 0.25 (the activity spreads: rates of 60 Hz and over). The window is one-sided: more inhibition lowers the rates but never kills the bump in this model, and above a scale of about 2 whether the bump holds depends on the seed, so the range stops at 1.8. Cues 4 and 12 are used because their resting places do not move with the gain (cue 11, for one, has a notch at 0.48 to 0.52). The other cell types have no window at all (E-PG: one edge only; P-EG and P-EN: none), see 4d. The verdict says the inhibition is too weak, too strong or out of range, in words.

## Reference
Delta7 scale 1.03 (off the stepper's 0.05 grid on purpose). Tests: `tasks_atlas_compass_test` (every step of the card on the seeds tried, tampering), `cheap_rows_h.inc`, `godot/tests/test_atlas.gd`.
