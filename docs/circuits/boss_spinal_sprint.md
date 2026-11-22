# Reference circuit: boss_spinal_sprint (`circuits/boss_spinal_sprint.json`)

Task `boss_spinal_sprint` (`engine/src/tasks/boss_spinal_sprint.cpp`, chapter "spinal", order 135, `level_kind` "boss", on the spine). The finale of the spinal chapter: the arm of Posture (gravity on, 1.0 kg forearm,
base angle 0.5 rad) must hold, stand firm against nine pushes (1.0-1.6 N m), make three reaches (+0.30, +0.40, +0.25 rad, go cue and target as in Reach) and hold the new angle under pushes, in 40 s.
Generator: `python3 circuits/tools/gen_bosses.py boss_spinal_sprint` (reuses `gen_spinal.py`).

## Wiring (80 neurons)
Posture circuit (Ia / II spindle reflex, reciprocal Ia inhibition, GAM gamma pool, TONIC descending tone: flexor 17.9, extensor 5, gamma 5.5) + Renshaw cells RC_F / RC_E (3 each, MN -> RC 2.2, RC -> MN -3.5, RC -> IN -3, RC <-> RC -3)
+ the Reach generator (GO, TGT, B1-B3) with a stronger hold command (TGT -> MN_F 21.7, TGT -> MN_E -21.7: heavier than Reach's 5.95 / -10 because the arm works against gravity). The weights are the ones of `gen_bosses.py` (see 'Robustness' below for how they were set).
Sensors `ag_Ia, ant_Ia, ag_II, ant_II, drive, go, target`; motors `ag_motor, ant_motor, gamma_motor` (extra outputs `burst1-3` for the scopes).

## Score (see the task header)
Hold: rms and worst error against the displayed target (not judged 900 ms after a go cue or after the display ends). Reach: time, overshoot, final error per reach (good: <= 800 ms, <= 65 %, <= 0.20 rad). Steadiness: no ringing after a push, no creep
before 'go'. Pass: rms <= 0.23, max <= 0.40, 3/3 good reaches, no ringing, creep <= 0.10. Limits are wider than Posture's because the base posture wanders +-0.15 rad between seeds (the balance of tone and reflex is steep, see `posture.md`) and the shared neurons
leave less margin. Modes: guided x1.25, researcher x0.85 on every tolerance.

## Design notes and dead ends
* Posture with a 1.5 kg arm and the Renshaw cells added sagged (rms 0.27-0.45): the extra inhibition takes flexor drive. A 1.0 kg forearm with the Posture tone balances.
* Reach's hold command (5.95 / -10) left the arm 0.14 rad short under gravity: final error failed on most seeds. 14 / -16 gave final error < 0.07 rad on most seeds (the v0.5 reference used 16.1 / -16; it is now 21.7 / -21.7, see 'Robustness').
* As in `reach.md` the hold command alone brings the arm to the target quickly; the burst mainly shapes overshoot (cutting B2 -> MN raises overshoot to 0.6), so the bursts do not by themselves fail the task when cut at B1.

## Robustness (after 0.5.0): why the old reference failed one seed in five, and what was done
**Cause, per criterion and seed** (the old reference, standard mode, full episode, seeds 1-30 and 101-105: 30 of 35 passed; researcher 29 of 35). Every failure but one (seed 10, researcher: hold rms 0.23) was the *reach* criterion, not the hold or the ringing: the slowest of the three reaches took 1.3 to 1.9 s (limit 0.8 s standard, 0.68 s researcher) while the median reach takes 0.2 s. The slow reach was the first (+0.30 rad) or the third (+0.25 rad, the smallest burst): the arm starts from a base posture that wanders from 0.31 to 0.63 rad between seeds (the pools fire in lumpy waves and the posture has only a weak restoring stiffness; the task's header documents the +-0.15 rad), so from a low start the burst falls short of the band and the hold command then pulls the arm up slowly. Nothing in the estimator was wrong: reach time, overshoot and final error are measured as written. The reference was tuned to the edge: its worst case sat at the limit.
**What was changed (the reference only; no criterion, limit or estimator).** The weights of `gen_bosses.py sprint()` were re-fitted by a hill-climb (random three-weight moves, accept when better) on 24 training seeds (1-24), full episode, researcher limits (the strictest), with a smooth objective: the 90th percentile over seeds of the largest ratio (measured / limit) across reach time, overshoot, final error, hold rms, hold maximum, creep and ringing, plus 0.3 of the mean ratio. The hold command went from 16.1 / -16 to 21.7 / -21.7, the tone, the Renshaw and burst weights moved by 0 to 30 percent (see the diff of `gen_bosses.py`). An earlier stage on the quick episode alone reached 29 of 30 on its training seeds and 32 of 35 in the full episode (researcher), which is why the final stage trains on full episodes.
**Result (reference, `tasks_heldout_test`):** seeds 1-30 and 101-105, guided, standard and researcher, quick and full: 35 of 35 in all six combinations (seeds 25-30 and 101-105 were not used in the fit). Seeds 31-90, never used: standard 60/60 quick and 58/60 full, researcher 58/60 quick and 54/60 full (90 percent: the researcher reach limit of 0.68 s is still within the reference's tail, seeds 37, 40, 42, 67, 84, 88).

## What breaks it (tests in `engine/tests/tasks_boss_dx_test.cpp`, seeds 1 and 2)
| Lesion | Result |
|---|---|
| Ia, II -> MN cut (both pools) | arm rides up, rms 0.40, overshoot > 100 % |
| TONIC -> GAM cut | spindles slack: rms 0.29, overshoot 82 % |
| TGT -> MN_F, MN_E cut (no hold command) | final error 0.44-0.6 rad, no reach good |
| TONIC -> MN_F cut | the arm falls: rms 0.9 |
Also failing on seed 2 but borderline (not in the test): MN -> RC cut (overshoot 55 %, rms 0.19), RC -> MN cut (overshoot 72 %, rms 0.23), B2 -> MN cut (overshoot 62 %).

Credits: Matthews PBC (1972); Nichols TR, Houk JC (1976) J Neurophysiol 39:119; Hultborn H (2006) Prog Neurobiol 78:215; Hallett M, Shahani BT, Young RR (1975) J Neurol Neurosurg Psychiatry 38:1154; Feldman AG (1986) J Mot Behav 18:17.

## v0.4: the honesty contract (docs/REQ-0.4.md)
- **Mechanism check.** Before the behaviour, the task reads the circuit structure (`Engine::structure()`, captured when the run starts) and requires all of its component mechanisms: spindle feedback and fusimotor drive (posture), the monosynaptic reflex with reciprocal inhibition, a Renshaw loop, and the agonist and antagonist bursts. A circuit that lacks it fails with a verdict naming the missing mechanism, and its value is capped at 40.
- **No numbers in verdicts or hint tiers 1-3.** Failed verdicts and the three hint tiers describe the observed behaviour and the direction to change; the numbers are only in `recipe()`. Pass messages still report observed values.
- **Negative tests.** `engine/tests/cheap_rows_b.inc` (run by `tasks_cheap_test`): empty bench, one cell with the lab's first-connect weights, one tonic cell with every weight 0, the reference with every weight 0, the reference with every weight at an extreme, and the reference minus the taught mechanism, each must fail with the mechanism named.
- **Non-circular novice.** Brief and lab defaults only, at most 8 runs: not attempted by the scripted novice: it composes the four levels above, two of which the novice does not solve.
- **Speed.** Quick check (14.25 s instead of 40 s): posture, the first two reaches and the pushes up to the one during the second reach's hold; the same rules (a full run judges three reaches and nine pushes).
