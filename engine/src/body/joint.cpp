#include "spike/body/joint.h"

namespace spike::body
{

void Spindle::step( double dt_ms, double length, double velocity, double gd, double gs )
{
  const double a = 1.0 - std::exp( -dt_ms / p.tau_ms );
  const double vterm = ( velocity >= 0 ? 1.0 : -p.ia_release ) * std::pow( std::fabs( velocity ), p.ia_vel_exp );
  const double dl = length - 1.0;
  const double ia = p.ia_bias + p.gamma_sta_bias * gs + p.ia_len * ( 1.0 + 0.5 * gs ) * dl + p.ia_vel * ( 1.0 + p.gamma_dyn_gain * gd ) * vterm;
  const double ii = p.ii_bias + p.gamma_sta_bias * gs + p.ii_len * ( 1.0 + 0.5 * gs ) * dl;
  this->ia += a * ( clamp( ia, 0.0, p.ia_max ) - this->ia );
  this->ii += a * ( clamp( ii, 0.0, p.ii_max ) - this->ii );
}

void Gto::step( double dt_ms, double active_frac, double passive_frac )
{
  const double f = std::max( 0.0, active_frac + p.passive_weight * passive_frac );
  const double a = 1.0 - std::exp( -dt_ms / p.tau_ms );
  ib += a * ( p.max_hz * f / ( f + p.f50 ) - ib );
}

void Joint::configure( const JointParams& p )
{
  p_ = p;
  agonist.configure( p.flexor );
  antagonist.configure( p.extensor );
  sp_ag_.p = sp_ant_.p = p.spindle;
  gto_ag_.p = gto_ant_.p = p.gto;
  reset( p.theta_ref );
}

void Joint::reset( double th0 )
{
  theta = clamp( th0, p_.theta_min, p_.theta_max );
  omega = 0;
  agonist.reset();
  antagonist.reset();
  sp_ag_.reset();
  sp_ant_.reset();
  gto_ag_.reset();
  gto_ant_.reset();
  torque_muscle = torque_gravity = torque_ext = torque_limit = 0;
  ag_aff = ant_aff = {};
}

double Joint::flexor_length( double th ) const { return 1.0 - p_.moment_arm_m * ( th - p_.theta_ref ) / p_.flexor.l_opt_m; }
double Joint::extensor_length( double th ) const { return 1.0 + p_.moment_arm_m * ( th - p_.theta_ref ) / p_.extensor.l_opt_m; }

void Joint::torques( double th, double om, double ext, double& t_mus, double& t_grav, double& t_lim ) const
{
  const double r = p_.moment_arm_m;
  const double vf = -r * om / p_.flexor.l_opt_m, ve = r * om / p_.extensor.l_opt_m;  // normalised lengthening velocities (1/s)
  const double Ff = agonist.force_at( flexor_length( th ), vf ), Fe = antagonist.force_at( extensor_length( th ), ve );
  t_mus = r * ( Ff - Fe ) - p_.damping * om;
  t_grav = -( p_.mass_kg * p_.com_m + p_.load_mass_kg * p_.load_arm_m ) * p_.g * std::cos( th ) + ext;
  t_lim = 0;
  if ( th > p_.theta_max )
    t_lim = -p_.limit_k * ( th - p_.theta_max ) - ( om > 0 ? p_.limit_b * om : 0 );
  else if ( th < p_.theta_min )
    t_lim = -p_.limit_k * ( th - p_.theta_min ) - ( om < 0 ? p_.limit_b * om : 0 );
}

void Joint::step( double dt_ms, const JointInput& in )
{
  const double r = p_.moment_arm_m;
  // neural / twitch update once per slice at the present kinematics
  MuscleInput a = in.ag, b = in.ant;
  a.length = flexor_length( theta );
  a.velocity = -r * omega / p_.flexor.l_opt_m;
  b.length = extensor_length( theta );
  b.velocity = r * omega / p_.extensor.l_opt_m;
  agonist.step( dt_ms, a );
  antagonist.step( dt_ms, b );
  // rigid-body integration with sub-steps (semi-implicit Euler)
  const double I = p_.inertia + p_.load_mass_kg * p_.load_arm_m * p_.load_arm_m;
  const int n = std::max( 1, p_.substeps );
  const double h = dt_ms * 1e-3 / n;
  for ( int k = 0; k < n; ++k )
  {
    torques( theta, omega, in.ext_torque, torque_muscle, torque_gravity, torque_limit );
    omega += h * ( torque_muscle + torque_gravity + torque_limit ) / I;
    theta += h * omega;
  }
  torque_ext = in.ext_torque;
  // proprioceptors at the new state
  const double lf = flexor_length( theta ), le = extensor_length( theta );
  const double vf = -r * omega / p_.flexor.l_opt_m, ve = r * omega / p_.extensor.l_opt_m;
  sp_ag_.step( dt_ms, lf, vf, in.gamma_dyn, in.gamma_sta );
  sp_ant_.step( dt_ms, le, ve, in.gamma_dyn, in.gamma_sta );
  gto_ag_.step( dt_ms, agonist.active_N / p_.flexor.fmax_N, agonist.passive_N / p_.flexor.fmax_N );
  gto_ant_.step( dt_ms, antagonist.active_N / p_.extensor.fmax_N, antagonist.passive_N / p_.extensor.fmax_N );
  ag_aff = { sp_ag_.ia, sp_ag_.ii, gto_ag_.ib };
  ant_aff = { sp_ant_.ia, sp_ant_.ii, gto_ant_.ib };
}

Readouts Joint::readouts() const
{
  Readouts r = { { "theta_rad", theta },           { "omega_rad_s", omega },        { "torque_muscle_Nm", torque_muscle },
                 { "torque_gravity_Nm", torque_gravity }, { "ag_Ia_hz", ag_aff.ia }, { "ag_II_hz", ag_aff.ii }, { "ag_Ib_hz", ag_aff.ib },
                 { "ant_Ia_hz", ant_aff.ia },      { "ant_II_hz", ant_aff.ii },     { "ant_Ib_hz", ant_aff.ib },
                 { "ag_force_N", agonist.force_N }, { "ant_force_N", antagonist.force_N },
                 { "ag_fatigue", agonist.fatigue_level }, { "ant_fatigue", antagonist.fatigue_level } };
  return r;
}

Joint::View Joint::view() const
{
  return { theta, ( theta - p_.theta_min ) / ( p_.theta_max - p_.theta_min ), agonist.activation, antagonist.activation,
           agonist.force_N / p_.flexor.fmax_N, antagonist.force_N / p_.extensor.fmax_N };
}

} // namespace spike::body
