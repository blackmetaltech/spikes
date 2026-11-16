// Unit tests (no NEST needed) for bursts, PRC estimation, HRV and the 4-node census, plus the cross-language hooks used by python/tests:
//   analysis_extra_test --bursts  max_start max_end min_ibi min_dur min_spikes   (stdin: spike times)
//   analysis_extra_test --prc     period_ms                                      (stdin: ns np, spike times, perturbation times)
//   analysis_extra_test --hrv                                                    (stdin: beat times, ms)
//   analysis_extra_test --census4 [any|E|I] n_null seed                          (stdin: "n m" then m lines "pre post weight")
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <random>
#include <set>

#include "check.h"
#include "spike/analysis.h"

#define CHECK_NEAR( a, b, tol ) CHECK( std::fabs( ( a ) - ( b ) ) <= ( tol ) )

using namespace spike;
using namespace spike::analysis;

static void test_bursts()
{
  // two bursts of 4 spikes 5 ms apart, 500 ms apart, plus isolated spikes
  std::vector< double > t = { 10, 300 };
  for ( int b = 0; b < 2; ++b )
    for ( int i = 0; i < 4; ++i )
      t.push_back( 1000 + 500 * b + 5 * i );
  t.push_back( 2500 );
  BurstParams p;
  p.max_isi_start_ms = p.max_isi_end_ms = 20;
  p.min_ibi_ms = 100;
  auto b = detect_bursts( t, p );
  CHECK( b.size() == 2 );
  if ( b.size() == 2 )
  {
    CHECK_NEAR( b[ 0 ].onset_ms, 1000, 1e-9 );
    CHECK_NEAR( b[ 0 ].duration_ms, 15, 1e-9 );
    CHECK( b[ 0 ].n_spikes == 4 );
    CHECK_NEAR( b[ 1 ].onset_ms, 1500, 1e-9 );
    CHECK( b[ 1 ].first_index == 6 );
  }
  p.min_ibi_ms = 600;  // merges the two
  b = detect_bursts( t, p );
  CHECK( b.size() == 1 && b[ 0 ].n_spikes == 8 );
  p.min_ibi_ms = 100;
  p.min_spikes = 5;
  CHECK( detect_bursts( t, p ).empty() );
  CHECK( detect_bursts( std::vector< double >{}, p ).empty() );
  // per-neuron
  std::vector< Spike > sp;
  for ( double x : t )
    sp.push_back( { 1, x } );
  p.min_spikes = 3;
  const auto pn = detect_bursts( sp, 2, 0, 3000, p );
  CHECK( pn[ 0 ].empty() && pn[ 1 ].size() == 2 );
  b = detect_bursts( t, p );
  const auto s = summarize_bursts( b, static_cast< int64_t >( t.size() ) );
  CHECK( s.n_bursts == 2 && s.spikes_in_bursts == 8 );
  CHECK_NEAR( s.fraction_in_bursts, 8.0 / 11.0, 1e-12 );
  CHECK_NEAR( s.mean_duration_ms, 15, 1e-12 );
  CHECK_NEAR( s.mean_period_ms, 500, 1e-12 );
  CHECK_NEAR( s.mean_intra_burst_isi_ms, 5, 1e-12 );
}

static void test_prc()
{
  // period 100, perturbation at phase 0.5 of the cycle starting at 1000 advances the next spike by 10
  std::vector< double > t;
  for ( int i = 0; i <= 10; ++i )
    t.push_back( 100.0 * i );
  t.erase( t.begin() + 6, t.end() );
  t.push_back( 590 );  // cycle 5: 500 .. 590 (advance 10)
  for ( int i = 1; i <= 5; ++i )
    t.push_back( 590 + 100.0 * i );
  auto r = prc_estimate( t, { 550 } );
  CHECK_NEAR( r.period_ms, 100, 1e-9 );
  CHECK( r.points.size() == 1 );
  if ( !r.points.empty() )
  {
    CHECK_NEAR( r.points[ 0 ].phase, 0.5, 1e-9 );
    CHECK_NEAR( r.points[ 0 ].shift, 0.1, 1e-9 );
  }
  // two perturbations in one cycle, one outside the train: rejected
  r = prc_estimate( t, { 520, 550, 5000 } );
  CHECK( r.points.empty() && r.n_rejected == 3 );
  // a perturbation right after a perturbed cycle (recovery) is rejected
  r = prc_estimate( t, { 550, 650 } );
  CHECK( r.points.size() == 1 && r.n_rejected == 1 );
  // explicit period
  r = prc_estimate( t, { 550 }, 100.0 );
  CHECK( r.points.size() == 1 );
  const auto bins = prc_bins( r, 4 );
  CHECK( bins.size() == 4 && bins[ 2 ].count == 1 && std::isnan( bins[ 0 ].mean_shift ) );
  CHECK( prc_estimate( std::vector< double >{ 1 }, { 1 } ).points.empty() );
}

