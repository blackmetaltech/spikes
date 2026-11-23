#include "spike_sim.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <cmath>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include "bridge.h"
#include "spike/theory.h"

using namespace godot;

static std::string to_std( const String& s ) { return std::string( s.utf8().get_data() ); }

static Array channels_to_array( const std::vector< spike::ChannelInfo >& v )
{
  Array out;
  for ( const auto& c : v )
  {
    Dictionary d;
    d[ "name" ] = String::utf8( c.name.c_str() );
    d[ "kind" ] = String::utf8( c.kind.c_str() );
    PackedInt32Array ns;
    for ( int32_t n : c.neurons )
      ns.push_back( n );
    d[ "neurons" ] = ns;
    out.push_back( d );
  }
  return out;
}

SpikeSim* SpikeSim::owner_ = nullptr;

// One engine for the whole process, deliberately never destroyed (NEST tears down badly during static destruction).
static spike::Engine& shared_engine()
{
  static spike::Engine* e = new spike::Engine();
  return *e;
}

SpikeSim::SpikeSim() : engine_( &shared_engine() ) {}
SpikeSim::~SpikeSim()
{
  if ( owner_ == this )
  {
    engine_->stop();
    owner_ = nullptr;
  }
}

bool SpikeSim::load_circuit( const String& json )
{
  loaded_ = engine_->load( to_std( json ) );
  owner_ = loaded_ ? this : nullptr;
  return loaded_;
}
String SpikeSim::error() const { return String::utf8( engine_->error().c_str() ); }
void SpikeSim::step( double ms )
{
  if ( owns() )
    engine_->step( ms );
}
void SpikeSim::set_input( const String& name, double value )
{
  if ( owns() )
    engine_->set_input( to_std( name ), value );
}
PackedFloat64Array SpikeSim::drain_spikes()
{
  PackedFloat64Array out;
  if ( !owns() )
    return out;
  auto v = engine_->drain_spikes();
  out.resize( int64_t( v.size() ) * 2 );
  double* w = out.ptrw();
  for ( size_t i = 0; i < v.size(); ++i )
  {
    w[ 2 * i ] = v[ i ].neuron;
    w[ 2 * i + 1 ] = v[ i ].t_ms;
  }
  return out;
}
double SpikeSim::voltage( int64_t n ) const
{
  if ( !owns() || n < 0 || n >= engine_->neuron_count() )
    return std::nan( "" );
  return engine_->voltage( int32_t( n ) );
}
void SpikeSim::watch( int64_t n )
{
  if ( owns() && n >= 0 && n < engine_->neuron_count() )
    engine_->watch( int32_t( n ) );
}
double SpikeSim::output_rate( const String& name ) const
{
  return owns() ? engine_->output_rate( to_std( name ) ) : 0.0;
}
double SpikeSim::neuron_state( int64_t n, const String& key ) const { return owns() ? engine_->neuron_state( int32_t( n ), to_std( key ) ) : std::nan( "" ); }
double SpikeSim::input_value( const String& name ) const { return owns() ? engine_->input_value( to_std( name ) ) : 0.0; }
double SpikeSim::output_value( const String& name ) const { return owns() ? engine_->output_value( to_std( name ) ) : 0.0; }
double SpikeSim::input_weight( const String& name ) const { return owns() ? engine_->input_weight( to_std( name ) ) : 0.0; }
Dictionary SpikeSim::phase_plane( int64_t n, double i_ext ) const
{
  Dictionary d;
  if ( !owns() )
  {
    d[ "ok" ] = false;
    return d;
  }
  const spike::theory::PhasePlane pp = spike::theory::phase_plane_of( *engine_, int32_t( n ), i_ext );
  d[ "ok" ] = pp.ok;
  d[ "note" ] = String::utf8( pp.note.c_str() );
  d[ "model" ] = String::utf8( pp.model.c_str() );
  d[ "w_name" ] = String::utf8( pp.w_name.c_str() );
  d[ "v_lo" ] = pp.v_lo;
  d[ "v_hi" ] = pp.v_hi;
  d[ "w_lo" ] = pp.w_lo;
  d[ "w_hi" ] = pp.w_hi;
  d[ "v_reset" ] = pp.v_reset;
  d[ "v_peak" ] = pp.v_peak;
  d[ "w_jump" ] = pp.w_jump;
  d[ "i_total" ] = pp.i_total;
  PackedVector2Array vn, wn;
  for ( const auto& q : pp.v_null ) vn.push_back( Vector2( real_t( q[ 0 ] ), real_t( q[ 1 ] ) ) );
  for ( const auto& q : pp.w_null ) wn.push_back( Vector2( real_t( q[ 0 ] ), real_t( q[ 1 ] ) ) );
  d[ "v_null" ] = vn;
  d[ "w_null" ] = wn;
  Array fx;
  for ( const auto& f : pp.fixed )
  {
    Dictionary e;
    e[ "v" ] = f.v;
    e[ "w" ] = f.w;
    e[ "kind" ] = String::utf8( f.kind.c_str() );
    e[ "stable" ] = f.stable;
    fx.push_back( e );
  }
  d[ "fixed" ] = fx;
  return d;
}
double SpikeSim::output_angle( const String& name ) const { return owns() ? engine_->output_angle( to_std( name ) ) : 0.0; }
double SpikeSim::output_strength( const String& name ) const { return owns() ? engine_->output_strength( to_std( name ) ) : 0.0; }
bool SpikeSim::queue_spikes( const String& name, const PackedFloat64Array& times_ms )
{
  if ( !owns() )
    return false;
  std::vector< double > t( size_t( times_ms.size() ) );
  for ( int64_t i = 0; i < times_ms.size(); ++i )
    t[ size_t( i ) ] = times_ms[ i ];
  return engine_->queue_spikes( to_std( name ), t );
}
bool SpikeSim::set_input_param( const String& name, const String& key, double value )
{
  return owns() && engine_->set_input_param( to_std( name ), to_std( key ), value );
}
int64_t SpikeSim::neuron_count() const { return owns() ? engine_->neuron_count() : 0; }
Dictionary SpikeSim::stats() const
{
  Dictionary d;
  if ( !owns() )
    return d;
  auto s = engine_->stats();
  d[ "t_ms" ] = s.t_ms;
  d[ "spikes" ] = int64_t( s.spikes );
  d[ "checksum" ] = String::num_uint64( s.checksum );
  d[ "dropped" ] = int64_t( engine_->dropped() );
  return d;
}
String SpikeSim::neuron_population( int64_t n ) const
{
  if ( !owns() || n < 0 || n >= engine_->neuron_count() )
    return "";
  return String::utf8( engine_->neuron_population( int32_t( n ) ).c_str() );
}
Array SpikeSim::inputs() const { return owns() ? channels_to_array( engine_->inputs() ) : Array(); }
Array SpikeSim::outputs() const { return owns() ? channels_to_array( engine_->outputs() ) : Array(); }
bool SpikeSim::set_weight( int64_t p, int64_t e, double w )
{
  return owns() && engine_->set_weight( int32_t( p ), int32_t( e ), w );
}
bool SpikeSim::set_neuron_param( int64_t n, const String& k, double v )
{
  return owns() && engine_->set_neuron_param( int32_t( n ), to_std( k ), v );
}
int64_t SpikeSim::dropped() const { return owns() ? int64_t( engine_->dropped() ) : 0; }
void SpikeSim::stop()
{
  if ( owns() )
    engine_->stop();
  if ( owner_ == this )
    owner_ = nullptr;
  loaded_ = false;
}

