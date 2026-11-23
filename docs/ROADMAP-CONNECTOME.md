# Roadmap 0.6: The Atlas (connectomics)

Overview and shared rules: [ROADMAP.md](ROADMAP.md).

## Idea
Until now the player builds a circuit and the game checks it. In the Atlas the wiring is **real and given**, and the player does what a connectomics lab does with it: reads it, tests it against chance, predicts what a lesion will do, and supplies what an electron-microscope volume does not contain (signs, gains, time constants) until the circuit works. The wing's lesson, stated on its last page: a wiring diagram constrains a circuit's function and does not determine it.

## What is new
- **Locked-wiring levels** (`level_kind` "atlas"): the projections come from data and cannot be edited. The level declares its **free parameters** (per cell type: sign, weight scale, a time constant; or a short list of candidate edits). The mechanism check reads those parameters and the given structure.
- **Wiring matrix** in the lab: the adjacency matrix ordered by cell type, with rows and columns that can be sorted, grouped, and clicked through to the bench. Path finder between two cells. The existing motif census is shown against a null model.
- **Null-model bench**: shuffles that keep chosen things fixed (edge count; in- and out-degree; cell-type block structure), run by the engine, with the real count placed in the shuffled distribution.
- **Prediction bet** and **experiment budget** (see ROADMAP.md).
- **State-space view**: population activity projected on its leading principal components, trajectory drawn live. First used on the compass, where the bump traces a ring.

## Data
| Dataset | Source | Use | Reachable from the build machine | Notes |
|---|---|---|---|---|
| *C. elegans* hermaphrodite | Cook et al. 2019, via OpenWorm `cect` | worm chapter | yes (already used by `tools/connectome/worm.py`) | most worm neurons are graded, not spiking; the levels say so |
| Adult fly hemibrain v1.2.1 | Scheffer et al. 2020; public adjacency export (45 MB) | compass (EPG, PEN, PEG, Delta7), mushroom body (PN, KC, APL, MBON, DAN) | yes | published under CC BY 4.0; attribution on the level card |
| Adult fly whole brain (FlyWire) | Dorkenwald et al. 2024; Schlegel et al. 2024; transmitter predictions Eckstein et al. 2024 | feeding-circuit boss (excerpt) | Zenodo is reachable; terms to be read before use | the whole brain (about 139,000 neurons) is over the engine's limit; an excerpt is used |
| Larval fly | Winding et al. 2023 | optional whole-animal level (about 3,000 neurons) | not confirmed | depends on the scale benchmark in A1 |

`tools/connectome/*.py` download to a cache outside the repo, extract the excerpt, and write a small table plus a provenance record (dataset, version, terms, date, script, cell counts, what was dropped). Only the table and the record are committed.

## Chapters
| Chapter | Levels | Concept | What is scored | Sources (verified when the level lands) |
|---|---|---|---|---|
| **Reading wiring** (worm) | `a_path`, `a_motif`, `a_predict`, case `dx_a_worm` | from adjacency to pathway; motifs against a null model; hubs | test the path finder's shortest route by keeping it alone (in the model it does not carry the response: a route on paper is not a pathway that works; the level scores the route and tells the run, not "pathway confirmed"); pick the null model under which the feed-forward loop is over-represented (a bet; the engine computes the z-scores); bet on which one interneuron class can be silenced without losing the backward response, then run it; find which class of four is silenced by recording | Cook 2019; Varshney 2011; Milo 2002; Chalfie 1985 |
| **A ring from real wiring** (fly compass) | `a_signs`, `a_shift`, task `a_compass`, case `dx_a_compass` | connectome-constrained modelling; cell types; the same idea as the Bestiary compass, now with measured wiring | assign signs and gains per cell type so the real EPG, PEN and Delta7 wiring holds one bump; find which cell type shifts it; hold a heading in the dark and turn with the fly | Scheffer 2020; Turner-Evans 2020; Hulse 2021; Seelig and Jayaraman 2015 |
| **Random or designed?** (mushroom body) | `a_random`, `a_sparse`, task `a_odour` | convergence statistics; sparse coding from real fan-in and feedback inhibition | test whether projection-neuron-to-Kenyon-cell wiring differs from a degree-preserving shuffle; set the inhibitory feedback so the real wiring gives a sparse code; discriminate two similar odours | Caron 2013; Li 2020; Lin 2014 |
| **Same wiring, different dynamics** | `a_degenerate`, case `dx_a_twins` | degeneracy; what a connectome leaves out | two circuits with identical wiring behave differently: find the recording that tells them apart within the experiment budget | Bargmann and Marder 2013; Prinz 2004 |
| **Boss: a brain predicts a behaviour** | `boss_atlas_feeding` | whole-brain modelling from wiring and predicted transmitters | on an excerpt of the sugar-to-proboscis pathway, predict which cell types are needed for the motor output, bet, then test by silencing | Shiu 2024; Dorkenwald 2024; Eckstein 2024 |

**Worm chapter, as built.** `a_path` no longer scores "shortest pathway confirmed": in the model every shortest route kept alone leaves the backward motor neurons silent, so it scores the route and tells what the run showed. `a_motif` and `a_predict` are bets, decided by what the engine measures on the given circuit (research/atlas-a.md 4b); both are `level_kind` "diagnosis" (decided by the commit; the engine applies the lesion itself).

About 12 levels and 3 cases. The compass and mushroom-body chapters replace nothing: the Bestiary levels teach the idea on a hand-built circuit, the Atlas levels meet the measured one.

