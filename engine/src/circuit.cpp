#include "circuit.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace spike

{
/// Standard-normal draw for the weight spread: splitmix64 keyed by (circuit seed, projection index, edge index), two uniforms, Box-Muller.
/// Mirrored bit for bit in python/spike/circuit.py (edge_spread_normal).
static double spread_normal( unsigned long long seed, size_t proj, size_t edge )
{
  auto u = [ & ]( unsigned i ) {
    const uint64_t key = ( ( ( static_cast< uint64_t >( proj ) + 1 ) << 32 ) | static_cast< uint64_t >( edge ) ) * 2 + i + 1;
    uint64_t z = seed + key * 0x9E3779B97F4A7C15ULL;
    z = ( z ^ ( z >> 30 ) ) * 0xBF58476D1CE4E5B9ULL;
    z = ( z ^ ( z >> 27 ) ) * 0x94D049BB133111EBULL;
    z ^= z >> 31;
    return static_cast< double >( z >> 11 ) * ( 1.0 / 9007199254740992.0 );
  };
  const double u1 = 1.0 - u( 0 ), u2 = u( 1 );  // u1 in (0, 1]
  return std::sqrt( -2.0 * std::log( u1 ) ) * std::cos( 2.0 * 3.14159265358979323846 * u2 );
}

double spread_weight( double w, int kind, double amount, unsigned long long seed, size_t proj, size_t edge )
{
  const double z = spread_normal( seed, proj, edge );
  double w2 = kind == 1 ? w + amount * z : w * ( 1.0 + amount * z );
  const double floor_mag = 1e-3 * std::fabs( w );  // keep the sign (and never exactly 0)
  if ( w > 0 )
    w2 = std::max( w2, floor_mag );
  else if ( w < 0 )
    w2 = std::min( w2, -floor_mag );
  return w2;
}

using nlohmann::json;

namespace
{
struct CircuitError : std::runtime_error
{
  using std::runtime_error::runtime_error;
};

[[noreturn]] void fail( const std::string& path, const std::string& msg )
{
  throw CircuitError( path + ": " + msg );
}

std::string at( const std::string& base, size_t i ) { return base + "[" + std::to_string( i ) + "]"; }
std::string sub( const std::string& base, const char* key ) { return base + "." + key; }

const json& need( const json& o, const char* key, const std::string& path )
{
  auto it = o.find( key );
  if ( it == o.end() )
    fail( sub( path, key ), "required field is missing" );
  return *it;
}

std::string get_string( const json& o, const char* key, const std::string& path, bool required, const std::string& def = "" )
{
  auto it = o.find( key );
  if ( it == o.end() )
  {
    if ( required )
      fail( sub( path, key ), "required field is missing" );
    return def;
  }
  if ( not it->is_string() )
    fail( sub( path, key ), "must be a string" );
  return it->get< std::string >();
}

double get_number( const json& o, const char* key, const std::string& path, double def )
{
  auto it = o.find( key );
  if ( it == o.end() )
    return def;
  if ( not it->is_number() )
    fail( sub( path, key ), "must be a number" );
  return it->get< double >();
}

void check_keys( const json& o, const std::set< std::string >& allowed, const std::string& path )
{
  for ( auto it = o.begin(); it != o.end(); ++it )
    if ( not allowed.count( it.key() ) )
      fail( sub( path, it.key().c_str() ), "unknown key" );
}

void require_object( const json& j, const std::string& path )
{
  if ( not j.is_object() )
    fail( path, "must be an object" );
}

int find_pop( const CircuitSpec& c, const std::string& id )
{
  for ( size_t i = 0; i < c.pops.size(); ++i )
    if ( c.pops[ i ].id == id )
      return static_cast< int >( i );
  return -1;
}

// Selector forms: "pop" | { "pop": id, "idx": [..] } (idx omitted: whole population).
void parse_selector( const CircuitSpec& c, const json& s, const std::string& path, std::vector< int32_t >& out )
{
  std::string pid;
  const json* idx = nullptr;
  if ( s.is_string() )
    pid = s.get< std::string >();
  else if ( s.is_object() )
  {
    check_keys( s, { "pop", "idx" }, path );
    pid = get_string( s, "pop", path, true );
    auto it = s.find( "idx" );
    if ( it != s.end() )
      idx = &*it;
  }
  else
    fail( path, "must be a population id or {\"pop\", \"idx\"}" );
  int p = find_pop( c, pid );
  if ( p < 0 )
    fail( path, "unknown population '" + pid + "'" );
  const PopSpec& ps = c.pops[ p ];
  if ( idx == nullptr )
  {
    for ( int32_t i = 0; i < ps.n; ++i )
      out.push_back( ps.first + i );
    return;
  }
  if ( not idx->is_array() )
    fail( sub( path, "idx" ), "must be an array of integers" );
  for ( size_t k = 0; k < idx->size(); ++k )
  {
    const json& e = ( *idx )[ k ];
    if ( not e.is_number_integer() )
      fail( at( sub( path, "idx" ), k ), "must be an integer" );
    long v = e.get< long >();
    if ( v < 0 or v >= ps.n )
      fail( at( sub( path, "idx" ), k ), "index " + std::to_string( v ) + " out of range for population '" + pid + "' (n=" + std::to_string( ps.n ) + ")" );
    out.push_back( ps.first + static_cast< int32_t >( v ) );
  }
}

void parse_selector_list( const CircuitSpec& c, const json& list, const std::string& path, std::vector< int32_t >& out )
{
  if ( not list.is_array() )
    fail( path, "must be an array" );
  for ( size_t i = 0; i < list.size(); ++i )
    parse_selector( c, list[ i ], at( path, i ), out );
}

void sort_unique( std::vector< int32_t >& v )
{
  std::sort( v.begin(), v.end() );
  v.erase( std::unique( v.begin(), v.end() ), v.end() );
}

bool opt_number( const json& o, const char* key, const std::string& path, double& out )
{
  auto it = o.find( key );
  if ( it == o.end() )
    return false;
  if ( not it->is_number() )
    fail( sub( path, key ), "must be a number" );
  out = it->get< double >();
  return true;
}


}  // namespace

bool is_nestml_model( const std::string& m ) { return m == "butera_burster" or m == "stg_cell" or m == "sa_node_pacemaker" or m == "icc_slow_wave"; }

// Documented physiological weight ceilings (docs/CIRCUIT-FORMAT.md "Weight limits"); mirrored in python/spike/circuit.py (weight_limit).
double weight_limit( const std::string& neuron_model, const char** unit )
{
  const char* u = nullptr;
  double lim = 0;
  if ( is_nestml_model( neuron_model ) )
  {
    u = "nS";
    lim = kMaxNestmlWeightNs;
  }
  else if ( neuron_model.find( "_cond_" ) != std::string::npos )
  {
    u = "nS";
    lim = kMaxConductanceNs;
  }
  else if ( neuron_model.find( "_psc_" ) != std::string::npos or neuron_model.find( "_curr_" ) != std::string::npos )
  {
    u = "pA";
    lim = kMaxCurrentPa;
  }
  if ( unit )
    *unit = u;
  return lim;
}

namespace
{

bool is_gap_model( const std::string& m ) { return m == "hh_psc_alpha_gap" or m == "hh_cond_beta_gap_traub"; }

// Synapse-model specific validation and defaults (docs/CIRCUIT-FORMAT.md "Synapse models"). `path` is "projections[i]".
void validate_synapse( const CircuitSpec& c, ProjSpec& pr, const std::string& path )
{
  const std::string model = pr.synapse[ "synapse_model" ].get< std::string >();
  const std::string spath = sub( path, "synapse" );
  const PopSpec& a = c.pops[ pr.pre_pop ];
  const PopSpec& b = c.pops[ pr.post_pop ];
  double v = 0;

  // weight(s): the single number of a rule projection, or every edge weight
  auto weights = [ & ]() {
    std::vector< double > w;
    if ( pr.has_edges )
      for ( const auto& e : pr.edge_wd )
        w.push_back( e[ 0 ] );
    else if ( pr.synapse.contains( "weight" ) and pr.synapse[ "weight" ].is_number() )
      w.push_back( pr.synapse[ "weight" ].get< double >() );
    else if ( not pr.synapse.contains( "weight" ) )
      w.push_back( 1.0 );  // NEST default
    return w;
  };

  if ( opt_number( pr.synapse, "delay", spath, v ) and not( v <= kMaxDelayMs ) )
    fail( sub( spath, "delay" ), "delay " + std::to_string( v ) + " ms is above the allowed " + std::to_string( static_cast< long >( kMaxDelayMs ) ) + " ms" );

  // weight ceiling of the postsynaptic model (a huge conductance makes the GSL integrator of aeif_cond_* diverge, and NEST then aborts)
  if ( model != "gap_junction" )
  {
    const char* unit = nullptr;
    const double lim = weight_limit( b.model, &unit );
    if ( lim > 0 )
    {
      auto check_w = [ & ]( double w, const std::string& where ) {
        if ( not std::isfinite( w ) or std::fabs( w ) > lim )
          fail( where, "weight " + std::to_string( w ) + " is outside the allowed range |w| <= " + std::to_string( static_cast< long >( lim ) ) + " " + unit + " for " +
                         b.model + " (reference circuits use at most 60 nS / 1500 pA)" );
      };
      if ( pr.has_edges )
        for ( size_t k = 0; k < pr.edge_wd.size(); ++k )
          check_w( pr.edge_wd[ k ][ 0 ], at( sub( path, "edges" ), k ) );
      else if ( pr.synapse.contains( "weight" ) and pr.synapse[ "weight" ].is_number() )
        check_w( pr.synapse[ "weight" ].get< double >(), sub( spath, "weight" ) );
    }
  }

  if ( model == "diffusion_connection" )
  {
    // mean-field projection between siegert_neurons (0.6): weight = J (mV, the postsynaptic amplitude of one spike of the spiking network it stands for), K = the number
    // of such inputs per cell. NEST's drift_factor = tau_m K J / 1000 and diffusion_factor = tau_m K J^2 / 1000 (Hahne et al. 2017, eqs. 28-29, tau_m of the target in ms; rates in 1/s).
    if ( a.model != "siegert_neuron" or b.model != "siegert_neuron" )
      fail( sub( spath, "synapse_model" ), "diffusion_connection joins siegert_neurons (the mean-field cell); '" + a.id + "' is " + a.model + ", '" + b.id + "' is " + b.model );
    if ( pr.synapse.contains( "delay" ) )
      fail( sub( spath, "delay" ), "a diffusion_connection is instantaneous: remove 'delay'" );
    if ( pr.has_edges )
      fail( path, "a diffusion_connection projection uses a rule (all_to_all or one_to_one), not edges" );
    double J = 0, K = 1;
    if ( not opt_number( pr.synapse, "weight", spath, J ) )
      fail( sub( spath, "weight" ), "give weight: J, the postsynaptic amplitude in mV (negative for inhibition)" );
    opt_number( pr.synapse, "K", spath, K );
    if ( not( K > 0 ) )
      fail( sub( spath, "K" ), "K, the number of inputs per cell, must be > 0" );
    double tau_m = 10.0;  // NEST default of siegert_neuron
    if ( b.params.is_object() and b.params.contains( "tau_m" ) and b.params[ "tau_m" ].is_number() )
      tau_m = b.params[ "tau_m" ].get< double >();
    pr.common[ "J" ] = J;
    pr.common[ "K" ] = K;
    pr.common[ "tau_m" ] = tau_m;
    pr.synapse.erase( "weight" );
    pr.synapse.erase( "K" );
    pr.synapse[ "drift_factor" ] = tau_m * 1e-3 * K * J;
    pr.synapse[ "diffusion_factor" ] = tau_m * 1e-3 * K * J * J;
    return;
  }
  if ( model == "rate_connection_instantaneous" or model == "rate_connection_delayed" )
  {
    if ( not is_rate_model( a.model ) or not is_rate_model( b.model ) or a.model == "siegert_neuron" or b.model == "siegert_neuron" )
      fail( sub( spath, "synapse_model" ), model + " joins rate neurons (lin_rate_ipn, tanh_rate_ipn, ...); '" + a.id + "' is " + a.model + ", '" + b.id + "' is " + b.model );
    if ( model == "rate_connection_instantaneous" and pr.synapse.contains( "delay" ) )
      fail( sub( spath, "delay" ), "rate_connection_instantaneous has no delay (use rate_connection_delayed)" );
    return;
  }
  if ( is_rate_model( a.model ) or is_rate_model( b.model ) )
    fail( sub( spath, "synapse_model" ), "rate neurons are joined by rate_connection_instantaneous / rate_connection_delayed (siegert_neuron: diffusion_connection), not " + model );

  if ( model == "gap_junction" )
  {
    pr.gap = true;
    if ( not is_gap_model( a.model ) or not is_gap_model( b.model ) )
      fail( sub( spath, "synapse_model" ),
        "gap_junction needs gap-capable neurons on both sides (hh_psc_alpha_gap or hh_cond_beta_gap_traub); '" + a.id + "' is " + a.model + ", '" + b.id +
          "' is " + b.model );
    if ( pr.synapse.contains( "delay" ) )
      fail( sub( spath, "delay" ), "gap junctions are instantaneous: remove 'delay'" );
    for ( double w : weights() )
      if ( not( w > 0 ) )
        fail( spath + ".weight", "gap_junction weight is a conductance in nS and must be > 0" );
      else if ( w > kMaxGapNs )
        fail( spath + ".weight", "gap_junction weight " + std::to_string( w ) + " nS is above the allowed " + std::to_string( static_cast< long >( kMaxGapNs ) ) + " nS" );
    if ( not pr.has_edges )
    {
      const std::string rn = pr.rule[ "rule" ].get< std::string >();
      if ( rn != "one_to_one" and rn != "all_to_all" )
        fail( sub( path, "rule" ), "gap_junction supports the rules one_to_one and all_to_all (within one population), or edges (got '" + rn + "')" );
      if ( pr.rule.contains( "make_symmetric" ) and pr.rule[ "make_symmetric" ] != true )
        fail( sub( path, "make_symmetric" ), "gap junctions must be symmetric: make_symmetric is always true" );
      pr.rule.erase( "make_symmetric" );
      if ( rn == "one_to_one" )
        pr.rule[ "make_symmetric" ] = true;  // NEST mirrors every connection
      else if ( rn == "all_to_all" and pr.pre_pop != pr.post_pop )
        fail( sub( path, "rule" ), "gap_junction all_to_all needs pre and post to be the same population (use one_to_one or edges between two populations)" );
      if ( rn == "all_to_all" and not pr.rule.contains( "allow_autapses" ) )
        pr.rule[ "allow_autapses" ] = false;
    }
  }
  else if ( model == "tsodyks2_synapse" or model == "tsodyks_synapse" or model == "quantal_stp_synapse" )
  {
    if ( opt_number( pr.synapse, "U", spath, v ) and not( v >= 0 and v <= 1 ) )
      fail( sub( spath, "U" ), "utilisation U must be in [0, 1]" );
    if ( opt_number( pr.synapse, "u", spath, v ) and not( v >= 0 and v <= 1 ) )
      fail( sub( spath, "u" ), "initial utilisation u must be in [0, 1]" );
    if ( opt_number( pr.synapse, "tau_rec", spath, v ) and not( v > 0 ) )
      fail( sub( spath, "tau_rec" ), "recovery time constant tau_rec must be > 0 ms" );
    if ( opt_number( pr.synapse, "tau_fac", spath, v ) and not( v >= 0 ) )
      fail( sub( spath, "tau_fac" ), "facilitation time constant tau_fac must be >= 0 ms (0 = depression only)" );
    if ( model == "tsodyks2_synapse" or model == "tsodyks_synapse" )
    {
      if ( opt_number( pr.synapse, "x", spath, v ) and not( v >= 0 and v <= 1 ) )
        fail( sub( spath, "x" ), "initial resources x must be in [0, 1]" );
      if ( model == "tsodyks_synapse" )
      {
        if ( opt_number( pr.synapse, "y", spath, v ) and not( v >= 0 and v <= 1 ) )
          fail( sub( spath, "y" ), "initial active fraction y must be in [0, 1]" );
        if ( opt_number( pr.synapse, "tau_psc", spath, v ) and not( v > 0 ) )
          fail( sub( spath, "tau_psc" ), "postsynaptic current time constant tau_psc must be > 0 ms" );
      }
    }
    else
    {
      long n = 1;
      if ( auto it = pr.synapse.find( "n" ); it != pr.synapse.end() )
      {
        if ( not it->is_number_integer() or it->get< long >() < 1 )
          fail( sub( spath, "n" ), "number of release sites n must be an integer >= 1" );
        n = it->get< long >();
      }
      if ( auto it = pr.synapse.find( "a" ); it != pr.synapse.end() )
      {
        if ( not it->is_number_integer() or it->get< long >() < 0 )
          fail( sub( spath, "a" ), "number of available sites a must be an integer >= 0" );
        if ( it->get< long >() > n )
          fail( sub( spath, "a" ), "available sites a must not exceed n" );
      }
    }
  }
  else if ( model == "stdp_synapse" or model == "stdp_dopamine_synapse" )
  {
    const bool da = model == "stdp_dopamine_synapse";
    pr.dopamine = da;
    auto w = weights();
    for ( double x : w )
      if ( not( x > 0 ) )
        fail( spath + ".weight",
          model + " supports excitatory (positive) weights only, got " + std::to_string( x ) +
            ". Inhibitory plasticity is not available: use static_synapse (or tsodyks2_synapse) for inhibitory connections" );
    if ( opt_number( pr.synapse, "tau_plus", spath, v ) and not( v > 0 ) )
      fail( sub( spath, "tau_plus" ), "tau_plus must be > 0 ms" );
    // Wmax default: 2 * |weight| (rule projection: the weight; edges: the largest edge weight). Must be given for a weight distribution.
    double wmax = 0;
    if ( opt_number( pr.synapse, "Wmax", spath, wmax ) )
    {
      if ( not( wmax > 0 ) )
        fail( sub( spath, "Wmax" ), "Wmax must be > 0" );
      for ( double x : w )
        if ( x > wmax )
          fail( sub( spath, "Wmax" ), "weight " + std::to_string( x ) + " exceeds Wmax " + std::to_string( wmax ) );
    }
    else if ( w.empty() and not pr.has_edges )
      fail( sub( spath, "Wmax" ), "Wmax is required when weight is not a plain number (the default is 2 * weight)" );
    else if ( w.empty() )
      pr.synapse[ "Wmax" ] = 1.0;  // no edges: nothing is connected
    else
      pr.synapse[ "Wmax" ] = 2.0 * *std::max_element( w.begin(), w.end() );
    if ( not da )
    {
      if ( pr.synapse.contains( "volume_transmitter" ) )
        fail( spath, "volume_transmitter is only valid for stdp_dopamine_synapse" );
    }
    else
    {
      if ( opt_number( pr.synapse, "Wmin", spath, v ) and ( v < 0 or v > pr.synapse[ "Wmax" ].get< double >() ) )
        fail( sub( spath, "Wmin" ), "Wmin must be in [0, Wmax]" );
      for ( const char* k : { "tau_c", "tau_n" } )
        if ( opt_number( pr.synapse, k, spath, v ) and not( v > 0 ) )
          fail( sub( spath, k ), std::string( k ) + " must be > 0 ms" );
      if ( pr.synapse.contains( "volume_transmitter" ) )
        fail( sub( spath, "volume_transmitter" ), "set by the engine from \"dopamine_source\"" );
      if ( pr.dopamine_pop < 0 )
        fail( sub( path, "dopamine_source" ),
          "stdp_dopamine_synapse needs a \"dopamine_source\" population (its spikes drive the volume_transmitter)" );
      // common properties are set on a copy of the model, not per connection
      for ( const char* k : { "A_plus", "A_minus", "Wmin", "Wmax", "b", "tau_c", "tau_n", "tau_plus" } )
        if ( pr.synapse.contains( k ) )
        {
          pr.common[ k ] = pr.synapse[ k ];
          pr.synapse.erase( k );
        }
    }
  }
}
} // namespace

bool vm_init_range( const std::string& model, double e_l, double v_th, double& lo, double& hi )
{
  if ( model == "sa_node_pacemaker" )
  {
    lo = -74.0;
    hi = -45.0;
    return true;
  }
  lo = e_l;
  hi = v_th;
  return v_th > e_l;
}

bool is_rate_model( const std::string& m ) { return m == "siegert_neuron" or m.find( "rate" ) != std::string::npos; }

bool is_modulator_name( const std::string& s ) { return s == "NE" or s == "ACh" or s == "DA"; }

bool parse_circuit( const std::string& text, CircuitSpec& c, std::string& error )
{
  try
  {
    json j;
    try
    {
      // depth guard first: deeply nested values would overflow the stack of every recursive walk over the document
      j = json::parse( text, [ & ]( int depth, json::parse_event_t ev, json& ) {
        if ( ( ev == json::parse_event_t::object_start or ev == json::parse_event_t::array_start ) and depth >= kMaxJsonDepth )
          fail( "$", "JSON is nested deeper than " + std::to_string( kMaxJsonDepth ) + " levels" );
        return true;
      } );
    }
    catch ( const json::parse_error& e )
    {
      fail( "$", std::string( "invalid JSON: " ) + e.what() );
    }
    c = CircuitSpec();
    require_object( j, "$" );
    check_keys( j, { "schema_version", "name", "resolution_ms", "seed", "populations", "projections", "inputs", "outputs", "modulation", "record", "meta" }, "$" );

    const json& sv = need( j, "schema_version", "$" );
    if ( not sv.is_number_integer() or sv.get< long >() != 1 )
      fail( "schema_version", "unsupported version (this engine reads schema_version 1)" );
    c.name = get_string( j, "name", "name", false );
    c.resolution_ms = get_number( j, "resolution_ms", "resolution_ms", 0.1 );
    if ( not( c.resolution_ms >= 0.001 and c.resolution_ms <= 10.0 ) )
      fail( "resolution_ms", "must be between 0.001 and 10 ms" );
    c.seed = 1;  // NEST refuses seed 0
    if ( auto it = j.find( "seed" ); it != j.end() )
    {
      if ( not it->is_number_integer() or not( it->get< double >() >= 1.0 and it->get< double >() <= static_cast< double >( kMaxSeed ) ) )
        fail( "seed", "must be an integer from 1 to " + std::to_string( kMaxSeed ) );
      c.seed = it->get< unsigned long long >();
    }

    // populations
    const json& pops = need( j, "populations", "$" );
    if ( not pops.is_array() or pops.empty() )
      fail( "populations", "must be a non-empty array" );
    for ( size_t i = 0; i < pops.size(); ++i )
    {
      const json& p = pops[ i ];
      std::string path = at( "populations", i );
      require_object( p, path );
      check_keys( p, { "id", "model", "n", "params", "pos", "role" }, path );
      PopSpec ps;
      ps.id = get_string( p, "id", path, true );
      if ( ps.id.empty() )
        fail( sub( path, "id" ), "must not be empty" );
      if ( find_pop( c, ps.id ) >= 0 )
        fail( sub( path, "id" ), "duplicate population id '" + ps.id + "'" );
      ps.model = get_string( p, "model", path, true );
      const json& n = need( p, "n", path );
      if ( not n.is_number_integer() or not( n.get< double >() >= 1.0 ) )
        fail( sub( path, "n" ), "must be an integer >= 1" );
      if ( n.get< double >() > static_cast< double >( kMaxNeurons ) or static_cast< double >( c.n_neurons ) + n.get< double >() > static_cast< double >( kMaxNeurons ) )
        fail( sub( path, "n" ), "this circuit has more than " + std::to_string( kMaxNeurons ) + " neurons in total (the limit that keeps the simulation responsive)" );
      ps.n = n.get< int32_t >();
      ps.role = get_string( p, "role", path, false );
      if ( auto it = p.find( "params" ); it != p.end() )
      {
        require_object( *it, sub( path, "params" ) );
        ps.params = *it;
        if ( auto ie = it->find( "I_e" ); ie != it->end() and ie->is_number() )
        {
          const double cap = is_nestml_model( ps.model ) ? kMaxNestmlDcPa : kMaxDcPa;
          if ( not( std::fabs( ie->get< double >() ) <= cap ) )
            fail( sub( sub( path, "params" ), "I_e" ), "a constant current of " + std::to_string( ie->get< double >() ) + " pA is above the allowed " + std::to_string( static_cast< long >( cap ) ) + " pA" );
        }
      }
      if ( auto it = p.find( "pos" ); it != p.end() )
      {
        if ( not it->is_array() or static_cast< long >( it->size() ) != ps.n )
          fail( sub( path, "pos" ), "must be an array with one [x, y] per neuron (n=" + std::to_string( ps.n ) + ")" );
        for ( size_t k = 0; k < it->size(); ++k )
          if ( not( *it )[ k ].is_array() or ( *it )[ k ].size() != 2 )
            fail( at( sub( path, "pos" ), k ), "must be [x, y]" );
      }
      ps.first = c.n_neurons;
      c.n_neurons += ps.n;
      c.pops.push_back( std::move( ps ) );
    }

    // projections
    if ( auto it = j.find( "projections" ); it != j.end() )
    {
      if ( not it->is_array() )
        fail( "projections", "must be an array" );
      for ( size_t i = 0; i < it->size(); ++i )
      {
        const json& p = ( *it )[ i ];
        std::string path = at( "projections", i );
        require_object( p, path );
        ProjSpec pr;
        pr.pre = get_string( p, "pre", path, true );
        pr.post = get_string( p, "post", path, true );
        pr.pre_pop = find_pop( c, pr.pre );
        pr.post_pop = find_pop( c, pr.post );
        if ( pr.pre_pop < 0 )
          fail( sub( path, "pre" ), "unknown population '" + pr.pre + "'" );
        if ( pr.post_pop < 0 )
          fail( sub( path, "post" ), "unknown population '" + pr.post + "'" );
        if ( auto ds = p.find( "dopamine_source" ); ds != p.end() )
        {
          if ( not ds->is_string() )
            fail( sub( path, "dopamine_source" ), "must be a population id" );
          pr.dopamine_pop = find_pop( c, ds->get< std::string >() );
          if ( pr.dopamine_pop < 0 )
            fail( sub( path, "dopamine_source" ), "unknown population '" + ds->get< std::string >() + "'" );
        }
        bool has_rule = p.contains( "rule" );
        pr.has_edges = p.contains( "edges" );
        if ( has_rule == pr.has_edges )
          fail( path, "exactly one of 'rule' or 'edges' is required" );
        if ( auto s = p.find( "synapse" ); s != p.end() )
        {
          require_object( *s, sub( path, "synapse" ) );
          pr.synapse = *s;
        }
        if ( not pr.synapse.contains( "synapse_model" ) )
          pr.synapse[ "synapse_model" ] = "static_synapse";
        else if ( not pr.synapse[ "synapse_model" ].is_string() )
          fail( sub( sub( path, "synapse" ), "synapse_model" ), "must be a string" );
        if ( has_rule )
        {
          if ( not p[ "rule" ].is_string() )
            fail( sub( path, "rule" ), "must be a string (NEST connection rule name)" );
          for ( auto e = p.begin(); e != p.end(); ++e )
            if ( e.key() != "pre" and e.key() != "post" and e.key() != "synapse" and e.key() != "dopamine_source" and e.key() != "weight_sd" and
                 e.key() != "weight_cv" )
              pr.rule[ e.key() ] = e.value();
          // weight spread on a rule projection: drawn per connection after the connect (engine.cpp), ordered by (pre, post, port)
          const bool has_sd = p.contains( "weight_sd" ), has_cv = p.contains( "weight_cv" );
          if ( has_sd and has_cv )
            fail( path, "give weight_sd or weight_cv, not both" );
          if ( has_sd or has_cv )
          {
            const std::string key = has_sd ? "weight_sd" : "weight_cv";
            if ( not p[ key ].is_number() or not( p[ key ].get< double >() >= 0.0 ) )
              fail( sub( path, key.c_str() ), "must be a number >= 0" );
            const std::string sm = pr.synapse[ "synapse_model" ].get< std::string >();
            if ( sm == "gap_junction" or sm.rfind( "stdp", 0 ) == 0 )
              fail( sub( path, key.c_str() ), "weight spread on a rule projection is not supported for " + sm + " (use an 'edges' projection)" );
            pr.spread = has_sd ? 1 : 2;
            pr.spread_amount = p[ key ].get< double >();
          }
        }
        else
        {
          check_keys( p, { "pre", "post", "edges", "synapse", "dopamine_source", "weight_sd", "weight_cv" }, path );
          const json& ed = p[ "edges" ];
          if ( not ed.is_array() )
            fail( sub( path, "edges" ), "must be an array of [pre_idx, post_idx, weight, delay]" );
          const PopSpec& a = c.pops[ pr.pre_pop ];
          const PopSpec& b = c.pops[ pr.post_pop ];
          for ( size_t k = 0; k < ed.size(); ++k )
          {
            std::string ep = at( sub( path, "edges" ), k );
            const json& e = ed[ k ];
            if ( not e.is_array() or e.size() != 4 )
              fail( ep, "must be [pre_idx, post_idx, weight, delay]" );
            if ( not e[ 0 ].is_number_integer() or e[ 0 ].get< long >() < 0 or e[ 0 ].get< long >() >= a.n )
              fail( ep, "pre_idx out of range for population '" + a.id + "' (n=" + std::to_string( a.n ) + ")" );
            if ( not e[ 1 ].is_number_integer() or e[ 1 ].get< long >() < 0 or e[ 1 ].get< long >() >= b.n )
              fail( ep, "post_idx out of range for population '" + b.id + "' (n=" + std::to_string( b.n ) + ")" );
            if ( not e[ 2 ].is_number() or not e[ 3 ].is_number() )
              fail( ep, "weight and delay must be numbers" );
            double d = e[ 3 ].get< double >();
            if ( not( d >= c.resolution_ms ) )
              fail( ep, "delay must be >= resolution_ms" );
            if ( not( d <= kMaxDelayMs ) )
              fail( ep, "delay " + std::to_string( d ) + " ms is above the allowed " + std::to_string( static_cast< long >( kMaxDelayMs ) ) + " ms" );
            pr.edges.emplace_back( e[ 0 ].get< int32_t >(), e[ 1 ].get< int32_t >() );
            pr.edge_wd.push_back( { e[ 2 ].get< double >(), d } );
          }
          // weight spread (docs/CIRCUIT-FORMAT.md): w' = w + sd * z (weight_sd) or w * (1 + cv * z) (weight_cv), z ~ N(0,1) seeded per (circuit seed, projection, edge)
          const bool has_sd = p.contains( "weight_sd" ), has_cv = p.contains( "weight_cv" );
          if ( has_sd and has_cv )
            fail( path, "give weight_sd or weight_cv, not both" );
          if ( has_sd or has_cv )
          {
            const std::string key = has_sd ? "weight_sd" : "weight_cv";
            if ( not p[ key ].is_number() or not( p[ key ].get< double >() >= 0.0 ) )
              fail( sub( path, key.c_str() ), "must be a number >= 0" );
            const double amount = p[ key ].get< double >();
            const size_t proj_index = c.projs.size();
            for ( size_t k = 0; k < pr.edge_wd.size(); ++k )
              pr.edge_wd[ k ][ 0 ] = spread_weight( pr.edge_wd[ k ][ 0 ], has_sd ? 1 : 2, amount, c.seed, proj_index, k );
          }
        }
        if ( pr.dopamine_pop >= 0 and pr.synapse[ "synapse_model" ] != "stdp_dopamine_synapse" )
          fail( sub( path, "dopamine_source" ), "only valid with synapse_model stdp_dopamine_synapse" );
        validate_synapse( c, pr, path );
        {  // connection budget: an estimate from the rule, before anything is built
          const double npre = c.pops[ pr.pre_pop ].n, npost = c.pops[ pr.post_pop ].n;
          auto num = [ & ]( const char* k, double def ) {
            auto f = pr.rule.find( k );
            return f != pr.rule.end() and f->is_number() ? f->get< double >() : def;
          };
          double est = npre * npost;
          if ( pr.has_edges )
            est = static_cast< double >( pr.edges.size() );
          else
          {
            const std::string rn = pr.rule.value( "rule", std::string() );
            if ( rn == "one_to_one" )
              est = std::min( npre, npost );
            else if ( rn == "fixed_indegree" )
              est = std::max( 0.0, num( "indegree", 0.0 ) ) * npost;
            else if ( rn == "fixed_outdegree" )
              est = std::max( 0.0, num( "outdegree", 0.0 ) ) * npre;
            else if ( rn == "fixed_total_number" )
              est = std::max( 0.0, num( "N", 0.0 ) );
            else if ( rn == "pairwise_bernoulli" )
              est = std::min( est, std::max( 0.0, num( "p", 1.0 ) ) * npre * npost );
          }
          if ( pr.gap )
            est *= 2;
          c.n_connections += est;
          if ( not( est <= kMaxConnections ) or not( c.n_connections <= kMaxConnections ) )
            fail( path, "this circuit would make about " + std::to_string( static_cast< long long >( std::min( c.n_connections, 1e18 ) ) ) + " connections; the limit is " +
                          std::to_string( static_cast< long long >( kMaxConnections ) ) );
        }
        c.projs.push_back( std::move( pr ) );
      }
    }

    // inputs
    std::set< std::string > names;
    if ( auto it = j.find( "inputs" ); it != j.end() )
    {
      if ( not it->is_array() )
        fail( "inputs", "must be an array" );
      for ( size_t i = 0; i < it->size(); ++i )
      {
        const json& p = ( *it )[ i ];
        std::string path = at( "inputs", i );
        require_object( p, path );
        check_keys( p, { "name", "kind", "targets", "weight", "value", "delay", "spike_times", "rate", "amplitude", "freq_hz", "phase", "rate_max", "rate_base", "kappa", "period_deg" }, path );
        InputSpec in;
        in.name = get_string( p, "name", path, true );
        if ( in.name.empty() or not names.insert( in.name ).second )
          fail( sub( path, "name" ), "must be non-empty and unique among inputs" );
        in.kind = get_string( p, "kind", path, true );
        if ( in.kind != "dc" and in.kind != "poisson" and in.kind != "spike_generator" and in.kind != "sinusoidal_poisson" and in.kind != "rate_drive" and in.kind != "tuned_poisson" )
          fail( sub( path, "kind" ), "unsupported input kind '" + in.kind + "' (supported: dc, poisson, spike_generator, sinusoidal_poisson, rate_drive, tuned_poisson)" );
        parse_selector_list( c, need( p, "targets", path ), sub( path, "targets" ), in.targets );
        sort_unique( in.targets );
        {  // rate neurons take only rate_drive; every other kind needs spiking (or current-driven) targets
          auto pop_of = [ & ]( int32_t n ) -> const PopSpec& {
            for ( const auto& q : c.pops )
              if ( n >= q.first and n < q.first + q.n )
                return q;
            return c.pops.front();
          };
          for ( int32_t n : in.targets )
          {
            const bool rate = is_rate_model( pop_of( n ).model );
            if ( rate and in.kind != "rate_drive" )
              fail( sub( path, "targets" ), "'" + pop_of( n ).id + "' is a rate population: it takes input kind \"rate_drive\", not " + in.kind );
            if ( not rate and in.kind == "rate_drive" )
              fail( sub( path, "targets" ), "'" + pop_of( n ).id + "' is not a rate population: \"rate_drive\" drives rate neurons only" );
          }
        }
        in.weight = get_number( p, "weight", path, 1.0 );
        in.value = get_number( p, "value", path, 0.0 );
        in.delay = get_number( p, "delay", path, -1.0 );
        if ( not std::isfinite( in.weight ) or not std::isfinite( in.value ) )
          fail( path, "weight and value must be finite numbers" );
        if ( in.delay > kMaxDelayMs )
          fail( sub( path, "delay" ), "delay " + std::to_string( in.delay ) + " ms is above the allowed " + std::to_string( static_cast< long >( kMaxDelayMs ) ) + " ms" );
        if ( in.kind != "rate_drive" and in.kind != "tuned_poisson" )
        {
          // an input weight is a synaptic weight onto the targets: same ceiling as a projection; a dc input's current is weight * value
          for ( int32_t n : in.targets )
          {
            const PopSpec* q = nullptr;
            for ( const auto& pp : c.pops )
              if ( n >= pp.first and n < pp.first + pp.n )
                q = &pp;
            if ( not q )
              continue;
            const char* unit = nullptr;
            const double lim = weight_limit( q->model, &unit );
            if ( lim > 0 and in.kind != "dc" and std::fabs( in.weight ) > lim )
              fail( sub( path, "weight" ), "weight " + std::to_string( in.weight ) + " is outside the allowed range |w| <= " + std::to_string( static_cast< long >( lim ) ) + " " + unit +
                                               " for " + q->model );
            if ( in.kind == "dc" )
            {
              const double cap = is_nestml_model( q->model ) ? kMaxNestmlDcPa : kMaxDcPa;
              if ( std::fabs( in.weight * in.value ) > cap )
                fail( sub( path, "value" ), "a constant current of " + std::to_string( in.weight * in.value ) + " pA onto '" + q->id + "' is above the allowed " + std::to_string( static_cast< long >( cap ) ) + " pA" );
            }
          }
        }
        const bool sg = in.kind == "spike_generator", sp = in.kind == "sinusoidal_poisson";
        if ( not sg and p.contains( "spike_times" ) )
          fail( sub( path, "spike_times" ), "only valid for kind \"spike_generator\"" );
        for ( const char* k : { "rate", "amplitude", "freq_hz", "phase" } )
          if ( not sp and p.contains( k ) )
            fail( sub( path, k ), "only valid for kind \"sinusoidal_poisson\"" );
        if ( ( sg or sp ) and p.contains( "value" ) )
          fail( sub( path, "value" ), sg ? "a spike_generator has no value: give \"spike_times\" (ms)" : "a sinusoidal_poisson input has no value: give \"rate\" (mean, Hz), \"amplitude\" (Hz), \"freq_hz\" and \"phase\" (degrees)" );
        if ( in.kind == "poisson" and in.value < 0 )
          fail( sub( path, "value" ), "rate must be >= 0" );
        if ( in.kind == "poisson" and in.value > kMaxRateHz )
          fail( sub( path, "value" ), "rate " + std::to_string( in.value ) + " Hz is above the allowed " + std::to_string( static_cast< long >( kMaxRateHz ) ) + " Hz" );
        for ( const char* k : { "rate_max", "rate_base", "kappa", "period_deg" } )
          if ( in.kind != "tuned_poisson" and p.contains( k ) )
            fail( sub( path, k ), "only valid for kind \"tuned_poisson\"" );
        if ( in.kind == "tuned_poisson" )
        {
          in.rate_max = get_number( p, "rate_max", path, in.rate_max );
          in.rate_base = get_number( p, "rate_base", path, in.rate_base );
          in.kappa = get_number( p, "kappa", path, in.kappa );
          in.period_deg = get_number( p, "period_deg", path, in.period_deg );
          if ( not( in.rate_base >= 0 and in.rate_max >= in.rate_base ) )
            fail( sub( path, "rate_max" ), "need 0 <= rate_base <= rate_max (Hz)" );
          if ( in.rate_max > kMaxRateHz )
            fail( sub( path, "rate_max" ), "rate_max " + std::to_string( in.rate_max ) + " Hz is above the allowed " + std::to_string( static_cast< long >( kMaxRateHz ) ) + " Hz" );
          if ( not( in.kappa <= 1e4 ) )
            fail( sub( path, "kappa" ), "kappa is above the allowed 10000" );
          if ( not( in.kappa >= 0 ) )
            fail( sub( path, "kappa" ), "kappa (tuning sharpness) must be >= 0" );
          if ( not( in.period_deg > 0 ) )
            fail( sub( path, "period_deg" ), "period_deg must be > 0 (360 for a direction, 180 for an orientation)" );
        }
        if ( sg )
        {
          if ( auto it = p.find( "spike_times" ); it != p.end() )
          {
            if ( not it->is_array() )
              fail( sub( path, "spike_times" ), "must be an array of times in ms" );
            for ( size_t k = 0; k < it->size(); ++k )
            {
              const std::string tp = at( sub( path, "spike_times" ), k );
              if ( not( *it )[ k ].is_number() )
                fail( tp, "must be a number (ms)" );
              const double t = ( *it )[ k ].get< double >();
              if ( not( t > 0.0 ) )
                fail( tp, "spike time must be > 0 ms" );
              const double steps = t / c.resolution_ms;
              if ( std::fabs( steps - std::round( steps ) ) > 1e-6 )
                fail( tp, "spike time " + std::to_string( t ) + " ms is not a multiple of resolution_ms (" + std::to_string( c.resolution_ms ) + ")" );
              if ( not in.spike_times.empty() and t < in.spike_times.back() )
                fail( tp, "spike times must be sorted in ascending order" );
              in.spike_times.push_back( std::round( steps ) * c.resolution_ms );
            }
          }
        }
        if ( sp )
        {
          in.value = get_number( p, "rate", path, 0.0 );
          in.amplitude = get_number( p, "amplitude", path, 0.0 );
          in.freq_hz = get_number( p, "freq_hz", path, 0.0 );
          in.phase = get_number( p, "phase", path, 0.0 );
          if ( not( in.value >= 0 ) )
            fail( sub( path, "rate" ), "mean rate must be >= 0 Hz" );
          if ( not( in.amplitude >= 0 ) )
            fail( sub( path, "amplitude" ), "amplitude must be >= 0 Hz" );
          if ( in.value > kMaxRateHz or in.amplitude > kMaxRateHz )
            fail( sub( path, in.value > kMaxRateHz ? "rate" : "amplitude" ), "rate above the allowed " + std::to_string( static_cast< long >( kMaxRateHz ) ) + " Hz" );
          if ( not( in.freq_hz >= 0 and in.freq_hz * c.resolution_ms / 1000.0 < 0.5 ) )
            fail( sub( path, "freq_hz" ), "frequency must be >= 0 and below the Nyquist limit of the resolution (" + std::to_string( 500.0 / c.resolution_ms ) + " Hz)" );
        }
        c.inputs.push_back( std::move( in ) );
      }
    }

    // outputs
    names.clear();
    if ( auto it = j.find( "outputs" ); it != j.end() )
    {
      if ( not it->is_array() )
        fail( "outputs", "must be an array" );
      for ( size_t i = 0; i < it->size(); ++i )
      {
        const json& p = ( *it )[ i ];
        std::string path = at( "outputs", i );
        require_object( p, path );
        check_keys( p, { "name", "kind", "neurons", "modulator", "gain", "tau_ms" }, path );
        OutputSpec o;
        o.name = get_string( p, "name", path, true );
        if ( o.name.empty() or not names.insert( o.name ).second )
          fail( sub( path, "name" ), "must be non-empty and unique among outputs" );
        o.kind = get_string( p, "kind", path, false, "rate" );
        if ( o.kind != "rate" and o.kind != "modulator" and o.kind != "population_vector" and o.kind != "rate_value" )
          fail( sub( path, "kind" ), "unsupported output kind '" + o.kind + "' (supported: rate, modulator, population_vector, rate_value)" );
        if ( o.kind == "modulator" )
        {
          o.modulator = get_string( p, "modulator", path, true );
          if ( not is_modulator_name( o.modulator ) )
            fail( sub( path, "modulator" ), "unknown modulator '" + o.modulator + "' (NE, ACh or DA)" );
          o.gain = get_number( p, "gain", path, o.gain );
          o.tau_ms = get_number( p, "tau_ms", path, o.tau_ms );
          if ( not( o.gain >= 0 ) )
            fail( sub( path, "gain" ), "must be >= 0 (concentration added per spike)" );
          if ( not( o.tau_ms > 0 ) )
            fail( sub( path, "tau_ms" ), "must be > 0" );
        }
        else if ( p.contains( "modulator" ) or p.contains( "gain" ) or p.contains( "tau_ms" ) )
          fail( path, "'modulator', 'gain' and 'tau_ms' are only valid for kind \"modulator\"" );
        parse_selector_list( c, need( p, "neurons", path ), sub( path, "neurons" ), o.neurons );
        if ( o.kind == "population_vector" )
        {
          // ring order = the order the selectors list the neurons (neuron k of N sits at angle 2 pi k / N): no sorting, no repeats
          std::vector< int32_t > u = o.neurons;
          sort_unique( u );
          if ( u.size() != o.neurons.size() )
            fail( sub( path, "neurons" ), "a population_vector lists every neuron once (it is an ordered ring)" );
        }
        else
          sort_unique( o.neurons );
        if ( o.kind == "rate_value" )
          for ( int32_t n : o.neurons )
            for ( const auto& q : c.pops )
              if ( n >= q.first and n < q.first + q.n and not is_rate_model( q.model ) )
                fail( sub( path, "neurons" ), "rate_value reads rate neurons; '" + q.id + "' is " + q.model );
        c.outputs.push_back( std::move( o ) );
      }
    }

    // modulation table
    if ( auto it = j.find( "modulation" ); it != j.end() )
    {
      if ( not it->is_array() )
        fail( "modulation", "must be an array" );
      for ( size_t i = 0; i < it->size(); ++i )
      {
        const json& p = ( *it )[ i ];
        std::string path = at( "modulation", i );
        require_object( p, path );
        check_keys( p, { "modulator", "pop", "projection", "param", "gain", "scale_per_unit", "min", "max" }, path );
        ModulationSpec m;
        m.modulator = get_string( p, "modulator", path, true );
        if ( not is_modulator_name( m.modulator ) )
          fail( sub( path, "modulator" ), "unknown modulator '" + m.modulator + "' (NE, ACh or DA)" );
        if ( p.contains( "pop" ) == p.contains( "projection" ) )
          fail( path, "exactly one of 'pop' (a neuron parameter) or 'projection' (a synapse parameter) is required" );
        if ( p.contains( "pop" ) )
        {
          m.pop = get_string( p, "pop", path, true );
          m.pop_index = find_pop( c, m.pop );
          if ( m.pop_index < 0 )
            fail( sub( path, "pop" ), "unknown population '" + m.pop + "'" );
        }
        else
        {
          const json& pj = p[ "projection" ];
          if ( not pj.is_number_integer() or pj.get< long >() < 0 or pj.get< long >() >= static_cast< long >( c.projs.size() ) )
            fail( sub( path, "projection" ), "must be a projection number (0-based, file order; this circuit has " + std::to_string( c.projs.size() ) + ")" );
          m.proj_index = pj.get< int >();
          const std::string sm = c.projs[ m.proj_index ].synapse[ "synapse_model" ].get< std::string >();
          if ( sm == "gap_junction" )
            fail( sub( path, "projection" ), "gap_junction projections cannot be modulated" );
          const std::string prm = get_string( p, "param", path, true );
          if ( prm == "weight" and sm.rfind( "stdp", 0 ) == 0 )
            fail( sub( path, "param" ), "the weight of a plastic " + sm + " projection is owned by the plasticity rule and cannot be modulated" );
          if ( prm == "delay" or prm == "synapse_model" or prm == "port" or prm == "target" or prm == "source" )
            fail( sub( path, "param" ), "'" + prm + "' cannot be modulated" );
        }
        m.param = get_string( p, "param", path, true );
        if ( p.contains( "gain" ) == p.contains( "scale_per_unit" ) )
          fail( path, "exactly one of 'gain' (value += gain * conc) or 'scale_per_unit' (value *= 1 + scale_per_unit * conc) is required" );
        m.scale = p.contains( "scale_per_unit" );
        m.amount = get_number( p, m.scale ? "scale_per_unit" : "gain", path, 0.0 );
        m.has_min = p.contains( "min" );
        m.has_max = p.contains( "max" );
        m.vmin = get_number( p, "min", path, 0.0 );
        m.vmax = get_number( p, "max", path, 0.0 );
        if ( m.has_min and m.has_max and m.vmin > m.vmax )
          fail( path, "min must be <= max" );
        c.modulation.push_back( std::move( m ) );
      }
    }

    // record
    if ( auto it = j.find( "record" ); it != j.end() )
    {
      require_object( *it, "record" );
      check_keys( *it, { "spikes", "voltage" }, "record" );
      if ( auto s = it->find( "spikes" ); s != it->end() )
      {
        if ( s->is_string() )
        {
          std::string v = s->get< std::string >();
          if ( v == "all" )
            c.record_all = true;
          else if ( v == "none" )
          {
            c.record_all = false;
          }
          else
            fail( "record.spikes", "must be \"all\", \"none\" or a list of neuron selectors" );
        }
        else
        {
          c.record_all = false;
          parse_selector_list( c, *s, "record.spikes", c.record_neurons );
          sort_unique( c.record_neurons );
        }
      }
      if ( auto v = it->find( "voltage" ); v != it->end() )
      {
        parse_selector_list( c, *v, "record.voltage", c.record_voltage );
        sort_unique( c.record_voltage );
      }
    }
    if ( auto it = j.find( "meta" ); it != j.end() )
      require_object( *it, "meta" );
    return true;
  }
  catch ( const CircuitError& e )
  {
    error = e.what();
    return false;
  }
  catch ( const std::exception& e )
  {
    error = std::string( "$: " ) + e.what();
    return false;
  }
}

} // namespace spike
