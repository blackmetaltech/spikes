# Diagnosis case: dx_clonus (`circuits/dx_clonus.json`)

Task `dx_clonus` (`engine/src/tasks/dx_clonus.cpp`, chapter "spinal", order 140, `level_kind` "diagnosis", off the spine). The world is the ordinary `renshaw` task (`renshaw.md`);
the player is given a motor-pool circuit with one missing connection, probes it, and names the lesion. Generator: `python3 circuits/tools/gen_boss_dx.py dx_clonus`.

## The case
A man with a cervical spinal cord injury has an arm that trembles at about 10 Hz and muscles that "run away" with small stretches. In the circuit: the force wobble is not damped, the
motor pool fires at about 48 impulses/s per cell instead of about 30, the stretch reflex is intact.

**Answer: `MN_F>RC_F+MN_E>RC_E`** (the motoneurons no longer excite their Renshaw cells). The probe that tells it from the look-alike `RC>MN` is the Renshaw cells' own scope: silent here, firing at about 140 Hz when only their output is cut.

## Candidates and what each would show (seed 1 on the reference circuit with that lesion)
| Candidate | World result | Renshaw cells |
|---|---|---|
| `MN>RC` (answer) | pool 48 Hz, improvement 0.14, reflex resistance 0.29, fails | silent (0 Hz) |
| `RC>MN` | pool 58 Hz (saturated), improvement 0, resistance 0.27, fails | fire at about 140 Hz |
| `Ia>MN` | pool 20 Hz, tremor still braked (improvement 0.45), passes | 52 Hz |
| `Ia>IN` | pool 35 Hz, improvement 0.74, passes | 77 Hz |
| `DESC>MN` | tremor smaller (0.7 %, improvement 0.89): the 10 Hz comes from that input | 50 Hz |
| `RC>IN` | improvement 0.55, passes | 65 Hz |

## Biology and limits
Renshaw cells damp the 8-12 Hz coupling between descending drive and muscle (Williams & Baker 2009); reduced recurrent inhibition has been proposed as one contributor to spasticity (Hultborn 2006; Pierrot-Deseilligny & Burke 2005). This case is a model, not a clinical entity: isolated loss of Renshaw excitation is not a recognised cause of any disease. The game's
patient shows too much tremor and an over-driven pool, not clonus itself (the model limb does not ring); the case is about finding the missing brake. Real clonus is a 5-8 Hz stretch-evoked oscillation from a hyperexcitable stretch reflex loop after upper motor neuron injury (Hidler & Rymer 1999 model it as reflex instability). Credits: Williams ER, Baker SN (2009) J Neurosci 29:6616; Hultborn H (2006) Prog Neurobiol 78:215; Pierrot-Deseilligny E, Burke D (2005) *The Circuitry of the Human Spinal Cord*; Hidler JM, Rymer WZ (1999) IEEE Trans Rehabil Eng 7:327 (reflex instability in spasticity).

## How it is built
The patient circuit is the reference circuit of the underlying task with the answer's projection(s) kept in the file at weight 0: every candidate is drawn and can be cut or probed in the lesion lab, and the dead wire is found from activity (a source that fires into a target that does not answer), not from a missing arrow. Cutting a working candidate changes the symptoms or the downstream rates; cutting the dead one (or anything after a silent group) changes nothing.

## v0.4: the CHECK
The diagnosis is decided by the submitted answer (`set_variant`), never by the run, so its quick episode is the underlying task's quick episode, forwarded (`DxTask::quick_duration_ms()`), cut at 8 s: the run only feeds the patient's findings (`run_*` parts). Nothing submitted never passes, every wrong candidate fails (`tasks_boss_dx_test`), and `scripts/smoke.sh` no longer reads the answer from the C++ source: it takes the two candidates the tier-3 hint names from the case's own output and checks that exactly one of them passes.
