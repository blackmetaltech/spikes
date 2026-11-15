// Motif census, degree-preserving nulls and spike statistics. Pure C++, no NEST (see analysis_nest.cpp for the glue).
#include "spike/analysis.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numeric>
#include <sstream>
#include <unordered_set>

namespace spike::analysis
{

// ---------------------------------------------------------------- graph

int64_t Graph::edge_count() const
{
  int64_t m = 0;
  for ( const auto& o : out )
    m += static_cast< int64_t >( o.size() );
  return m;
}

bool Graph::has_edge( int32_t a, int32_t b ) const
{
  if ( a < 0 || a >= n )
    return false;
  const auto& o = out[ a ];
  return std::binary_search( o.begin(), o.end(), b );
}

Graph make_graph( const std::vector< Edge >& edges, int32_t n, Sign sign )
{
  if ( n < 0 )
  {
    n = 0;
    for ( const auto& e : edges )
      n = std::max( n, std::max( e.pre, e.post ) + 1 );
  }
  Graph g;
  g.n = n;
  g.out.assign( n, {} );
  for ( const auto& e : edges )
  {
    if ( e.pre == e.post || e.pre < 0 || e.post < 0 || e.pre >= n || e.post >= n )
      continue;
    if ( sign == Sign::Excitatory && !( e.weight > 0 ) )
      continue;
    if ( sign == Sign::Inhibitory && !( e.weight < 0 ) )
      continue;
    g.out[ e.pre ].push_back( e.post );
  }
  for ( auto& o : g.out )
  {
    std::sort( o.begin(), o.end() );
    o.erase( std::unique( o.begin(), o.end() ), o.end() );
  }
  return g;
}

// ---------------------------------------------------------------- motif classes

namespace
{
const MotifInfo kMotifs[ kMotifClasses ] = {
  { 0, 6, "021D", "fan_out", 2, false },
  { 1, 12, "021C", "chain", 2, false },
  { 2, 14, "111U", "mutual_pair_driving", 3, true },
  { 3, 36, "021U", "fan_in", 2, false },
  { 4, 38, "030T", "feed_forward_loop", 3, false },
  { 5, 46, "120U", "mutual_pair_co_driving", 4, true },
  { 6, 74, "111D", "mutual_pair_fed", 3, true },
  { 7, 78, "201", "two_mutual_pairs", 4, true },
  { 8, 98, "030C", "feedback_loop", 3, false },
  { 9, 102, "120C", "mutual_pair_in_loop", 4, true },
  { 10, 108, "120D", "mutual_pair_co_driven", 4, true },
  { 11, 110, "210", "mutual_pair_double", 5, true },
  { 12, 238, "300", "fully_connected", 6, true },
};

// Node pair bit positions for the 6 possible edges among nodes 0,1,2.
constexpr int kPairs[ 6 ][ 2 ] = { { 0, 1 }, { 0, 2 }, { 1, 0 }, { 1, 2 }, { 2, 0 }, { 2, 1 } };

int milo_index( int milo_id )
{
  for ( const auto& m : kMotifs )
    if ( m.milo_id == milo_id )
      return m.index;
  return -1;
}

struct ClassTable
{
  std::array< int8_t, 64 > cls;  // 6-bit edge code -> class index, -1 if not weakly connected
  ClassTable()
  {
    static const int perms[ 6 ][ 3 ] = { { 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 } };
    for ( int code = 0; code < 64; ++code )
    {
      bool m[ 3 ][ 3 ] = {};
      for ( int b = 0; b < 6; ++b )
        if ( code >> b & 1 )
          m[ kPairs[ b ][ 0 ] ][ kPairs[ b ][ 1 ] ] = true;
      // weak connectivity: at least two of the three undirected pairs are linked
      int links = 0;
      for ( int a = 0; a < 3; ++a )
        for ( int b = a + 1; b < 3; ++b )
          links += ( m[ a ][ b ] || m[ b ][ a ] ) ? 1 : 0;
      if ( links < 2 )
      {
        cls[ code ] = -1;
        continue;
      }
      int best = 1 << 9;
      for ( const auto& p : perms )
      {
        int c = 0;
        for ( int i = 0; i < 3; ++i )
          for ( int j = 0; j < 3; ++j )
            if ( m[ i ][ j ] )
              c |= 1 << ( 8 - ( 3 * p[ i ] + p[ j ] ) );
        best = std::min( best, c );
      }
      cls[ code ] = static_cast< int8_t >( milo_index( best ) );
    }
  }
};

const ClassTable& table()
{
  static const ClassTable t;
  return t;
}

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
} // namespace

const MotifInfo& motif_info( int index )
{
  return kMotifs[ index ];
}

int motif_index_from_milo_id( int milo_id )
{
  return milo_index( milo_id );
}

Census motif_census( const Graph& g )
{
  Census census( kMotifClasses, 0 );
  const int32_t n = g.n;
  std::vector< std::vector< int32_t > > nb( n );  // undirected neighbourhoods
  for ( int32_t a = 0; a < n; ++a )
    for ( int32_t b : g.out[ a ] )
    {
      nb[ a ].push_back( b );
      nb[ b ].push_back( a );
    }
  for ( auto& v : nb )
  {
    std::sort( v.begin(), v.end() );
    v.erase( std::unique( v.begin(), v.end() ), v.end() );
  }
  const auto& cls = table().cls;
  for ( int32_t v = 0; v < n; ++v )
  {
    const auto& N = nb[ v ];
    for ( size_t i = 0; i < N.size(); ++i )
      for ( size_t j = i + 1; j < N.size(); ++j )
      {
        const int32_t u = N[ i ], w = N[ j ];
        const bool uw = g.has_edge( u, w ) || g.has_edge( w, u );
        if ( uw && v > u )  // triangle: counted once, at its smallest node
          continue;
        const int32_t node[ 3 ] = { v, u, w };
        int code = 0;
        for ( int b = 0; b < 6; ++b )
          if ( g.has_edge( node[ kPairs[ b ][ 0 ] ], node[ kPairs[ b ][ 1 ] ] ) )
            code |= 1 << b;
        ++census[ cls[ code ] ];
      }
  }
  return census;
}

Census motif_census( const std::vector< Edge >& edges, int32_t n, Sign sign )
{
  return motif_census( make_graph( edges, n, sign ) );
}

Graph rewire( const Graph& g, uint64_t seed, int swaps_per_edge )
{
  std::vector< std::pair< int32_t, int32_t > > e;
  std::unordered_set< uint64_t > present;
  const uint64_t N = static_cast< uint64_t >( std::max( g.n, 1 ) );
  for ( int32_t a = 0; a < g.n; ++a )
    for ( int32_t b : g.out[ a ] )
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
      const auto [ a, b ] = e[ i ];
      const auto [ c, d ] = e[ j ];
      if ( a == c || b == d || a == d || c == b )
        continue;
      if ( present.count( a * N + d ) || present.count( c * N + b ) )
        continue;
      present.erase( a * N + b );
      present.erase( c * N + d );
      present.insert( a * N + d );
      present.insert( c * N + b );
      e[ i ] = { a, d };
      e[ j ] = { c, b };
    }
  }
  Graph r;
  r.n = g.n;
  r.out.assign( g.n, {} );
  for ( const auto& [ a, b ] : e )
    r.out[ a ].push_back( b );
  for ( auto& o : r.out )
    std::sort( o.begin(), o.end() );
  return r;
}

