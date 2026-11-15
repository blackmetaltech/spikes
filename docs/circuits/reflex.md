# Reference circuit: reflex arc (`circuits/reflex_arc.json`)

Task `reflex` (engine/src/tasks/reflex.cpp): a touch afferent fires 40 Hz of background noise and 440 Hz during each
200 ms poke; the motor pool must burst (>= 3 spikes in 25 ms) within 150 ms of every poke and must not burst otherwise.

## Wiring (15 neurons)
```
touch (poisson, 40 -> 440 Hz) --400 pA--> SN x8 --270 pA--> IN x4 --380 pA--> MN x3 --> withdraw
```
| Population | Model | Role |
|---|---|---|
| SN (8) | `iaf_psc_alpha` | low-threshold mechanoreceptor afferents. Each gets its own independent Poisson train |
| IN (4) | `iaf_psc_alpha` | excitatory interneurons; each sums all 8 afferents |
| MN (3) | `iaf_psc_alpha` | withdrawal motoneurons; each sums all 4 interneurons |

All neurons use the NEST defaults of `iaf_psc_alpha` (C_m 250 pF, tau_m 10 ms, E_L -70 mV, V_th -55 mV, V_reset -70 mV,
t_ref 2 ms, tau_syn 2 ms), a typical cortical/spinal membrane time constant (10-20 ms) and a 2 ms
absolute refractory period (Rotter & Diesmann 1999 for the exact integration scheme NEST uses). Synaptic delays are
1 ms (monosynaptic chemical delays are about 1-2 ms; Castellucci & Kandel 1974 for the Aplysia withdrawal arc,
Sherrington 1906 for the reflex-arc concept).

## Why it works (design rationale)
* **Noise rejection by convergence.** One afferent spike gives an EPSP of a few mV, far below the 15 mV gap to threshold.
  At 40 Hz background, 8 afferents give 320 spikes/s into the arc, so two or three of them regularly land within the
  10 ms membrane window, but the interneuron needs about four coincident afferent spikes, which noise almost never
  produces, whereas a poke (440 Hz per afferent, about 65 Hz output per afferent) delivers dozens per 10 ms.
* **Two layers add up.** Afferent -> interneuron -> motoneuron gives a second coincidence stage, so the motor pool
  fires a clean burst only when several interneurons fire together. The reference arc answers in about 17 ms (two
  synaptic delays of 1 ms, two membrane charging times, plus the 3-spike burst detector).
* **Parameter provenance.** Neuron constants are the NEST defaults of the model; the afferent gain (400 pA) and the two
  synaptic weights (270, 380 pA) were tuned in closed loop (a random search over ~65 candidates, then a mid-range
  rather than the fastest solution was picked). All six pokes are answered with 0 false triggers on 8 seeds (the task
  seed also reseeds NEST's Poisson noise), mean latency 17-18 ms (limit 60 ms). Margin: doubling IN->MN changes nothing,
  doubling SN->IN gives 1-2 false triggers on some seeds, and halving either weight still passes (22 ms).

## What breaks it (tests in `engine/tests/tasks_circuits_test.cpp`)
| Lesion | Result |
|---|---|
| SN -> IN weight x0 (cut the arc) | 6 of 6 pokes missed |
| IN -> MN weight x0.1 | 6 of 6 missed |
| SN -> IN x4 and IN -> MN x4 (no coincidence requirement left) | 19-21 false withdrawals from noise |

Credits: Sherrington CS (1906) *The Integrative Action of the Nervous System*; Castellucci V, Kandel ER (1974) PNAS
71:5004; Rotter S, Diesmann M (1999) Biol Cybern 81:381.
