// Circuit JSON (docs/CIRCUIT-FORMAT.md) parsed and validated into plain structs. No NEST dependency.
//
// Creation / connection order used by the engine (a PyNEST re-implementation must follow it to get identical
// node IDs and RNG streams): (1) populations in file order (NEST node ids 1..N, neuron i = node id i+1),
// (2) one device per input in file order, (3) one spike_recorder; then connect(): projections in file order,
// inputs in file order (weight = input.weight, delay = input.delay or resolution_ms), recorder (weight 1,
// delay = resolution_ms). With stdp_dopamine_synapse projections: after the recorder, one volume_transmitter per distinct
// dopamine_source (order of first use), then a poisson_generator and a parrot_neuron (the DA level chain); see docs/CIRCUIT-FORMAT.md.
#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "nlohmann/json.hpp"

namespace spike
{

struct PopSpec
{
  std::string id, model, role;
  int32_t n = 0;
  int32_t first = 0;  // circuit-level index of the population's first neuron
  nlohmann::json params = nlohmann::json::object();
};

struct ProjSpec
{
  std::string pre, post;
  int pre_pop = -1, post_pop = -1;
  bool has_edges = false;
  nlohmann::json rule = nlohmann::json::object();     // rule + rule parameters
  nlohmann::json synapse = nlohmann::json::object();  // syn_spec
  std::vector< std::array< double, 2 > > edge_wd;     // weight, delay per edge
  std::vector< std::pair< int32_t, int32_t > > edges; // pre_idx, post_idx within the populations
  // Synapse-model handling resolved by the parser (docs/CIRCUIT-FORMAT.md "Synapse models"):
  bool gap = false;                  // gap_junction: instantaneous, symmetric (rule: make_symmetric; edges: mirrored by the engine)
  bool dopamine = false;             // stdp_dopamine_synapse: needs a volume_transmitter fed by `dopamine_source`
  int dopamine_pop = -1;             // population index of "dopamine_source"
  nlohmann::json common = nlohmann::json::object();  // stdp_dopamine common properties (A_plus, Wmax, ...): set on a model copy, not per connection
};

struct InputSpec
{
  std::string name, kind;
  std::vector< int32_t > targets;  // circuit-level neuron indices (sorted, unique)
  double weight = 1.0, value = 0.0;
  double delay = -1.0;             // < 0: use resolution
};

struct OutputSpec
{
  std::string name, kind;                // kind: "rate" | "modulator"
  std::vector< int32_t > neurons;
  std::string modulator;                 // kind "modulator": "NE" | "ACh" | "DA"
  double gain = 0.05, tau_ms = 1000.0;   // kind "modulator": concentration added per spike (a.u.), decay time constant (ms)
};

/// One row of the modulation table: modulator concentration -> a parameter of every neuron of one population.
///   value = value + gain * conc          (additive; "gain")
///   value = value * (1 + scale_per_unit * conc)   (multiplicative; "scale_per_unit")
/// starting from the parameter's value after the population's `params` are applied; clamped to [min, max] if given.
struct ModulationSpec
{
  std::string modulator, pop, param;
  int pop_index = -1;
  bool scale = false;                    // true: scale_per_unit, false: gain
  double amount = 0.0;
  bool has_min = false, has_max = false;
  double vmin = 0.0, vmax = 0.0;
};

/// Modulator names the engine knows (Engine::set_modulator).
bool is_modulator_name( const std::string& s );

struct CircuitSpec
{
  std::string name;
  double resolution_ms = 0.1;
  uint64_t seed = 0;
  std::vector< PopSpec > pops;
  std::vector< ProjSpec > projs;
  std::vector< InputSpec > inputs;
  std::vector< OutputSpec > outputs;
  std::vector< ModulationSpec > modulation;
  bool record_all = true;
  std::vector< int32_t > record_neurons;  // when !record_all
  std::vector< int32_t > record_voltage;
  int32_t n_neurons = 0;
};

/// Parse and validate. On failure returns false and sets `error` to "path.to.field: message".
bool parse_circuit( const std::string& text, CircuitSpec& out, std::string& error );

} // namespace spike
