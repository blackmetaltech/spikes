#pragma once
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <memory>
#include "spike/task.h"

namespace godot
{
class SpikeSim;
/// A task (body in a world) driving a SpikeSim.
class SpikeTask : public RefCounted
{
  GDCLASS( SpikeTask, RefCounted )
public:
  bool make( const String& id );
  void reset( int64_t seed );
  void step( const Ref< SpikeSim >& sim, double ms );
  /// False when the last engine step failed (Engine::last_step_ok).
  bool step_ok( const Ref< SpikeSim >& sim ) const;
  Dictionary body() const;
  Dictionary score() const;
  String brief() const;
  double field( double x, double y ) const;
  Rect2 bounds() const;
  String id() const;
  String title() const;
  double duration_ms() const;
  PackedStringArray sensor_channels() const;
  PackedStringArray motor_channels() const;
  String reference_circuit() const;
  // v0.2 (add-only): campaign grouping, renderer hint, par budget, perturbation timeline, judged bands, modulator channels
  String chapter() const;
  String view() const;
  int64_t par_neurons() const;
  Array timeline() const;       // [{t_ms, dur_ms, label}]
  Array bands() const;          // [{name, unit, lo, hi}]
  PackedStringArray modulators() const;
  /// {"inputs": {name: {kind, value}}, "outputs": {name: {kind, modulator, gain, tau_ms}}} from Task::channel_kinds() (names the task does not list are absent).
  Dictionary channel_kinds() const;
  // v0.2 engine bridges (static, add-only): the parts-bin card list, and the neuromodulator channels of a SpikeSim (engine.h set_modulator / modulator)
  static String catalog_json();
  static PackedStringArray modulator_names();
  static bool set_modulator( const Ref< SpikeSim >& sim, const String& name, double conc );
  static double modulator( const Ref< SpikeSim >& sim, const String& name );
  static PackedStringArray task_ids();
  static String reference_circuit_of( const String& id );

protected:
  static void _bind_methods();

private:
  std::unique_ptr< spike::Task > task_;
};
} // namespace godot
