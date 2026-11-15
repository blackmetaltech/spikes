#include "spike_task.h"
#include "spike_sim.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>

using namespace godot;

static String S( const std::string& s ) { return String::utf8( s.c_str() ); }
static std::string to_std( const String& s ) { return std::string( s.utf8().get_data() ); }
static PackedStringArray strs( const std::vector< std::string >& v )
{
  PackedStringArray a;
  for ( const auto& s : v )
    a.push_back( S( s ) );
  return a;
}

bool SpikeTask::make( const String& id )
{
  task_ = spike::make_task( to_std( id ) );
  return task_ != nullptr;
}
void SpikeTask::reset( int64_t seed )
{
  if ( task_ )
    task_->reset( uint64_t( seed ) );
}
void SpikeTask::step( const Ref< SpikeSim >& sim, double ms )
{
  if ( task_ && sim.is_valid() && sim->owns() )
    task_->step( sim->engine(), ms );
}
Dictionary SpikeTask::body() const
{
  Dictionary d;
  if ( !task_ )
    return d;
  spike::BodyState b = task_->body();
  PackedVector2Array pts;
  for ( const auto& p : b.points )
    pts.push_back( Vector2( real_t( p[ 0 ] ), real_t( p[ 1 ] ) ) );
  Dictionary ro;
  for ( const auto& kv : b.readouts )
    ro[ S( kv.first ) ] = kv.second;
  d[ "points" ] = pts;
  d[ "heading" ] = b.heading;
  d[ "readouts" ] = ro;
  return d;
}
Dictionary SpikeTask::score() const
{
  Dictionary d;
  if ( !task_ )
    return d;
  spike::Score s = task_->score();
  Dictionary parts;
  for ( const auto& kv : s.parts )
    parts[ S( kv.first ) ] = kv.second;
  d[ "value" ] = s.value;
  d[ "passed" ] = s.passed;
  d[ "parts" ] = parts;
  d[ "verdict" ] = S( s.verdict );
  return d;
}
String SpikeTask::brief() const { return task_ ? S( task_->brief() ) : String(); }
double SpikeTask::field( double x, double y ) const { return task_ ? task_->field( x, y ) : 0.0; }
Rect2 SpikeTask::bounds() const
{
  if ( !task_ )
    return Rect2();
  auto b = task_->world_bounds();
  return Rect2( real_t( b[ 0 ] ), real_t( b[ 1 ] ), real_t( b[ 2 ] - b[ 0 ] ), real_t( b[ 3 ] - b[ 1 ] ) );
}
String SpikeTask::id() const { return task_ ? S( task_->id() ) : String(); }
String SpikeTask::title() const { return task_ ? S( task_->title() ) : String(); }
double SpikeTask::duration_ms() const { return task_ ? task_->duration_ms() : 0.0; }
PackedStringArray SpikeTask::sensor_channels() const { return task_ ? strs( task_->sensor_channels() ) : PackedStringArray(); }
PackedStringArray SpikeTask::motor_channels() const { return task_ ? strs( task_->motor_channels() ) : PackedStringArray(); }
String SpikeTask::reference_circuit() const { return task_ ? S( spike::reference_circuit( task_->id() ) ) : String(); }
PackedStringArray SpikeTask::task_ids() { return strs( spike::task_ids() ); }
String SpikeTask::reference_circuit_of( const String& id ) { return S( spike::reference_circuit( to_std( id ) ) ); }

void SpikeTask::_bind_methods()
{
  ClassDB::bind_method( D_METHOD( "make", "id" ), &SpikeTask::make );
  ClassDB::bind_method( D_METHOD( "reset", "seed" ), &SpikeTask::reset );
  ClassDB::bind_method( D_METHOD( "step", "sim", "ms" ), &SpikeTask::step );
  ClassDB::bind_method( D_METHOD( "body" ), &SpikeTask::body );
  ClassDB::bind_method( D_METHOD( "score" ), &SpikeTask::score );
  ClassDB::bind_method( D_METHOD( "brief" ), &SpikeTask::brief );
  ClassDB::bind_method( D_METHOD( "field", "x", "y" ), &SpikeTask::field );
  ClassDB::bind_method( D_METHOD( "bounds" ), &SpikeTask::bounds );
  ClassDB::bind_method( D_METHOD( "id" ), &SpikeTask::id );
  ClassDB::bind_method( D_METHOD( "title" ), &SpikeTask::title );
  ClassDB::bind_method( D_METHOD( "duration_ms" ), &SpikeTask::duration_ms );
  ClassDB::bind_method( D_METHOD( "sensor_channels" ), &SpikeTask::sensor_channels );
  ClassDB::bind_method( D_METHOD( "motor_channels" ), &SpikeTask::motor_channels );
  ClassDB::bind_method( D_METHOD( "reference_circuit" ), &SpikeTask::reference_circuit );
  ClassDB::bind_static_method( "SpikeTask", D_METHOD( "task_ids" ), &SpikeTask::task_ids );
  ClassDB::bind_static_method( "SpikeTask", D_METHOD( "reference_circuit_of", "id" ), &SpikeTask::reference_circuit_of );
}
