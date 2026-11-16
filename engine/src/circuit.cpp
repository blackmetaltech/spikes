#include "circuit.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace spike
{
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
  else if ( model == "tsodyks2_synapse" or model == "quantal_stp_synapse" )
  {
    if ( opt_number( pr.synapse, "U", spath, v ) and not( v >= 0 and v <= 1 ) )
      fail( sub( spath, "U" ), "utilisation U must be in [0, 1]" );
    if ( opt_number( pr.synapse, "u", spath, v ) and not( v >= 0 and v <= 1 ) )
      fail( sub( spath, "u" ), "initial utilisation u must be in [0, 1]" );
    if ( opt_number( pr.synapse, "tau_rec", spath, v ) and not( v > 0 ) )
      fail( sub( spath, "tau_rec" ), "recovery time constant tau_rec must be > 0 ms" );
    if ( opt_number( pr.synapse, "tau_fac", spath, v ) and not( v >= 0 ) )
      fail( sub( spath, "tau_fac" ), "facilitation time constant tau_fac must be >= 0 ms (0 = depression only)" );
    if ( model == "tsodyks2_synapse" )
    {
      if ( opt_number( pr.synapse, "x", spath, v ) and not( v >= 0 and v <= 1 ) )
        fail( sub( spath, "x" ), "initial resources x must be in [0, 1]" );
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

bool is_modulator_name( const std::string& s ) { return s == "NE" or s == "ACh" or s == "DA"; }

bool parse_circuit( const std::string& text, CircuitSpec& c, std::string& error )
{
  try
  {
    json j;
    try
    {
      j = json::parse( text );
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
    if ( auto it = j.find( "seed" ); it != j.end() )
    {
      if ( not it->is_number_integer() or it->get< long long >() < 0 )
        fail( "seed", "must be a non-negative integer" );
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
      if ( not n.is_number_integer() or n.get< long >() < 1 or n.get< long >() > 10000000 )
        fail( sub( path, "n" ), "must be an integer >= 1" );
      ps.n = n.get< int32_t >();
      ps.role = get_string( p, "role", path, false );
      if ( auto it = p.find( "params" ); it != p.end() )
      {
        require_object( *it, sub( path, "params" ) );
        ps.params = *it;
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
            if ( e.key() != "pre" and e.key() != "post" and e.key() != "synapse" and e.key() != "dopamine_source" )
              pr.rule[ e.key() ] = e.value();
        }
        else
        {
          check_keys( p, { "pre", "post", "edges", "synapse", "dopamine_source" }, path );
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
            pr.edges.emplace_back( e[ 0 ].get< int32_t >(), e[ 1 ].get< int32_t >() );
            pr.edge_wd.push_back( { e[ 2 ].get< double >(), d } );
          }
        }
        if ( pr.dopamine_pop >= 0 and pr.synapse[ "synapse_model" ] != "stdp_dopamine_synapse" )
          fail( sub( path, "dopamine_source" ), "only valid with synapse_model stdp_dopamine_synapse" );
        validate_synapse( c, pr, path );
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
        check_keys( p, { "name", "kind", "targets", "weight", "value", "delay" }, path );
        InputSpec in;
        in.name = get_string( p, "name", path, true );
        if ( in.name.empty() or not names.insert( in.name ).second )
          fail( sub( path, "name" ), "must be non-empty and unique among inputs" );
        in.kind = get_string( p, "kind", path, true );
        if ( in.kind != "dc" and in.kind != "poisson" )
          fail( sub( path, "kind" ), "unsupported input kind '" + in.kind + "' (supported: dc, poisson)" );
        parse_selector_list( c, need( p, "targets", path ), sub( path, "targets" ), in.targets );
        sort_unique( in.targets );
        in.weight = get_number( p, "weight", path, 1.0 );
        in.value = get_number( p, "value", path, 0.0 );
        in.delay = get_number( p, "delay", path, -1.0 );
        if ( in.kind == "poisson" and in.value < 0 )
          fail( sub( path, "value" ), "rate must be >= 0" );
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
        if ( o.kind != "rate" and o.kind != "modulator" )
          fail( sub( path, "kind" ), "unsupported output kind '" + o.kind + "' (supported: rate, modulator)" );
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
        sort_unique( o.neurons );
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
        check_keys( p, { "modulator", "pop", "param", "gain", "scale_per_unit", "min", "max" }, path );
        ModulationSpec m;
        m.modulator = get_string( p, "modulator", path, true );
        if ( not is_modulator_name( m.modulator ) )
          fail( sub( path, "modulator" ), "unknown modulator '" + m.modulator + "' (NE, ACh or DA)" );
        m.pop = get_string( p, "pop", path, true );
        m.pop_index = find_pop( c, m.pop );
        if ( m.pop_index < 0 )
          fail( sub( path, "pop" ), "unknown population '" + m.pop + "'" );
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
