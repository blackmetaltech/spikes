#pragma once
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
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
  // v0.3 (add-only): see engine/include/spike/task.h and docs/DESIGN-0.3.md
  String level_kind() const;                 // "micro" | "task" | "boss" | "diagnosis"
  PackedStringArray concepts_new() const;
  PackedStringArray concepts_used() const;
  bool on_spine() const;
  void set_mode( const String& mode );       // before reset(): "guided" | "standard" | "researcher"
  String mode() const;
  Dictionary variants() const;               // {id: description}
  void set_variant( const String& v );       // "" = base task; before reset()
  String variant() const;
  Array hints() const;                       // [{tier:int, text:String, target:String}]
  Dictionary metrics() const;                // {name: {value:float, unit:String, lower_is_better:bool}}
  double next_event_ms( double t_ms ) const; // < 0 if none
  Dictionary diagnosis() const;              // {circuit, symptoms, candidates:PackedStringArray, answer}
  Dictionary notebook() const;               // {title, text, citation}
  String default_model() const;              // catalog card id the lab selects by default ("" = lab default)
  String recipe() const;                     // concrete numbers kept apart from the brief ("" = none)
  // v0.3 instant check: headless scoring in a worker thread (own spike-run process per call; never blocks the main thread)
  /// Start a headless score of `circuit_json` on this task id (make() first). seeds: PackedInt64Array (empty = [1]). Returns a job id (> 0), or 0 on failure.
  int64_t score_headless_async( const String& circuit_json, const PackedInt64Array& seeds, const String& mode, const String& variant, int64_t circuit_seed = -1 );
  /// Kill the job's spike-run process, delete its temp files, forget the job. True if it was still running.
  static bool headless_cancel( int64_t job );
  /// The spike-run that score_headless_async would launch ("" if none found). Searched: set_headless_runner, $SPIKE_RUN, next to the extension library, next to the executable, res://bin, dev tree, PATH.
  static String headless_runner_path();
  /// Poll: {done:bool, ok:bool, error:String, passed:bool, value:float (mean), passes:int, seeds:int, runs:Array of {seed, value, passed, parts, verdict}, ms:float}. Unknown id -> {done:true, ok:false}.
  Dictionary score_headless_poll( int64_t job );
  // v0.2 engine bridges (static, add-only): the parts-bin card list, and the neuromodulator channels of a SpikeSim (engine.h set_modulator / modulator)
  static String catalog_json();
  static PackedStringArray modulator_names();
  static bool set_modulator( const Ref< SpikeSim >& sim, const String& name, double conc );
  static double modulator( const Ref< SpikeSim >& sim, const String& name );
  /// Path of the spike-run executable used by score_headless_async (default: $SPIKE_RUN, then common build locations, then PATH).
  static void set_headless_runner( const String& path );
  static PackedStringArray task_ids();
  static String reference_circuit_of( const String& id );

protected:
  static void _bind_methods();

private:
  std::unique_ptr< spike::Task > task_;
};
} // namespace godot
