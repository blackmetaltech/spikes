# Diagnosis case: dx_vagal (`circuits/dx_vagal.json`)

Task `dx_vagal` (`engine/src/tasks/dx_vagal.cpp`, chapter "heart", order 240, diagnosis, off the spine). World: the `heart_autonomic` task (`heart_autonomic.md`: rest 65 bpm, exercise 120, recovery). Generator: `python3 circuits/tools/gen_boss_dx.py dx_vagal`.

## The case
A woman has a resting pulse of about 100 per minute that nothing slows, almost no beat-to-beat variability, an exercise rate of about 130 and no fall below 100 afterwards; atropine changes nothing.
In the circuit: `ach_rest` 0, rest HR 99, `rest_rmssd_ms` 0.5, peak 130.

**Answer: `NA>VG`** (the nucleus ambiguus no longer excites the cardiac ganglion cells: vagotomy-like).

## Candidates (seed 1, lesion applied to the reference)
| Candidate | Result |
|---|---|
| `NA>VG` (answer) | rest 99 bpm, `ach_rest` 0: resting tachycardia, value 0 |
| `IML>SG` | rest 65, exercise only 99 (no sympathetic), value 27 |
| `CC>IML` | rest 65, exercise 107 |
| `CC>NA` | rest 65, exercise 102 (no vagal withdrawal) |
| `HIGH>NA` | rest 65, exercise 128: schedule tracked within 4 bpm (a distractor that does little) |

## Biology
The sinus node's own rate is about 100/min and vagal tone slows it to 60-70; blocking or cutting the vagus raises the resting rate to the intrinsic rate (Jose & Collison 1970), as in the denervated transplanted heart, which rests at 90-100.
Credits: Jose AD, Collison D (1970) Cardiovasc Res 4:160; Robinson BF et al. (1966) Circ Res 19:400; Task Force (1996) Circulation 93:1043.

## How it is built
The patient circuit is the reference circuit of the underlying task with the answer's projection(s) kept in the file at weight 0: every candidate is drawn and can be cut or probed in the lesion lab, and the dead wire is found from activity (a source that fires into a target that does not answer), not from a missing arrow. Cutting a working candidate changes the symptoms or the downstream rates; cutting the dead one (or anything after a silent group) changes nothing.

## v0.4: the CHECK
The diagnosis is decided by the submitted answer (`set_variant`), never by the run, so its quick episode is the underlying task's quick episode, forwarded (`DxTask::quick_duration_ms()`), cut at 8 s: the run only feeds the patient's findings (`run_*` parts). Nothing submitted never passes, every wrong candidate fails (`tasks_boss_dx_test`), and `scripts/smoke.sh` no longer reads the answer from the C++ source: it takes the two candidates the tier-3 hint names from the case's own output and checks that exactly one of them passes.