static void test_hrv()
{
  // constant RR: sdnn 0, rmssd 0
  std::vector< double > b;
  for ( int i = 0; i < 100; ++i )
    b.push_back( 800.0 * i );
  auto r = hrv_analyze( b );
  CHECK_NEAR( r.mean_rr_ms, 800, 1e-9 );
  CHECK_NEAR( r.mean_hr_bpm, 75, 1e-9 );
  CHECK_NEAR( r.sdnn_ms, 0, 1e-9 );
  CHECK_NEAR( r.pnn50_pct, 0, 1e-12 );
  // alternating RR 800/900: diffs 100 -> pNN50 100, rmssd 100
  b.assign( 1, 0.0 );
  for ( int i = 0; i < 40; ++i )
    b.push_back( b.back() + ( i % 2 ? 900.0 : 800.0 ) );
  r = hrv_analyze( b );
  CHECK_NEAR( r.rmssd_ms, 100, 1e-9 );
  CHECK_NEAR( r.pnn50_pct, 100, 1e-9 );
  CHECK( r.sdnn_ms > 49 && r.sdnn_ms < 52 );
  // slow modulation (0.1 Hz, LF) vs fast (0.25 Hz, HF)
  for ( double f : { 0.1, 0.25 } )
  {
    b.assign( 1, 0.0 );
    while ( b.back() < 600000 )
      b.push_back( b.back() + 900.0 + 60.0 * std::sin( 2 * M_PI * f * b.back() * 1e-3 ) );
    r = hrv_analyze( b );
    CHECK( r.spectrum_ok );
    if ( f < 0.15 )
      CHECK( r.lf_hf > 10 );
    else
      CHECK( r.lf_hf < 0.1 );
    // sinusoid of amplitude A has variance A^2/2 = 1800; the band power recovers most of it
    const double band = f < 0.15 ? r.lf_ms2 : r.hf_ms2;
    CHECK( band > 0.6 * 1800 && band < 1.4 * 1800 );
  }
  CHECK( !hrv_analyze( { 0, 800, 1600 } ).spectrum_ok );
  CHECK( std::isnan( hrv_analyze( { 0.0 } ).sdnn_ms ) );
  // white-noise RR: band powers integrate (roughly) to the variance share of the band
  std::mt19937_64 g( 3 );
  std::normal_distribution< double > nd( 0, 30 );
  b.assign( 1, 0.0 );
  for ( int i = 0; i < 2000; ++i )
    b.push_back( b.back() + 800 + nd( g ) );
  r = hrv_analyze( b );
  const double fN = 0.5 / 0.8;  // mean Nyquist
  CHECK( std::fabs( ( r.lf_ms2 + r.hf_ms2 ) / ( 900.0 * ( 0.15 - 0.04 + 0.4 - 0.15 ) / fN ) - 1 ) < 0.25 );
}

// ---- 4-node census: independent brute force
static int canon4( const bool m[ 4 ][ 4 ] )
{
  int p[ 4 ] = { 0, 1, 2, 3 }, best = 1 << 16;
  do
  {
    int c = 0;
    for ( int i = 0; i < 4; ++i )
      for ( int j = 0; j < 4; ++j )
        if ( i != j && m[ i ][ j ] )
          c |= 1 << ( 15 - ( 4 * p[ i ] + p[ j ] ) );
    best = std::min( best, c );
  } while ( std::next_permutation( p, p + 4 ) );
  return best;
}