String SpikeSim::motif_json( int64_t sign, int64_t n_null, int64_t seed ) const
{
  if ( !owns() )
    return "";
  return String::utf8( spike_gd::motif_json( *engine_, int( sign ), int( n_null ), uint64_t( seed ) ).c_str() );
}

String SpikeSim::motif_json_edges( const PackedFloat64Array& triples, int64_t n, int64_t sign, int64_t n_null, int64_t seed )
{
  std::vector< double > t( size_t( triples.size() ) );
  for ( int64_t i = 0; i < triples.size(); ++i )
    t[ size_t( i ) ] = triples[ i ];
  return String::utf8( spike_gd::motif_json_edges( t, int( n ), int( sign ), int( n_null ), uint64_t( seed ) ).c_str() );
}

Dictionary SpikeSim::spike_stats( const PackedFloat64Array& pairs, int64_t n, double t0_ms, double t1_ms )
{
  std::vector< spike::Spike > sp;
  const double* r = pairs.ptr();
  for ( int64_t i = 0; i + 1 < pairs.size(); i += 2 )
    sp.push_back( { int32_t( r[ i ] ), r[ i + 1 ] } );
  auto st = spike_gd::spike_stats( sp, int( n ), t0_ms, t1_ms );
  Dictionary d;
  PackedFloat64Array rates, cv;
  for ( double v : st.rates )
    rates.push_back( v );
  for ( double v : st.cv )
    cv.push_back( v );
  d[ "rates" ] = rates;
  d[ "isi_cv" ] = cv;
  d[ "chi" ] = st.chi;
  return d;
}

