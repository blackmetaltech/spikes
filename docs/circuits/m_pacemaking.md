# Micro-puzzle: intrinsic pacemaking (`circuits/m_pacemaking.json`)

Task `m_pacemaking` (engine/src/tasks/m_pacemaking.cpp), concept 9, chapter heart, order 198 (before `m_coupling` at 199 and `heart_pacemaker` at 200). Uses: threshold.
(Concept 9 sits before concept 8 in the campaign: a player must know a cell can beat on its own before two such cells are coupled.)

## The puzzle
No sensors at all, ACh and NE held at 0. Wire a cell to `beat` so that it fires by itself, 50-140 times a minute (per cell), with beat intervals varying by at most 15 %, over 0.5-4 s.

## Reference (1 neuron)
```
N1 (sa_node_pacemaker) --> beat
```
103 beats per minute (interval 580 ms), CV 0.15 %.

## Why it works
Only an intrinsic oscillator passes. A plain `iaf_psc_alpha` cell is silent at 0 pA; with a bias current over its 375 pA rheobase it fires at 22 Hz or more (380 pA: 1320 beats per minute);
`izhikevich` with a bias of 4 pA fires at 429 per minute. The SA node cell fires at 103 per minute with no wiring. The lab opens a heart task on the SA card (its default part comes from the
reference circuit), so the first build usually passes.

## Novice path
| run | build | result |
|---|---|---|
| 1 | a plain cell on `beat` | "stays silent ... a pacemaker cell fires by itself" |
| 2 | the same with a 380 pA bias | "fires, but at 1320 per minute ... far too fast" |
| 3 | the SA node cell | pass |

**3 runs** for a player who starts with the plain cell; **1** with the lab's default part. Several uncoupled SA cells on `beat` fail on irregularity (merged trains).

## Modes
Rate 40-160 / 50-140 / 50-140 per minute; CV <= 25 % / 15 % / 10 %. There is no stimulus, so no noise.

## What breaks it
Cell silenced; the SA cell replaced by a plain integrate-and-fire cell.

Hints: (1) pacemaking is rhythmic firing with no input; (2) the SA node pacemaker card; (3) one SA cell on `beat`, nothing else.
Metric: `interval_cv_pct`.

Credits: DiFrancesco D (1993) Annu Rev Physiol 55:455; Jose AD, Collison D (1970) Cardiovasc Res 4:160.