static Census brute4( const Graph& g )
{
  Census c( kMotifClasses4, 0 );
  for ( int a = 0; a < g.n; ++a )
    for ( int b = a + 1; b < g.n; ++b )
      for ( int d = b + 1; d < g.n; ++d )
        for ( int e = d + 1; e < g.n; ++e )
        {
          const int v[ 4 ] = { a, b, d, e };
          bool m[ 4 ][ 4 ] = {};
          for ( int i = 0; i < 4; ++i )
            for ( int j = 0; j < 4; ++j )
              m[ i ][ j ] = i != j && g.has_edge( v[ i ], v[ j ] );
          int comp[ 4 ] = { 0, 1, 2, 3 };
          for ( int it = 0; it < 4; ++it )
            for ( int i = 0; i < 4; ++i )
              for ( int j = 0; j < 4; ++j )
                if ( m[ i ][ j ] || m[ j ][ i ] )
                  comp[ i ] = comp[ j ] = std::min( comp[ i ], comp[ j ] );
          if ( comp[ 0 ] != comp[ 1 ] || comp[ 0 ] != comp[ 2 ] || comp[ 0 ] != comp[ 3 ] )
            continue;
          const int cls = motif4_index_from_id( canon4( m ) );
          CHECK( cls >= 0 );
          if ( cls >= 0 )
            ++c[ cls ];
        }
  return c;
}

static std::vector< Edge > random_edges( int n, double p, uint64_t seed )
{
  std::mt19937_64 g( seed );
  std::uniform_real_distribution< double > u( 0, 1 );
  std::vector< Edge > e;
  for ( int a = 0; a < n; ++a )
    for ( int b = 0; b < n; ++b )
      if ( a != b && u( g ) < p )
        e.push_back( { a, b, u( g ) < 0.3 ? -1.0 : 1.0 } );
  return e;
}

static void test_census4()
{
  int prev = -1, edges_ok = 0;
  for ( int i = 0; i < kMotifClasses4; ++i )
  {
    const auto& m = motif4_info( i );
    CHECK( m.id > prev );
    prev = m.id;
    CHECK( motif4_index_from_id( m.id ) == i );
    edges_ok += ( m.edges >= 3 && m.edges <= 12 ) ? 1 : 0;
  }
  CHECK( edges_ok == kMotifClasses4 );
  CHECK( motif4_info( 0 ).edges == 3 );
  CHECK( motif4_info( kMotifClasses4 - 1 ).edges == 12 );
  for ( int seed = 0; seed < 10; ++seed )
  {
    const int n = 6 + seed;
    const auto g = make_graph( random_edges( n, 0.1 + 0.05 * seed, seed ), n );
    CHECK( motif4_census( g ) == brute4( g ) );
  }
  // directed 4-cycle and a star
  CHECK( motif4_census( std::vector< Edge >{ { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 } }, 4 )[ motif4_index_from_id( [] {
    bool m[ 4 ][ 4 ] = {};
    m[ 0 ][ 1 ] = m[ 1 ][ 2 ] = m[ 2 ][ 3 ] = m[ 3 ][ 0 ] = true;
    return canon4( m );
  }() ) ] == 1 );
  // sign filter and speed
  const auto big = random_edges( 400, 0.02, 7 );
  const auto t0 = std::chrono::steady_clock::now();
  const auto c = motif4_census( big, 400 );
  const double sec = std::chrono::duration< double >( std::chrono::steady_clock::now() - t0 ).count();
  int64_t tot = 0;
  for ( auto x : c )
    tot += x;
  CHECK( tot > 0 );
  std::printf( "census4: 400 nodes, %zu edges, %lld subgraphs in %.3f s (work estimate %.0f)\n", big.size(), static_cast< long long >( tot ), sec,
    motif4_work_estimate( make_graph( big, 400 ) ) );
  CHECK( sec < 20 );
  const auto ex = motif4_census( big, 400, Sign::Excitatory );
  const auto in = motif4_census( big, 400, Sign::Inhibitory );
  int64_t te = 0, ti = 0;
  for ( int i = 0; i < kMotifClasses4; ++i )
  {
    te += ex[ i ];
    ti += in[ i ];
  }
  CHECK( te < tot && ti < tot );
  // z-scores: planted 4-cycles are over-represented
  std::vector< Edge > e = random_edges( 120, 0.012, 9 );
  for ( int k = 0; k < 30; ++k )
  {
    const int b = 4 * k;
    e.push_back( { b, b + 1 } );
    e.push_back( { b + 1, b + 2 } );
    e.push_back( { b + 2, b + 3 } );
    e.push_back( { b + 3, b } );
  }
  const auto r = motif4_zscores( e, 120, Sign::Any, 20, 5 );
  bool m[ 4 ][ 4 ] = {};
  m[ 0 ][ 1 ] = m[ 1 ][ 2 ] = m[ 2 ][ 3 ] = m[ 3 ][ 0 ] = true;
  const int cyc = motif4_index_from_id( canon4( m ) );
  std::printf( "planted 4-cycle: count %lld null %.2f sd %.2f z %.2f acc %.2f\n", static_cast< long long >( r.counts[ cyc ] ), r.null_mean[ cyc ],
    r.null_sd[ cyc ], r.z[ cyc ], r.swap_acceptance );
  CHECK( r.z[ cyc ] > 5 && r.significant[ cyc ] );
  CHECK( motif4_result_json( r ).find( "\"classes\"" ) != std::string::npos );
  const auto r2 = motif4_zscores( e, 120, Sign::Any, 20, 5 );
  CHECK( r2.counts == r.counts && r2.z == r.z );
}

