# Micro-puzzle: excitation, weight and delay (`circuits/m_excitation.json`)

Task `m_excitation` (engine/src/tasks/m_excitation.cpp), concept 2, chapter invertebrates, order 8. Uses: threshold.

## The puzzle
Five 14 ms taps (dc sensor `tap`, level 2.0) make a first cell fire one or two spikes each (read on `source`). A second cell must repeat every source spike
**8 ms later** (8 +- 1.5 ms mean and per spike; read on `echo`), at least 90 % of them, with at most one echo that has no source spike before it. The task
steps the engine in 0.1 ms slices inside every 1 ms task step, so latencies are measured to 0.1 ms.

## Reference (2 neurons)
```
tap (dc, 300 pA/unit) --> A --> source        A --1500 pA, delay 4.3 ms--> B --> echo        (A, B: iaf_psc_alpha)
```
Measured echo latency 7.9 ms on every seed.

## Why it works
* One spike through the lab's default 80 pA synapse is a 1.3 mV nudge, the cell needs 15 mV: nothing fires below about 1200 pA (900 and 1100 give no echo).
* Latency = synaptic delay + the time the PSP needs to reach threshold (alpha current, 2 ms rise; 3.4 ms at 1500 pA). With the default 2 ms delay the
  echo comes 5.6 ms after the source at 1500 pA (6.0 at 1400, 5.4 at 1600): too early for 8 +- 1.5, so the delay slider has to add about 2.3 ms.
  Delays 2.8-5.8 ms pass at 1500 pA; 6.2 does not. (Near 1200 pA the PSP is slow and the latency is 7.4 ms already: the weight alone passes.)

## Novice path
| run | build | result |
|---|---|---|
| 1 | tap > A > B at the default 80 pA | "echo never fired ... raise the weight a lot" |
| 2 | weight 400 | same |
| 3 | weight 1500 (top of the slider) | "echo too early: 5.6 ms ... lengthen the delay by about 2.4 ms" |
| 4 | delay 4.3 ms | pass |

**4 runs** without hints; 2 with the tier-3 hint (weight about 1500 pA, delay 4-5 ms).

## Modes
Latency band 8 +- 2.25 ms (guided, echoes needed 80 %, extras <= 3), +- 1.5 (standard), +- 1.05 (researcher); tap level noise 4 % per ms and 2 % per episode (x2 researcher).

## What breaks it
Link cut; link weight x0.2; delay back to the default 2 ms (latency 5.6 ms).

Hints: (1) weight decides whether a spike gets through, delay when; (2) the link into the `echo` cell; (3) 1500 pA and about 4-5 ms.
Metric: `latency_error_ms` (|mean latency - 8|, lower is better).

Credits: Katz B, Miledi R (1965) Proc R Soc B 161:483 (synaptic delay).

## Default part and recipe (v0.3 fix round)
* **`default_model()`:** `iaf_psc_alpha` - the card the lab's parts picker opens this level on: the cell type the reference circuit and every verdict assume.
* **`recipe()`:** tap > cell A (the default weights work) > cell B. A is wired to 'source', B to 'echo'. Raise the A > B weight to about 1500 pA so that one spike fires B, then lengthen the delay to about 4 or 5 ms (it starts at 2) until the echo latency reads 8 ms.
  The brief keeps the concept; the numbers live here, so the lab can show them by mode (Guided inline, Standard behind SHOW RECIPE at the cost of the third star, Researcher never).
* **Novice path with the lab's defaults** (`tasks_micro_a_test`): first build made of the default part and the default weights, then one change per verdict: **4 runs** (gate: at most 8).
