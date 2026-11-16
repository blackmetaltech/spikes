# Reference circuit: sinoatrial pacemaker cluster (`circuits/heart_pacemaker.json`)

Task `heart_pacemaker` (engine/src/tasks/heart_pacemaker.cpp, chapter "heart", view "heart", order 200). The SA node is the heart's
pacemaker: a patch of cells that fire spontaneously, each spike one beat. The player places `sa_node_pacemaker` cells
(`docs/models/sa_node_pacemaker.md`), reads their spikes out as `sa_beat`, and the task feeds every spike to the heart body
(`spike::body::Heart`, `docs/body/heart.md`) as a beat request. The heart merges requests closer than its 250 ms refractory
period into the first one, so the first cell of a cluster to fire starts the beat, and a cluster that cannot agree on one rhythm
produces extra, irregular beats. Autonomic tone is held at rest (sympathetic 0.2, vagal 0.6): this task is about the intrinsic rhythm.

## Score
Analysis window 6-30 s (the first 6 s are the settling of the cells onto their limit cycle). From the beat times of the heart:
`hr_bpm` = 60000 / mean interval, `cv` = sd / mean of the intervals, `sdnn_ms` and `rmssd_ms` from `analysis::hrv_analyze` (Task Force
1996), and `synchrony` = Golomb (2007) chi of the cluster's spike counts in 40 ms bins (0 = asynchronous, 1 = all cells spike together;
only for a motor pool of 2 or more cells).

* **Pass**: 60 <= hr <= 80 bpm **and** beat-interval CV < 5 %, at the end of the episode (30 s).
* **Value**: `100 (0.5 rate_term + 0.5 cv_term) (0.85 + 0.15 sync_term)`. `rate_term` is 1 inside 60-80 and falls linearly to 0 at 20 bpm
  outside; `cv_term` is 1 up to 2 % and 0 at 15 %; `sync_term = clamp((chi - 0.3)/0.6, 0, 1)` for a cluster, 0 for one cell. So a single
  working cell scores 85, and a tightly coupled cluster up to 100: synchrony is the bonus.
* **Verdict** names the failure: not wired, no beats, too few beats (pauses or arrest), bradycardia, tachycardia (and whether the cells
  are out of step), irregular rhythm.

## Wiring (7 neurons)
```
SA_c (3 cells, I_e -3.5 pA)  <-- 2 nS excitatory, 1 ms, all to all -->  SA_p (4 cells, I_e +2.5 pA)       outputs: sa_beat = all 7 cells
```
| Population | Model | Role |
|---|---|---|
| SA_c (3) | `sa_node_pacemaker`, `I_e` -3.5 pA, `V_m` -65 mV | "central" cells: intrinsically slow (62 beats/min alone) |
| SA_p (4) | `sa_node_pacemaker`, `I_e` +2.5 pA, `V_m` -45 mV | "peripheral" cells: intrinsically fast (about 72/min alone) |

The two groups differ in intrinsic rate (the bias current stands for the node's centre-to-periphery gradient in cell size, I_f density and
capacitance) and in starting phase (`V_m` pinned, so the result does not depend on the engine's random initial draw). The coupling is an
excitatory synapse (2 nS, conductance, reversal 0 mV, 1 ms delay) in both directions. The `modulation` table maps the engine's `ACh` and `NE`
channels onto every cell's `ach` / `ne`, so the sandbox dials work. There are no sensors.

## Why it works
* **Alone, the two groups disagree** (62 and 72.7 beats/min). Uncoupled, the heart gets both rhythms interleaved: 96 beats/min with 36 % interval
  variation (task value 11).
* **Coupling entrains them** to one rate (73.8/min here, between the two) with intervals constant to 0.02 %. The phase offset between the groups
  shrinks as the coupling grows: the cells lock for any weight from about 0.5 nS up to 4 nS (synchrony chi 0.70 at 0.5 nS, 0.78 at 1, 0.90 at 2, 0.93 at 4);
  at 2 nS the bonus is nearly full. Entrainment of pacemaker cells of different intrinsic rate by coupling is the classical picture of how the node
  works (Jalife 1984; Michaels, Matyas & Jalife 1987).
* **A single cell** beats at 68.0/min, CV 0.04 %, and scores 85 (pass, no cluster bonus).
* The cells are deterministic limit cycles: the three-plus seeds give the same score. (A Poisson "channel noise" input was tried and rejected:
  at 0.01-0.03 nS and 100 Hz it already knocks cells out of lock in some seeds, which says more about the sharp excitability of the reduced model than
  about the node.)

## Gap junctions
The node's coupling is electrical (connexin 43/45/40 gap junctions). The engine's `gap_junction` synapse needs `hh_psc_alpha_gap` or
`hh_cond_beta_gap_traub` cells on both sides, which fire at tens of hertz, not once a second. The NESTML SA cell has no gap-junction interface, so the
cluster uses a fast excitatory conductance as the stand-in (a pulse-coupled oscillator network rather than a continuously coupled one). Both entrain
cells of different intrinsic rate; the difference is that a pulse is delivered at the spike, where a gap junction acts through the whole cycle.

## What breaks it (tests in `engine/tests/tasks_heart_test.cpp`, seeds 1 and 2)
| Lesion | Result | Expected biology |
|---|---|---|
| coupling removed (SA_c <-> SA_p weight x0) | 96 beats/min, CV 36 %, "tachycardia" (value 11) | uncoupled node cells of different rates give no single rhythm (sinus node dysfunction, exit block) |
| I_CaL blocked by 45 % (`g_CaL` x0.55) | no beats at all | L-type Ca current makes the upstroke (nifedipine, Cav1.3 knockout) |
| I_CaT blocked (`g_CaT` 0) | 51.8 beats/min, bradycardia | T-type Ca (Cav3.1) helps late diastolic depolarisation; Cav3.1-null mice are bradycardic (Mangoni et al. 2006) |
| tonic ACh 0.8 (`ach` param) | 5 beats in 24 s: pauses | M2 / I_KACh: vagal arrest at high ACh |
| tonic NE 1.0 (`ne` param) | 109 beats/min, tachycardia | beta-adrenergic stimulation |

Blocking I_f completely (`g_f` 0) only slows the reduced cell to 64.9 beats/min and still passes; in a real node I_f block (ivabradine) slows the rate by
15-20 %, so the I_f contribution in this re-fit model is smaller than in the cells it is modelled on (see `docs/models/sa_node_pacemaker.md`).

Credits: Jose AD, Collison D (1970) Cardiovasc Res 4:160 (intrinsic heart rate); Jalife J (1984) J Physiol 356:221; Michaels DC, Matyas EP, Jalife J (1987)
Circ Res 61:704; Mangoni ME et al. (2006) Circ Res 98:1422; Golomb D (2007) Scholarpedia 2(2):1347 (chi measure); Task Force of the ESC and NASPE (1996)
Circulation 93:1043 (HRV).
