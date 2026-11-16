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
} // namespace

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
    check_keys( j, { "schema_version", "name", "resolution_ms", "seed", "populations", "projections", "inputs", "outputs", "record", "meta" }, "$" );

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
            if ( e.key() != "pre" and e.key() != "post" and e.key() != "synapse" )
              pr.rule[ e.key() ] = e.value();
        }
        else
        {
          check_keys( p, { "pre", "post", "edges", "synapse" }, path );
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
        check_keys( p, { "name", "kind", "neurons" }, path );
        OutputSpec o;
        o.name = get_string( p, "name", path, true );
        if ( o.name.empty() or not names.insert( o.name ).second )
          fail( sub( path, "name" ), "must be non-empty and unique among outputs" );
        o.kind = get_string( p, "kind", path, false, "rate" );
        if ( o.kind != "rate" )
          fail( sub( path, "kind" ), "unsupported output kind '" + o.kind + "' (supported: rate)" );
        parse_selector_list( c, need( p, "neurons", path ), sub( path, "neurons" ), o.neurons );
        sort_unique( o.neurons );
        c.outputs.push_back( std::move( o ) );
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
