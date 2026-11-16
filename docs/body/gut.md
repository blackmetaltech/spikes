# Gut: peristalsis

`spike::body::Gut` (`body/gut.h`): `n_segments` (12) smooth-muscle segments and a bolus.

## Segment
Membrane drive `m_i = ex_i - inh_i + A * sw_i(t)`; contraction target `sigmoid((m - 0.6)/0.07)`; contraction follows with `tau = 500 ms` (rise) / 800 ms
(fall); luminal pressure `10 cmH2O * contraction`.
- `ex`, `inh` (0..1 per segment) are enteric excitatory (ACh / substance P) and inhibitory (NO / VIP) motor output.
- **Slow waves**: the interstitial cells of Cajal pacemaker (Sanders 1996; Huizinga & Lammers 2009): the depolarising half of a 0.2 Hz (12/min, small intestine)
  sine, amplitude `A = 0.4`, an aboral phase lag of 0.04 cycles/segment (test: 800 ms from segment 2 to 6). Alone they stay below threshold (max contraction 0.04, no
  propulsion); with sub-threshold ACh (0.35) they gate phasic contractions (0.05..0.78) travelling aboral.

## Bolus
Position `x` (segment units, length 1.2). Net push = oral-side minus aboral-side luminal pressure at `x -/+ L/2` (interpolated between segment centres),
minus a static friction `p_static = 0.3 cmH2O`; velocity relaxes (200 ms) to `mu * push`, `mu = 0.25 segments/s per cmH2O`. Clamped at both ends. A uniform pressure
field exerts no net force, so uniform contraction (or relaxation) cannot move the bolus; only a **gradient** can: this is the Bayliss-Starling law of the intestine
(1899: contraction oral, relaxation aboral; reviewed by Costa, Brookes & Hennig 2000).

## Tests
- A ring of excitation moving aboral at 1 segment/s with descending inhibition ahead carries the bolus 4.8 segments in 5 s (stays with the wave).
- Uniform contraction (all ex = 1): displacement 0.000. Uniform relaxation: 0.
- A reversed (oral) wave moves it orally (-2.8). Slow waves alone: no propulsion. Bolus stays inside the tube.

`readouts()`: `bolus_pos, bolus_vel, bolus_distance, mean_contraction, seg0..seg{N-1}`; `view()`: per-segment contraction, bolus position/length as fractions.
The task's circuit decides the pattern of `ex`/`inh`, for instance a chain of enteric neurons with ascending excitation and descending inhibition.

## Limitations
1-D plug-flow bolus, no fluid mixing / segmentation motility (the slow-wave-gated contractions only hint at it), no viscosity change, no ICC-muscle coupling dynamics.
