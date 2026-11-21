# Engine::structure() (v0.4, add-only)

Declared in `engine/include/spike/engine.h`. Tasks get `Engine&` in `step()`; call these in `score()` (or any time). Everything is read from the live NEST connections, so rule projections, weight spread, `set_weight()` lesions and plasticity are all seen. The snapshot is cached and recomputed only after a `step()` or `set_weight()`.

```cpp
struct PopulationInfo   { std::string id, model; int32_t n, first_index; int sign;            // sign: +1/-1 from the summed outgoing weights, 0 if none / zero
                          std::map<std::string,double> params; };                           // numeric entries of the population's "params"
struct ProjectionInfo   { int32_t index;                                                     // = set_weight() projection number
                          std::string pre_pop, post_pop; int32_t pre_index, post_index;      // ids and positions in Structure::populations
                          std::string rule;                                                  // NEST rule name, or "edges"
                          std::string synapse_model;
                          int32_t n_edges; double w_mean, w_min, w_max; int sign;            // sign of w_mean (0 when n_edges == 0 or w_mean == 0)
                          double indegree_mean; int32_t indegree_max; };                     // connections per post neuron (convergence)
struct ChannelStructure { std::string name, kind; std::vector<std::string> pops; std::vector<int32_t> neurons; };
                          // kind: inputs "dc" | "poisson" | "spike_generator" | "sinusoidal_poisson" (0.5); outputs "rate" | "modulator" | "population_vector" (0.5).
                          // neurons: for a population_vector, in ring order (neuron k of N at angle 2 pi k / N), not sorted
struct Structure        { std::vector<PopulationInfo> populations; std::vector<ProjectionInfo> projections;
                          std::vector<ChannelStructure> inputs, outputs;                     // pops = target pops (inputs) / source pops (outputs)
                          int population_index(const std::string& id) const;                 // -1 if unknown
                          const PopulationInfo* population(const std::string& id) const; };

Structure Engine::structure() const;                       // empty (no populations) when nothing is loaded
bool Engine::has_path(from, to, int max_hops = 0, int sign_filter = 0) const;
bool Engine::has_loop(pop, int max_hops = 0, int via_sign = 0) const;
bool Engine::has_loop_via(pop, via, int max_hops = 0, int sign_filter = 0) const;
```

Helper semantics (population ids, not neuron indices):

- Only projections with `n_edges > 0` and `sign != 0` count (a projection whose weights are all 0, i.e. a lesion, is cut).
- `has_path(a, b, max_hops, sign_filter)`: a chain of 1..max_hops projections from a to b; `max_hops <= 0` = unlimited; `sign_filter` 0 any, +1 only excitatory, -1 only inhibitory projections (sign of `w_mean`). `a == b` asks for a cycle.
- `has_loop(pop, max_hops, via_sign)`: `pop` lies on a cycle of at most max_hops projections that contains at least one projection of sign `via_sign` (0 = any). Motor -> inhibitory -> motor: `has_loop("MN", 2, -1)`. A self-projection is a 1-hop cycle.
- `has_loop_via(pop, via, max_hops, sign_filter)`: a cycle through `via`, e.g. `has_loop_via("MN", "RC", 2)` (the sign filter applies to every projection of the cycle; use `has_path` for mixed-sign legs: `has_path("MN","RC",1,+1) && has_path("RC","MN",1,-1)`).
- Reflex convergence (>= 2 sensory cells onto a motor pool): `projections[k].indegree_max >= 2` for the projection onto the motor pop, or sum `n` of the source pops with a path of 1 hop to it.

Example:

```cpp
auto st = e.structure();
const auto* mn = st.population("MN");
if (!e.has_loop("MN", 2, -1)) verdict = "No inhibitory feedback loop onto the motor pool: nothing is turning the output back down.";
```

Tests: `engine/tests/test_structure.cpp` (new channel kinds: `test_spike_inputs.cpp`, `test_population_vector.cpp`). Format notes: `docs/CIRCUIT-FORMAT.md` ("Structure").

## Additions in 0.5 (add-only)
`ChannelStructure::modulator`: for an output of kind `modulator`, the engine channel it releases (`NE`, `ACh`, `DA`). `Structure::modulation`: the circuit's `modulation` table as `ModulationInfo { modulator, pop, param, pop_index, proj_index, amount, scale }` (exactly one of `pop_index`, `proj_index` is >= 0), so a task can ask whether a modulator reaches a synapse on a given path (`aplysia_gill`, `a_facilitate`).

0.5, add-only: `ProjectionInfo::dopamine_index` / `dopamine_source` give the population that drives a `stdp_dopamine_synapse` projection (-1 / empty otherwise; `synapse_model` is the private copy `stdp_dopamine_synapse__p<k>`). `Engine::output_spiked(channel)` lists the circuit-level neurons of an output that spiked in the last step (one entry per spike), so a task can count distinct active cells.

## Additions in 0.6 (add-only): the edge list
`Engine::connections()` returns every live connection with a non-zero weight as `Engine::Connection { pre, post, weight, projection, j, k }` (circuit-level neuron indices; `j` and `k` are the J and K of a `diffusion_connection`). The memory levels judge who connects to whom on it (symmetry of the weights, recurrent excitation inside a pattern, inhibition between two sets of cells), so the answer does not depend on how the player split the cells into populations: an outer-product Hopfield wiring inside one population is judged like two populations, and a directed random graph is not symmetric. `r_meanfield` compares the J and K of the siegert projections with those of the spiking ones.
