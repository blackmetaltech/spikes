# Roadmap 0.7: Memory (hippocampus and working memory)

Overview and shared rules: [ROADMAP.md](ROADMAP.md).

## Idea
A rat learns a place, a route and a goal, sleeps, and is better the next day. The player builds the circuits that make each step possible: a map (place and grid codes), a store (dentate gyrus and CA3), a clock (theta and sequences), consolidation (replay) and, last, memory that lasts seconds instead of days (working memory). The Theory wing's attractor chapter taught storage in the abstract; this wing puts it in an animal that has to find its way.

## What is new
- **World:** a rat on a linear track, then in a small square arena with a goal. The world gives position-tuned inputs (boundaries, landmarks), a velocity signal and a reward; the circuit's output steers.
- **Day and night:** one run has phases (explore, rest, test). Plastic weights carry across phases. In rest there is no sensory input and the modulator level changes, as acetylcholine does between waking and slow-wave sleep.
- **Instruments:** place-field map (firing rate against position, per cell); theta-phase plot (spike phase against position); replay raster (cells ordered by place field, so a replayed route is a diagonal); the state-space view from 0.6.

## Chapters
| Chapter | Levels | Concept | What is scored | Sources (verified when the level lands) |
|---|---|---|---|---|
| **Where am I** | `hp_place`, `hp_integrate`, task `hp_home` | place fields from convergent inputs; path integration on a continuous attractor driven by velocity | one compact field per cell that tiles the track; the position estimate follows the animal in the dark; return to the start without landmarks | O'Keefe and Dostrovsky 1971; Hafting 2005; McNaughton 2006; Burak and Fiete 2009 |
| **Storing** | `hp_separate`, `hp_complete`, task `hp_goal` | pattern separation by sparse expansion (dentate gyrus); pattern completion by recurrent collaterals (CA3); one-shot learning of a goal | two similar places get distinct codes; a partial cue recalls the whole place; one rewarded visit is enough to return | Marr 1971; Treves and Rolls 1994; Leutgeb 2007; Nakazawa 2002 |
| **Time** | `hp_theta`, `hp_precess`, task `hp_sequence` | a theta rhythm from a pacemaker and inhibition; phase precession; asymmetric plasticity turns a route into a sequence | a rhythm in band that paces the pyramidal cells; spikes come earlier in the cycle as the animal crosses the field; place fields shift backwards with laps | O'Keefe and Recce 1993; Skaggs 1996; Mehta 2000 |
| **Consolidation** | `hp_ripple`, `hp_replay`, boss `boss_day_night` | fast ripple oscillation from interneurons; replay of a learned sequence in rest; rest improves the next day's run | a brief fast oscillation on a burst; the day's route replays in order, compressed; the animal reaches the goal faster after rest than without it | Wilson and McNaughton 1994; Foster and Wilson 2006; Diba and Buzsaki 2007 |
| **Holding** (working memory) | `wm_bump`, `wm_silent`, task `wm_delay` | persistent activity; resistance to a distractor; activity-silent memory in facilitated synapses | a cue is held across a delay; a distractor does not move it; the memory survives a silent gap and is read out by a probe | Compte 2000; Mongillo 2008; Wang 2002 |

Diagnosis cases (probe-scored, as in the Bestiary): `dx_hp_separate` (dentate lesion: similar places confused), `dx_hp_complete` (CA3 recurrents cut: no recall from a partial cue), `dx_hp_theta` (pacemaker lesion: no rhythm, sequences lost), `dx_hp_replay` (no replay: no overnight gain). About 15 levels and 4 cases.

## Contested science, and how the levels handle it
| Topic | Live accounts | The level |
|---|---|---|
| Phase precession | dual oscillators, a depolarising ramp, network sweeps | scores precession; the reference uses one account; the notebook names the others |
| Grid cells | continuous attractor, oscillatory interference | `hp_integrate` is one-dimensional and attractor-based; grid fields in two dimensions are a notebook page, not a scored level |
| What replay is for | consolidation, planning, both | the boss scores the behavioural gain only |

## Engine and tool work (add-only)
| Need | Plan |
|---|---|
| The world | `body/arena`: position, heading, walls, goal, reward; deterministic; one-dimensional first |
| Position-tuned input | input kind `field_poisson`: Gaussian fields over position (the one-dimensional case reuses `tuned_poisson`) |
| Phases | a task schedule with named phases, per-phase input and modulator levels; weights persist; the score may compare phases |
| Plasticity | `stdp_synapse` and dopamine-gated learning are in the build; add a declared weight bound and a snapshot of weights per phase for the lab and the checks |
| Rhythm readout | output kind `lfp_proxy`: band-passed population rate, with phase; spike phase per cell |
| Place-field and replay analysis | `analysis/`: rate maps, field centre and width, sequence order score (rank correlation of replay order with field order) |

## Iterations
| # | Scope | Exit criteria | Status |
|---|---|---|---|
| M1 | Arena body, position-tuned input, place-field map; chapter "Where am I" | the honesty contract on 3 levels; the rat view in the interaction graph | planned |
| M2 | Chapter "Storing"; weight snapshots | separation and completion each fail when their mechanism is lesioned | planned |
| M3 | Rhythm readout, theta-phase plot; chapter "Time" | precession and backward shift measured by the engine and shown in the lab | planned |
| M4 | Phases, replay raster; chapter "Consolidation" and the boss | the boss fails with replay lesioned and passes with it, on held-out seeds | planned |
| M5 | Chapter "Holding", cases, map plate, wiki, release 0.7 | full CI; release | planned |

## Risks
| Risk | Answer |
|---|---|
| Learning levels are noisy across seeds (the 0.5 references that learn were the fragile ones) | held-out-seed tests from the first learning level; margins before release, never looser criteria |
| A day-and-night run is long | time compression in rest; Check stays within the budget the other bosses keep |
| Plasticity that the player cannot see is not a puzzle | weight snapshots and the place-field map make learning visible before and after |
