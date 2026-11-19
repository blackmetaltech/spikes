#pragma once
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_float64_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
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
  /// 0.5 (add-only): population_vector outputs (bump angle in radians [0, 2 pi) and strength 0..1 over the last step), and the spike-train inputs.
  double neuron_state( int64_t n, const String& key ) const;   // 0.6: any numeric NEST status entry of a neuron (NaN if unknown)
  double output_value( const String& name ) const;              // 0.6: mean rate of a rate_value output
  double input_weight( const String& name ) const;
  double input_value( const String& name ) const;
  Dictionary phase_plane( int64_t n, double i_ext ) const;     // 0.6: nullclines, fixed points, reset of a 2-D cell (spike/theory.h)
  double output_angle( const String& name ) const;
  double output_strength( const String& name ) const;
  /// Append spikes (absolute model time, ms, on the resolution grid, after the time reached) to a "spike_generator" input.
  bool queue_spikes( const String& name, const PackedFloat64Array& times_ms );
  /// "sinusoidal_poisson" input: key is "rate", "amplitude", "freq_hz" or "phase".
  bool set_input_param( const String& name, const String& key, double value );
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

  // ---- v0.2 (add-only): modulators and the analyses of bridge.h. All analysis results are JSON text ({"error": ...} on failure).
  bool set_modulator( const String& name, double conc );
  double modulator( const String& name ) const;
  static PackedStringArray modulator_names();
  /// Bursts of `neurons` (empty = pooled) in [t0_ms, t1_ms) from drained (neuron, t) pairs.
  static String bursts_json( const PackedFloat64Array& pairs, int64_t n, const PackedInt32Array& neurons, double t0_ms, double t1_ms );
  /// PRC from drained spike pairs and perturbation times (ms). merge_gap_ms > 0 collapses population spikes into one marker.
  static String prc_json( const PackedFloat64Array& pairs, int64_t n, const PackedInt32Array& neurons, const PackedFloat64Array& perturb_ms,
    double period_ms, double merge_gap_ms, int64_t nbins );
  /// PRC experiment on the loaded circuit (optional; it perturbs and advances the simulation).
  String prc_experiment_json( const String& channel, double baseline, double pulse_value, double pulse_ms, int64_t n_phases,
    const PackedInt32Array& neurons, double merge_gap_ms, int64_t repeats, int64_t nbins );
  static String hrv_json( const PackedFloat64Array& beat_times_ms );
  static String sta_json( const PackedFloat64Array& stimulus, double dt_ms, const PackedFloat64Array& spike_times_ms, double before_ms, double after_ms );   // 0.6: reverse correlation
  /// 4-node census; refuses (error JSON) when the work estimate exceeds max_work (0 = no limit).
  String motif4_json( int64_t sign, int64_t n_null, int64_t seed, double max_work ) const;
  static String motif4_json_edges( const PackedFloat64Array& triples, int64_t n, int64_t sign, int64_t n_null, int64_t seed, double max_work );

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
