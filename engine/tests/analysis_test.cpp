// Unit tests for spike::analysis (no NEST needed): motif census vs a brute-force enumerator, nulls, stats.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <iostream>
#include <set>

#include "spike/analysis.h"

using namespace spike;
using namespace spike::analysis;

static int g_fail = 0;
#define CHECK( cond )                                                                                                  \
  do                                                                                                                   \
  {                                                                                                                    \
    if ( !( cond ) )                                                                                                   \
    {                                                                                                                  \
      std::printf( "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond );                                                    \
      ++g_fail;                                                                                                        \
    }                                                                                                                  \
  } while ( 0 )

// Independent classifier: compare a triple against the 13 MAN representatives (edge lists as in networkx.triad_graph).
struct Rep
{
  const char* man;
  std::set< std::pair< int, int > > e;
};
static const std::vector< Rep >& reps()
{
  static const std::vector< Rep > r = {
    { "021D", { { 1, 0 }, { 1, 2 } } }, { "021U", { { 0, 1 }, { 2, 1 } } }, { "021C", { { 0, 1 }, { 1, 2 } } },
    { "111D", { { 0, 2 }, { 1, 2 }, { 2, 0 } } }, { "111U", { { 0, 2 }, { 2, 0 }, { 2, 1 } } },
    { "030T", { { 0, 1 }, { 0, 2 }, { 2, 1 } } }, { "030C", { { 0, 2 }, { 1, 0 }, { 2, 1 } } },
    { "201", { { 0, 1 }, { 0, 2 }, { 1, 0 }, { 2, 0 } } }, { "120D", { { 0, 2 }, { 1, 2 }, { 1, 0 }, { 2, 0 } } },
    { "120U", { { 0, 1 }, { 0, 2 }, { 2, 1 }, { 2, 0 } } }, { "120C", { { 0, 1 }, { 0, 2 }, { 1, 2 }, { 2, 0 } } },
    { "210", { { 0, 1 }, { 0, 2 }, { 1, 2 }, { 2, 1 }, { 2, 0 } } },
    { "300", { { 0, 1 }, { 0, 2 }, { 1, 0 }, { 1, 2 }, { 2, 1 }, { 2, 0 } } } };
  return r;
}

static int classify_bf( const Graph& g, int a, int b, int c )
{
  const int node[ 3 ] = { a, b, c };
  std::set< std::pair< int, int > > e;
  for ( int i = 0; i < 3; ++i )
    for ( int j = 0; j < 3; ++j )
      if ( i != j && g.has_edge( node[ i ], node[ j ] ) )
        e.insert( { i, j } );
  static const int p[ 6 ][ 3 ] = { { 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 } };
  for ( const auto& r : reps() )
    for ( const auto& q : p )
    {
      std::set< std::pair< int, int > > m;
      for ( const auto& [ i, j ] : r.e )
        m.insert( { q[ i ], q[ j ] } );
      if ( m == e )
      {
        for ( int k = 0; k < kMotifClasses; ++k )
          if ( std::string( motif_info( k ).man ) == r.man )
            return k;
      }
    }
  return -1;  // not connected
}

static Census brute( const Graph& g )
{
  Census c( kMotifClasses, 0 );
  for ( int a = 0; a < g.n; ++a )
    for ( int b = a + 1; b < g.n; ++b )
      for ( int d = b + 1; d < g.n; ++d )
      {
        const int k = classify_bf( g, a, b, d );
        if ( k >= 0 )
          ++c[ k ];
      }
  return c;
}

static std::vector< Edge > random_edges( int n, double p, uint32_t seed, double p_neg = 0.0 )
{
  std::mt19937 rng( seed );
  std::uniform_real_distribution< double > u( 0, 1 );
  std::vector< Edge > e;
  for ( int a = 0; a < n; ++a )
    for ( int b = 0; b < n; ++b )
      if ( a != b && u( rng ) < p )
        e.push_back( { a, b, u( rng ) < p_neg ? -1.0 : 1.0 } );
  return e;
}

static void test_classes()
{
  std::set< int > ids;
  for ( int k = 0; k < kMotifClasses; ++k )
  {
    const auto& m = motif_info( k );
    CHECK( m.index == k );
    CHECK( motif_index_from_milo_id( m.milo_id ) == k );
    ids.insert( m.milo_id );
    if ( k )
      CHECK( motif_info( k - 1 ).milo_id < m.milo_id );
  }
  CHECK( ids == std::set< int >( { 6, 12, 14, 36, 38, 46, 74, 78, 98, 102, 108, 110, 238 } ) );
  // each representative lands in the class of its own MAN name, with exactly one subgraph
  for ( const auto& r : reps() )
  {
    std::vector< Edge > e;
    for ( auto [ a, b ] : r.e )
      e.push_back( { a, b, 1.0 } );
    const auto c = motif_census( e, 3 );
    int64_t total = 0;
    for ( auto x : c )
      total += x;
    CHECK( total == 1 );
    int k = -1;
    for ( int i = 0; i < kMotifClasses; ++i )
      if ( c[ i ] == 1 )
        k = i;
    CHECK( k >= 0 && std::string( motif_info( k ).man ) == r.man );
    CHECK( motif_info( k ).edges == static_cast< int >( r.e.size() ) );
  }
  // named ones
  CHECK( motif_info( 4 ).milo_id == 38 && std::string( motif_info( 4 ).name ) == "feed_forward_loop" );
  CHECK( motif_info( 8 ).milo_id == 98 && std::string( motif_info( 8 ).name ) == "feedback_loop" );
}

static void test_census_vs_bruteforce()
{
  for ( uint32_t s = 0; s < 40; ++s )
  {
    const int n = 3 + static_cast< int >( s % 14 );
    const double p = 0.05 + 0.9 * ( ( s * 7 ) % 10 ) / 10.0;
    const Graph g = make_graph( random_edges( n, p, 1000 + s ), n );
    CHECK( motif_census( g ) == brute( g ) );
  }
  // multi-edges and self-loops are ignored
  std::vector< Edge > e = { { 0, 1, 1 }, { 0, 1, 2 }, { 1, 1, 1 }, { 1, 2, 1 } };
  const auto c = motif_census( e, 3 );
  CHECK( c[ 1 ] == 1 );  // chain
}

static void test_sign()
{
  std::vector< Edge > e = { { 0, 1, 1.0 }, { 1, 2, -1.0 }, { 0, 2, 1.0 } };
  const auto all = motif_census( e, 3, Sign::Any );
  CHECK( all[ 4 ] == 1 );  // FFL
  const auto ex = motif_census( e, 3, Sign::Excitatory );
  int64_t t = 0;
  for ( auto x : ex )
    t += x;
  CHECK( t == 1 );  // only two E edges: 0->1, 0->2 form a fan-out
  CHECK( ex[ 0 ] == 1 );
  const auto in = motif_census( e, 3, Sign::Inhibitory );
  t = 0;
  for ( auto x : in )
    t += x;
  CHECK( t == 0 );
}

static void test_rewire()
{
  const Graph g = make_graph( random_edges( 60, 0.1, 7 ), 60 );
  const Graph r = rewire( g, 123 );
  std::vector< int > io( 60 ), ii( 60 ), ro( 60 ), ri( 60 );
  for ( int a = 0; a < 60; ++a )
    for ( int b : g.out[ a ] )
      ++io[ a ], ++ii[ b ];
  for ( int a = 0; a < 60; ++a )
    for ( int b : r.out[ a ] )
    {
      ++ro[ a ], ++ri[ b ];
      CHECK( a != b );
    }
  CHECK( io == ro && ii == ri );
  CHECK( r.edge_count() == g.edge_count() );
  for ( int a = 0; a < 60; ++a )
    CHECK( std::set< int32_t >( r.out[ a ].begin(), r.out[ a ].end() ).size() == r.out[ a ].size() );
  bool differs = false;
  for ( int a = 0; a < 60; ++a )
    differs |= r.out[ a ] != g.out[ a ];
  CHECK( differs );
  CHECK( rewire( g, 123 ).out == r.out );
  CHECK( rewire( g, 124 ).out != r.out );
}

static void test_zscores()
{
  // random graph: |z| small. planted feed-forward triangles: FFL z is large.
  const Graph rnd = make_graph( random_edges( 80, 0.08, 3 ), 80 );
  const auto a = motif_zscores( rnd, 30, 5 );
  for ( int k = 0; k < kMotifClasses; ++k )
    CHECK( std::fabs( a.z[ k ] ) < 5.0 );
  std::vector< Edge > e = random_edges( 80, 0.03, 4 );
  for ( int t = 0; t < 40; ++t )
  {
    const int x = 2 * t, y = ( x + 1 ) % 80, z = ( x + 2 ) % 80;
    e.push_back( { x, y, 1 } );
    e.push_back( { y, z, 1 } );
    e.push_back( { x, z, 1 } );
  }
  const auto b = motif_zscores( e, 80, Sign::Any, 30, 5 );
  CHECK( b.z[ 4 ] > 5.0 );
  const auto b2 = motif_zscores( e, 80, Sign::Any, 30, 5 );
  CHECK( b2.z == b.z );
  CHECK( motif_result_json( b ).find( "feed_forward_loop" ) != std::string::npos );
}

static void test_stats()
{
  std::vector< Spike > sp;
  // neuron 0: regular 10 ms ISI over 1 s (100 Hz); neuron 1: silent; neuron 2: 5 spikes
  for ( int i = 0; i < 100; ++i )
    sp.push_back( { 0, i * 10.0 + 1.0 } );
  for ( int i = 0; i < 5; ++i )
    sp.push_back( { 2, 100.0 * i + 3 } );
  const auto r = firing_rates( sp, 3, 0, 1000 );
  CHECK( std::fabs( r[ 0 ] - 100 ) < 1e-9 && r[ 1 ] == 0 && std::fabs( r[ 2 ] - 5 ) < 1e-9 );
  const auto cv = isi_cv( sp, 3, 0, 1000 );
  CHECK( std::fabs( cv[ 0 ] ) < 1e-9 && std::isnan( cv[ 1 ] ) && std::fabs( cv[ 2 ] ) < 1e-9 );
  // Poisson CV ~ 1
  std::mt19937 rng( 1 );
  std::exponential_distribution< double > ex( 0.05 );  // 50 Hz
  std::vector< Spike > po;
  double t = 0;
  while ( t < 200000 )
  {
    t += ex( rng );
    po.push_back( { 0, t } );
  }
  CHECK( std::fabs( isi_cv( po, 1, 0, 200000 )[ 0 ] - 1.0 ) < 0.05 );
  // synchrony: identical trains chi = 1; independent trains chi small
  std::vector< Spike > sync, async;
  std::uniform_real_distribution< double > u( 0, 1000 );
  std::vector< double > common;
  for ( int i = 0; i < 50; ++i )
    common.push_back( std::floor( u( rng ) ) + 0.5 );
  for ( int n = 0; n < 40; ++n )
    for ( int i = 0; i < 50; ++i )
    {
      sync.push_back( { n, common[ i ] } );
      async.push_back( { n, std::floor( u( rng ) ) + 0.5 } );
    }
  CHECK( std::fabs( golomb_chi( sync, 40, 0, 1000, 1.0 ) - 1.0 ) < 1e-9 );
  const double ca = golomb_chi( async, 40, 0, 1000, 1.0 );
  CHECK( ca > 0.0 && ca < 0.3 );
  CHECK( golomb_chi( {}, 40, 0, 1000 ) == 0.0 );
  // PSTH
  const auto h = psth( sp, 3, {}, 0, 1000, 100 );
  CHECK( h.size() == 10 );
  CHECK( std::fabs( h[ 0 ] - ( 10 + 1 ) * 1000.0 / ( 3 * 100.0 ) ) < 1e-9 );
  const auto h0 = psth( sp, 3, { 0 }, 0, 1000, 100 );
  CHECK( std::fabs( h0[ 5 ] - 100.0 ) < 1e-9 );
  const auto ha = psth_aligned( sp, 3, { 0 }, { 100.0, 500.0 }, 0, 100, 50 );
  CHECK( ha.size() == 2 && std::fabs( ha[ 0 ] - 100.0 ) < 1e-9 );
}

// `analysis_test --census [any|E|I] [n_null] [seed]` is the cross-language hook used by python/tests: stdin is
// "n m" then m lines "pre post weight"; stdout is the motif result as JSON (same numbers as spike.motifs).
static int census_cli( int argc, char** argv )
{
  const std::string s = argc > 2 ? argv[ 2 ] : "any";
  const int n_null = argc > 3 ? std::atoi( argv[ 3 ] ) : 0;
  const uint64_t seed = argc > 4 ? std::strtoull( argv[ 4 ], nullptr, 10 ) : 1;
  const Sign sign = s == "E" ? Sign::Excitatory : s == "I" ? Sign::Inhibitory : Sign::Any;
  int n, m;
  if ( !( std::cin >> n >> m ) )
    return 2;
  std::vector< Edge > e( m );
  for ( auto& x : e )
    std::cin >> x.pre >> x.post >> x.weight;
  std::puts( motif_result_json( motif_zscores( e, n, sign, n_null, seed ) ).c_str() );
  return 0;
}

int main( int argc, char** argv )
{
  if ( argc > 1 && std::string( argv[ 1 ] ) == "--census" )
    return census_cli( argc, argv );
  test_classes();
  test_census_vs_bruteforce();
  test_sign();
  test_rewire();
  test_zscores();
  test_stats();
  std::printf( g_fail ? "analysis_test: %d FAILED\n" : "analysis_test: all passed\n", g_fail );
  return g_fail ? 1 : 0;
}
