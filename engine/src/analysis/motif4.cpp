// 4-node directed motif census (199 weakly connected classes), ESU enumeration, same null model as the 3-node census.
// Pure C++; python/spike/motifs.py (census4 / zscores4) mirrors it and gives identical results.
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "json_util.h"
#include "spike/analysis.h"

namespace spike::analysis
{
namespace
{
// ordered node pairs among nodes 0..3, bit b of the 12-bit code
constexpr int kP4[ 12 ][ 2 ] = { { 0, 1 }, { 0, 2 }, { 0, 3 }, { 1, 0 }, { 1, 2 }, { 1, 3 }, { 2, 0 }, { 2, 1 }, { 2, 3 }, { 3, 0 }, { 3, 1 }, { 3, 2 } };

struct Table4
{
  std::array< int16_t, 4096 > cls;  // 12-bit code -> class index, -1 if not weakly connected
  std::array< MotifInfo4, kMotifClasses4 > info;
  std::array< int16_t, 65536 > by_id;
  Table4()
  {
    by_id.fill( -1 );
    std::array< int, 4096 > canon;
    std::vector< int > ids;
    std::array< int, 4 > p = { 0, 1, 2, 3 };
    std::vector< std::array< int, 4 > > perms;
    do
      perms.push_back( p );
    while ( std::next_permutation( p.begin(), p.end() ) );
    for ( int code = 0; code < 4096; ++code )
    {
      int parent[ 4 ] = { 0, 1, 2, 3 };
      auto find = [ & ]( int x ) {
        while ( parent[ x ] != x )
          x = parent[ x ];
        return x;
      };
      for ( int b = 0; b < 12; ++b )
        if ( code >> b & 1 )
          parent[ find( kP4[ b ][ 0 ] ) ] = find( kP4[ b ][ 1 ] );
      const bool conn = find( 0 ) == find( 1 ) && find( 0 ) == find( 2 ) && find( 0 ) == find( 3 );
      if ( !conn )
      {
        canon[ code ] = -1;
        continue;
      }
      int best = 1 << 16;
      for ( const auto& q : perms )
      {
        int c = 0;
        for ( int b = 0; b < 12; ++b )
          if ( code >> b & 1 )
            c |= 1 << ( 15 - ( 4 * q[ kP4[ b ][ 0 ] ] + q[ kP4[ b ][ 1 ] ] ) );
        best = std::min( best, c );
      }
      canon[ code ] = best;
      ids.push_back( best );
    }
    std::sort( ids.begin(), ids.end() );
    ids.erase( std::unique( ids.begin(), ids.end() ), ids.end() );
    for ( size_t i = 0; i < ids.size() && i < info.size(); ++i )
    {
      by_id[ ids[ i ] ] = static_cast< int16_t >( i );
      int edges = 0, mutual = 0;
      for ( int a = 0; a < 4; ++a )
        for ( int b = 0; b < 4; ++b )
          if ( a != b && ( ids[ i ] >> ( 15 - ( 4 * a + b ) ) & 1 ) )
          {
            ++edges;
            if ( a < b && ( ids[ i ] >> ( 15 - ( 4 * b + a ) ) & 1 ) )
              ++mutual;
          }
      info[ i ] = MotifInfo4{ static_cast< int >( i ), ids[ i ], edges, mutual };
    }
    for ( int code = 0; code < 4096; ++code )
      cls[ code ] = canon[ code ] < 0 ? int16_t( -1 ) : by_id[ canon[ code ] ];
  }
};
const Table4& table4()
{
  static const Table4 t;
  return t;
}

struct Esu
{
  const Graph& g;
  std::vector< std::vector< int32_t > > nb;
  std::vector< int > cover;
  Census census;
  int32_t v = 0;
  int32_t sub[ 4 ] = {};
  const std::array< int16_t, 4096 >& cls = table4().cls;

