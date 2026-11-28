# Atlas level: a_signs (`circuits/a_signs.json`, given `circuits/a_signs_given.json`)

Task `a_signs` (`engine/src/tasks/a_signs.cpp`, shared world and measures in `atlas_compass.h`, chapter "a_compass", `level_kind` "atlas", order 995, spine). Generator: `python3 godot/circuits/tools/gen_atlas_compass.py`. Study: `research/atlas-a.md` 4c and 4d.

## The puzzle
The wiring of the fly compass (148 cells: E-PG 46, P-EN a 20, P-EN b 22, P-EG 18, Delta7 42; 9,487 measured connections of hemibrain v1.2.1, `data/connectome/hemibrain/compass.json`) is given and locked, with the fitted gains of `compass_setting.json` in the stored weights (weight = synapse count x one gain per cell-type pair, leaky integrate-and-fire cells, background current per type, delay 2 ms). The wiring does not say whether a connection excites or inhibits. Four cell-type cards: E-PG, P-EG and Delta7 have a free sign (given excitatory, which is wrong); P-EN has nothing free, its sign barely changes the result in this model (research/atlas-a.md 4c).

## The world (`atlas_compass.h`)
A run is a row of trials of 3,700 ms: a reset (the sensor `clear` holds a negative current on every cell, 0 to 200 ms), a cue (the Poisson sensor `cue<k>`, 300 Hz, on the E-PG cells of heading k-1, k and k+1, 200 to 700 ms), then nothing. The motors `h0` to `h15` read the E-PG cells of each heading (22.5 degrees). Five trials, at the headings 4, 11, 6, 15, 8 (resting places that hold on every seed, see 4d). The bump HOLDS where it was put when, over the last 500 ms, its population-vector angle is within 30 degrees of the cue, its strength is above 0.5, at least 0.7 of the spikes are within two headings of the peak and the mean rate per heading is 2 to 60 Hz (the definition of the study).

## What passes
The bump holds at every one of the five cues. Measured, not stored: on circuit seeds 1 to 40 the reference (E-PG +, P-EG +, Delta7 -) holds at all five cues on 40 of 40, and each of the seven other assignments of the three signs passes on 0 of 40 (research/atlas-a.md 4d). The verdict says what the bump did (it died out, it spread over the ring, it settled at another heading) and that at least one sign is wrong; it carries no number.

## Reference
E-PG and P-EG excitatory, Delta7 inhibitory. Tests: `tasks_atlas_compass_test` (digests, tampering, the eight assignments), `cheap_rows_h.inc` (cheap circuits), `godot/tests/test_atlas.gd` (through the game's engine).
