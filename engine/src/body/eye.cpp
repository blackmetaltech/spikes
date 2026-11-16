#include "spike/body/eye.h"

namespace spike::body
{

static double diameter_of( const EyeParams& p, double sph, double dil )
{
  // balance of the two antagonist muscles: both silent -> mid-range (a denervated pupil is mid-dilated)
  const double x = clamp( 0.5 + 0.5 * ( dil - sph ), 0.0, 1.0 );
  return p.d_min_mm + ( p.d_max_mm - p.d_min_mm ) * x;
}

void Eye::reset( double s0, double d0 )
{
  sph_ = Lowpass( p_.sph_tau_up_ms, p_.sph_tau_down_ms );
  dil_ = Lowpass( p_.dil_tau_up_ms, p_.dil_tau_down_ms );
  sph_.reset( s0 );
  dil_.reset( d0 );
  ec_s_.delay_ms = ec_d_.delay_ms = p_.ec_delay_ms;
  ret_.delay_ms = p_.retina_latency_ms;
  ec_s_.reset( s0 );
  ec_d_.reset( d0 );
  sphincter_act = s0;
  dilator_act = d0;
  diameter_mm = diameter_of( p_, s0, d0 );
  lum_ = 1;
  illuminance_td = lum_ * kPi / 4 * diameter_mm * diameter_mm;
  retinal_hz = retina_rate( lum_, diameter_mm );
  ret_.reset( retinal_hz );
}

double Eye::retina_rate( double lum, double d ) const
{
  const double E = std::max( 0.0, lum ) * kPi / 4 * d * d;
  const double x = std::log10( 1.0 + E / p_.e0_td );
  const double xn = std::pow( x, p_.n );
  return p_.r_spont_hz + ( p_.r_max_hz - p_.r_spont_hz ) * xn / ( xn + std::pow( p_.x50, p_.n ) );
}

void Eye::step( double dt_ms, const EyeInput& in )
{
  sphincter_act = sph_.step( ec_s_.step( clamp( in.sphincter, 0.0, 1.0 ), dt_ms ), dt_ms );
  dilator_act = dil_.step( ec_d_.step( clamp( in.dilator, 0.0, 1.0 ), dt_ms ), dt_ms );
  diameter_mm = diameter_of( p_, sphincter_act, dilator_act );
  lum_ = in.luminance;
  illuminance_td = std::max( 0.0, lum_ ) * kPi / 4 * diameter_mm * diameter_mm;
  retinal_hz = ret_.step( retina_rate( lum_, diameter_mm ), dt_ms );
}

Readouts Eye::readouts() const
{
  return { { "pupil_mm", diameter_mm }, { "sphincter_act", sphincter_act }, { "dilator_act", dilator_act },
           { "retinal_td", illuminance_td }, { "retinal_hz", retinal_hz } };
}

Eye::View Eye::view() const
{
  return { diameter_mm, ( diameter_mm - p_.d_min_mm ) / ( p_.d_max_mm - p_.d_min_mm ), retinal_hz, std::log10( std::max( lum_, 1e-4 ) ) };
}

} // namespace spike::body
