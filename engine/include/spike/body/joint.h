// Single hinge joint with an agonist (flexor) / antagonist (extensor) muscle pair, inertia, gravity, load, limits,
// and the proprioceptors of each muscle (muscle spindle Ia/II, Golgi tendon organ Ib). See docs/body/joint.md.
#pragma once
#include "spike/body/muscle.h"

namespace spike::body
{

/// Spindle afferent model, Prochazka & Gorassini (1998) / Houk et al. (1981) form: rate = bias + k_L*dL + k_V*sign(v)|v|^0.6.
/// Lengths in l_opt, velocities in l_opt/s, output in impulses/s. Fusimotor drive scales the gains (Mileusnic et al. 2006).
struct SpindleParams
{
  double ia_bias = 40, ia_len = 80, ia_vel = 40, ia_vel_exp = 0.6, ia_release = 0.5;  // release: less sensitive when shortening
  double ii_bias = 30, ii_len = 90;
  double gamma_dyn_gain = 1.0;   // Ia velocity gain x (1 + gain * gamma_dyn)
  double gamma_sta_bias = 40;    // + Hz at gamma_static = 1 (both Ia and II), and length gain x (1 + 0.5 gamma_sta)
  double ia_max = 250, ii_max = 120;
  double tau_ms = 10;            // transduction low-pass
};
struct Spindle
{
  SpindleParams p;
  double ia = 0, ii = 0;
  void reset() { ia = p.ia_bias; ii = p.ii_bias; }
  void step( double dt_ms, double length, double velocity, double gamma_dyn, double gamma_sta );
};

/// Golgi tendon organ: saturating function of muscle force (mostly active; Houk & Henneman 1967): Ib = max * F/(F + F50).
struct GtoParams
{
  double max_hz = 120, f50 = 0.25, passive_weight = 0.3, tau_ms = 20;
};
struct Gto
{
  GtoParams p;
  double ib = 0;
  void reset() { ib = 0; }
  void step( double dt_ms, double active_frac, double passive_frac );
};

struct JointParams
{
  MuscleParams flexor;           // agonist: raises theta
  MuscleParams extensor;         // antagonist
  double moment_arm_m = 0.04;    // both muscles, constant
  double theta_ref = 0.2;        // rad, joint angle at which both muscles are at optimal length (mid-range)
  double theta_min = -1.0, theta_max = 1.45; // limits (rad); theta = 0 is horizontal, positive = flexion (limb raised, stops short of vertical)
  double inertia = 0.07;         // kg m^2 about the joint (forearm + hand ~ 0.06-0.09)
  double mass_kg = 1.5, com_m = 0.17;        // segment mass and centre-of-mass distance (gravity torque = -m g c cos(theta))
  double load_mass_kg = 0, load_arm_m = 0.30;// held weight at load_arm_m: adds torque and inertia
  double damping = 0.05;         // N m s/rad (soft tissue)
  double limit_k = 150, limit_b = 3;         // stiffness (N m/rad) and damping of the joint stops
  double g = 9.81;
  int substeps = 4;              // physics sub-steps per slice
  SpindleParams spindle;
  GtoParams gto;
  JointParams()
  {
    flexor.fmax_N = 350;
    extensor.fmax_N = 350;
    flexor.seed = 11;
    extensor.seed = 12;
  }
};

struct JointInput
{
  MuscleInput ag, ant;           // length / velocity fields are overwritten by the joint
  double ext_torque = 0;         // N m, external push (positive = flexion direction)
  double gamma_dyn = 0.3, gamma_sta = 0.3;  // fusimotor drive 0..1 (both spindles)
};

struct Afferents { double ia = 0, ii = 0, ib = 0; };

class Joint
{
public:
  explicit Joint( JointParams p = {} ) { configure( p ); }
  void configure( const JointParams& p );
  /// theta0 in rad; velocity zero; muscles fresh.
  void reset( double theta0 = 0.2 );
  void step( double dt_ms, const JointInput& in );

  double theta = 0, omega = 0;   // rad, rad/s
  double torque_muscle = 0, torque_gravity = 0, torque_ext = 0, torque_limit = 0;
  Muscle agonist, antagonist;
  Afferents ag_aff, ant_aff;
  const JointParams& params() const { return p_; }
  /// Normalised muscle length l/l_opt for the flexor (shortens as theta rises) and the extensor.
  double flexor_length( double th ) const;
  double extensor_length( double th ) const;
  Readouts readouts() const;
  struct View { double theta, theta_norm; double ag_act, ant_act, ag_force_frac, ant_force_frac; };
  View view() const;

private:
  JointParams p_;
  Spindle sp_ag_, sp_ant_;
  Gto gto_ag_, gto_ant_;
  void torques( double th, double om, double ext, double& t_mus, double& t_grav, double& t_lim ) const;
};

} // namespace spike::body
