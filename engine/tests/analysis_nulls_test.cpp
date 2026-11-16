// Tests for the null-model diagnostics: swap acceptance, degenerate-null detection, significance marking, JSON for undefined z.
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

#include "spike/analysis.h"

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

static std::vector< Edge > dense( int n, double keep_every )
{
  std::vector< Edge > e;
  int k = 0;
  for ( int a = 0; a < n; ++a )
    for ( int b = 0; b < n; ++b )
      if ( a != b && ( ++k % static_cast< int >( keep_every ) ) != 0 )
        e.push_back( { a, b, 1.0 } );
  return e;
}

static std::vector< Edge > sparse_random( int n, int m, uint64_t seed )
{
  std::vector< Edge > e;
  uint64_t s = seed;
  while ( static_cast< int >( e.size() ) < m )
  {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    const int a = static_cast< int >( ( s >> 33 ) % n );
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    const int b = static_cast< int >( ( s >> 33 ) % n );
    if ( a != b )
      e.push_back( { a, b, 1.0 } );
  }
  return e;
}

static void test_rewire_stats()
{
  const Graph g = make_graph( sparse_random( 60, 150, 3 ), 60 );
  RewireStats st;
  const Graph r = rewire( g, 11, 10, &st );
  CHECK( st.attempts == static_cast< uint64_t >( g.edge_count() ) * 10 );
  CHECK( st.accepted > 0 && st.accepted <= st.attempts );
  CHECK( st.accepted > st.attempts / 2 );  // sparse graph: most swaps succeed
  CHECK( r.out == rewire( g, 11, 10 ).out );  // the stats overload gives the same graph
  // complete digraph: no swap is possible
  RewireStats full;
  rewire( make_graph( dense( 5, 1000000 ), 5 ), 1, 10, &full );
  CHECK( full.attempts > 0 && full.accepted == 0 );
}

static void test_degenerate_null()
{
  // 6 nodes, nearly complete: almost every swap would create a duplicate edge
  const auto r = motif_zscores( dense( 6, 7 ), 6, Sign::Any, 50, 1 );
  CHECK( r.swap_acceptance < kMinSwapAcceptance );
  CHECK( r.degenerate );
  for ( int c = 0; c < kMotifClasses; ++c )
    CHECK( !motif_is_significant( r, c ) );  // never starred on a degenerate null
  const std::string j = motif_result_json( r );
  CHECK( j.find( "\"degenerate\":true" ) != std::string::npos );
  CHECK( j.find( "\"swap_acceptance\":" ) != std::string::npos );
  CHECK( j.find( "\"significant\":true" ) == std::string::npos );

  // a sparse graph randomises fine
  const auto s = motif_zscores( sparse_random( 80, 160, 5 ), 80, Sign::Any, 30, 1 );
  CHECK( !s.degenerate );
  CHECK( s.swap_acceptance > 0.5 );
  CHECK( s.effective_swaps_per_edge > 1.0 );
  CHECK( s.node_count == 80 && s.edge_count > 100 );
  CHECK( motif_result_json( s ).find( "\"degenerate\":false" ) != std::string::npos );
}

static void test_significance_rule()
{
  // planted feed-forward loops in a sparse random graph: FFL (index 4) is significant
  std::vector< Edge > e = sparse_random( 80, 90, 4 );
  for ( int t = 0; t < 25; ++t )
  {
    const int x = 3 * t, y = x + 1, z = x + 2;
    e.push_back( { x, y, 1 } );
    e.push_back( { y, z, 1 } );
    e.push_back( { x, z, 1 } );
  }
  const auto r = motif_zscores( e, 80, Sign::Any, 40, 5 );
  CHECK( !r.degenerate );
  CHECK( r.z[ 4 ] > 2.0 && r.null_sd[ 4 ] >= kMinNullSd );
  CHECK( motif_is_significant( r, 4 ) );
  // marking requires both |z| > 2 and enough null variance
  for ( int c = 0; c < kMotifClasses; ++c )
    CHECK( motif_is_significant( r, c ) == ( std::isfinite( r.z[ c ] ) && std::fabs( r.z[ c ] ) > kSignificantZ && r.null_sd[ c ] >= kMinNullSd ) );
  // few tiny-variance classes with a large |z| exist here and are not starred
  bool tiny = false;
  for ( int c = 0; c < kMotifClasses; ++c )
    if ( r.null_sd[ c ] > 0 && r.null_sd[ c ] < kMinNullSd && std::fabs( r.z[ c ] ) > kSignificantZ )
    {
      tiny = true;
      CHECK( !motif_is_significant( r, c ) );
    }
  (void)tiny;
}

static void test_nonfinite_z_json()
{
  MotifResult r;
  r.counts.assign( kMotifClasses, 3 );
  r.null_mean.assign( kMotifClasses, 3.0 );
  r.null_sd.assign( kMotifClasses, 0.0 );
  r.z.assign( kMotifClasses, 0.0 );
  r.significant.assign( kMotifClasses, 0 );
  r.z[ 0 ] = std::numeric_limits< double >::infinity();
  r.z[ 1 ] = -std::numeric_limits< double >::infinity();
  const std::string j = motif_result_json( r );
  // +-inf serialises as null with z_defined false and is never significant; a finite z is defined
  CHECK( j.find( "\"z\":null,\"z_defined\":false,\"significant\":false" ) != std::string::npos );
  CHECK( j.find( "\"z\":0,\"z_defined\":true,\"significant\":false" ) != std::string::npos );
  size_t n_null = 0, pos = 0;
  while ( ( pos = j.find( "\"z\":null", pos ) ) != std::string::npos )
    ++n_null, ++pos;
  CHECK( n_null == 2 );
}

int main()
{
  test_rewire_stats();
  test_degenerate_null();
  test_significance_rule();
  test_nonfinite_z_json();
  std::printf( g_fail ? "analysis_nulls_test: %d FAILED\n" : "analysis_nulls_test: all passed\n", g_fail );
  return g_fail ? 1 : 0;
}
