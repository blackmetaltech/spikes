// Tests for the graph queries (hops, shortest paths, degrees, hubs) and the edge-count and block-preserving null models.
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

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

using Nodes = std::vector< int32_t >;

static Graph graph_of( int n, const std::vector< std::pair< int, int > >& pairs )
{
  std::vector< Edge > e;
  for ( const auto& p : pairs )
    e.push_back( { p.first, p.second, 1.0 } );
  return make_graph( e, n );
}

// 0 -> 1, 0 -> 2, 1 -> 3, 2 -> 3, 3 -> 4; node 5 is isolated.
static Graph diamond()
{
  return graph_of( 6, { { 0, 1 }, { 0, 2 }, { 1, 3 }, { 2, 3 }, { 3, 4 } } );
}

static Graph random_graph( int n, int m, uint64_t seed )
{
  std::vector< Edge > e;
  std::set< std::pair< int, int > > seen;
  uint64_t s = seed;
  while ( static_cast< int >( e.size() ) < m )
  {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    const int a = static_cast< int >( ( s >> 33 ) % n );
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    const int b = static_cast< int >( ( s >> 33 ) % n );
    if ( a != b && seen.insert( { a, b } ).second )
      e.push_back( { a, b, 1.0 } );
  }
  return make_graph( e, n );
}

static bool same_graph( const Graph& a, const Graph& b )
{
  return a.n == b.n && a.out == b.out;
}

static bool simple( const Graph& g )  // no self-loops, no multi-edges, targets in range
{
  for ( int32_t a = 0; a < g.n; ++a )
    for ( size_t i = 0; i < g.out[ a ].size(); ++i )
    {
      const int32_t b = g.out[ a ][ i ];
      if ( b == a || b < 0 || b >= g.n || ( i > 0 && g.out[ a ][ i - 1 ] >= b ) )
        return false;
    }
  return true;
}

static bool same_degrees( const Graph& a, const Graph& b )
{
  const Degrees da = degrees( a ), db = degrees( b );
  return da.in == db.in && da.out == db.out;
}

static std::map< std::pair< int, int >, int > block_pairs( const Graph& g, const Nodes& block )
{
  std::map< std::pair< int, int >, int > c;
  for ( int32_t a = 0; a < g.n; ++a )
    for ( int32_t b : g.out[ a ] )
      ++c[ { block[ a ], block[ b ] } ];
  return c;
}

static void test_hops()
{
  const Graph g = diamond();
  CHECK( hops( g, { 0 } ) == Nodes( { 0, 1, 1, 2, 3, -1 } ) );
  CHECK( hops( g, { 1, 2 } ) == Nodes( { -1, 0, 0, 1, 2, -1 } ) );  // several sources
  CHECK( hops( g, { 3 } ) == Nodes( { -1, -1, -1, 0, 1, -1 } ) );   // directed: no way back
  CHECK( hops( g, { 0 }, { 1 } ) == Nodes( { 0, -1, 1, 2, 3, -1 } ) );  // 1 banned, 2 still carries the route
  CHECK( hops( g, { 0 }, { 1, 2 } ) == Nodes( { 0, -1, -1, -1, -1, -1 } ) );
  CHECK( hops( g, { 0 }, { 0 } ) == Nodes( { -1, -1, -1, -1, -1, -1 } ) );  // a banned source is dropped
  CHECK( hops( g, { 99, -1 } ) == Nodes( 6, -1 ) );                         // out-of-range sources are ignored
  CHECK( hops( g, {}, {} ) == Nodes( 6, -1 ) );
}

static void test_paths()
{
  const Graph g = diamond();
  CHECK( shortest_path( g, { 0 }, { 4 } ) == Nodes( { 0, 1, 3, 4 } ) );  // tie 1 / 2: the lower index
  CHECK( shortest_path( g, { 0 }, { 4 }, { 1 } ) == Nodes( { 0, 2, 3, 4 } ) );
  CHECK( shortest_path( g, { 0 }, { 4 }, { 1, 2 } ).empty() );
  CHECK( shortest_path( g, { 0 }, { 4 }, { 3 } ).empty() );
  CHECK( shortest_path( g, { 0 }, { 4 }, { 4 } ).empty() );  // a banned target is unreachable
  CHECK( shortest_path( g, { 0 }, { 5 } ).empty() );
  CHECK( shortest_path( g, { 4 }, { 0 } ).empty() );  // against the edges
  CHECK( shortest_path( g, { 3 }, { 3 } ) == Nodes( { 3 } ) );
  CHECK( shortest_path( g, { 0, 3 }, { 4 } ) == Nodes( { 3, 4 } ) );        // the nearer source wins
  CHECK( shortest_path( g, { 0 }, { 3, 4 } ) == Nodes( { 0, 1, 3 } ) );     // the nearer target wins
  // lexicographic tie-break over the whole path, not just the first step: 0 -> 1 -> 4 -> 5 and 0 -> 2 -> 3 -> 5 are both length 3
  const Graph h = graph_of( 6, { { 0, 2 }, { 0, 1 }, { 2, 3 }, { 3, 5 }, { 1, 4 }, { 4, 5 } } );
  CHECK( shortest_path( h, { 0 }, { 5 } ) == Nodes( { 0, 1, 4, 5 } ) );
}

