# Micro-puzzle: coupling and synchrony (`circuits/m_coupling.json`)

Task `m_coupling` (engine/src/tasks/m_coupling.cpp), concept 8, chapter heart, order 199 (just before `heart_pacemaker`). Uses: pacemaking, excitation.

## The puzzle
Two SA-node cells, `a` and `b`, with ACh = NE = 0. The task sets the membrane capacitance of the cell on `a` to 28.8 pF and of the cell on `b` to 35.2 pF (-10 % and +10 % of 32 pF), so
alone they beat every 580 ms and 635 ms. Over 1.5-5 s they must beat one for one, with the delay between a and b changing by at most 12 % of a beat (and the lag below 30 %).

## Reference (2 neurons)
```
a-cell <--2 nS--> b-cell        (sa_node_pacemaker, all-to-all excitatory both ways, delay 2 ms)      a-cell --> a      b-cell --> b
```
Both beat every 580 ms (103 per minute), drift 1 ms, b lags 42 ms.

## Why it works
Excitatory synapses stand in for the node's gap junctions (as in `heart_pacemaker`). Uncoupled: the delay sweeps a whole beat in 5 s. The lab's default 0.2 nS pulls a little but does not lock
("Not locked: the beats slip past each other"); 0.5 nS passes 5 of 6 seeds, **1, 2 and 4 nS pass 6 of 6** (lag 61, 42, 29 ms). The weight slider tops at 4 nS. One link in one direction is enough.

## Novice path
| run | build | result |
|---|---|---|
| 1 | two SA cells linked both ways at the default 0.2 nS | "not locked: the beats slip past each other ... a stronger weight" |
| 2 | 2 nS | pass |

**2 runs.**

## Modes
Drift <= 20 % / 12 % / 8 % of a beat; the lag limit (30 %) is the same. No stimulus noise.

## What breaks it
Both links cut; both links x0.1 (the default 0.2 nS).

Hints: (1) oscillators of different speed lock when each pulls the other; (2) two SA cells, one on `a`, one on `b`, linked both ways, look at the weight; (3) about 2 nS.
Metric: `drift_ms`.

Credits: Jalife J (1984) J Physiol 356:221; Michaels DC, Matyas EP, Jalife J (1987) Circ Res 61:704.

## Default part and recipe (v0.3 fix round)
* **`default_model()`:** `sa_node_pacemaker` - the card the lab's parts picker opens this level on: the cell type the reference circuit and every verdict assume.
* **`recipe()`:** N1 (SA node cell) > 'a' and N2 (SA node cell) > 'b'. Link N1 > N2 and N2 > N1 (both excitatory) and raise their weight from 0.2 nS to about 2 nS.
  The brief keeps the concept; the numbers live here, so the lab can show them by mode (Guided inline, Standard behind SHOW RECIPE at the cost of the third star, Researcher never).
* **Novice path with the lab's defaults** (`tasks_micro_a_test`): first build made of the default part and the default weights, then one change per verdict: **2 runs** (gate: at most 8).

## Honesty contract (v0.4)
Mechanism check: a projection between the two pacemaker cells (either direction), no bias currents, two different cells. Cheap rows: uncoupled, default weight, one cell on a and b, plain cells coupled.
