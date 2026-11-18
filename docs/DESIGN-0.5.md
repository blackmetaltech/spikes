# 0.5 design: The Bestiary

Research: `docs/research/bestiary-a.md` (0.5 species), `docs/research/bestiary-b.md` (0.6 species). Numbers tagged `[mem]`/`[M]` there are unverified and may not reach player text until checked.

## Idea
A new campaign wing: one chapter per model organism, each built around the circuit comp-neuro knows best. Each chapter = 2-3 micro-puzzles (one concept each), one task (the organism's behaviour, scored), and 1-2 diagnosis cases on that organism's circuit. The body chapters stay; the Bestiary is a second map page drawn as a natural-history plate (one engraved animal per chapter).

Diagnosis becomes a second core loop here: cases are scored on probes used (scopes watched, lesions tried), not just the right answer.

## 0.5 chapters
| Chapter | Task (scored behaviour) | Concepts | Diagnosis cases | Real wiring |
|---|---|---|---|---|
| Worm (*C. elegans*) | Touch head -> reverse, touch tail -> forward; command switch | mutual inhibition, command neurons, graded-ish transmission | AVA ablated (no reversal), PLM cut (tail touch ignored) | Yes: touch circuit (~14 cells) from OpenWorm `cect` (MIT), weights from synapse counts |
| Leech | Heartbeat: two sides alternate, period in band | half-centre oscillator, post-inhibitory rebound, escape vs release | one HN pair lesioned (side stops / synchronous) | No (from Calabrese papers) |
| Sea slug (*Aplysia*) | Gill withdrawal habituates to repeated touch, recovers, sensitises after tail shock | short-term depression, heterosynaptic facilitation | facilitator interneuron cut (no sensitisation), depression blocked (no habituation) | No |
| Crab (STG) | Pyloric rhythm: PD -> LP -> PY phase order and period | pacemaker kernel, rebound phase, degeneracy | three different lesions that look alike; probes needed to tell them apart | Yes (fully mapped, ~14 cells; hand-entered from Marder lab diagrams) |
| Fly: mushroom body | Learn to avoid one odour after pairing with punishment | sparse coding (KC + APL), dopamine-gated plasticity | APL removed (dense code, no discrimination), DAN cut (no learning) | Excerpt (needs off-machine extraction, see below) |
| Fly: compass | Hold heading in the dark, rotate bump with turning | ring attractor, path integration | P-EN cut one side (bump drifts), delta7 lesion (bump spreads) | Excerpt (same) |

Lamprey swimmer and worm chemotaxis (existing levels) move into the Bestiary as the lamprey and worm openers. 0.6: locust, zebrafish, barn owl, songbird, cat, rat (`bestiary-b.md`).

## Engine work (add-only)
| Need | For | Plan |
|---|---|---|
| Synaptic depression with recovery | Aplysia | `tsodyks2_synapse` already in NEST; expose in the circuit format if not already allowed |
| Modulator gating a synapse (U or weight) | Aplysia sensitisation | extend the `modulation` table to target projection params (applied between steps, like neuron params) |
| Dopamine-gated learning | Fly MB | try `stdp_dopamine_synapse` with depressing sign first; if it can't express KC x DAN depression, a NESTML synapse |
| Graded transmission | Worm, leech | spike-only approximation in 0.5 (said so on the What-is-real page); revisit with a NESTML graded synapse |
| Bursting pacemaker | Crab STG | NESTML conductance cell (CaT/CaS/A/H/KCa, Prinz-style); a small fixed set of parameter sets, not a sweep |
| Ring readout | Fly compass | new output kind `population_vector` (bump angle from rates of an ordered population) |
| Spike-train inputs | (0.6 owl; cheap to add now) | input kinds `spike_generator`, `sinusoidal_poisson` in circuit JSON |

## Connectome excerpts
- **Worm:** `cect` from PyPI (reachable). A script `tools/connectome/worm.py` pulls the touch circuit, writes circuit JSON with weight = synapse count x scale, and records the source dataset and version. Committed output is the small JSON only.
- **Fly:** FlyWire/neuPrint hosts are blocked from this machine. Options: (a) extract once elsewhere and commit the small tables, with a script that reproduces them; (b) build from published cell-type counts and wiring diagrams (no synapse-level data). 0.5 does (b), with (a) when the tables exist.
- Every excerpt names its source, version, licence and what was simplified, on the level brief and the What-is-real page.

## Honesty contract
Unchanged from 0.4: mechanism checks via structure, no numbers in verdicts and tier-1-3 hints, a cheap-circuit table per level, a non-circular novice test, citations verified with PMID/DOI before shipping.

## Staging
1. Engine: tsodyks2, synapse modulation, spike-train inputs, population_vector output. Tests.
2. Worm + leech + Aplysia chapters (no new cell models needed).
3. Crab (NESTML STG cell) and fly MB/compass.
4. Diagnosis scoring on probes; Bestiary map plate.
5. Release check, CHANGELOG, tag.
