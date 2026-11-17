# Reference circuit: boss_spinal_sprint (`circuits/boss_spinal_sprint.json`)

Task `boss_spinal_sprint` (`engine/src/tasks/boss_spinal_sprint.cpp`, chapter "spinal", order 135, `level_kind` "boss", on the spine). The finale of the spinal chapter: the arm of Posture (gravity on, 1.0 kg forearm,
base angle 0.5 rad) must hold, stand firm against nine pushes (1.0-1.6 N m), make three reaches (+0.30, +0.40, +0.25 rad, go cue and target as in Reach) and hold the new angle under pushes, in 40 s.
Generator: `python3 circuits/tools/gen_bosses.py boss_spinal_sprint` (reuses `gen_spinal.py`).

## Wiring (80 neurons)
Posture circuit (Ia / II spindle reflex, reciprocal Ia inhibition, GAM gamma pool, TONIC descending tone: flexor 18.5, extensor 5, gamma 6) + Renshaw cells RC_F / RC_E (3 each, MN -> RC 3, RC -> MN -3, RC -> IN -3, RC <-> RC -3)
+ the Reach generator (GO, TGT, B1-B3 with the Reach weights) with a stronger hold command (TGT -> MN_F 14, TGT -> MN_E -16: heavier than Reach's 5.95 / -10 because the arm works against gravity).
Sensors `ag_Ia, ant_Ia, ag_II, ant_II, drive, go, target`; motors `ag_motor, ant_motor, gamma_motor` (extra outputs `burst1-3` for the scopes).

## Score (see the task header)
Hold: rms and worst error against the displayed target (not judged 900 ms after a go cue or after the display ends). Reach: time, overshoot, final error per reach (good: <= 800 ms, <= 65 %, <= 0.20 rad). Steadiness: no ringing after a push, no creep
before 'go'. Pass: rms <= 0.23, max <= 0.40, 3/3 good reaches, no ringing, creep <= 0.10. Limits are wider than Posture's because the base posture wanders +-0.15 rad between seeds (the balance of tone and reflex is steep, see `posture.md`) and the shared neurons
leave less margin. Modes: guided x1.25, researcher x0.85 on every tolerance.

## Design notes and dead ends
* Posture with a 1.5 kg arm and the Renshaw cells added sagged (rms 0.27-0.45): the extra inhibition takes flexor drive. A 1.0 kg forearm with the Posture tone balances.
* Reach's hold command (5.95 / -10) left the arm 0.14 rad short under gravity: final error failed on most seeds. 14 / -16 gives final error < 0.07 rad.
* As in `reach.md` the hold command alone brings the arm to the target quickly; the burst mainly shapes overshoot (cutting B2 -> MN raises overshoot to 0.6), so the bursts do not by themselves fail the task when cut at B1.

## What breaks it (tests in `engine/tests/tasks_boss_dx_test.cpp`, seeds 1 and 2)
| Lesion | Result |
|---|---|
| Ia, II -> MN cut (both pools) | arm rides up, rms 0.40, overshoot > 100 % |
| TONIC -> GAM cut | spindles slack: rms 0.29, overshoot 82 % |
| TGT -> MN_F, MN_E cut (no hold command) | final error 0.44-0.6 rad, no reach good |
| TONIC -> MN_F cut | the arm falls: rms 0.9 |
Also failing on seed 2 but borderline (not in the test): MN -> RC cut (overshoot 55 %, rms 0.19), RC -> MN cut (overshoot 72 %, rms 0.23), B2 -> MN cut (overshoot 62 %).

Credits: Matthews PBC (1972); Nichols TR, Houk JC (1976) J Neurophysiol 39:119; Hultborn H (2006) Prog Neurobiol 78:215; Hallett M, Shahani BT, Young RR (1975) J Neurol Neurosurg Psychiatry 38:1154; Feldman AG (1986) J Mot Behav 18:17.
