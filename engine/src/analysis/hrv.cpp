// Heart-rate variability: time-domain indices and Lomb-Scargle LF/HF power. Pure C++; python/spike/hrv.py mirrors it.
#include <algorithm>
#include <cmath>

#include "json_util.h"
#include "spike/analysis.h"

namespace spike::analysis
{
namespace
{
constexpr double kPi = 3.14159265358979323846;

struct Tachogram
{
  std::vector< double > t_s;  // time of each RR value (the closing beat), seconds from the first beat
  std::vector< double > rr;   // ms
};

Tachogram tachogram( std::vector< double > beats )
{
  std::sort( beats.begin(), beats.end() );
  Tachogram g;
  for ( size_t i = 1; i < beats.size(); ++i )
  {
    g.t_s.push_back( ( beats[ i ] - beats[ 0 ] ) * 1e-3 );
    g.rr.push_back( beats[ i ] - beats[ i - 1 ] );
  }
  return g;
}

// Unnormalised Lomb-Scargle power P(f) of y at times t (see analysis.h), y already mean-removed.
double ls_power( const std::vector< double >& t, const std::vector< double >& y, double f )
{
  const double w = 2.0 * kPi * f;
  double s2 = 0, c2 = 0;
  for ( double x : t )
  {
    s2 += std::sin( 2.0 * w * x );
    c2 += std::cos( 2.0 * w * x );
  }
  const double tau = std::atan2( s2, c2 ) / ( 2.0 * w );
  double yc = 0, ys = 0, cc = 0, ss = 0;
  for ( size_t i = 0; i < t.size(); ++i )
  {
    const double c = std::cos( w * ( t[ i ] - tau ) ), s = std::sin( w * ( t[ i ] - tau ) );
    yc += y[ i ] * c;
    ys += y[ i ] * s;
    cc += c * c;
    ss += s * s;
  }
  double p = 0;
  if ( cc > 0 )
    p += yc * yc / cc;
  if ( ss > 0 )
    p += ys * ys / ss;
  return 0.5 * p;
}

std::vector< double > density( const Tachogram& g, const std::vector< double >& freqs )
{
  const size_t n = g.rr.size();
  std::vector< double > out;
  if ( n < 3 )
    return out;
  double mean = 0;
  for ( double x : g.rr )
    mean += x;
  mean /= static_cast< double >( n );
  std::vector< double > y( n );
  for ( size_t i = 0; i < n; ++i )
    y[ i ] = g.rr[ i ] - mean;
  const double span = g.t_s.back() - 0.0;  // T: first beat to last beat
  const double scale = 2.0 * span / static_cast< double >( n );
  for ( double f : freqs )
    out.push_back( f > 0 ? ls_power( g.t_s, y, f ) * scale : 0.0 );
  return out;
}

double band_power( const Tachogram& g, double lo, double hi, double df )
{
  const int nk = std::max( 1, static_cast< int >( std::lround( ( hi - lo ) / df ) ) );
  std::vector< double > f( nk );
  for ( int k = 0; k < nk; ++k )
    f[ k ] = lo + ( k + 0.5 ) * df;
  double s = 0;
  for ( double x : density( g, f ) )
    s += x;
  return s * df;
}
} // namespace

std::vector< double > hrv_lomb_scargle( const std::vector< double >& beat_times_ms, const std::vector< double >& freqs_hz )
{
  return density( tachogram( beat_times_ms ), freqs_hz );
}

HrvResult hrv_analyze( const std::vector< double >& beat_times_ms, const HrvParams& p )
{
  HrvResult r;
  const auto g = tachogram( beat_times_ms );
  r.n_beats = static_cast< int32_t >( beat_times_ms.size() );
  r.n_rr = static_cast< int32_t >( g.rr.size() );
  const size_t n = g.rr.size();
  if ( n < 1 )
    return r;
  double mean = 0;
  for ( double x : g.rr )
    mean += x;
  mean /= static_cast< double >( n );
  r.mean_rr_ms = mean;
  r.mean_hr_bpm = mean > 0 ? 60000.0 / mean : NAN;
  if ( n >= 2 )
  {
    double var = 0;
    for ( double x : g.rr )
      var += ( x - mean ) * ( x - mean );
    r.sdnn_ms = std::sqrt( var / static_cast< double >( n - 1 ) );
    double sq = 0;
    int64_t over = 0;
    for ( size_t i = 1; i < n; ++i )
    {
      const double d = g.rr[ i ] - g.rr[ i - 1 ];
      sq += d * d;
      over += std::fabs( d ) > 50.0 ? 1 : 0;
    }
    r.rmssd_ms = std::sqrt( sq / static_cast< double >( n - 1 ) );
    r.pnn50_pct = 100.0 * static_cast< double >( over ) / static_cast< double >( n - 1 );
  }
  if ( r.n_beats >= p.min_beats_spectrum && g.t_s.back() > 0 )
  {
    r.spectrum_ok = true;
    r.lf_ms2 = band_power( g, p.lf_lo_hz, p.lf_hi_hz, p.df_hz );
    r.hf_ms2 = band_power( g, p.hf_lo_hz, p.hf_hi_hz, p.df_hz );
    r.lf_hf = r.hf_ms2 > 0 ? r.lf_ms2 / r.hf_ms2 : NAN;
    const double tot = r.lf_ms2 + r.hf_ms2;
    if ( tot > 0 )
    {
      r.lf_nu = 100.0 * r.lf_ms2 / tot;
      r.hf_nu = 100.0 * r.hf_ms2 / tot;
    }
  }
  return r;
}

std::string hrv_json( const HrvResult& r )
{
  using detail::jnum;
  std::ostringstream o;
  auto field = [ & ]( const char* k, double v, bool first = false ) {
    o << ( first ? "" : "," ) << "\"" << k << "\":";
    jnum( o, v );
  };
  o << "{\"n_beats\":" << r.n_beats << ",\"n_rr\":" << r.n_rr << ",";
  field( "mean_rr_ms", r.mean_rr_ms, true );
  field( "mean_hr_bpm", r.mean_hr_bpm );
  field( "sdnn_ms", r.sdnn_ms );
  field( "rmssd_ms", r.rmssd_ms );
  field( "pnn50_pct", r.pnn50_pct );
  field( "lf_ms2", r.lf_ms2 );
  field( "hf_ms2", r.hf_ms2 );
  field( "lf_hf", r.lf_hf );
  field( "lf_nu", r.lf_nu );
  field( "hf_nu", r.hf_nu );
  o << ",\"spectrum_ok\":" << ( r.spectrum_ok ? "true" : "false" ) << "}";
  return o.str();
}

} // namespace spike::analysis
