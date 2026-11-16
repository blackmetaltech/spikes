#include "spike_sim.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <cmath>
#include "bridge.h"

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
}