// ---- v0.2 additions
static std::vector< spike::Spike > pairs_to_spikes( const PackedFloat64Array& pairs )
{
  std::vector< spike::Spike > sp;
  const double* r = pairs.ptr();
  for ( int64_t i = 0; i + 1 < pairs.size(); i += 2 )
    sp.push_back( { int32_t( r[ i ] ), r[ i + 1 ] } );
  return sp;
}
static std::vector< int > ints( const PackedInt32Array& a )
{
  std::vector< int > v;
  for ( int64_t i = 0; i < a.size(); ++i )
    v.push_back( a[ i ] );
  return v;
}
static std::vector< double > dbls( const PackedFloat64Array& a )
{
  std::vector< double > v;
  for ( int64_t i = 0; i < a.size(); ++i )
    v.push_back( a[ i ] );
  return v;
}
static String js( const std::string& s ) { return String::utf8( s.c_str() ); }
bool SpikeSim::set_modulator( const String& name, double conc ) { return owns() && engine_->set_modulator( std::string( name.utf8().get_data() ), conc ); }
double SpikeSim::modulator( const String& name ) const { return owns() ? engine_->modulator( std::string( name.utf8().get_data() ) ) : 0.0; }
PackedStringArray SpikeSim::modulator_names()
{
  PackedStringArray a;
  for ( const auto& s : spike::modulator_names() )
    a.push_back( js( s ) );
  return a;
}
String SpikeSim::bursts_json( const PackedFloat64Array& pairs, int64_t n, const PackedInt32Array& neurons, double t0_ms, double t1_ms )
{
  return js( spike_gd::burst_json( pairs_to_spikes( pairs ), int( n ), ints( neurons ), t0_ms, t1_ms ) );
}
String SpikeSim::prc_json( const PackedFloat64Array& pairs, int64_t n, const PackedInt32Array& neurons, const PackedFloat64Array& perturb_ms,
  double period_ms, double merge_gap_ms, int64_t nbins )
{
  return js( spike_gd::prc_json( pairs_to_spikes( pairs ), int( n ), ints( neurons ), dbls( perturb_ms ), period_ms, merge_gap_ms, int( nbins ) ) );
}
String SpikeSim::prc_experiment_json( const String& channel, double baseline, double pulse_value, double pulse_ms, int64_t n_phases,
  const PackedInt32Array& neurons, double merge_gap_ms, int64_t repeats, int64_t nbins )
{
  if ( !owns() )
    return "";
  return js( spike_gd::prc_experiment_json( *engine_, std::string( channel.utf8().get_data() ), baseline, pulse_value, pulse_ms, int( n_phases ),
    ints( neurons ), merge_gap_ms, int( repeats ), int( nbins ) ) );
}
String SpikeSim::hrv_json( const PackedFloat64Array& beats ) { return js( spike_gd::hrv_json( dbls( beats ) ) ); }
String SpikeSim::sta_json( const PackedFloat64Array& stim, double dt, const PackedFloat64Array& spikes, double before, double after ) { return js( spike_gd::sta_json( dbls( stim ), dt, dbls( spikes ), before, after ) ); }
String SpikeSim::motif4_json( int64_t sign, int64_t n_null, int64_t seed, double max_work ) const
{
  if ( !owns() )
    return "";
  return js( spike_gd::motif4_json( *engine_, int( sign ), int( n_null ), uint64_t( seed ), max_work ) );
}
String SpikeSim::motif4_json_edges( const PackedFloat64Array& triples, int64_t n, int64_t sign, int64_t n_null, int64_t seed, double max_work )
{
  return js( spike_gd::motif4_json_edges( dbls( triples ), int( n ), int( sign ), int( n_null ), uint64_t( seed ), max_work ) );
}
String SpikeSim::graph_path_json( const PackedInt32Array& sources, const PackedInt32Array& targets, const PackedInt32Array& banned ) const
{
  if ( !owns() )
    return "";
  return js( spike_gd::graph_path_json( *engine_, ints( sources ), ints( targets ), ints( banned ) ) );
}
String SpikeSim::graph_path_json_edges( const PackedFloat64Array& triples, int64_t n, const PackedInt32Array& sources, const PackedInt32Array& targets,
  const PackedInt32Array& banned )
{
  return js( spike_gd::graph_path_json_edges( dbls( triples ), int( n ), ints( sources ), ints( targets ), ints( banned ) ) );
}
String SpikeSim::graph_degrees_json( int64_t k ) const
{
  if ( !owns() )
    return "";
  return js( spike_gd::graph_degrees_json( *engine_, int( k ) ) );
}
String SpikeSim::graph_degrees_json_edges( const PackedFloat64Array& triples, int64_t n, int64_t k )
{
  return js( spike_gd::graph_degrees_json_edges( dbls( triples ), int( n ), int( k ) ) );
}
String SpikeSim::motif_null_json( int64_t model, const PackedInt32Array& block_of, int64_t n_null, int64_t seed ) const
{
  if ( !owns() )
    return "";
  return js( spike_gd::motif_null_json( *engine_, int( model ), ints( block_of ), int( n_null ), uint64_t( seed ) ) );
}
String SpikeSim::motif_null_json_edges( const PackedFloat64Array& triples, int64_t n, int64_t model, const PackedInt32Array& block_of, int64_t n_null, int64_t seed )
{
  return js( spike_gd::motif_null_json_edges( dbls( triples ), int( n ), int( model ), ints( block_of ), int( n_null ), uint64_t( seed ) ) );
}

