# Micro-puzzle: adaptation (`circuits/m_adaptation.json`)

Task `m_adaptation` (engine/src/tasks/m_adaptation.cpp), concept 6, chapter invertebrates, order 28 (before `lamprey`). Uses: threshold.

## The puzzle
A dc step (`step`, level 3.0, 500-3500 ms) drives one cell. Its rate in the first 100 ms must be at least **3.5 times** its rate in the last second (2500-3500 ms),
at least 15 Hz at first and at least 3 Hz at the end.

## Reference (1 neuron)
```
step (dc, 300 pA/unit) --> N1 (aeif_cond_alpha, tau_w 400 ms, other parameters at the NEST default a 4 nS, b 80.5 pA) --> spikes
```
50 spikes/s at the start, 9 at the end: ratio 5.6 on every seed.

## Why it works
* A plain integrate-and-fire cell has ratio 1.0 (133 vs 135 Hz). The adaptive exponential cell as it comes (tau_w 144 ms) has 2.3 (50 vs 22): not enough.
  The late rate is set by the adaptation current w, which builds to b x tau_w x rate: a longer tau_w adapts more. tau_w 200: 2.9; 300: 4.5; 400: 5.6
  (the Adapting preset, a 0, b 60, tau_w 100, gives 1.4, and 4.0 at tau_w 400). A larger b also works but not alone (b 200 at tau_w 144 is still below 3.5).
* The lab's default dc weight (300 pA, 900 pA of current) fires the aeif cell; no weight change is needed.

## Novice path
| run | build | result |
|---|---|---|
| 1 | step > the aeif cell > spikes (iaf gives the same verdict) | "the rate falls only a little: ratio 2.3, needs 3.5 ... raise tau_w or b" |
| 2 | tau_w 400 ms | pass |

**2 runs.**

## Modes
Ratio needed 2.5 / 3.5 / 4.0 (guided / standard / researcher); noise x0 / x1 / x2. tau_w 200 passes in guided only.

## What breaks it
a = b = 0 (no adaptation, ratio 0.9); tau_w 30 ms (fades before it builds up); cell silenced.

Hints: (1) adaptation is a slow spike-triggered current; (2) use the aeif cell, its b and tau_w; (3) keep the weight, set tau_w to 400 ms.
Metric: `adaptation_ratio` (higher is better).

Credits: Connors BW, Gutnick MJ (1990) Trends Neurosci 13:99; Brette R, Gerstner W (2005) J Neurophysiol 94:3637.
