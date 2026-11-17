// Weight spread (projection keys weight_sd / weight_cv): deterministic per (circuit seed, projection, edge), sign-preserving, errors on bad use.
// Parse-only (no NEST). python/tests/test_weight_spread.py checks the same numbers in the Python mirror.
#include <cmath>
#include <string>
#include "../src/circuit.h"
#include "check.h"

using nlohmann::json;
using namespace spike;

static std::string circuit( const std::string& proj_extra, int seed, double w = 10.0, int n_edges = 400 )
{
  json edges = json::array();
  for ( int i = 0; i < n_edges; ++i )
    edges.push_back( { 0, 0, w, 1.0 } );
  json c = { { "schema_version", 1 }, { "seed", seed }, { "resolution_ms", 0.1 },
    { "populations", { { { "id", "A" }, { "model", "iaf_psc_alpha" }, { "n", 1 } } } },
    { "projections", { { { "pre", "A" }, { "post", "A" }, { "edges", edges } } } } };
  if ( not proj_extra.empty() )
  {
    json x = json::parse( "{" + proj_extra + "}" );
    for ( auto it = x.begin(); it != x.end(); ++it )
      c[ "projections" ][ 0 ][ it.key() ] = it.value();
  }
  return c.dump();
}
static bool parse( const std::string& t, CircuitSpec& s, std::string& err ) { return parse_circuit( t, s, err ); }

int main()
{
  CircuitSpec s0, s1, s2, s3;
  std::string err;
  CHECK_MSG( parse( circuit( "", 7 ), s0, err ), err );
  CHECK( s0.projs[ 0 ].edge_wd[ 5 ][ 0 ] == 10.0 );
  CHECK_MSG( parse( circuit( "\"weight_cv\": 0.3", 7 ), s1, err ), err );
  CHECK_MSG( parse( circuit( "\"weight_cv\": 0.3", 7 ), s2, err ), err );
  CHECK_MSG( parse( circuit( "\"weight_cv\": 0.3", 8 ), s3, err ), err );
  double m = 0, v = 0;
  const auto& e1 = s1.projs[ 0 ].edge_wd;
  bool same = true, differs = false;
  for ( size_t k = 0; k < e1.size(); ++k )
  {
    m += e1[ k ][ 0 ];
    same = same and e1[ k ][ 0 ] == s2.projs[ 0 ].edge_wd[ k ][ 0 ];
    differs = differs or e1[ k ][ 0 ] != s3.projs[ 0 ].edge_wd[ k ][ 0 ];
    CHECK( e1[ k ][ 1 ] == 1.0 );
  }
  m /= e1.size();
  for ( auto& e : e1 )
    v += ( e[ 0 ] - m ) * ( e[ 0 ] - m );
  const double sd = std::sqrt( v / e1.size() );
  std::printf( "  weight_cv 0.3 on 400 edges of 10: mean %.3f sd %.3f; first weights %.15g %.15g %.15g\n", m, sd, e1[ 0 ][ 0 ], e1[ 1 ][ 0 ], e1[ 2 ][ 0 ] );
  CHECK( same );     // deterministic
  CHECK( differs );  // reseeded with the circuit seed
  CHECK( std::fabs( m - 10.0 ) < 0.5 and std::fabs( sd - 3.0 ) < 0.4 );
  // weight_sd: absolute; huge spread never flips the sign
  CircuitSpec s4, s5;
  CHECK_MSG( parse( circuit( "\"weight_sd\": 50.0", 3 ), s4, err ), err );
  CHECK_MSG( parse( circuit( "\"weight_sd\": 50.0", 3, -4.0 ), s5, err ), err );
  bool pos = true, neg = true;
  for ( auto& e : s4.projs[ 0 ].edge_wd )
    pos = pos and e[ 0 ] > 0;
  for ( auto& e : s5.projs[ 0 ].edge_wd )
    neg = neg and e[ 0 ] < 0;
  CHECK( pos and neg );
  // errors
  CircuitSpec bad;
  CHECK( not parse( circuit( "\"weight_cv\": 0.1, \"weight_sd\": 1", 1 ), bad, err ) and err.find( "not both" ) != std::string::npos );
  CHECK( not parse( circuit( "\"weight_sd\": -1", 1 ), bad, err ) and err.find( "weight_sd" ) != std::string::npos );
  {
    json c = json::parse( circuit( "", 1 ) );
    c[ "projections" ][ 0 ].erase( "edges" );
    c[ "projections" ][ 0 ][ "rule" ] = "all_to_all";
    c[ "projections" ][ 0 ][ "weight_cv" ] = 0.1;
    CHECK( not parse( c.dump(), bad, err ) and err.find( "weight_cv" ) != std::string::npos );
  }
  return chk::finish( "test_circuit_spread" );
}