static void test_on_shortest_paths()
{
  const Graph g = diamond();
  CHECK( on_shortest_paths( g, { 0 }, { 4 } ) == Nodes( { 0, 1, 2, 3, 4 } ) );
  CHECK( on_shortest_paths( g, { 0 }, { 3 } ) == Nodes( { 0, 1, 2, 3 } ) );
  CHECK( on_shortest_paths( g, { 0 }, { 4 }, { 1 } ) == Nodes( { 0, 2, 3, 4 } ) );
  CHECK( on_shortest_paths( g, { 0 }, { 4 }, { 1, 2 } ).empty() );
  CHECK( on_shortest_paths( g, { 0 }, { 5 } ).empty() );
  // a longer detour is not a shortest path: 0 -> 4 directly, 0 -> 1 -> 2 -> 4 is longer
  const Graph h = graph_of( 5, { { 0, 4 }, { 0, 1 }, { 1, 2 }, { 2, 4 }, { 3, 4 } } );
  CHECK( on_shortest_paths( h, { 0 }, { 4 } ) == Nodes( { 0, 4 } ) );
  CHECK( on_shortest_paths( h, { 0, 3 }, { 4 } ) == Nodes( { 0, 3, 4 } ) );
}

static void test_degrees_hubs()
{
  const Graph g = graph_of( 5, { { 0, 1 }, { 0, 2 }, { 0, 3 }, { 1, 0 }, { 3, 2 } } );
  const Degrees d = degrees( g );
  CHECK( d.out == Nodes( { 3, 1, 0, 1, 0 } ) );
  CHECK( d.in == Nodes( { 1, 1, 2, 1, 0 } ) );
  CHECK( hubs( g, 1 ) == Nodes( { 0 } ) );
  CHECK( hubs( g, 3 ) == Nodes( { 0, 1, 2 } ) );  // totals 4, 2, 2, 2, 0: the ties go to the lower index
  CHECK( hubs( g, 99 ).size() == 5 );
  CHECK( hubs( g, 0 ).empty() );
  CHECK( hubs( g, -3 ).empty() );
}

static void test_json_shapes()
{
  const Graph g = diamond();
  const std::string p = graph_path_json( g, { 0 }, { 4 } );
  CHECK( p.find( "\"path\":[0,1,3,4]" ) != std::string::npos );
  CHECK( p.find( "\"on_paths\":[0,1,2,3,4]" ) != std::string::npos );
  CHECK( p.find( "\"hops\":[0,1,1,2,3,-1]" ) != std::string::npos );
  CHECK( p.find( "\"length\":3" ) != std::string::npos && p.find( "\"reachable\":true" ) != std::string::npos );
  const std::string none = graph_path_json( g, { 0 }, { 5 } );
  CHECK( none.find( "\"reachable\":false" ) != std::string::npos && none.find( "\"path\":[]" ) != std::string::npos );
  const std::string d = graph_degrees_json( g, 2 );
  CHECK( d.find( "\"in\":[0,1,1,2,1,0]" ) != std::string::npos && d.find( "\"out\":[2,1,1,1,0,0]" ) != std::string::npos );
  CHECK( d.find( "\"hubs\":[{\"node\":3,\"in\":2,\"out\":1,\"total\":3}" ) != std::string::npos );
}

