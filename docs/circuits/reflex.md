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
* **Novice-reachable.** Touch -> 10 receptor cells -> 6 motor cells with links of 100-160 pA passes on 8 of 8 seeds (the recipe, link 140: latency 20-26 ms). Touch -> 5 cells -> 1 motor cell with 200 pA links passes 5 of 5 seeds in Standard (latency 34-57 ms); 3 cells at 400 pA and 5 cells at 300 pA now fail on one seed in five, because Standard allows no false withdrawal. Only the weight window is narrow: too weak and nothing fires ("withdraw never fired" verdict), too strong and noise fires it ("Too jumpy").

## What breaks it (tests in `engine/tests/tasks_circuits_test.cpp`)
| Lesion | Result |
|---|---|
| SN -> IN weight x0 (cut the arc) | 6 of 6 pokes missed |
| SN -> IN x2 | about 175 false withdrawals from noise |
| IN -> MN weight x0.1 | 6 of 6 missed |
| SN -> IN x4 and IN -> MN x4 (no coincidence requirement left) | about 175 false withdrawals from noise |

**Biology and limits.** The arrangement (weak sensory-to-motor connections made effective by summation) follows the Aplysia gill-withdrawal reflex (Castellucci & Kandel 1974); the figure of about 0.1 mV per afferent against about 10 mV needed to fire is from cat Ia afferents (Mendell & Henneman 1971), a stretch-reflex measurement used here only for the arithmetic. This is not a model of a mammalian touch reflex, which is polysynaptic (Sherrington 1910). The 150 Hz background is a game value: low-threshold cutaneous afferents are mostly silent at rest (Johnson 2001).

Credits: Sherrington CS (1906) *The Integrative Action of the Nervous System*; Sherrington CS (1910) J Physiol 40:28; Castellucci V, Kandel ER (1974) PNAS
71:5004; Mendell LM, Henneman E (1971) J Neurophysiol 34:171; Johnson KO (2001) Curr Opin Neurobiol 11:455; Rotter S, Diesmann M (1999) Biol Cybern 81:381.

## v0.3: concepts, modes, variants (item D)

* **Concepts:** none new; threshold, excitation, convergence used (the micro-puzzles `m_threshold`, `m_excitation` and `m_convergence` teach them first). Level kind `task`, on the spine.
* **Modes** (guided / standard / researcher): Poke contrast against the chatter: x1.67 guided, x0.8 researcher (200 Hz above 150); false withdrawals allowed 1 / 0 / 0 (a lone cell cannot pass: see Convergence is enforced); mean latency <= 75 / 60 / 51 ms. The structure of the pass test never changes; the reference circuit passes every mode (researcher on seeds 1-5; `tasks_modes_test`).
* **Variants** (optional branches, never weaken the base bar): half_neurons (par 8); noise_x2 (poke 125 Hz above the chatter: reference passes, latency 48-58 ms).
* **Hints:** three tiers from `score().parts` (concept, then the place: a channel or population id, then a partial recipe).
* **Quality metric:** `latency_ms`. **Notebook:** "Many weak inputs, one decision" (Mendell & Henneman 1971: one Ia afferent gives ~0.1 mV in a cat motoneuron that needs ~10 mV). The earlier entry cited Castellucci & Kandel 1974 (PNAS 71:5004) for interneurons summing weak afferents to beat noise; that paper is the quantal analysis of habituation of the gill-withdrawal reflex and does not show it.
* **Default part:** `iaf_psc_alpha` (`default_model()`); the lab's Poisson first connect (280 pA = 3.5 x 80) is declared as the `touch` weight.
* **Recipe** (`recipe()`, kept out of the brief): touch onto 10 cells, those onto 6 motor cells with a link of about 140 pA (the default 80 answers too slowly or misses, above about 170 the motor cells fire on noise), motor cells onto `withdraw`. Passes Guided, Standard and Researcher on seeds 1-5.
* **Convergence is enforced.** Standard allowed one false withdrawal, and one iaf cell tuned to a weight of about 280 pA passed 5 of 8 seeds ("Clean reflex ... 1 false withdrawals"). Now Standard and Researcher allow none and the verdict says "Clean" only when there were none; a single cell touch > N1 > withdraw swept over weights 200-315 pA never passes seeds 1, 2 or 4 (a few lucky seeds pass at 245-285 pA), so it cannot pass TEST x5. The 10 + 6 recipe and the 23-neuron reference pass on seeds 1-8. `tasks_physics_test` pins the rule with a stub (one stray burst fails Standard and Researcher, passes Guided).
