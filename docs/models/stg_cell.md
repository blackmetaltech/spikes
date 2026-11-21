# stg_cell: crab stomatogastric neuron

`models/nestml/stg_cell.nestml`, NESTML 9.0.1. A single compartment with fast sodium (Na), delayed-rectifier potassium (Kd), transient and slow calcium (CaT, CaS), A-type potassium (A), calcium-activated potassium (KCa), the hyperpolarisation-activated current (H), a leak, and intracellular calcium (time constant 303 ms). Units: mV, ms, nS (1 mS/cm2 of a 0.628e-3 cm2 cell = 628 nS), pF (C = 628 pF), calcium in uM.

**Provenance, honestly.** The structure and the form of the gating functions (Boltzmann steady states, voltage-dependent time constants, the calcium dependence of KCa, the Nernst reversal of calcium) follow the model family of Prinz et al. 2003. The numerical constants in the gating functions were written from memory of those papers and **have not been checked against them**. The default maximal conductances are game values in the ranges used by Prinz et al. 2004, not their published parameter sets. The model has not been validated against recordings.

**Parts-bin default.** The parts bin creates a cell with only the spike currents (g_Na 62800, g_Kd 31400, g_A 6280, g_L 6.28 nS) and g_CaT, g_CaS, g_KCa, g_H at 0: it is silent and has no burst or rebound mechanism, so a bursting or rebounding cell has to be built by adding the currents (the levels `s_pacemaker`, `s_phase`, `crab_pyloric` need them). The NESTML parameter defaults below (the full set) are unchanged.

**Behaviour measured here.** With the NESTML defaults and a bias of 0 pA a single cell bursts about every 0.9 to 1 s, 8 to 9 spikes in a burst of about 0.3 s (`g_KCa` 3140 nS); at -50 pA it is silent; with `g_KCa` 0, a strong H current and some inhibition it fires a rebound burst after the inhibition ends (`s_phase`).

**Spikes.** One spike per upward crossing of -10 mV (re-armed below -30 mV); bursts are the clusters of these spikes.

**Synapses.** Spike-triggered conductances: `excitatory` (reversal 0 mV, 5 ms) and `inhibitory` (reversal -80 mV, 20 ms) ports, weight in nS (a negative circuit weight inhibits). The real glutamatergic inhibition is graded as well; that is not modelled.

**Input.** `I_stim` (a `dc` input in pA) and the parameter `I_e`. No `ne`/`ach` parameters: the modulator of the pyloric rhythm is a dc input.
