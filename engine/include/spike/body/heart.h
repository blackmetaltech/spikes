// Heart + systemic circulation: beat events, stroke volume (Frank-Starling, contractility, filling time), a closed
// two-compartment loop (arterial 2-element Windkessel + lumped venous reservoir), sympathetic and vagal effects,
// orthostatic and haemorrhage perturbations, baroreceptor afferent. See docs/body/heart.md.
#pragma once
#include "spike/body/common.h"

namespace spike::body
{

struct HeartParams
{
  // Windkessel (Frank 1899; Westerhof et al. 2009): C dP/dt = Q_in - (P - Pv)/R
  double c_art = 1.3;          // ml/mmHg arterial compliance
  double r0 = 1.06;            // mmHg s/ml total peripheral resistance at rest (MAP 95, CVP 5, CO 82 ml/s)
  double c_ven = 100;          // ml/mmHg lumped venous / central compliance
  double pv0 = 5.0;            // mmHg central venous pressure at rest
  // Ventricle
  double sv_rest = 70;         // ml at 70 bpm, rest tone
  double hr_ref_bpm = 70;
  double starling_k = 4.0;     // mmHg, SV ~ Pv/(Pv + K) (Frank-Starling, saturating)
  double fill_tau_s = 0.35;    // diastolic filling time constant (Td = RR - T_ej)
  double ejection_s_per_sqrtrr = 0.33;  // T_ej = 0.33 sqrt(RR) (Weissler-type)
  double refractory_ms = 250;  // minimum beat interval
  // Autonomic: all drives 0..1; resting tone sym 0.2, vagal 0.6
  double sym_rest = 0.2, vagal_rest = 0.6;
  double intrinsic_bpm = 105, sym_hr_bpm = 80, vagal_hr_bpm = 85;      // HR = 105 + 80 sym - 85 vagal (rest 70)
  double sym_contractility = 1.0;   // contractility factor = 1 + k (sym_cardiac - rest)
  double sym_resistance = 0.8;      // R = R0 (1 + k (sym_vascular - rest))
  double sym_venous_ml = 200;       // venoconstriction: stressed volume added per unit (sym_vascular - rest), ml
  double tau_sym_ms = 1500;         // effector smoothing of sympathetic effects
  // Perturbations
  double pool_ml_standing = 220;    // blood pooled in the legs when fully upright (venous return drop)
  double tau_pool_s = 6;
  // Baroreceptor (Kirchheim 1976; Chapleau & Abboud 1987; Mahdi et al. 2013): sigmoid in P + tau_d dP/dt
  double baro_max_hz = 100, baro_p50 = 100, baro_slope = 20, baro_tau_d_s = 0.04;
};

struct HeartInput
{
  /// SA-node spike count in this slice: any spike fires a beat (subject to the refractory period).
  int sa_spikes = 0;
  /// If >= 0 and no spike: beat rate in Hz (deterministic phase accumulator; 0 = beats only from sa_spikes). If < 0 the intrinsic pacemaker is used:
  /// HR = intrinsic + k_sym*sym_cardiac - k_vagal*vagal.
  double sa_rate_hz = -1;
  double sym_cardiac = 0.2;   // sympathetic drive to heart: contractility (and HR in intrinsic mode)
  double sym_vascular = 0.2;  // sympathetic drive to vessels: resistance + venoconstriction
  double vagal = 0.6;         // vagal drive: lowers HR (intrinsic mode)
  double standing = 0;        // 0 supine .. 1 upright (orthostatic stress)
  double haemorrhage_ml_s = 0;// blood loss rate (ml/s); cumulative loss is in blood_lost_ml
};

class Heart
{
public:
  explicit Heart( HeartParams p = {} ) : p_( p ) { reset(); }
  void reset();
  void step( double dt_ms, const HeartInput& in );

  // outputs
  double pressure = 95;       // arterial pressure now, mmHg
  double dpdt = 0;            // mmHg/s
  double map_beat = 95;       // mean over the last completed cycle
  double map_smooth = 95;     // low-passed (tau 2 s), for gauges
  double systolic = 120, diastolic = 80;   // of the last completed cycle
  double cvp = 5;             // central venous pressure
  double hr_bpm = 70;         // from the last RR interval
  double stroke_ml = 70, co_l_min = 4.9;
  double contractility = 1, resistance = 1.1;  // present effector values
  double baro_hz = 0;         // instantaneous baroreceptor afferent rate (impulses/s)
  double baro_mean_hz = 0;    // averaged over ~1 s (use this to drive neurons)
  double blood_lost_ml = 0, pooled_ml = 0;
  bool beat_now = false;      // a beat started in this slice
  int beats = 0;
  const HeartParams& params() const { return p_; }
  Readouts readouts() const;
  /// phase: 0..1 since the last beat (relative to the last RR); ejection: 0..1 half-sine of ventricular ejection (heart size on screen).
  struct View { double phase, ejection, pressure, map, systolic, diastolic, hr_bpm; bool beat; };
  View view() const;

private:
  HeartParams p_;
  double pv_ = 5;
  double t_since_beat_s_ = 0.857, rr_s_ = 0.857, t_ej_ = 0.3, sv_beat_ = 70;
  double rate_phase_ = 0;
  double sym_c_ = 0.2, sym_v_ = 0.2, ven_shift_ = 0;
  double cyc_sum_ = 0, cyc_n_ = 0, cyc_max_ = 0, cyc_min_ = 1e9;
  double baro_filt_ = 0, dpdt_filt_ = 0;
  bool ejecting_ = false;
  double eject_ms_ = 0;
};

} // namespace spike::body
