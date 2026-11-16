#pragma once
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_float64_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <memory>
#include <string>
#include "spike/engine.h"

namespace godot
{
/// A NEST-backed spiking network for GDScript. The NEST kernel is a process-wide singleton (spike::Engine allows one per process), so all
/// SpikeSim objects share one engine: whichever called load_circuit() last owns it, and the others read as empty until they load again.
class SpikeSim : public RefCounted
{
  GDCLASS( SpikeSim, RefCounted )
public:
  SpikeSim();
  ~SpikeSim();
  bool load_circuit( const String& json );
  String error() const;
  void step( double ms );
  void set_input( const String& name, double value );
  /// Flat (neuron, t_ms, neuron, t_ms, ...) of the spikes since the last drain.
  PackedFloat64Array drain_spikes();
  double voltage( int64_t neuron ) const;
  void watch( int64_t neuron );
  double output_rate( const String& name ) const;
  int64_t neuron_count() const;
  Dictionary stats() const;
  // extras for the editor and scopes
  String neuron_population( int64_t neuron ) const;
  Array inputs() const;
  Array outputs() const;
  bool set_weight( int64_t projection, int64_t edge, double weight );
  bool set_neuron_param( int64_t neuron, const String& key, double value );
  int64_t dropped() const;
  void stop();
  /// 3-node motif census of the loaded circuit with z-scores vs degree-preserving nulls, as a JSON string (analysis.h).
  /// sign: 0 all, 1 excitatory only, 2 inhibitory only.
  String motif_json( int64_t sign, int64_t n_null, int64_t seed ) const;
  /// Census of an explicit graph: `triples` = flat (pre, post, weight) per edge over nodes 0..n-1. No loaded circuit needed.
  static String motif_json_edges( const PackedFloat64Array& triples, int64_t n, int64_t sign, int64_t n_null, int64_t seed );
  /// Spike-train statistics of a drained (neuron, t) pair array: rates, ISI CV, Golomb chi.
  static Dictionary spike_stats( const PackedFloat64Array& pairs, int64_t n, double t0_ms, double t1_ms );

  spike::Engine& engine() { return *engine_; }
  bool owns() const { return loaded_ && owner_ == this; }

protected:
  static void _bind_methods();

private:
  spike::Engine* engine_;
  bool loaded_ = false;
  static SpikeSim* owner_;
};
} // namespace godot
