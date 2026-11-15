#pragma once
#include <godot_cpp/classes/ref_counted.hpp>
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
  static PackedStringArray task_ids();
  static String reference_circuit_of( const String& id );

protected:
  static void _bind_methods();

private:
  std::unique_ptr< spike::Task > task_;
};
} // namespace godot
