# sa_node_pacemaker: sinoatrial node pacemaker cell

Spontaneously beating, about 68 beats/min, slowed by ACh and sped up by noradrenaline. One spike is one beat.

## Source and what was reduced

The structure follows the rabbit SA node cell models of Demir et al. 1994 (*Am J Physiol* 266:C832), Kurata et al. 2002 (*Am J Physiol Heart Circ Physiol* 283:H2074) and Maltsev & Lakatta 2009 (*Am J Physiol Heart Circ Physiol* 296:H594). Kept: the five currents that make the pacemaker cycle, and the way the autonomic system acts on them. Removed: ion concentrations, the Na/K pump, Na/Ca exchange and the Ca clock (the SR release that Maltsev & Lakatta add), the I_Kr/I_Ks split (one delayed rectifier), I_st and I_Na.

**This is a re-fit, not a copy.** Each current is a Hodgkin-Huxley gate with a sigmoid steady state and a sech-shaped time constant, with conductances adjusted by hand-scripted scans (a throwaway Python prototype, not kept) until the isolated cell beat at about 70/min and showed the right direction and rough size of the ACh and NE effects. The kinetics are not those of any published model, and the numbers below are not claimed to match a paper figure. It is the right tool for a game cell with a believable rate range; it is not a research model of SA node electrophysiology.

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
| I_f activation | shift -5 mV (`shift_f_ach`) | shift +10 mV (`shift_f_ne`) and g_f x(1 + 1.0 a) (`g_f_ne`) |
| I_CaL | g_CaL x(1 - 0.1 a) (`g_CaL_ach`) | g_CaL x(1 + 0.3 a) (`g_CaL_ne`) |
| I_K | I_KACh, g_KACh = 0.03 nS | activation tau / (1 + a) (`tau_K_ne`) |

Engine mapping: `{"modulator":"ACh","pop":"SA","param":"ach","gain":1.0}` and the same for `NE` / `ne`.

## Parameters

C_m 32 pF, g_f 0.34 nS (E_f -30 mV), g_CaL 3.53 nS (46 mV), g_CaT 0.78 nS (45 mV), g_K 3.71 nS (E_K -85 mV), g_L 0.035 nS (E_L -30 mV). Synapses: `E_exc` 0 mV, `E_inh` -85 mV, taus 5 and 10 ms; spike weights are in nS (positive excites, negative inhibits). Beat detector: upward crossing of `V_th` = 0 mV, re-armed below -30 mV. Solver: GSL rk45.

## Results (`test_nestml_sa_node`, 0.1 ms resolution)

| | beats/min |
|---|---|
| baseline | 68.0 (ISI CV < 0.0005: a limit cycle) |
| ACh 0.3 / 1.0 | 63.7 / 54.8 |
| NE 0.3 / 1.0 | 91.6 / 103.1 |
| ACh 0.3 and NE 0.3 | between the two single effects |
| I_e +3 / -3 pA | 74.7 / 58.8 |

V_m runs from -74 mV (maximum diastolic) to +3 mV, against about -60 / +20 mV in a real rabbit cell: the cell is more hyperpolarised and has a smaller overshoot. Beating is also lost under strong sustained depolarisation (I_e of about 6 pA, or a heavy excitatory conductance), like a real cell in depolarisation block.

Known limits: no accentuated antagonism, no ACh above about 0.6 behaving gradually (the rate falls steeply near the bifurcation: ACh 1.0 is already -20 %), no heart-rate variability beyond what the inputs give.
