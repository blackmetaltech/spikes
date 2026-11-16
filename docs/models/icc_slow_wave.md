# icc_slow_wave: interstitial cell of Cajal slow-wave pacemaker

A two-variable relaxation oscillator for the gut's electrical slow wave, 3 cycles/min (gastric rate; the small intestine runs 10-12/min, set `tau_w` lower for that). One spike is one slow-wave onset.

## Source

FitzHugh 1961 (*Biophys J* 1:445) and Nagumo et al. 1962, rescaled; **phenomenological, not a channel model** (the real mechanism is cyclic Ca release, Ano1 and Cav3.2 currents, e.g. Sanders 1996 *Gastroenterology* 111:492; Corrias & Buist 2007 *Ann Biomed Eng* 35:1595 for a biophysical model). It reproduces the slow wave's shape (fast upstroke, plateau of several seconds, fast repolarisation, long interval) and period, nothing else.

```
tau_x x' = x - x^3/3 - w + I_drive + (I_syn + I_e + I_stim)/I_scale
tau_w w' = x + 0.7 - 0.8 w
V_m = -70 mV + 25 mV (x + 2)/4
```

tau_x = 600 ms, tau_w = 6000 ms (this sets the period: a relaxation cycle is about 3.3 tau_w), `drive` 0.5, `I_scale` 100 pA (the current that shifts x by one unit).

## Interface

* Spike at the upward crossing of x = 0, re-armed below x = -0.5.
* `ach` raises the drive by `k_ach` = 0.3 per unit (muscarinic depolarisation: faster, a little larger); `ne` lowers it by `k_ne` = 0.1 per unit (sympathetic inhibition: slower). Both are heuristic mappings of the direction of the effects, not fitted to data.
* Current-based synapses (exponential PSC, 50 ms, weights in pA; positive excites, negative inhibits), `continuous` port, `I_e`.
* `V_m` is a read-out state updated each step.

## Results (`test_nestml_icc`)

| | cycles/min |
|---|---|
| baseline | 2.983 (period 20.1 s, ISI CV < 0.001) |
| ACh 1 | 3.230 |
| NE 1 | 2.762 |
| I_e +20 pA | 3.189 |
| 100 Hz x 1 pA Poisson excitation | 3.055 |

V_m spans -69.7 to -46.1 mV and spends about 35 % of the cycle above half amplitude. The frequency is set by `tau_w` and rises slowly with drive, so the tests check direction and a +-5 % target window, not a drive-frequency curve from the literature.
