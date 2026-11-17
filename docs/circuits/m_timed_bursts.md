# Reference circuit: Timed bursts (pattern generator) (`circuits/m_timed_bursts.json`)

Micro-puzzle `m_timed_bursts` (`engine/src/tasks/m_timed_bursts.cpp`, chapter "spinal", order 129), concept 16 Timed bursts, placed just before `reach (130)`
(docs/DESIGN-0.3.md section 3). A fish-tank puzzle: one new idea, a 1-12 neuron circuit, a 2-5 s episode, a dashboard with the readouts above.
Generator: `python3 circuits/tools/gen_micro_b.py m_timed_bursts`.

## World
3.4 s. Two trials: `go` (dc, level 3.0 = 900 pA for 50 ms) at 600 + J ms and again at 2000 + J ms. Three motor pools `b1`, `b2`, `b3` stand for agonist, antagonist, agonist. A burst of a pool is the first spike of a sliding 20 ms window holding at least max(2, ceil(0.35 N)) spikes (N cells of the pool); 60 ms of quiet separates two bursts.

## Reference circuit
```
go --> G1 (4 cells) --(300 pA, delay 120 ms)--> G2 (4) --(300 pA, delay 120 ms)--> G3 (4)
G1, G2, G3 --> b1, b2, b3
```
12 neurons. Cells stop firing when their input stops, so every burst ends by itself. Generator: `python3 circuits/tools/gen_micro_b.py m_timed_bursts`.

## Score
| criterion | limit |
|---|---|
| quiet | <= 2 spikes in all pools in 150-500 ms |
| all three | every pool bursts in both trials |
| single | exactly one burst per pool per trial |
| order | b1 starts within [-10, 120] ms of go, b1 < b2 < b3 |
| rhythm | b2 - b1 and b3 - b2 in [100, 220] ms (guided [75, 295], researcher [115, 190]) |

Value 100 when all five hold, else 85 x the credit. The quality metric is the mean distance of the two gaps from 150 ms.

## Measured (standard mode, reference circuit)
| Seed | gap b1-b2 ms | gap b2-b3 ms | b1 latency ms | gap b1-b2 trial 2 | Score |
|---|---|---|---|---|---|
| 1 | 125 | 126 | 7 | 125 | 100 |
| 2 | 125 | 126 | 7 | 125 | 100 |
| 3 | 125 | 126 | 7 | 125 | 100 |
| 4 | 125 | 126 | 7 | 125 | 100 |
| 5 | 125 | 126 | 7 | 125 | 100 |
| 6 | 125 | 126 | 7 | 125 | 100 |

All six seeds pass (`spike-run --task m_timed_bursts --seeds 1,2,3,4,5,6`). Guided mode: 3/3 seeds pass; researcher mode: 3/3.

## From an empty bench
Run 1: three groups in a chain at the default 80 pA and 2 ms: `b2 never burst`. Run 2: 300 pA links: all three burst, but 7-8 ms apart (verdict: lengthen the delays). Run 3: delay 120 ms on both links passes. The delay slider reaches 250 ms.

Lesions that must fail (tested in `engine/tests/tasks_micro_b_test.cpp`): Delays set back to 2 ms (the bursts coincide); the second link deleted (no third burst).

## Biology
Hallett M, Shahani BT, Young RR (1975) EMG analysis of stereotyped voluntary movements in man. J Neurol Neurosurg Psychiatry 38:1154-1162: fast, accurate elbow movements show a triphasic EMG, an agonist burst, an antagonist burst and a second agonist burst, with 100-200 ms between bursts, nearly identical from trial to trial; Wacholder K, Altenburger H (1926) on the same pattern. In the tank the delays stand for polysynaptic pathways.
