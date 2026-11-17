# Diagnosis case: dx_apnoea (`circuits/dx_apnoea.json`)

Task `dx_apnoea` (`engine/src/tasks/dx_apnoea.cpp`, chapter "breathing", order 340, diagnosis, off the spine). World: the `breathing` task (`breathing.md`: 5 % CO2 from 50 to 110 s). Generator: `python3 circuits/tools/gen_boss_dx.py dx_apnoea`.

## The case
A newborn breathes when awake but turns blue and stops in sleep; CO2 rises without any extra effort. In the circuit: resting rate 10 per minute, PaCO2 54 mmHg at rest, ventilation x1.21 under CO2 (peak PaCO2 87 mmHg); the rhythm is regular.

**Answer: `RTN>PBC+RTN>PHR`** (the chemoreceptor relay reaches neither the rhythm generator nor the phrenic neurons: congenital central hypoventilation syndrome (CCHS)).

## Candidates (seed 1)
| Candidate | Result |
|---|---|
| `RTN>PBC+RTN>PHR` (answer) | slow (10/min), high resting CO2 (54), ventilation x1.2 |
| `PBC>PBC` | rest 13/min and PaCO2 39 are normal but ventilation x1.05, PaCO2 peak 82: the bursters fire out of step (scope) |
| `PBC>EXP` | no breaths at rest (phrenic held), PaCO2 above 120 |
| `EXP>PHR` | tonic phrenic firing: 47/min, shallow, no ventilation |
(`PBC>PHR` passes the task, the circuit being redundant through RTN>PHR, so it is not offered.)

## Biology
PHOX2B mutations remove the CO2-sensitive drive to the brainstem network; patients hypoventilate in sleep and do not respond to rising CO2. Credits: Amiel J et al. (2003) Nat Genet 33:459; Weese-Mayer DE et al. (2010) Am J Respir Crit Care Med 181:626; Guyenet PG, Stornetta RL, Bayliss DA (2010) Respir Physiol Neurobiol 173:244.

## How it is built
The patient circuit is the reference circuit of the underlying task with the answer's projection(s) kept in the file at weight 0: every candidate is drawn and can be cut or probed in the lesion lab, and the dead wire is found from activity (a source that fires into a target that does not answer), not from a missing arrow. Cutting a working candidate changes the symptoms or the downstream rates; cutting the dead one (or anything after a silent group) changes nothing.
