# Micro-puzzle: neuromodulation (`circuits/m_neuromodulation.json`)

Task `m_neuromodulation` (engine/src/tasks/m_neuromodulation.cpp), concept 10, chapter heart, order 209 (before `heart_autonomic` at 210). Uses: pacemaking, excitation.

## The puzzle
A dc `vagus` signal (level 2.0) comes on at 1.7 s in a 5 s episode. The SA pacemaker cell on `beat` must slow: its mean beat interval over 3-5 s must be at least 1.25 times its
interval over 0.1-1.7 s (and not stop: >= 2 beats, ratio <= 3). The slowing has to come through ACh: the highest ACh level of the run must be >= 0.15, so a synapse onto the SA cell does not count.
The `vagal` output is a modulator channel (ACh, gain 0.012 per spike, tau 500 ms), pre-set by `channel_kinds()`.

## Reference (2 neurons)
```
vagus (dc, 300 pA/unit) --> V (iaf_psc_alpha) --> vagal (releases ACh)          S (sa_node_pacemaker) --> beat
```
V fires 84 Hz, so ACh settles at gain x rate x tau = 0.012 x 84 x 0.5 = 0.51 and the heart slows from 98 to 67 per minute (interval x1.46). The SA cell follows ACh by the engine's default
modulation, with no wiring and no `modulation` table.

## Why it works
Four cells on `vagal` give ACh 2.0 and 53 per minute (x1.88): still a pass. The lab's default weights suffice, so the only thing to find is that the modulator output, not a synapse, carries the message.

## Novice path
| run | build | result |
|---|---|---|
| 1 | vagus > cell, SA cell on `beat`, `vagal` not wired | "Not wired: motor 'vagal' is not connected to any neuron" |
| 2 | `vagal` wired to the vagus cell | pass |

**2 runs** (1 if the player wires the vagal output at once).

## Modes
Slowing needed x1.15 / x1.25 / x1.35; noise on the vagus level x0 / x1 / x2.

## What breaks it
Vagus cell silenced; vagus cut; ACh gain x0.05 (the level stays at 0.03).

Hints: (1) a neuromodulator changes how cells behave over a region, not one spike to one cell; (2) the `vagal` output; (3) the full wiring and the numbers.
Metric: `slowing_ratio` (higher is better).

Credits: Loewi O (1921) Pflugers Arch 189:239 (Vagusstoff).

## Default part and recipe (v0.3 fix round)
* **`default_model()`:** `iaf_psc_alpha` - the card the lab's parts picker opens this level on: the cell type the reference circuit and every verdict assume.
* **`recipe()`:** vagus > cell V (an ordinary integrate-and-fire cell, the default sensor weight is right) > 'vagal'; an SA node cell > 'beat'. V fires about 85 times a second, the vagal output (gain 0.012, tau 500 ms) turns that into an ACh level near 0.5, and the heart slows from about 103 to 67 per minute. More cells on 'vagal' means more ACh: too many stop the node.
  The brief keeps the concept; the numbers live here, so the lab can show them by mode (Guided inline, Standard behind SHOW RECIPE at the cost of the third star, Researcher never).
* **Novice path with the lab's defaults** (`tasks_micro_a_test`): first build made of the default part and the default weights, then one change per verdict: **2 runs** (gate: at most 8).
