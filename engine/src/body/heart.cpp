#include "spike/body/heart.h"

namespace spike::body
{

void Heart::reset()
{
  pressure = 95;
  map_beat = map_smooth = 95;
  systolic = 120;
  diastolic = 80;
  pv_ = cvp = p_.pv0;
  rr_s_ = t_since_beat_s_ = 60.0 / p_.hr_ref_bpm;
  hr_bpm = p_.hr_ref_bpm;
  t_ej_ = p_.ejection_s_per_sqrtrr * std::sqrt( rr_s_ );
  sv_beat_ = stroke_ml = p_.sv_rest;
  co_l_min = stroke_ml * hr_bpm / 1000.0;
  contractility = 1;
  resistance = p_.r0;
  sym_c_ = sym_v_ = p_.sym_rest;
  ven_shift_ = 0;
  rate_phase_ = 0.5;
  cyc_sum_ = cyc_n_ = cyc_max_ = 0;
  cyc_min_ = 1e9;
  ejecting_ = false;
  eject_ms_ = 0;
  blood_lost_ml = pooled_ml = 0;
  beats = 0;
  baro_filt_ = baro_hz = baro_mean_hz = 0;
  dpdt = dpdt_filt_ = 0;
  beat_now = false;
}

void Heart::step( double dt_ms, const HeartInput& in )
{
  const double dt = dt_ms * 1e-3;
  const HeartParams& p = p_;
  beat_now = false;
  t_since_beat_s_ += dt;

  // --- effectors (smooth muscle / myocardium: first-order) ---
  const double a = 1.0 - std::exp( -dt_ms / p.tau_sym_ms );
  sym_c_ += a * ( clamp( in.sym_cardiac, 0.0, 1.0 ) - sym_c_ );
  sym_v_ += a * ( clamp( in.sym_vascular, 0.0, 1.0 ) - sym_v_ );
  contractility = std::max( 0.2, 1.0 + p.sym_contractility * ( sym_c_ - p.sym_rest ) );
  resistance = p.r0 * std::max( 0.3, 1.0 + p.sym_resistance * ( sym_v_ - p.sym_rest ) );

  // --- perturbations act on the venous reservoir (volume -> pressure via C_ven) ---
  const double pool_target = p.pool_ml_standing * clamp( in.standing, 0.0, 1.0 );
  const double new_pool = pooled_ml + ( 1.0 - std::exp( -dt / p.tau_pool_s ) ) * ( pool_target - pooled_ml );
  const double new_shift = p.sym_venous_ml * ( sym_v_ - p.sym_rest );
  const double loss = std::max( 0.0, in.haemorrhage_ml_s ) * dt;
  pv_ += ( -( new_pool - pooled_ml ) + ( new_shift - ven_shift_ ) - loss ) / p.c_ven;
  pooled_ml = new_pool;
  ven_shift_ = new_shift;
  blood_lost_ml += loss;

  // --- beat generation ---
  bool fire = false;
  if ( in.sa_spikes > 0 )
    fire = true;
  else
  {
    double rate_hz = in.sa_rate_hz;
    if ( rate_hz < 0 )
      rate_hz = clamp( p.intrinsic_bpm + p.sym_hr_bpm * clamp( in.sym_cardiac, 0.0, 1.0 ) - p.vagal_hr_bpm * clamp( in.vagal, 0.0, 1.0 ), 30.0, 220.0 ) / 60.0;
    rate_phase_ += rate_hz * dt;
    if ( rate_phase_ >= 1.0 )
    {
      rate_phase_ -= 1.0;
      fire = true;
    }
  }
  if ( fire && t_since_beat_s_ * 1000.0 >= p.refractory_ms )
  {
    // close the previous cycle statistics
    if ( cyc_n_ > 0 && beats > 0 )
    {
      map_beat = cyc_sum_ / cyc_n_;
      systolic = cyc_max_;
      diastolic = cyc_min_;
    }
    cyc_sum_ = cyc_n_ = cyc_max_ = 0;
    cyc_min_ = 1e9;
    rr_s_ = clamp( t_since_beat_s_, 0.25, 2.0 );
    hr_bpm = 60.0 / rr_s_;
    t_ej_ = p.ejection_s_per_sqrtrr * std::sqrt( rr_s_ );
    const double td = std::max( 0.05, rr_s_ - t_ej_ );
    const double rr0 = 60.0 / p.hr_ref_bpm, td0 = rr0 - p.ejection_s_per_sqrtrr * std::sqrt( rr0 );
    const double fill = ( 1.0 - std::exp( -td / p.fill_tau_s ) ) / ( 1.0 - std::exp( -td0 / p.fill_tau_s ) );
    const double pvx = std::max( pv_, 0.05 );
    const double star = pvx / ( pvx + p.starling_k ) / ( p.pv0 / ( p.pv0 + p.starling_k ) );  // 1 at rest
    sv_beat_ = stroke_ml = std::min( p.sv_rest * 3.0, p.sv_rest * contractility * star * fill );
    co_l_min = stroke_ml * hr_bpm / 1000.0;
    pv_ -= sv_beat_ / p.c_ven;  // the ventricle takes its stroke volume from the veins ...
    ejecting_ = true;           // ... and ejects it into the arteries over T_ej
    eject_ms_ = 0;
    t_since_beat_s_ = 0;
    beat_now = true;
    ++beats;
  }

  // --- ejection flow: half-sine with integral = stroke volume ---
  double q_in = 0;
  if ( ejecting_ )
  {
    const double t = eject_ms_ * 1e-3;
    if ( t < t_ej_ )
      q_in = sv_beat_ * kPi / ( 2.0 * t_ej_ ) * std::sin( kPi * t / t_ej_ );
    else
      ejecting_ = false;
    eject_ms_ += dt_ms;
  }

  // --- closed-loop two-compartment circulation (volume conserving) ---
  const double q_out = ( pressure - pv_ ) / resistance;
  const double dP = ( q_in - q_out ) / p.c_art * dt;
  pressure += dP;
  pv_ += q_out * dt / p.c_ven;
  dpdt = dP / dt;
  cvp = pv_;

  cyc_sum_ += pressure;
  cyc_n_ += 1;
  cyc_max_ = std::max( cyc_max_, pressure );
  cyc_min_ = std::min( cyc_min_, pressure );
  map_smooth += ( 1.0 - std::exp( -dt / 2.0 ) ) * ( pressure - map_smooth );

  // --- baroreceptor (rate of the aortic / carotid sinus afferent) ---
  dpdt_filt_ += ( 1.0 - std::exp( -dt / 0.02 ) ) * ( dpdt - dpdt_filt_ );
  const double p_eff = pressure + p.baro_tau_d_s * dpdt_filt_;
  baro_hz = p.baro_max_hz * sigmoid( ( p_eff - p.baro_p50 ) / p.baro_slope );
  baro_mean_hz += ( 1.0 - std::exp( -dt / 1.0 ) ) * ( baro_hz - baro_mean_hz );
  co_l_min = stroke_ml * hr_bpm / 1000.0;
}

Readouts Heart::readouts() const
{
  return { { "map_mmHg", map_smooth },       { "sys_mmHg", systolic },     { "dia_mmHg", diastolic },
           { "pressure_mmHg", pressure },    { "hr_bpm", hr_bpm },         { "stroke_ml", stroke_ml },
           { "co_l_min", co_l_min },         { "cvp_mmHg", cvp },          { "baro_hz", baro_mean_hz },
           { "contractility", contractility }, { "tpr", resistance },      { "blood_lost_ml", blood_lost_ml },
           { "pooled_ml", pooled_ml } };
}

Heart::View Heart::view() const
{
  const double ej = ejecting_ && t_ej_ > 0 ? std::sin( kPi * clamp( eject_ms_ * 1e-3 / t_ej_, 0.0, 1.0 ) ) : 0.0;
  return { clamp( t_since_beat_s_ / rr_s_, 0.0, 1.0 ), ej, pressure, map_smooth, systolic, diastolic, hr_bpm, beat_now };
}

} // namespace spike::body
