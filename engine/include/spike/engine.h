// spike engine: a closed-loop wrapper around the NEST kernel (C++ API, no Python at runtime).
// This header is the contract shared by the Godot extension, the task library and the tests.
// Keep it stable; extend by adding, not by changing signatures.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace spike
{

/// One spike: the circuit-level neuron index (0..N-1 over all populations in file order) and its time in ms.
struct Spike
{
  int32_t neuron;
  double t_ms;
};

/// A named input channel declared in the circuit ("sensors"), driving a NEST stimulation device
/// (dc_generator: value = pA; poisson_generator: value = Hz; step-current etc. as documented in CIRCUIT-FORMAT.md).
/// A named output channel ("motors") reads spike counts of a set of neurons.
struct ChannelInfo
{
  std::string name;
  std::string kind;              // "dc", "poisson", ... for inputs; "rate" for outputs
  std::vector< int32_t > neurons; // circuit-level neuron indices it targets / reads
};

struct RunStats
{
  double t_ms = 0;          // model time reached
  uint64_t spikes = 0;      // total spikes so far
  uint64_t checksum = 0;    // running FNV-1a over (neuron, step) of every spike, in time order: golden traces use it
};


/// Additions (v0.4, add-only): circuit structure for tasks and the editor (docs/ENGINE-STRUCTURE.md, docs/CIRCUIT-FORMAT.md "Structure").
/// All numbers come from the live NEST connections (so rule projections, weight spread, set_weight() edits and plasticity are seen).
struct PopulationInfo
{
  std::string id, model;
  int32_t n = 0;
  int32_t first_index = 0;                    // circuit-level index of the population's first neuron
  int sign = 0;                               // +1 / -1 from the sign of the summed outgoing weights (sum of w over every outgoing connection); 0 if no outgoing connection or the sum is 0
  std::map< std::string, double > params;     // the numeric entries of the population's "params"
};

struct ProjectionInfo
{
  int32_t index = 0;                          // the `projection` argument of set_weight()
  std::string pre_pop, post_pop;              // population ids
  int32_t pre_index = -1, post_index = -1;    // positions in Structure::populations
  std::string rule;                           // NEST rule name, or "edges"
  std::string synapse_model;                  // "static_synapse" by default
  int32_t n_edges = 0;                        // connections pre -> post (a gap junction's mirror is not counted)
  double w_mean = 0, w_min = 0, w_max = 0;    // over the connections (0 when n_edges == 0)
  int sign = 0;                               // sign of w_mean (+1, -1, 0 when n_edges == 0 or w_mean == 0)
  double indegree_mean = 0;                   // connections per post neuron (over all post neurons of the population)
  int32_t indegree_max = 0;                   // most connections onto one post neuron
};

struct ChannelStructure
{
  std::string name, kind;                     // kind: "dc", "poisson", ... (inputs); "rate" / "modulator" (outputs)
  std::vector< std::string > pops;            // populations touched (target pops of an input, source pops of an output), file order, unique
  std::vector< int32_t > neurons;             // circuit-level neuron indices
};

struct Structure
{
  std::vector< PopulationInfo > populations;  // file order
  std::vector< ProjectionInfo > projections;  // file order
  std::vector< ChannelStructure > inputs, outputs;
  /// Position of a population id in `populations`, -1 if unknown.
  int population_index( const std::string& id ) const;
  const PopulationInfo* population( const std::string& id ) const;
};

class Engine
{
public:
  Engine();
  ~Engine();
  Engine( const Engine& ) = delete;
  Engine& operator=( const Engine& ) = delete;

  /// Build the network from circuit JSON (docs/CIRCUIT-FORMAT.md). Resets the NEST kernel first.
  /// Returns false and fills error() on any problem (schema, unknown model, bad parameter).
  bool load( const std::string& circuit_json );
  const std::string& error() const;

  /// Number of circuit neurons and their population / model, for the editor and scopes.
  int32_t neuron_count() const;
  std::string neuron_population( int32_t neuron ) const;
  std::vector< ChannelInfo > inputs() const;
  std::vector< ChannelInfo > outputs() const;

  /// Closed loop. Call set_input() any number of times, then step(ms). step() runs NEST for `ms`
  /// (a multiple of the resolution; the first call does prepare()). Spikes produced during the step are
  /// appended to an internal ring buffer by a custom NEST recording backend (no per-step copying of history).
  void set_input( const std::string& channel, double value );
  void step( double ms );
  /// Spikes since the last call (drains the buffer). Capacity is large; overflow sets dropped().
  std::vector< Spike > drain_spikes();
  uint64_t dropped() const;
  /// Spike count per output channel during the last step() (for motor readout), and the rate in Hz.
  double output_rate( const std::string& channel ) const;
  /// Additions (v0.1, add-only). output_rate() = output_count() * 1000 / step_ms is the summed population rate (Hz);
  /// divide by outputs()[i].neurons.size() for the per-neuron mean.
  double output_count( const std::string& channel ) const;
  /// Membrane potential samples of a watched neuron, one per step() (taken at the step's end), since the last call.
  std::vector< double > drain_voltage( int32_t neuron );
  /// True when the last step() completed (false after a failed step(); error() then says why; true again after the next
  /// successful step() or load()). Tasks and the game should stop and surface error() when this is false.
  bool last_step_ok() const;
  /// Resolution of the loaded circuit (ms); 0 when nothing is loaded.
  double resolution_ms() const;
  /// Wall time (us) spent inside NEST's run() during the last step(); step() wall time minus this is the engine overhead.
  double last_run_us() const;

  /// Membrane potential of a neuron (mV), via a multimeter the engine attaches on demand (watch()) or
  /// via get_status V_m if not watched. Watch a handful of neurons for scope traces.
  void watch( int32_t neuron );
  double voltage( int32_t neuron ) const;

  /// Change a synapse weight / a neuron parameter live (editor sliders). Takes effect from the next step.
  bool set_weight( int32_t projection, int32_t edge, double weight );
  bool set_neuron_param( int32_t neuron, const std::string& key, double value );

  /// Additions (v0.2, add-only): neuromodulators NE, ACh, DA (docs/CIRCUIT-FORMAT.md "Modulation").
  /// set_modulator() sets the externally driven level (arbitrary units, >= 0; typically 0..1) and returns immediately; the circuit's
  /// "modulation" table is re-applied to the affected neuron parameters at the start of the next step() (between steps, never inside one).
  /// modulator() is the total level as of the last step(): the external level plus every "modulator" output's release pool
  /// (spikes of a population -> concentration, first-order decay). Unknown names set error() and return false / 0.
  bool set_modulator( const std::string& name, double conc );
  double modulator( const std::string& name ) const;


  /// Additions (v0.4, add-only): circuit structure. structure() is a snapshot (recomputed when steps ran or set_weight() was called since the last call);
  /// empty when nothing is loaded. Only connections with a non-zero weight count as a path in the helpers below (a weight-0 projection is "cut").
  Structure structure() const;
  /// True when a chain of 1..max_hops projections leads from population `from` to `to` (max_hops <= 0: unlimited). from == to asks for a cycle.
  /// sign_filter: 0 = any projection, +1 = only projections with w_mean > 0, -1 = only w_mean < 0. Unknown ids give false.
  bool has_path( const std::string& from, const std::string& to, int max_hops = 0, int sign_filter = 0 ) const;
  /// True when `pop` lies on a cycle of at most max_hops projections (max_hops <= 0: unlimited) that contains at least one projection of sign `via_sign`
  /// (0 = any; -1 = the motor -> inhibitory -> motor loop: a cycle with an inhibitory projection). A self-projection is a 1-hop cycle.
  bool has_loop( const std::string& pop, int max_hops = 0, int via_sign = 0 ) const;
  /// True when `pop` lies on a cycle of at most max_hops projections that passes through population `via` (e.g. has_loop_via("MN", "RC", 2)).
  /// sign_filter as for has_path (applied to every projection of the cycle).
  bool has_loop_via( const std::string& pop, const std::string& via, int max_hops = 0, int sign_filter = 0 ) const;

  RunStats stats() const;
  /// End the run (cleanup()). load() and destruction also do this.
  void stop();

private:
  struct Impl;
  Impl* impl_;
};

/// The modulator names the engine accepts: "NE", "ACh", "DA".
const std::vector< std::string >& modulator_names();

/// Kernel threads (OpenMP builds only). Call before load().
void set_threads( int n );

} // namespace spike
