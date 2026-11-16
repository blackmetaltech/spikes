// Phase-response curve estimation from spike (cycle marker) times and perturbation times. Pure C++; python/spike/prc.py mirrors it.
#include <algorithm>
#include <cmath>
#include <limits>

#include "json_util.h"
#include "spike/analysis.h"

namespace spike::analysis
{
namespace
{
double median( std::vector< double > v )
{
  std::sort( v.begin(), v.end() );
  const size_t n = v.size();
  return n % 2 ? v[ n / 2 ] : 0.5 * ( v[ n / 2 - 1 ] + v[ n / 2 ] );
}
} // namespace

PrcResult prc_estimate( const std::vector< double >& spike_times_ms, const std::vector< double >& perturb_times_ms, double period_ms )
{
  PrcResult r;
  std::vector< double > t = spike_times_ms, pt = perturb_times_ms;
  std::sort( t.begin(), t.end() );
  std::sort( pt.begin(), pt.end() );
  r.n_perturbations = static_cast< int32_t >( pt.size() );
  const int64_t ncyc = static_cast< int64_t >( t.size() ) - 1;  // cycle k: t[k] .. t[k+1]
  if ( ncyc < 1 )
  {
    r.n_rejected = r.n_perturbations;
    return r;
  }
  // perturbations per cycle (cycle k holds t[k] <= p < t[k+1]); outside the train: rejected
  std::vector< int > cnt( ncyc, 0 );
  std::vector< int64_t > cyc_of( pt.size(), -1 );
  for ( size_t i = 0; i < pt.size(); ++i )
  {
    const auto it = std::upper_bound( t.begin(), t.end(), pt[ i ] );
    const int64_t k = static_cast< int64_t >( it - t.begin() ) - 1;
    if ( k >= 0 && k < ncyc )
    {
      cyc_of[ i ] = k;
      ++cnt[ k ];
    }
  }
  auto quiet = [ & ]( int64_t k ) { return k < 0 || cnt[ k ] == 0; };
  auto clean = [ & ]( int64_t k ) { return quiet( k ) && quiet( k - 1 ) && quiet( k - 2 ); };
  if ( period_ms > 0 )
    r.period_ms = period_ms;
  else
  {
    std::vector< double > c;
    for ( int64_t k = 0; k < ncyc; ++k )
      if ( clean( k ) )
        c.push_back( t[ k + 1 ] - t[ k ] );
    r.n_clean_cycles = static_cast< int32_t >( c.size() );
    if ( c.empty() )
    {
      r.n_rejected = r.n_perturbations;
      return r;
    }
    r.period_ms = median( c );
  }
  if ( period_ms > 0 )
    for ( int64_t k = 0; k < ncyc; ++k )
      r.n_clean_cycles += clean( k ) ? 1 : 0;
  for ( size_t i = 0; i < pt.size(); ++i )
  {
    const int64_t k = cyc_of[ i ];
    if ( k < 0 || cnt[ k ] != 1 || !quiet( k - 1 ) || !quiet( k - 2 ) )
    {
      ++r.n_rejected;
      continue;
    }
    PrcPoint p;
    p.perturb_ms = pt[ i ];
    p.prev_spike_ms = t[ k ];
    p.cycle_ms = t[ k + 1 ] - t[ k ];
    p.phase = ( pt[ i ] - t[ k ] ) / r.period_ms;
    p.shift = ( r.period_ms - p.cycle_ms ) / r.period_ms;
    r.points.push_back( p );
  }
  std::stable_sort( r.points.begin(), r.points.end(), []( const PrcPoint& a, const PrcPoint& b ) { return a.phase < b.phase; } );
  return r;
}

PrcResult prc_estimate( const std::vector< Spike >& spikes, int32_t n, const std::vector< int32_t >& neurons,
  const std::vector< double >& perturb_times_ms, double period_ms, double merge_gap_ms )
{
  const auto pooled = pooled_spike_times( spikes, n, neurons, -std::numeric_limits< double >::infinity(), std::numeric_limits< double >::infinity() );
  std::vector< double > marks;
  double last = -std::numeric_limits< double >::infinity();
  for ( double x : pooled )
  {
    if ( marks.empty() || merge_gap_ms <= 0 || x - last > merge_gap_ms )
      marks.push_back( x );
    last = x;
  }
  return prc_estimate( marks, perturb_times_ms, period_ms );
}

std::vector< PrcBin > prc_bins( const PrcResult& r, int nbins )
{
  nbins = std::max( 1, nbins );
  std::vector< std::vector< double > > v( nbins );
  for ( const auto& p : r.points )
  {
    const int b = std::min( nbins - 1, std::max( 0, static_cast< int >( std::floor( p.phase * nbins ) ) ) );
    v[ b ].push_back( p.shift );
  }
  std::vector< PrcBin > out( nbins );
  for ( int b = 0; b < nbins; ++b )
  {
    out[ b ].phase_center = ( b + 0.5 ) / nbins;
    out[ b ].count = static_cast< int32_t >( v[ b ].size() );
    if ( v[ b ].empty() )
      continue;
    double m = 0;
    for ( double x : v[ b ] )
      m += x;
    m /= static_cast< double >( v[ b ].size() );
    out[ b ].mean_shift = m;
    if ( v[ b ].size() >= 2 )
    {
      double s = 0;
      for ( double x : v[ b ] )
        s += ( x - m ) * ( x - m );
      out[ b ].sd_shift = std::sqrt( s / static_cast< double >( v[ b ].size() - 1 ) );
    }
  }
  return out;
}

std::string prc_json( const PrcResult& r, int nbins )
{
  using detail::jnum;
  std::ostringstream o;
  o << "{\"period_ms\":";
  jnum( o, r.period_ms );
  o << ",\"n_clean_cycles\":" << r.n_clean_cycles << ",\"n_perturbations\":" << r.n_perturbations << ",\"n_rejected\":" << r.n_rejected
    << ",\"points\":[";
  for ( size_t i = 0; i < r.points.size(); ++i )
  {
    o << ( i ? "," : "" ) << "{\"phase\":";
    jnum( o, r.points[ i ].phase );
    o << ",\"shift\":";
    jnum( o, r.points[ i ].shift );
    o << ",\"cycle_ms\":";
    jnum( o, r.points[ i ].cycle_ms );
    o << "}";
  }
  o << "],\"bins\":[";
  const auto b = prc_bins( r, nbins );
  for ( size_t i = 0; i < b.size(); ++i )
  {
    o << ( i ? "," : "" ) << "{\"phase\":";
    jnum( o, b[ i ].phase_center );
    o << ",\"mean_shift\":";
    jnum( o, b[ i ].mean_shift );
    o << ",\"sd_shift\":";
    jnum( o, b[ i ].sd_shift );
    o << ",\"count\":" << b[ i ].count << "}";
  }
  o << "]}";
  return o.str();
}

} // namespace spike::analysis
