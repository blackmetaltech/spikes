# Micro-puzzle: inhibition (`circuits/m_inhibition.json`)

Task `m_inhibition` (engine/src/tasks/m_inhibition.cpp), concept 4, chapter invertebrates, order 17 (before `worm`). Uses: threshold, excitation.

## The puzzle
`drive` (dc, level 2.0) is on from 0.4 to 3.4 s, `stop` (level 2.0) from 1.4 to 2.4 s. The output must fire with the drive alone (>= 8 spikes in 600-1400 ms), stay quiet
during the stop (<= 2 spikes in 1650-2400 ms) and fire again after it (>= 8 in 2650-3400 ms). A sensor can only excite, so the stop needs an inhibitory cell.

## Reference (2 neurons)
```
drive (dc, 300 pA/unit) --> A --> out         stop (dc, 300) --> B (inhibitory) --(-1000 pA)--> A
```
A fires at about 80-90 Hz with the drive alone (600 pA), nothing during the stop, 68-72 spikes before and after.

## Why it works
B fires at about 85 Hz during the stop; its inhibitory currents must take A's 600 pA below the 375 pA rheobase. Measured rate of A during stop for a B > A weight
of -80 / -300 / -500 / -600 / -1000 pA: 83 / 60 / 43 / 28 / 0 Hz (from 90): the cut is roughly linear in the weight, so the verdict says "cut by X %: about 100 / X times the present weight".
-700 to -1600 pA pass on 6 of 6 seeds.

## Novice path
| run | build | result |
|---|---|---|
| 1 | stop wired through an excitatory cell onto A | "the stop made no difference ... a sensor only excites; send the stop through a second neuron set to inhibitory" |
| 2 | that cell set inhibitory, default weight -80 | "cut by 8 % ... about 12 times its present weight" |
| 3 | weight -1000 | pass |

**3 runs** (2 if the player already knows to make the cell inhibitory).

## Modes
During-stop spikes allowed 4 (guided) / 2 / 2; noise on both levels x0 / x1 / x2.

## What breaks it
B > A cut; B > A x0.1 (cut by 11 %); B silenced.

Hints: (1) inhibition lowers the membrane; a sensor can only excite, so use an inhibitory neuron of your own; (2) wire stop to a second neuron (E / I switch) and that to the output cell; (3) -1000 pA.
Metric: `during_spikes`.

Credits: Hartline HK, Wagner HG, Ratliff F (1956) J Gen Physiol 39:651 (lateral inhibition in Limulus).
