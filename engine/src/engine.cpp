// spike::Engine on the NEST C++ API. Single instance per process: NEST's kernel is a process-wide singleton, so
// a second live Engine throws std::logic_error. load() may be called any number of times (ResetKernel each time).
#include "spike/engine.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <limits>
#include <map>
#include <stdexcept>

#include "circuit.h"
#include "ring_backend.h"

#include "connection_id.h"
#include "dictionary.h"
#include "kernel_manager.h"
#include "nest.h"
#include "node_collection.h"

namespace spike
{
namespace
{
std::atomic< bool > g_alive{ false };
bool g_nest_ready = false;
int g_threads = 1;
using clk = std::chrono::steady_clock;
double us_since( clk::time_point t ) { return std::chrono::duration< double, std::micro >( clk::now() - t ).count(); }

constexpr uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr uint64_t kFnvPrime = 1099511628211ULL;
constexpr size_t kPendingCap = size_t( 1 ) << 22;
constexpr size_t kVoltageCap = 4096;

inline uint64_t fnv_bytes( uint64_t h, uint64_t v, int nbytes )
{
  for ( int i = 0; i < nbytes; ++i )
  {
    h ^= ( v >> ( 8 * i ) ) & 0xff;
    h *= kFnvPrime;
  }
  return h;
}

Dictionary to_dict( const nlohmann::json& o );

void put( Dictionary& d, const std::string& key, const nlohmann::json& v )
{
  using J = nlohmann::json;
  switch ( v.type() )
  {
  case J::value_t::boolean:
    d[ key ] = v.get< bool >();
    break;
  case J::value_t::number_integer:
  case J::value_t::number_unsigned:
    d[ key ] = v.get< long >();
    break;
  case J::value_t::number_float:
    d[ key ] = v.get< double >();
    break;
  case J::value_t::string:
    d[ key ] = v.get< std::string >();
    break;
  case J::value_t::object:
    d[ key ] = to_dict( v );
    break;
  case J::value_t::array:
  {
    bool all_int = true;
    for ( const auto& e : v )
    {
      if ( not e.is_number() )
        throw std::runtime_error( "'" + key + "': arrays must hold numbers" );
      all_int = all_int and e.is_number_integer();
    }
    if ( v.empty() )
      throw std::runtime_error( "'" + key + "': empty arrays are not supported" );
    if ( all_int )
      d[ key ] = v.get< std::vector< long > >();
    else
      d[ key ] = v.get< std::vector< double > >();
    break;
  }
  default:
    throw std::runtime_error( "'" + key + "': null is not a valid value" );
  }
}

Dictionary to_dict( const nlohmann::json& o )
{
  Dictionary d;
  for ( auto it = o.begin(); it != o.end(); ++it )
    put( d, it.key(), it.value() );
  return d;
}

std::vector< size_t > node_ids( const std::vector< int32_t >& neurons )
{
  std::vector< size_t > ids( neurons.size() );
  for ( size_t i = 0; i < neurons.size(); ++i )
    ids[ i ] = static_cast< size_t >( neurons[ i ] ) + 1;
  return ids;
}
} // namespace

struct Engine::Impl
{
  std::string error;
  CircuitSpec spec;
  bool loaded = false, prepared = false;

  std::vector< std::string > neuron_pop;
  std::vector< nest::NodeCollectionPTR > pop_nc;
  struct Dev
  {
    std::string param;
    size_t node_id;
    double value;
    bool dirty;
  };
  std::vector< Dev > devs;
  std::map< std::string, size_t > in_idx, out_idx;
  std::vector< double > out_count;
  std::vector< std::vector< int > > out_of_neuron;
  struct Proj
  {
    nest::NodeCollectionPTR src, tgt;
    std::string model;
  };
  std::vector< Proj > projs;

  std::vector< Spike > pending;
  std::vector< RingSpike > scratch;
  uint64_t spikes = 0, checksum = kFnvOffset, overflow = 0;
  int64_t steps_done = 0;
  double last_step_ms = 0, last_run_us = 0;
  std::map< int32_t, std::deque< double > > watched;

  void clear_state()
  {
    error.clear();
    spec = CircuitSpec();
    loaded = prepared = false;
    neuron_pop.clear();
    pop_nc.clear();
    devs.clear();
    in_idx.clear();
    out_idx.clear();
    out_count.clear();
    out_of_neuron.clear();
    projs.clear();
    pending.clear();
    scratch.clear();
    spikes = overflow = 0;
    checksum = kFnvOffset;
    steps_done = 0;
    last_step_ms = last_run_us = 0;
    watched.clear();
  }

