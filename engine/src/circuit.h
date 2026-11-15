// Circuit JSON (docs/CIRCUIT-FORMAT.md) parsed and validated into plain structs. No NEST dependency.
//
// Creation / connection order used by the engine (a PyNEST re-implementation must follow it to get identical
// node IDs and RNG streams): (1) populations in file order (NEST node ids 1..N, neuron i = node id i+1),
// (2) one device per input in file order, (3) one spike_recorder; then connect(): projections in file order,
// inputs in file order (weight = input.weight, delay = input.delay or resolution_ms), recorder (weight 1,
// delay = resolution_ms).
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
  std::string name, kind;
  std::vector< int32_t > neurons;
};

struct CircuitSpec
{
  std::string name;
  double resolution_ms = 0.1;
  uint64_t seed = 0;
  std::vector< PopSpec > pops;
  std::vector< ProjSpec > projs;
  std::vector< InputSpec > inputs;
  std::vector< OutputSpec > outputs;
  bool record_all = true;
  std::vector< int32_t > record_neurons;  // when !record_all
  std::vector< int32_t > record_voltage;
  int32_t n_neurons = 0;
};

/// Parse and validate. On failure returns false and sets `error` to "path.to.field: message".
bool parse_circuit( const std::string& text, CircuitSpec& out, std::string& error );

} // namespace spike
