#include "spike/body/gut.h"

namespace spike::body
{

void Gut::reset( double pos )
{
  contraction.assign( p_.n_segments, 0.0 );
  pressure.assign( p_.n_segments, 0.0 );
  bolus_pos = pos;
  bolus_vel = 0;
  distance = 0;
  t_s_ = 0;
}

double Gut::pressure_at( double x ) const
{
  const int n = int( pressure.size() );
  const double u = clamp( x - 0.5, 0.0, double( n - 1 ) );
  const int i = std::min( int( u ), n - 2 );
  if ( n < 2 )
    return pressure.empty() ? 0.0 : pressure[ 0 ];
  const double f = u - i;
  return pressure[ i ] * ( 1 - f ) + pressure[ i + 1 ] * f;
}

void Gut::step( double dt_ms, const GutInput& in )
{
  const double dt = dt_ms * 1e-3;
  t_s_ += dt;
  const int n = p_.n_segments;
  for ( int i = 0; i < n; ++i )
  {
    const double ex = in.excitatory && i < int( in.excitatory->size() ) ? ( *in.excitatory )[ i ] : 0.0;
    const double inh = in.inhibitory && i < int( in.inhibitory->size() ) ? ( *in.inhibitory )[ i ] : 0.0;
    double sw = 0;
    if ( in.slow_waves )
    {
      const double ph = p_.slow_wave_hz * t_s_ - p_.phase_lag_cycles * i;
      sw = std::max( 0.0, std::sin( 2 * kPi * ph ) );   // depolarising half of the slow wave (plateau)
    }
    const double m = ex - inh + p_.slow_wave_amp * sw;
    const double target = sigmoid( ( m - p_.threshold ) / p_.width );
    const double tau = target > contraction[ i ] ? p_.tau_up_ms : p_.tau_down_ms;
    contraction[ i ] += ( 1.0 - std::exp( -dt_ms / tau ) ) * ( target - contraction[ i ] );
    pressure[ i ] = p_.p_max * contraction[ i ];
  }
  // bolus: net force from the pressure drop across its length (oral side pushes, aboral side resists)
  const double half = 0.5 * p_.bolus_len;
  const double dp = pressure_at( bolus_pos - half ) - pressure_at( bolus_pos + half );
  const double drive = dp > p_.p_static ? dp - p_.p_static : ( dp < -p_.p_static ? dp + p_.p_static : 0.0 );
  const double v_target = p_.mu * drive;
  bolus_vel += ( 1.0 - std::exp( -dt_ms / p_.v_tau_ms ) ) * ( v_target - bolus_vel );
  const double old = bolus_pos;
  bolus_pos = clamp( bolus_pos + bolus_vel * dt, half, n - half );
  if ( bolus_pos == half || bolus_pos == n - half )
    bolus_vel = 0;
  distance += bolus_pos - old;
}

Readouts Gut::readouts() const
{
  Readouts r = { { "bolus_pos", bolus_pos }, { "bolus_vel", bolus_vel }, { "bolus_distance", distance } };
  double m = 0;
  for ( double c : contraction )
    m += c;
  r[ "mean_contraction" ] = contraction.empty() ? 0 : m / contraction.size();
  for ( size_t i = 0; i < contraction.size(); ++i )
    r[ "seg" + std::to_string( i ) ] = contraction[ i ];
  return r;
}

Gut::View Gut::view() const
{
  View v;
  for ( double c : contraction )
    v.contraction.push_back( float( c ) );
  v.bolus_pos_frac = bolus_pos / p_.n_segments;
  v.bolus_len_frac = p_.bolus_len / p_.n_segments;
  return v;
}

} // namespace spike::body
