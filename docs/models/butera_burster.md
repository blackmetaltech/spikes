# butera_burster: preBotzinger persistent-sodium bursting pacemaker

Butera RJ, Rinzel J, Smith JC (1999) Models of respiratory rhythm generation in the pre-Botzinger complex. I. Bursting pacemaker neurons. *J Neurophysiol* 82:382-397, model 1. The equations and parameters are the paper's: single compartment, `I_NaP`, `I_Na`, `I_K`, `I_L` and the tonic excitatory conductance `g_tonic-e`.

```
C dV/dt = -I_NaP - I_Na - I_K - I_L - I_tonic - I_syn - I_mod + I_e + I_stim
I_NaP = g_NaP m_p,inf(V) h (V-E_Na)       h slow: tau_h = 10000 ms / cosh((V+48)/12)
I_Na  = g_Na m_inf^3 (1-n) (V-E_Na)       n fast: tau_n = 10 ms / cosh((V+29)/-8)
I_K   = g_K n^4 (V-E_K)
I_tonic = g_tonic (V-0 mV)
```

C 21 pF, g_NaP 2.8 nS, g_Na 28 nS, g_K 11.2 nS, g_L 2.8 nS, E_Na 50, E_K -85, E_L -65 mV, theta/sigma as in the paper (m -34/-5, n -29/-4, mp -40/-6, h -48/6 mV). Bursts start when I_NaP depolarises the cell past the spike threshold, and stop as h inactivates over seconds; the silent phase is h recovering.

## Interface

* One spike per action potential (upward crossing of -10 mV, re-armed below -30 mV). A burst is a cluster of spikes; analysis code splits them by an inter-spike gap (the tests use 500 ms; intra-burst intervals are tens of ms).
* `g_tonic` (nS) is the paper's drive. Default 0.3 nS, which bursts at about 0.2 Hz.
* `ne`, `ach` (0..1) add an excitatory conductance `g_ne * ne + g_ach * ach` (0.3 and 0.1 nS per unit, E = 0 mV). **This mapping is a game choice, not a paper result:** NE excites preBotC neurons (alpha1), and ACh has mixed muscarinic effects here, taken as net excitatory and weaker.
* Exponential conductance synapses (E_exc 0, E_inh -75 mV, 5 and 10 ms), weights in nS, positive excites and negative inhibits; a `continuous` current port and `I_e`.

## Results (`test_nestml_butera`, g_NaP = 2.8 nS unless stated, 60 s after a 10 s transient)

| g_tonic (nS) | burst period | burst duration | spikes/burst |
|---|---|---|---|
| 0 | silent | | |
| 0.30 | 4.88 s | 0.43 s | 13 |
| 0.35 | 2.60 s | 0.34 s | 7 |
| 0.40 | 1.30 s | 0.21 s | 3 |
| 0.60 | tonic spiking (about 5.7 Hz) | | |

With g_tonic = 0.30 nS: g_NaP = 3.5 nS gives period 2.34 s and duration 0.63 s; g_NaP = 2.0 nS at g_tonic 0.4 nS is silent. Excitation moves the cell silent -> bursting (faster as drive grows) -> tonic, and more I_NaP also speeds bursting.

### Figure check (S6): what matches the 1999 paper, and what is not claimed

`test_regime_sweep` scans g_tonic-e at g_NaP 2.8 nS (60 s after a 10 s transient):

| g_tonic (nS) | 0.15-0.25 | 0.28 | 0.30 | 0.33 | 0.35 | 0.38 | 0.40 | 0.45 | 0.60 |
|---|---|---|---|---|---|---|---|---|---|
| behaviour | silent | burst 6.7 s | 4.9 s | 3.3 s | 2.6 s | 1.7 s | 1.3 s | tonic | tonic |

Matches the paper qualitatively (model 1, Fig. 2-3 of Butera et al. 1999, which show the g_tonic-e axis of the activity map): (1) silent at low drive, one contiguous bursting window, tonic spiking above it; (2) burst frequency rises steeply across the window (period 6.7 s down to 1.3 s here, i.e. 0.15 to 0.8 Hz, the paper's range of preBotC bursting); (3) the burst gets shorter and has fewer spikes as drive rises (17 -> 3 spikes/burst), the cell moving toward tonic firing; (4) bursts are seconds long in period and a few hundred ms in duration (a short active phase on a slow I_NaP inactivation, tau_h,max 10 s); (5) more g_NaP speeds bursting and too little g_NaP leaves the cell quiescent. The test asserts these relations (window of at least 5 grid steps, monotone period, non-increasing duration).

Not claimed: the paper gives the maps as plots and no digitised copy exists in this repo (the paper is not machine-readable offline), so the window edges (0.28 and 0.40-0.45 nS here) and exact periods are NOT compared against numbers read off the figures. Treat this as a regime check against the paper's published qualitative behaviour, not a digitised-figure validation. The period at a given drive depends on the 10 s tau_h.
