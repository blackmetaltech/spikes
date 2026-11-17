# Micro-puzzle: subtraction (`circuits/m_subtraction.json`)

Task `m_subtraction` (engine/src/tasks/m_subtraction.cpp), concept 5, chapter invertebrates, order 18 (before `worm`). Uses: inhibition, convergence.

## The puzzle
Two dc sensors `plus` and `minus` step through seven 500 ms segments (plus, minus): (0,0) (3,0) (3,3) (0,3) (3,2) (2,2) (3,0). `diff` must fire (>= 5 spikes in the last 400 ms)
for (3,0), (3,2), (3,0) and stay quiet (<= 1) for the other four: out = plus minus minus.

## Reference (2 neurons)
```
plus (dc, 300 pA/unit) --> O --> diff        minus (dc, 300) --> B (inhibitory) --(-1100 pA)--> O
```
O fires 54 spikes in 400 ms for (3,0), 17 for (3,2), nothing when the inputs are equal.

## Why it works
Plus 3 gives O 900 pA; B (900 pA at minus 3) fires about 130 Hz and its inhibition must cancel O's drive below the 375 pA rheobase for (3,3) and (2,2) but not for (3,2).
Measured: -600 pA leaves 21 spikes at (3,3) (fail); **-800 to -1400 pA pass** (4 of 6 seeds at 800, 6 of 6 from 900); -1600 silences (3,2) too.

## Novice path
| run | build | result |
|---|---|---|
| 1 | plus > O > diff, minus > inhibitory cell > O at the default -80 pA | "fired when plus 3, minus 3 ... cut the firing by 7 %, needs about 14 times its present weight" |
| 2 | -1100 | pass |

**2 runs.**

## Modes
Quiet segments may hold <= 3 spikes (guided) / 1 / 1; noise x0 / x1 / x2.

## What breaks it
B > O cut; B > O x1.6 (the (3,2) segment goes quiet); plus cut.

Hints: (1) excitation from plus, inhibition from minus on one cell, and the inhibition about as strong as the excitation; (2) the weight of the last link; (3) about -1100 pA.
Metric: `fire_rate_hz` (mean rate in the firing segments).

Credits: Suzuki H et al. (2008) Nature 454:114 (ASEL / ASER, C. elegans).
