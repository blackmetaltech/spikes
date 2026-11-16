#include "spike/body/muscle.h"

namespace spike::body
{

void Muscle::configure( const MuscleParams& p )
{
  p_ = p;
  const int n = std::max( 1, p.n_units );
  units_.assign( n, MotorUnit{} );
  double sum = 0;
  for ( int i = 0; i < n; ++i )
  {
    const double f = n > 1 ? double( i ) / ( n - 1 ) : 0.0;  // 0 = smallest, 1 = largest
    MotorUnit& u = units_[ i ];
    u.weight = std::pow( p.force_range, f );  // relative twitch/tetanic force P_i
    sum += u.weight;
    // contraction time scales as P^-c with c chosen so that the largest unit hits contraction_ms_fast (Fuglevand 1993 eq. 4)
    u.contraction_ms = p.contraction_ms_slow * std::pow( p.contraction_ms_fast / p.contraction_ms_slow, f );
    // 2-stage critically damped kernel: s(t) = (t/T) exp(1 - t/T); falls to 1/2 at t = T (1 + x), (1+x)exp(-x) = 1/2 -> x = 1.678
    u.half_relax_ms = 1.678 * u.contraction_ms;
    u.threshold = p.thr_first * std::pow( p.thr_last / p.thr_first, f );
    u.peak_rate_hz = p.peak_rate_slow_hz + ( p.peak_rate_fast_hz - p.peak_rate_slow_hz ) * f;
    u.vmax = p.vmax_slow + ( p.vmax_fast - p.vmax_slow ) * f;
    u.fatigue_rate = p.fatigue_rate_slow + ( p.fatigue_rate_fast - p.fatigue_rate_slow ) * f;
  }
  for ( auto& u : units_ )
    u.weight /= sum;
  reset();
}

void Muscle::reset()
{
  rng_ = Rng( p_.seed );
  time_ms_ = 0;
  last_spike_ms_.assign( units_.size(), -1e9 );
  const int n = int( units_.size() );
  for ( int i = 0; i < n; ++i )
  {
    MotorUnit& u = units_[ i ];
    u.x1 = u.x2 = u.act = 0;
    u.capacity = 1;
    u.rate_hz = 0;
    u.phase = 0.5 + 0.5 * ( ( i * 7 ) % n ) / n;  // deterministic stagger so units do not fire in lock-step
  }
  force_N = active_N = passive_N = activation = fatigue_level = 0;
  n_active = 0;
  passive_N = p_.fmax_N * passive( 1.0 );
  force_N = passive_N;
}

double Muscle::force_length( double l ) const
{
  const double z = ( l - 1.0 ) / p_.fl_width;
  return std::exp( -z * z );
}

double Muscle::passive( double l ) const
{
  if ( l <= 1.0 )
    return 0;
  return ( std::exp( p_.pe_k * ( l - 1.0 ) / p_.pe_e0 ) - 1.0 ) / ( std::exp( p_.pe_k ) - 1.0 );
}

double Muscle::force_velocity( double u, double k, double ecc )
{
  // Hill 1938 for shortening (u >= 0): (1-u)/(1+u/k), zero beyond Vmax.
  // Lengthening (u < 0, w = -u): (1 + ecc*s*w)/(1 + s*w), slope-matched at 0 (s = (1+1/k)/(ecc-1)), plateau = ecc.
  if ( u >= 0 )
    return u >= 1.0 ? 0.0 : ( 1.0 - u ) / ( 1.0 + u / k );
  const double w = -u, s = ( 1.0 + 1.0 / k ) / ( ecc - 1.0 );
  return ( 1.0 + ecc * s * w ) / ( 1.0 + s * w );
}

double Muscle::force_at( double l, double v ) const
{
  const double fl = force_length( l );
  double act = 0;
  for ( const auto& u : units_ )
    act += u.weight * u.capacity * u.act * force_velocity( -v / u.vmax, p_.curvature_k, p_.ecc_plateau );
  double F = act * fl + passive( l ) + p_.damping * v;
  return p_.fmax_N * std::max( F, 0.0 );
}

void Muscle::step( double dt_ms, const MuscleInput& in )
{
  const bool spike_mode = in.spikes && in.spikes->size() == units_.size();
  const double e = clamp( in.excitation, 0.0, 1.0 );
  const double dt_s = dt_ms * 1e-3;
  time_ms_ += dt_ms;
  double act_sum = 0, cap_sum = 0;
  int active = 0;
  for ( size_t i = 0; i < units_.size(); ++i )
  {
    MotorUnit& u = units_[ i ];
    int n_sp = 0;
    if ( spike_mode )
    {
      n_sp = std::max( 0, ( *in.spikes )[ i ] );
      u.rate_hz += 0.05 * ( n_sp / dt_s - u.rate_hz );  // display only
    }
    else
    {
      const double r = e > u.threshold ? std::min( u.peak_rate_hz, p_.min_rate_hz + p_.rate_gain_hz * ( e - u.threshold ) ) : 0.0;
      if ( r > 0 && u.rate_hz == 0 )
        u.phase = 1.0;  // a newly recruited unit fires at once (the recruitment spike)
      u.rate_hz = r;
      if ( r > 0 )
      {
        double inc = r * dt_s;
        if ( p_.rate_cv > 0 )
          inc *= 1.0 + p_.rate_cv * ( rng_.uniform() * 3.4641016 - 1.7320508 );  // uniform, unit variance
        u.phase += inc;
        while ( u.phase >= 1.0 )
        {
          u.phase -= 1.0;
          ++n_sp;
        }
      }
      else
        u.phase = 1.0;
    }
    if ( n_sp > 0 )
      last_spike_ms_[ i ] = time_ms_;
    // Twitch kernel: two cascaded first-order stages with equal tau = T; impulse amplitude e*1 so one twitch peaks at c = 1.
    // x1' = -x1/T,  x2' = (x1 - x2)/T. Exact update over dt for piecewise-constant dynamics (impulses applied first).
    u.x1 += n_sp * 2.718281828459045;
    const double d = dt_ms / u.contraction_ms, ed = std::exp( -d );
    const double x1 = u.x1, x2 = u.x2;
    u.x1 = x1 * ed;
    u.x2 = ed * ( x2 + x1 * d );
    u.act = 1.0 - std::exp( -u.x2 / p_.sat_c0 );
    if ( p_.fatigue )
    {
      const double s = p_.fatigue_time_scale;
      u.capacity += dt_s * s * ( -u.fatigue_rate * u.act * u.capacity + p_.recovery_rate * ( 1.0 - u.capacity ) );
      u.capacity = clamp( u.capacity, 0.0, 1.0 );
    }
    act_sum += u.weight * u.capacity * u.act;
    cap_sum += u.weight * u.capacity;
    if ( time_ms_ - last_spike_ms_[ i ] < 100.0 )
      ++active;
  }
  activation = act_sum;
  fatigue_level = 1.0 - cap_sum;
  n_active = active;
  passive_N = p_.fmax_N * std::max( passive( in.length ) + p_.damping * in.velocity, 0.0 );
  force_N = force_at( in.length, in.velocity );
  active_N = std::max( 0.0, force_N - passive_N );
}

Readouts Muscle::readouts() const
{
  return { { "muscle_force_N", force_N },        { "muscle_active_N", active_N }, { "muscle_passive_N", passive_N },
           { "muscle_activation", activation },  { "muscle_fatigue", fatigue_level }, { "muscle_units_active", double( n_active ) } };
}

Muscle::View Muscle::view() const
{
  View v;
  for ( const auto& u : units_ )
    v.unit_activation.push_back( float( u.act * u.capacity ) );
  v.activation = activation;
  v.force_frac = force_N / p_.fmax_N;
  v.fatigue = fatigue_level;
  return v;
}

} // namespace spike::body
