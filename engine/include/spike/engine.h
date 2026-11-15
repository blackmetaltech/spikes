// spike engine: a closed-loop wrapper around the NEST kernel (C++ API, no Python at runtime).
// This header is the contract shared by the Godot extension, the task library and the tests.
// Keep it stable; extend by adding, not by changing signatures.
#pragma once
#include <cstdint>
#include <functional>
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

  RunStats stats() const;
  /// End the run (cleanup()). load() and destruction also do this.
  void stop();

private:
  struct Impl;
  Impl* impl_;
};

/// Kernel threads (OpenMP builds only). Call before load().
void set_threads( int n );

} // namespace spike