// ---- cross-language hooks
static std::vector< Edge > read_edges( int& n )
{
  int m;
  std::cin >> n >> m;
  std::vector< Edge > e;
  for ( int i = 0; i < m; ++i )
  {
    Edge x;
    std::cin >> x.pre >> x.post >> x.weight;
    e.push_back( x );
  }
  return e;
}

static int cli( int argc, char** argv )
{
  const std::string mode = argv[ 1 ];
  if ( mode == "--bursts" )
  {
    BurstParams p;
    p.max_isi_start_ms = std::atof( argv[ 2 ] );
    p.max_isi_end_ms = std::atof( argv[ 3 ] );
    p.min_ibi_ms = std::atof( argv[ 4 ] );
    p.min_duration_ms = std::atof( argv[ 5 ] );
    p.min_spikes = std::atoi( argv[ 6 ] );
    std::vector< double > t;
    double x;
    while ( std::cin >> x )
      t.push_back( x );
    std::printf( "%s\n", bursts_json( detect_bursts( t, p ), static_cast< int64_t >( t.size() ), p ).c_str() );
  }
  else if ( mode == "--prc" )
  {
    size_t ns, np;
    std::cin >> ns >> np;
    std::vector< double > s( ns ), p( np );
    for ( auto& x : s )
      std::cin >> x;
    for ( auto& x : p )
      std::cin >> x;
    std::printf( "%s\n", prc_json( prc_estimate( s, p, std::atof( argv[ 2 ] ) ), 8 ).c_str() );
  }
  else if ( mode == "--hrv" )
  {
    std::vector< double > t;
    double x;
    while ( std::cin >> x )
      t.push_back( x );
    std::printf( "%s\n", hrv_json( hrv_analyze( t ) ).c_str() );
  }
  else if ( mode == "--census4" )
  {
    const std::string s = argc > 2 ? argv[ 2 ] : "any";
    const int n_null = argc > 3 ? std::atoi( argv[ 3 ] ) : 0;
    const uint64_t seed = argc > 4 ? std::strtoull( argv[ 4 ], nullptr, 10 ) : 1;
    int n;
    const auto e = read_edges( n );
    const auto r = motif4_zscores( e, n, s == "E" ? Sign::Excitatory : s == "I" ? Sign::Inhibitory : Sign::Any, n_null, seed );
    std::printf( "%s\n", motif4_result_json( r, false ).c_str() );
  }
  else
    return 2;
  return 0;
}

int main( int argc, char** argv )
{
  if ( argc > 1 )
    return cli( argc, argv );
  test_bursts();
  test_prc();
  test_hrv();
  test_census4();
  return chk::finish( "analysis_extra_test" );
}
