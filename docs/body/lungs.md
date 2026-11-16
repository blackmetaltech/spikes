# Lungs, CO2 and chemoreceptors

`spike::body::Lungs` (`body/lungs.h`).

## Mechanics (Otis et al. 1956, single compartment)
`R dV/dt + (V - FRC)/C = Pmus`, `R = 3 cmH2O s/L`, `C = 0.1 L/cmH2O` (tau 0.3 s), `FRC = 2.4 L`, `Pmus = 20 cmH2O * lowpass_80ms(drive)`.
`LungInput::drive` (0..1) is the phrenic / inspiratory motor-pool output; expiration is passive (exponential return to FRC; no active
expiration, floor at 1.2 L). Rest: drive ~0.25 for 1.5 s every 5 s gives VT 0.50 L, 12 breaths/min, VE 5.9 L/min; drive 1 gives VT ~2 L.
Breaths are detected from flow; `tidal_l`, `rate_bpm`, `ve_l_min` and `va_l_min` use a 30 s window. Alveolar ventilation excludes the first
`dead_space = 0.15 L` of each inspiration: `VA = (VT - VD) f`.

## Gas exchange: PaCO2 balance (Grodins et al. 1967; Khoo et al. 1982 style, reduced)
Two pools (volumes are gas-equivalents): arterial/lung `Va = 4 L`, tissue/brain `Vt = 15 L`, coupled by conductance `Qc = 36 L/min`.
```
Va dPa/dt = Qc (Pt - Pa) - VA (Pa - PI)          Vt dPt/dt = 863 VCO2 - Qc (Pt - Pa)
```
Steady state is the alveolar gas equation `Pa = PI + 863 VCO2/VA` (VCO2 0.2 L/min, VA 4.2 L/min -> 41 mmHg; tissue 46 mmHg). `VA` enters through a 2 s
mixing low-pass. Perturbations: `metabolic` multiplies VCO2 (exercise 3-8x), `inspired_pco2` (mmHg, 5 % CO2 ~ 36), `time_scale` divides the pool volumes
for game pacing (steady states unchanged; breath-synchronous ripple grows). Time constants at `time_scale = 1`: about 1.5-3 min; apnoea raises PaCO2 ~11 mmHg/min (real 3-6, a compromise to keep the game fast).
Tests: eupnoea PaCO2 41 mmHg and VE 5.9 L/min; PaCO2 falls monotonically with drive; simulated steady state within 7 % of the gas equation; exercise x4 at fixed
ventilation 174 mmHg vs 48 mmHg with a proportional chemoreflex (VE 26 L/min); inspired CO2 raises PaCO2 and both chemo rates.

## Chemoreceptor afferents
- **Peripheral (carotid body)**: `0.7 + 0.45 (PaCO2 - 35)` Hz, 2 s low-pass, x(1 + 2 hypoxia) (Lahiri & DeLaney 1975; fast, O2-sensitive). ~3.5 Hz at rest.
- **Central (retrotrapezoid / brainstem)**: `1.2 (Pt - 38)` Hz on tissue/brain PCO2 (Guyenet, Stornetta & Bayliss 2010; Nattie & Li 2012): slow, ~9.6 Hz at rest.
Gains are illustrative. Both are rates (Hz) for sensor channels (`peripheral_hz`, `central_hz`); `readouts()`: `paco2_mmHg, tissue_pco2_mmHg, volume_l, tidal_l,
resp_rate_bpm, ve_l_min, va_l_min, periph_chemo_hz, central_chemo_hz`. `view()`: volume fraction, flow, PaCO2, VE, rate, `inspiring`.

## Limitations
No O2 / pH / bicarbonate buffering model (hypoxia is a sensitising input only), no airway reflexes, no stretch receptors (Hering-Breuer), no active expiration.
