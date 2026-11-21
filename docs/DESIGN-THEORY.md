# 0.6 design: the Theory wing

**Status: shipped in 0.5.0** (the wing was folded into the 0.5 release; there is no separate 0.6). All 12 levels, the four lab views, the rate-unit engine work and the spike-triggered-average tab are in. What changed from the plan below:
- **The wing is not open from the start.** Like the Bestiary it opens after the Invertebrates (chapter I) spine is passed; there is no further gate inside the Theory wing (`WING_GATES` has no Theory entry). The levels inside a chapter open in order.
- Every level card has "Needs" chips for the concepts it uses that another level teaches. Guided mode drops in-wing waits only (there are none in Theory).
- Star rules are those of the other levels: CHECK runs five worlds, star 3 is a per-level challenge, Guided caps at two stars.
- The Python package was not extended: it lacks the `rate_drive` and `tuned_poisson` input kinds, the `rate_value` output and the rate populations. A Theory circuit runs in the game and in `spike-run`, not in `spike.build`.
- Cheap-circuit rows for the wing are in `engine/tests/cheap_rows_f.inc`.

## Idea
A third campaign wing next to the body chapters and the Bestiary: four chapters that cover what a graduate computational-neuroscience course teaches and the game lacked. Each chapter keeps the format of the others: 2-3 micro-puzzles (one concept each), one scored task, references, notebook entries, glossary cards, What-is-real entries, a three-tier hint ladder and a recipe. The honesty contract of 0.4 is unchanged: the mechanism is checked through the circuit's structure, verdicts and hint tiers 1-3 carry no numbers, every level has a table of cheap circuits that must fail (`engine/tests/cheap_rows_f.inc`), the reference circuits pass on several seeds, and no check is weakened to make a level pass. The wing is open after the chapter I spine, like the Bestiary (planned as open from the start; see the status above).

## Chapters
| Chapter | Levels (micro, micro, task) | Concepts | Scored behaviour |
|---|---|---|---|
| Dynamics (`dynamics`) | `d_class`, `d_phasic`, `d_switch` | phase plane, excitability class (saddle-node vs Hopf onset), a stable fixed point that ends firing, bistability | f-I onset abrupt or continuous; fires at the change and then falls quiet; a brief pulse switches firing on and another off |
| Rate models (`rates`) | `r_oscillate`, `r_choose`, `r_meanfield` | Wilson-Cowan E-I loop, winner-take-all between rate units, mean field and its limits | a rate E-I pair that oscillates; one of two units wins and holds; a spiking E-I network whose mean rate agrees with a siegert mean-field prediction of the same network |
| Attractor memory (`memory`) | `h_complete`, `h_two`, `h_learn` | Hopfield attractor, pattern completion, spurious mixtures, plasticity as storage | a stored pattern is completed from a partial cue; two patterns do not blend; weights written by STDP during an exposure are enough for recall |
| Tuning and decoding (`tuning`) | `t_tuned`, `t_sharpen`, `t_decode` | orientation tuning from convergence, sharpening by lateral inhibition, the population vector | decoding error over random stimuli; the analysis tab adds a spike-triggered average of a white-noise current |

Every level is a scripted world on the Bestiary scaffolding (`engine/src/tasks/bestiary_a.h`: schedule of input levels, measurement windows, a structural mechanism check, a judge). The chapters are drawn with four new lab views (phase plane, rate strips, pattern grid, tuning ring) and the Analysis screen gets a reverse-correlation tab.

## Engine work (add-only, `engine.h`, `docs/CIRCUIT-FORMAT.md`)
| Need | Plan |
|---|---|
| Recovery variable of a 2-D cell | `Engine::neuron_state(neuron, key)` reads any numeric NEST status entry (`V_m`, `w`, `U_m`, `rate`, parameters), so the game and the tasks see `w` without a new recorder |
| Phase plane | `engine/include/spike/theory.h`: nullclines, fixed points with their type, reset and the onset type (saddle-node or Hopf, with the resonance frequency of rest) for `izhikevich`, `aeif_cond_exp`, `aeif_cond_alpha`, from the cell's live parameters; tested against the NEST cell settling on the fixed point |
| Rate neurons | NEST's own `lin_rate_ipn`, `tanh_rate_ipn`, `sigmoid_rate_gg_1998_ipn`, `threshold_lin_rate_ipn`, `gauss_rate_ipn` and `siegert_neuron` are in the pinned build already. They are accepted as population models, joined by `rate_connection_instantaneous` / `rate_connection_delayed` (siegert: `diffusion_connection`), kept off the spike recorder, driven by the new input kind `rate_drive` (writes `mu`, or `mean` for a siegert cell, between steps) and read by the new output kind `rate_value` (`Engine::output_value`) |
| Mean-field projection | a `diffusion_connection` takes the weight `J` (mV) and `K` (inputs per cell) of the spiking network it stands for; the loader computes NEST's `drift_factor = tau_m K J / 1000` and `diffusion_factor = tau_m K J^2 / 1000` (Hahne et al. 2017, eqs. 28-29) |
| Tuned inputs | input kind `tuned_poisson`: one Poisson generator per target with a von Mises tuning curve around the target's preferred angle; `set_input` sets the stimulus angle |
| Reverse correlation | `theory::spike_triggered_average` and an Analysis tab; the stimulus is a seeded Gaussian white-noise current the analysis writes into a dc input every millisecond |
| Dev aid | `spike-run <circuit> <ms> --sample key@neuron,out:name --every ms --set t:channel=value` prints state samples |

The Python package (`python/spike`) is not extended: the new kinds are engine-only for now (PyNEST parity is checked for the circuit features that existed before).

## What is simplified
* The phase plane is drawn for a constant current: the synaptic conductances of the aeif cells are not part of the picture, and only the first cell of the level's output is shown.
* Class I / class II is read from the resonance of the resting state at the point where it is lost (a damped oscillation of about 15 Hz or more means a Hopf-type onset). In the 2-D reset models the two classes shade into each other near a Bogdanov-Takens point (the default regular-spiking Izhikevich cell is of that kind), and the level says so rather than pretend a sharp line.
* The Wilson-Cowan units are NEST threshold-linear rate neurons with a ceiling (not a sigmoid, and without refractoriness); the drive is added to the unit's rate after the nonlinearity (NEST's `mu`), so inhibition cannot cancel it. An oscillation needs the excitation to be able to run away by itself (self-connection strong enough relative to the time constants: the linearised trace is positive) and the E-I loop to be strong enough to catch it (the determinant is positive) but not so strong that the excitation is pinned just above the drive; the inhibition does NOT have to be slower, a slower inhibition only lowers the threshold and the frequency. The verdict says which side of the band you are on. The mean-field task uses NEST's `siegert_neuron` for integrate-and-fire neurons with delta synapses, which is exact only for large, sparse, asynchronous networks. The level's network is large enough for it to hold, so it teaches the method and checks that J and K are carried over; it does not show where the prediction stops holding (that would need a failing regime, which the level does not have).
* The Hopfield levels are spiking networks of 16-32 cells: attractors here are states the network falls into, not the binary units of the 1982 model, and the storage capacity is far below the formal limit. A pattern is a set of cells; symmetry and the inhibition between patterns are judged on the actual edge list between cells (`Engine::connections()`), so an outer-product Hopfield wiring inside one population passes and a directed random graph is not called symmetric.
* `r_choose` runs two choices in one episode (A leads, then B wins and must be held too), so a feed-forward gain bias, which holds only its favourite, is not mistaken for a memory.
* STDP is NEST's `stdp_synapse` (excitatory only), so "inhibition between patterns" is fixed, not learned.