MotifResult motif_zscores( const Graph& g, int n_null, uint64_t seed, int swaps_per_edge )
{
  MotifResult r;
  r.counts = motif_census( g );
  r.null_samples = n_null;
  std::vector< Census > nulls;
  for ( int k = 0; k < n_null; ++k )
    nulls.push_back( motif_census( rewire( g, seed + static_cast< uint64_t >( k ), swaps_per_edge ) ) );
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
  }
  return r;
}

MotifResult motif_zscores(
  const std::vector< Edge >& edges, int32_t n, Sign sign, int n_null, uint64_t seed, int swaps_per_edge )
{
  return motif_zscores( make_graph( edges, n, sign ), n_null, seed, swaps_per_edge );
}

static void json_num( std::ostringstream& o, double v )
{
  if ( !std::isfinite( v ) )
    o << "null";
  else
  {
    char buf[ 40 ];
    std::snprintf( buf, sizeof buf, "%.10g", v );
    o << buf;
  }
}

std::string motif_result_json( const MotifResult& r )
{
  std::ostringstream o;
  o << "{\"null_samples\":" << r.null_samples << ",\"motifs\":[";
  for ( int c = 0; c < kMotifClasses; ++c )
  {
    const auto& m = motif_info( c );
    o << ( c ? "," : "" ) << "{\"index\":" << c << ",\"milo_id\":" << m.milo_id << ",\"man\":\"" << m.man << "\",\"name\":\""
      << m.name << "\",\"count\":" << r.counts[ c ] << ",\"null_mean\":";
    json_num( o, c < static_cast< int >( r.null_mean.size() ) ? r.null_mean[ c ] : 0.0 );
    o << ",\"null_sd\":";
    json_num( o, c < static_cast< int >( r.null_sd.size() ) ? r.null_sd[ c ] : 0.0 );
    o << ",\"z\":";
    json_num( o, c < static_cast< int >( r.z.size() ) ? r.z[ c ] : 0.0 );
    o << "}";
  }
  o << "]}";
  return o.str();
}

