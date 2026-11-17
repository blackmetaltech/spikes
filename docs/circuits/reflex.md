# Reference circuit: reflex arc (`circuits/reflex_arc.json`)

Task `reflex` (engine/src/tasks/reflex.cpp): a touch afferent fires 150 Hz of background noise and 400 Hz during each
200 ms poke (only 2.7x more). The motor pool must burst within 150 ms of every poke (the hit window; the pass bar is a
*mean* latency of 60 ms) and must not burst otherwise. A burst is >= max(3, ceil(N/2)) spikes in 25 ms, N = motor pool size,
so a big pool cannot cheat with the absolute count of 3.

## Wiring (23 neurons)
```
touch (poisson, 150 -> 400 Hz) --400 pA--> SN x16 --55 pA--> IN x4 --380 pA--> MN x3 --> withdraw
```
| Population | Model | Role |
|---|---|---|
| SN (16) | `iaf_psc_alpha` | low-threshold mechanoreceptor afferents. Each gets its own independent Poisson train |
| IN (4) | `iaf_psc_alpha` | excitatory interneurons; each sums all 16 afferents |
| MN (3) | `iaf_psc_alpha` | withdrawal motoneurons; each sums all 4 interneurons |

All populations pin `V_m` = -70 mV (= E_L) explicitly, so the result does not depend on any random initial V_m the engine may draw.
Other constants are the NEST defaults of `iaf_psc_alpha` (C_m 250 pF, tau_m 10 ms, E_L -70 mV, V_th -55 mV, t_ref 2 ms,
tau_syn 2 ms). Delays 1 ms (Castellucci & Kandel 1974; Sherrington 1906; Rotter & Diesmann 1999 for the integration scheme).

## Why it works (design rationale)
* **One cell cannot do it.** With a 150 Hz background a single cell sees about 1.5 spikes per membrane window by chance and
  about 4 during a poke; Poisson bunching makes any weight either miss pokes or fire on noise. Tested: touch -> one
  motor cell, weights 150-1000 pA, 3 seeds each: never a pass (misses at low weight, 3-100 false withdrawals at high).
* **Convergence averages the noise.** 16 independent afferents summed in each interneuron separate 150 from 400 Hz by
  several standard deviations; a second layer (4 interneurons -> 3 motoneurons) makes the burst clean. Reference latency
  23-25 ms.
* **Parameter provenance.** The afferent gain (400 pA) and weights (55 and 380 pA) come from a closed-loop grid over
  SN count, SN->IN and IN->MN weight, scored with the task itself. Margin on 6 seeds: SN->IN weight x0.7 and x1.4 still
  pass on most seeds (x1.5-2 gives false withdrawals, x0.6 misses), IN->MN x0.6 to x2 passes.
* **Novice-reachable.** Touch -> 3 receptor cells -> 1 motor cell with 400 pA links passes on 5 of 5 seeds; touch -> 5 cells ->
  1 motor cell passes with links of 200-300 pA. Only the weight window is narrow: too weak and nothing fires ("withdraw
  never fired" verdict), too strong and noise fires it ("Too jumpy").

## What breaks it (tests in `engine/tests/tasks_circuits_test.cpp`)
| Lesion | Result |
|---|---|
| SN -> IN weight x0 (cut the arc) | 6 of 6 pokes missed |
| SN -> IN x2 | about 175 false withdrawals from noise |
| IN -> MN weight x0.1 | 6 of 6 missed |
| SN -> IN x4 and IN -> MN x4 (no coincidence requirement left) | about 175 false withdrawals from noise |

Credits: Sherrington CS (1906) *The Integrative Action of the Nervous System*; Castellucci V, Kandel ER (1974) PNAS
71:5004; Rotter S, Diesmann M (1999) Biol Cybern 81:381.

## v0.3: concepts, modes, variants (item D)

* **Concepts:** threshold, excitation, convergence (new); none used. Level kind `task`, on the spine.
* **Modes** (guided / standard / researcher): Poke contrast against the chatter: x1.67 guided, x0.8 researcher (200 Hz above 150); false withdrawals allowed 2 / 1 / 0; mean latency <= 75 / 60 / 51 ms. The structure of the pass test never changes; the reference circuit passes every mode (researcher on seeds 1-5; `tasks_modes_test`).
* **Variants** (optional branches, never weaken the base bar): half_neurons (par 8); noise_x2 (poke 125 Hz above the chatter: reference passes, latency 48-58 ms).
* **Hints:** three tiers from `score().parts` (concept, then the place: a channel or population id, then a partial recipe).
* **Quality metric:** `latency_ms`. **Notebook:** Castellucci & Kandel 1974.
