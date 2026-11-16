#include "spike_task.h"
#include "spike_sim.h"
#include "spike/catalog.h"
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
bool SpikeTask::step_ok( const Ref< SpikeSim >& sim ) const
{
  return !sim.is_valid() || !sim->owns() || sim->engine().last_step_ok();
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
String SpikeTask::chapter() const { return task_ ? S( task_->chapter() ) : String(); }
String SpikeTask::view() const { return task_ ? S( task_->view() ) : String(); }
int64_t SpikeTask::par_neurons() const { return task_ ? int64_t( task_->par_neurons() ) : 0; }
Array SpikeTask::timeline() const
{
  Array a;
  if ( !task_ )
    return a;
  for ( const auto& e : task_->timeline() )
  {
    Dictionary d;
    d[ "t_ms" ] = e.t_ms;
    d[ "dur_ms" ] = e.dur_ms;
    d[ "label" ] = S( e.label );
    a.push_back( d );
  }
  return a;
}
Array SpikeTask::bands() const
{
  Array a;
  if ( !task_ )
    return a;
  for ( const auto& b : task_->bands() )
  {
    Dictionary d;
    d[ "name" ] = S( b.name );
    d[ "unit" ] = S( b.unit );
    d[ "lo" ] = b.lo;
    d[ "hi" ] = b.hi;
    a.push_back( d );
  }
  return a;
}
PackedStringArray SpikeTask::modulators() const { return task_ ? strs( task_->modulators() ) : PackedStringArray(); }
Dictionary SpikeTask::channel_kinds() const
{
  Dictionary in, out, all;
  if ( task_ )
  {
    const auto ck = task_->channel_kinds();
    for ( const auto& name : task_->sensor_channels() )
    {
      auto it = ck.find( name );
      if ( it == ck.end() )
        continue;
      Dictionary d;
      d[ "kind" ] = S( it->second.kind );
      d[ "value" ] = it->second.value;
      in[ S( name ) ] = d;
    }
    for ( const auto& name : task_->motor_channels() )
    {
      auto it = ck.find( name );
      if ( it == ck.end() )
        continue;
      Dictionary d;
      d[ "kind" ] = S( it->second.kind );
      if ( it->second.kind == "modulator" )
      {
        d[ "modulator" ] = S( it->second.modulator );
        d[ "gain" ] = it->second.gain;
        d[ "tau_ms" ] = it->second.tau_ms;
      }
      out[ S( name ) ] = d;
    }
  }
  all[ "inputs" ] = in;
  all[ "outputs" ] = out;
  return all;
}
String SpikeTask::catalog_json() { return S( spike::catalog_json() ); }
PackedStringArray SpikeTask::modulator_names() { return strs( spike::modulator_names() ); }
bool SpikeTask::set_modulator( const Ref< SpikeSim >& sim, const String& name, double conc )
{
  return sim.is_valid() && sim->owns() && sim->engine().set_modulator( to_std( name ), conc );
}
double SpikeTask::modulator( const Ref< SpikeSim >& sim, const String& name )
{
  return ( sim.is_valid() && sim->owns() ) ? sim->engine().modulator( to_std( name ) ) : 0.0;
}
PackedStringArray SpikeTask::task_ids() { return strs( spike::task_ids() ); }
String SpikeTask::reference_circuit_of( const String& id ) { return S( spike::reference_circuit( to_std( id ) ) ); }

void SpikeTask::_bind_methods()
{
  ClassDB::bind_method( D_METHOD( "make", "id" ), &SpikeTask::make );
  ClassDB::bind_method( D_METHOD( "reset", "seed" ), &SpikeTask::reset );
  ClassDB::bind_method( D_METHOD( "step", "sim", "ms" ), &SpikeTask::step );
  ClassDB::bind_method( D_METHOD( "step_ok", "sim" ), &SpikeTask::step_ok );
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
  ClassDB::bind_method( D_METHOD( "chapter" ), &SpikeTask::chapter );
  ClassDB::bind_method( D_METHOD( "view" ), &SpikeTask::view );
  ClassDB::bind_method( D_METHOD( "par_neurons" ), &SpikeTask::par_neurons );
  ClassDB::bind_method( D_METHOD( "timeline" ), &SpikeTask::timeline );
  ClassDB::bind_method( D_METHOD( "bands" ), &SpikeTask::bands );
  ClassDB::bind_method( D_METHOD( "modulators" ), &SpikeTask::modulators );
  ClassDB::bind_method( D_METHOD( "channel_kinds" ), &SpikeTask::channel_kinds );
  ClassDB::bind_static_method( "SpikeTask", D_METHOD( "catalog_json" ), &SpikeTask::catalog_json );
  ClassDB::bind_static_method( "SpikeTask", D_METHOD( "modulator_names" ), &SpikeTask::modulator_names );
  ClassDB::bind_static_method( "SpikeTask", D_METHOD( "set_modulator", "sim", "name", "conc" ), &SpikeTask::set_modulator );
  ClassDB::bind_static_method( "SpikeTask", D_METHOD( "modulator", "sim", "name" ), &SpikeTask::modulator );
  ClassDB::bind_static_method( "SpikeTask", D_METHOD( "task_ids" ), &SpikeTask::task_ids );
  ClassDB::bind_static_method( "SpikeTask", D_METHOD( "reference_circuit_of", "id" ), &SpikeTask::reference_circuit_of );
}
