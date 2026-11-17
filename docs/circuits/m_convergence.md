# Micro-puzzle: convergence and coincidence (`circuits/m_convergence.json`)

Task `m_convergence` (engine/src/tasks/m_convergence.cpp), concept 3, chapter invertebrates, order 9 (just before `reflex`). Uses: threshold, excitation.

## The puzzle
Three dc sensors `in_a`, `in_b`, `in_c` (level 1.0 when on) come on in seven patterns of 300 ms: a alone; b+c; **a+b+c**; a, b, c one after the other (100 ms each, never
together); a+c; **a+b+c**; c alone. One cell must fire (>= 3 spikes) for both a+b+c patterns and never otherwise (0 stray spikes).

## Reference (1 neuron)
```
in_a, in_b, in_c (dc, 150 pA/unit each) --> N1 (iaf_psc_alpha) --> out
```
One input gives 150 pA, two 300 (under the 375 pA rheobase), three 450 (over it): the cell fires at about 50 Hz for 300 ms, first spike 17-18 ms after the onset.

## Why it works
With a weight x per input, only 125 < x < 187 pA separates three from two. Measured on 6 seeds: 120 never fires, 130-180 pass (180: 5 of 6), 187 and 190 fire for pairs.
The lab starts each sensor at 300 pA, so the first build fires on the pairs ("also fired for 'b + c'"). (A higher cell threshold, V_th -45 mV, also works at the default weight.)

## Novice path
| run | build | result |
|---|---|---|
| 1 | three sensors onto one cell, the lab's 300 pA | "also fired for 'b + c' ... lower the sensor weights until two inputs together are no longer enough" |
| 2 | all three at 150 | pass |

**2 runs.**

## Modes
Stray spikes allowed 2 / 0 / 0 (guided / standard / researcher); stimulus noise 4 % per ms and 2 % per episode, x0 / x1 / x2. The weight window is the same; noise narrows it.

## What breaks it
`in_a` cut (two inputs never reach threshold); all weights x2 (pairs fire); all weights x0.5 (three inputs stay under threshold).

Hints: (1) a neuron sums what converges on it; make each input weak, even two together; (2) the weights of the three sensors; (3) about 150 pA each.
Metric: `latency_ms` (mean time from the onset of an a+b+c pattern to the first spike).

Credits: Mendell LM, Henneman E (1971) J Neurophysiol 34:171 (a Ia fibre gives about 0.1 mV; a motoneuron needs about 10 mV).
