# sa_node_pacemaker: sinoatrial node pacemaker cell

Spontaneously beating, about 100 beats/min on its own (the intrinsic rate, with no autonomic input), slowed to 60-70/min by resting vagal tone (ACh about 0.5) and sped up by noradrenaline. One spike is one beat.

## Source and what was reduced

The structure follows the rabbit SA node cell models of Demir et al. 1994 (*Am J Physiol* 266:C832), Kurata et al. 2002 (*Am J Physiol Heart Circ Physiol* 283:H2074) and Maltsev & Lakatta 2009 (*Am J Physiol Heart Circ Physiol* 296:H594). Kept: the five currents that make the pacemaker cycle, and the way the autonomic system acts on them. Removed: ion concentrations, the Na/K pump, Na/Ca exchange and the Ca clock (the SR release that Maltsev & Lakatta add), the I_Kr/I_Ks split (one delayed rectifier), I_st and I_Na.

**This is a re-fit, not a copy.** Each current is a Hodgkin-Huxley gate with a sigmoid steady state and a sech-shaped time constant, with conductances adjusted by hand-scripted scans (a throwaway Python prototype, not kept) until the isolated cell with ACh = NE = 0 beat at about 100/min (the human intrinsic rate, Jose & Collison 1970; the published rabbit cells beat at 150-200/min, so the rate scale here is human, not rabbit), resting ACh about 0.5 brought it to 60-70/min, and the cell showed the right direction and rough size of the ACh and NE effects. The kinetics are not those of any published model, and the numbers below are not claimed to match a paper figure: **no published figure was digitised for this cell, so it is not "validated against its paper figure"**. What is checked (`test_nestml_sa_node`) is the intrinsic rate range (92-108/min), the resting-tone rate (60-72), the direction and rough size of the ACh and NE effects, and the NEST/PyNEST spike parity. It is the right tool for a game cell with a believable rate range; it is not a research model of SA node electrophysiology.

```
C dV/dt = -(I_f + I_CaT + I_CaL + I_K + I_KACh + I_leak) - I_syn + I_e + I_stim
```

| Current | Form | Notes |
|---|---|---|
| `I_f` (HCN) | `g_f y (V-E_f)` | `y_inf` half-activation -64 mV, slope 9 mV, tau 60-960 ms; the slow diastolic depolarisation |
| `I_CaT` | `g_CaT d_T f_T (V-E_CaT)` | low threshold (half-activation -50 mV): late diastolic depolarisation |
| `I_CaL` | `g_CaL d_L f_L (V-E_CaL)` | half-activation -25 mV (Cav1.3-like): upstroke |
| `I_K` | `g_K n (V-E_K)` | slow delayed rectifier, tau up to 1.6 s: repolarisation and the pacing period |
| `I_KACh` | `g_KACh a_ach (V-E_K)` | only with ACh |
| `I_leak` | `g_L (V-E_L)` | |

## Autonomic parameters

`ach` and `ne` are dimensionless concentrations, 0 = none, about 1 = near saturating. Each acts through `x/(x+K_mod)` with `K_mod` = 0.3.

| | ach (vagal) | ne (sympathetic) |
|---|---|---|
| I_f activation | shift -35 mV (`shift_f_ach`) | shift +10 mV (`shift_f_ne`) and g_f x(1 + 1.0 a) (`g_f_ne`) |
| I_CaL | g_CaL x(1 - 0.1 a) (`g_CaL_ach`) | g_CaL x(1 + 0.3 a) (`g_CaL_ne`) |
| I_K | I_KACh, g_KACh = 0.2 nS | activation tau / (1 + a) (`tau_K_ne`) |

Engine mapping: `{"modulator":"ACh","pop":"SA","param":"ach","gain":1.0}` and the same for `NE` / `ne`.

## Parameters

C_m 32 pF, g_f 0.34 nS (E_f -30 mV), g_CaL 3.53 nS (46 mV), g_CaT 1.9 nS (45 mV), g_K 3.71 nS (E_K -85 mV), g_L 0.035 nS (E_L -30 mV). Synapses: `E_exc` 0 mV, `E_inh` -85 mV, taus 5 and 10 ms; spike weights are in nS (positive excites, negative inhibits). Beat detector: upward crossing of `V_th` = 0 mV, re-armed below -30 mV. Solver: GSL rk45.

## Results (`test_nestml_sa_node`, 0.1 ms resolution)

| | beats/min |
|---|---|
| baseline (ACh 0, NE 0: intrinsic) | 99.0 (ISI CV < 0.0005: a limit cycle) |
| ACh 0.3 / 0.5 / 1.0 / 1.5 | 74.4 / 67.1 / 58.4 / 54.4 (graded, no arrest) |
| NE 0.3 / 1.0 | 123.8 / 135.4 |
| I_e +3 / -3 pA | 110.5 / 86.0 |

V_m runs from -74 mV (maximum diastolic) to +3 mV, against about -60 / +20 mV in a real rabbit cell: the cell is more hyperpolarised and has a smaller overshoot. Beating is also lost under strong sustained depolarisation (I_e of about 6 pA, or a heavy excitatory conductance), like a real cell in depolarisation block.

Known limits: no accentuated antagonism, no heart-rate variability beyond what the inputs give. With the original (68/min) parameters ACh fell off a cliff near 0.6; the re-fit (I_KACh x6.7, I_f shift -35 mV, g_CaT x2.4) makes the ACh response graded up to ACh 1.5. The cell is close to depolarisation block: g_f above about 0.42 nS or I_e above about 4 pA stops it, and g_CaT x0.5 or g_CaL x0.4 silences it. The f-I catalogue thumbnail of a 1 Hz cell from a 1 s window is quantised (a catalogue issue, not a model one).
