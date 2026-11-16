#include "spike/body/lungs.h"

namespace spike::body
{

void Lungs::reset()
{
  volume_l = p_.frc_l;
  flow_l_s = pmus = tidal_l = rate_bpm = ve_l_min = va_l_min = 0;
  paco2 = p_.pa_rest;
  tissue_pco2 = paco2 + p_.k863 * p_.vco2_rest_l_min / p_.q_c_l_min;
  inspiring = false;
  breaths = 0;
  vco2_l_min = p_.vco2_rest_l_min;
  mus_ = Lowpass( p_.muscle_tau_ms, p_.muscle_tau_ms );
  per_ = Lowpass( p_.periph_tau_s * 1000, p_.periph_tau_s * 1000 );
  per_.reset( p_.periph_base_hz + p_.periph_gain_hz_mmhg * ( paco2 - p_.periph_p0 ) );
  peripheral_hz = per_.y;
  central_hz = 0;
  t_s_ = 0;
  breath_start_t_ = -1;
  v_start_ = volume_l;
  cum_in_ = breath_va_ = 0;
  win_.clear();
  va_f_ = 0.07;
}

void Lungs::step( double dt_ms, const LungInput& in )
{
  const double dt = dt_ms * 1e-3;
  t_s_ += dt;
  // --- mechanics ---
  pmus = p_.pmus_max * mus_.step( clamp( in.drive, 0.0, 1.0 ), dt_ms );
  const double recoil = ( volume_l - p_.frc_l ) / p_.compliance_l_cmh2o;
  flow_l_s = ( pmus - recoil ) / p_.resistance;
  const double v_old = volume_l;
  volume_l = std::max( p_.rv_l, volume_l + flow_l_s * dt );
  const double dv = volume_l - v_old;

  // --- breath detection and per-breath bookkeeping ---
  double dva = 0;  // alveolar part of the inspired increment (first dead-space litres of each breath do not reach the alveoli)
  if ( dv > 0 )
  {
    if ( !inspiring && flow_l_s > 0.05 )
    {
      inspiring = true;
      if ( breath_start_t_ >= 0 )
        rate_bpm = 60.0 / std::max( 0.5, t_s_ - breath_start_t_ );
      breath_start_t_ = t_s_;
      v_start_ = v_old;
      cum_in_ = 0;
      breath_va_ = 0;
    }
    if ( inspiring )
    {
      const double prev = cum_in_;
      cum_in_ += dv;
      dva = std::max( 0.0, cum_in_ - p_.dead_space_l ) - std::max( 0.0, prev - p_.dead_space_l );
      breath_va_ += dva;
    }
  }
  if ( inspiring && flow_l_s < 0.0 )
  {
    inspiring = false;
    tidal_l = volume_l - v_start_;
    win_.push_back( { t_s_, tidal_l, breath_va_ } );
    ++breaths;
  }
  while ( !win_.empty() && t_s_ - win_.front().t > 30.0 )
    win_.pop_front();
  {
    double vt = 0, va = 0;
    for ( const auto& b : win_ )
      vt += b.vt, va += b.va;
    const double span = std::min( 30.0, std::max( t_s_, 1.0 ) );
    ve_l_min = vt / span * 60.0;
    va_l_min = va / span * 60.0;
  }

  // --- CO2 pools (small explicit sub-steps) ---
  vco2_l_min = p_.vco2_rest_l_min * std::max( 0.0, in.metabolic );
  const double s = p_.time_scale;
  const double qc = p_.q_c_l_min / 60.0, vco2 = vco2_l_min / 60.0;
  const int n = std::max( 1, int( std::ceil( dt * s / 0.05 ) ) );
  // alveolar ventilation (L/s) is pulsatile; the gas pools see it through a ~2 s mixing low-pass (in game time)
  va_f_ += ( 1.0 - std::exp( -dt * s / 2.0 ) ) * ( dva / dt - va_f_ );
  const double h = dt * s / n, va_s = va_f_;
  for ( int k = 0; k < n; ++k )
  {
    const double d_pa = ( qc * ( tissue_pco2 - paco2 ) - va_s * ( paco2 - in.inspired_pco2 ) ) / p_.v_art;
    const double d_pt = ( p_.k863 * vco2 - qc * ( tissue_pco2 - paco2 ) ) / p_.v_tissue;
    paco2 += h * d_pa;
    tissue_pco2 += h * d_pt;
  }
  // time_scale divides all pool volumes (every rate x s), so steady states are unchanged.
  paco2 = std::max( 0.0, paco2 );

  // --- chemoreceptors ---
  const double per_target = ( p_.periph_base_hz + p_.periph_gain_hz_mmhg * ( paco2 - p_.periph_p0 ) ) * ( 1.0 + 2.0 * clamp( in.hypoxia, 0.0, 1.0 ) );
  peripheral_hz = clamp( per_.step( std::max( 0.0, per_target ), dt_ms ), 0.0, p_.periph_max_hz );
  central_hz = clamp( p_.central_gain_hz_mmhg * ( tissue_pco2 - p_.central_p0 ), 0.0, p_.central_max_hz );
}

Readouts Lungs::readouts() const
{
  return { { "paco2_mmHg", paco2 },       { "tissue_pco2_mmHg", tissue_pco2 }, { "volume_l", volume_l },
           { "tidal_l", tidal_l },        { "resp_rate_bpm", rate_bpm },       { "ve_l_min", ve_l_min },
           { "va_l_min", va_l_min },      { "periph_chemo_hz", peripheral_hz }, { "central_chemo_hz", central_hz } };
}

Lungs::View Lungs::view() const
{
  return { clamp( ( volume_l - p_.rv_l ) / 3.5, 0.0, 1.0 ), flow_l_s, paco2, ve_l_min, rate_bpm, inspiring };
}

} // namespace spike::body