## Engine and tool work (add-only)
| Need | Plan |
|---|---|
| Large explicit wiring | the circuit format's `edges` lists already carry it; measure load and step time at 500, 3,000 and 10,000 neurons, and set the Atlas size cap from the result |
| Locked wiring and free parameters | a `locked` flag per projection and a `free` table in the level; the editor shows locked projections as engraved, not inked; the reference lock ignores locked parts |
| Cell-type parameters | a per-population-group scale and sign applied at load, so one control changes every projection out of a type |
| Null models | `analysis/`: degree-preserving edge swap, block-preserving shuffle; motif census z-scores |
| Graph queries | shortest path, reachability and hub measures exposed to tasks and the lab |
| State space | `analysis/`: incremental PCA over binned population rates; projection of live activity |
| Prediction bet | task contract: a declared question with options, committed before the run; scored like a diagnosis answer with probes |

## Iterations
| # | Scope | Exit criteria | Status |
|---|---|---|---|
| A1 | Hemibrain extraction script with compass and mushroom-body tables and provenance; scale benchmark; `locked` and `free` in the format, engine and editor; one throwaway level proving the loop | tables reproduce from the script; benchmark numbers in this file; tests for locked wiring; CI green | done (2026-11-23) |
| A2 | Worm chapter; wiring matrix; null-model bench; prediction bet | 3 levels and 1 case pass the honesty contract; the matrix and bench are in the interaction graph | engine side built (`a_path`, `a_motif`, `a_predict`, `dx_a_worm`, chapter `a_worm`; research/atlas-a.md 4b), game side next |
| A3 | Compass chapter; state-space view | real wiring holds a bump with the reference's signs and gains on seeds 101 to 105; a wrong sign assignment fails | planned |
| A4 | Mushroom-body chapter | the real fan-in statistics are reported with the null model; sparse code scored | planned |
| A5 | Degeneracy chapter, feeding boss, Atlas map plate, wiki, release 0.6 | full CI; CHANGELOG; release | planned |

## Benchmark (A1: scale)
`python3 tools/bench/scale.py` builds random circuits with explicit `edges` projections and runs them through `build/engine/spike-run`: 80 % excitatory and 20 % inhibitory `iaf_psc_alpha` cells, in-degree 50 (40 from excitatory, 10 from inhibitory cells), delay 1.5 ms, a Poisson drive (3,000 Hz at 18.5 pA to every cell) that holds the cells at about 4 Hz, resolution 0.1 ms, one engine thread. Load time is the wall time of a 1 ms run (process start, NEST start, circuit build). Time per simulated second is a 2 s run minus the load time. Best of 3 runs. Output of the script, 2026-11-22.

Machine: AMD Ryzen 9 9900X (12 cores, 24 logical CPUs), load average about 10 while the runs were made (other jobs were running, so these numbers are pessimistic). The NEST in this build does not support multiple threads (`--threads 4` is refused), and the game does not set a thread count, so one thread is what the game gets.

| cells | connections | load (s) | wall seconds per simulated second | real-time factor | mean rate (Hz) |
|---|---|---|---|---|---|
| 500 | 25,000 | 0.05 | 0.23 | 4.3 | 3.7 |
| 1,000 | 50,000 | 0.09 | 0.47 | 2.1 | 3.8 |
| 1,500 | 75,000 | 0.12 | 0.69 | 1.4 | 3.9 |
| 2,000 | 100,000 | 0.16 | 0.92 | 1.1 | 4.1 |
| 3,000 | 150,000 | 0.23 | 1.45 | 0.69 | 3.9 |
| 10,000 | 500,000 | 0.72 | 4.99 | 0.20 | 4.0 |

Real-time factor is simulated seconds per wall second; below 1 the lab view runs slower than the clock. The cost grows in proportion to the number of cells. It is not the spikes: 10,000 cells with almost no drive (0.01 Hz, silent) took about 3.8 s per simulated second, against 5.0 s at 4 Hz. Most of the time is the per-step update of every cell at 0.1 ms. One quick, single run of the 10,000-cell circuit at `resolution_ms` 0.2 gave about half the time, and at 0.5 about a third; this was not repeated and the levels' timing needs 0.1 ms, so it is a lead, not a result.

**Recommendation.** Cap an Atlas level at **2,000 cells** (about 100,000 connections) for the live lab view: it runs at about real time (1.1) on this loaded machine and faster on an idle one. Up to 1,000 cells it runs at 2 times real time or more, which suits levels that are played with sliders. 3,000 cells (0.7 times real time) is acceptable only for a batch check or a run that shows a progress bar; 10,000 cells (0.2) is not interactive. Excerpts of the compass (148 cells) and the mushroom body (about 2,200 cells with all Kenyon cells, about 400 with a subset) fit; the larval fly (about 3,000 neurons) and the whole-brain excerpt need a smaller excerpt or a non-live run.

## Risks
| Risk | Answer |
|---|---|
| Real wiring with spiking point neurons may not hold a bump or a sparse code without tuning | A1 and A3 test this first; if it needs a hand-set parameter, the level says which and why |
| Scale: thousands of neurons on a laptop | the cap comes from the A1 benchmark; excerpts are sized to it |
| Worm neurons are graded | said on the card; the worm chapter scores wiring questions more than dynamics |
| A locked level is less of a game | the bet, the budget and the null-model duel carry the play; A1's throwaway level tests whether it is fun before three chapters are built on it |