static void test_shuffle_edges()
{
  const Graph g = random_graph( 40, 200, 7 );
  const Graph s = shuffle_edges( g, 11 );
  CHECK( s.n == g.n );
  CHECK( s.edge_count() == g.edge_count() );
  CHECK( simple( s ) );
  CHECK( !same_degrees( g, s ) );  // only n and m are kept
  CHECK( same_graph( s, shuffle_edges( g, 11 ) ) );
  CHECK( !same_graph( s, shuffle_edges( g, 12 ) ) );
  CHECK( !same_graph( s, g ) );
  // a nearly complete graph is drawn as its complement
  std::vector< std::pair< int, int > > p;
  for ( int a = 0; a < 6; ++a )
    for ( int b = 0; b < 6; ++b )
      if ( a != b && !( a == 0 && b < 5 ) )
        p.push_back( { a, b } );
  const Graph dense = graph_of( 6, p );
  const Graph sd = shuffle_edges( dense, 3 );
  CHECK( sd.edge_count() == dense.edge_count() && simple( sd ) );
  CHECK( shuffle_edges( graph_of( 4, { { 0, 1 }, { 1, 0 }, { 0, 2 }, { 2, 0 }, { 0, 3 }, { 3, 0 }, { 1, 2 }, { 2, 1 }, { 1, 3 }, { 3, 1 }, { 2, 3 }, { 3, 2 } } ), 1 )
      .edge_count() == 12 );  // complete stays complete
  CHECK( shuffle_edges( graph_of( 5, {} ), 1 ).edge_count() == 0 );
  CHECK( shuffle_edges( graph_of( 1, {} ), 1 ).edge_count() == 0 );
}

static void test_rewire_blocks()
{
  const Graph g = random_graph( 45, 300, 21 );
  Nodes block( 45 );
  for ( int i = 0; i < 45; ++i )
    block[ i ] = i % 3;
  RewireStats rs;
  const Graph r = rewire_blocks( g, block, 5, 10, &rs );
  CHECK( r.edge_count() == g.edge_count() );
  CHECK( simple( r ) );
  CHECK( same_degrees( g, r ) );
  CHECK( block_pairs( g, block ) == block_pairs( r, block ) );
  CHECK( rs.attempts == 3000 && rs.accepted > 0 && rs.accepted <= rs.attempts );
  CHECK( !same_graph( g, r ) );
  CHECK( same_graph( r, rewire_blocks( g, block, 5, 10 ) ) );  // the stats pointer changes nothing
  CHECK( !same_graph( r, rewire_blocks( g, block, 6, 10 ) ) );
  // a different block assignment keeps its own counts
  Nodes b2( 45 );
  for ( int i = 0; i < 45; ++i )
    b2[ i ] = i < 10 ? 0 : i < 30 ? 1 : 2;
  const Graph r2 = rewire_blocks( g, b2, 5, 10 );
  CHECK( block_pairs( g, b2 ) == block_pairs( r2, b2 ) && same_degrees( g, r2 ) && simple( r2 ) );
  // one block for everybody is the degree-preserving null, bit for bit
  CHECK( same_graph( rewire_blocks( g, Nodes( 45, 0 ), 5, 10 ), rewire( g, 5, 10 ) ) );
  // zero swaps, or a graph with fewer than two edges: nothing moves
  CHECK( same_graph( rewire_blocks( g, block, 5, 0 ), g ) );
  const Graph one = graph_of( 3, { { 0, 1 } } );
  CHECK( same_graph( rewire_blocks( one, Nodes( 3, 0 ), 5, 10 ), one ) );
}

static void test_degree_overload_matches_old()
{
  const Graph g = random_graph( 30, 150, 3 );
  const MotifResult a = motif_zscores( g, 12, 9, 10 );
  const MotifResult b = motif_zscores( g, NullModel::Degree, {}, 12, 9, 10 );
  CHECK( a.counts == b.counts );
  CHECK( a.null_mean == b.null_mean && a.null_sd == b.null_sd && a.z == b.z );  // exact
  CHECK( a.significant == b.significant );
  CHECK( a.null_samples == b.null_samples && a.node_count == b.node_count && a.edge_count == b.edge_count );
  CHECK( a.swap_attempts == b.swap_attempts && a.swap_accepted == b.swap_accepted && a.swap_acceptance == b.swap_acceptance );
  CHECK( a.effective_swaps_per_edge == b.effective_swaps_per_edge && a.degenerate == b.degenerate );
  CHECK( a.null_counts.empty() && b.null_counts.size() == 12 );
  for ( int k = 0; k < 12; ++k )
    CHECK( b.null_counts[ k ] == motif_census( rewire( g, 9 + k, 10 ) ) );
  CHECK( motif_null_json( b ).find( "\"model\":\"degree\"" ) != std::string::npos );
}

