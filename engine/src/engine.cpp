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
#include "modules/nestml_models.h" // NESTML custom cell models (models/nestml)

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
constexpr double kDaVtHzPerUnit = 100.0;  // DA concentration 1.0 = 100 Hz of dopamine "spikes" onto every volume_transmitter

int modulator_index( const std::string& n ) { return n == "NE" ? 0 : n == "ACh" ? 1 : n == "DA" ? 2 : -1; }

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

  // neuromodulation (v0.2)
  struct Pool  // release pool of one "modulator" output: spikes -> concentration with first-order decay
  {
    double c = 0, t = 0;
  };
  std::vector< Pool > pools;                 // per output (used by kind "modulator")
  double mod_ext[ 3 ] = { 0, 0, 0 };         // set_modulator()
  double mod_pool[ 3 ] = { 0, 0, 0 };        // sum of release pools at the end of the last step
  struct ModTarget                           // one (population, parameter) written by the modulation table
  {
    int pop;
    std::string param;
    double base;
    double applied;
    bool is_long;
    std::vector< size_t > rows;              // indices into spec.modulation, file order
  };
  std::vector< ModTarget > mod_targets;
  int da_dev = -1;                           // index into devs: poisson_generator feeding the volume_transmitters
  double mod_total( int m ) const { return mod_ext[ m ] + mod_pool[ m ]; }

  std::vector< Spike > pending;
  std::vector< RingSpike > scratch;
  uint64_t spikes = 0, checksum = kFnvOffset, overflow = 0;
  int64_t steps_done = 0;
  bool step_ok = true;
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
    pools.clear();
    for ( int m = 0; m < 3; ++m )
      mod_ext[ m ] = mod_pool[ m ] = 0;
    mod_targets.clear();
    da_dev = -1;
    pending.clear();
    scratch.clear();
    spikes = overflow = 0;
    checksum = kFnvOffset;
    steps_done = 0;
    step_ok = true;
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
  void apply_modulation();
  void decay_pools();
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
  // NESTML models: must follow set_kernel_status (a thread-count change drops every registered model) and ResetKernel
  register_nestml_models();

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
    // Seeded random initial V_m (docs/CIRCUIT-FORMAT.md "Initial membrane potential"): only when the
    // population's params give no V_m and the model has V_m, E_L and V_th. Mirrored in python/spike/circuit.py.
    if ( not p.params.contains( "V_m" ) )
    {
      try
      {
        Dictionary d0 = nest::get_node_status( ( *nc )[ 0 ] );
        if ( d0.known( "V_m" ) and d0.known( "E_L" ) and d0.known( "V_th" ) )
        {
          const double e_l = d0.get< double >( "E_L" ), v_th = d0.get< double >( "V_th" );
          if ( v_th > e_l )
            for ( int32_t j = 0; j < p.n; ++j )
            {
              uint64_t z = spec.seed + static_cast< uint64_t >( p.first + j + 1 ) * 0x9E3779B97F4A7C15ULL;
              z = ( z ^ ( z >> 30 ) ) * 0xBF58476D1CE4E5B9ULL;
              z = ( z ^ ( z >> 27 ) ) * 0x94D049BB133111EBULL;
              z ^= z >> 31;
              const double u = static_cast< double >( z >> 11 ) * ( 1.0 / 9007199254740992.0 );
              Dictionary v;
              v[ "V_m" ] = e_l + u * ( v_th - e_l );
              nest::set_node_status( ( *nc )[ j ], v );
            }
        }
      }
      catch ( const std::exception& e )
      {
        throw std::runtime_error( path + ": initial V_m: " + e.what() );
      }
    }
    pop_nc.push_back( nc );
    for ( int32_t j = 0; j < p.n; ++j )
      neuron_pop.push_back( p.id );
  }

  // modulation table: remember each target parameter's base value (after the population's params were applied)
  for ( size_t i = 0; i < spec.modulation.size(); ++i )
  {
    const ModulationSpec& m = spec.modulation[ i ];
    const std::string path = "modulation[" + std::to_string( i ) + "]";
    ModTarget* mt = nullptr;
    for ( auto& t : mod_targets )
      if ( t.pop == m.pop_index and t.param == m.param )
        mt = &t;
    if ( not mt )
    {
      ModTarget t{ m.pop_index, m.param, 0.0, std::numeric_limits< double >::quiet_NaN(), false, {} };
      Dictionary d0 = nest::get_node_status( ( *pop_nc[ m.pop_index ] )[ 0 ] );
      if ( not d0.known( m.param ) )
        throw std::runtime_error( path + ".param: model " + spec.pops[ m.pop_index ].model + " has no parameter '" + m.param + "'" );
      try
      {
        t.base = d0.get< double >( m.param );
      }
      catch ( const std::exception& )
      {
        try
        {
          t.base = static_cast< double >( d0.get< long >( m.param ) );
          t.is_long = true;
        }
        catch ( const std::exception& )
        {
          throw std::runtime_error( path + ".param: '" + m.param + "' of " + spec.pops[ m.pop_index ].model + " is not a scalar number" );
        }
      }
      mod_targets.push_back( t );
      mt = &mod_targets.back();
    }
    mt->rows.push_back( i );
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

  // (4) dopamine plumbing, only if a stdp_dopamine_synapse projection exists: one volume_transmitter per distinct
  // "dopamine_source" (in order of first use), then one poisson_generator -> parrot_neuron chain (the engine's DA
  // concentration: kDaVtHzPerUnit Hz per unit) that feeds all of them (a device cannot connect to a volume_transmitter directly).
  std::map< int, nest::NodeCollectionPTR > vt_of;
  std::vector< int > vt_order;
  for ( const ProjSpec& p : spec.projs )
    if ( p.dopamine and not vt_of.count( p.dopamine_pop ) )
    {
      try
      {
        vt_of[ p.dopamine_pop ] = nest::create( "volume_transmitter", 1 );
        vt_order.push_back( p.dopamine_pop );
      }
      catch ( const std::exception& e )
      {
        throw std::runtime_error( std::string( "volume_transmitter: " ) + e.what() );
      }
    }
  if ( not vt_order.empty() )
  {
    auto connect_static = [ & ]( nest::NodeCollectionPTR a, nest::NodeCollectionPTR b ) {
      Dictionary rule, syn;
      rule[ "rule" ] = std::string( "all_to_all" );
      syn[ "synapse_model" ] = std::string( "static_synapse" );
      syn[ "weight" ] = 1.0;
      syn[ "delay" ] = spec.resolution_ms;
      nest::connect( a, b, rule, { syn } );
    };
    auto gen = nest::create( "poisson_generator", 1 );
    auto relay = nest::create( "parrot_neuron", 1 );
    da_dev = static_cast< int >( devs.size() );
    {
      Dictionary v;
      v[ "rate" ] = 0.0;
      nest::set_node_status( ( *gen )[ 0 ], v );
    }
    devs.push_back( Dev{ "rate", ( *gen )[ 0 ], 0.0, false } );
    for ( int pp : vt_order )
      connect_static( pop_nc[ pp ], vt_of[ pp ] );
    connect_static( gen, relay );
    for ( int pp : vt_order )
      connect_static( relay, vt_of[ pp ] );
  }

  // connections: projections
  for ( size_t i = 0; i < spec.projs.size(); ++i )
  {
    const ProjSpec& p = spec.projs[ i ];
    std::string path = "projections[" + std::to_string( i ) + "]";
    Proj pr{ pop_nc[ p.pre_pop ], pop_nc[ p.post_pop ], p.synapse[ "synapse_model" ].get< std::string >() };
    try
    {
      if ( p.dopamine )
      {
        // stdp_dopamine_synapse keeps A_plus, Wmax, ... and its volume_transmitter as common properties: set them on a model copy
        pr.model = "stdp_dopamine_synapse__p" + std::to_string( i );
        Dictionary cm = to_dict( p.common );
        cm[ "volume_transmitter" ] = vt_of[ p.dopamine_pop ];
        nest::copy_model( "stdp_dopamine_synapse", pr.model, cm );
      }
      if ( not p.has_edges )
      {
        Dictionary rule = to_dict( p.rule );
        Dictionary syn = to_dict( p.synapse );
        syn[ "synapse_model" ] = pr.model;
        nest::connect( pr.src, pr.tgt, rule, { syn } );
      }
      else if ( not p.edges.empty() )
      {
        const PopSpec& a = spec.pops[ p.pre_pop ];
        const PopSpec& b = spec.pops[ p.post_pop ];
        size_t n = p.edges.size();
        if ( p.gap )
          n *= 2;  // gap junctions are symmetric: every edge also gets its mirror (post -> pre)
        std::vector< long > src( n ), tgt( n );
        std::vector< double > w( n ), d( n );
        const size_t ne = p.edges.size();
        for ( size_t e = 0; e < ne; ++e )
        {
          src[ e ] = a.first + p.edges[ e ].first + 1;
          tgt[ e ] = b.first + p.edges[ e ].second + 1;
          w[ e ] = p.edge_wd[ e ][ 0 ];
          d[ e ] = p.edge_wd[ e ][ 1 ];
          if ( p.gap )
          {
            src[ ne + e ] = tgt[ e ];
            tgt[ ne + e ] = src[ e ];
            w[ ne + e ] = w[ e ];
            d[ ne + e ] = d[ e ];
          }
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
        nest::connect_arrays( src.data(), tgt.data(), w.data(), p.gap ? nullptr : d.data(), keys, vals.empty() ? nullptr : vals.data(), n, pr.model );
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
    if ( in.targets.empty() )
      continue;  // unwired channel: the device exists, nothing is connected
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
  pools.assign( spec.outputs.size(), Pool{} );
  for ( int32_t n : spec.record_voltage )
    watched[ n ];
}

// Neuromodulation, applied between steps: write every (population, parameter) of the modulation table whose value changed.
void Engine::Impl::apply_modulation()
{
  for ( ModTarget& t : mod_targets )
  {
    double v = t.base;
    for ( size_t r : t.rows )
    {
      const ModulationSpec& m = spec.modulation[ r ];
      const double conc = mod_total( modulator_index( m.modulator ) );
      v = m.scale ? v * ( 1.0 + m.amount * conc ) : v + m.amount * conc;
      if ( m.has_min )
        v = std::max( v, m.vmin );
      if ( m.has_max )
        v = std::min( v, m.vmax );
    }
    if ( v == t.applied or ( t.applied != t.applied and v == t.base ) )
      continue;  // unchanged (initially NaN: nothing to write while the value equals the base)
    Dictionary d;
    if ( t.is_long )
      d[ t.param ] = static_cast< long >( std::llround( v ) );
    else
      d[ t.param ] = v;
    std::vector< Dictionary > ds{ d };
    nest::set_nc_status( pop_nc[ t.pop ], ds );
    t.applied = v;
  }
  if ( da_dev >= 0 )
  {
    Dev& d = devs[ da_dev ];
    const double rate = std::max( 0.0, mod_total( 2 ) ) * kDaVtHzPerUnit;
    if ( std::fabs( rate - d.value ) > 0.05 or ( rate == 0.0 and d.value != 0.0 ) )
    {
      d.value = rate;
      d.dirty = true;
    }
  }
}

// Decay every release pool to the end of the step and refresh the per-modulator sums.
void Engine::Impl::decay_pools()
{
  const double now = steps_done * spec.resolution_ms;
  for ( int m = 0; m < 3; ++m )
    mod_pool[ m ] = 0;
  for ( size_t o = 0; o < spec.outputs.size(); ++o )
  {
    const OutputSpec& os = spec.outputs[ o ];
    if ( os.kind != "modulator" )
      continue;
    Pool& pl = pools[ o ];
    pl.c *= std::exp( -std::max( 0.0, now - pl.t ) / os.tau_ms );
    pl.t = std::max( pl.t, now );
    mod_pool[ modulator_index( os.modulator ) ] += pl.c;
  }
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
    const double t_spike = static_cast< double >( s.step ) * res - s.offset;
    for ( int o : out_of_neuron[ n ] )
    {
      out_count[ o ] += 1.0;
      const OutputSpec& os = spec.outputs[ o ];
      if ( os.kind == "modulator" )
      {
        Pool& pl = pools[ o ];
        pl.c = pl.c * std::exp( -std::max( 0.0, t_spike - pl.t ) / os.tau_ms ) + os.gain;
        pl.t = std::max( pl.t, t_spike );
      }
    }
    if ( pending.size() < kPendingCap )
      pending.push_back( Spike{ n, t_spike } );
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
bool Engine::last_step_ok() const { return impl_->step_ok; }
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
  m.step_ok = false;
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
    m.apply_modulation();
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
    // Long steps run in slices of <= 100 ms (identical spikes) so the spike ring cannot overflow.
    const int64_t total = static_cast< int64_t >( std::llround( n ) );
    const int64_t chunk = std::max< int64_t >( 1, static_cast< int64_t >( std::llround( 100.0 / res ) ) );
    m.last_run_us = 0;
    for ( int64_t done = 0; done < total; )
    {
      const int64_t k = std::min( chunk, total - done );
      auto t0 = clk::now();
      nest::run( k == total ? ms : static_cast< double >( k ) * res );
      m.last_run_us += us_since( t0 );
      m.steps_done += k;
      m.collect();
      done += k;
    }
    m.last_step_ms = ms;
    m.decay_pools();
    for ( auto& w : m.watched )
    {
      w.second.push_back( m.read_vm( w.first ) );
      if ( w.second.size() > kVoltageCap )
        w.second.pop_front();
    }
    m.step_ok = true;
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
    if ( ps.gap )
    {
      // gap junctions stay symmetric: set the mirror connection (target -> source) too
      Dictionary q;
      q[ "source" ] = nest::make_nodecollection( { static_cast< size_t >( conns[ pick ].get_target_node_id() ) } );
      q[ "target" ] = nest::make_nodecollection( { static_cast< size_t >( conns[ pick ].get_source_node_id() ) } );
      q[ "synapse_model" ] = pr.model;
      for ( auto& dq : nest::get_connections( q ) )
        if ( not dq.empty() )
          nest::set_connection_status( dq, d );
    }
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

bool Engine::set_modulator( const std::string& name, double conc )
{
  Impl& m = *impl_;
  const int k = modulator_index( name );
  if ( k < 0 )
  {
    m.error = "set_modulator: unknown modulator '" + name + "' (NE, ACh or DA)";
    return false;
  }
  m.mod_ext[ k ] = conc > 0.0 ? conc : 0.0;
  return true;
}

double Engine::modulator( const std::string& name ) const
{
  const int k = modulator_index( name );
  if ( k < 0 )
  {
    impl_->error = "modulator: unknown modulator '" + name + "' (NE, ACh or DA)";
    return 0.0;
  }
  return impl_->mod_total( k );
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

const std::vector< std::string >& modulator_names()
{
  static const std::vector< std::string > n{ "NE", "ACh", "DA" };
  return n;
}

void set_threads( int n ) { g_threads = n < 1 ? 1 : n; }

} // namespace spike
