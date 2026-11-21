#include "spike_task.h"
#include "spike_sim.h"
#include "spike/catalog.h"
#include "bridge.h"
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
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

// the project's res://bin (globalized) is a search directory for spike-run: a shipped game keeps the runner next to its data
static void add_project_bin()
{
  if ( ProjectSettings* ps = ProjectSettings::get_singleton() )
    spike_gd::headless_add_search_dir( to_std( ps->globalize_path( "res://bin" ) ) );
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
double SpikeTask::quick_duration_ms() const { return task_ ? task_->quick_duration_ms() : 0.0; }
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
      d[ "weight" ] = it->second.weight;  // v0.3: default first-connect weight, 0 = unset
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
// ---- v0.3 ----
String SpikeTask::level_kind() const { return task_ ? S( task_->level_kind() ) : String( "task" ); }
PackedStringArray SpikeTask::concepts_new() const { return task_ ? strs( task_->concepts_new() ) : PackedStringArray(); }
PackedStringArray SpikeTask::concepts_used() const { return task_ ? strs( task_->concepts_used() ) : PackedStringArray(); }
bool SpikeTask::on_spine() const { return task_ ? task_->on_spine() : true; }
void SpikeTask::set_mode( const String& m )
{
  if ( task_ )
    task_->set_mode( to_std( m ) );
}
String SpikeTask::mode() const { return task_ ? S( task_->mode() ) : String( "standard" ); }
Dictionary SpikeTask::variants() const
{
  Dictionary d;
  if ( task_ )
    for ( const auto& kv : task_->variants() )
      d[ S( kv.first ) ] = S( kv.second );
  return d;
}
Dictionary SpikeTask::challenge() const
{
  Dictionary d;
  if ( !task_ )
    return d;
  const auto c = task_->challenge();
  d[ "text" ] = S( c.text );
  d[ "max_neurons" ] = int64_t( c.max_neurons );
  d[ "max_rate_hz" ] = c.max_rate_hz;
  d[ "no_modulators" ] = c.no_modulators;
  return d;
}
void SpikeTask::set_variant( const String& v )
{
  if ( task_ )
    task_->set_variant( to_std( v ) );
}
String SpikeTask::variant() const { return task_ ? S( task_->variant() ) : String(); }
Array SpikeTask::hints() const
{
  Array a;
  if ( !task_ )
    return a;
  for ( const auto& h : task_->hints() )
  {
    Dictionary d;
    d[ "tier" ] = int64_t( h.tier );
    d[ "text" ] = S( h.text );
    d[ "target" ] = S( h.target );
    a.push_back( d );
  }
  return a;
}
Dictionary SpikeTask::metrics() const
{
  Dictionary d;
  if ( !task_ )
    return d;
  for ( const auto& kv : task_->metrics() )
  {
    Dictionary m;
    m[ "value" ] = kv.second.value;
    m[ "unit" ] = S( kv.second.unit );
    m[ "lower_is_better" ] = kv.second.lower_is_better;
    d[ S( kv.first ) ] = m;
  }
  return d;
}
double SpikeTask::next_event_ms( double t_ms ) const { return task_ ? task_->next_event_ms( t_ms ) : -1.0; }
Dictionary SpikeTask::diagnosis() const
{
  Dictionary d;
  if ( !task_ )
    return d;
  const auto g = task_->diagnosis();
  d[ "circuit" ] = S( g.circuit );
  d[ "symptoms" ] = S( g.symptoms );
  d[ "candidates" ] = strs( g.candidates );
  d[ "answer" ] = S( g.answer );
  Dictionary lb, ex;
  for ( auto& [ k, v ] : g.labels ) lb[ S( k ) ] = S( v );
  for ( auto& [ k, v ] : g.explain ) ex[ S( k ) ] = S( v );
  d[ "labels" ] = lb;
  d[ "explain" ] = ex;
  Array pr;   // add-only: the evidence probes of the case ({kind, target, tells}); for the game to CHECK, never to display
  for ( auto& p : g.probes )
  {
    Dictionary q;
    q[ "kind" ] = S( p.kind );
    q[ "target" ] = S( p.target );
    q[ "tells" ] = S( p.tells );
    pr.push_back( q );
  }
  d[ "probes" ] = pr;
  return d;
}
Dictionary SpikeTask::notebook() const
{
  Dictionary d;
  if ( !task_ )
    return d;
  const auto n = task_->notebook();
  d[ "title" ] = S( n.title );
  d[ "text" ] = S( n.text );
  d[ "citation" ] = S( n.citation );
  return d;
}
bool SpikeTask::headless_cancel( int64_t job ) { return spike_gd::headless_cancel( long( job ) ); }
String SpikeTask::headless_runner_path()
{
  add_project_bin();
  return S( spike_gd::headless_runner_path() );
}
String SpikeTask::default_model() const { return task_ ? S( task_->default_model() ) : String(); }
String SpikeTask::recipe() const { return task_ ? S( task_->recipe() ) : String(); }
int64_t SpikeTask::score_headless_async( const String& circuit_json, const PackedInt64Array& seeds, const String& mode, const String& variant, int64_t circuit_seed )
{
  if ( !task_ )
    return 0;
  std::vector< long long > sv;
  for ( int64_t i = 0; i < seeds.size(); ++i )
    sv.push_back( seeds[ i ] );
  std::vector< long long > cs;
  if ( circuit_seed >= 0 )
    cs.push_back( circuit_seed );  // pinned for every task seed (the live run's circuit seed); -1 = the 1000 * seed + circuit seed reseed
  add_project_bin();
  std::string err;
  const long id = spike_gd::headless_start( task_->id(), to_std( circuit_json ), sv, to_std( mode ), to_std( variant ), err, cs );
  if ( id == 0 )
    UtilityFunctions::push_warning( String( "score_headless_async: " ) + S( err ) );
  return id;
}
int64_t SpikeTask::score_headless_quick_async( const String& circuit_json, const PackedInt64Array& seeds, const String& mode, const String& variant, int64_t circuit_seed )
{
  if ( !task_ )
    return 0;
  std::vector< long long > sv;
  for ( int64_t i = 0; i < seeds.size(); ++i )
    sv.push_back( seeds[ i ] );
  std::vector< long long > cs;
  if ( circuit_seed >= 0 )
    cs.push_back( circuit_seed );  // pinned for every task seed (the live run's circuit seed); -1 = the 1000 * seed + circuit seed reseed
  add_project_bin();
  std::string err;
  const long id = spike_gd::headless_start( task_->id(), to_std( circuit_json ), sv, to_std( mode ), to_std( variant ), err, cs, true );
  if ( id == 0 )
    UtilityFunctions::push_warning( String( "score_headless_quick_async: " ) + S( err ) );
  return id;
}
Dictionary SpikeTask::score_headless_poll( int64_t job )
{
  Dictionary out;
  Variant v = JSON::parse_string( S( spike_gd::headless_poll( long( job ) ) ) );
  Dictionary r = v.get_type() == Variant::DICTIONARY ? Dictionary( v ) : Dictionary();
  out[ "done" ] = bool( r.get( "done", true ) );
  out[ "ok" ] = bool( r.get( "ok", false ) );
  out[ "error" ] = r.get( "error", "" );
  out[ "ms" ] = r.get( "ms", 0.0 );
  if ( bool( out[ "ok" ] ) && r.has( "result" ) )
  {
    Dictionary res = r[ "result" ];
    const int64_t n = int64_t( res.get( "n", 0 ) );
    const int64_t passes = int64_t( res.get( "passes", 0 ) );
    out[ "runs" ] = res.get( "runs", Array() );
    out[ "seeds" ] = n;
    out[ "passes" ] = passes;
    out[ "value" ] = res.get( "mean_value", 0.0 );
    out[ "passed" ] = n > 0 && passes == n;
  }
  return out;
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
void SpikeTask::set_headless_runner( const String& path ) { spike_gd::headless_set_runner( to_std( path ) ); }
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
  ClassDB::bind_method( D_METHOD( "quick_duration_ms" ), &SpikeTask::quick_duration_ms );
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
  ClassDB::bind_method( D_METHOD( "level_kind" ), &SpikeTask::level_kind );
  ClassDB::bind_method( D_METHOD( "concepts_new" ), &SpikeTask::concepts_new );
  ClassDB::bind_method( D_METHOD( "concepts_used" ), &SpikeTask::concepts_used );
  ClassDB::bind_method( D_METHOD( "on_spine" ), &SpikeTask::on_spine );
  ClassDB::bind_method( D_METHOD( "set_mode", "mode" ), &SpikeTask::set_mode );
  ClassDB::bind_method( D_METHOD( "mode" ), &SpikeTask::mode );
  ClassDB::bind_method( D_METHOD( "variants" ), &SpikeTask::variants );
  ClassDB::bind_method( D_METHOD( "challenge" ), &SpikeTask::challenge );
  ClassDB::bind_method( D_METHOD( "set_variant", "variant" ), &SpikeTask::set_variant );
  ClassDB::bind_method( D_METHOD( "variant" ), &SpikeTask::variant );
  ClassDB::bind_method( D_METHOD( "hints" ), &SpikeTask::hints );
  ClassDB::bind_method( D_METHOD( "metrics" ), &SpikeTask::metrics );
  ClassDB::bind_method( D_METHOD( "next_event_ms", "t_ms" ), &SpikeTask::next_event_ms );
  ClassDB::bind_method( D_METHOD( "diagnosis" ), &SpikeTask::diagnosis );
  ClassDB::bind_method( D_METHOD( "notebook" ), &SpikeTask::notebook );
  ClassDB::bind_method( D_METHOD( "default_model" ), &SpikeTask::default_model );
  ClassDB::bind_method( D_METHOD( "recipe" ), &SpikeTask::recipe );
  ClassDB::bind_method( D_METHOD( "score_headless_async", "circuit_json", "seeds", "mode", "variant", "circuit_seed" ), &SpikeTask::score_headless_async, DEFVAL( int64_t( -1 ) ) );
  ClassDB::bind_static_method( "SpikeTask", D_METHOD( "headless_cancel", "job" ), &SpikeTask::headless_cancel );
  ClassDB::bind_static_method( "SpikeTask", D_METHOD( "headless_runner_path" ), &SpikeTask::headless_runner_path );
  ClassDB::bind_method( D_METHOD( "score_headless_quick_async", "circuit_json", "seeds", "mode", "variant", "circuit_seed" ), &SpikeTask::score_headless_quick_async, DEFVAL( int64_t( -1 ) ) );
  ClassDB::bind_method( D_METHOD( "score_headless_poll", "job" ), &SpikeTask::score_headless_poll );
  ClassDB::bind_static_method( "SpikeTask", D_METHOD( "set_headless_runner", "path" ), &SpikeTask::set_headless_runner );
  ClassDB::bind_static_method( "SpikeTask", D_METHOD( "catalog_json" ), &SpikeTask::catalog_json );
  ClassDB::bind_static_method( "SpikeTask", D_METHOD( "modulator_names" ), &SpikeTask::modulator_names );
  ClassDB::bind_static_method( "SpikeTask", D_METHOD( "set_modulator", "sim", "name", "conc" ), &SpikeTask::set_modulator );
  ClassDB::bind_static_method( "SpikeTask", D_METHOD( "modulator", "sim", "name" ), &SpikeTask::modulator );
  ClassDB::bind_static_method( "SpikeTask", D_METHOD( "task_ids" ), &SpikeTask::task_ids );
  ClassDB::bind_static_method( "SpikeTask", D_METHOD( "reference_circuit_of", "id" ), &SpikeTask::reference_circuit_of );
}