static void test_null_models_ensembles()
{
  const Graph g = random_graph( 36, 150, 5 );
  Nodes block( 36 );
  for ( int i = 0; i < 36; ++i )
    block[ i ] = i / 12;
  const MotifResult e = motif_zscores( g, NullModel::EdgeCount, {}, 10, 4 );
  const MotifResult d = motif_zscores( g, NullModel::Degree, {}, 10, 4 );
  const MotifResult b = motif_zscores( g, NullModel::Blocks, block, 10, 4 );
  CHECK( e.model == NullModel::EdgeCount && d.model == NullModel::Degree && b.model == NullModel::Blocks );
  CHECK( e.null_counts.size() == 10 && d.null_counts.size() == 10 && b.null_counts.size() == 10 );
  CHECK( e.swap_attempts == 0 && !e.degenerate );
  CHECK( b.swap_attempts > 0 && b.swap_accepted > 0 );
  CHECK( e.counts == d.counts && d.counts == b.counts );
  for ( int k = 0; k < 10; ++k )
  {
    CHECK( e.null_counts[ k ] == motif_census( shuffle_edges( g, 4 + k ) ) );
    CHECK( b.null_counts[ k ] == motif_census( rewire_blocks( g, block, 4 + k, 10 ) ) );
  }
  // deterministic per seed, different across seeds
  const MotifResult b2 = motif_zscores( g, NullModel::Blocks, block, 10, 4 );
  const MotifResult b3 = motif_zscores( g, NullModel::Blocks, block, 10, 400 );
  CHECK( b.null_counts == b2.null_counts && b.z == b2.z );
  CHECK( b.null_counts != b3.null_counts );
  CHECK( e.null_counts != motif_zscores( g, NullModel::EdgeCount, {}, 10, 400 ).null_counts );
  // zero nulls
  const MotifResult none = motif_zscores( g, NullModel::Blocks, block, 0, 4 );
  CHECK( none.null_counts.empty() && !none.degenerate && none.null_samples == 0 );
  // the null bench JSON
  const std::string j = motif_null_json( b );
  CHECK( j.find( "\"model\":\"blocks\"" ) != std::string::npos );
  CHECK( j.find( "\"swap_attempts\":" ) != std::string::npos && j.find( "\"null_counts\":[" ) != std::string::npos );
  CHECK( motif_null_json( e ).find( "\"model\":\"edge_count\"" ) != std::string::npos );
}

// Three layers, every edge from each layer to every later layer: a feed-forward loop on every (A, B, C). Against random graphs of
// the same size the loop is wildly over-represented; the block null keeps exactly what built it (the layer structure), so the
// excess disappears.
static void test_ffl_explained_by_layers()
{
  const int L = 10;
  std::vector< std::pair< int, int > > p;
  Nodes block( 3 * L );
  for ( int i = 0; i < 3 * L; ++i )
    block[ i ] = i / L;
  for ( int a = 0; a < 3 * L; ++a )
    for ( int b = 0; b < 3 * L; ++b )
      if ( block[ a ] < block[ b ] )
        p.push_back( { a, b } );
  const Graph g = graph_of( 3 * L, p );
  int ffl = -1;
  for ( int c = 0; c < kMotifClasses; ++c )
    if ( std::string( motif_info( c ).name ) == "feed_forward_loop" )
      ffl = c;
  CHECK( ffl >= 0 );
  if ( ffl < 0 )
    return;
  const MotifResult edge = motif_zscores( g, NullModel::EdgeCount, {}, 30, 1 );
  CHECK( edge.counts[ ffl ] == L * L * L );
  CHECK( edge.z[ ffl ] > 10.0 && motif_is_significant( edge, ffl ) );
  CHECK( edge.null_mean[ ffl ] < 0.5 * static_cast< double >( edge.counts[ ffl ] ) );
  const MotifResult blocks = motif_zscores( g, NullModel::Blocks, block, 30, 1 );
  CHECK( !motif_is_significant( blocks, ffl ) );
  CHECK( blocks.z[ ffl ] == 0.0 );
  for ( const Census& c : blocks.null_counts )
    CHECK( c[ ffl ] == L * L * L );
}

int main()
{
  test_hops();
  test_paths();
  test_on_shortest_paths();
  test_degrees_hubs();
  test_json_shapes();
  test_shuffle_edges();
  test_rewire_blocks();
  test_degree_overload_matches_old();
  test_null_models_ensembles();
  test_ffl_explained_by_layers();
  std::printf( g_fail ? "analysis_graph_test: %d FAILED\n" : "analysis_graph_test: all passed\n", g_fail );
  return g_fail ? 1 : 0;
}
