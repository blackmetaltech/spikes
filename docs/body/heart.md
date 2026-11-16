# Heart and circulation

`spike::body::Heart` (`body/heart.h`): a closed, volume-conserving loop. Arterial compartment = **2-element Windkessel**
(Frank 1899; Westerhof, Lankhaar & Westerhof 2009), venous/central compartment = lumped capacitor.
```
C_a dP/dt  = Q_ej(t) - (P - Pv)/R           (mmHg, ml/s;  C_a = 1.3 ml/mmHg, R = 1.06 mmHg s/ml)
C_v dPv/dt = (P - Pv)/R   [- SV at each beat] [- pooling] [- haemorrhage] [+ venoconstriction]     (C_v = 100 ml/mmHg)
```
Ejection `Q_ej` is a half-sine of duration `T_ej = 0.33 sqrt(RR)` (Weissler-type) with integral = stroke volume.

## Beat events
`HeartInput::sa_spikes > 0` fires a beat (250 ms refractory merges doublets); else `sa_rate_hz >= 0` runs a deterministic phase
accumulator (`0` = beats only from spikes); else (`< 0`) the intrinsic pacemaker `HR = 105 + 80 sym - 85 vagal` bpm
(Jose & Collison 1970: intrinsic ~105 bpm; resting tone sym 0.2, vagal 0.6 gives 70 bpm).

## Stroke volume
`SV = SV0 * contractility * Starling(Pv) * filling(RR)`: Starling `Pv/(Pv + 4)` normalised to rest (Frank-Starling, saturating), contractility
`1 + (sym_cardiac - 0.2)` (sympathetic), diastolic filling `(1 - exp(-Td/0.35 s))`, `Td = RR - T_ej`, normalised to 70 bpm. Higher HR shortens filling,
so SV falls (71 -> 47 ml from 70 to 120 bpm in the test) while CO and MAP still rise. At each beat SV leaves the venous compartment (conservation).

## Autonomic and perturbations (all drives 0..1, filtered by 1.5 s)
- `sym_vascular`: `R = R0 (1 + 0.8 (sym - 0.2))` and venoconstriction (+200 ml stressed volume per unit above rest).
- `standing` 0..1: up to 220 ml pools in the legs (tau 6 s): venous return and CVP fall, SV and MAP fall (without reflex the test shows MAP 93 -> 73 mmHg).
- `haemorrhage_ml_s`: volume removed from the venous side (300 ml: MAP 93 -> 63 mmHg without reflex, a deliberately severe, game-readable fall).
- Resting values (test): MAP 93, 112/76 mmHg, HR 70, SV 72 ml, CO 5.0 L/min, CVP 5 mmHg; diastolic decay tau = R C = 1.4 s.

## Baroreceptor afferent
`rate = 100 Hz * sigmoid((P + tau_d dP/dt - 100)/20)`, `tau_d = 0.04 s`: sigmoid pressure coding with threshold ~50-60 and saturation ~180-200 mmHg
(Kirchheim 1976) and a rate (dP/dt) term (Chapleau & Abboud 1987; Mahdi et al. 2013): at equal pressure the rising limb fires more than the falling limb (test: 53 vs 39 Hz). `baro_hz`
is instantaneous, `baro_mean_hz` (1 s average) is the one to wire to a sensor channel (or generate spikes from it). This is a whole-nerve rate, not per fibre.

## API
Outputs: `pressure, dpdt, map_beat, map_smooth, systolic, diastolic, cvp, hr_bpm, stroke_ml, co_l_min, contractility, resistance, baro_hz,
baro_mean_hz, blood_lost_ml, pooled_ml, beat_now, beats`. `readouts()`: `map_mmHg, sys_mmHg, dia_mmHg, pressure_mmHg, hr_bpm, stroke_ml, co_l_min,
cvp_mmHg, baro_hz, contractility, tpr, blood_lost_ml, pooled_ml`. `view()`: beat phase, ejection (0..1, heart size), pressures, `beat`.

## Limitations
No afterload dependence of SV, no valve/chamber model, no venous-return curve beyond the lumped reservoir, no nonlinear compliance, no respiratory pump.
