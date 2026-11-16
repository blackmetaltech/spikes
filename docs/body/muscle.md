# Muscle: Hill-type muscle driven by motor units

`spike::body::Muscle` (`body/muscle.h`). One muscle = a pool of `n_units` motor units (default 20, index 0 = smallest/slowest)
plus Hill contractile mechanics (rigid tendon).

## Motor unit
- **Twitch**: critically damped two-stage kernel, `s(t) = (t/T) exp(1 - t/T)` (peak 1 at t = T, the contraction time). It is
  implemented as two cascaded first-order stages, updated exactly per slice, so a spike adds an impulse and **twitches sum linearly**
  (Burke et al. 1973 for the shapes). Half-relaxation time is `1.68 T` (measured HRT in the test: 164 ms for T = 90 ms, 55 ms for T = 30 ms).
- **Saturation**: the summed twitch signal `c` is turned into activation `a = 1 - exp(-c/c0)`, `c0 = 3`: one twitch reaches ~28 % of
  the tetanus (tetanus/twitch ~ 3.5), unfused trains ripple, fast trains saturate.
- **Fusion** (test, ripple < 10 %): slow unit 10 Hz, fast unit 31 Hz. Slow units therefore fuse at the 10-25 Hz of cat soleus
  (Burke 1973), fast units at 30-60 Hz; a pool whose large units fire at <= 45 Hz stays partly unfused (pool maximum with all
  units recruited = ~66 % of the electrically fused tetanus, in line with voluntary vs stimulated maxima).

## Size principle (Henneman 1957/1965; Fuglevand, Winters & Patla 1993)
Unit `i` (f = i/(n-1)): force share `∝ force_range^f` (30x), contraction time geometrically from 90 ms (S) to 30 ms (FF),
recruitment threshold geometrically from 0.02 to 0.80 of pool excitation, peak rate 35 -> 45 Hz, `vmax` 3 -> 8 `l_opt`/s,
fatigue rate 0.002 -> 0.03 /s. Rate coding: a unit fires at `min_rate (8 Hz) + 50 Hz * (e - threshold)`, capped at its peak.
- **Rate mode** `MuscleInput::excitation` (0..1): regular, phase-staggered spike trains per unit (optional `rate_cv` jitter, seeded).
  Force rises monotonically with excitation and units are recruited strictly small to large (tests).
- **Drive from a motor pool (spinal tasks)**: the limb rig (`engine/src/tasks/spinal_limb.h`) sets `excitation` = the pool's *total* rate (cells x
  mean rate, low-passed 15 ms) / (20 units x `rmax_hz`), so the drive scales with how many motoneurons fire and how fast: one motoneuron at `rmax_hz`
  excites 1/20 of the muscle (one unit's share), not all of it. Pool size is therefore part of every spinal design. Motoneuron spike timing is still
  not delivered unit by unit (that would be spike mode, below); the muscle makes its own regular unit trains.
- **Spike mode** `MuscleInput::spikes[i]`: spike count of unit `i` this slice (e.g. one motor neuron per unit, ordered small to large).

## Hill mechanics (Zajac 1989; Millard et al. 2013)
`F = F_max [ sum_i w_i cap_i a_i f_V(v/vmax_i) * f_L(l) + f_PE(l) + b v ]`, `l` in `l_opt`, `v` in `l_opt/s` (+ = lengthening).
- `f_L(l) = exp(-((l-1)/0.45)^2)` (Gaussian, after Gordon, Huxley & Julian 1966).
- `f_V`: Hill 1938 hyperbola `(1-u)/(1+u/k)`, `u = -v/vmax`, `k = 0.25`; eccentric branch `(1 + 1.8 s w)/(1 + s w)` slope-matched, plateau 1.8.
- `f_PE(l) = (exp(4 (l-1)/0.6) - 1)/(exp(4) - 1)` for `l > 1` (Schutte 1993): F_max at 60 % strain; damping `b = 0.05 F_max s/l_opt`.
- Each unit has its own `vmax`, so fast units dominate at high shortening speed.

## Fatigue (after Xia & Frey-Law 2008, reduced to one capacity state per unit)
`d cap/dt = s ( -F_i a_i cap + R (1 - cap) )`, `R = 0.004 /s`, `s = fatigue_time_scale` (use 30-60 to see minutes of fatigue in game
seconds). Fast units fatigue 15x faster than slow ones; force drops to ~46 % after a sustained maximal effort of 60 s x 60 (test), recovers at rest.
The F/R values are illustrative, not fitted.

## API
`Muscle::step(dt_ms, MuscleInput{excitation | spikes, length, velocity})`; outputs `force_N, active_N, passive_N, activation,
fatigue_level, n_active`; `force_at(l, v)` (const) for sub-stepped mechanics; `readouts()`: `muscle_force_N, muscle_active_N,
muscle_passive_N, muscle_activation, muscle_fatigue, muscle_units_active`; `view()`: per-unit activation, force fraction, fatigue.

## Limitations
Rigid tendon (no series elasticity), no pennation, no catch-like doublet potentiation, no muscle-length dependence of twitch speed.
