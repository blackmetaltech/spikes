# Atlas level: a_path (`circuits/a_path.json`, given `circuits/a_path_given.json`)

Task `a_path` (`engine/src/tasks/a_path.cpp`, shared world and measures in `atlas_worm.h`, chapter "a_worm", `level_kind` "atlas", order 991, spine). Generator: `python3 godot/circuits/tools/gen_atlas_worm.py`. Study that fixed the model point: `research/atlas-a.md` 4b.

## The puzzle
The path finder gives the shortest route from a touch on the head to the backward motor neurons. Test it: silence every interneuron class that is not on your route and run the touch. The touch circuit is the measured wiring of 54 cells (`data/connectome/worm/touch.json`, Cook et al. 2019) with the synapse floor, signs and weights of the model point: every synapse excitatory, weight = synapse count x 15.5 pA, no electrical coupling, background current 170 pA, excitatory time constant 2 ms. Wiring locked; the five classes AVA, AVB, AVD, AVE, PVC are cell types with a free scale from 0 (silenced) to 1 (kept), the sign fixed (`"off": true` on the group lets the scale reach 0, see CIRCUIT-FORMAT.md).

## What is checked (mechanism)
The classes kept (scale exactly 1) are exactly the interneuron classes of ONE shortest route at class level (`analysis::shortest_path` and `on_shortest_paths` on the given graph; the shortest routes are two hops long, through AVB, AVD or AVE), and every other class is silenced (scale exactly 0). Not on a route, two routes kept, nothing silenced, everything silenced and a class half on each fail with a verdict that says why. The run must have been made.

## What the verdict says
What the measured motor rates show, in words: the worm backs up, or it does not. In this model no route kept alone carries the response (the backward motor neurons stay silent on circuit seeds 1 to 3 and 101 to 105 for each of the three routes), so the level passes with the verdict "a route on paper is not yet a pathway that works". Every statement is about the model; the notebook cites Chalfie et al. 1985 for what the animal's touch circuit was found to contain.

## Reference
AVD kept, AVA, AVB, AVE, PVC silenced. Tests: `tasks_atlas_worm_test` (references in three modes, quick and full, seeds 101 to 105; each route alone; tampering), `cheap_rows_h.inc` (cheap circuits).