void SpikeSim::_bind_methods()
{
  ClassDB::bind_method( D_METHOD( "load_circuit", "json" ), &SpikeSim::load_circuit );
  ClassDB::bind_method( D_METHOD( "error" ), &SpikeSim::error );
  ClassDB::bind_method( D_METHOD( "step", "ms" ), &SpikeSim::step );
  ClassDB::bind_method( D_METHOD( "set_input", "name", "value" ), &SpikeSim::set_input );
  ClassDB::bind_method( D_METHOD( "drain_spikes" ), &SpikeSim::drain_spikes );
  ClassDB::bind_method( D_METHOD( "voltage", "i" ), &SpikeSim::voltage );
  ClassDB::bind_method( D_METHOD( "watch", "i" ), &SpikeSim::watch );
  ClassDB::bind_method( D_METHOD( "output_rate", "name" ), &SpikeSim::output_rate );
  ClassDB::bind_method( D_METHOD( "neuron_state", "i", "key" ), &SpikeSim::neuron_state );
  ClassDB::bind_method( D_METHOD( "output_value", "name" ), &SpikeSim::output_value );
  ClassDB::bind_method( D_METHOD( "input_weight", "name" ), &SpikeSim::input_weight );
  ClassDB::bind_method( D_METHOD( "input_value", "name" ), &SpikeSim::input_value );
  ClassDB::bind_method( D_METHOD( "phase_plane", "i", "i_ext" ), &SpikeSim::phase_plane );
  ClassDB::bind_method( D_METHOD( "output_angle", "name" ), &SpikeSim::output_angle );
  ClassDB::bind_method( D_METHOD( "output_strength", "name" ), &SpikeSim::output_strength );
  ClassDB::bind_method( D_METHOD( "queue_spikes", "name", "times_ms" ), &SpikeSim::queue_spikes );
  ClassDB::bind_method( D_METHOD( "set_input_param", "name", "key", "value" ), &SpikeSim::set_input_param );
  ClassDB::bind_method( D_METHOD( "neuron_count" ), &SpikeSim::neuron_count );
  ClassDB::bind_method( D_METHOD( "stats" ), &SpikeSim::stats );
  ClassDB::bind_method( D_METHOD( "neuron_population", "i" ), &SpikeSim::neuron_population );
  ClassDB::bind_method( D_METHOD( "inputs" ), &SpikeSim::inputs );
  ClassDB::bind_method( D_METHOD( "outputs" ), &SpikeSim::outputs );
  ClassDB::bind_method( D_METHOD( "set_weight", "projection", "edge", "weight" ), &SpikeSim::set_weight );
  ClassDB::bind_method( D_METHOD( "set_neuron_param", "i", "key", "value" ), &SpikeSim::set_neuron_param );
  ClassDB::bind_method( D_METHOD( "dropped" ), &SpikeSim::dropped );
  ClassDB::bind_method( D_METHOD( "stop" ), &SpikeSim::stop );
  ClassDB::bind_method( D_METHOD( "motif_json", "sign", "n_null", "seed" ), &SpikeSim::motif_json );
  ClassDB::bind_static_method( "SpikeSim", D_METHOD( "motif_json_edges", "triples", "n", "sign", "n_null", "seed" ), &SpikeSim::motif_json_edges );
  ClassDB::bind_static_method( "SpikeSim", D_METHOD( "spike_stats", "pairs", "n", "t0_ms", "t1_ms" ), &SpikeSim::spike_stats );
  ClassDB::bind_method( D_METHOD( "set_modulator", "name", "conc" ), &SpikeSim::set_modulator );
  ClassDB::bind_method( D_METHOD( "modulator", "name" ), &SpikeSim::modulator );
  ClassDB::bind_static_method( "SpikeSim", D_METHOD( "modulator_names" ), &SpikeSim::modulator_names );
  ClassDB::bind_static_method( "SpikeSim", D_METHOD( "bursts_json", "pairs", "n", "neurons", "t0_ms", "t1_ms" ), &SpikeSim::bursts_json );
  ClassDB::bind_static_method( "SpikeSim", D_METHOD( "prc_json", "pairs", "n", "neurons", "perturb_ms", "period_ms", "merge_gap_ms", "nbins" ), &SpikeSim::prc_json );
  ClassDB::bind_method( D_METHOD( "prc_experiment_json", "channel", "baseline", "pulse_value", "pulse_ms", "n_phases", "neurons", "merge_gap_ms", "repeats", "nbins" ), &SpikeSim::prc_experiment_json );
  ClassDB::bind_static_method( "SpikeSim", D_METHOD( "hrv_json", "beat_times_ms" ), &SpikeSim::hrv_json );
  ClassDB::bind_static_method( "SpikeSim", D_METHOD( "sta_json", "stimulus", "dt_ms", "spike_times_ms", "before_ms", "after_ms" ), &SpikeSim::sta_json );
  ClassDB::bind_method( D_METHOD( "motif4_json", "sign", "n_null", "seed", "max_work" ), &SpikeSim::motif4_json );
  ClassDB::bind_static_method( "SpikeSim", D_METHOD( "motif4_json_edges", "triples", "n", "sign", "n_null", "seed", "max_work" ), &SpikeSim::motif4_json_edges );
  ClassDB::bind_method( D_METHOD( "graph_path_json", "sources", "targets", "banned" ), &SpikeSim::graph_path_json );
  ClassDB::bind_static_method( "SpikeSim", D_METHOD( "graph_path_json_edges", "triples", "n", "sources", "targets", "banned" ), &SpikeSim::graph_path_json_edges );
  ClassDB::bind_method( D_METHOD( "graph_degrees_json", "k" ), &SpikeSim::graph_degrees_json );
  ClassDB::bind_static_method( "SpikeSim", D_METHOD( "graph_degrees_json_edges", "triples", "n", "k" ), &SpikeSim::graph_degrees_json_edges );
  ClassDB::bind_method( D_METHOD( "motif_null_json", "model", "block_of", "n_null", "seed" ), &SpikeSim::motif_null_json );
  ClassDB::bind_static_method( "SpikeSim", D_METHOD( "motif_null_json_edges", "triples", "n", "model", "block_of", "n_null", "seed" ), &SpikeSim::motif_null_json_edges );
}
