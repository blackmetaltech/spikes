# Diagnosis case: dx_achalasia (`circuits/dx_achalasia.json`)

Task `dx_achalasia` (`engine/src/tasks/dx_achalasia.cpp`, chapter "autonomic", order 441, diagnosis, off the spine). World: the `peristalsis` task (`peristalsis.md`). The Hirschsprung-like alternative (an aganglionic segment) cannot be built from the one-neuron-per-segment gut, so the case is the achalasia-like loss of the inhibitory limb. Generator: `python3 circuits/tools/gen_boss_dx.py dx_achalasia`.

## The case
A man whose swallowed food sticks. In the circuit: the bolus moves but arrives at 34.9 s (normal 14-16 s), the segments ahead of it contract instead of relaxing, the inhibitory motoneurons (`no0..7`) never fire, the segments sense the bolus and the slow waves are present.

**Answer: `DIN>IMN`** (descending inhibitory interneurons no longer drive the inhibitory motoneurons).

## Candidates (seed 1)
| Candidate | Result |
|---|---|
| `DIN>IMN` (answer) | arrival 34.9 s, no NO/VIP spikes, fails the 25 s limit |
| `S>AIN+S>DIN` | bolus stalls at segment 2.5: no reflex |
| `AIN>EMN` | arrival 16.1 s: passes (tone and slow waves still carry the bolus) |
| `ICCP>ICC+ICC>ICC` | no slow waves: 6.4 of 7 segments, inhibitory neurons still active (1000 spikes) |

## Biology
In achalasia the nitric-oxide/VIP inhibitory neurons of the oesophageal myenteric plexus are lost, so the sphincter and the segments ahead of a bolus do not relax. Credits: Goyal RK, Chaudhury A (2010) Gastroenterology 139:1086; Pandolfino JE, Gawron AJ (2015) JAMA 313:1841; Bayliss WM, Starling EH (1899) J Physiol 24:99.

## How it is built
The patient circuit is the reference circuit of the underlying task with the answer's projection(s) kept in the file at weight 0: every candidate is drawn and can be cut or probed in the lesion lab, and the dead wire is found from activity (a source that fires into a target that does not answer), not from a missing arrow. Cutting a working candidate changes the symptoms or the downstream rates; cutting the dead one (or anything after a silent group) changes nothing.

## v0.4: the CHECK
The diagnosis is decided by the submitted answer (`set_variant`), never by the run, so its quick episode is the underlying task's quick episode, forwarded (`DxTask::quick_duration_ms()`), cut at 8 s: the run only feeds the patient's findings (`run_*` parts). Nothing submitted never passes, every wrong candidate fails (`tasks_boss_dx_test`), and `scripts/smoke.sh` no longer reads the answer from the C++ source: it takes the two candidates the tier-3 hint names from the case's own output and checks that exactly one of them passes.