// ---------------------------------------------------------------- statistics

namespace
{
std::vector< std::vector< double > > times_by_neuron( const std::vector< Spike >& spikes, int32_t n, double t0, double t1 )
{
  std::vector< std::vector< double > > t( n );
  for ( const auto& s : spikes )
    if ( s.neuron >= 0 && s.neuron < n && s.t_ms >= t0 && s.t_ms < t1 )
      t[ s.neuron ].push_back( s.t_ms );
  for ( auto& v : t )
    std::sort( v.begin(), v.end() );
  return t;
}

int32_t bin_count( double t0, double t1, double bin )
{
  return std::max< int32_t >( 1, static_cast< int32_t >( std::ceil( ( t1 - t0 ) / bin - 1e-9 ) ) );
}
} // namespace

std::vector< double > firing_rates( const std::vector< Spike >& spikes, int32_t n, double t0_ms, double t1_ms )
{
  std::vector< double > r( n, 0.0 );
  const double dur = t1_ms - t0_ms;
  if ( dur <= 0 )
    return r;
  for ( const auto& s : spikes )
    if ( s.neuron >= 0 && s.neuron < n && s.t_ms >= t0_ms && s.t_ms < t1_ms )
      r[ s.neuron ] += 1.0;
  for ( auto& x : r )
    x = x * 1000.0 / dur;
  return r;
}

std::vector< double > isi_cv( const std::vector< Spike >& spikes, int32_t n, double t0_ms, double t1_ms )
{
  const auto t = times_by_neuron( spikes, n, t0_ms, t1_ms );
  std::vector< double > cv( n, std::numeric_limits< double >::quiet_NaN() );
  for ( int32_t i = 0; i < n; ++i )
  {
    if ( t[ i ].size() < 3 )
      continue;
    const size_t k = t[ i ].size() - 1;
    double mean = 0;
    for ( size_t j = 0; j < k; ++j )
      mean += t[ i ][ j + 1 ] - t[ i ][ j ];
    mean /= static_cast< double >( k );
    double var = 0;
    for ( size_t j = 0; j < k; ++j )
    {
      const double d = ( t[ i ][ j + 1 ] - t[ i ][ j ] ) - mean;
      var += d * d;
    }
    var /= static_cast< double >( k );
    if ( mean > 0 )
      cv[ i ] = std::sqrt( var ) / mean;
  }
  return cv;
}

