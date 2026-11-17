# Diagnosis case: dx_apnoea (`circuits/dx_apnoea.json`)

Task `dx_apnoea` (`engine/src/tasks/dx_apnoea.cpp`, chapter "breathing", order 340, diagnosis, off the spine). World: the `breathing` task (`breathing.md`: 5 % CO2 from 50 to 110 s). Generator: `python3 circuits/tools/gen_boss_dx.py dx_apnoea`.

## The case
A newborn breathes when awake but turns blue and stops in sleep; CO2 rises without any extra effort. In the circuit: resting rate 10 per minute, PaCO2 54 mmHg at rest, ventilation x1.21 under CO2 (peak PaCO2 87 mmHg); the rhythm is regular.

**Answer: `RTN>PBC+RTN>PHR`** (the chemoreceptor relay reaches neither the rhythm generator nor the phrenic neurons: congenital central hypoventilation, "Ondine's curse").

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
