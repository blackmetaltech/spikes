# Diagnosis case: dx_orthostatic (`circuits/dx_orthostatic.json`)

Task `dx_orthostatic` (`engine/src/tasks/dx_orthostatic.cpp`, chapter "heart", order 241, diagnosis, off the spine). World: the `baroreflex` task (`baroreflex.md`: stand at 15 s, lie at 60 s, haemorrhage 80-110 s). Generator: `python3 circuits/tools/gen_boss_dx.py dx_orthostatic`.

## The case
A 68-year-old man nearly faints each time he stands. In the circuit: mean pressure about 68 mmHg supine, 56 mmHg on standing and after blood loss, with the baroreceptors reporting the fall normally and RVLM firing at about 19 Hz.

**Answer: `RVLM>IML`** (the vasomotor neurons no longer reach the spinal sympathetic neurons: neurogenic orthostatic hypotension).

## Candidates (seed 1)
| Candidate | Result |
|---|---|
| `RVLM>IML` (answer) | MAP 56-81, mean 68, NE 0, never in band |
| `AFF>NTS` | MAP 129-185 (mean 155), HR 122: high and unregulated |
| `NTS>CVLM` | MAP 121-171 (mean 144): high |
| `CVLM>RVLM` | identical to `NTS>CVLM` (the RVLM gets no inhibition either way): high |
| `NA>VG` | HR 109, MAP 84-100: pressure still regulated, passes |
(`IML>SG` is behaviourally the same as the answer and is left out of the list; so is `NTS>NA`, which behaves exactly like `NA>VG`.)

## Biology
Standing pools blood in the legs; a healthy baroreflex raises sympathetic tone within seconds. With the descending sympathetic outflow interrupted (autonomic failure, spinal cord lesions) pressure falls on standing (Freeman et al. 2011: a fall of 20 mmHg systolic or more).
Credits: Freeman R et al. (2011) Clin Auton Res 21:69; Guyenet PG (2006) Nat Rev Neurosci 7:335; Dampney RAL (1994) Physiol Rev 74:323. Limit: the model's heart rate still rises through vagal withdrawal, while real neurogenic OH typically has a fixed pulse.

## How it is built
The patient circuit is the reference circuit of the underlying task with the answer's projection(s) kept in the file at weight 0: every candidate is drawn and can be cut or probed in the lesion lab, and the dead wire is found from activity (a source that fires into a target that does not answer), not from a missing arrow. Cutting a working candidate changes the symptoms or the downstream rates; cutting the dead one (or anything after a silent group) changes nothing.

## v0.4: the CHECK
The diagnosis is decided by the submitted answer (`set_variant`), never by the run, so its quick episode is the underlying task's quick episode, forwarded (`DxTask::quick_duration_ms()`), cut at 8 s: the run only feeds the patient's findings (`run_*` parts). Nothing submitted never passes, every wrong candidate fails (`tasks_boss_dx_test`), and `scripts/smoke.sh` no longer reads the answer from the C++ source: it takes the two candidates the tier-3 hint names from the case's own output and checks that exactly one of them passes.