double golomb_chi( const std::vector< Spike >& spikes, int32_t n, double t0_ms, double t1_ms, double bin_ms )
{
  if ( n <= 0 || t1_ms <= t0_ms || bin_ms <= 0 )
    return 0.0;
  const int32_t nb = bin_count( t0_ms, t1_ms, bin_ms );
  const uint64_t NB = static_cast< uint64_t >( nb );
  std::vector< uint64_t > keys;  // (neuron, bin) of every spike, sparse
  for ( const auto& s : spikes )
    if ( s.neuron >= 0 && s.neuron < n && s.t_ms >= t0_ms && s.t_ms < t1_ms )
    {
      const int32_t b = std::min< int32_t >( nb - 1, static_cast< int32_t >( std::floor( ( s.t_ms - t0_ms ) / bin_ms ) ) );
      keys.push_back( static_cast< uint64_t >( s.neuron ) * NB + static_cast< uint64_t >( b ) );
    }
  if ( keys.empty() )
    return 0.0;
  std::sort( keys.begin(), keys.end() );
  std::vector< double > pop( nb, 0.0 );
  double sum_var = 0;
  size_t i = 0;
  while ( i < keys.size() )
  {
    const uint64_t neuron = keys[ i ] / NB;
    double s1 = 0, s2 = 0;
    while ( i < keys.size() && keys[ i ] / NB == neuron )
    {
      size_t j = i;
      while ( j < keys.size() && keys[ j ] == keys[ i ] )
        ++j;
      const double c = static_cast< double >( j - i );
      pop[ keys[ i ] % NB ] += c;
      s1 += c;
      s2 += c * c;
      i = j;
    }
    const double mean = s1 / nb;
    sum_var += s2 / nb - mean * mean;
  }
  const double mean_var = sum_var / n;
  double pm = 0, pv = 0;
  for ( double x : pop )
    pm += x / n;
  pm /= nb;
  for ( double x : pop )
  {
    const double d = x / n - pm;
    pv += d * d;
  }
  pv /= nb;
  if ( mean_var <= 0 )
    return 0.0;
  return std::sqrt( pv / mean_var );
}

static std::vector< char > subset_mask( int32_t n, const std::vector< int32_t >& neurons, int32_t& count )
{
  std::vector< char > m( n, neurons.empty() ? 1 : 0 );
  for ( int32_t i : neurons )
    if ( i >= 0 && i < n )
      m[ i ] = 1;
  count = static_cast< int32_t >( std::count( m.begin(), m.end(), 1 ) );
  return m;
}

std::vector< double > psth( const std::vector< Spike >& spikes, int32_t n, const std::vector< int32_t >& neurons, double t0_ms,
  double t1_ms, double bin_ms )
{
  int32_t cnt;
  const auto mask = subset_mask( n, neurons, cnt );
  const int32_t nb = bin_count( t0_ms, t1_ms, bin_ms );
  std::vector< double > h( nb, 0.0 );
  if ( cnt == 0 )
    return h;
  for ( const auto& s : spikes )
    if ( s.neuron >= 0 && s.neuron < n && mask[ s.neuron ] && s.t_ms >= t0_ms && s.t_ms < t1_ms )
      h[ std::min< int32_t >( nb - 1, static_cast< int32_t >( std::floor( ( s.t_ms - t0_ms ) / bin_ms ) ) ) ] += 1.0;
  for ( auto& x : h )
    x = x * 1000.0 / ( cnt * bin_ms );
  return h;
}

std::vector< double > psth_aligned( const std::vector< Spike >& spikes, int32_t n, const std::vector< int32_t >& neurons,
  const std::vector< double >& events_ms, double pre_ms, double post_ms, double bin_ms )
{
  int32_t cnt;
  const auto mask = subset_mask( n, neurons, cnt );
  const int32_t nb = bin_count( -pre_ms, post_ms, bin_ms );
  std::vector< double > h( nb, 0.0 );
  if ( cnt == 0 || events_ms.empty() )
    return h;
  for ( const auto& s : spikes )
  {
    if ( s.neuron < 0 || s.neuron >= n || !mask[ s.neuron ] )
      continue;
    for ( double ev : events_ms )
    {
      const double rel = s.t_ms - ev;
      if ( rel >= -pre_ms && rel < post_ms )
        h[ std::min< int32_t >( nb - 1, static_cast< int32_t >( std::floor( ( rel + pre_ms ) / bin_ms ) ) ) ] += 1.0;
    }
  }
  for ( auto& x : h )
    x = x * 1000.0 / ( static_cast< double >( events_ms.size() ) * cnt * bin_ms );
  return h;
}

} // namespace spike::analysis
