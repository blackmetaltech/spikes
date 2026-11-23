// Graph queries (hop distances, shortest paths, degrees, hubs) and two more null models (edge-count only, block-preserving).
// Pure C++, no NEST. The motif census and the degree-preserving rewire() live in analysis.cpp.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <unordered_set>

#include "json_util.h"
#include "spike/analysis.h"

namespace spike::analysis
{

namespace
{
using Adj = std::vector< std::vector< int32_t > >;

// Same generator and constants as rewire() in analysis.cpp, so a seed means the same stream in every null model.
struct SplitMix64
{
  uint64_t s;
  uint64_t next()
  {
    uint64_t z = ( s += 0x9e3779b97f4a7c15ULL );
    z = ( z ^ ( z >> 30 ) ) * 0xbf58476d1ce4e5b9ULL;
    z = ( z ^ ( z >> 27 ) ) * 0x94d049bb133111ebULL;
    return z ^ ( z >> 31 );
  }
};

std::vector< char > ban_mask( int32_t n, const std::vector< int32_t >& banned )
{
  std::vector< char > m( static_cast< size_t >( std::max( n, 0 ) ), 0 );
  for ( int32_t b : banned )
    if ( b >= 0 && b < n )
      m[ static_cast< size_t >( b ) ] = 1;
  return m;
}

// Multi-source BFS over `adj`; banned nodes are neither seeds nor entered. -1 = not reached.
std::vector< int32_t > bfs_dist( int32_t n, const Adj& adj, const std::vector< int32_t >& seeds, const std::vector< char >& banned )
{
  std::vector< int32_t > d( static_cast< size_t >( std::max( n, 0 ) ), -1 );
  std::vector< int32_t > queue;
  for ( int32_t s : seeds )
    if ( s >= 0 && s < n && !banned[ static_cast< size_t >( s ) ] && d[ static_cast< size_t >( s ) ] < 0 )
    {
      d[ static_cast< size_t >( s ) ] = 0;
      queue.push_back( s );
    }
  for ( size_t head = 0; head < queue.size(); ++head )
  {
    const int32_t v = queue[ head ];
    for ( int32_t w : adj[ static_cast< size_t >( v ) ] )
      if ( !banned[ static_cast< size_t >( w ) ] && d[ static_cast< size_t >( w ) ] < 0 )
      {
        d[ static_cast< size_t >( w ) ] = d[ static_cast< size_t >( v ) ] + 1;
        queue.push_back( w );
      }
  }
  return d;
}

Adj reversed( const Graph& g )
{
  Adj r( static_cast< size_t >( std::max( g.n, 0 ) ) );
  for ( int32_t a = 0; a < g.n; ++a )
    for ( int32_t b : g.out[ static_cast< size_t >( a ) ] )
      r[ static_cast< size_t >( b ) ].push_back( a );
  for ( auto& v : r )
    std::sort( v.begin(), v.end() );
  return r;
}

Graph graph_from_pairs( int32_t n, std::vector< std::pair< int32_t, int32_t > >& e )
{
  Graph r;
  r.n = n;
  r.out.assign( static_cast< size_t >( std::max( n, 0 ) ), {} );
  for ( const auto& [ a, b ] : e )
    r.out[ static_cast< size_t >( a ) ].push_back( b );
  for ( auto& o : r.out )
    std::sort( o.begin(), o.end() );
  return r;
}
} // namespace

// ---------------------------------------------------------------- queries

std::vector< int32_t > hops( const Graph& g, const std::vector< int32_t >& sources, const std::vector< int32_t >& banned )
{
  return bfs_dist( g.n, g.out, sources, ban_mask( g.n, banned ) );
}

std::vector< int32_t > shortest_path(
  const Graph& g, const std::vector< int32_t >& sources, const std::vector< int32_t >& targets, const std::vector< int32_t >& banned )
{
  const auto mask = ban_mask( g.n, banned );
  const auto rt = bfs_dist( g.n, reversed( g ), targets, mask );  // distance to the nearest target
  int32_t start = -1;
  for ( int32_t s : sources )  // closest source; ties to the lowest index
    if ( s >= 0 && s < g.n && rt[ static_cast< size_t >( s ) ] >= 0
      && ( start < 0 || rt[ static_cast< size_t >( s ) ] < rt[ static_cast< size_t >( start ) ]
           || ( rt[ static_cast< size_t >( s ) ] == rt[ static_cast< size_t >( start ) ] && s < start ) ) )
      start = s;
  std::vector< int32_t > path;
  if ( start < 0 )
    return path;
  path.push_back( start );
  int32_t cur = start;
  while ( rt[ static_cast< size_t >( cur ) ] > 0 )
  {
    int32_t next = -1;
    for ( int32_t w : g.out[ static_cast< size_t >( cur ) ] )  // sorted: the first hit is the lowest index
      if ( rt[ static_cast< size_t >( w ) ] == rt[ static_cast< size_t >( cur ) ] - 1 )
      {
        next = w;
        break;
      }
    if ( next < 0 )  // cannot happen: a node at distance d has a neighbour at d - 1
      break;
    path.push_back( next );
    cur = next;
  }
  return path;
}

std::vector< int32_t > on_shortest_paths(
  const Graph& g, const std::vector< int32_t >& sources, const std::vector< int32_t >& targets, const std::vector< int32_t >& banned )
{
  const auto mask = ban_mask( g.n, banned );
  const auto ds = bfs_dist( g.n, g.out, sources, mask );
  const auto rt = bfs_dist( g.n, reversed( g ), targets, mask );
  int32_t best = -1;
  for ( int32_t s : sources )
    if ( s >= 0 && s < g.n && rt[ static_cast< size_t >( s ) ] >= 0 && ( best < 0 || rt[ static_cast< size_t >( s ) ] < best ) )
      best = rt[ static_cast< size_t >( s ) ];
  std::vector< int32_t > nodes;
  if ( best < 0 )
    return nodes;
  for ( int32_t v = 0; v < g.n; ++v )
    if ( ds[ static_cast< size_t >( v ) ] >= 0 && rt[ static_cast< size_t >( v ) ] >= 0
      && ds[ static_cast< size_t >( v ) ] + rt[ static_cast< size_t >( v ) ] == best )
      nodes.push_back( v );
  return nodes;
}

Degrees degrees( const Graph& g )
{
  Degrees d;
  d.in.assign( static_cast< size_t >( std::max( g.n, 0 ) ), 0 );
  d.out.assign( static_cast< size_t >( std::max( g.n, 0 ) ), 0 );
  for ( int32_t a = 0; a < g.n; ++a )
    for ( int32_t b : g.out[ static_cast< size_t >( a ) ] )
    {
      ++d.out[ static_cast< size_t >( a ) ];
      ++d.in[ static_cast< size_t >( b ) ];
    }
  return d;
}

std::vector< int32_t > hubs( const Graph& g, int k )
{
  const Degrees d = degrees( g );
  std::vector< int32_t > idx( static_cast< size_t >( std::max( g.n, 0 ) ) );
  for ( int32_t i = 0; i < g.n; ++i )
    idx[ static_cast< size_t >( i ) ] = i;
  auto total = [ & ]( int32_t i ) { return d.in[ static_cast< size_t >( i ) ] + d.out[ static_cast< size_t >( i ) ]; };
  std::sort( idx.begin(), idx.end(), [ & ]( int32_t a, int32_t b ) { return total( a ) != total( b ) ? total( a ) > total( b ) : a < b; } );
  const size_t keep = static_cast< size_t >( std::max( k, 0 ) );
  if ( idx.size() > keep )
    idx.resize( keep );
  return idx;
}

// ---------------------------------------------------------------- null models

Graph shuffle_edges( const Graph& g, uint64_t seed )
{
  const uint64_t N = static_cast< uint64_t >( std::max( g.n, 0 ) );
  const uint64_t total = N >= 2 ? N * ( N - 1 ) : 0;  // ordered pairs without self-loops
  const uint64_t m = std::min< uint64_t >( static_cast< uint64_t >( g.edge_count() ), total );
  std::vector< std::pair< int32_t, int32_t > > e;
  if ( m > 0 )
  {
    SplitMix64 rng{ seed };
    // A dense graph is drawn as its complement, so the rejection loop never starves.
    const bool complement = m * 2 > total;
    const uint64_t want = complement ? total - m : m;
    std::unordered_set< uint64_t > chosen;
    while ( chosen.size() < want )
    {
      const uint64_t a = rng.next() % N, b = rng.next() % N;
      if ( a != b )
        chosen.insert( a * N + b );
    }
    if ( !complement )
      for ( uint64_t key : chosen )
        e.emplace_back( static_cast< int32_t >( key / N ), static_cast< int32_t >( key % N ) );
    else
      for ( uint64_t a = 0; a < N; ++a )
        for ( uint64_t b = 0; b < N; ++b )
          if ( a != b && !chosen.count( a * N + b ) )
            e.emplace_back( static_cast< int32_t >( a ), static_cast< int32_t >( b ) );
  }
  return graph_from_pairs( g.n, e );  // sorted per node, so the unordered_set order does not leak out
}

Graph rewire_blocks( const Graph& g, const std::vector< int32_t >& block_of, uint64_t seed, int swaps_per_edge, RewireStats* stats )
{
  std::vector< std::pair< int32_t, int32_t > > e;
  std::unordered_set< uint64_t > present;
  const uint64_t N = static_cast< uint64_t >( std::max( g.n, 1 ) );
  auto block = [ & ]( int32_t v ) { return static_cast< size_t >( v ) < block_of.size() ? block_of[ static_cast< size_t >( v ) ] : -1; };
  for ( int32_t a = 0; a < g.n; ++a )
    for ( int32_t b : g.out[ static_cast< size_t >( a ) ] )
    {
      e.emplace_back( a, b );
      present.insert( a * N + b );
    }
  const uint64_t m = e.size();
  if ( m >= 2 )
  {
    SplitMix64 rng{ seed };
    const uint64_t tries = m * static_cast< uint64_t >( std::max( swaps_per_edge, 0 ) );
    for ( uint64_t t = 0; t < tries; ++t )
    {
      const uint64_t i = rng.next() % m, j = rng.next() % m;
      if ( stats )
        ++stats->attempts;
      const auto [ a, b ] = e[ i ];
      const auto [ c, d ] = e[ j ];
      if ( a == c || b == d || a == d || c == b )
        continue;
      // (a,b) and (c,d) become (a,d) and (c,b). The block pairs are kept if the posts share a block (each new edge replaces
      // the old edge with its pre) or the pres share a block (each new edge replaces the old edge with its post).
      if ( block( b ) != block( d ) && block( a ) != block( c ) )
        continue;
      if ( present.count( a * N + d ) || present.count( c * N + b ) )
        continue;
      if ( stats )
        ++stats->accepted;
      present.erase( a * N + b );
      present.erase( c * N + d );
      present.insert( a * N + d );
      present.insert( c * N + b );
      e[ i ] = { a, d };
      e[ j ] = { c, b };
    }
  }
  return graph_from_pairs( g.n, e );
}

const char* null_model_name( NullModel m )
{
  switch ( m )
  {
    case NullModel::EdgeCount: return "edge_count";
    case NullModel::Degree: return "degree";
    case NullModel::Blocks: return "blocks";
  }
  return "degree";
}

MotifResult motif_zscores( const Graph& g, NullModel model, const std::vector< int32_t >& block_of, int n_null, uint64_t seed,
  int swaps_per_edge )
{
  MotifResult r;
  r.model = model;
  r.counts = motif_census( g );
  r.null_samples = n_null;
  RewireStats rs;
  for ( int k = 0; k < n_null; ++k )
  {
    const uint64_t s = seed + static_cast< uint64_t >( k );
    switch ( model )
    {
      case NullModel::EdgeCount: r.null_counts.push_back( motif_census( shuffle_edges( g, s ) ) ); break;
      case NullModel::Degree: r.null_counts.push_back( motif_census( rewire( g, s, swaps_per_edge, &rs ) ) ); break;
      case NullModel::Blocks: r.null_counts.push_back( motif_census( rewire_blocks( g, block_of, s, swaps_per_edge, &rs ) ) ); break;
    }
  }
  const std::vector< Census >& nulls = r.null_counts;
  // From here on the arithmetic is that of motif_zscores( g, n_null, seed, swaps_per_edge ), so Degree matches it exactly.
  r.node_count = g.n;
  r.edge_count = g.edge_count();
  r.swap_attempts = rs.attempts;
  r.swap_accepted = rs.accepted;
  r.swap_acceptance = rs.attempts ? static_cast< double >( rs.accepted ) / static_cast< double >( rs.attempts ) : 0.0;
  r.effective_swaps_per_edge =
    ( n_null > 0 && r.edge_count > 0 ) ? static_cast< double >( rs.accepted ) / static_cast< double >( n_null ) / static_cast< double >( r.edge_count ) : 0.0;
  r.degenerate = model != NullModel::EdgeCount && n_null > 0 && r.swap_acceptance < kMinSwapAcceptance;
  r.significant.assign( kMotifClasses, 0 );
  r.null_mean.assign( kMotifClasses, 0.0 );
  r.null_sd.assign( kMotifClasses, 0.0 );
  r.z.assign( kMotifClasses, 0.0 );
  for ( int c = 0; c < kMotifClasses; ++c )
  {
    double mean = 0;
    for ( const auto& x : nulls )
      mean += static_cast< double >( x[ c ] );
    if ( n_null > 0 )
      mean /= n_null;
    double var = 0;
    for ( const auto& x : nulls )
    {
      const double d = static_cast< double >( x[ c ] ) - mean;
      var += d * d;
    }
    const double sd = n_null > 1 ? std::sqrt( var / ( n_null - 1 ) ) : 0.0;
    r.null_mean[ c ] = mean;
    r.null_sd[ c ] = sd;
    const double diff = static_cast< double >( r.counts[ c ] ) - mean;
    if ( sd > 0 )
      r.z[ c ] = diff / sd;
    else if ( diff == 0 || n_null == 0 )
      r.z[ c ] = 0.0;
    else
      r.z[ c ] = diff > 0 ? std::numeric_limits< double >::infinity() : -std::numeric_limits< double >::infinity();
    r.significant[ c ] = ( std::isfinite( r.z[ c ] ) && std::fabs( r.z[ c ] ) > kSignificantZ && sd >= kMinNullSd && !r.degenerate ) ? 1 : 0;
  }
  return r;
}

// ---------------------------------------------------------------- JSON

namespace
{
template < typename T >
void json_list( std::ostringstream& o, const std::vector< T >& v )
{
  o << "[";
  for ( size_t i = 0; i < v.size(); ++i )
    o << ( i ? "," : "" ) << v[ i ];
  o << "]";
}
} // namespace

std::string graph_path_json(
  const Graph& g, const std::vector< int32_t >& sources, const std::vector< int32_t >& targets, const std::vector< int32_t >& banned )
{
  const auto path = shortest_path( g, sources, targets, banned );
  std::ostringstream o;
  o << "{\"nodes\":" << g.n << ",\"edges\":" << g.edge_count() << ",\"reachable\":" << ( path.empty() ? "false" : "true" )
    << ",\"length\":" << ( path.empty() ? -1 : static_cast< int >( path.size() ) - 1 ) << ",\"hops\":";
  json_list( o, hops( g, sources, banned ) );
  o << ",\"path\":";
  json_list( o, path );
  o << ",\"on_paths\":";
  json_list( o, on_shortest_paths( g, sources, targets, banned ) );
  o << "}";
  return o.str();
}

std::string graph_degrees_json( const Graph& g, int k )
{
  const Degrees d = degrees( g );
  std::ostringstream o;
  o << "{\"nodes\":" << g.n << ",\"edges\":" << g.edge_count() << ",\"in\":";
  json_list( o, d.in );
  o << ",\"out\":";
  json_list( o, d.out );
  o << ",\"hubs\":[";
  const auto h = hubs( g, k );
  for ( size_t i = 0; i < h.size(); ++i )
  {
    const size_t v = static_cast< size_t >( h[ i ] );
    o << ( i ? "," : "" ) << "{\"node\":" << h[ i ] << ",\"in\":" << d.in[ v ] << ",\"out\":" << d.out[ v ] << ",\"total\":" << d.in[ v ] + d.out[ v ] << "}";
  }
  o << "]}";
  return o.str();
}

std::string motif_null_json( const MotifResult& r )
{
  std::ostringstream o;
  o << "{\"model\":\"" << null_model_name( r.model ) << "\",\"null_samples\":" << r.null_samples << ",\"nodes\":" << r.node_count
    << ",\"edges\":" << r.edge_count << ",\"swap_attempts\":" << r.swap_attempts << ",\"swap_accepted\":" << r.swap_accepted
    << ",\"swap_acceptance\":";
  detail::jnum( o, r.swap_acceptance );
  o << ",\"effective_swaps_per_edge\":";
  detail::jnum( o, r.effective_swaps_per_edge );
  o << ",\"degenerate\":" << ( r.degenerate ? "true" : "false" ) << ",\"motifs\":[";
  for ( int c = 0; c < kMotifClasses; ++c )
  {
    const auto& m = motif_info( c );
    const size_t cs = static_cast< size_t >( c );
    o << ( c ? "," : "" ) << "{\"index\":" << c << ",\"milo_id\":" << m.milo_id << ",\"man\":\"" << m.man << "\",\"name\":\"" << m.name
      << "\",\"count\":" << ( cs < r.counts.size() ? r.counts[ cs ] : 0 ) << ",\"null_mean\":";
    detail::jnum( o, cs < r.null_mean.size() ? r.null_mean[ cs ] : 0.0 );
    o << ",\"null_sd\":";
    detail::jnum( o, cs < r.null_sd.size() ? r.null_sd[ cs ] : 0.0 );
    o << ",\"z\":";
    detail::jnum( o, cs < r.z.size() ? r.z[ cs ] : 0.0 );
    o << ",\"z_defined\":" << ( cs < r.z.size() && std::isfinite( r.z[ cs ] ) ? "true" : "false" )
      << ",\"significant\":" << ( motif_is_significant( r, c ) ? "true" : "false" ) << ",\"null_counts\":[";
    for ( size_t k = 0; k < r.null_counts.size(); ++k )
      o << ( k ? "," : "" ) << ( cs < r.null_counts[ k ].size() ? r.null_counts[ k ][ cs ] : 0 );
    o << "]}";
  }
  o << "]}";
  return o.str();
}

} // namespace spike::analysis