  explicit Esu( const Graph& gr ) : g( gr ), nb( gr.n ), cover( gr.n, 0 ), census( kMotifClasses4, 0 )
  {
    for ( int32_t a = 0; a < g.n; ++a )
      for ( int32_t b : g.out[ a ] )
      {
        nb[ a ].push_back( b );
        nb[ b ].push_back( a );
      }
    for ( auto& x : nb )
    {
      std::sort( x.begin(), x.end() );
      x.erase( std::unique( x.begin(), x.end() ), x.end() );
    }
  }
  void classify()
  {
    int code = 0;
    for ( int b = 0; b < 12; ++b )
      if ( g.has_edge( sub[ kP4[ b ][ 0 ] ], sub[ kP4[ b ][ 1 ] ] ) )
        code |= 1 << b;
    ++census[ cls[ code ] ];
  }
  void add( int32_t x, int d )
  {
    cover[ x ] += d;
    for ( int32_t y : nb[ x ] )
      cover[ y ] += d;
  }
  void extend( int k, std::vector< int32_t > ext )
  {
    if ( k == 3 )
    {
      for ( int32_t w : ext )
      {
        sub[ 3 ] = w;
        classify();
      }
      return;
    }
    while ( !ext.empty() )
    {
      const int32_t w = ext.back();
      ext.pop_back();
      std::vector< int32_t > ext2 = ext;
      for ( int32_t u : nb[ w ] )
        if ( u > v && cover[ u ] == 0 )
          ext2.push_back( u );
      sub[ k ] = w;
      add( w, 1 );
      extend( k + 1, std::move( ext2 ) );
      add( w, -1 );
    }
  }
  void run()
  {
    for ( v = 0; v < g.n; ++v )
    {
      sub[ 0 ] = v;
      std::vector< int32_t > ext;
      for ( int32_t u : nb[ v ] )
        if ( u > v )
          ext.push_back( u );
      add( v, 1 );
      extend( 1, std::move( ext ) );
      add( v, -1 );
    }
  }
};
} // namespace

const MotifInfo4& motif4_info( int index )
{
  return table4().info[ index ];
}

int motif4_index_from_id( int id )
{
  return id >= 0 && id < 65536 ? table4().by_id[ id ] : -1;
}

Census motif4_census( const Graph& g )
{
  Esu e( g );
  e.run();
  return e.census;
}

Census motif4_census( const std::vector< Edge >& edges, int32_t n, Sign sign )
{
  return motif4_census( make_graph( edges, n, sign ) );
}

double motif4_work_estimate( const Graph& g )
{
  std::vector< double > deg( g.n, 0.0 );
  std::vector< std::vector< int32_t > > nb( g.n );
  for ( int32_t a = 0; a < g.n; ++a )
    for ( int32_t b : g.out[ a ] )
    {
      nb[ a ].push_back( b );
      nb[ b ].push_back( a );
    }
  double w = 0;
  for ( int32_t a = 0; a < g.n; ++a )
  {
    std::sort( nb[ a ].begin(), nb[ a ].end() );
    nb[ a ].erase( std::unique( nb[ a ].begin(), nb[ a ].end() ), nb[ a ].end() );
    const double d = static_cast< double >( nb[ a ].size() );
    w += d * ( d - 1 ) * ( d - 2 ) / 6.0;  // 3-leaf stars centred here
  }
  for ( int32_t a = 0; a < g.n; ++a )  // paths of length 3 through the skeleton
    for ( int32_t b : nb[ a ] )
      if ( b > a )
        w += ( static_cast< double >( nb[ a ].size() ) - 1 ) * ( static_cast< double >( nb[ b ].size() ) - 1 );
  return w;
}

MotifResult4 motif4_zscores( const Graph& g, int n_null, uint64_t seed, int swaps_per_edge )
{
  MotifResult4 r;
  r.counts = motif4_census( g );
  r.null_samples = n_null;
  std::vector< Census > nulls;
  RewireStats rs;
  for ( int k = 0; k < n_null; ++k )
    nulls.push_back( motif4_census( rewire( g, seed + static_cast< uint64_t >( k ), swaps_per_edge, &rs ) ) );
  r.node_count = g.n;
  r.edge_count = g.edge_count();
  r.swap_attempts = rs.attempts;
  r.swap_accepted = rs.accepted;
  r.swap_acceptance = rs.attempts ? static_cast< double >( rs.accepted ) / static_cast< double >( rs.attempts ) : 0.0;
  r.effective_swaps_per_edge =
    ( n_null > 0 && r.edge_count > 0 ) ? static_cast< double >( rs.accepted ) / static_cast< double >( n_null ) / static_cast< double >( r.edge_count ) : 0.0;
  r.degenerate = n_null > 0 && r.swap_acceptance < kMinSwapAcceptance;
  r.significant.assign( kMotifClasses4, 0 );
  r.null_mean.assign( kMotifClasses4, 0.0 );
  r.null_sd.assign( kMotifClasses4, 0.0 );
  r.z.assign( kMotifClasses4, 0.0 );
  for ( int c = 0; c < kMotifClasses4; ++c )
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

MotifResult4 motif4_zscores( const std::vector< Edge >& edges, int32_t n, Sign sign, int n_null, uint64_t seed, int swaps_per_edge )
{
  return motif4_zscores( make_graph( edges, n, sign ), n_null, seed, swaps_per_edge );
}

std::string motif4_result_json( const MotifResult4& r, bool only_present )
{
  using detail::jnum;
  std::ostringstream o;
  o << "{\"null_samples\":" << r.null_samples << ",\"nodes\":" << r.node_count << ",\"edges\":" << r.edge_count << ",\"swap_acceptance\":";
  jnum( o, r.swap_acceptance );
  o << ",\"effective_swaps_per_edge\":";
  jnum( o, r.effective_swaps_per_edge );
  o << ",\"degenerate\":" << ( r.degenerate ? "true" : "false" ) << ",\"classes\":[";
  bool first = true;
  for ( int c = 0; c < kMotifClasses4; ++c )
  {
    if ( only_present && r.counts[ c ] == 0 && r.null_mean[ c ] == 0.0 )
      continue;
    const auto& m = motif4_info( c );
    o << ( first ? "" : "," ) << "{\"index\":" << c << ",\"id\":" << m.id << ",\"edges\":" << m.edges << ",\"mutual_pairs\":" << m.mutual_pairs
      << ",\"count\":" << r.counts[ c ] << ",\"null_mean\":";
    first = false;
    jnum( o, r.null_mean[ c ] );
    o << ",\"null_sd\":";
    jnum( o, r.null_sd[ c ] );
    o << ",\"z\":";
    jnum( o, r.z[ c ] );
    o << ",\"z_defined\":" << ( std::isfinite( r.z[ c ] ) ? "true" : "false" ) << ",\"significant\":" << ( r.significant[ c ] ? "true" : "false" )
      << "}";
  }
  o << "]}";
  return o.str();
}

} // namespace spike::analysis
