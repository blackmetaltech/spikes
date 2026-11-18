# Reference circuit: boss_autonomic_exam (`circuits/boss_autonomic_exam.json`)

Task `boss_autonomic_exam` (`engine/src/tasks/boss_autonomic_exam.cpp`, chapter "autonomic", order 435, `level_kind` "boss", on the spine). A 120 s day: rest (0-15 s), stand up (15 s, stays upright), exercise (50-70 s), threat (85-100 s), recovery.
Bodies: Heart, Eye, Lungs; the CO2 production rises 3x in exercise, so the chemoreflex loop is closed. Generator: `python3 circuits/tools/gen_bosses.py boss_autonomic_exam`.

## Wiring (75 neurons)
```
baroreceptor -> AFF -> NTS -> NA (I_e 372) -> VG -> vagal (ACh);  NTS -> CVLM -| RVLM (I_e 380) -> IML -> SG -> sympathetic (NE)     [the baroreflex circuit, unchanged]
exercise_cue -> CC ; threat -> HYP :   CC, HYP -> RVLM (+60)   CC, HYP -> CVLM (-120)   CC, HYP -> NA (-150)   CC, HYP -> VRG (+60 / +100)
HYP -> SCG (+60) -> dilator ;  HYP -| EW (-150) ; EW -> CG -> sphincter (tonic parasympathetic tone, withdrawn by HYP)
chemo -> CHEMO -> VRG (+30) -> vent
SA (sa_node_pacemaker) -> sa_beat, with the NE / ACh modulation of baroreflex.json
```
Seeds 1-5 (standard): 19 of 19 conditions, value 100. HR 69 rest, 76 standing, 113 exercise, 115 threat, 76 recovery; MAP 93 / 89 / 104 / 106 / 88; pupil 4.2 / 4.2 / 4.2 / 7.5 / 4.1 mm; VE 7.2 / 7.1 / 13.8 / 16.4 / 7.0 L/min.

## Score
Five phases x four organs, 19 conditions (list in the task header), soft score per condition, pass when all hold. Modes: guided widens tolerances x1.25 (needed rises shrink), researcher tightens x0.85.

## What breaks it (tests, seeds 1 and 2)
| Lesion | Result |
|---|---|
| RVLM -> IML cut | no sympathetic tone: MAP 79 at rest, 66 standing, 59 exercise (13 of 19 conditions) |
| NTS -> CVLM cut | pressure unregulated: MAP 164 at rest (16 of 19) |
| HYP -> NA and CC -> NA cut | no vagal withdrawal: HR 74 in exercise and threat (17 of 19) |
| CC -> VRG and HYP -> VRG cut | ventilation flat (8 L/min in exercise, 17 of 19) |
Milder cuts that still pass: HYP -> SCG alone (EW withdrawal still dilates the pupil by 1.9 mm), CHEMO -> VRG alone (the feed-forward carries ventilation), HYP, CC -> CVLM (the baroreflex opposes the rise only weakly at these gains).

## Limits
Fixed luminance (no light reflex), a lumped sympathetic chain, no hypoxic drive, no skeletal muscle pump. Credits: Kirchheim HR (1976) Physiol Rev 56:100; Guyenet PG (2006) Nat Rev Neurosci 7:335; Raven PB, Fadel PJ, Ogoh S (2006) Exp Physiol 91:37 (doi:10.1113/expphysiol.2005.032250); Rowell LB, O'Leary DS (1990) J Appl Physiol 69:407; Dampney RAL (1994) Physiol Rev 74:323; Cannon WB (1915); Steinhauer SR, Hakerem G (1992) Ann N Y Acad Sci 658:182; Guyenet PG, Stornetta RL, Bayliss DA (2010) J Comp Neurol 518:3883.

## v0.4: mechanism check, quick check, negative tests
**Mechanism (Engine::structure()).** Every component of the chapter must be in the circuit: the baroreflex (both limbs, as in the `baroreflex` level), central command (the exercise cue and the threat each reach the cells behind `sympathetic` by excitation; a disinhibition through CVLM counts), the defence reaction (the threat reaches the cells behind `dilator` and behind `vent`, or the cue reaches `vent`), the chemoreflex (`chemo` reaches the cells behind `vent`) and separate pools for dilator, sphincter and vent. The verdict names the component; the value is capped at 49.

**Quick check.** `quick_duration_ms()` = 82 s: the same day compressed (stand up at 12 s, exercise 36-50 s, threat 56-68 s, windows rest 8-12 s, stand 28-34 s, exercise 43-50 s, threat 62-68 s, recovery 74-82 s, gas exchange clock 1.5 times faster), same 19 conditions. Reference: 7.8 s -> 5.4 s wall (under the 8 s target).

The verdicts and the tier 1-3 hints carry no parameter numbers (weights, biases, gains): those live in `recipe()` only (checked in the tests). Negative tests: `engine/tests/cheap_rows_c.inc` (run by `tasks_cheap_test`) and the mechanism cases in the level's own test file.
