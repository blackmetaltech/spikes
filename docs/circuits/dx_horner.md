# Diagnosis case: dx_horner (`circuits/dx_horner.json`)

Task `dx_horner` (`engine/src/tasks/dx_horner.cpp`, chapter "autonomic", order 440, diagnosis, off the spine). World: the `pupil` task (`pupil.md`: light flash, darkness, fright). Generator: `python3 circuits/tools/gen_boss_dx.py dx_horner`.

## The case
A 61-year-old smoker has one small pupil and a drooping lid. In the circuit: the pupil constricts briskly in bright light (2.6 mm, 218 ms), opens in the dark only to 5.0 mm (7.2 normal), fright adds 1.7 mm, the dilator never fires (SCG 0 Hz) while the sphincter works.

**Answer: `IML>SCG`** (the sympathetic chain no longer drives the superior cervical ganglion: Horner syndrome).

## Candidates (seed 1)
| Candidate | Result |
|---|---|
| `IML>SCG` (answer) | small pupil (5 mm dark), light reflex normal (2.6 mm), dilator silent |
| `PON>EW` | light reflex weak (5 mm bright), dark pupil 7.1 mm: the opposite pattern |
| `EW>CG` | the same one synapse later |
| `RGC>PON` | no light reflex at all, wide fixed pupil 7.2 mm |
| `HYP>IML+HYP>EW` | light reflex and dark pupil normal (7.1 mm), fright adds only 0.3 mm |
Cutting `HYP>IML` alone would look the same as the answer (first-order Horner); the pupil model cannot tell the three neurons apart, so it is not offered.

## Biology
The sympathetic supply to the eye is a three-neuron chain (hypothalamus to spinal cord, then the chain, then the superior cervical ganglion); interrupting it anywhere gives miosis, ptosis and dilation lag with a normal light reflex. Credits: Kanagalingam S, Miller NR (2015) Eye Brain 7:35; Loewenfeld IE (1993) *The Pupil*; McDougal DH, Gamlin PD (2015) Compr Physiol 5:439.
