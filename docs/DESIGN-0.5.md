# 0.5 design: The Bestiary

**Status: shipped in 0.5.0, with changes.** What changed from the plan below:
- **Gated, not open from the start.** The Bestiary opens once the Invertebrates (chapter I) spine is passed. Inside it, Sea slug waits for the Heart level `m_neuromodulation`, Crab waits for the Leech spine and the Fly mushroom body waits for the Sea slug spine (`WING_GATES` in `godot/game/campaign.gd`). Guided mode drops the in-wing waits and keeps the chapter I gate. Every level card has "Needs" chips that name the earlier levels whose concepts it uses.
- **Chapter order** in the game is Worm, Leech, Sea slug, Fly compass, Fly mushroom body, Crab (the table below lists them in the planning order).
- **Diagnosis cases:** seven, not twelve: `dx_worm`, `dx_leech`, `dx_aplysia`, `dx_compass`, `dx_mb_apl`, `dx_mb_dan`, `dx_crab`. The compass and the worm have one case each. Diagnosis is scored on probes, with the candidate order independent of the answer.
- **The lamprey and the touch-reflex levels stayed in Invertebrates.** The Bestiary has only the new chapters (18 levels plus the 7 cases). `worm` (salt-seeking) stays in Invertebrates; the Bestiary worm chapter is `w_latch`, `w_switch`, `worm_touch`.
- **Real wiring:** only `worm_touch` (OpenWorm `cect`, Cook et al. 2019 counts). The fly compass and mushroom body are built from published cell types, not synapse-level data, and the crab is hand-entered. The fly connectome excerpt was never extracted (the hosts were unreachable), so the "excerpt" rows are not met.
- **Engine:** `tsodyks2_synapse` depression, modulator gating of a synapse parameter, dopamine-gated learning (`ProjectionInfo::dopamine_source`), the `population_vector` ring output, `spike_generator` and `sinusoidal_poisson` inputs and the `stg_cell` NESTML cell all shipped. Graded transmission stayed a spike-only approximation (said on the What-is-real page).
- **Bestiary map plates** are engraved (`tools/art/plates.py`), and the map has a wing selector (Body / Bestiary / Theory).
- Leech heart: reworked after 0.5.0 to read escape versus release from the response to a test current (`docs/circuits/bestiary_a.md`); only the release kind has a tested reference, see What-is-real. Not done: 0.6 species (locust, zebrafish, barn owl, songbird, cat, rat) and any real fly synapse data.

Research: `research/bestiary-a.md` (0.5 species), `research/bestiary-b.md` (0.6 species). Numbers tagged `[mem]`/`[M]` there are unverified and may not reach player text until checked.

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

## Answer strings of diagnosis cases and prediction bets (0.6)
A player's answer travels in the variant slot: `task->set_variant( "candidate" )`, `"candidate@N"` (N probes or instrument reads so far), `"candidate@N:W"` (W wrong guesses made before this one, charged to the score) and `"candidate@revealed"` (the player gave up; nothing is scored). An empty string means nothing submitted. `score()` can be read at any time. A diagnosis case (`level_kind` "diagnosis", `DxTask`) judges the candidate against a stored answer; value is 100 minus 15 per charged wrong guess minus 3 per probe beyond the first three, floor 25.

A **prediction bet** is a level whose `Task::Diagnosis` has `bet` true: the answer is committed BEFORE the experiment (the game offers no lesion probes until the player has committed) and the level is judged on what the run measures for the committed candidate, not on a stored answer (`Diagnosis::answer` is the expected one, and a test holds the run to it). `Diagnosis::budget` is the number of free instrument reads (the same formula, with this budget in place of three). A bet carries its commit in the same slot; the registry guard lets a bet (not only a diagnosis case) carry a variant string. Bets of the worm chapter: `a_motif`, `a_predict` (docs/circuits). A case may replace the generic paragraph about one dead connection with `DxSpec::method`.

