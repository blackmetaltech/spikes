// "Theory" wing helpers (see spike/theory.h). Pure maths, no NEST.
#include "spike/theory.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace spike::theory
{
namespace
{
double get( const std::map< std::string, double >& p, const char* k, double dflt )
{
  auto it = p.find( k );
  return it != p.end() and std::isfinite( it->second ) ? it->second : dflt;
}

bool is_aeif( const std::string& m ) { return m == "aeif_cond_exp" or m == "aeif_cond_alpha"; }

void classify( FixedPoint& f )
{
  f.stable = f.trace < 0 and f.det > 0;
  const bool focus = f.trace * f.trace < 4.0 * f.det;
  if ( f.det < 0 )
    f.kind = "saddle";
  else
    f.kind = std::string( f.trace < 0 ? "stable " : "unstable " ) + ( focus ? "focus" : "node" );
}

struct Aeif
{
  double C, gL, EL, dT, Vth, Vpk, Vr, a, b, tau, Ie;
  // C dV/dt = -gL (V - EL) + gL dT exp((V - Vth) / dT) - w + I ; tau dw/dt = a (V - EL) - w
  double w_v( double V, double I ) const { return -gL * ( V - EL ) + gL * dT * std::exp( std::min( ( V - Vth ) / dT, 30.0 ) ) + I; }
  double w_w( double V ) const { return a * ( V - EL ); }
};
Aeif aeif_of( const std::map< std::string, double >& p )
{
  return { get( p, "C_m", 281.0 ), get( p, "g_L", 30.0 ), get( p, "E_L", -70.6 ), get( p, "Delta_T", 2.0 ), get( p, "V_th", -50.4 ), get( p, "V_peak", 0.0 ),
    get( p, "V_reset", -60.0 ), get( p, "a", 4.0 ), get( p, "b", 80.5 ), get( p, "tau_w", 144.0 ), get( p, "I_e", 0.0 ) };
}

std::vector< FixedPoint > aeif_fixed( const Aeif& m, double I )
{
  std::vector< FixedPoint > out;
  auto f = [ & ]( double V ) { return m.w_v( V, I ) - m.w_w( V ); };
  const double lo = m.EL - 40.0, hi = std::max( m.Vth + 12.0 * m.dT, m.EL + 20.0 );
  const int N = 4000;
  double prev = f( lo ), pv = lo;
  for ( int i = 1; i <= N; ++i )
  {
    const double V = lo + ( hi - lo ) * i / N, cur = f( V );
    if ( prev == 0.0 or ( prev < 0 ) != ( cur < 0 ) )
    {
      double a = pv, b = V, fa = prev;
      for ( int k = 0; k < 60; ++k )
      {
        const double mid = 0.5 * ( a + b ), fm = f( mid );
        if ( ( fm < 0 ) == ( fa < 0 ) )
        {
          a = mid;
          fa = fm;
        }
        else
          b = mid;
      }
      FixedPoint fp;
      fp.v = 0.5 * ( a + b );
      fp.w = m.w_w( fp.v );
      const double dv = ( -m.gL + m.gL * std::exp( std::min( ( fp.v - m.Vth ) / m.dT, 30.0 ) ) ) / m.C;
      fp.trace = dv - 1.0 / m.tau;
      fp.det = -dv / m.tau + m.a / ( m.C * m.tau );
      classify( fp );
      out.push_back( fp );
    }
    prev = cur;
    pv = V;
  }
  return out;
}

// izhikevich: V' = 0.04 V^2 + 5 V + 140 - U + I ; U' = a (b V - U)
std::vector< FixedPoint > izh_fixed( double a, double b, double I )
{
  std::vector< FixedPoint > out;
  const double A = 0.04, B = 5.0 - b, Cc = 140.0 + I, disc = B * B - 4.0 * A * Cc;
  if ( disc < 0 )
    return out;
  const double r = std::sqrt( disc );
  std::vector< double > roots = { ( -B - r ) / ( 2 * A ), ( -B + r ) / ( 2 * A ) };
  if ( disc == 0 )
    roots.pop_back();
  for ( double V : roots )
  {
    FixedPoint fp;
    fp.v = V;
    fp.w = b * V;
    const double dv = 0.08 * V + 5.0;
    fp.trace = dv - a;
    fp.det = -a * dv + a * b;
    classify( fp );
    out.push_back( fp );
  }
  return out;
}
}  // namespace

bool phase_plane_supported( const std::string& model ) { return model == "izhikevich" or is_aeif( model ); }

std::vector< std::string > phase_plane_params( const std::string& model )
{
  if ( model == "izhikevich" )
    return { "a", "b", "c", "d", "I_e", "V_th" };
  if ( is_aeif( model ) )
    return { "C_m", "g_L", "E_L", "Delta_T", "V_th", "V_peak", "V_reset", "a", "b", "tau_w", "I_e" };
  return {};
}

PhasePlane phase_plane( const std::string& model, const std::map< std::string, double >& p, double i_ext )
{
  PhasePlane r;
  r.model = model;
  if ( not phase_plane_supported( model ) )
  {
    r.note = "no phase plane for " + model + ": it has no recovery variable (only izhikevich, aeif_cond_exp and aeif_cond_alpha do)";
    return r;
  }
  r.ok = true;
  const int N = 160;
  if ( model == "izhikevich" )
  {
    const double a = get( p, "a", 0.02 ), b = get( p, "b", 0.2 ), c = get( p, "c", -65.0 ), d = get( p, "d", 8.0 ), I = get( p, "I_e", 0.0 ) + i_ext;
    r.w_name = "U";
    r.i_total = I;
    r.v_peak = get( p, "V_th", 30.0 );
    r.v_reset = c;
    r.w_jump = d;
    r.v_lo = -90;
    r.v_hi = r.v_peak + 5;
    for ( int i = 0; i <= N; ++i )
    {
      const double V = r.v_lo + ( r.v_hi - r.v_lo ) * i / N;
      r.v_null.push_back( { V, 0.04 * V * V + 5.0 * V + 140.0 + I } );
      r.w_null.push_back( { V, b * V } );
    }
    r.fixed = izh_fixed( a, b, I );
    double lo = 1e9, hi = -1e9;
    for ( const auto& q : r.v_null )
      if ( q[ 0 ] >= -80 and q[ 0 ] <= 10 )
      {
        lo = std::min( lo, q[ 1 ] );
        hi = std::max( hi, q[ 1 ] );
      }
    for ( const auto& q : r.w_null )
    {
      lo = std::min( lo, q[ 1 ] );
      hi = std::max( hi, q[ 1 ] );
    }
    for ( const auto& f : r.fixed )
    {
      lo = std::min( lo, f.w );
      hi = std::max( hi, f.w );
    }
    const double pad = 0.1 * ( hi - lo ) + 1.0;
    r.w_lo = lo - pad;
    r.w_hi = std::min( hi + pad, lo + 120.0 );
    return r;
  }
  const Aeif m = aeif_of( p );
  const double I = m.Ie + i_ext;
  r.w_name = "w";
  r.i_total = I;
  r.v_peak = m.Vpk;
  r.v_reset = m.Vr;
  r.w_jump = m.b;
  r.v_lo = m.EL - 30.0;
  r.v_hi = std::max( m.Vth + 10.0 * m.dT, m.EL + 15.0 );
  r.v_hi = std::min( r.v_hi, m.Vpk );
  for ( int i = 0; i <= N; ++i )
  {
    const double V = r.v_lo + ( r.v_hi - r.v_lo ) * i / N;
    r.v_null.push_back( { V, m.w_v( V, I ) } );
    r.w_null.push_back( { V, m.w_w( V ) } );
  }
  r.fixed = aeif_fixed( m, I );
  double lo = 1e18, hi = -1e18;
  for ( const auto& q : r.w_null )
  {
    lo = std::min( lo, q[ 1 ] );
    hi = std::max( hi, q[ 1 ] );
  }
  for ( const auto& q : r.v_null )
    if ( q[ 0 ] <= m.Vth + 4.0 * m.dT )
    {
      lo = std::min( lo, q[ 1 ] );
      hi = std::max( hi, q[ 1 ] );
    }
  for ( const auto& f : r.fixed )
  {
    lo = std::min( lo, f.w );
    hi = std::max( hi, f.w );
  }
  const double pad = 0.1 * ( hi - lo ) + 1.0;
  r.w_lo = lo - pad;
  r.w_hi = hi + pad;
  return r;
}

PhasePlane phase_plane_of( const Engine& e, int32_t neuron, double i_ext )
{
  const Structure st = e.structure();
  const PopulationInfo* pop = st.population( e.neuron_population( neuron ) );
  if ( not pop )
  {
    PhasePlane r;
    r.note = "no such neuron";
    return r;
  }
  std::map< std::string, double > p;
  for ( const auto& k : phase_plane_params( pop->model ) )
  {
    const double v = e.neuron_state( neuron, k );
    if ( std::isfinite( v ) )
      p[ k ] = v;
  }
  return phase_plane( pop->model, p, i_ext );
}

Onset onset( const std::string& model, const std::map< std::string, double >& params, double i_lo, double i_hi, int n )
{
  Onset o;
  if ( not phase_plane_supported( model ) )
    return o;
  const FixedPoint* last = nullptr;
  FixedPoint keep;
  for ( int i = 0; i <= n; ++i )
  {
    const double I = i_lo + ( i_hi - i_lo ) * i / n;
    const PhasePlane pp = phase_plane( model, params, I );
    const FixedPoint* st = nullptr;
    for ( const auto& f : pp.fixed )
      if ( f.stable )
        st = &f;
    if ( not st )
    {
      if ( last )
      {
        o.found = true;
        o.current = I;
        o.kind = last->kind == "stable focus" ? "hopf" : "saddle-node";
        const double q = last->det - 0.25 * last->trace * last->trace;   // omega^2 per ms^2: the rates are per ms in both models
        o.freq_hz = q > 0 ? std::sqrt( q ) * 1000.0 / ( 2.0 * 3.14159265358979323846 ) : 0.0;
      }
      return o;
    }
    keep = *st;
    last = &keep;
  }
  return o;
}

Sta spike_triggered_average( const std::vector< double >& s, double dt, const std::vector< double >& spikes, double before_ms, double after_ms )
{
  Sta r;
  r.dt_ms = dt;
  r.before_ms = before_ms;
  const long nb = static_cast< long >( std::lround( before_ms / dt ) ), na = static_cast< long >( std::lround( after_ms / dt ) );
  r.mean.assign( static_cast< size_t >( nb + na + 1 ), 0.0 );
  for ( double t : spikes )
  {
    const long k = static_cast< long >( std::lround( t / dt ) );
    if ( k - nb < 0 or k + na >= static_cast< long >( s.size() ) )
      continue;
    for ( long j = -nb; j <= na; ++j )
      r.mean[ static_cast< size_t >( j + nb ) ] += s[ static_cast< size_t >( k + j ) ];
    ++r.n_used;
  }
  if ( r.n_used > 0 )
    for ( double& x : r.mean )
      x /= r.n_used;
  return r;
}

}  // namespace spike::theory