  bool fail( const std::string& m )
  {
    error = m;
    loaded = false;
    return false;
  }

  void do_stop()
  {
    if ( prepared )
    {
      try
      {
        nest::cleanup();
      }
      catch ( ... )
      {
      }
      prepared = false;
    }
  }

  void build();
  void collect();
  double read_vm( int32_t neuron ) const;
  std::deque< nest::ConnectionID > find_conns( const Proj& p, int32_t pre_neuron, int32_t post_neuron ) const;
};

void Engine::Impl::build()
{
  nest::reset_kernel();
  register_spike_ring();

  Dictionary k;
  k[ "resolution" ] = spec.resolution_ms;
  k[ "rng_seed" ] = static_cast< long >( spec.seed );
  k[ "print_time" ] = false;
  k[ "verbosity" ] = nest::VerbosityLevel::ERROR;
  if ( g_threads > 1 )
    k[ "local_num_threads" ] = static_cast< long >( g_threads );
  nest::set_kernel_status( k );

  // (1) populations
  for ( size_t i = 0; i < spec.pops.size(); ++i )
  {
    const PopSpec& p = spec.pops[ i ];
    std::string path = "populations[" + std::to_string( i ) + "]";
    nest::NodeCollectionPTR nc;
    try
    {
      nc = nest::create( p.model, static_cast< size_t >( p.n ) );
    }
    catch ( const std::exception& e )
    {
      throw std::runtime_error( path + ".model: " + e.what() );
    }
    if ( not p.params.empty() )
    {
      try
      {
        std::vector< Dictionary > ds{ to_dict( p.params ) };
        nest::set_nc_status( nc, ds );
      }
      catch ( const std::exception& e )
      {
        throw std::runtime_error( path + ".params: " + e.what() );
      }
    }
    pop_nc.push_back( nc );
    for ( int32_t j = 0; j < p.n; ++j )
      neuron_pop.push_back( p.id );
  }

  // (2) input devices
  for ( size_t i = 0; i < spec.inputs.size(); ++i )
  {
    const InputSpec& in = spec.inputs[ i ];
    std::string path = "inputs[" + std::to_string( i ) + "]";
    try
    {
      auto nc = nest::create( in.kind == "dc" ? "dc_generator" : "poisson_generator", 1 );
      Dev d{ in.kind == "dc" ? "amplitude" : "rate", ( *nc )[ 0 ], in.value, false };
      Dictionary v;
      v[ d.param ] = in.value;
      nest::set_node_status( d.node_id, v );
      in_idx[ in.name ] = devs.size();
      devs.push_back( d );
    }
    catch ( const std::exception& e )
    {
      throw std::runtime_error( path + ": " + e.what() );
    }
  }

  // (3) spike recorder
  auto rec = nest::create( "spike_recorder", 1 );
  {
    Dictionary r;
    r[ "record_to" ] = std::string( "spike_ring" );
    std::vector< Dictionary > ds{ r };
    nest::set_nc_status( rec, ds );
  }

  // connections: projections
  for ( size_t i = 0; i < spec.projs.size(); ++i )
  {
    const ProjSpec& p = spec.projs[ i ];
    std::string path = "projections[" + std::to_string( i ) + "]";
    Proj pr{ pop_nc[ p.pre_pop ], pop_nc[ p.post_pop ], p.synapse[ "synapse_model" ].get< std::string >() };
    try
    {
      if ( not p.has_edges )
      {
        Dictionary rule = to_dict( p.rule );
        Dictionary syn = to_dict( p.synapse );
        nest::connect( pr.src, pr.tgt, rule, { syn } );
      }
      else if ( not p.edges.empty() )
      {
        const PopSpec& a = spec.pops[ p.pre_pop ];
        const PopSpec& b = spec.pops[ p.post_pop ];
        size_t n = p.edges.size();
        std::vector< long > src( n ), tgt( n );
        std::vector< double > w( n ), d( n );
        for ( size_t e = 0; e < n; ++e )
        {
          src[ e ] = a.first + p.edges[ e ].first + 1;
          tgt[ e ] = b.first + p.edges[ e ].second + 1;
          w[ e ] = p.edge_wd[ e ][ 0 ];
          d[ e ] = p.edge_wd[ e ][ 1 ];
        }
        // extra constant synapse parameters (e.g. tau_plus) are passed as per-edge columns
        std::vector< std::string > keys;
        std::vector< double > vals;
        for ( auto it = p.synapse.begin(); it != p.synapse.end(); ++it )
          if ( it.key() != "synapse_model" and it.key() != "weight" and it.key() != "delay" )
          {
            if ( not it.value().is_number() )
              throw std::runtime_error( "synapse." + it.key() + ": must be a number for 'edges' projections" );
            keys.push_back( it.key() );
          }
        vals.resize( keys.size() * n );
        for ( size_t kx = 0; kx < keys.size(); ++kx )
          for ( size_t e = 0; e < n; ++e )
            vals[ e * keys.size() + kx ] = p.synapse[ keys[ kx ] ].get< double >();
        nest::connect_arrays( src.data(), tgt.data(), w.data(), d.data(), keys, vals.empty() ? nullptr : vals.data(), n, pr.model );
      }
      projs.push_back( pr );
    }
    catch ( const std::exception& e )
    {
      throw std::runtime_error( path + ": " + e.what() );
    }
  }

  // connections: inputs
  for ( size_t i = 0; i < spec.inputs.size(); ++i )
  {
    const InputSpec& in = spec.inputs[ i ];
    Dictionary rule, syn;
    rule[ "rule" ] = std::string( "all_to_all" );
    syn[ "synapse_model" ] = std::string( "static_synapse" );
    syn[ "weight" ] = in.weight;
    syn[ "delay" ] = in.delay > 0 ? in.delay : spec.resolution_ms;
    try
    {
      nest::connect( nest::make_nodecollection( { devs[ i ].node_id } ), nest::make_nodecollection( node_ids( in.targets ) ), rule, { syn } );
    }
    catch ( const std::exception& e )
    {
      throw std::runtime_error( "inputs[" + std::to_string( i ) + "]: " + e.what() );
    }
  }

  // connections: recorder
  {
    std::vector< int32_t > ids;
    if ( spec.record_all )
      for ( int32_t i = 0; i < spec.n_neurons; ++i )
        ids.push_back( i );
    else
      ids = spec.record_neurons;
    if ( not ids.empty() )
    {
      Dictionary rule, syn;
      rule[ "rule" ] = std::string( "all_to_all" );
      syn[ "synapse_model" ] = std::string( "static_synapse" );
      syn[ "weight" ] = 1.0;
      syn[ "delay" ] = spec.resolution_ms;
      nest::connect( nest::make_nodecollection( node_ids( ids ) ), rec, rule, { syn } );
    }
  }

  // outputs
  out_count.assign( spec.outputs.size(), 0.0 );
  out_of_neuron.assign( spec.n_neurons, {} );
  for ( size_t o = 0; o < spec.outputs.size(); ++o )
  {
    out_idx[ spec.outputs[ o ].name ] = o;
    for ( int32_t n : spec.outputs[ o ].neurons )
      out_of_neuron[ n ].push_back( static_cast< int >( o ) );
  }
  for ( int32_t n : spec.record_voltage )
    watched[ n ];
}

void Engine::Impl::collect()
{
  RingBackend* ring = RingBackend::instance();
  scratch.clear();
  if ( not ring )
    return;
  ring->pop_all( [ this ]( const RingSpike& s ) { scratch.push_back( s ); } );
  std::sort( scratch.begin(), scratch.end(), []( const RingSpike& a, const RingSpike& b ) {
    return a.step != b.step ? a.step < b.step : a.node_id < b.node_id;
  } );
  const double res = spec.resolution_ms;
  for ( const RingSpike& s : scratch )
  {
    const int32_t n = s.node_id - 1;
    if ( n < 0 or n >= spec.n_neurons )
      continue;
    checksum = fnv_bytes( checksum, static_cast< uint32_t >( n ), 4 );
    checksum = fnv_bytes( checksum, static_cast< uint64_t >( s.step ), 8 );
    ++spikes;
    for ( int o : out_of_neuron[ n ] )
      out_count[ o ] += 1.0;
    if ( pending.size() < kPendingCap )
      pending.push_back( Spike{ n, static_cast< double >( s.step ) * res - s.offset } );
    else
      ++overflow;
  }
}

double Engine::Impl::read_vm( int32_t neuron ) const
{
  try
  {
    Dictionary d = nest::get_node_status( static_cast< size_t >( neuron ) + 1 );
    if ( d.known( "V_m" ) )
      return d.get< double >( "V_m" );
  }
  catch ( ... )
  {
  }
  return std::numeric_limits< double >::quiet_NaN();
}

std::deque< nest::ConnectionID > Engine::Impl::find_conns( const Proj& p, int32_t pre_neuron, int32_t post_neuron ) const
{
  Dictionary q;
  q[ "source" ] = pre_neuron < 0 ? p.src : nest::make_nodecollection( { static_cast< size_t >( pre_neuron ) + 1 } );
  q[ "target" ] = post_neuron < 0 ? p.tgt : nest::make_nodecollection( { static_cast< size_t >( post_neuron ) + 1 } );
  q[ "synapse_model" ] = p.model;
  std::deque< nest::ConnectionID > flat;
  for ( auto& dq : nest::get_connections( q ) )
    flat.insert( flat.end(), dq.begin(), dq.end() );
  return flat;
}

// ---------------------------------------------------------------------------------------------------------------

Engine::Engine() : impl_( nullptr )
{
  bool expected = false;
  if ( not g_alive.compare_exchange_strong( expected, true ) )
    throw std::logic_error( "spike::Engine: only one Engine may exist per process (the NEST kernel is a singleton)" );
  try
  {
    if ( not g_nest_ready )
    {
      static char arg0[] = "spike";
      static char* argv[] = { arg0, nullptr };
      char** pargv = argv;
      int argc = 1;
      nest::init_nest( &argc, &pargv );
      g_nest_ready = true;
    }
    impl_ = new Impl();
  }
  catch ( ... )
  {
    g_alive = false;
    throw;
  }
}

Engine::~Engine()
{
  impl_->do_stop();
  delete impl_;
  g_alive = false;
}

bool Engine::load( const std::string& circuit_json )
{
  Impl& m = *impl_;
  m.do_stop();
  m.clear_state();
  if ( not parse_circuit( circuit_json, m.spec, m.error ) )
    return false;
  try
  {
    m.build();
  }
  catch ( const std::exception& e )
  {
    m.error = e.what();
    return false;
  }
  m.loaded = true;
  return true;
}

const std::string& Engine::error() const { return impl_->error; }
int32_t Engine::neuron_count() const { return impl_->loaded ? impl_->spec.n_neurons : 0; }

std::string Engine::neuron_population( int32_t n ) const
{
  return ( n >= 0 and static_cast< size_t >( n ) < impl_->neuron_pop.size() ) ? impl_->neuron_pop[ n ] : std::string();
}

std::vector< ChannelInfo > Engine::inputs() const
{
  std::vector< ChannelInfo > v;
  for ( const auto& in : impl_->spec.inputs )
    v.push_back( { in.name, in.kind, in.targets } );
  return v;
}

std::vector< ChannelInfo > Engine::outputs() const
{
  std::vector< ChannelInfo > v;
  for ( const auto& o : impl_->spec.outputs )
    v.push_back( { o.name, o.kind, o.neurons } );
  return v;
}

void Engine::set_input( const std::string& channel, double value )
{
  Impl& m = *impl_;
  auto it = m.in_idx.find( channel );
  if ( it == m.in_idx.end() )
  {
    m.error = "set_input: unknown input channel '" + channel + "'";
    return;
  }
  Impl::Dev& d = m.devs[ it->second ];
  if ( d.value != value )
  {
    d.value = value;
    d.dirty = true;
  }
}

void Engine::step( double ms )
{
  Impl& m = *impl_;
  if ( not m.loaded )
  {
    m.error = "step: no circuit loaded";
    return;
  }
  const double res = m.spec.resolution_ms;
  const double n = ms / res;
  if ( not( n >= 1.0 ) or std::fabs( n - std::round( n ) ) > 1e-6 )
  {
    m.error = "step: ms must be a positive multiple of the resolution (" + std::to_string( res ) + " ms)";
    return;
  }
  try
  {
    if ( not m.prepared )
    {
      nest::prepare();
      m.prepared = true;
    }
    bool reprep = false;
    for ( auto& d : m.devs )
      if ( d.dirty )
      {
        Dictionary v;
        v[ d.param ] = d.value;
        nest::set_node_status( d.node_id, v );
        d.dirty = false;
        reprep = reprep or d.param == "rate";
      }
    if ( reprep )
    {
      nest::cleanup();
      nest::prepare();
    }
    std::fill( m.out_count.begin(), m.out_count.end(), 0.0 );
    auto t0 = clk::now();
    nest::run( ms );
    m.last_run_us = us_since( t0 );
    m.last_step_ms = ms;
    m.steps_done += static_cast< int64_t >( std::llround( n ) );
    m.collect();
    for ( auto& w : m.watched )
    {
      w.second.push_back( m.read_vm( w.first ) );
      if ( w.second.size() > kVoltageCap )
        w.second.pop_front();
    }
  }
  catch ( const std::exception& e )
  {
    m.error = std::string( "step: " ) + e.what();
  }
}

std::vector< Spike > Engine::drain_spikes()
{
  std::vector< Spike > out;
  out.swap( impl_->pending );
  return out;
}

uint64_t Engine::dropped() const
{
  RingBackend* r = RingBackend::instance();
  return impl_->overflow + ( r ? r->dropped() : 0 );
}

double Engine::output_count( const std::string& channel ) const
{
  auto it = impl_->out_idx.find( channel );
  return it == impl_->out_idx.end() ? 0.0 : impl_->out_count[ it->second ];
}

double Engine::output_rate( const std::string& channel ) const
{
  return impl_->last_step_ms > 0 ? output_count( channel ) * 1000.0 / impl_->last_step_ms : 0.0;
}

void Engine::watch( int32_t neuron )
{
  if ( neuron >= 0 and neuron < neuron_count() )
    impl_->watched[ neuron ];
}

double Engine::voltage( int32_t neuron ) const
{
  Impl& m = *impl_;
  if ( not m.loaded or neuron < 0 or neuron >= m.spec.n_neurons )
    return std::numeric_limits< double >::quiet_NaN();
  auto it = m.watched.find( neuron );
  if ( it != m.watched.end() and not it->second.empty() )
    return it->second.back();
  return m.read_vm( neuron );
}

std::vector< double > Engine::drain_voltage( int32_t neuron )
{
  std::vector< double > v;
  auto it = impl_->watched.find( neuron );
  if ( it != impl_->watched.end() )
  {
    v.assign( it->second.begin(), it->second.end() );
    it->second.clear();
  }
  return v;
}

double Engine::resolution_ms() const { return impl_->loaded ? impl_->spec.resolution_ms : 0.0; }
double Engine::last_run_us() const { return impl_->last_run_us; }

bool Engine::set_weight( int32_t projection, int32_t edge, double weight )
{
  Impl& m = *impl_;
  if ( not m.loaded or projection < 0 or static_cast< size_t >( projection ) >= m.projs.size() or edge < 0 )
  {
    m.error = "set_weight: bad projection or edge index";
    return false;
  }
  try
  {
    const ProjSpec& ps = m.spec.projs[ projection ];
    const Impl::Proj& pr = m.projs[ projection ];
    std::deque< nest::ConnectionID > conns;
    size_t pick = static_cast< size_t >( edge );
    if ( ps.has_edges )
    {
      if ( static_cast< size_t >( edge ) >= ps.edges.size() )
      {
        m.error = "set_weight: edge index out of range";
        return false;
      }
      const auto& e = ps.edges[ edge ];
      pick = 0;  // duplicates of the same (pre, post) pair: the k-th one in file order
      for ( int32_t j = 0; j < edge; ++j )
        if ( ps.edges[ j ] == e )
          ++pick;
      conns = m.find_conns( pr, m.spec.pops[ ps.pre_pop ].first + e.first, m.spec.pops[ ps.post_pop ].first + e.second );
    }
    else
      conns = m.find_conns( pr, -1, -1 );
    if ( pick >= conns.size() )
    {
      m.error = "set_weight: edge index out of range";
      return false;
    }
    Dictionary d;
    d[ "weight" ] = weight;
    nest::set_connection_status( std::deque< nest::ConnectionID >{ conns[ pick ] }, d );
    return true;
  }
  catch ( const std::exception& e )
  {
    m.error = std::string( "set_weight: " ) + e.what();
    return false;
  }
}

bool Engine::set_neuron_param( int32_t neuron, const std::string& key, double value )
{
  Impl& m = *impl_;
  if ( not m.loaded or neuron < 0 or neuron >= m.spec.n_neurons )
  {
    m.error = "set_neuron_param: bad neuron index";
    return false;
  }
  try
  {
    Dictionary d;
    d[ key ] = value;
    nest::set_node_status( static_cast< size_t >( neuron ) + 1, d );
    return true;
  }
  catch ( const std::exception& e )
  {
    m.error = std::string( "set_neuron_param: " ) + e.what();
    return false;
  }
}

RunStats Engine::stats() const
{
  RunStats s;
  s.t_ms = impl_->steps_done * impl_->spec.resolution_ms;
  s.spikes = impl_->spikes;
  s.checksum = impl_->checksum;
  return s;
}

void Engine::stop() { impl_->do_stop(); }

void set_threads( int n ) { g_threads = n < 1 ? 1 : n; }

} // namespace spike
