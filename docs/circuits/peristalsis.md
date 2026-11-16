# Reference circuit: peristalsis (`circuits/peristalsis.json`)

Task `peristalsis` (engine/src/tasks/peristalsis.cpp, chapter "autonomic", view `gut`, order 410). An enteric circuit over 8 gut segments (`spike::body::Gut`, `docs/body/gut.md`) must carry a bolus (1.2 segments long) from segment 1.0 to 7.0. The episode is 40 s.

## World and score
- **Sensors** `dist0`..`dist7`: Poisson rate `500 Hz x overlap(bolus, segment i)`, so the segment the bolus fills fires its stretch afferent.
- **Motors.** `ach0..7` (excitatory motoneuron pool of segment i, Hz / 45 Hz), `no0..7` (inhibitory pool, Hz / 50 Hz), each low-passed over 300 ms (transmitter lifetime) and fed to the gut as `ex_i` and `inh_i`; `icc0..7` (**optional**): one spike of an interstitial cell of Cajal starts a 2.5 s slow-wave plateau (half a sine, amplitude 0.4, a 12 per minute wave) that the task adds to the excitatory drive; if the circuit declares icc outputs the gut's own clock is switched off and the ICC cells ARE the clock; if it declares none, the gut's built-in 12/min slow wave runs. `ach0..7` are required; `no0..7` are optional too (without them nothing relaxes ahead of the bolus). **A first circuit therefore needs only `dist0..7` -> interneurons -> `ach0..7` (8 sensors + 8 motors)**: with just that, in the 10-seed check the bolus reaches the end in about 32 s, a fail by the 25 s deadline (value 90); adding `no0..7` (inhibitory pools) brings it to 13 s and passes without any ICC; building ICC pacemakers is then the further refinement. Pass value for the reference: 10/10 seeds, arrival 13.9-14.5 s.
- **Rule of the muscle.** Contraction target `sigmoid((ex - inh + 0.4 sw - 0.6)/0.07)`, so a segment contracts when its drive is above 0.6; the bolus moves with the pressure drop across it (`mu` 0.25 segments/s per cmH2O, friction 0.3 cmH2O).
- **Score.** Progress `(position - 1)/6`; the bolus has arrived when its centre passes 7.0. Pass: arrival within 25 s. `value = 100 (0.8 progress + 0.2 time bonus)`.

## Wiring (49 neurons, par 49)
```
dist_i (Poisson) --> S_i --> AIN_i --(+36 pA)--> EMN_{i-1}  (oral)    --> ach_{i-1}
                       \---> DIN_i --(+100 pA)-> IMN_{i+1}  (aboral)  --> no_{i+1}
ICCP (pacemaker, oral) --> ICC_0 --> ICC_1 --> ... --> ICC_7 (400 pA, 100 ms hops)       --> icc_i
```
| Population (n) | Model | Role |
|---|---|---|
| S (8) | `iaf_psc_alpha` | intrinsic primary afferent neurons (IPANs): fire when the bolus fills more than about half of their segment |
| AIN (8), DIN (8) | `iaf_psc_alpha`, I_e 340 pA | ascending excitatory and descending inhibitory interneurons, one-to-one from S, 1:1 relays |
| EMN (8) | `iaf_psc_alpha`, I_e 376 pA | excitatory motoneurons (ACh / substance P): a resting tone of about 16 Hz (ex 0.35), raised to about 0.55 when AIN fires |
| IMN (8) | `iaf_psc_alpha`, I_e 365 pA | inhibitory motoneurons (NO / VIP): silent at rest, about 23 Hz (inh 0.45) when DIN fires |
| ICC (8) + ICCP (1) | `icc_slow_wave` (NESTML), tau_x 300 ms, tau_w 1060 ms | the slow-wave pacemakers (`docs/models/icc_slow_wave.md`; period 5.3 s). ICCP (drive 0.5) oscillates; ICC cells (drive 0.3) are excitable and each fires when the cell on its oral side does, 100 ms synaptic delay plus about 170 ms to threshold: a slow wave that travels aboral at about 3 segments/s |

## Why it works
* **Law of the intestine** (Bayliss & Starling 1899). A bolus in segments i, i+1 fires S_i, S_{i+1}; AIN_{i+1} excites EMN_i, so the segment under and behind the bolus contracts; DIN_i inhibits IMN_{i+1}, so the segment ahead relaxes. That pressure gradient pushes it aboral; as it moves, the next sensors fire and the pattern moves with it (the bolus stays with the wave).
* **ICC gating.** Resting tone (0.35) and the reflex excitation each stay below the 0.6 threshold; the slow-wave plateau (up to +0.4) lifts the total above it only for the 2.5 s of the wave. Contraction is therefore phasic, locked to the ICC wave, and the bolus advances in pulses every 5 s (reference arrival 14-14.5 s).
* **Descending inhibition matters** because the same tone plus slow wave also makes the aboral segments contract in each plateau (up to 0.8), which would press back on the bolus; the descending pathway holds the segment ahead relaxed (inh 0.45 against tone 0.35 + wave 0.4 < 0.6).
* **Parameter provenance.** Rates and gains were set with the task closed loop. Kept deliberately: tone and gains so that no one ingredient is enough to move the bolus fast (reflex alone, tone alone, wave alone); the 25 s deadline sits between the reference (14.5 s) and the lesions. ICC period is `3.3 tau_w`-ish for this FitzHugh-Nagumo cell (12 per minute is the small-intestinal rate; the cell's default is the 3 per minute gastric one).

## What breaks it (tests in `engine/tests/tasks_autonomic_test.cpp`)
| Lesion | Result |
|---|---|
| DIN -> IMN x0 (descending inhibition removed) | the segments ahead contract against the bolus; it arrives only after 34.8-34.9 s (limit 25 s): fails |
| ICCP -> ICC and ICC -> ICC x0 (no slow waves) | no plateau, so contraction stays near threshold; the bolus does not arrive in time: fails |
| S -> AIN and S -> DIN x0 (bolus not sensed) | no reflex: tone and the slow waves push it only to segment 2.5: fails |
| AIN -> EMN x0 (ascending excitation removed) | **partial**: the tonic ACh plus slow waves still carry the bolus (arrival 16-19 s against 14 s), so it still passes, only slower. A reminder that real gut also has propulsion from slow-wave-gated tone (gastric antrum) |

## Limits
8 segments of a 1-D plug-flow tube, no segmentation motility, no fluid, no ICC-to-muscle dynamics (the plateau is a fixed half sine triggered by the ICC spike). One neuron per type per segment.

Credits: Bayliss WM, Starling EH (1899) J Physiol 24:99; Costa M, Brookes SJH, Hennig GW (2000) Gut 47(Suppl 4):iv15; Furness JB (2012) Nat Rev Gastroenterol Hepatol 9:286; Sanders KM (1996) Gastroenterology 111:492; Huizinga JD, Lammers WJEP (2009) Am J Physiol Gastrointest Liver Physiol 296:G1; FitzHugh R (1961) Biophys J 1:445.
